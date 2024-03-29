#include "server.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <microhttpd.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <zlib.h>
#include <zorder.h>

#include "MAB/log.h"
#include "base64.h"
#include "vec.h"
#include "voxel_traversal.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <glad/glad.h>
#include <glad/glad_egl.h>

#include "stb_image_write.h"
#define GLAPIENTRY APIENTRY

#define FG_MAGIC (('f' << 24) | ('g' << 16) | ('0' << 8) | ('1'))

struct attrib {
  GLuint64 size;
  union {
    GLuint64 rawtype;
    GLenum type;
  };
  GLuint64 count;
  union {
    GLbyte range[8];
    GLfloat frange[2];
    GLint irange[2];
    GLuint urange[2];
  };
  union {
    GLbyte *data;
    GLfloat *floats;
    GLint *ints;
    GLuint *uints;
  };
};

typedef struct partition_data {
  Vec_GLuint partitions;
  Vec_GLuint edgeIdxs;
  GLuint indexBuffer;
  GLuint perPartitionSSBO;
  GLuint count;
  GLuint dcIdent;
} PartitionData;

enum {
  ERROR_NONE = 0,
  ERROR_COMPILE_VERTEX_SHADER,
  ERROR_COMPILE_FRAGMENT_SHADER,
  ERROR_LINK_PROGRAM,
  ERROR_BAD_SORT,
  NUM_ERRORS,
};

char *errorMessages[NUM_ERRORS] = {
        [ERROR_NONE] = "Render Successful",
        [ERROR_COMPILE_VERTEX_SHADER] = "Could not compile vertex shader",
        [ERROR_COMPILE_FRAGMENT_SHADER] = "Could not compile fragment shader",
        [ERROR_LINK_PROGRAM] = "Could not link program",
        [ERROR_BAD_SORT] = "Bad sort",
};

static const int _g_resolution = 256;
static volatile GLsizei _resolution = _g_resolution;
static volatile GLsizei _depth = 4;
static volatile GLfloat _x;
static volatile GLfloat _y;
static volatile GLfloat _z;
static GLchar *volatile _dataset;
static volatile GLuint _dcIdent;
static GLuint *volatile _dcIndex;
static GLfloat *volatile _dcMult;
static GLfloat *volatile _dcOffset;
static volatile GLfloat _dcMinMult;
static volatile GLfloat _dcMaxMult;
static GLchar *volatile _info;
static GLchar *volatile _vertexShaderSource;
static GLchar *volatile _fragmentShaderSource;
static GLubyte *volatile _image;
static volatile GLubyte volatile _error;
static pthread_mutex_t *_lock;
static pthread_barrier_t *_barrier;

static volatile int _logGLCalls;
static volatile int _doOcclusionCulling;
static volatile int _doScissorTest;
static volatile int _doRepartition;

static volatile GLuint _max_depth = 0;
static unsigned volatile _max_res;
static PartitionData *_partition_cache;
static volatile struct attrib *edges;

// EDGE ATTRIBS
static GLuint globalSSBO;

// EGL STATICS
static volatile EGLDisplay eglDisplay = EGL_NO_DISPLAY;
static volatile EGLContext eglContext = NULL;
static GLuint eglFrameBuffer = 0;
static GLuint eglFrameBufferColorAttachmentTexture = 0;
static GLuint eglFrameBufferDepthAttachmentTexture = 0;

/** EGL SPECIFIC SETUP **/
/// A surface that is RGBA8
/// A surface that has a depth + stencil 24bit/8bit UInt
/// A surface that is OpenGL _NOT ES_ conformant
/// A surface that is renderable
/// A surface that will not take caveats (must meet all demands!)
static const EGLint configAttribs[] = {EGL_COLOR_BUFFER_TYPE,
                                       EGL_RGB_BUFFER,
                                       EGL_BLUE_SIZE,
                                       8,
                                       EGL_GREEN_SIZE,
                                       8,
                                       EGL_RED_SIZE,
                                       8,
                                       EGL_ALPHA_SIZE,
                                       8,
                                       EGL_DEPTH_SIZE,
                                       24,
                                       EGL_STENCIL_SIZE,
                                       8,
                                       EGL_RENDERABLE_TYPE,
                                       EGL_OPENGL_BIT,
                                       EGL_CONFORMANT,
                                       EGL_OPENGL_BIT,
                                       EGL_CONFIG_CAVEAT,
                                       EGL_NONE,
                                       EGL_LEVEL,
                                       0,
                                       EGL_NONE};

void GLAPIENTRY eglMessageCallback(GLenum source, GLenum type, GLuint id,
                                   GLenum severity, GLsizei length,
                                   const GLchar *message,
                                   const void *userParam) {
  MAB_WRAP("eglMessageCallback") {
    mabLogMessage("type", "0x%x", type);
    mabLogMessage("severity", "0x%x", severity);
    mabLogMessage("message", "%s", message);
  }
}
static pthread_mutex_t *_fgl_lock;
static pthread_barrier_t *_fgl_barrier;
static volatile char *_fgl_in_url;  // request thread frees
static volatile char *_fgl_out_url; // transformer thread frees

void log_partition_cache(PartitionData *pdc) {
  printf("tot: %u\n", _max_res * _max_res);
  for (int i = 0; i < _max_res * _max_res; ++i) {
    PartitionData *pd = &(pdc[i]);

    if (pd->partitions.length == 0)
      continue;

    printf("idx: %d\n", i);
    printf("edgeCount: %lu\n", pd->partitions.length / 2);
    printf("     ");
    for (int j = 0; j < pd->partitions.length; ++j) {
      printf("%d,", pd->partitions.data[j]);
    }
    printf("\n---------------------------------------\n\n");
  }
}

void *render(void *v) {
  GLuint vertexShader, fragmentShader, program,
      /*indexBuffer,*/ aNodeBuffers[16];
  unsigned long i, j;
  GLuint64 ncount, ecount;
  GLint rc, uTranslateX, uTranslateY, uScale, uNodeMins[16], uNodeMaxs[16],
      uEdgeMins[16], uEdgeMaxs[16],
      aNodeLocations[16], uCat6[6];
  GLchar log[512];
  struct attrib nattribs[16], eattribs[16], *edges;
  FILE *f;
  enum {
    INIT_OSMESA,
    INIT_GRAPH,
    INIT_BUFFERS,
    INIT_PROGRAM,
    INIT_UNIFORMS,
    INIT_PARTITION,
    INIT_ATTRIBUTES,
    INIT_SORT,
    INIT_INDEX_BUFFER,
    RENDER,
    WAIT
  } where;

  GLfloat x, y, z;
  int max_depth;
  GLchar *dataset, *vertexShaderSource, *fragmentShaderSource;
  GLboolean first;
  GLuint dcIdent, *dcIndex;
  GLfloat *dcMult, *dcOffset, dcMinMult, dcMaxMult;
  int logGLCalls, doOcclusionCulling, doScissorTest;
  int doRepartition;

  _partition_cache = NULL;

  vertexShader = fragmentShader = program = 0;
  nattribs[0] = (struct attrib){0};
  eattribs[0] = (struct attrib){0};
  edges = malloc(sizeof(*edges));
  *edges = (struct attrib){0};
  aNodeBuffers[0] = 0;
  // indexBuffer = 0;
  dcIdent = 0;
  dcIndex = NULL;
  dcMult = dcOffset = NULL;
  logGLCalls = 0;
  doOcclusionCulling = 0;
  doScissorTest = 0;
  doRepartition = 0;

  x = y = z = INFINITY;
  dataset = vertexShaderSource = fragmentShaderSource = NULL;
  first = GL_TRUE;

  globalSSBO = 0;

  // sync with main function
  pthread_barrier_wait(_barrier);
  goto wait_for_request;

  for (;;) {
    switch (where) {
    case INIT_OSMESA:
      MAB_WRAP("create osmesa context") {
        if (!gladLoadEGL()) {
          mabLogMessage("@error", "Could not load EGL!");
          exit(1);
        }

        // 0. Get EGL Display (offscreen in case of headless)
        eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (EGL_NO_DISPLAY == eglDisplay) {
          mabLogMessage("@error", "Could not create the EGL Display!");
          exit(1);
        }

        // 1. Inititialize EGL
        EGLint major, minor;
        if (EGL_FALSE == eglInitialize(eglDisplay, &major, &minor)) {
          mabLogMessage("@error", "Could not initialize EGL!");
          exit(1);
        }

        // 2. Choose Configuration, give them the minimum requirements
        // (specified by configAttribs)
        EGLint numConfigs;
        EGLConfig eglCfg;
        // Ideally, iterate eglCfg and make sure numConfigs matches
        // configAttribs but I digress
        eglChooseConfig(eglDisplay, configAttribs, &eglCfg, 1, &numConfigs);

        // 3. Bind the API, we are Using OPENGL not ES!
        if (EGL_FALSE == eglBindAPI(EGL_OPENGL_API)) {
          mabLogMessage("@error", "OpenGL is not support for this EGL context!");
          exit(1);
        }

        // Try to snag a Context with OpenGL4.6 + Debug Features.
        // Compatibility profile for Global VAO
        const EGLint contextAttribs[] = {
            EGL_CONTEXT_MAJOR_VERSION,
            4,
            EGL_CONTEXT_MINOR_VERSION,
            6,
            EGL_CONTEXT_OPENGL_PROFILE_MASK,
            EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT,
            EGL_CONTEXT_OPENGL_DEBUG,
            EGL_TRUE,
            EGL_NONE};
        eglContext = eglCreateContext(eglDisplay, eglCfg, EGL_NO_CONTEXT,
                                      contextAttribs);
        if (!eglContext) {
          mabLogMessage("@error", "could not init EGL context");
          mabLogMessage("@info", "Do you have a GPU? (If you're part of Seelab, then run FG from Kavir");
          exit(1);
        }

        // Establish context as current and ensure NO_SURFACE is specified.
        EGLBoolean currentSuccess = eglMakeCurrent(eglDisplay, EGL_NO_SURFACE,
                                                   EGL_NO_SURFACE, eglContext);
        if (!currentSuccess) {
          mabLogMessage("@error", "could not bind to image buffer");
          exit(1);
        }

        // Load GL+Extensions
        if (!gladLoadGL()) {
          mabLogMessage("@error", "Could not load GL!");
        }

        // Enable Debugging
        glEnable(GL_DEBUG_OUTPUT);
        glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE,
                              GL_DEBUG_SEVERITY_NOTIFICATION, 0, 0, GL_FALSE);
        glDebugMessageCallback(eglMessageCallback, 0);

        // Generate a FrameBuffer Object
        glGenFramebuffers(1, &eglFrameBuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, eglFrameBuffer);

        // Bind a Texture to the Color Attachment
        glGenTextures(1, &eglFrameBufferColorAttachmentTexture);
        glBindTexture(GL_TEXTURE_2D, eglFrameBufferColorAttachmentTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, _resolution, _resolution, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D,
                               eglFrameBufferColorAttachmentTexture, 0);

        // Create a texture for the Depth + Stencil attachment
        glGenTextures(1, &eglFrameBufferDepthAttachmentTexture);
        glBindTexture(GL_TEXTURE_2D, eglFrameBufferDepthAttachmentTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, _resolution,
                     _resolution, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                               GL_TEXTURE_2D,
                               eglFrameBufferDepthAttachmentTexture, 0);

        // Make sure the Framebuffer is complete!
        if (!(GL_FRAMEBUFFER_COMPLETE ==
              glCheckFramebufferStatus(GL_FRAMEBUFFER))) {
          mabLogMessage("@error", "Could not complete the framebuffer!");
          exit(1);
        }

        // Set Viewport to specified resolution
        glViewport(0, 0, _resolution, _resolution);
        GLint viewport[4];
        glGetIntegerv(GL_VIEWPORT, viewport);
        // Ensure resolution and viewport match 1:1, EGL can "fake" this call
        // and one symptom is viewport mismatch
        if ((_resolution + _resolution) != (viewport[2] + viewport[3])) {
          mabLogMessage("@error", "Viewport does not meet the resolution requirement!");
          exit(1);
        }
        mabLogMessage("@info", "EGL+OpenGL Setup complete.");

        // Reset clear color and empty depth buffer
        // glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glFlush();

        _image = malloc(_depth * _resolution * _resolution);
        if (!_image) {
          mabLogMessage("@error", "could not allocate image buffer");
          exit(1);
        }

        const GLubyte *eglVendor = eglQueryString(eglDisplay, EGL_VENDOR);
        const GLubyte *eglVersion = eglQueryString(eglDisplay, EGL_VERSION);
        const GLubyte *eglExtensions =
            eglQueryString(eglDisplay, EGL_EXTENSIONS);
        const GLubyte *eglAPIs = eglQueryString(eglDisplay, EGL_CLIENT_APIS);
        mabLogMessage("EGL Vendor", "%s", eglVendor);
        mabLogMessage("EGL Version", "%s", eglVersion);
        mabLogMessage("EGL Extensions", "%s", eglExtensions);
        mabLogMessage("EGL APIs", "%s", eglAPIs);

        const GLubyte *glVendor = glGetString(GL_VENDOR);
        mabLogMessage("GL Vendor", "%s", glVendor);
        const GLubyte *glRenderer = glGetString(GL_RENDERER);
        mabLogMessage("GL Renderer", "%s", glRenderer);
        const GLubyte *glVersion = glGetString(GL_VERSION);
        mabLogMessage("GL Version", "%s", glVersion);

        const GLubyte *glslVersion = glGetString(GL_SHADING_LANGUAGE_VERSION);
        mabLogMessage("GLSL Version", "%s", glslVersion);
      }
      __attribute__((fallthrough));

    case INIT_GRAPH:
      MAB_WRAP("load graph") {
        if (nattribs[0].data)
          for (i = 0; i < ncount; ++i)
            free(nattribs[i].data);

        MAB_WRAP("load node attributes") {
          char temp[512];
          snprintf(temp, sizeof(temp), "data/%s/nodes.dat", _dataset);

          f = fopen(temp, "r");
          fread(&ncount, sizeof(ncount), 1, f);

          for (i = 0; i < ncount; ++i) {
            nattribs[i] = (struct attrib){0};

            fread(&nattribs[i].size, sizeof(nattribs[i].size), 1, f);
            fread(&nattribs[i].rawtype, sizeof(nattribs[i].rawtype), 1, f);
            switch (nattribs[i].rawtype) {
            case 'f':
              nattribs[i].type = GL_FLOAT;
              break;
            case 'i':
              nattribs[i].type = GL_INT;
              break;
            case 'u':
              nattribs[i].type = GL_UNSIGNED_INT;
              break;
            }

            fread(&nattribs[i].count, sizeof(nattribs[i].count), 1, f);
            fread(nattribs[i].range, sizeof(nattribs[i].range), 1, f);

            nattribs[i].data = malloc(nattribs[i].size);
            fread(nattribs[i].data, 1, nattribs[i].size, f);
          }

          fclose(f);
        }

        if (eattribs[0].data)
          for (i = 0; i < ecount; ++i)
            free(eattribs[i].data);

        MAB_WRAP("load edge data") {
          char temp[512];
          snprintf(temp, sizeof(temp), "data/%s/edges.dat", _dataset);

          f = fopen(temp, "r");
          fread(&ecount, sizeof(ecount), 1, f);

          GLuint64 runningStride = 0;
          GLuint64 runningSize = 0;
          GLuint64 totalEdgeCount = 0;
          // Edge count should be 1 more than the total count of edges.
          mabLogMessage("edge count", "%lu", ecount);
          for (i = 0; i < ecount; ++i) {
            eattribs[i] = (struct attrib){0};

            fread(&eattribs[i].size, sizeof(eattribs[i].size), 1, f);

            if (!totalEdgeCount) {
              // Why 8? The first column is twice the size of other! X+Y are
              // combined!!
              totalEdgeCount = eattribs[i].size / 8;
            }

            fread(&eattribs[i].rawtype, sizeof(eattribs[i].rawtype), 1, f);
            switch (eattribs[i].rawtype) {
            case 'f':
              eattribs[i].type = GL_FLOAT;
              break;
            case 'i':
              eattribs[i].type = GL_INT;
              break;
            case 'u':
              eattribs[i].type = GL_UNSIGNED_INT;
              break;
            }

            fread(&eattribs[i].count, sizeof(eattribs[i].count), 1, f);

            fread(eattribs[i].range, sizeof(eattribs[i].range), 1, f);

            eattribs[i].data = malloc(eattribs[i].size);
            fread(eattribs[i].data, 1, eattribs[i].size, f);
            //fprintf(stderr, "eattribs size is: %ld bytes \n", eattribs[i].size);
            if (i > 0) {
              runningStride += 4;
              runningSize += eattribs[i].size;
            }
          }
          // fprintf(stderr, "Total bytes per edge: %lu, there are %lu edges \n",
          //         runningStride, totalEdgeCount);
          // fprintf(stderr, "Total size for edge attributes: %lu \n",
          //         runningSize);
          // send me to jail
          // fprintf(stderr, "Next 16 byte alignment: %lu \n",
          //         ((GLint64)runningStride + 15) & -16);

          if (globalSSBO) {
            glDeleteBuffers(1, &globalSSBO);
            globalSSBO = 0;
          }

          if (runningSize) {
            glGenBuffers(1, &globalSSBO);
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, globalSSBO);

            size_t amountPerEdge = ((GLint64)runningStride + 15) & -16;
            // Add one more edge. Why? Edge IDs are zero indexed!
            size_t ssboSize = (size_t)(totalEdgeCount + 1) * amountPerEdge;
            void *edgeData = malloc(ssboSize);
            memset(edgeData, 0, amountPerEdge);

            int edgePad = ((int)(ecount - 1) + 3) & -4;
            // fprintf(stderr, "edge pad: %d, edge count: %lu \n", edgePad,
            // ecount);
            // fprintf(stderr, "Total size for edge attributes: %lu \n",
            // runningSize);
            for (int edgeNumber = 1; edgeNumber < totalEdgeCount + 1;
                 ++edgeNumber) {
              for (int i = 0; i < edgePad; ++i) {
                // fprintf(stderr, "i: %d, ep: %d, ec: %ld \n", i + 1, edgePad,
                // ecount);
                if (i + 1 >= ecount) {
                  ((float *)edgeData)[edgeNumber * edgePad + i] = 0.0;
                } else {
                  // fprintf(stderr, "\tSet edge %d attribute %d to %f \n",
                  //         edgeNumber, i + 1,
                  //         eattribs[i + 1].floats[edgeNumber - 1]);
                  ((float *)edgeData)[edgeNumber * edgePad + i] =
                      eattribs[i + 1].floats[edgeNumber - 1];
                }
              }
            }

            /* for (size_t i = 0; i < (size_t)(totalEdgeCount + 1) * edgePad;
            ++i) {
              fprintf(stderr, "%ld: %f ", i, ((float *)edgeData)[i]);
            }
            fprintf(stderr, "\n"); */

            glBufferData(GL_SHADER_STORAGE_BUFFER, ssboSize, edgeData,
                         GL_STATIC_DRAW);
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
            free(edgeData);
          }

          fclose(f);
        }
      }
      __attribute__((fallthrough));

    case INIT_PARTITION:
      MAB_WRAP("init partition") {
        // printf("partitioning data...\n");
        unsigned j;
        char filename[128];

        // clear previous partitions
        if (_partition_cache) {
          for (i = 0; i < _max_res * _max_res; ++i) {
            vec_destroy(&_partition_cache[i].partitions);
            vec_destroy(&_partition_cache[i].edgeIdxs);
          }
          free(_partition_cache);
          _partition_cache = NULL;
        }

        // create new partitions
        _max_res = pow(2, _max_depth);
        // printf("partition res: (%ux%u)\n", _max_res, _max_res);
        unsigned long blkSize = _max_res * _max_res * sizeof(PartitionData);
        _partition_cache = (PartitionData *)calloc(blkSize, sizeof(char));
        if (!_partition_cache) {
          mabLogMessage("@error", "Could not create partition cache of size %lu bytes!\n", blkSize);
          exit(1);
        }
        for (i = 0; i < _max_res * _max_res; ++i) {
          vec_init(&_partition_cache[i].partitions);
          vec_init(&_partition_cache[i].edgeIdxs);
        }

        if (doRepartition) {
          goto stop_reading_partition_cache; // XXX(th): sorry
        }

        snprintf(filename, sizeof(filename), "cache/%s_mr%u.partition.dat", dataset, _max_res);
        f = fopen(filename, "rb");
        if (f == NULL) {
          mabLogMessage("@info", "Partition cache %s does not exist", filename);
        }

        if (f != NULL) {
          uint32_t magic;

          if (fread(&magic, sizeof(magic), 1, f) != 1) {
            mabLogMessage("@info", "Partition cache %s has no magic! Ignoring it", filename);
            fclose(f);
            goto stop_reading_partition_cache; // XXX(th): sorry
          }

          if (magic != FG_MAGIC) {
            mabLogMessage("@info", "Partition cache %s has bad magic! Ignoring it", filename);
            fclose(f);
            goto stop_reading_partition_cache; // XXX(th): sorry
          }

          for (i=0; i< _max_res * _max_res; ++i) {
            size_t length, size;

            fread(&length, sizeof(length), 1, f);

            _partition_cache[i].partitions.length = length;
            _partition_cache[i].partitions.capacity = length;

            size = sizeof(*_partition_cache[i].partitions.data) * length;
            _partition_cache[i].partitions.data = malloc(size);
            fread(_partition_cache[i].partitions.data, size, 1, f);

            fread(&length, sizeof(length), 1, f);

            _partition_cache[i].edgeIdxs.length = length;
            _partition_cache[i].edgeIdxs.capacity = length;

            size = sizeof(*_partition_cache[i].edgeIdxs.data) * length;
            _partition_cache[i].edgeIdxs.data = malloc(size);
            fread(_partition_cache[i].edgeIdxs.data, size, 1, f);
          }
          fclose(f);

          goto done_partitioning; // XXX(th): sorry
        }
stop_reading_partition_cache: ; // XXX(th): sorry

        GLfloat *vertsX = nattribs[0].floats;
        GLfloat *vertsY = nattribs[1].floats;

        GLfloat minX = nattribs[0].frange[0];
        GLfloat maxX = nattribs[0].frange[1];
        GLfloat minY = nattribs[1].frange[0];
        GLfloat maxY = nattribs[1].frange[1];

        GLfloat gx = (maxX - minX) / 1024;
        GLfloat gy = (maxY - minY) / 1024;
        minX -= gx;
        maxX += gx;
        minY -= gy;
        maxY += gy;

        *edges = eattribs[0];
        long sz = edges->count;
        long hundredth = sz / 100;
        if (hundredth == 0)
          ++hundredth;
        for (i = 0; i < sz; i += 2) {
          if (i % hundredth < 2)
            mabLogMessage("@info", "partitioning %d%% done", (int)(100 * (double)i / (double)sz));

          GLuint e0 = edges->uints[i + 0];
          GLuint e1 = edges->uints[i + 1];

          GLfloat x0 = vertsX[e0];
          GLfloat y0 = vertsY[e0];
          GLfloat x1 = vertsX[e1];
          GLfloat y1 = vertsY[e1];

          Vec_GLuint partitions = voxel_traversal(
              _max_res, _max_res, x0, y0, x1, y1, minX, minY, maxX, maxY);
          // printf("edge lies in %lu voxels\n", partitions.length);

          for (j = 0; j < partitions.length; ++j) {
            // fprintf(stderr, "pushing edge (%d,%d) (%ld) to partition %d\n",
            // e0, e1, i/2, partitions.data[j]);
            assert(vec_push(&_partition_cache[partitions.data[j]].partitions,
                            e0) != -1);
            assert(vec_push(&_partition_cache[partitions.data[j]].partitions,
                            e1) != -1);
            assert(vec_push(&_partition_cache[partitions.data[j]].edgeIdxs,
                            i / 2) != -1);
          }
          vec_destroy(&partitions);
        }
        mabLogMessage("@info", "done partitioning");
        // log_partition_cache(_partition_cache);

        f = fopen(filename, "wb");
        if (f == NULL) {
          mabLogMessage("@error", "Could not create partition cache %s", filename);
        }

        if (f != NULL) {
          uint32_t magic;
          magic = 0;
          fwrite(&magic, sizeof(magic), 1, f);

          for (i=0; i< _max_res * _max_res; ++i) {
            size_t length, size;

            length = _partition_cache[i].partitions.length;
            fwrite(&length, sizeof(length), 1, f);

            size = sizeof(*_partition_cache[i].partitions.data) * length;
            fwrite(_partition_cache[i].partitions.data, size, 1, f);

            length = _partition_cache[i].edgeIdxs.length;
            fwrite(&length, sizeof(length), 1, f);

            size = sizeof(*_partition_cache[i].edgeIdxs.data) * length;
            fwrite(_partition_cache[i].edgeIdxs.data, size, 1, f);
          }

          fseek(f, 0, SEEK_SET);

          magic = FG_MAGIC;
          fwrite(&magic, sizeof(magic), 1, f);

          fclose(f);
        }

done_partitioning: ; // XXX(th): sorry
      }
      __attribute__((fallthrough));

    case INIT_SORT:
      MAB_WRAP("sort depth") {
        unsigned j;
        GLuint sz = _max_res * _max_res;
        long hundredth = sz / 10;
        if (hundredth == 0)
          ++hundredth;

        for (j = 0; j < sz; ++j) {
          if (j % hundredth < 1)
            mabLogMessage("@info", "sorting %d%% done", (int)(100 * (double)j / (double)sz));

          PartitionData *pd = &_partition_cache[j];
          unsigned long count = pd->partitions.length / 2;
          int i;
          GLfloat *dcDepth;
          dcDepth = malloc(count * sizeof(*dcDepth));
          for (i = 0; i < count; ++i) {
            GLuint e0 = pd->partitions.data[2 * i + 0];
            GLuint e1 = pd->partitions.data[2 * i + 1];

            dcDepth[i] = 0.0;

            int k = 0;
            for (k = 0; k < 16; ++k)
              if (!dcIndex[k])
                break;
            --k;

            if (k < 0) {
              _error = ERROR_BAD_SORT;
              goto done_with_request;
            }

            float sourceDepth =
                dcMult[k] *
                    (
                      nattribs[dcIndex[k] - 1].floats[eattribs[0].uints[e0]] -
                      nattribs[dcIndex[k] - 1].frange[0]
                    ) / (
                      nattribs[dcIndex[k] - 1].frange[1] -
                      nattribs[dcIndex[k] - 1].frange[0]
                    ) +
                dcOffset[k];
            float targetDepth =
                dcMult[k] *
                  (
                    nattribs[dcIndex[k] - 1].floats[eattribs[0].uints[e1]] -
                    nattribs[dcIndex[k] - 1].frange[0]
                  ) / (
                    nattribs[dcIndex[k] - 1].frange[1] -
                    nattribs[dcIndex[k] - 1].frange[0]
                  ) +
                dcOffset[k];

            dcDepth[i] += sourceDepth < targetDepth
                              ? dcMinMult * sourceDepth + dcMaxMult * targetDepth
                              : dcMinMult * targetDepth + dcMaxMult * sourceDepth;
            dcDepth[i] *= -1.;
          }

          GLuint64 *dcReorder;
          dcReorder = malloc(count * sizeof(*dcReorder));
          for (i = 0; i < count; ++i) {
            dcReorder[i] = i;
          }

          int compare(const void *av, const void *bv) {
            const GLuint64 *a = av;
            const GLuint64 *b = bv;

            if (dcDepth[*a] < dcDepth[*b])
              return -1;
            if (dcDepth[*a] > dcDepth[*b])
              return 1;
            return 0;
          }
          qsort(dcReorder, count, sizeof(*dcReorder), compare);

          GLuint *edges = malloc(count * 2 * sizeof(*edges));
          GLuint *edgeIdxs = malloc(count * sizeof(*edgeIdxs));
          for (i = 0; i < count; ++i) {
            edges[2 * i + 0] = pd->partitions.data[2 * dcReorder[i] + 0];
            edges[2 * i + 1] = pd->partitions.data[2 * dcReorder[i] + 1];
            edgeIdxs[i] = pd->edgeIdxs.data[dcReorder[i]];
          }

          vec_destroy(&pd->partitions);
          pd->partitions.data = edges;
          pd->partitions.capacity = pd->partitions.length = count * 2;

          vec_destroy(&pd->edgeIdxs);
          pd->edgeIdxs.data = edgeIdxs;
          pd->edgeIdxs.capacity = pd->edgeIdxs.length = count;

          free(dcReorder);
          free(dcDepth);
        }
        mabLogMessage("@info", "sorting 100%% done");
      }
      __attribute__((fallthrough));

    case INIT_BUFFERS:
      MAB_WRAP("init node buffers") {
        if (aNodeBuffers[0])
          for (i = 0; i < ncount; ++i)
            glDeleteBuffers(1, &aNodeBuffers[i]);

        for (i = 0; i < ncount; ++i) {
          glGenBuffers(1, &aNodeBuffers[i]);
          glBindBuffer(GL_ARRAY_BUFFER, aNodeBuffers[i]);
          if (logGLCalls)
            mabLogMessage("glBufferData", "%lu", nattribs[i].size);
          glBufferData(GL_ARRAY_BUFFER, nattribs[i].size, nattribs[i].data,
                       GL_STATIC_DRAW);
          glBindBuffer(GL_ARRAY_BUFFER, 0);
        }
      }
      __attribute__((fallthrough));

    case INIT_PROGRAM:
      MAB_WRAP("create program") {
        MAB_WRAP("create vertex shader") {
          if (vertexShader) {
            glDeleteShader(vertexShader);
          }
          vertexShader = glCreateShader(GL_VERTEX_SHADER);

          glShaderSource(vertexShader, 1,
                         (const GLchar *const *)&_vertexShaderSource, NULL);

          glCompileShader(vertexShader);

          glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &rc);
          if (!rc) {
            glGetShaderInfoLog(vertexShader, sizeof(log), NULL, log);
            mabLogMessage("@error", "vertex shader compilation failed: %s", log);
            mabLogEnd("ERROR: Vertex Shader Compilation Failed");
            mabLogEnd("ERROR: Vertex Shader Compilation Failed");
            _error = ERROR_COMPILE_VERTEX_SHADER;
            vertexShaderSource = NULL;
            fragmentShaderSource = NULL;
            goto done_with_request;
          }
        }

        MAB_WRAP("create fragment shader") {
          if (fragmentShader) {
            glDeleteShader(fragmentShader);
          }

          fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
          glShaderSource(fragmentShader, 1,
                         (const GLchar *const *)&_fragmentShaderSource, NULL);
          glCompileShader(fragmentShader);

          glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &rc);
          if (!rc) {
            glGetShaderInfoLog(fragmentShader, sizeof(log), NULL, log);
            mabLogMessage("@error", "fragment shader compilation failed: %s", log);
            mabLogEnd("ERROR: Fragment Shader Compilation Failed");
            mabLogEnd("ERROR: Fragment Shader Compilation Failed");
            _error = ERROR_COMPILE_FRAGMENT_SHADER;
            vertexShaderSource = NULL;
            fragmentShaderSource = NULL;
            goto done_with_request;
          }
        }

        MAB_WRAP("link program") {
          if (program) {
            glDeleteProgram(program);
          }

          program = glCreateProgram();
          glAttachShader(program, vertexShader);
          glAttachShader(program, fragmentShader);
          glLinkProgram(program);

          glGetProgramiv(program, GL_LINK_STATUS, &rc);
          if (!rc) {
            glGetProgramInfoLog(program, sizeof(log), NULL, log);
            mabLogMessage("@error", "shader program linking failed: %s", log);
            mabLogEnd("ERROR: Shader Program Linking Failed");
            mabLogEnd("ERROR: Shader Program Linking Failed");
            _error = ERROR_LINK_PROGRAM;
            vertexShaderSource = NULL;
            fragmentShaderSource = NULL;
            goto done_with_request;
          }

          glUseProgram(program);
        }
      }
      __attribute__((fallthrough));

    case INIT_ATTRIBUTES:
      MAB_WRAP("init node attributes") {
        if (globalSSBO) {
          // fprintf(stderr, "Looks like we got a global buffer on our
          // hands!\n");
          glBindBuffer(GL_SHADER_STORAGE_BUFFER, globalSSBO);
          glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, globalSSBO);
          glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
        }
        for (i = 0; i < ncount; ++i)
          MAB_WRAP("init attribute i=%d", i) {
            char temp[32];
            snprintf(temp, sizeof(temp), "aNode%lu", (unsigned long)(i + 1));

            aNodeLocations[i] = glGetAttribLocation(program, temp);
            if (aNodeLocations[i] != -1) {
              glBindBuffer(GL_ARRAY_BUFFER, aNodeBuffers[i]);
              glVertexAttribPointer(aNodeLocations[i], 1, nattribs[i].type,
                                    GL_FALSE, 0, 0);
              glEnableVertexAttribArray(aNodeLocations[i]);
              glBindBuffer(GL_ARRAY_BUFFER, 0);
            }
          }
      }
      __attribute__((fallthrough));

    case INIT_UNIFORMS:
      MAB_WRAP("init uniforms") {
        uTranslateX = glGetUniformLocation(program, "uTranslateX");
        uTranslateY = glGetUniformLocation(program, "uTranslateY");
        uScale = glGetUniformLocation(program, "uScale");

        for (i = 0; i < ncount; ++i) {
          char temp[32];
          snprintf(temp, sizeof(temp), "uNodeMin%lu", (unsigned long)(i + 1));
          uNodeMins[i] = glGetUniformLocation(program, temp);
        }
        
        for (i = 0; i < ncount; ++i) {
          char temp[32];
          snprintf(temp, sizeof(temp), "uNodeMax%lu", (unsigned long)(i + 1));
          uNodeMaxs[i] = glGetUniformLocation(program, temp);
        }

        for (i = 0; i < ecount; ++i) {
          char temp[32];
          snprintf(temp, sizeof(temp), "uEdgeMin%lu", (unsigned long)(i + 1));
          uEdgeMins[i] = glGetUniformLocation(program, temp);
        }
        
        for (i = 0; i < ecount; ++i) {
          char temp[32];
          snprintf(temp, sizeof(temp), "uEdgeMax%lu", (unsigned long)(i + 1));
          uEdgeMaxs[i] = glGetUniformLocation(program, temp);
        }

        for (i = 0; i < 6; ++i) {
          char temp[32];
          snprintf(temp, sizeof(temp), "uCat6[%lu]", (unsigned long)i);
          uCat6[i] = glGetUniformLocation(program, temp);
        }
      }
      __attribute__((fallthrough));

    case INIT_INDEX_BUFFER:
      MAB_WRAP("init index buffer") {
        // partition cache index buffers
        for (i = 0; i < _max_res * _max_res; ++i) {
          if (_partition_cache[i].indexBuffer)
            glDeleteBuffers(1, &_partition_cache[i].indexBuffer);

          if (_partition_cache[i].perPartitionSSBO)
            glDeleteBuffers(1, &_partition_cache[i].perPartitionSSBO);

          if (logGLCalls &&
              _partition_cache[i].partitions.length * sizeof(GLuint) > 0) {
            mabLogMessage("glBufferData", "%lu",
                          _partition_cache[i].partitions.length *
                              sizeof(GLuint));
          }

          if (_partition_cache[i].partitions.length * sizeof(GLuint) > 0) {
            glGenBuffers(1, &_partition_cache[i].indexBuffer);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,
                         _partition_cache[i].indexBuffer);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                         _partition_cache[i].partitions.length * sizeof(GLuint),
                         _partition_cache[i].partitions.data, GL_STATIC_DRAW);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

            glGenBuffers(1, &_partition_cache[i].perPartitionSSBO);
            glBindBuffer(GL_SHADER_STORAGE_BUFFER,
                         _partition_cache[i].perPartitionSSBO);
            glBufferData(GL_SHADER_STORAGE_BUFFER,
                         _partition_cache[i].edgeIdxs.length * sizeof(GLuint),
                         _partition_cache[i].edgeIdxs.data, GL_STATIC_DRAW);
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

            /* for (int elemID = 0; elemID <
            _partition_cache[i].edgeIdxs.length; ++elemID) {
              fprintf(stderr, "Partition: %ld - At index: %d, found: %d \n", i,
            elemID, vec_at(&_partition_cache[i].edgeIdxs, elemID));
            } */
          }
        }
      }
      __attribute__((fallthrough));

    case RENDER:
      MAB_WRAP("render") {
        // get current partition resolution by zoom level
        unsigned long res = (unsigned long)pow(2, _z);

        // glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glDisable(GL_DEPTH_TEST);
        // glDepthFunc(GL_LEQUAL);

        glEnable(GL_SCISSOR_TEST);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (_x >= 0 && _y >= 0 && _x < res && _y < res) {
          glUniform1f(uTranslateX, -1.0f * _x);
          glUniform1f(uTranslateY, -1.0f * _y);
          glUniform1f(uScale, pow(2.0f, _z));

          glUniform3f(uCat6[0], 0.86, 0.3712, 0.33999);
          glUniform3f(uCat6[1], 0.82879, 0.86, 0.33999);
          glUniform3f(uCat6[2], 0.33999, 0.86, 0.3712);
          glUniform3f(uCat6[3], 0.33999, 0.82879, 0.86);
          glUniform3f(uCat6[4], 0.3712, 0.33999, 0.86);
          glUniform3f(uCat6[5], 0.86, 0.33999, 0.82879);

          for (i = 0; i < ncount; ++i) {
            glUniform1f(uNodeMins[i], nattribs[i].frange[0]);
            glUniform1f(uNodeMaxs[i], nattribs[i].frange[1]);
          }

          for (i = 0; i + 1 < ecount; ++i) {
            glUniform1f(uEdgeMins[i], eattribs[i+1].frange[0]);
            glUniform1f(uEdgeMaxs[i], eattribs[i+1].frange[1]);
          }

          // figure out which partition we're rendering in
          unsigned long rx =
              (unsigned long)interpolate(0, res, 0, _max_res, _x);
          unsigned long ry =
              (unsigned long)interpolate(0, res, 0, _max_res, _y);

          if (rx < _max_res && ry < _max_res) {
            int numTilesX = (int)ceil((float)_max_res / (float)res);
            int numTilesY =
                numTilesX; // seperated just in case if in the future we want
                           // to have different x and y
            size_t distBetweenTilesX = _resolution / numTilesX;
            size_t distBetweenTilesY = _resolution / numTilesY;

            MAB_WRAP("rendering tiles") {
              for (i = 0; i < numTilesX; ++i) {
                for (j = 0; j < numTilesY; ++j) {
                  // if (i > 0 || j > 0) continue;

                  if (doScissorTest) {
                    // coordinates are from bottom left
                    // (left 0, bottom 0) bottom left
                    // (left 10, bottom 20) 10 pixels from left, 20 pixels from bottom
                    size_t left, bottom, width, height;

                    left = i * distBetweenTilesX;
                    bottom = j * distBetweenTilesY;
                    width = distBetweenTilesX;
                    height = distBetweenTilesY;

                    glScissor(left, bottom, width, height);
                  }

                  unsigned long idx = (ry + j) * _max_res + (rx + i);
                  PartitionData *pd = &_partition_cache[idx];
                  pd->count++;

                  if (pd->partitions.length == 0)
                    continue;
                  if (doOcclusionCulling) {
                    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,
                                 _partition_cache[idx].indexBuffer);
                    if (logGLCalls)
                      if (pd->partitions.length > 0)
                        mabLogMessage("glDrawElements", "%lu",
                                      pd->partitions.length);

                    int batchRenderSize =
                        10000; // experiment with this, should be some
                               // function of max_res
                    int numEdges = pd->partitions.length / 2;
                    for (int i = 0; i < (int)ceil((double)numEdges /
                                                  (double)batchRenderSize);
                         ++i) {
                      int startIdx = i * batchRenderSize;
                      int length = numEdges > (startIdx + batchRenderSize)
                                       ? batchRenderSize
                                       : numEdges - startIdx;

                      // GLuint q;
                      // glGenQueries(1, &q);

                      // glBeginQuery(GL_SAMPLES_PASSED, q);
                      
                      glDrawElements(GL_LINES, length * 2, GL_UNSIGNED_INT,
                                     (void *)(intptr_t)startIdx);

                      // glEndQuery(GL_SAMPLES_PASSED);

                      // GLint samples;
                      // glGetQueryObjectiv(q, GL_QUERY_RESULT, &samples);

                      // if(samples == 0)    break;
                    }
                    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

                  } else {
                    if (logGLCalls && pd->partitions.length > 0)
                      mabLogMessage("glDrawElements", "%lu",
                                    pd->partitions.length);

                    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,
                                 _partition_cache[idx].indexBuffer);

                    // Some check to see if there are edge attributes...
                    int thereAreEdgeAttributes = 1;
                    if (thereAreEdgeAttributes) {
                      glBindBuffer(GL_SHADER_STORAGE_BUFFER,
                                   _partition_cache[idx].perPartitionSSBO);
                      glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1,
                                       _partition_cache[idx].perPartitionSSBO);
                    } // end edge attribute setup

                    glDrawElements(GL_LINES, pd->partitions.length,
                                   GL_UNSIGNED_INT, 0);
                    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

                    // Disable edge edge attribute buffer if there are edge
                    // attributes...
                    if (thereAreEdgeAttributes) {
                      glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
                    } // end edge attributes

                    // printf(stderr, "\n\n\n",
                    // _partition_cache[idx].indexBuffer);
                    // }
                  }
                }

                if (doScissorTest) {
                  glScissor(0, 0, _resolution, _resolution);
                }
              }
            }
          }
        }
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glReadPixels(0, 0, _resolution, _resolution, GL_RGBA, GL_UNSIGNED_BYTE,
                     _image);
        __attribute__((fallthrough));

      case WAIT:
      done_with_request:
        // give control of buffer
        pthread_barrier_wait(_barrier);

        // wait for them to be done
        pthread_barrier_wait(_barrier);
        mabLogEnd(NULL);

      wait_for_request:
        // wait for request
        pthread_barrier_wait(_barrier);

        mabLogContinue(_info);
        mabLogAction("receive from request thread");

        where = RENDER;

        logGLCalls = _logGLCalls;
        doOcclusionCulling = _doOcclusionCulling;
        doScissorTest = _doScissorTest;
        doRepartition = _doRepartition;

        if (fabsf(x - _x) > 0.1) {
          x = _x;
          where = RENDER;
        }
        if (fabsf(y - _y) > 0.1) {
          y = _y;
          where = RENDER;
        }
        if (vertexShaderSource == NULL ||
            strcmp(vertexShaderSource, _vertexShaderSource) != 0) {
          if (vertexShaderSource)
            free(vertexShaderSource);
          vertexShaderSource = strdup(_vertexShaderSource);
          where = INIT_PROGRAM;
        }
        if (fragmentShaderSource == NULL ||
            strcmp(fragmentShaderSource, _fragmentShaderSource) != 0) {
          if (fragmentShaderSource)
            free(fragmentShaderSource);
          fragmentShaderSource = strdup(_fragmentShaderSource);
          where = INIT_PROGRAM;
        }
        if (doRepartition || max_depth != _max_depth) {
          max_depth = _max_depth;
          where = INIT_PARTITION;
        }
        if (dcIdent != _dcIdent) {
          dcIdent = _dcIdent;
          if (dcIndex)
            free(dcIndex);
          dcIndex = _dcIndex;
          if (dcMult)
            free(dcMult);
          dcMult = _dcMult;
          if (dcOffset)
            free(dcOffset);
          dcOffset = _dcOffset;
          dcMinMult = _dcMinMult;
          dcMaxMult = _dcMaxMult;
          where = INIT_SORT;
        }
        if (dataset == NULL || strcmp(dataset, _dataset) != 0) {
          if (dataset)
            free(dataset);
          dataset = strdup(_dataset);
          where = INIT_GRAPH;
        }
        if (first) {
          first = GL_FALSE;
          where = INIT_OSMESA;
        }

        break;
      } /* switch */
    }
  }
} /* render */

size_t tojpeg(void *rgba, int resolution, void **jpg, size_t *jpgsize) {
  size_t jpglen;

  mabLogMessage("resolution", "%d", resolution);
  mabLogMessage("rgba", "%p", rgba);

  *jpgsize = 1024;
  *jpg = malloc(*jpgsize);
  jpglen = 0;

  void mywriter(void *context, void *data, int size) {
    // mabLogMessage("size", "%d", size);
    if (jpglen + size > *jpgsize) {
      // mabLogMessage("realloc", "%lu bytes -> %lu bytes", *jpgsize, jpglen +
      // size);
      *jpgsize = jpglen + size;
      *jpg = realloc(*jpg, *jpgsize);
    }
    memcpy(*jpg + jpglen, data, size);
    jpglen += size;
  }

  (void)stbi_write_jpg_to_func(mywriter, NULL, resolution, resolution, 4, rgba,
                               95);

  return jpglen;
}

struct MHD_Response *MAB_create_response_from_file(const char *filename) {
  FILE *f;
  size_t size, actual;
  char *data;

  f = fopen(filename, "rb");
  if (f == NULL) {
    fprintf(stderr, "could not open %s\n", filename);
    // exit(1);
    return NULL;
  }
  fseek(f, 0, SEEK_END);
  size = ftell(f);
  rewind(f);

  data = malloc(size);
  actual = fread(data, 1, size, f);
  if (actual != size) {
    fprintf(stderr, "didn't read all of %s\n", filename);
    // exit(1);
    free(data);
    fclose(f);
    return NULL;
  }
  fclose(f);

  return MHD_create_response_from_buffer(size, data, MHD_RESPMEM_MUST_FREE);
}

#define ANSWER(name)                                                           \
  int name(void *cls, struct MHD_Connection *conn, const char *url,            \
           const char *method, const char *version, const char *data,          \
           size_t *size, void **con_cls)

struct MHD_Response *error_response;
ANSWER(Error) {
  fprintf(stderr, "404: '%s' '%s'\n", method, url);
  return MHD_queue_response(conn, MHD_HTTP_NOT_FOUND, error_response);
}

ANSWER(Static) {
  struct MHD_Response *response;
  int rc;

  // skip leading slash
  const char *URL = url + 1;

  response = MAB_create_response_from_file(URL);
  if (response == NULL) {
    rc = Error(cls, conn, url, method, version, data, size, con_cls);
  } else {
    rc = MHD_queue_response(conn, MHD_HTTP_OK, response);
    MHD_destroy_response(response);
  }
  return rc;
}

ANSWER(Tile) {
  int rc;
  struct MHD_Response *response;
  void *output;
  size_t outputlen;
  char *renderInfo;
  char *transformed_url;

  const char *info =
      MHD_lookup_connection_value(conn, MHD_HEADER_KIND, "X-MAB-Log-Info");
  if (info != NULL) {
    // fprintf(stderr, "info = '%s'\n", info);
    mabLogContinue(info);
  }

  MAB_WRAP("transform url") {
    pthread_mutex_lock(_fgl_lock);
    _fgl_in_url = (char *)url;

    // signal we're ready for fgl to transform this url
    pthread_barrier_wait(_fgl_barrier);

    // wait for url to be ready
    pthread_barrier_wait(_fgl_barrier);

    transformed_url = strdup((char *)_fgl_out_url);

    // signal we're done with the url
    pthread_barrier_wait(_fgl_barrier);

    pthread_mutex_unlock(_fgl_lock);
  }

  MAB_WRAP("answer tile request") {
    mabLogMessage("rxsize", "%zu", strlen(url));
    if (strlen(transformed_url) == 0) {
      mabLogMessage("@error", "transformed url is empty (FGL error)");
      response = MHD_create_response_from_buffer(strlen("FGL error"), "FGL error",
                                                  MHD_RESPMEM_PERSISTENT);
      rc = MHD_queue_response(conn, MHD_HTTP_NOT_ACCEPTABLE, response);
      MHD_destroy_response(response);
      goto end;
    }
    float x, y, z;
    int max_depth;
    char *dataset, *options;
    if (5 != (rc = sscanf(transformed_url, "/tile/%m[^/]/%f/%f/%f/%ms",
                          &dataset, &z, &x, &y, &options))) {
      mabLogMessage("@error", "sscanf returned %d instead of 5", rc);
      mabLogMessage("options", "%s", options);
      rc = MHD_queue_response(conn, MHD_HTTP_NOT_FOUND, error_response);
      goto end;
    }
    mabLogMessage("x", "%f", x);
    mabLogMessage("y", "%f", y);
    mabLogMessage("z", "%f", z);
    mabLogMessage("dataset", "%s", dataset);
    mabLogMessage("options", "%s", options);

    char *s;

    char *opt_vert = NULL;
    char *opt_frag = NULL;
    GLuint opt_dcIdent = 0;
    GLuint *opt_dcIndex = NULL;
    GLfloat *opt_dcMult = NULL;
    GLfloat *opt_dcOffset = NULL;
    GLfloat opt_dcMinMult = 0.0;
    GLfloat opt_dcMaxMult = 0.0;
    int opt_logGLCalls = 0;
    int opt_doOcclusionCulling = 0;
    int opt_doScissorTest = 0;
    int opt_debugTileBoundaries = 0;
    int opt_debugPartitionBoundaries = 0;
    int opt_doRepartition = 0;

    // fprintf(stderr, "options: '''\n%s\n'''\n", options);

    char *it = options + 1;
    for (;;) {
      char *key = it;

      it = strchrnul(it, ',');
      if (!*it)
        break;
      *it++ = '\0';

      void *value = it;

      it = strchrnul(it, ',');
      int done = *it == '\0';
      *it++ = '\0';

      if (strncmp(value, "base64:", strlen("base64:")) == 0) {
        char *data = value + strlen("base64:");
        size_t len = strlen(data);

        size_t outlen = len + 128;
        char *out = malloc(outlen + 1);
        if (base64decode(data, len, out, &outlen) != 0) {
          mabLogMessage("@error", "base64decode failed");
          return MHD_NO;
        }

        out[outlen] = '\0';
        value = out;
      }

      if (strcmp(key, "vert") == 0) {
        opt_vert = value;
      } else if (strcmp(key, "frag") == 0) {
        opt_frag = value;
      } else if (strcmp(key, "pDepth") == 0) {
        max_depth = atoi(value);
      } else if (strcmp(key, "dcIdent") == 0) {
        opt_dcIdent = atoi(value);
      } else if (strcmp(key, "dcIndex") == 0) {
        opt_dcIndex = value;
      } else if (strcmp(key, "dcMult") == 0) {
        opt_dcMult = value;
      } else if (strcmp(key, "dcOffset") == 0) {
        opt_dcOffset = value;
      } else if (strcmp(key, "dcMinMult") == 0) {
        opt_dcMinMult = atof(value);
      } else if (strcmp(key, "dcMaxMult") == 0) {
        opt_dcMaxMult = atof(value);
      } else if (strcmp(key, "logGLCalls") == 0) {
        opt_logGLCalls = atoi(value);
      } else if (strcmp(key, "doOcclusionCulling") == 0) {
        opt_doOcclusionCulling = atoi(value);
      } else if (strcmp(key, "doScissorTest") == 0) {
        opt_doScissorTest = atoi(value);
      } else if (strcmp(key, "debugTileBoundaries") == 0) {
        opt_debugTileBoundaries = atoi(value);
      } else if (strcmp(key, "debugPartitionBoundaries") == 0) {
        opt_debugPartitionBoundaries = atoi(value);
      } else if (strcmp(key, "doRepartition") == 0) {
        opt_doRepartition = atoi(value);
      } else {
        mabLogMessage("@warn", "Unhandled options '%s' = '%s'", key, (char *)value);
      }

      if (done)
        break;
    }

    mabLogMessage("opt_vert length", "%d", strlen(opt_vert));
    mabLogMessage("opt_frag length", "%d", strlen(opt_frag));
    mabLogMessage("opt_dcIdent", "%u", opt_dcIdent);

    MAB_WRAP("send to render thread") {
      MAB_WRAP("lock")
      pthread_mutex_lock(_lock);

      _max_depth = max_depth;
      _x = x;
      _y = y;
      _z = z;
      _dataset = dataset;
      _vertexShaderSource = opt_vert;
      _fragmentShaderSource = opt_frag;
      _dcIdent = opt_dcIdent;
      _dcIndex = opt_dcIndex;
      _dcMult = opt_dcMult;
      _dcOffset = opt_dcOffset;
      _dcMinMult = opt_dcMinMult;
      _dcMaxMult = opt_dcMaxMult;
      _logGLCalls = opt_logGLCalls;
      _doOcclusionCulling = opt_doOcclusionCulling;
      _doScissorTest = opt_doScissorTest;
      _doRepartition = opt_doRepartition;
      _error = ERROR_NONE;

      mabLogForward(&renderInfo);
      _info = renderInfo;

      // tell render thread to run
      pthread_barrier_wait(_barrier);

      // wait for render to be finished
      pthread_barrier_wait(_barrier);

      if (_error != ERROR_NONE) {
        char *message;
        message = errorMessages[_error];

        mabLogMessage("error", "%s", message);

        response = MHD_create_response_from_buffer(strlen(message), message,
                                                   MHD_RESPMEM_PERSISTENT);
        rc = MHD_queue_response(conn, MHD_HTTP_NOT_ACCEPTABLE, response);
        MHD_destroy_response(response);

        // tell render thread we're done
        pthread_barrier_wait(_barrier);

        MAB_WRAP("unlock")
        pthread_mutex_unlock(_lock);

        goto end;

      } else {
        if (opt_debugPartitionBoundaries) {
          size_t tileres = _resolution;
          size_t maxres = (size_t)pow(2, max_depth);
          size_t res = (size_t)pow(2, z);
          // size_t rx = (size_t)interpolate(0, res, 0, maxres, x);
          // size_t ry = (size_t)interpolate(0, res, 0, maxres, y);

          // assume x and y partition resolution are the same
          size_t numTiles = (size_t)ceil((float)maxres / (float)res);
          size_t distBetweenTiles = tileres / numTiles;

          for (size_t i=0; i<tileres; ++i) {
            for (size_t j=0; j<numTiles; ++j) {
              // left column
              _image[(i)*tileres*4 + (j*distBetweenTiles)*4 + 0] = 0x00;
              _image[(i)*tileres*4 + (j*distBetweenTiles)*4 + 1] = 0xFF;
              _image[(i)*tileres*4 + (j*distBetweenTiles)*4 + 2] = 0x00;
              _image[(i)*tileres*4 + (j*distBetweenTiles)*4 + 3] = 0xFF;

              // top row
              _image[(j*distBetweenTiles)*tileres*4 + (i)*4 + 0] = 0x00;
              _image[(j*distBetweenTiles)*tileres*4 + (i)*4 + 1] = 0xFF;
              _image[(j*distBetweenTiles)*tileres*4 + (i)*4 + 2] = 0x00;
              _image[(j*distBetweenTiles)*tileres*4 + (i)*4 + 3] = 0xFF;
            }
          }
        }

        if (opt_debugTileBoundaries) {
          size_t res = _resolution;
          for (size_t i=0; i<res; ++i) {
            // left column
            _image[i*res*4+0*4+0] = 0xFF;
            _image[i*res*4+0*4+1] = 0x00;
            _image[i*res*4+0*4+2] = 0xFF;
            _image[i*res*4+0*4+3] = 0xFF;

            // top row
            _image[0*res*4+i*4+0] = 0xFF;
            _image[0*res*4+i*4+1] = 0x00;
            _image[0*res*4+i*4+2] = 0xFF;
            _image[0*res*4+i*4+3] = 0xFF;
          }
        }

        MAB_WRAP("jpeg") {
          output = NULL;
          outputlen = 0;
          tojpeg(_image, _resolution, &output, &outputlen);
        }

        // tell render thread we're done
        pthread_barrier_wait(_barrier);

        MAB_WRAP("unlock")
        pthread_mutex_unlock(_lock);
      }
    }

    mabLogMessage("txsize", "%zu", outputlen);

    free(transformed_url);

    response = MHD_create_response_from_buffer(outputlen, output,
                                               MHD_RESPMEM_MUST_FREE);

    const char *origin =
        MHD_lookup_connection_value(conn, MHD_HEADER_KIND, "origin");
    if (origin != NULL) {
      MHD_add_response_header(response, "Access-Control-Allow-Origin", origin);
    }
    MHD_add_response_header(response, "Content-Type", "image/jpeg");
    MHD_add_response_header(response, "Connection", "close");

    rc = MHD_queue_response(conn, MHD_HTTP_OK, response);
    MHD_destroy_response(response);

  end:;
  }
  return rc;
}

ANSWER(Log) {
  struct MHD_Response *response;
  int rc;

  char *message = strdup(url + strlen("/log/"));
  size_t len = MHD_http_unescape(message);

  mabLogDirectly(message);

  response =
      MHD_create_response_from_buffer(len, message, MHD_RESPMEM_MUST_COPY);
  rc = MHD_queue_response(conn, MHD_HTTP_OK, response);
  MHD_destroy_response(response);
  return rc;
}

#define ANSWER_WITH_FILE(TheName, TheFile, TheContentType)                     \
  ANSWER(TheName) {                                                            \
    struct MHD_Response *response;                                             \
    int rc;                                                                    \
                                                                               \
    response = MAB_create_response_from_file(TheFile);                         \
    if (response == NULL) {                                                    \
      printf("Could not read file " TheFile "\n");                             \
      exit(1);                                                                 \
    }                                                                          \
    MHD_add_response_header(response, "Content-Type", TheContentType);         \
    rc = MHD_queue_response(conn, MHD_HTTP_OK, response);                      \
    MHD_destroy_response(response);                                            \
    return rc;                                                                 \
  }

ANSWER_WITH_FILE(IndexHTML         , "static/index.html"           , "text/html")

ANSWER_WITH_FILE(CommonIndexHTML   , "static/common/index.html"    , "text/html")
ANSWER_WITH_FILE(CommonIndexCSS    , "static/common/index.css"     , "text/css")
ANSWER_WITH_FILE(CommonFGJS        , "static/common/fg.js"         , "text/javascript")

ANSWER_WITH_FILE(JSDepsIndexJS     , "static/JS-Deps/index.js"     , "text/javascript")
ANSWER_WITH_FILE(JSDepsIndexJS0    , "static/JS-Deps-0/index.js"   , "text/javascript")
ANSWER_WITH_FILE(JSDepsIndexJS1    , "static/JS-Deps-1/index.js"   , "text/javascript")
ANSWER_WITH_FILE(JSDepsIndexJS2    , "static/JS-Deps-2/index.js"   , "text/javascript")
ANSWER_WITH_FILE(JSDepsIndexJS3    , "static/JS-Deps-3/index.js"   , "text/javascript")
ANSWER_WITH_FILE(JSDepsIndexJS4    , "static/JS-Deps-4/index.js"   , "text/javascript")
ANSWER_WITH_FILE(JSDepsIndexJS5    , "static/JS-Deps-5/index.js"   , "text/javascript")
ANSWER_WITH_FILE(JSDepsIndexJS6    , "static/JS-Deps-6/index.js"   , "text/javascript")
ANSWER_WITH_FILE(JSDepsIndexJS7    , "static/JS-Deps-7/index.js"   , "text/javascript")
ANSWER_WITH_FILE(JSDepsIndexJS8    , "static/JS-Deps-8/index.js"   , "text/javascript")
ANSWER_WITH_FILE(JSDepsIndexJS9    , "static/JS-Deps-9/index.js"   , "text/javascript")
ANSWER_WITH_FILE(SOAnswersIndexJS  , "static/SO-Answers/index.js"  , "text/javascript")
ANSWER_WITH_FILE(SOAnswersIndexJS0 , "static/SO-Answers-0/index.js", "text/javascript")
ANSWER_WITH_FILE(SOAnswersIndexJS1 , "static/SO-Answers-1/index.js", "text/javascript")
ANSWER_WITH_FILE(SOAnswersIndexJS2 , "static/SO-Answers-2/index.js", "text/javascript")
ANSWER_WITH_FILE(SOAnswersIndexJS3 , "static/SO-Answers-3/index.js", "text/javascript")
ANSWER_WITH_FILE(SOAnswersIndexJS4 , "static/SO-Answers-4/index.js", "text/javascript")
ANSWER_WITH_FILE(SOAnswersIndexJS5 , "static/SO-Answers-5/index.js", "text/javascript")
ANSWER_WITH_FILE(SOAnswersIndexJS6 , "static/SO-Answers-6/index.js", "text/javascript")
ANSWER_WITH_FILE(SOAnswersIndexJS7 , "static/SO-Answers-7/index.js", "text/javascript")
ANSWER_WITH_FILE(SOAnswersIndexJS8 , "static/SO-Answers-8/index.js", "text/javascript")
ANSWER_WITH_FILE(SOAnswersIndexJS9 , "static/SO-Answers-9/index.js", "text/javascript")
ANSWER_WITH_FILE(NBERPatentsIndexJS, "static/NBER-Patents/index.js", "text/javascript")
ANSWER_WITH_FILE(NBERPatentsIndexJS0, "static/NBER-Patents-0/index.js", "text/javascript")
ANSWER_WITH_FILE(NBERPatentsIndexJS1, "static/NBER-Patents-1/index.js", "text/javascript")
ANSWER_WITH_FILE(NBERPatentsIndexJS2, "static/NBER-Patents-2/index.js", "text/javascript")
ANSWER_WITH_FILE(NBERPatentsIndexJS3, "static/NBER-Patents-3/index.js", "text/javascript")
ANSWER_WITH_FILE(NBERPatentsIndexJS4, "static/NBER-Patents-4/index.js", "text/javascript")
ANSWER_WITH_FILE(NBERPatentsIndexJS5, "static/NBER-Patents-5/index.js", "text/javascript")
ANSWER_WITH_FILE(NBERPatentsIndexJS6, "static/NBER-Patents-6/index.js", "text/javascript")
ANSWER_WITH_FILE(NBERPatentsIndexJS7, "static/NBER-Patents-7/index.js", "text/javascript")
ANSWER_WITH_FILE(NBERPatentsIndexJS8, "static/NBER-Patents-8/index.js", "text/javascript")
ANSWER_WITH_FILE(NBERPatentsIndexJS9, "static/NBER-Patents-9/index.js", "text/javascript")
ANSWER_WITH_FILE(COMOrkutIndexJS   , "static/COM-Orkut/index.js"   , "text/javascript")

struct {
  const char *method;
  const char *url;
  int exact;
  MHD_AccessHandlerCallback cb;
} routes[] = {
//  { METHOD , URL                      , EXACT , CALLBACK           },
    { NULL   , NULL                     , 0     , Error              },

    { "GET"  , "/"                      , 1     , IndexHTML          },

    { "GET"  , "/JS-Deps/"              , 1     , CommonIndexHTML    },
    { "GET"  , "/JS-Deps-0/"            , 1     , CommonIndexHTML    },
    { "GET"  , "/JS-Deps-1/"            , 1     , CommonIndexHTML    },
    { "GET"  , "/JS-Deps-2/"            , 1     , CommonIndexHTML    },
    { "GET"  , "/JS-Deps-3/"            , 1     , CommonIndexHTML    },
    { "GET"  , "/JS-Deps-4/"            , 1     , CommonIndexHTML    },
    { "GET"  , "/JS-Deps-5/"            , 1     , CommonIndexHTML    },
    { "GET"  , "/JS-Deps-6/"            , 1     , CommonIndexHTML    },
    { "GET"  , "/JS-Deps-7/"            , 1     , CommonIndexHTML    },
    { "GET"  , "/JS-Deps-8/"            , 1     , CommonIndexHTML    },
    { "GET"  , "/JS-Deps-9/"            , 1     , CommonIndexHTML    },
    { "GET"  , "/SO-Answers/"           , 1     , CommonIndexHTML    },
    { "GET"  , "/SO-Answers-0/"         , 1     , CommonIndexHTML    },
    { "GET"  , "/SO-Answers-1/"         , 1     , CommonIndexHTML    },
    { "GET"  , "/SO-Answers-2/"         , 1     , CommonIndexHTML    },
    { "GET"  , "/SO-Answers-3/"         , 1     , CommonIndexHTML    },
    { "GET"  , "/SO-Answers-4/"         , 1     , CommonIndexHTML    },
    { "GET"  , "/SO-Answers-5/"         , 1     , CommonIndexHTML    },
    { "GET"  , "/SO-Answers-6/"         , 1     , CommonIndexHTML    },
    { "GET"  , "/SO-Answers-7/"         , 1     , CommonIndexHTML    },
    { "GET"  , "/SO-Answers-8/"         , 1     , CommonIndexHTML    },
    { "GET"  , "/SO-Answers-9/"         , 1     , CommonIndexHTML    },
    { "GET"  , "/NBER-Patents/"         , 1     , CommonIndexHTML    },
    { "GET"  , "/NBER-Patents-0/"       , 1     , CommonIndexHTML    },
    { "GET"  , "/NBER-Patents-1/"       , 1     , CommonIndexHTML    },
    { "GET"  , "/NBER-Patents-2/"       , 1     , CommonIndexHTML    },
    { "GET"  , "/NBER-Patents-3/"       , 1     , CommonIndexHTML    },
    { "GET"  , "/NBER-Patents-4/"       , 1     , CommonIndexHTML    },
    { "GET"  , "/NBER-Patents-5/"       , 1     , CommonIndexHTML    },
    { "GET"  , "/NBER-Patents-6/"       , 1     , CommonIndexHTML    },
    { "GET"  , "/NBER-Patents-7/"       , 1     , CommonIndexHTML    },
    { "GET"  , "/NBER-Patents-8/"       , 1     , CommonIndexHTML    },
    { "GET"  , "/NBER-Patents-9/"       , 1     , CommonIndexHTML    },
    { "GET"  , "/COM-Orkut/"            , 1     , CommonIndexHTML    },

    { "GET"  , "/JS-Deps/index.css"     , 1     , CommonIndexCSS     },
    { "GET"  , "/JS-Deps-0/index.css"   , 1     , CommonIndexCSS     },
    { "GET"  , "/JS-Deps-1/index.css"   , 1     , CommonIndexCSS     },
    { "GET"  , "/JS-Deps-2/index.css"   , 1     , CommonIndexCSS     },
    { "GET"  , "/JS-Deps-3/index.css"   , 1     , CommonIndexCSS     },
    { "GET"  , "/JS-Deps-4/index.css"   , 1     , CommonIndexCSS     },
    { "GET"  , "/JS-Deps-5/index.css"   , 1     , CommonIndexCSS     },
    { "GET"  , "/JS-Deps-6/index.css"   , 1     , CommonIndexCSS     },
    { "GET"  , "/JS-Deps-7/index.css"   , 1     , CommonIndexCSS     },
    { "GET"  , "/JS-Deps-8/index.css"   , 1     , CommonIndexCSS     },
    { "GET"  , "/JS-Deps-9/index.css"   , 1     , CommonIndexCSS     },
    { "GET"  , "/SO-Answers/index.css"  , 1     , CommonIndexCSS     },
    { "GET"  , "/SO-Answers-0/index.css", 1     , CommonIndexCSS     },
    { "GET"  , "/SO-Answers-1/index.css", 1     , CommonIndexCSS     },
    { "GET"  , "/SO-Answers-2/index.css", 1     , CommonIndexCSS     },
    { "GET"  , "/SO-Answers-3/index.css", 1     , CommonIndexCSS     },
    { "GET"  , "/SO-Answers-4/index.css", 1     , CommonIndexCSS     },
    { "GET"  , "/SO-Answers-5/index.css", 1     , CommonIndexCSS     },
    { "GET"  , "/SO-Answers-6/index.css", 1     , CommonIndexCSS     },
    { "GET"  , "/SO-Answers-7/index.css", 1     , CommonIndexCSS     },
    { "GET"  , "/SO-Answers-8/index.css", 1     , CommonIndexCSS     },
    { "GET"  , "/SO-Answers-9/index.css", 1     , CommonIndexCSS     },
    { "GET"  , "/NBER-Patents/index.css", 1     , CommonIndexCSS     },
    { "GET"  , "/NBER-Patents-0/index.css", 1   , CommonIndexCSS     },
    { "GET"  , "/NBER-Patents-1/index.css", 1   , CommonIndexCSS     },
    { "GET"  , "/NBER-Patents-2/index.css", 1   , CommonIndexCSS     },
    { "GET"  , "/NBER-Patents-3/index.css", 1   , CommonIndexCSS     },
    { "GET"  , "/NBER-Patents-4/index.css", 1   , CommonIndexCSS     },
    { "GET"  , "/NBER-Patents-5/index.css", 1   , CommonIndexCSS     },
    { "GET"  , "/NBER-Patents-6/index.css", 1   , CommonIndexCSS     },
    { "GET"  , "/NBER-Patents-7/index.css", 1   , CommonIndexCSS     },
    { "GET"  , "/NBER-Patents-8/index.css", 1   , CommonIndexCSS     },
    { "GET"  , "/NBER-Patents-9/index.css", 1   , CommonIndexCSS     },
    { "GET"  , "/COM-Orkut/index.css"   , 1     , CommonIndexCSS     },

    { "GET"  , "/JS-Deps/fg.js"         , 1     , CommonFGJS         },
    { "GET"  , "/JS-Deps-0/fg.js"       , 1     , CommonFGJS         },
    { "GET"  , "/JS-Deps-1/fg.js"       , 1     , CommonFGJS         },
    { "GET"  , "/JS-Deps-2/fg.js"       , 1     , CommonFGJS         },
    { "GET"  , "/JS-Deps-3/fg.js"       , 1     , CommonFGJS         },
    { "GET"  , "/JS-Deps-4/fg.js"       , 1     , CommonFGJS         },
    { "GET"  , "/JS-Deps-5/fg.js"       , 1     , CommonFGJS         },
    { "GET"  , "/JS-Deps-6/fg.js"       , 1     , CommonFGJS         },
    { "GET"  , "/JS-Deps-7/fg.js"       , 1     , CommonFGJS         },
    { "GET"  , "/JS-Deps-8/fg.js"       , 1     , CommonFGJS         },
    { "GET"  , "/JS-Deps-9/fg.js"       , 1     , CommonFGJS         },
    { "GET"  , "/SO-Answers/fg.js"      , 1     , CommonFGJS         },
    { "GET"  , "/SO-Answers-0/fg.js"    , 1     , CommonFGJS         },
    { "GET"  , "/SO-Answers-1/fg.js"    , 1     , CommonFGJS         },
    { "GET"  , "/SO-Answers-2/fg.js"    , 1     , CommonFGJS         },
    { "GET"  , "/SO-Answers-3/fg.js"    , 1     , CommonFGJS         },
    { "GET"  , "/SO-Answers-4/fg.js"    , 1     , CommonFGJS         },
    { "GET"  , "/SO-Answers-5/fg.js"    , 1     , CommonFGJS         },
    { "GET"  , "/SO-Answers-6/fg.js"    , 1     , CommonFGJS         },
    { "GET"  , "/SO-Answers-7/fg.js"    , 1     , CommonFGJS         },
    { "GET"  , "/SO-Answers-8/fg.js"    , 1     , CommonFGJS         },
    { "GET"  , "/SO-Answers-9/fg.js"    , 1     , CommonFGJS         },
    { "GET"  , "/NBER-Patents/fg.js"    , 1     , CommonFGJS         },
    { "GET"  , "/NBER-Patents-0/fg.js"  , 1     , CommonFGJS         },
    { "GET"  , "/NBER-Patents-1/fg.js"  , 1     , CommonFGJS         },
    { "GET"  , "/NBER-Patents-2/fg.js"  , 1     , CommonFGJS         },
    { "GET"  , "/NBER-Patents-3/fg.js"  , 1     , CommonFGJS         },
    { "GET"  , "/NBER-Patents-4/fg.js"  , 1     , CommonFGJS         },
    { "GET"  , "/NBER-Patents-5/fg.js"  , 1     , CommonFGJS         },
    { "GET"  , "/NBER-Patents-6/fg.js"  , 1     , CommonFGJS         },
    { "GET"  , "/NBER-Patents-7/fg.js"  , 1     , CommonFGJS         },
    { "GET"  , "/NBER-Patents-8/fg.js"  , 1     , CommonFGJS         },
    { "GET"  , "/NBER-Patents-9/fg.js"  , 1     , CommonFGJS         },
    { "GET"  , "/COM-Orkut/fg.js"       , 1     , CommonFGJS         },

    { "GET"  , "/JS-Deps/index.js"      , 1     , JSDepsIndexJS      },
    { "GET"  , "/JS-Deps-0/index.js"    , 1     , JSDepsIndexJS0     },
    { "GET"  , "/JS-Deps-1/index.js"    , 1     , JSDepsIndexJS1     },
    { "GET"  , "/JS-Deps-2/index.js"    , 1     , JSDepsIndexJS2     },
    { "GET"  , "/JS-Deps-3/index.js"    , 1     , JSDepsIndexJS3     },
    { "GET"  , "/JS-Deps-4/index.js"    , 1     , JSDepsIndexJS4     },
    { "GET"  , "/JS-Deps-5/index.js"    , 1     , JSDepsIndexJS5     },
    { "GET"  , "/JS-Deps-6/index.js"    , 1     , JSDepsIndexJS6     },
    { "GET"  , "/JS-Deps-7/index.js"    , 1     , JSDepsIndexJS7     },
    { "GET"  , "/JS-Deps-8/index.js"    , 1     , JSDepsIndexJS8     },
    { "GET"  , "/JS-Deps-9/index.js"    , 1     , JSDepsIndexJS9     },
    { "GET"  , "/SO-Answers/index.js"   , 1     , SOAnswersIndexJS   },
    { "GET"  , "/SO-Answers-0/index.js" , 1     , SOAnswersIndexJS0  },
    { "GET"  , "/SO-Answers-1/index.js" , 1     , SOAnswersIndexJS1  },
    { "GET"  , "/SO-Answers-2/index.js" , 1     , SOAnswersIndexJS2  },
    { "GET"  , "/SO-Answers-3/index.js" , 1     , SOAnswersIndexJS3  },
    { "GET"  , "/SO-Answers-4/index.js" , 1     , SOAnswersIndexJS4  },
    { "GET"  , "/SO-Answers-5/index.js" , 1     , SOAnswersIndexJS5  },
    { "GET"  , "/SO-Answers-6/index.js" , 1     , SOAnswersIndexJS6  },
    { "GET"  , "/SO-Answers-7/index.js" , 1     , SOAnswersIndexJS7  },
    { "GET"  , "/SO-Answers-8/index.js" , 1     , SOAnswersIndexJS8  },
    { "GET"  , "/SO-Answers-9/index.js" , 1     , SOAnswersIndexJS9  },
    { "GET"  , "/NBER-Patents/index.js" , 1     , NBERPatentsIndexJS },
    { "GET"  , "/NBER-Patents-0/index.js", 1    , NBERPatentsIndexJS0 },
    { "GET"  , "/NBER-Patents-1/index.js", 1    , NBERPatentsIndexJS1 },
    { "GET"  , "/NBER-Patents-2/index.js", 1    , NBERPatentsIndexJS2 },
    { "GET"  , "/NBER-Patents-3/index.js", 1    , NBERPatentsIndexJS3 },
    { "GET"  , "/NBER-Patents-4/index.js", 1    , NBERPatentsIndexJS4 },
    { "GET"  , "/NBER-Patents-5/index.js", 1    , NBERPatentsIndexJS5 },
    { "GET"  , "/NBER-Patents-6/index.js", 1    , NBERPatentsIndexJS6 },
    { "GET"  , "/NBER-Patents-7/index.js", 1    , NBERPatentsIndexJS7 },
    { "GET"  , "/NBER-Patents-8/index.js", 1    , NBERPatentsIndexJS8 },
    { "GET"  , "/NBER-Patents-9/index.js", 1    , NBERPatentsIndexJS9 },
    { "GET"  , "//OM-Orkut/index.js"    , 1     , COMOrkutIndexJS    },

//  { "GET"  , "/static/"               , 0     , Static             },
    { "GET"  , "/tile/"                 , 0     , Tile               },
    { "POST" , "/log/"                  , 0     , Log                },
};

ANSWER(answer) {
  size_t i;

  for (i = 1; i < sizeof(routes) / sizeof(*routes); ++i) {
    if (strcmp(method, routes[i].method) != 0)
      continue;
    if (routes[i].exact && strcmp(url, routes[i].url) != 0)
      continue;
    if (!routes[i].exact &&
        strncmp(url, routes[i].url, strlen(routes[i].url)) != 0)
      continue;

    return routes[i].cb(cls, conn, url, method, version, data, size, con_cls);
  }

  return routes[0].cb(cls, conn, url, method, version, data, size, con_cls);
}

static void initialize(void) {
  error_response = MAB_create_response_from_file("static/error.html");
  if (error_response == NULL) {
    printf("Could not read file static/error.html");
    exit(1);
  }
}

static void *fgl_transformer_thread(void *v) {
  int proc_in[2], proc_out[2];
  FILE *proc_stdin, *proc_stdout;
  ssize_t len;
  size_t size;
  char *line;

  (void)v;

  pipe2(proc_in, O_CLOEXEC);
  pipe2(proc_out, O_CLOEXEC);

  if (fork() == 0) {
    close(proc_in[1]);
    close(proc_out[0]);

    dup2(proc_in[0], 0);
    dup2(proc_out[1], 1);

    execlp("/usr/bin/python3.8", "/usr/bin/python3.8", "/opt/fgl/fgl.py", (char *)NULL);
    perror("execlp");
    exit(1);
  } else {
    close(proc_in[0]);
    close(proc_out[1]);

    proc_stdin = fdopen(proc_in[1], "w");
    proc_stdout = fdopen(proc_out[0], "r");
  }

  // tell main we're ready
  pthread_barrier_wait(_fgl_barrier);

  line = NULL;
  size = 0;
  for (;;)
  MAB_WRAP("transform url") {
    // wait for url from request thread
    pthread_barrier_wait(_fgl_barrier);

    mabLogMessage("input url", "%s", _fgl_in_url);

    fprintf(proc_stdin, "%s\n", _fgl_in_url);
    fflush(proc_stdin);

    len = getline(&line, &size, proc_stdout);
    if (len < 0) {
      int myerrno = errno;
      mabLogMessage("@error", "getline failed; probably at EOF");
      mabLogMessage("errno", "%d", myerrno);
      mabLogMessage("len", "%zd", len);
      exit(1);
    }

    _fgl_out_url = line;

    // signal request thread that url is ready
    pthread_barrier_wait(_fgl_barrier);

    // wait for request thread to be done
    pthread_barrier_wait(_fgl_barrier);
  }

  printf("fgl done\n");
}

int main(int argc, char **argv) {
  int opt_port, opt_service;
  char *s, *opt_bind, *opt_logfile;
  struct MHD_Daemon *daemon;
  struct sockaddr_in addr;
  pthread_t tid;

  opt_logfile = (s = getenv("FG_LOGFILE")) ? s : NULL;
  if (opt_logfile == NULL) {
    for (int i = 0; i < 64; ++i) {
      char temp[32];
      snprintf(temp, sizeof(temp), "logs/%d.log", i);

      if (!mabLogToFile(temp, "wx")) {
        goto got_log;
        break;
      }
    }

  } else {
    if (!mabLogToFile(opt_logfile, "w")) {
      mabLogFlush(1);
      goto got_log;
    }
  }

  fprintf(stderr, "could not open log\n");
  fprintf(stderr, "It's likely that the logs/ directory is full. Remove unneeded .log files\n");
  return 1;

got_log:
  _lock = malloc(sizeof(*_lock));
  pthread_mutex_init(_lock, NULL);

  _barrier = malloc(sizeof(*_barrier));
  pthread_barrier_init(_barrier, NULL, 2);

  pthread_create(&tid, NULL, render, NULL);
  pthread_setname_np(tid, "render");

  pthread_barrier_wait(_barrier);

  _fgl_lock = malloc(sizeof(*_fgl_lock));
  pthread_mutex_init(_fgl_lock, NULL);

  _fgl_barrier = malloc(sizeof(*_fgl_barrier));
  pthread_barrier_init(_fgl_barrier, NULL, 2);

  pthread_create(&tid, NULL, fgl_transformer_thread, NULL);
  pthread_setname_np(tid, "transformer");

  pthread_barrier_wait(_fgl_barrier);

  MAB_WRAP("server main loop") {
    opt_port = (s = getenv("FG_PORT")) ? atoi(s) : 8889;
    opt_bind = (s = getenv("FG_BIND")) ? s : "0.0.0.0";
    opt_service = (s = getenv("FG_SERVICE")) ? atoi(s) : 0;

    mabLogMessage("port", "%d", opt_port);
    mabLogMessage("bind", "%s", opt_bind);

    fprintf(stderr, "Listening on %s:%d\n", opt_bind, opt_port);

    mabLogAction("initialize");
    initialize();
    mabLogEnd(NULL);

    memset(&addr, sizeof(addr), 0);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(opt_port);
    inet_aton(opt_bind, &addr.sin_addr);

    daemon =
        MHD_start_daemon(MHD_USE_THREAD_PER_CONNECTION, 0, NULL, NULL, &answer,
                         NULL, MHD_OPTION_SOCK_ADDR, &addr, MHD_OPTION_END);
    if (daemon == NULL) {
      fprintf(stderr, "could not start daemon\n");
      return 1;
    }

    if (opt_service) {
      while (sleep(60 * 60) == 0)
        ;
    } else {
      fprintf(stderr, "press enter to stop\n");
      getchar();
    }
    fprintf(stderr, "stopping...\n");

    MHD_stop_daemon(daemon);
  }
  return 0;
}

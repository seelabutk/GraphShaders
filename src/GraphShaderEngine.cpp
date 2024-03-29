// For alignment:
//
//345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789\

#include "debug-trap.h"

// c++ stdlib
#include <string>
#include <tuple>
#include <vector>
#include <map>

// c stdlib
#include <arpa/inet.h>
#include <cstdio>
#include <errno.h>
#include <fcntl.h>
#include <fcntl.h>
#include <math.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <zlib.h>

// OpenGL / GLAD
#define GLAPIENTRY APIENTRY
#include <glad/glad.h>
#include <glad/glad_egl.h>

// STB
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"


//--- Utilities


static void dief(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "Error: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    exit(EXIT_FAILURE);
}

static void die(const char *msg) {
    return dief("%s", msg);
}

#define X_OPTION(PARSE, FORMAT, ...)                                                                                   \
    ({                                                                                                                 \
        char name[128];                                                                                                \
        std::snprintf(name, sizeof(name), FORMAT, ## __VA_ARGS__);                                                     \
        const char *s = getenv(name);                                                                                  \
        if (!s) dief("Missing required option: %s", name);                                                             \
        (PARSE)(s);                                                                                                    \
    })                                                                                                                 \
/* X_OPTION */


namespace gs {

struct Environment {
    std::map<std::string, std::string> app_gs_options;

    void init();
    void done();

    void clearenv() {
        app_gs_options.clear();
    }

    void setenv(const std::string &name, const std::string &value) {
        app_gs_options[name] = value;
    }

    const char *getenv(const std::string &name) {
        auto it = app_gs_options.find(name);
        if (it == app_gs_options.end()) {
            return nullptr;
        }

        return it->second.c_str();
    }
};

struct EGL : public Environment {
    void init();
    void done();
};

struct GL : public EGL {
    GLsizei opt_gs_tile_width;
    GLsizei opt_gs_tile_height;

    GLuint app_gl_framebuffer;

    void init();
    void done();
};

struct Data : public GL {
    GLint opt_gs_buffer_count;
    std::vector<std::string> opt_gs_buffer_kinds;
    std::vector<std::string> opt_gs_buffer_names;
    std::vector<std::string> opt_gs_buffer_files;
    std::vector<std::string> opt_gs_buffer_sizes;
    std::vector<std::string> opt_gs_buffer_types;
    std::vector<std::string> opt_gs_buffer_binds;

    std::vector<GLuint> app_gl_buffers;
    std::vector<GLsizeiptr> app_gl_buffer_sizes;
    GLsizeiptr app_gs_edge_count;
    GLsizeiptr app_gs_node_count;

    struct DataBuffer {
        const char *buffer;
        size_t buflen;
    };

    std::map<std::string, DataBuffer> app_data_buffers;

    void set(const std::string &name, const char *buffer, size_t buflen);
    void init();
    void done();
};

struct Shader : public Data {
    std::string opt_gs_shader_common;
    std::string opt_gs_shader_vertex;
    std::string opt_gs_shader_geometry;
    std::string opt_gs_shader_fragment;

    GLuint app_gl_program;

    void init();
    void done();
};

struct Render : public Shader {
    GLfloat opt_gs_tile_x;
    GLfloat opt_gs_tile_y;
    GLfloat opt_gs_tile_z;

    GLsizei app_rgba_width;
    GLsizei app_rgba_height;
    GLvoid *app_rgba_data;
    std::vector<uint8_t> app_jpeg;

    void init();
    void get(void **jpeg, size_t *jpeglen) {
        *jpeg = app_jpeg.data();
        *jpeglen = app_jpeg.size();
    }

    void done();
};

using Application = Render;


void Environment::init() {
} /* Environment::init */


void EGL::init() {

    //--- Setup EGL

    EGLBoolean egl_success;

    if (!gladLoadEGL()) {
        dief("Failed to load EGL");
    }


    // The default display should be sufficient for offscreen rendering.

    EGLDisplay egl_display;
    egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (egl_display == EGL_NO_DISPLAY) {
        dief("Failed to get egl display");
    }


    // Then we have to tell EGL we want to use that display

    egl_success = eglInitialize(egl_display, NULL, NULL);
    if (egl_success == EGL_FALSE) {
        dief("Failed to initialize");
    }


    // We require this to pass EGL_NO_SURFACE to eglMakeCurrent.

    if (!EGL_KHR_surfaceless_context) {
        dief("Failed to get EGL_KHR_surfaceless_context");
    }


    // We require this to pass EGL_NO_CONFIG_KHR to eglCreateContext

    if (!EGL_KHR_no_config_context) {
        dief("Failed to get EGL_KHR_no_config_context");
    }


    // Print out EGL constants

#   define X_QUERY(DISPLAY, NAME)                                                                                      \
    ({                                                                                                                 \
        char const *string;                                                                                            \
        EGLDisplay display = (DISPLAY);                                                                                \
        EGLint name = (NAME);                                                                                          \
        string = eglQueryString(display, name);                                                                        \
        std::fprintf(stderr, "%s: %s\n",                                                                               \
            #NAME, string);                                                                                            \
    })                                                                                                                 \
    /* X_QUERY */

    X_QUERY(egl_display, EGL_VERSION);
    X_QUERY(egl_display, EGL_VENDOR);
    X_QUERY(egl_display, EGL_CLIENT_APIS);
    X_QUERY(egl_display, EGL_EXTENSIONS);

#   undef X_QUERY


    // We have to tell EGL that we'll be using OpenGL and not GLES

    egl_success = eglBindAPI(EGL_OPENGL_API);
    if (egl_success == EGL_FALSE) {
        dief("Failed to bind api");
    }


    // OpenGL 4.6+ is needed for some global buffer objects

    EGLint const egl_context_attribute_list[] = {
        EGL_CONTEXT_MAJOR_VERSION, 4,
        EGL_CONTEXT_MINOR_VERSION, 6,
        EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
        EGL_CONTEXT_OPENGL_DEBUG, EGL_TRUE,
        EGL_NONE,
    };
    EGLContext egl_context;
    egl_context = eglCreateContext(egl_display, EGL_NO_CONFIG_KHR, EGL_NO_CONTEXT, egl_context_attribute_list);
    if (egl_context == EGL_NO_CONTEXT) {
        dief("Failed to create context");
    }


    // Finally, we need OpenGL to use the EGL functions

    egl_success = eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, egl_context);
    if (egl_success == EGL_FALSE) {
        dief("Failed to make current");
    }
} /* EGL::init */

void GL::init() {

    //--- Setup OpenGL


    // Use GLAD to populate the OpenGL function pointers

    if (!gladLoadGL()) {
        dief("Failed to load GL");
    }


    // Print out OpenGL constants

#   define X_QUERY(NAME)                                                                                               \
    ({                                                                                                                 \
        const GLubyte *string;                                                                                         \
        GLenum name = (NAME);                                                                                          \
        string = glGetString(name);                                                                                    \
        std::fprintf(stderr, "%s: %s\n",                                                                               \
            #NAME, string);                                                                                            \
    })                                                                                                                 \
    /* X_QUERY */

    X_QUERY(GL_VENDOR);
    X_QUERY(GL_VERSION);
    X_QUERY(GL_RENDERER);
    X_QUERY(GL_SHADING_LANGUAGE_VERSION);

#   undef X_QUERY


    // We should print out any debugging information

    {
        GLenum source = GL_DONT_CARE;
        GLenum type = GL_DONT_CARE;
        GLenum severity = GL_DEBUG_SEVERITY_NOTIFICATION;
        GLsizei count = 0;
        const GLuint *ids = nullptr;
        GLboolean enabled = GL_FALSE;
        const void *userParam = nullptr;

        // Thanks https://stackoverflow.com/a/56145419
        auto callback = +[](GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *userParam) -> void {
            if (type == 0x8250 && severity == 0x9147) {
                // The Nvidia driver outputs some warning about having to
                // recompile the shaders, but we don't really care because it
                // still works. The constants here correspond to this message:
                //
                //   DEBUG:type=0x8250:severity=0x9147: Program/shader state performance warning: Geometry shader in program 4 is being recompiled based on GL state, and was not found in the disk cache 
                //
                return;
            }

            std::fprintf(stderr, "DEBUG:type=0x%x:severity=0x%x: %s\n", type, severity, message);
            // psnip_trap();
        };

        glEnable(GL_DEBUG_OUTPUT);
        glDebugMessageControl(source, type, severity, count, ids, enabled);
        glDebugMessageCallback(callback, userParam);
    }


    // We need somewhere to store the color data from our framebuffer

    opt_gs_tile_width = X_OPTION(std::stoi, "GS_TILE_WIDTH");
    opt_gs_tile_height = X_OPTION(std::stoi, "GS_TILE_HEIGHT");

    GLuint gl_framebuffer_texture_color;
    glGenTextures(1, &gl_framebuffer_texture_color);
    glBindTexture(GL_TEXTURE_2D, gl_framebuffer_texture_color);
    {
        GLint level = 0;
        GLint internalformat = GL_RGBA;
        GLsizei width = opt_gs_tile_width;
        GLsizei height = opt_gs_tile_height;
        GLint border = 0;
        GLenum format = GL_RGBA;
        GLenum type = GL_UNSIGNED_BYTE;
        const void *data = nullptr;

        glTexImage2D(GL_TEXTURE_2D, level, internalformat, width, height, border, format, type, data);
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);


    // We need somewhere to store our depth and stencil buffers

    GLuint gl_framebuffer_texture_depth_and_stencil;
    glGenTextures(1, &gl_framebuffer_texture_depth_and_stencil);
    glBindTexture(GL_TEXTURE_2D, gl_framebuffer_texture_depth_and_stencil);
    {
        GLint level = 0;
        GLint internalformat = GL_DEPTH24_STENCIL8;
        GLsizei width = opt_gs_tile_width;
        GLsizei height = opt_gs_tile_height;
        GLint border = 0;
        GLenum format = GL_DEPTH_STENCIL;
        GLenum type = GL_UNSIGNED_INT_24_8;
        const void *data = nullptr;

        glTexImage2D(GL_TEXTURE_2D, level, internalformat, width, height, border, format, type, data);
    }


    // We need a framebuffer

    app_gl_framebuffer = ({
        GLuint gl_framebuffer;
        glGenFramebuffers(1, &gl_framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, gl_framebuffer);
        {
            GLenum attachment = GL_COLOR_ATTACHMENT0;
            GLuint textarget = GL_TEXTURE_2D;
            GLuint texture = gl_framebuffer_texture_color;
            GLint level = 0;
            glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, textarget, texture, level);
        }

        gl_framebuffer;
    });

    ({
        GLenum attachment = GL_DEPTH_STENCIL_ATTACHMENT;
        GLuint textarget = GL_TEXTURE_2D;
        GLuint texture = gl_framebuffer_texture_depth_and_stencil;
        GLint level = 0;
        glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, textarget, texture, level);
    });

    ({
        GLenum status;
        status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            dief("Failed to complete framebuffer");
        }
    });


    //--- Configure Features

    glDisable(GL_DEPTH_TEST);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


    // Prepare the viewport

    {
        GLint x = 0;
        GLint y = 0;
        GLsizei width = opt_gs_tile_width;
        GLsizei height = opt_gs_tile_height;

        glViewport(x, y, width, height);

        // Sanity check: EGL can choose to not implement glViewport which we can
        // check by querying the viewport after setting it.

        GLint viewport[4];
        glGetIntegerv(GL_VIEWPORT, viewport);
        if ((x != viewport[0]) || (y != viewport[1]) || (width != viewport[2]) || (height != viewport[3])) {
            dief("Failed to set viewport correctly: expected (%d, %d, %d, %d); got (%d, %d, %d, %d)", x, y, (int)width, (int)height, viewport[0], viewport[1], viewport[2], viewport[3]);
        }
    }
} /* GL::init */


void Data::set(const std::string &name, const char *buffer, size_t buflen) {
    app_data_buffers.emplace(name, DataBuffer{ buffer, buflen });
} /* Data::set */


void Data::init() {

    //--- We need a Vertex Array Object (VAO)

    GLuint gl_vertex_array;
    glGenVertexArrays(1, &gl_vertex_array);
    glBindVertexArray(gl_vertex_array);

    // Load node attributes

#   define X_MAKE_BUFFER(TARGET)                                                                                       \
    ({                                                                                                                 \
        GLuint buffer;                                                                                                 \
        glGenBuffers(1, &buffer);                                                                                      \
                                                                                                                       \
        GLenum target = (TARGET);                                                                      \
        glBindBuffer(target, buffer);                                                                                  \
                                                                                                                       \
        buffer;                                                                                                        \
    })                                                                                                                 \
    /* X_MAKE_BUFFER */

#   define X_BUFFER_FROM_FILE(TARGET, PATTERN, ...)                                                                    \
    ({                                                                                                                 \
        char name[256];                                                                                                \
        snprintf(name, sizeof(name), PATTERN, ## __VA_ARGS__);                                                         \
                                                                                                                       \
        std::fprintf(stderr, "Read from %s\n", name);                                                                  \
                                                                                                                       \
        FILE *file = fopen(name, "rb");                                                                                \
        if (!file) {                                                                                                   \
            dief("fopen: %s\n", name);                                                                                 \
        }                                                                                                              \
                                                                                                                       \
        int success;                                                                                                   \
        success = fseek(file, 0L, SEEK_END);                                                                           \
        if (success != 0) {                                                                                            \
            dief("fseek: %s\n", name);                                                                                 \
        }                                                                                                              \
                                                                                                                       \
        long size;                                                                                                     \
        size = ftell(file);                                                                                            \
        if (size < 0) {                                                                                                \
            dief("ftell: %s\n", name);                                                                                 \
        }                                                                                                              \
                                                                                                                       \
        success = fseek(file, 0L, SEEK_SET);                                                                           \
        if (success != 0) {                                                                                            \
            dief("fseek: %s\n", name);                                                                                 \
        }                                                                                                              \
                                                                                                                       \
        void *data;                                                                                                    \
        data = new char[size];                                                                                         \
        if (data == nullptr) {                                                                                         \
            dief("new: %ld\n", size);                                                                                  \
        }                                                                                                              \
                                                                                                                       \
        long nread;                                                                                                    \
        nread = fread(data, 1, size, file);                                                                            \
        if (nread < size) {                                                                                            \
            dief("fread: %s\n", name);                                                                                 \
        }                                                                                                              \
                                                                                                                       \
        GLenum target = (TARGET);                                                                                      \
        glBufferData(target, size, data, GL_STATIC_DRAW);                                                              \
                                                                                                                       \
        size;                                                                                                          \
    })                                                                                                                 \
    /* X_BUFFER_FROM_FILE */

#   define X_BUFFER_FROM_ZERO(TARGET, USAGE, SIZE, INTERNALFORMAT, FORMAT, TYPE)                                       \
    ({                                                                                                                 \
        GLenum target = (TARGET);                                                                                      \
        GLsizeiptr size = (SIZE);                                                                                      \
        GLvoid *data = nullptr;                                                                                        \
        GLenum usage = (USAGE);                                                                                        \
        glBufferData(target, size, data, usage);                                                                       \
                                                                                                                       \
        GLenum internalformat = (INTERNALFORMAT);                                                                      \
        GLenum format = (FORMAT);                                                                                      \
        GLenum type = (TYPE);                                                                                          \
        glClearBufferData(target, internalformat, format, type, data);                                                 \
                                                                                                                       \
        size;                                                                                                          \
    })                                                                                                                 \
    /* X_BUFFER_FROM_ZERO */

#   define X_BUFFER_FROM_MEMORY(TARGET, USAGE, MEM, MEMLEN) \
    ({ \
        GLenum target = (TARGET);                                                                                      \
        GLsizeiptr size = (MEMLEN);                                                                                    \
        const GLvoid *data = static_cast<const GLvoid *>(MEM);                                                         \
        GLenum usage = (USAGE);                                                                                        \
        glBufferData(target, size, data, usage);                                                                       \
                                                                                                                       \
        size;                                                                                                          \
    }) \
    /* X_BUFFER_FROM_MEMORY */

    opt_gs_buffer_count = X_OPTION(std::stoi, "GS_BUFFER_COUNT");
    opt_gs_buffer_kinds.resize(opt_gs_buffer_count);
    opt_gs_buffer_names.resize(opt_gs_buffer_count);
    opt_gs_buffer_files.resize(opt_gs_buffer_count);
    opt_gs_buffer_sizes.resize(opt_gs_buffer_count);
    opt_gs_buffer_types.resize(opt_gs_buffer_count);
    opt_gs_buffer_binds.resize(opt_gs_buffer_count);
    for (GLint i=0, n=opt_gs_buffer_count; i<n; ++i) {
        opt_gs_buffer_kinds[i] = X_OPTION(std::string, "GS_BUFFER_KIND_%d", i);
        opt_gs_buffer_names[i] = X_OPTION(std::string, "GS_BUFFER_NAME_%d", i);
        opt_gs_buffer_files[i] = X_OPTION(std::string, "GS_BUFFER_FILE_%d", i);
        opt_gs_buffer_sizes[i] = X_OPTION(std::string, "GS_BUFFER_SIZE_%d", i);
        opt_gs_buffer_types[i] = X_OPTION(std::string, "GS_BUFFER_TYPE_%d", i);
        opt_gs_buffer_binds[i] = X_OPTION(std::string, "GS_BUFFER_BIND_%d", i);
    }

    app_gl_buffers.resize(opt_gs_buffer_count);
    app_gl_buffer_sizes.resize(opt_gs_buffer_count);

    app_gs_edge_count = 0;
    app_gs_node_count = 0;
    for (GLint i=0, n=opt_gs_buffer_count; i<n; ++i) {
        std::string &opt_gs_buffer_name = opt_gs_buffer_names[i];
        std::string &opt_gs_buffer_kind = opt_gs_buffer_kinds[i];
        std::string &opt_gs_buffer_file = opt_gs_buffer_files[i];
        std::string &opt_gs_buffer_size = opt_gs_buffer_sizes[i];
        std::string &opt_gs_buffer_type = opt_gs_buffer_types[i];

#       define X(suffix) \
        std::fprintf(stderr, ("  " #suffix ": %s\n"), opt_gs_buffer_##suffix.c_str())

        std::fprintf(stderr, "buffer %d\n", i);
        X(name);
        X(kind);
        X(file);
        X(size);
        X(type);

#       undef X

        auto it = app_data_buffers.find(opt_gs_buffer_name);
        const char *app_data_buffer = it != app_data_buffers.end()
            ? it->second.buffer
            : nullptr;
        size_t app_data_buflen = it != app_data_buffers.end()
            ? it->second.buflen
            : 0;
        
        std::fprintf(stderr, "app_data_buffer = %p; app_data_buflen = %zu\n", app_data_buffer, app_data_buflen);

        if (opt_gs_buffer_kind == "ATTRIBUTE") {
            app_gl_buffers[i] = X_MAKE_BUFFER(GL_SHADER_STORAGE_BUFFER);
            app_gl_buffer_sizes[i] = app_data_buffer
                ? X_BUFFER_FROM_MEMORY(GL_SHADER_STORAGE_BUFFER, GL_STATIC_DRAW, app_data_buffer, app_data_buflen)
                : X_BUFFER_FROM_FILE(GL_SHADER_STORAGE_BUFFER, "%s", opt_gs_buffer_file.c_str());

            if (opt_gs_buffer_type == "GL_FLOAT" && opt_gs_buffer_size == "N") {
                app_gs_node_count = app_gl_buffer_sizes[i] / sizeof(GLfloat);

            } else if (opt_gs_buffer_type == "GL_UNSIGNED_INT" && opt_gs_buffer_size == "N") {
                app_gs_node_count = app_gl_buffer_sizes[i] / sizeof(GLuint);

            } else if (opt_gs_buffer_type == "GL_UNSIGNED_INT" && opt_gs_buffer_size == "E") {
                app_gs_edge_count = app_gl_buffer_sizes[i] / sizeof(GLuint);

            } else {
                dief("2Unrecognized buffer: kind=%s; file=%s; size=%s; type=%s",
                    opt_gs_buffer_kind.c_str(), opt_gs_buffer_file.c_str(), opt_gs_buffer_size.c_str(), opt_gs_buffer_type.c_str());
            }

        } else if (opt_gs_buffer_kind == "ELEMENT") {
            app_gl_buffers[i] = X_MAKE_BUFFER(GL_ELEMENT_ARRAY_BUFFER);
            app_gl_buffer_sizes[i] = app_data_buffer
                ? X_BUFFER_FROM_MEMORY(GL_ELEMENT_ARRAY_BUFFER, GL_STATIC_DRAW, app_data_buffer, app_data_buflen)
                : X_BUFFER_FROM_FILE(GL_ELEMENT_ARRAY_BUFFER, "%s", opt_gs_buffer_file.c_str());

            if (opt_gs_buffer_type == "GL_UNSIGNED_INT" && opt_gs_buffer_size == "2E") {
                app_gs_edge_count = app_gl_buffer_sizes[i] / sizeof(GLuint) / 2;

            } else {
                dief("1Unrecognized buffer: kind=%s; file=%s; size=%s; type=%s",
                    opt_gs_buffer_kind.c_str(), opt_gs_buffer_file.c_str(), opt_gs_buffer_size.c_str(), opt_gs_buffer_type.c_str());
            }

        }
    }

    if (app_gs_edge_count == 0) {
        dief("Failed to read any edges");
    }

    if (app_gs_node_count == 0) {
        dief("Failed to read any nodes");
    }

    for (GLint i=0, n=opt_gs_buffer_count; i<n; ++i) {
        std::string &opt_gs_buffer_kind = opt_gs_buffer_kinds[i];
        std::string &opt_gs_buffer_file = opt_gs_buffer_files[i];
        std::string &opt_gs_buffer_size = opt_gs_buffer_sizes[i];
        std::string &opt_gs_buffer_type = opt_gs_buffer_types[i];
        std::string &opt_gs_buffer_name = opt_gs_buffer_names[i];

        if (opt_gs_buffer_kind == "ATOMIC") {
            app_gl_buffers[i] = X_MAKE_BUFFER(GL_ATOMIC_COUNTER_BUFFER);

            if (opt_gs_buffer_type == "GL_UNSIGNED_INT") {
                app_gl_buffer_sizes[i] = X_BUFFER_FROM_ZERO(GL_ATOMIC_COUNTER_BUFFER, GL_DYNAMIC_COPY, std::stoi(opt_gs_buffer_size), GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT);

            } else {
                dief("3Unrecognized buffer: kind=%s; file=%s; size=%s; type=%s",
                    opt_gs_buffer_kind.c_str(), opt_gs_buffer_file.c_str(), opt_gs_buffer_size.c_str(), opt_gs_buffer_type.c_str());
            }

        } else if (opt_gs_buffer_kind == "SCRATCH") {
            app_gl_buffers[i] = X_MAKE_BUFFER(GL_SHADER_STORAGE_BUFFER);

            if (opt_gs_buffer_type == "GL_UNSIGNED_INT" && opt_gs_buffer_size == "N") {
                app_gl_buffer_sizes[i] = X_BUFFER_FROM_ZERO(GL_SHADER_STORAGE_BUFFER, GL_DYNAMIC_COPY, app_gs_node_count * sizeof(GLuint), GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT);

            } else if (opt_gs_buffer_type == "GL_UNSIGNED_INT" && opt_gs_buffer_size == "E") {
                app_gl_buffer_sizes[i] = X_BUFFER_FROM_ZERO(GL_SHADER_STORAGE_BUFFER, GL_DYNAMIC_COPY, app_gs_edge_count * sizeof(GLuint), GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT);

            } else {
                dief("4Unrecognized buffer: kind=%s; file=%s; size=%s; type=%s",
                    opt_gs_buffer_kind.c_str(), opt_gs_buffer_file.c_str(), opt_gs_buffer_size.c_str(), opt_gs_buffer_type.c_str());
            }

        }
    }

#   undef X_BUFFER_FROM_ZERO
#   undef X_BUFFER_FROM_FILE
#   undef X_MAKE_BUFFER


    // std::fprintf(stderr, "=== Buffer Stats\n");
    // std::fprintf(stderr, "Number of Buffers: %d\n", opt_gs_buffer_count);
    // std::fprintf(stderr, "Number of Nodes: %d\n", (int)app_gs_node_count);
    // std::fprintf(stderr, "Number of Edges: %d\n", (int)app_gs_edge_count);

    for (GLint i=0, n=opt_gs_buffer_count; i<n; ++i) {
        std::string &opt_gs_buffer_kind = opt_gs_buffer_kinds[i];
        std::string &opt_gs_buffer_file = opt_gs_buffer_files[i];
        std::string &opt_gs_buffer_size = opt_gs_buffer_sizes[i];
        std::string &opt_gs_buffer_type = opt_gs_buffer_types[i];
        std::string &opt_gs_buffer_name = opt_gs_buffer_names[i];
        std::string &opt_gs_buffer_bind = opt_gs_buffer_binds[i];

        GLuint gl_buffer = app_gl_buffers[i];
        GLsizeiptr gl_buffer_size = app_gl_buffer_sizes[i];

        // std::fprintf(stderr, "--- Buffer %d\n", i);
        // std::fprintf(stderr, "kind: %s\n", opt_gs_buffer_kind.c_str());
        // std::fprintf(stderr, "file: %s\n", opt_gs_buffer_file.c_str());
        // std::fprintf(stderr, "size: %s\n", opt_gs_buffer_size.c_str());
        // std::fprintf(stderr, "type: %s\n", opt_gs_buffer_type.c_str());
        // std::fprintf(stderr, "name: %s\n", opt_gs_buffer_name.c_str());
        // std::fprintf(stderr, "bind: %s\n", opt_gs_buffer_bind.c_str());
        // std::fprintf(stderr, "gl_buffer: %u\n", gl_buffer);
        // std::fprintf(stderr, "gl_buffer_size: %zu\n", gl_buffer_size);
    }

} /* Data::init */


void Shader::init() {

    //--- Shaders

#   define X_MAKE_SHADER(SHADERTYPE, SOURCE, ...)                                                                      \
    ({                                                                                                                 \
        GLuint shader;                                                                                                 \
        GLenum shadertype = (SHADERTYPE);                                                                              \
        shader = glCreateShader(shadertype);                                                                           \
                                                                                                                       \
        const GLchar *string[] = { SOURCE, ## __VA_ARGS__ };                                                           \
        GLsizei count = sizeof(string) / sizeof(*string);                                                              \
        GLint *length = nullptr;                                                                                       \
        glShaderSource(shader, count, string, length);                                                                 \
        glCompileShader(shader);                                                                                       \
                                                                                                                       \
        GLint gl_shader_success;                                                                                       \
        glGetShaderiv(shader, GL_COMPILE_STATUS, &gl_shader_success);                                                  \
        if (gl_shader_success == GL_FALSE) {                                                                           \
            GLint gl_shader_info_log_length;                                                                           \
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &gl_shader_info_log_length);                                     \
                                                                                                                       \
            GLchar *gl_shader_info_log = new GLchar[gl_shader_info_log_length];                                        \
            glGetShaderInfoLog(shader, gl_shader_info_log_length, &gl_shader_info_log_length, gl_shader_info_log);     \
                                                                                                                       \
            for (GLint i=0, n=count; i<n; ++i) { \
                std::fprintf(stderr, "%s", string[i]); \
            } \
            dief("Failed to compile:%s: %s\n",                                                                         \
                #SHADERTYPE, gl_shader_info_log);                                                                      \
        }                                                                                                              \
                                                                                                                       \
        shader;                                                                                                        \
    })                                                                                                                 \
    /* X_MAKE_SHADER */

    opt_gs_shader_common = X_OPTION(std::string, "GS_SHADER_COMMON");
    opt_gs_shader_vertex = X_OPTION(std::string, "GS_SHADER_VERTEX");
    opt_gs_shader_geometry = X_OPTION(std::string, "GS_SHADER_GEOMETRY");
    opt_gs_shader_fragment = X_OPTION(std::string, "GS_SHADER_FRAGMENT");

    GLuint gl_shader_vertex = X_MAKE_SHADER(GL_VERTEX_SHADER, opt_gs_shader_common.c_str(), opt_gs_shader_vertex.c_str());
    GLuint gl_shader_geometry = X_MAKE_SHADER(GL_GEOMETRY_SHADER, opt_gs_shader_common.c_str(), opt_gs_shader_geometry.c_str());
    GLuint gl_shader_fragment = X_MAKE_SHADER(GL_FRAGMENT_SHADER, opt_gs_shader_common.c_str(), opt_gs_shader_fragment.c_str());

#   undef X_MAKE_SHADER


    //--- Shader Program

    app_gl_program = ({
        GLuint gl_program;

        gl_program = glCreateProgram();
        glAttachShader(gl_program, gl_shader_vertex);
        glAttachShader(gl_program, gl_shader_geometry);
        glAttachShader(gl_program, gl_shader_fragment);
        glLinkProgram(gl_program);

        GLint gl_link_status;
        glGetProgramiv(gl_program, GL_LINK_STATUS, &gl_link_status);
        if (gl_link_status == GL_FALSE) {
            GLint gl_info_log_length;
            glGetProgramiv(gl_program, GL_INFO_LOG_LENGTH, &gl_info_log_length);

            GLchar *gl_info_log = new GLchar[gl_info_log_length];
            glGetProgramInfoLog(gl_program, gl_info_log_length, &gl_info_log_length, gl_info_log);
            
            dief("program: %s\n", gl_info_log);
        }

        glUseProgram(gl_program);

        gl_program;
    });


    //--- Node, Edge, and Scratch SSBOs

#   define X_BIND_BUFFER(TARGET, INDEX, BUFFER)                                                     \
    ({                                                                                                                 \
        GLenum target = (TARGET);                                                                                      \
        GLuint index = (INDEX); \
        GLuint buffer = (BUFFER);                                                                                      \
        glBindBufferBase(target, index, buffer);                                                                       \
        std::fprintf(stderr, "X_BIND_BUFFER: glBindBufferBase(0x%x, %u, %u)\n",                                        \
            target, index, buffer);                                                                                    \
    })                                                                                                                 \
    /* X_BIND_BUFFER */

    for (GLint i=0, n=opt_gs_buffer_count; i<n; ++i) {
        std::string &opt_gs_buffer_kind = opt_gs_buffer_kinds[i];
        std::string &opt_gs_buffer_file = opt_gs_buffer_files[i];
        std::string &opt_gs_buffer_size = opt_gs_buffer_sizes[i];
        std::string &opt_gs_buffer_name = opt_gs_buffer_names[i];
        std::string &opt_gs_buffer_type = opt_gs_buffer_types[i];
        std::string &opt_gs_buffer_bind = opt_gs_buffer_binds[i];

        if (opt_gs_buffer_kind == "ATTRIBUTE") {
            X_BIND_BUFFER(GL_SHADER_STORAGE_BUFFER, std::stoi(opt_gs_buffer_bind.c_str()), app_gl_buffers[i]);

        } else if (opt_gs_buffer_kind == "SCRATCH") {
            X_BIND_BUFFER(GL_SHADER_STORAGE_BUFFER, std::stoi(opt_gs_buffer_bind.c_str()), app_gl_buffers[i]);

        } else if (opt_gs_buffer_kind == "ATOMIC") {
            GLenum target = GL_ATOMIC_COUNTER_BUFFER;
            GLuint index = 0;
            GLuint buffer = app_gl_buffers[i];
            glBindBufferBase(target, index, buffer);

        }
    }

#   undef X_BIND_BUFFER

} /* Shader::init */


void Render::init() {

    //--- Uniforms

#   define X_UNIFORM1F(PROGRAM, NAME, VALUE)                                                                     \
    ({                                                                                                                 \
        GLint location;                                                                                                \
        GLuint program = (PROGRAM);                                                                                    \
        const GLchar *name = (NAME);                                                                                   \
        location = glGetUniformLocation(program, name);                                                                \
                                                                                                                       \
        glUniform1f(location, (VALUE));                                                                                   \
    })                                                                                                                 \
    /* X_UNIFORM */

    opt_gs_tile_x = X_OPTION(std::stof, "GS_TILE_X");
    opt_gs_tile_y = X_OPTION(std::stof, "GS_TILE_Y");
    opt_gs_tile_z = X_OPTION(std::stof, "GS_TILE_Z");

    X_UNIFORM1F(app_gl_program, "uTranslateX", -1.0f * opt_gs_tile_x);
    X_UNIFORM1F(app_gl_program, "uTranslateY", -1.0f * opt_gs_tile_y);
    X_UNIFORM1F(app_gl_program, "uScale", std::pow(2.0f, opt_gs_tile_z));

#   undef X_UNIFORM


    //--- Clear buffers

    GLfloat opt_gs_background_r;
    GLfloat opt_gs_background_g;
    GLfloat opt_gs_background_b;
    GLfloat opt_gs_background_a;

    opt_gs_background_r = X_OPTION(std::stof, "GS_BACKGROUND_R");
    opt_gs_background_g = X_OPTION(std::stof, "GS_BACKGROUND_G");
    opt_gs_background_b = X_OPTION(std::stof, "GS_BACKGROUND_B");
    opt_gs_background_a = X_OPTION(std::stof, "GS_BACKGROUND_A");

    glClearColor(opt_gs_background_r, opt_gs_background_g, opt_gs_background_b, opt_gs_background_a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glFlush();


    //--- Render

    ({
        // GLenum target = GL_ELEMENT_ARRAY_BUFFER;
        // GLuint buffer = gl_buffer_edge_index;
        // glBindBuffer(target, buffer);

        GLenum mode = GL_LINES;
        GLsizei count = app_gs_edge_count * 2;
        GLenum type = GL_UNSIGNED_INT;
        GLvoid *indices = 0;
        std::fprintf(stderr, "glDrawElements(0x%x, %u, 0x%x, %p)\n", mode, count, type, indices);
        glDrawElements(mode, count, type, indices);
    });


    //--- Query Buffers

    GLint opt_gs_atomic_count;
    std::vector<std::string> opt_gs_atomic_names;

    opt_gs_atomic_count = X_OPTION(std::stoi, "GS_ATOMIC_COUNT");
    opt_gs_atomic_names.resize(opt_gs_atomic_count);
    for (GLint i=0, n=opt_gs_atomic_count; i<n; ++i) {
        opt_gs_atomic_names[i] = X_OPTION(std::string, "GS_ATOMIC_NAME_%d", i);
    }

    for (GLint i=0, n=opt_gs_buffer_count; i<n; ++i) {
        std::string &opt_gs_buffer_kind = opt_gs_buffer_kinds[i];
        std::string &opt_gs_buffer_file = opt_gs_buffer_files[i];
        std::string &opt_gs_buffer_size = opt_gs_buffer_sizes[i];
        std::string &opt_gs_buffer_name = opt_gs_buffer_names[i];
        std::string &opt_gs_buffer_type = opt_gs_buffer_types[i];

        if (opt_gs_buffer_kind == "ATOMIC") {
            if (opt_gs_buffer_type == "GL_UNSIGNED_INT") {
                GLenum target = GL_ATOMIC_COUNTER_BUFFER;
                GLenum buffer = app_gl_buffers[i];
                glBindBuffer(target, buffer);

                GLuint *data;
                GLintptr offset = 0;
                GLsizeiptr size = app_gl_buffer_sizes[i];
                data = new GLuint[size];
                glGetBufferSubData(target, offset, size, static_cast<void *>(data));
                
                std::fprintf(stderr, "--- Atomics\n");
                std::fprintf(stdout, "{\n");
                std::fprintf(stdout, "  \"metadata\": {\n");
                std::fprintf(stdout, "     \"nodes\": %zu,\n", app_gs_node_count);
                std::fprintf(stdout, "     \"edges\": %zu\n", app_gs_edge_count);
                std::fprintf(stdout, "  },\n");
                std::fprintf(stdout, "  \"atomics\": {\n");
                for (GLint i=0, n=opt_gs_atomic_count; i<n; ++i) {
                    std::string &opt_gs_atomic_name = opt_gs_atomic_names[i];
                    std::fprintf(stderr, "atomic %d \"%s\" = %u\n",
                        i, opt_gs_atomic_name.c_str(), data[i]);
                    std::fprintf(stdout, "    \"%s\": %u%s\n",
                        opt_gs_atomic_name.c_str(), data[i], i<n-1 ? "," : "");
                }
                std::fprintf(stdout, "  }\n");
                std::fprintf(stdout, "}\n");

            } else {
                dief("can't query atomic");
            }

        }
    }


    // ({
    //     GLenum target = GL_ATOMIC_COUNTER_BUFFER;
    //     glBindBuffer(target, gl_buffer_scratch);

    //     GLuint in_degree_max;

    //     GLintptr offset = 0;
    //     GLsizeiptr size = sizeof(GLuint);
    //     glGetBufferSubData(target, offset, size, static_cast<void *>(&in_degree_max));
        
    //     std::fprintf(stderr, "in_degree_max: %u\n", in_degree_max);
    // });


    //--- Read RGBA Colors

    std::tie(app_rgba_width, app_rgba_height, app_rgba_data) = ({
        GLuint framebuffer = app_gl_framebuffer;
        GLenum mode = GL_COLOR_ATTACHMENT0;
        glNamedFramebufferReadBuffer(framebuffer, mode);

        GLint x = 0;
        GLint y = 0;
        GLsizei width = opt_gs_tile_width;
        GLsizei height = opt_gs_tile_height;
        GLenum format = GL_RGBA;
        GLenum type = GL_UNSIGNED_BYTE;
        GLvoid *data = new GLchar[4 * opt_gs_tile_width * opt_gs_tile_height];
        glReadPixels(x, y, width, height, format, type, data);

        std::make_tuple(width, height, data);
    });


    //--- Encode RGBA to JPEG

    ({
        app_jpeg.clear();
        app_jpeg.reserve(1000000);

        void *context = static_cast<void *>(&app_jpeg);
        int width = app_rgba_width;
        int height = app_rgba_height;
        int components = 4;
        void *rgba = app_rgba_data;
        int quality = 95;
        auto callback = +[](void *context, void *data, int size) {
            // std::fprintf(stderr, "Writing %zu bytes\n", (size_t)size);
            std::vector<uint8_t> &app_jpeg = *static_cast<std::vector<uint8_t> *>(context);
            uint8_t *bytes = static_cast<uint8_t *>(data);
            if (app_jpeg.capacity() < app_jpeg.size() + size) {
                app_jpeg.reserve(app_jpeg.capacity() + 10000);
            }
            std::copy(bytes, bytes + size, std::back_inserter(app_jpeg));
        };

        std::fprintf(stderr, "JPEG: %dx%d\n", width, height);

        int stbi_success;
        stbi_success = stbi_write_jpg_to_func(callback, context, width, height, components, rgba, quality);
        if (!stbi_success) {
            dief("Failed to write jpeg");
        }
    });


    //--- Write JPEG to Disk

    std::string opt_gs_output;

    opt_gs_output = X_OPTION(std::string, "GS_OUTPUT");

    if (opt_gs_output != "<MEMORY>") {
        ({
            GLchar name[128];
            snprintf(name, sizeof(name), "%s", opt_gs_output.c_str());

            FILE *file = fopen(name, "wb");
            fwrite(app_jpeg.data(), 1, app_jpeg.size(), file);

            fprintf(stderr, "Wrote %zu bytes to %s\n", app_jpeg.size(), name);
        });
    }

} /* Render::init */

void Render::done() {
} /* Render::done */

void Shader::done() {
} /* Shader::done */

void Data::done() {
} /* Data::done */

void GL::done() {
} /* GL::done */


void EGL::done() {
} /* EGL::done */

void Environment::done() {
} /* Environment::done */


} /* namespace gs */


extern "C"
void *gsNewApplication(void) {
    ::gs::Application *app = new ::gs::Render();
    return static_cast<void *>(app);
}

extern "C"
void gsEnvironmentInit(void *app_) {
    ::gs::Application *app = static_cast<::gs::Application *>(app_);
    return app->::gs::Environment::init();
}

extern "C"
void gsEnvironmentSet(void *app_, const char *name, const char *value) {
    ::gs::Application *app = static_cast<::gs::Application *>(app_);
    return app->::gs::Environment::setenv(name, value);
}

extern "C"
void gsEnvironmentDone(void *app_) {
    ::gs::Application *app = static_cast<::gs::Application *>(app_);
    return app->::gs::Environment::done();
}

extern "C"
void gsEGLInit(void *app_) {
    ::gs::Application *app = static_cast<::gs::Application *>(app_);
    return app->::gs::EGL::init();
}

extern "C"
void gsEGLDone(void *app_) {
    ::gs::Application *app = static_cast<::gs::Application *>(app_);
    return app->::gs::EGL::done();
}

extern "C"
void gsGLInit(void *app_) {
    ::gs::Application *app = static_cast<::gs::Application *>(app_);
    return app->::gs::GL::init();
}

extern "C"
void gsGLDone(void *app_) {
    ::gs::Application *app = static_cast<::gs::Application *>(app_);
    return app->::gs::GL::done();
}

extern "C"
void gsDataSet(void *app_, const char *name, const char *buffer, size_t buflen) {
    ::gs::Application *app = static_cast<::gs::Application *>(app_);
    return app->::gs::Data::set(name, buffer, buflen);
}

extern "C"
void gsDataInit(void *app_) {
    ::gs::Application *app = static_cast<::gs::Application *>(app_);
    return app->::gs::Data::init();
}

extern "C"
void gsDataDone(void *app_) {
    ::gs::Application *app = static_cast<::gs::Application *>(app_);
    return app->::gs::Data::done();
}

extern "C"
void gsShaderInit(void *app_) {
    ::gs::Application *app = static_cast<::gs::Application *>(app_);
    return app->::gs::Shader::init();
}

extern "C"
void gsShaderDone(void *app_) {
    ::gs::Application *app = static_cast<::gs::Application *>(app_);
    return app->::gs::Shader::done();
}

extern "C"
void gsRenderInit(void *app_) {
    ::gs::Application *app = static_cast<::gs::Application *>(app_);
    return app->::gs::Render::init();
}

extern "C"
void gsRenderGet(void *app_, void **jpeg, size_t *jpeglen) {
    ::gs::Application *app = static_cast<::gs::Application *>(app_);
    return app->::gs::Render::get(jpeg, jpeglen);
}

extern "C"
void gsRenderDone(void *app_) {
    ::gs::Application *app = static_cast<::gs::Application *>(app_);
    return app->::gs::Render::done();
}


#ifdef _GS_MAIN_

//--- Main Entrypoint

int main(int argc, char **argv, char **envp) {
    (void)argc;
    (void)argv;

    void *app = gsNewApplication();

    gsEnvironmentInit(app);
    for (char **env_=envp; *env_; ++env_) {
        std::string env = *env_;
        size_t index = env.find('=');
        if (index == std::string::npos) {
            continue;
        }

        std::string key = env.substr(0, index);
        std::string value = env.substr(index + 1);
        gsEnvironmentSet(app, key.c_str(), value.c_str());
    }

    gsEGLInit(app);
    gsGLInit(app);
    gsDataInit(app);
    gsShaderInit(app);
    gsRenderInit(app);

    gsRenderDone(app);
    gsShaderDone(app);
    gsDataDone(app);
    gsGLDone(app);
    gsEGLDone(app);

    gsEnvironmentDone(app);
}

#endif /* _GS_MAIN_ */

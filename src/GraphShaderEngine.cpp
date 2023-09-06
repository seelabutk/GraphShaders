// For alignment:
//
//345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789\

#include "debug-trap.h"

// c++ stdlib
#include <string>
#include <tuple>
#include <vector>

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

#define X_OPTION(PARSE, FORMAT, ...)                                                                                \
    ({                                                                                                                 \
        char name[128];                                                                                                \
        std::snprintf(name, sizeof(name), FORMAT, ## __VA_ARGS__);                                                     \
        char *s = getenv(name);                                                                                        \
        if (!s) dief("Missing required option: %s", name);                                                             \
        (PARSE)(s);                                                                                                    \
    })                                                                                                                 \
/* X_OPTION */


//--- Main Entrypoint

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;


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

    GLsizei opt_gs_tile_width = X_OPTION(std::stoi, "GS_TILE_WIDTH");
    GLsizei opt_gs_tile_height = X_OPTION(std::stoi, "GS_TILE_HEIGHT");

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
    {
        GLenum attachment = GL_DEPTH_STENCIL_ATTACHMENT;
        GLuint textarget = GL_TEXTURE_2D;
        GLuint texture = gl_framebuffer_texture_depth_and_stencil;
        GLint level = 0;
        glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, textarget, texture, level);
    }

    {
        GLenum status;
        status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            dief("Failed to complete framebuffer");
        }
    }


    //--- We need a Vertex Array Object (VAO)

    GLuint gl_vertex_array;
    glGenVertexArrays(1, &gl_vertex_array);
    glBindVertexArray(gl_vertex_array);


    //--- Clear buffers

    GLfloat opt_gs_background_r = X_OPTION(std::stof, "GS_BACKGROUND_R");
    GLfloat opt_gs_background_g = X_OPTION(std::stof, "GS_BACKGROUND_G");
    GLfloat opt_gs_background_b = X_OPTION(std::stof, "GS_BACKGROUND_B");
    GLfloat opt_gs_background_a = X_OPTION(std::stof, "GS_BACKGROUND_A");

    glClearColor(opt_gs_background_r, opt_gs_background_g, opt_gs_background_b, opt_gs_background_a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glFlush();


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

    GLsizeiptr gs_edge_count = 0;
    GLsizeiptr gs_node_count = 0;

    GLint opt_gs_buffer_count = X_OPTION(std::stoi, "GS_BUFFER_COUNT");
    std::string opt_gs_buffer_kinds[opt_gs_buffer_count];
    std::string opt_gs_buffer_names[opt_gs_buffer_count];
    std::string opt_gs_buffer_files[opt_gs_buffer_count];
    std::string opt_gs_buffer_sizes[opt_gs_buffer_count];
    std::string opt_gs_buffer_types[opt_gs_buffer_count];
    std::string opt_gs_buffer_binds[opt_gs_buffer_count];
    for (GLint i=0, n=opt_gs_buffer_count; i<n; ++i) {
        opt_gs_buffer_kinds[i] = X_OPTION(std::string, "GS_BUFFER_KIND_%d", i);
        opt_gs_buffer_names[i] = X_OPTION(std::string, "GS_BUFFER_NAME_%d", i);
        opt_gs_buffer_files[i] = X_OPTION(std::string, "GS_BUFFER_FILE_%d", i);
        opt_gs_buffer_sizes[i] = X_OPTION(std::string, "GS_BUFFER_SIZE_%d", i);
        opt_gs_buffer_types[i] = X_OPTION(std::string, "GS_BUFFER_TYPE_%d", i);
        opt_gs_buffer_binds[i] = X_OPTION(std::string, "GS_BUFFER_BIND_%d", i);
    }

    GLuint gl_buffers[opt_gs_buffer_count];
    GLsizeiptr gl_buffer_sizes[opt_gs_buffer_count];

    for (GLint i=0, n=opt_gs_buffer_count; i<n; ++i) {
        std::string &opt_gs_buffer_kind = opt_gs_buffer_kinds[i];
        std::string &opt_gs_buffer_file = opt_gs_buffer_files[i];
        std::string &opt_gs_buffer_size = opt_gs_buffer_sizes[i];
        std::string &opt_gs_buffer_type = opt_gs_buffer_types[i];

        if (opt_gs_buffer_kind == "ATTRIBUTE") {
            gl_buffers[i] = X_MAKE_BUFFER(GL_SHADER_STORAGE_BUFFER);
            gl_buffer_sizes[i] = X_BUFFER_FROM_FILE(GL_SHADER_STORAGE_BUFFER, "%s", opt_gs_buffer_file.c_str());

            if (opt_gs_buffer_type == "GL_FLOAT" && opt_gs_buffer_size == "N") {
                gs_node_count = gl_buffer_sizes[i] / sizeof(GLfloat);

            } else if (opt_gs_buffer_type == "GL_UNSIGNED_INT" && opt_gs_buffer_size == "N") {
                gs_node_count = gl_buffer_sizes[i] / sizeof(GLuint);

            } else if (opt_gs_buffer_type == "GL_UNSIGNED_INT" && opt_gs_buffer_size == "E") {
                gs_edge_count = gl_buffer_sizes[i] / sizeof(GLuint);

            } else {
                dief("2Unrecognized buffer: kind=%s; file=%s; size=%s; type=%s",
                    opt_gs_buffer_kind.c_str(), opt_gs_buffer_file.c_str(), opt_gs_buffer_size.c_str(), opt_gs_buffer_type.c_str());
            }

        } else if (opt_gs_buffer_kind == "ELEMENT") {
            gl_buffers[i] = X_MAKE_BUFFER(GL_ELEMENT_ARRAY_BUFFER);
            gl_buffer_sizes[i] = X_BUFFER_FROM_FILE(GL_ELEMENT_ARRAY_BUFFER, "%s", opt_gs_buffer_file.c_str());

            if (opt_gs_buffer_type == "GL_UNSIGNED_INT" && opt_gs_buffer_size == "2E") {
                gs_edge_count = gl_buffer_sizes[i] / sizeof(GLuint) / 2;

            } else {
                dief("1Unrecognized buffer: kind=%s; file=%s; size=%s; type=%s",
                    opt_gs_buffer_kind.c_str(), opt_gs_buffer_file.c_str(), opt_gs_buffer_size.c_str(), opt_gs_buffer_type.c_str());
            }

        }
    }

    if (gs_edge_count == 0) {
        dief("Failed to read any edges");
    }

    if (gs_node_count == 0) {
        dief("Failed to read any nodes");
    }

    for (GLint i=0, n=opt_gs_buffer_count; i<n; ++i) {
        std::string &opt_gs_buffer_kind = opt_gs_buffer_kinds[i];
        std::string &opt_gs_buffer_file = opt_gs_buffer_files[i];
        std::string &opt_gs_buffer_size = opt_gs_buffer_sizes[i];
        std::string &opt_gs_buffer_type = opt_gs_buffer_types[i];
        std::string &opt_gs_buffer_name = opt_gs_buffer_names[i];

        if (opt_gs_buffer_kind == "ATOMIC") {
            gl_buffers[i] = X_MAKE_BUFFER(GL_ATOMIC_COUNTER_BUFFER);

            if (opt_gs_buffer_type == "GL_UNSIGNED_INT") {
                gl_buffer_sizes[i] = X_BUFFER_FROM_ZERO(GL_ATOMIC_COUNTER_BUFFER, GL_DYNAMIC_COPY, std::stoi(opt_gs_buffer_size), GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT);

            } else {
                dief("3Unrecognized buffer: kind=%s; file=%s; size=%s; type=%s",
                    opt_gs_buffer_kind.c_str(), opt_gs_buffer_file.c_str(), opt_gs_buffer_size.c_str(), opt_gs_buffer_type.c_str());
            }

        } else if (opt_gs_buffer_kind == "SCRATCH") {
            gl_buffers[i] = X_MAKE_BUFFER(GL_SHADER_STORAGE_BUFFER);

            if (opt_gs_buffer_type == "GL_UNSIGNED_INT" && opt_gs_buffer_size == "N") {
                gl_buffer_sizes[i] = X_BUFFER_FROM_ZERO(GL_SHADER_STORAGE_BUFFER, GL_DYNAMIC_COPY, gs_node_count * sizeof(GLuint), GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT);

            } else if (opt_gs_buffer_type == "GL_UNSIGNED_INT" && opt_gs_buffer_size == "E") {
                gl_buffer_sizes[i] = X_BUFFER_FROM_ZERO(GL_SHADER_STORAGE_BUFFER, GL_DYNAMIC_COPY, gs_edge_count * sizeof(GLuint), GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT);

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
    // std::fprintf(stderr, "Number of Nodes: %d\n", (int)gs_node_count);
    // std::fprintf(stderr, "Number of Edges: %d\n", (int)gs_edge_count);

    for (GLint i=0, n=opt_gs_buffer_count; i<n; ++i) {
        std::string &opt_gs_buffer_kind = opt_gs_buffer_kinds[i];
        std::string &opt_gs_buffer_file = opt_gs_buffer_files[i];
        std::string &opt_gs_buffer_size = opt_gs_buffer_sizes[i];
        std::string &opt_gs_buffer_type = opt_gs_buffer_types[i];
        std::string &opt_gs_buffer_name = opt_gs_buffer_names[i];
        std::string &opt_gs_buffer_bind = opt_gs_buffer_binds[i];

        GLuint gl_buffer = gl_buffers[i];
        GLsizeiptr gl_buffer_size = gl_buffer_sizes[i];

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

    std::string opt_gs_shader_common = X_OPTION(std::string, "GS_SHADER_COMMON");
    std::string opt_gs_shader_vertex = X_OPTION(std::string, "GS_SHADER_VERTEX");
    std::string opt_gs_shader_geometry = X_OPTION(std::string, "GS_SHADER_GEOMETRY");
    std::string opt_gs_shader_fragment = X_OPTION(std::string, "GS_SHADER_FRAGMENT");

    GLuint gl_shader_vertex = X_MAKE_SHADER(GL_VERTEX_SHADER, opt_gs_shader_common.c_str(), opt_gs_shader_vertex.c_str());
    GLuint gl_shader_geometry = X_MAKE_SHADER(GL_GEOMETRY_SHADER, opt_gs_shader_common.c_str(), opt_gs_shader_geometry.c_str());
    GLuint gl_shader_fragment = X_MAKE_SHADER(GL_FRAGMENT_SHADER, opt_gs_shader_common.c_str(), opt_gs_shader_fragment.c_str());

#   undef X_MAKE_SHADER


    //--- Shader Program

    GLuint gl_program = ({
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
            X_BIND_BUFFER(GL_SHADER_STORAGE_BUFFER, std::stoi(opt_gs_buffer_bind.c_str()), gl_buffers[i]);

        } else if (opt_gs_buffer_kind == "SCRATCH") {
            X_BIND_BUFFER(GL_SHADER_STORAGE_BUFFER, std::stoi(opt_gs_buffer_bind.c_str()), gl_buffers[i]);

        } else if (opt_gs_buffer_kind == "ATOMIC") {
            GLenum target = GL_ATOMIC_COUNTER_BUFFER;
            GLuint index = 0;
            GLuint buffer = gl_buffers[i];
            glBindBufferBase(target, index, buffer);

        }
    }

#   undef X_BIND_BUFFER


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

    GLfloat opt_gs_tile_x = X_OPTION(std::stof, "GS_TILE_X");
    GLfloat opt_gs_tile_y = X_OPTION(std::stof, "GS_TILE_Y");
    GLfloat opt_gs_tile_z = X_OPTION(std::stof, "GS_TILE_Z");

    X_UNIFORM1F(gl_program, "uTranslateX", -1.0f * opt_gs_tile_x);
    X_UNIFORM1F(gl_program, "uTranslateY", -1.0f * opt_gs_tile_y);
    X_UNIFORM1F(gl_program, "uScale", std::pow(2.0f, opt_gs_tile_z));

#   undef X_UNIFORM

    //--- Render

    ({
        // GLenum target = GL_ELEMENT_ARRAY_BUFFER;
        // GLuint buffer = gl_buffer_edge_index;
        // glBindBuffer(target, buffer);

        GLenum mode = GL_LINES;
        GLsizei count = gs_edge_count * 2;
        GLenum type = GL_UNSIGNED_INT;
        GLvoid *indices = 0;
        std::fprintf(stderr, "glDrawElements(0x%x, %u, 0x%x, %p)\n", mode, count, type, indices);
        glDrawElements(mode, count, type, indices);
    });


    //--- Query Buffers

    GLint opt_gs_atomic_count = X_OPTION(std::stoi, "GS_ATOMIC_COUNT");
    std::string opt_gs_atomic_names[opt_gs_atomic_count];
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
                GLenum buffer = gl_buffers[i];
                glBindBuffer(target, buffer);

                GLuint *data;
                GLintptr offset = 0;
                GLsizeiptr size = gl_buffer_sizes[i];
                data = new GLuint[size];
                glGetBufferSubData(target, offset, size, static_cast<void *>(data));
                
                std::fprintf(stderr, "--- Atomics\n");
                std::fprintf(stdout, "{\n");
                std::fprintf(stdout, "  \"metadata\": {\n");
                std::fprintf(stdout, "     \"nodes\": %zu,\n", gs_node_count);
                std::fprintf(stdout, "     \"edges\": %zu\n", gs_edge_count);
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

    GLsizei rgba_width;
    GLsizei rgba_height;
    GLvoid *rgba_data;

    std::tie(rgba_width, rgba_height, rgba_data) = ({
        GLuint framebuffer = gl_framebuffer;
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

    std::vector<uint8_t> jpeg;

    ({
        jpeg.clear();
        jpeg.reserve(1000000);

        void *context = static_cast<void *>(&jpeg);
        int width = rgba_width;
        int height = rgba_height;
        int components = 4;
        void *rgba = rgba_data;
        int quality = 95;
        auto callback = +[](void *context, void *data, int size) {
            // std::fprintf(stderr, "Writing %zu bytes\n", (size_t)size);
            std::vector<uint8_t> &jpeg = *static_cast<std::vector<uint8_t> *>(context);
            uint8_t *bytes = static_cast<uint8_t *>(data);
            if (jpeg.capacity() < jpeg.size() + size) {
                jpeg.reserve(jpeg.capacity() + 10000);
            }
            std::copy(bytes, bytes + size, std::back_inserter(jpeg));
        };

        std::fprintf(stderr, "JPEG: %dx%d\n", width, height);

        int stbi_success;
        stbi_success = stbi_write_jpg_to_func(callback, context, width, height, components, rgba, quality);
        if (!stbi_success) {
            dief("Failed to write jpeg");
        }
    });


    //--- Write JPEG to Disk

    std::string opt_gs_output = X_OPTION(std::string, "GS_OUTPUT");

    ({
        GLchar name[128];
        snprintf(name, sizeof(name), "%s", opt_gs_output.c_str());

        FILE *file = fopen(name, "wb");
        fwrite(jpeg.data(), 1, jpeg.size(), file);

        fprintf(stderr, "Wrote %zu bytes to %s\n", jpeg.size(), name);
    });

    return 0;
}

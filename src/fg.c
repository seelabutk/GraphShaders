// For alignment:
//
//345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789\

#include "server.h"

#include "debug-trap.h"

#include <string>
#include <tuple>
#include <vector>

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
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


#include "base64.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <glad/glad.h>
#include <glad/glad_egl.h>

#include "stb_image_write.h"
#define GLAPIENTRY APIENTRY

/**
 *
 */

// stdlib
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <cstdio>
#include <fcntl.h>

// // glad
// #include <glad/gl.h>
// #include <glad/egl.h>

// // wasmer
// #include <wasmer.h>

// // Explicilty label that we are transferring ownership to the function, so that
// // we don't need to free the referenced variable.
// #define XFER(x) (x)


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

// static void file2wat(const char *name, wasm_byte_vec_t *wat) {
//     FILE *f = fopen(name, "rb");
//     if (!f) dief("failed to open: %s", name);
//     fseek(f, 0L, SEEK_END);
//     size_t size = ftell(f);
//     fseek(f, 0L, SEEK_SET);
//     wasm_byte_vec_new_uninitialized(wat, size);
//     if (fread(wat->data, size, 1, f) != 1) {
//         dief("failed to read: %s", name);
//     }
//     fclose(f); }

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    // Configurable values

#   define OPTION(parse, default, format, ...)                                                                         \
    ({                                                                                                                 \
        char name[128];                                                                                                \
        std::snprintf(name, sizeof(name), format, ## __VA_ARGS__);                                                     \
        char *s = getenv(name);                                                                                        \
        s ? parse(s) : default;                                                                                        \
    })                                                                                                                 \
    /**/

    GLsizei opt_resolution_x = OPTION(std::stoi, 256, "WIDTH");
    GLsizei opt_resolution_y = OPTION(std::stoi, 256, "HEIGHT");
    GLfloat opt_tile_x = OPTION(std::stof, 1.0f, "TILE_X");
    GLfloat opt_tile_y = OPTION(std::stof, 1.0f, "TILE_Y");
    GLfloat opt_tile_z = OPTION(std::stof, 2.0f, "TILE_Z");
    std::string opt_output_file = OPTION(std::string, "out.jpg", "OUTPUT");
    std::string opt_common_shader = OPTION(std::string, R"EOF(
        #version 460 core
        precision mediump float;

        uniform float uTranslateX;
        uniform float uTranslateY;
        uniform float uScale;

        layout (binding=0, offset=0) uniform atomic_uint in_degree_max;
        layout (binding=0, offset=4) uniform atomic_uint out_degree_max;

        layout (std430, binding=1) buffer InDegree {
            uint in_degree[];
        };

        layout (std430, binding=2) buffer OutDegree {
            uint out_degree[];
        };

    )EOF", "COMMON");
    std::string opt_vertex_shader = OPTION(std::string, R"EOF(
        layout (location=0) in float x;
        layout (location=1) in float y;
        layout (location=2) in uint date;
        layout (location=3) in uint nmaintainers;
        layout (location=4) in uint cve;

        out uint i;

        void main() {
            gl_Position.xy = vec2(x, y);

            // Apply tile transformations
            gl_Position.xy *= uScale;
            gl_Position.xy += vec2(uTranslateX, uTranslateY);

            // Transform xy in [0, 1] to xy in [-1, 1]
            gl_Position.xy *= 2.;
            gl_Position.xy -= 1.;

            i = uint(gl_VertexID);
            gl_Position = vec4(gl_Position.xy, 0.0, 1.0);
        }
    )EOF", "VERTEX");
    std::string opt_geometry_shader = OPTION(std::string, R"EOF(
        layout (lines) in;
        layout (line_strip, max_vertices=2) out;

        in uint i[];

        void main() {
            atomicCounterMax(out_degree_max, atomicAdd(out_degree[i[0]], 1));
            atomicCounterMax(in_degree_max, atomicAdd(in_degree[i[1]], 1));

            gl_Position = gl_in[0].gl_Position;
            gl_PrimitiveID = -(1 + int(i[0]));
            EmitVertex();

            gl_Position = gl_in[1].gl_Position;
            EmitVertex();

            EndPrimitive();

            gl_Position = gl_in[0].gl_Position;
            gl_PrimitiveID = +(1 + int(i[1]));
            EmitVertex();

            gl_Position = gl_in[1].gl_Position;
            EmitVertex();

            EndPrimitive();
        }
    )EOF", "GEOMETRY");
    std::string opt_fragment_shader = OPTION(std::string, R"EOF(
        out vec4 gl_FragColor;

        void main() {
            bool is_source = gl_PrimitiveID < 0;
            bool is_target = gl_PrimitiveID > 0;
            int i = is_source
                ? -gl_PrimitiveID - 1
                : +gl_PrimitiveID - 1;

            uint id = in_degree[i];
            uint od = out_degree[i];

            if (od < id) discard;
            gl_FragColor = vec4(1., 1., 1., 1.);
        }
    )EOF", "FRAGMENT");
    // GLsizei opt_node_attributes_count = OPTION(std::stoi, 0, "NODE_ATTRIBUTE_COUNT");
    // for (GLsizei i=0, n=opt_node_attributes_count; i<n; ++i) {
    // }

#   undef OPTION

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

#   define X(var, constant)                                                                                            \
    char const *var;                                                                                                   \
    var = eglQueryString(egl_display, constant);                                                                       \
    std::fprintf(stderr, #constant ": %s\n", var)

    X(egl_version, EGL_VERSION);
    X(egl_vendor, EGL_VENDOR);
    X(egl_client_apis, EGL_CLIENT_APIS);
    X(egl_extensions, EGL_EXTENSIONS);

#   undef X


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

#   define X(var, constant)                                                                                            \
    const GLubyte *var;                                                                                                \
    var = glGetString(constant);                                                                                       \
    std::fprintf(stderr, #constant ": %s\n", var)

    X(gl_vendor, GL_VENDOR);
    X(gl_version, GL_VERSION);
    X(gl_renderer, GL_RENDERER);
    X(gl_shading_language_version, GL_SHADING_LANGUAGE_VERSION);

#   undef X


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

    GLuint gl_framebuffer_texture_color;
    glGenTextures(1, &gl_framebuffer_texture_color);
    glBindTexture(GL_TEXTURE_2D, gl_framebuffer_texture_color);
    {
        GLint level = 0;
        GLint internalformat = GL_RGBA;
        GLsizei width = opt_resolution_x;
        GLsizei height = opt_resolution_y;
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
        GLsizei width = opt_resolution_x;
        GLsizei height = opt_resolution_y;
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

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glFlush();


    glDisable(GL_DEPTH_TEST);


    // Prepare the viewport

    {
        GLint x = 0;
        GLint y = 0;
        GLsizei width = opt_resolution_x;
        GLsizei height = opt_resolution_y;

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

#   define GL_BUFFER_FROM_FILE(TARGET, PATTERN, ...)                                                                   \
    ({                                                                                                                 \
        char file_name[256];                                                                                           \
        snprintf(file_name, sizeof(file_name), PATTERN, ## __VA_ARGS__);                                               \
                                                                                                                       \
        std::fprintf(stderr, "Read from %s\n", file_name);                                                             \
                                                                                                                       \
        FILE *file_object = fopen(file_name, "rb");                                                                    \
        if (!file_object) {                                                                                            \
            dief("fopen: %s\n", file_name);                                                                            \
        }                                                                                                              \
                                                                                                                       \
        int file_success;                                                                                              \
        file_success = fseek(file_object, 0L, SEEK_END);                                                               \
        if (file_success != 0) {                                                                                       \
            dief("fseek: %s\n", file_name);                                                                            \
        }                                                                                                              \
                                                                                                                       \
        long file_size;                                                                                                \
        file_size = ftell(file_object);                                                                                \
        if (file_size < 0) {                                                                                           \
            dief("ftell: %s\n", file_name);                                                                            \
        }                                                                                                              \
                                                                                                                       \
        file_success = fseek(file_object, 0L, SEEK_SET);                                                               \
        if (file_success != 0) {                                                                                       \
            dief("fseek: %s\n", file_name);                                                                            \
        }                                                                                                              \
                                                                                                                       \
        void *file_data;                                                                                               \
        file_data = new char[file_size];                                                                               \
        if (file_data == nullptr) {                                                                                    \
            dief("new: %ld\n", file_size);                                                                             \
        }                                                                                                              \
                                                                                                                       \
        long file_read;                                                                                                \
        file_read = fread(file_data, 1, file_size, file_object);                                                       \
        if (file_read < file_size) {                                                                                   \
            dief("fread: %s\n", file_name);                                                                            \
        }                                                                                                              \
                                                                                                                       \
        GLuint gl_buffer;                                                                                              \
        glGenBuffers(1, &gl_buffer);                                                                                   \
        glBindBuffer(TARGET, gl_buffer);                                                                               \
        glBufferData(TARGET, file_size, file_data, GL_STATIC_DRAW);                                                    \
                                                                                                                       \
        std::make_tuple(gl_buffer, file_size);                                                                         \
    })                                                                                                                 \
    /* GL_BUFFER_FROM_FILE */

#   define DECLARE_BUFFER(PREFIX, TARGET, PATTERN, ...)                                                                \
    GLuint PREFIX;                                                                                                     \
    GLsizeiptr PREFIX ## _size;                                                                                        \
    std::tie(PREFIX, PREFIX ## _size) = GL_BUFFER_FROM_FILE(TARGET, PATTERN, ## __VA_ARGS__);                          \
    (void)0                                                                                                            \
    /* DECLARE_BUFFER */

    DECLARE_BUFFER(gl_buffer_node_x, GL_ARRAY_BUFFER, "%s,kind=%s,name=%s,type=%s.bin", "JS-Deps", "node", "x", "f32");
    DECLARE_BUFFER(gl_buffer_node_y, GL_ARRAY_BUFFER, "%s,kind=%s,name=%s,type=%s.bin", "JS-Deps", "node", "y", "f32");
    DECLARE_BUFFER(gl_buffer_node_date, GL_ARRAY_BUFFER, "%s,kind=%s,name=%s,type=%s.bin", "JS-Deps", "node", "date", "u32");
    DECLARE_BUFFER(gl_buffer_node_nmaintainers, GL_ARRAY_BUFFER, "%s,kind=%s,name=%s,type=%s.bin", "JS-Deps", "node", "nmaintainers", "u32");
    DECLARE_BUFFER(gl_buffer_node_cve, GL_ARRAY_BUFFER, "%s,kind=%s,name=%s,type=%s.bin", "JS-Deps", "node", "cve", "u32");
    DECLARE_BUFFER(gl_buffer_edge_index, GL_ELEMENT_ARRAY_BUFFER, "%s,kind=%s,name=%s,type=%s.bin", "JS-Deps", "edge", "index", "2u32");

#   undef DECLARE_BUFFER

#   undef GL_BUFFER_FROM_FILE

    
    //--- Atomic Counter Buffers

    GLuint gl_buffer_atomic_counter = ({
        GLuint gl_buffer;
        glGenBuffers(1, &gl_buffer);

        GLenum target = GL_ATOMIC_COUNTER_BUFFER;
        glBindBuffer(target, gl_buffer);

        GLsizeiptr size = 1024 * sizeof(GLuint);
        GLvoid *data = nullptr;
        GLenum usage = GL_DYNAMIC_COPY;
        glBufferData(target, size, data, usage);

        GLenum internalformat = GL_R32UI;
        GLenum format = GL_RED_INTEGER;
        GLenum type = GL_UNSIGNED_INT;
        glClearBufferData(target, internalformat, format, type, data);

        GLuint index = 0;
        glBindBufferBase(target, index, gl_buffer);

        gl_buffer;
    });


    //--- Shader Storage Buffer Objects (SSBOs)

    GLuint gl_buffer_in_degree = ({
        GLuint gl_buffer;
        glGenBuffers(1, &gl_buffer);

        GLenum target = GL_SHADER_STORAGE_BUFFER;
        glBindBuffer(target, gl_buffer);

        GLsizeiptr size = gl_buffer_node_x_size / sizeof(GLfloat) * sizeof(GLuint);
        GLvoid *data = nullptr;
        GLenum usage = GL_DYNAMIC_COPY;
        glBufferData(target, size, data, usage);

        GLenum internalformat = GL_R32UI;
        GLenum format = GL_RED_INTEGER;
        GLenum type = GL_UNSIGNED_INT;
        glClearBufferData(target, internalformat, format, type, data);

        GLuint index = 1;
        glBindBufferBase(target, index, gl_buffer);

        gl_buffer;
    });


    //--- Shaders

#   define GL_SHADER_FROM_STRINGS(TYPE, SOURCE, ...)                                                                   \
    ({                                                                                                                 \
        GLuint gl_shader;                                                                                              \
                                                                                                                       \
        gl_shader = glCreateShader(TYPE);                                                                              \
                                                                                                                       \
        const GLchar *string[] = { SOURCE, ## __VA_ARGS__ };                                                           \
        GLsizei count = sizeof(string) / sizeof(*string);                                                              \
        GLint *length = nullptr;                                                                                       \
        glShaderSource(gl_shader, count, string, length);                                                              \
        glCompileShader(gl_shader);                                                                                    \
                                                                                                                       \
        GLint gl_shader_success;                                                                                       \
        glGetShaderiv(gl_shader, GL_COMPILE_STATUS, &gl_shader_success);                                               \
        if (gl_shader_success == GL_FALSE) {                                                                           \
            GLint gl_shader_info_log_length;                                                                           \
            glGetShaderiv(gl_shader, GL_INFO_LOG_LENGTH, &gl_shader_info_log_length);                                  \
                                                                                                                       \
            GLchar *gl_shader_info_log = new GLchar[gl_shader_info_log_length];                                        \
            glGetShaderInfoLog(gl_shader, gl_shader_info_log_length, &gl_shader_info_log_length, gl_shader_info_log);  \
                                                                                                                       \
            dief("Failed to compile:%s: %s\n", #TYPE, gl_shader_info_log);                                             \
        }                                                                                                              \
                                                                                                                       \
        gl_shader;                                                                                                     \
    })                                                                                                                 \
    /* GL_SHADER_FROM_STRING */

    GLuint gl_shader_vertex = GL_SHADER_FROM_STRINGS(GL_VERTEX_SHADER, opt_common_shader.c_str(), opt_vertex_shader.c_str());
    GLuint gl_shader_geometry = GL_SHADER_FROM_STRINGS(GL_GEOMETRY_SHADER, opt_common_shader.c_str(), opt_geometry_shader.c_str());
    GLuint gl_shader_fragment = GL_SHADER_FROM_STRINGS(GL_FRAGMENT_SHADER, opt_common_shader.c_str(), opt_fragment_shader.c_str());

#   undef GL_SHADER_FROM_STRING


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


    //--- Vertex Attributes

#   define GL_VERTEX_ATTRIBUTE(BUFFER, INDEX, TYPE)                                                                    \
    ({                                                                                                                 \
        GLenum target = GL_ARRAY_BUFFER;                                                                               \
        GLuint buffer = BUFFER;                                                                                        \
        glBindBuffer(target, buffer);                                                                                  \
                                                                                                                       \
        GLuint index = INDEX;                                                                                          \
        GLint size = 1;                                                                                                \
        GLenum type = TYPE;                                                                                            \
        GLboolean normalized = GL_FALSE;                                                                               \
        GLsizei stride = 0;                                                                                            \
        GLvoid *pointer = 0;                                                                                           \
        glVertexAttribPointer(index, size, type, normalized, stride, pointer);                                         \
                                                                                                                       \
        glEnableVertexAttribArray(index);                                                                              \
    })                                                                                                                 \
    /* GL_VERTEX_ATTRIBUTE */

    GL_VERTEX_ATTRIBUTE(gl_buffer_node_x, 0, GL_FLOAT);
    GL_VERTEX_ATTRIBUTE(gl_buffer_node_y, 1, GL_FLOAT);
    GL_VERTEX_ATTRIBUTE(gl_buffer_node_date, 2, GL_UNSIGNED_INT);
    GL_VERTEX_ATTRIBUTE(gl_buffer_node_nmaintainers, 3, GL_UNSIGNED_INT);
    GL_VERTEX_ATTRIBUTE(gl_buffer_node_cve, 4, GL_UNSIGNED_INT);

#   undef GL_VERTEX_ATTRIBUTE


    //--- Uniforms

    GLint gl_uniform_translate_x = glGetUniformLocation(gl_program, "uTranslateX");
    GLint gl_uniform_translate_y = glGetUniformLocation(gl_program, "uTranslateY");
    GLint gl_uniform_scale = glGetUniformLocation(gl_program, "uScale");

    glUniform1f(gl_uniform_translate_x, -1.0f * opt_tile_x);
    glUniform1f(gl_uniform_translate_y, -1.0f * opt_tile_y);
    glUniform1f(gl_uniform_scale, std::pow(2.0f, opt_tile_z));


    //--- Render

    ({
        GLenum target = GL_ELEMENT_ARRAY_BUFFER;
        GLuint buffer = gl_buffer_edge_index;
        glBindBuffer(target, buffer);

        GLenum mode = GL_LINES;
        GLsizei count = gl_buffer_edge_index_size / sizeof(GLuint);
        GLenum type = GL_UNSIGNED_INT;
        GLvoid *indices = 0;
        std::fprintf(stderr, "glDrawElements(0x%x, %u, 0x%x, %p)\n", mode, count, type, indices);
        glDrawElements(mode, count, type, indices);
    });


    //--- Query Buffers

    ({
        GLenum target = GL_ATOMIC_COUNTER_BUFFER;
        glBindBuffer(target, gl_buffer_atomic_counter);

        GLuint in_degree_max;

        GLintptr offset = 0;
        GLsizeiptr size = sizeof(GLuint);
        glGetBufferSubData(target, offset, size, static_cast<void *>(&in_degree_max));
        
        std::fprintf(stderr, "in_degree_max: %u\n", in_degree_max);
    });


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
        GLsizei width = opt_resolution_x;
        GLsizei height = opt_resolution_y;
        GLenum format = GL_RGBA;
        GLenum type = GL_UNSIGNED_BYTE;
        GLvoid *data = new GLchar[4 * opt_resolution_x * opt_resolution_y];
        glReadPixels(x, y, width, height, format, type, data);

        std::make_tuple(width, height, data);
    });


    //--- Encode RGBA to JPEG

    std::vector<uint8_t> jpeg;

    ({
        jpeg.clear();

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
            jpeg.reserve(jpeg.size() + size);
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

    ({
        GLchar file_name[128];
        snprintf(file_name, sizeof(file_name), "%s", opt_output_file.c_str());

        FILE *file = fopen(file_name, "wb");
        fwrite(jpeg.data(), 1, jpeg.size(), file);

        fprintf(stderr, "Wrote %zu bytes to %s\n", jpeg.size(), file_name);
    });

    return 0;
}

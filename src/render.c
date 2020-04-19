#include "render.h"
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include "fg.h"
#include <zorder.h> 

#include "glad/glad.h"
#include <EGL/egl.h>
#include <EGL/eglext.h>

static void initOpenGL(struct render_ctx *ctx, const char *VSS, const char *FSS);
static void initGraph(struct render_ctx *ctx, int*, int);
static const char *eglGetErrorString(EGLint);

int render_preinit(void) {
	static EGLint const configAttribs[] = {
		EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
		EGL_BLUE_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_RED_SIZE, 8,
		EGL_DEPTH_SIZE, 8,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
		EGL_NONE
	};

	int pbufferWidth = 256;
	int pbufferHeight = 256;

	EGLint pbufferAttribs[] = {
		EGL_WIDTH, pbufferWidth,
		EGL_HEIGHT, pbufferHeight,
		EGL_NONE,
	};

	const char *extString = (const char *)eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);

	// 1. Initialize EGL
	EGLDisplay eglDpy = eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, NULL);
	if (eglDpy == EGL_NO_DISPLAY) {
		printf("eglGetDisplay no display\n");
		return 1;
	}

	EGLint major, minor;
	if (eglInitialize(eglDpy, &major, &minor) == EGL_FALSE) {
		printf("eglInitialize failed: %s\n", eglGetErrorString(eglGetError()));
		return 1;
	}
	printf("got egl version %d %d\n", major, minor);

	// 2. Select an appropriate configuration
	EGLint numConfigs;
	EGLConfig eglCfg;

	eglChooseConfig(eglDpy, configAttribs, &eglCfg, 1, &numConfigs);

	// 3. Create a surface
	EGLSurface eglSurf = eglCreatePbufferSurface(eglDpy, eglCfg, pbufferAttribs);

	// 4. Bind the API
	eglBindAPI(EGL_OPENGL_API);

	// 5. Create a context and make it current
	EGLContext eglCtx = eglCreateContext(eglDpy, eglCfg, EGL_NO_CONTEXT, NULL);

	eglMakeCurrent(eglDpy, eglSurf, eglSurf, eglCtx);

	// from now on use your OpenGL context

	if (!gladLoadGL()) {
		printf("gladLoadGL failed\n");
		exit(1);
	}
	printf("OpenGL %d.%d\n", GLVersion.major, GLVersion.minor);
}

void
render_init(struct render_ctx *ctx, struct graph *graph, int *ZorderIndeces, int sz, const char *VSS, const char *FSS) {
	ctx->g = graph;
	initOpenGL(ctx, VSS, FSS);
	initGraph(ctx, ZorderIndeces, sz);
}

void render_focus_tile(struct render_ctx *ctx, int z, int x, int y) {
	float rz = 1.0;
	for (int i=0; i<z; ++i) {
		rz *= 2.0;
	}

	glUniform1f(ctx->uTranslateX, -1 * x);
	glUniform1f(ctx->uTranslateY, -1 * y);
	glUniform1f(ctx->uScale, rz);
}

void render_first_pass(struct render_ctx *ctx) {

}

void render_display(struct render_ctx *ctx) {
    unsigned int framebuffer; 
    glGenFramebuffers(1, &framebuffer); 
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer); 

    unsigned int texture;
    glGenTextures(1, &texture); 
    glBindTexture(GL_TEXTURE_2D, texture); 
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 256, 256, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL); 
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);  

    unsigned int rbo;
    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo); 
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 256, 256);  
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);

    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        printf("ERROR::FRAMEBUFFER:: Framebuffer is not complete!\n");

    // first pass
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);  

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);	//remember GL_DEPTH_BUFFER_BIT too
	
    ctx->uPass = glGetUniformLocation(ctx->shaderProgram, "pass");
	glUniform1i(ctx->uPass, 1);
	glBindVertexArray(ctx->VAO);
	glDrawElements(GL_LINES, ctx->numIndeces, GL_UNSIGNED_INT, 0);
	//glBindVertexArray(0);

    // second pass
    glBindFramebuffer(GL_FRAMEBUFFER, 0);  

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);	//remember GL_DEPTH_BUFFER_BIT too

	glUniform1i(ctx->uPass, 2);
	glBindVertexArray(ctx->VAO);
    glBindTexture(GL_TEXTURE_2D, texture);
	glDrawElements(GL_LINES, ctx->numIndeces, GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
}

void render_copy_to_buffer(struct render_ctx *ctx, size_t *size, void **pixels) {
	size_t expsize = 256 * 256 * 3 * sizeof(uint8_t);

	if (*pixels == NULL) {
		*size = expsize;
		*pixels = malloc(*size);
	} else if (*size < expsize) {
		*size = expsize;
		*pixels = realloc(*pixels, *size);
	}

	glReadPixels(0, 0,
	             256, 256,
	             GL_RGB,
	             GL_UNSIGNED_BYTE,
	             *pixels);
}

static void initOpenGL(struct render_ctx *ctx, const char *VSS, const char *FSS) {
	int success;
	char infoLog[512];

	unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, (const char * const*)&VSS, NULL);
	glCompileShader(vertexShader);

	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
	if(!success){
		glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
		printf("ERROR: Vertex Shader Compilation Failed\n%s\n", infoLog);
		exit(1);
	}
	
	unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, (const char * const*)&FSS, NULL);
	glCompileShader(fragmentShader);
	
	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
	if(!success){
		glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
		printf("ERROR: Fragment Shader Compilation Failed\n%s\n", infoLog);
		exit(1);	
	}
	
	ctx->shaderProgram = glCreateProgram();
	glAttachShader(ctx->shaderProgram, vertexShader);
	glAttachShader(ctx->shaderProgram, fragmentShader);
	glLinkProgram(ctx->shaderProgram);
	
	glGetProgramiv(ctx->shaderProgram, GL_LINK_STATUS, &success);
	if(!success){
		glGetProgramInfoLog(ctx->shaderProgram, 512, NULL, infoLog);
		printf("ERROR: Shader Program Linking Failed\n%s\n", infoLog);
		exit(1);
	}
	
	glUseProgram(ctx->shaderProgram);

	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	ctx->uTranslateX = glGetUniformLocation(ctx->shaderProgram, "uTranslateX");
	ctx->uTranslateY = glGetUniformLocation(ctx->shaderProgram, "uTranslateY");
	ctx->uScale = glGetUniformLocation(ctx->shaderProgram, "uScale");
	
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

static void initGraph(struct render_ctx *ctx, int *ZorderIndeces, int sz){
	struct graph *const g = ctx->g;	
	float *positions = ctx->vertices;
	int *edges = ctx->indeces;
	
	const int numAttrs = 3;

	ctx->numIndeces = 0;
	ctx->numVertices = 0;
	
	if(sz == 0){	//render whole graph normally on 1 RC
		//fills gContext->positions and gContext->edges with correct data
		rescale(0.0, 0.0, 1.0, 1.0, g);
		
		//2 floats per position
		positions = malloc(g->ncount*numAttrs * sizeof(*positions));
		//2 positions per edge
		edges = malloc(g->ecount*2 * sizeof(*edges));
		
		//interleave edges into contiguous buffer in-order
		for(int i = 0; i < g->ecount; ++i){
			//g->es[i]*2 because interleaved array of positions will be doubly offset
			edges[i*2 + 0] = g->es[i];
			edges[i*2 + 1] = g->et[i];
		}	
		//interleave positions into contiguous buffer in-order
		for(int i = 0; i < g->ncount; ++i){
			positions[i*numAttrs + 0] = g->nx[i];
			positions[i*numAttrs + 1] = g->ny[i];
			positions[i*numAttrs + 2] = g->attr1[i];
		}

		ctx->numIndeces = g->ecount*2;
		ctx->numVertices = g->ncount*2;
	}else if(sz != 1){	
		//render only the RCs specified
	}else{
		//render only the one RC but with viewport moved over it
	}

	//now openGL stuff
	//gen VBO, VAO, EBO
	glGenVertexArrays(1, &ctx->VAO);
	glGenBuffers(1, &ctx->VBO);
	glGenBuffers(1, &ctx->EBO);

	glBindVertexArray(ctx->VAO);
	
	glBindBuffer(GL_ARRAY_BUFFER, ctx->VBO);
	glBufferData(GL_ARRAY_BUFFER, ctx->numVertices*numAttrs*sizeof(*positions), positions, GL_STATIC_DRAW);
	
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ctx->EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, ctx->numIndeces*sizeof(*edges), edges, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, numAttrs*sizeof(*positions), (void*)0);
	glVertexAttribPointer(1, 1, GL_FLOAT, GL_TRUE, numAttrs*sizeof(*positions), (void*)(2*sizeof(float)));
	
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);


}

#define CASE_STR( value ) case value: return #value; 
static const char* eglGetErrorString( EGLint error )
{
    switch( error )
    {
    CASE_STR( EGL_SUCCESS             )
    CASE_STR( EGL_NOT_INITIALIZED     )
    CASE_STR( EGL_BAD_ACCESS          )
    CASE_STR( EGL_BAD_ALLOC           )
    CASE_STR( EGL_BAD_ATTRIBUTE       )
    CASE_STR( EGL_BAD_CONTEXT         )
    CASE_STR( EGL_BAD_CONFIG          )
    CASE_STR( EGL_BAD_CURRENT_SURFACE )
    CASE_STR( EGL_BAD_DISPLAY         )
    CASE_STR( EGL_BAD_SURFACE         )
    CASE_STR( EGL_BAD_MATCH           )
    CASE_STR( EGL_BAD_PARAMETER       )
    CASE_STR( EGL_BAD_NATIVE_PIXMAP   )
    CASE_STR( EGL_BAD_NATIVE_WINDOW   )
    CASE_STR( EGL_CONTEXT_LOST        )
    default: return "Unknown";
    }
}
#undef CASE_STR

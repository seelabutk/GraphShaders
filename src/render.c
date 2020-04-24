#include "render.h"
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include "fg.h"
#include <zorder.h> 

#include "GL/osmesa.h"
#include "glad/glad.h"

static void initOpenGL(struct render_ctx *ctx, const char *VSS, const char *FSS);
static void initGraph(struct render_ctx *ctx, int*, int);

int render_preinit(struct render_ctx *ctx) {

    	ctx->context = OSMesaCreateContextExt(OSMESA_RGBA, 16, 0, 0, NULL);
    	if(!ctx->context){
       		fprintf(stderr, "could not init OSMesa context\n");
   		exit(1);
    	}

    	unsigned int imageBytes = ctx->pbufferWidth*ctx->pbufferHeight*4*sizeof(GLubyte);
    	ctx->imageBuffer = (GLubyte *)malloc(imageBytes);
    	if(!ctx->imageBuffer){
        	fprintf(stderr, "could not allocate image buffer\n");
    		exit(1);
    	}

    	int ret = OSMesaMakeCurrent(ctx->context, ctx->imageBuffer, GL_UNSIGNED_BYTE, ctx->pbufferWidth, ctx->pbufferHeight);
    	if(!ret){
        	fprintf(stderr, "could not bind to image buffer\n");
        	exit(1);
    	}


	if (!gladLoadGL()){
		printf("gladLoadEGL failed\n");
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

void render_display(struct render_ctx *ctx) {
	glClear(GL_COLOR_BUFFER_BIT);	//remember GL_DEPTH_BUFFER_BIT too
	
	glBindVertexArray(ctx->VAO);
	glDrawElements(GL_LINES, ctx->numIndeces, GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
}

void render_copy_to_buffer(struct render_ctx *ctx, size_t *size, void **pixels) {
	size_t expsize = ctx->pbufferWidth * ctx->pbufferHeight * 3 * sizeof(uint8_t);

	if (*pixels == NULL) {
		*size = expsize;
		*pixels = malloc(*size);
	} else if (*size < expsize) {
		*size = expsize;
		*pixels = realloc(*pixels, *size);
	}

	glReadPixels(0, 0,
	             ctx->pbufferWidth, ctx->pbufferHeight,
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

	ctx->numIndeces = g->ecount*2;
	ctx->numVertices = g->ncount*2;
	
	if(sz == 0){	//render whole graph normally on 1 RC

		rescale(0.0, 0.0, 1.0, 1.0, g);
		
		ctx->vertices = malloc(g->ncount*g->numAttribs*sizeof(*ctx->vertices));
		ctx->indeces = malloc(g->ecount*2*sizeof(*ctx->indeces));
		
		//interleave ctx->indeces into contiguous buffer in-order
		for(int i = 0; i < g->ecount; ++i){
			ctx->indeces[i*2 + 0] = g->es[i];
			ctx->indeces[i*2 + 1] = g->et[i];
		}	

		//interleave ctx->vertices into contiguous buffer in-order
		for(int i = 0; i < g->ncount; ++i){
			for(int j = 0; j < g->numAttribs; ++j){
				ctx->vertices[i*g->numAttribs + j] = g->attribs[j][i];
			}
		}

	}else if(sz != 1){	//render only the RCs specified	

	}else{ //render only the one RC but with viewport moved over it
	
	}
		


	glGenVertexArrays(1, &ctx->VAO);
	glGenBuffers(1, &ctx->VBO);
	glGenBuffers(1, &ctx->EBO);

	glBindVertexArray(ctx->VAO);
	
	glBindBuffer(GL_ARRAY_BUFFER, ctx->VBO);
	glBufferData(GL_ARRAY_BUFFER, ctx->numVertices*g->numAttribs*sizeof(*ctx->vertices), ctx->vertices, GL_STATIC_DRAW);
	
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ctx->EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, ctx->numIndeces*sizeof(*ctx->indeces), ctx->indeces, GL_STATIC_DRAW);

	for(int i = 0; i < g->numAttribs; ++i){
		glVertexAttribPointer(i, 1, GL_FLOAT, GL_TRUE/*GL_FALSE?*/, g->numAttribs*sizeof(*ctx->vertices), (void*)(i*sizeof(*ctx->vertices)));
		glEnableVertexAttribArray(i);
	}

}

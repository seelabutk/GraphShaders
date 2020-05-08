#include "render.h"
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include "fg.h"
#include <zorder.h> 

//#include "GL/osmesa.h"
//#include "glad/glad.h"

#include <glad/glad.h>
#define GLAPIENTRY APIENTRY
#include <GL/osmesa.h>

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

	if (!gladLoadGLLoader((GLADloadproc)OSMesaGetProcAddress)){
		printf("gladLoadGL failed\n");
		exit(1);
	}
	printf("OpenGL %s\n", glGetString(GL_VERSION));
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
    /*unsigned int framebuffer; 
    glGenFramebuffers(1, &framebuffer); 
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer); 
   
    fprintf(stderr, "generated framebuffer\n");	
    */
	
    /*unsigned int texture;
    glGenTextures(1, &texture); 
    glBindTexture(GL_TEXTURE_2D, texture); 
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 256, 256, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL); 
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);  

    fprintf(stderr, "created texture\n");
    */

    /*unsigned int rbo;
    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo); 
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 256, 256);  
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);

    fprintf(stderr, "created renderbuffer\n");
    */


    /*if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        printf("ERROR::FRAMEBUFFER:: Framebuffer is not complete!\n");
    */

    // first pass
    //glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);  
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);	
    //ctx->uPass = glGetUniformLocation(ctx->shaderProgram, "pass");
    //glUniform1i(ctx->uPass, 1);
    glBindVertexArray(ctx->VAO);
    glDrawElements(GL_LINES, ctx->numIndeces, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    // second pass
    /*glBindFramebuffer(GL_FRAMEBUFFER, 0);  
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUniform1i(ctx->uPass, 2);
    //glBindVertexArray(ctx->VAO);
    glBindTexture(GL_TEXTURE_2D, texture);
    glDrawElements(GL_LINES, ctx->numIndeces, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    fprintf(stderr, "second pass\n");*/
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
		return;
	}
	
	unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, (const char * const*)&FSS, NULL);
	glCompileShader(fragmentShader);
	
	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
	if(!success){
		glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
		printf("ERROR: Fragment Shader Compilation Failed\n%s\n", infoLog);
		return;
	}
	
	if (ctx->shaderProgram) glDeleteProgram(ctx->shaderProgram);
	ctx->shaderProgram = glCreateProgram();
	glAttachShader(ctx->shaderProgram, vertexShader);
	glAttachShader(ctx->shaderProgram, fragmentShader);
	glLinkProgram(ctx->shaderProgram);
	
	glGetProgramiv(ctx->shaderProgram, GL_LINK_STATUS, &success);
	if(!success){
		glGetProgramInfoLog(ctx->shaderProgram, 512, NULL, infoLog);
		printf("ERROR: Shader Program Linking Failed\n%s\n", infoLog);
		return;
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

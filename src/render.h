#ifndef _RENDER_H_
#define _RENDER_H_

#include <stdlib.h>
#include <inttypes.h>

#include "fg.h"
#include "glad/glad.h"
#include "GL/osmesa.h"

struct render_ctx {
	struct graph *g;

	float *vertices;
	unsigned int *indeces;
	
	unsigned int numVertices;
	unsigned int numIndeces;
	
	int pbufferWidth;
	int pbufferHeight;
	char *imageBuffer;
	unsigned int shaderProgram, VAO, VBO, EBO;
	OSMesaContext context;	
	
	int uTranslateX;
	int uTranslateY;
	int uScale;

	int uPass;
};

int render_preinit(struct render_ctx *ctx);

int render_main(int argc, char **argv);

void render_init(struct render_ctx *ctx, struct graph *graph, int *ZorderIndeces, int sz, const char *VSS, const char *FSS);

void render_focus_tile(struct render_ctx *ctx, int z, int x, int y);

void render_display(struct render_ctx *ctx);

void render_copy_to_buffer(struct render_ctx *, size_t *size, void **pixels);

#endif

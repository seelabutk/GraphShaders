#ifndef _RENDER_H_
#define _RENDER_H_

#include <stdlib.h>
#include <inttypes.h>

#include "fg.h"
#include "glad/glad.h"

struct render_ctx {
	struct graph *g;

	float *vertices;
	unsigned int *indeces;
	
	unsigned int numVertices;
	unsigned int numIndeces;
	
	unsigned int shaderProgram, VAO, VBO, EBO;
	GLint uTranslateX;
	GLint uTranslateY;
	GLint uScale;
    GLint uPass;
};

int render_preinit(void);

int render_main(int argc, char **argv);

void render_init(struct render_ctx *ctx, struct graph *graph, int *ZorderIndeces, int sz, const char *VSS, const char *FSS);

void render_focus_tile(struct render_ctx *ctx, int z, int x, int y);

void render_display(struct render_ctx *ctx);

void render_copy_to_buffer(struct render_ctx *, size_t *size, void **pixels);

#endif

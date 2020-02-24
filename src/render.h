#ifndef _RENDER_H_
#define _RENDER_H_

#include <stdlib.h>
#include <inttypes.h>

#include "fg.h"
#include "glad/glad.h"

struct render_ctx {
	struct graph g;

	float *vertices;
	unsigned int *indeces;
	
	unsigned int numVertices;
	unsigned int numIndeces;
	
	unsigned int shaderProgram, VBO, EBO, VAO;
	GLint uTranslateX;
	GLint uTranslateY;
	GLint uScale;
};

int render_main(int argc, char **argv);

int render_init(struct render_ctx *ctx);

void render_focus_tile(struct render_ctx *ctx, int z, int x, int y);

void render_display(struct render_ctx *ctx);

void render_copy_to_buffer(struct render_ctx *, size_t *size, void **pixels);

#endif

#ifndef _RENDER_H_
#define _RENDER_H_

#include <stdlib.h>
#include <inttypes.h>

#include "fg.h"
#include "glad/glad.h"
#include "GL/osmesa.h"

struct render_params {
	pthread_mutex_t lock;
	pthread_cond_t cond;
	char *dataset;
	GLfloat x;
	GLfloat y;
	GLfloat z;
};

void *render(void *);

#endif

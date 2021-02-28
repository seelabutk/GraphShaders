#ifndef C_VEC
#define C_VEC

#include <stdlib.h>
#include <string.h>
#include <glad/glad.h>

#define Vec(T)\
	struct { T *data; unsigned long length, capacity; }


#define vec_init(v)\
	memset((v), 0, sizeof(*(v)))

#define vec_destroy(v)\
	free((v)->data)

#define _vec_unpack(v)\
	(char**)&(v)->data, &(v)->length, &(v)->capacity, sizeof(*(v)->data)

#define vec_push(v, val)\
	( _vec_expand(_vec_unpack(v)) ? -1 :\
	((v)->data[(v)->length++] = (val), 0), 0 )

#define vec_at(v, idx)\
	(v)->data[idx]

int _vec_expand(char **data, unsigned long *length, unsigned long *capacity, unsigned long memsz);

typedef Vec(GLuint) Vec_GLuint;

#endif


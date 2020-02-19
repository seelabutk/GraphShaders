#ifndef _FG_H_
#define _FG_H_

#include <stddef.h>
#include <stdint.h>


typedef void (*emitter)(int, int);
typedef void (*raytracer)(float, float, float, float, float, emitter);


struct graph {
	char *node_filename;
	char *edge_filename;

	size_t ncount;
	size_t nsize;
	float *nx;
	float *ny;
	char **nid;

	size_t ecount;
	size_t esize;
	uint16_t *es;
	uint16_t *et;
};

void
emit(int r, int c);

void
raytrace(float x0, float y0, float x1, float y1, float d, emitter emit);

int
load(char *node_filename, char *edge_filename, struct graph *graph);

void
rescale(struct graph *graph, float x0, float x1, float y0, float y1);

int
load_arg(struct graph *graph, int argc, char **argv);

int
fg_main(int argc, char **argv);

#endif

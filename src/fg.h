#ifndef _FG_H_
#define _FG_H_

#include <stddef.h>
#include <stdint.h>


typedef void (*emitter)(int, int);
typedef void (*raytracer)(float, float, float, float, float, emitter);

typedef int row_t;
typedef int col_t;
typedef int rc_t;
typedef int edgenum_t;

struct graph {
	char *node_filename;
	char *edge_filename;

	uint32_t ncount;
	struct attrib *nattribs;

	uint32_t ecount;
	struct attrib *eattribs;
};


void
emit(int r, int c);

void
raytrace(float x0, float y0, float x1, float y1, float d, emitter emit);

int
load(char *node_filename, char *edge_filename, struct graph *graph);

/*
void
rescale(float x0, float x1, float y0, float y1, struct graph *graph);
*/

int
load_arg(struct graph *graph);

/*
void
partition(struct graph *graph, int n, void (*preinit)(rc_t), void (*init)(rc_t), void (*emit)(rc_t, edgenum_t), void (*fini)(rc_t));
*/

int
fg_main(int argc, char **argv);

#endif

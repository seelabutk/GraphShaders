#include "fg.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <assert.h>
#include <inttypes.h>
#include <string.h>
#include <linux/limits.h>

#include <zorder.h>


void
emit(int r, int c) {
	printf("%d,%d\n", r, c);
}


void
raytrace(float x0, float y0, float x1, float y1, float d, emitter emit) {
	float X = floor(x0/(1/d));
	float Y = floor(y0/(1/d));
	float FX = floor(x1/(1/d));
	float FY = floor(y1/(1/d));
	float stepX = x1 > x0 ? 1 : -1;
	float stepY = y1 > y0 ? 1 : -1;
	const float dx = x1 - x0;
	const float dy = y1 - y0;
	const float nextX = (X+stepX)*(1/d);
	const float nextY = (Y+stepY)*(1/d);
	float tMaxX = (nextX - x0) / dx;
	float tMaxY = (nextY - y0) / dy;
	const float tDeltaX = (1/d) / dx * stepX;
	const float tDeltaY = (1/d) / dy * stepY;
	emit((int)(X/d), (int)(Y/d));
	for (int iii=0; iii<10000 && !(X == FX && Y == FY); ++iii) {
		if (tMaxX < tMaxY) {
			tMaxX += tDeltaX;
			X += stepX;
		} else {
			tMaxY += tDeltaY;
			Y += stepY;
		}
		emit((int)(X/d), (int)(Y/d));
	}
}


int
load(char *node_filename, char *edge_filename, struct graph *graph) {
#define READ(x) fread(&(x), sizeof(x), 1, f)
	FILE *f;
	uint32_t i;

	graph->node_filename = node_filename;
	graph->edge_filename = edge_filename;

	f = fopen(node_filename, "r");
	if (!f) return 1;

	READ(graph->ncount);
	graph->nattribs = malloc(graph->ncount * sizeof(*graph->nattribs));
	for (i=0; i<graph->ncount; ++i) {
		READ(graph->nattribs[i].type);
		READ(graph->nattribs[i].count);
		READ(graph->nattribs[i].range);
		graph->nattribs[i].data = malloc(4 * (size_t)graph->nattribs[i].count);
		fread(graph->nattribs[i].data, 4, graph->nattribs[i].count, f);
	}

	fclose(f);

	f = fopen(edge_filename, "r");
	if (!f) return 1;

	READ(graph->ecount);
	graph->eattribs = malloc(graph->ecount * sizeof(*graph->eattribs));
	for (i=0; i<graph->ecount; ++i) {
		READ(graph->eattribs[i].type);
		READ(graph->eattribs[i].count);
		READ(graph->eattribs[i].range);
		graph->eattribs[i].data = malloc(4 * (size_t)graph->eattribs[i].count);
		fread(graph->eattribs[i].data, 4, graph->eattribs[i].count, f);
	}

	fclose(f);
#undef READ
}


/*
void
rescale(float x0, float y0, float x1, float y1, struct graph *graph) {
	float xmin, xmax = xmin = graph->nx[0];
	float ymin, ymax = ymin = graph->ny[0];
	for (size_t i=1; i<graph->ncount; ++i) {
		if (graph->nx[i] < xmin) xmin = graph->nx[i];
		if (graph->nx[i] > xmax) xmax = graph->nx[i];
		if (graph->ny[i] < ymin) ymin = graph->ny[i];
		if (graph->ny[i] > ymax) ymax = graph->ny[i];
	}
	
	for (size_t i=0; i<graph->ncount; ++i) {
		graph->nx[i] = (graph->nx[i] - xmin) / (xmax - xmin) * (x1 - x0) + x0;
		graph->ny[i] = (graph->ny[i] - ymin) / (ymax - ymin) * (y1 - y0) + y0;
	}
}
*/

/*
void
partition(struct graph *graph, int n, void (*preinit)(rc_t), void (*init)(rc_t), void (*emit)(rc_t, edgenum_t), void (*fini)(rc_t)) {
	edgenum_t edge;
	ZOrderStruct *zo;

	void myemit(row_t row, col_t col) {
		rc_t rc = zoGetIndex(zo, row, col, 0);
		emit(rc, edge);
	}

	zo = zoInitZOrderIndexing(n, n, 0, ZORDER_LAYOUT);
	preinit(zo->zo_totalSize);

	for (int i=0; i<n; ++i)
	for (int j=0; j<n; ++j) {
		rc_t rc = zoGetIndex(zo, i, j, 0);
		init(rc);
	}

	rescale(0.0, 0.0, n-1.0, n-1.0, graph);

	for (edge=0; edge<graph->ecount; ++edge) {
		float x0 = graph->nx[graph->es[edge]];
		float y0 = graph->ny[graph->es[edge]];
		float x1 = graph->nx[graph->et[edge]];
		float y1 = graph->ny[graph->et[edge]];
		if (x1 < x0) {
			float t;
			t = x0; x0 = x1; x1 = t;
			t = y0; y0 = y1; y1 = t;
		}
		if ((x0 < x1 + 0.001) && (y0 > y1 - 0.001)) continue;
		raytrace(x0, y0, x1, y1, 1.0, myemit);
	}

	for (int i=0; i<n; ++i)
	for (int j=0; j<n; ++j) {
		rc_t rc = zoGetIndex(zo, i, j, 0);
		fini(rc);
	}
}
*/


int
load_arg(struct graph *graph) {
	char nodespath[PATH_MAX];
	char edgespath[PATH_MAX];
	char *s, *rootdir;

	rootdir = (s = getenv("FG_GRAPH")) ? s : "data/les-miserables";
	snprintf(nodespath, sizeof(nodespath), "%s/%s", rootdir, "nodes.csv");
	snprintf(edgespath, sizeof(edgespath), "%s/%s", rootdir, "edges.csv");
	
	int rc = load(nodespath, edgespath, graph);
	return rc;
}

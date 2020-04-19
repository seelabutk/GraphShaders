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
	FILE *f;
	char *line = NULL;
	size_t size = 0;
	ssize_t nread;
	int numattrs = 0;
	char *skips;
	int i,j;
	enum { ORDER_UNKNOWN = 0, ORDER_X_Y_ID, ORDER_SOURCE_TARGET } order = ORDER_UNKNOWN;
	
	graph->node_filename = node_filename;
	graph->edge_filename = edge_filename;
	graph->ncount = 0;
	graph->nsize = 16;
	graph->ecount = 0;
	graph->esize = 16;
	graph->es = malloc(graph->esize * sizeof(*graph->es));
	graph->et = malloc(graph->esize * sizeof(*graph->et));
	graph->numAttribs = 0;
	graph->nx = NULL;
	graph->ny = NULL;
	
	f = fopen(node_filename, "r");
	assert(f);
	
	//get number of attributes
	nread = getline(&line, &size, f);
	i = 0;
	for(char *c = line; 1; ++c, ++i){
		if(*c == '\n'){	//fix last attribute having a newline after it
			line[i] = '\0';
			break;
		}else if(*c == ',')	++numattrs;
	}
	++numattrs;


	//check if all datatypes are floats before entering loop
	skips = (char*)malloc(sizeof(*skips)*numattrs);
	nread = getline(&line, &size, f);
	i = 0;
	for(char *tok = strtok(line,","); i < numattrs ; tok = strtok(NULL, ","), ++i){
		char *e;
		strtod(tok, &e);	//can't use atof cuz can't tell the difference between fail and reading 0.0
		if((*e != '\0') && (*e != '\n')){
			skips[i] = 1;
		}else{
			skips[i] = 0;
			++graph->numAttribs;
		}
	}


	//store attribute names
	rewind(f);
	nread = getline(&line, &size, f);
	graph->attribNames = (char**)malloc(sizeof(*graph->attribNames)*numattrs);
	i = j = 0;
	for(char *tok = strtok(line,","); i < numattrs; tok = strtok(NULL, ","), ++i){
		if(skips[i] == 1)	continue;
		graph->attribNames[j] = strdup(tok);
		++j;
	}

	//allocate attributes - will not contained unused attributes that have no float conversion
	graph->attribs = (float**)malloc(sizeof(*graph->attribs)*graph->numAttribs);
	for(i = 0; i < graph->numAttribs; ++i){
		graph->attribs[i] = (float*)malloc(sizeof(**graph->attribs)*graph->nsize);
	}


	//read the data in
	while ((nread = getline(&line, &size, f)) > 0) {
		
		//storing
		i = j = 0;
		char *end, *tok;
		end = line;

		//need to use strsep instead of strtok b/c some nodes don't have values, leading to multiple delimeters in a row, which strtok can not compute
		for(tok = strsep(&end,","); i < numattrs; tok = strsep(&end,","), ++i){
			if(skips[i] == 1)	continue;
			graph->attribs[j][graph->ncount] = atof(tok);	
			++j;
		}

		//resizing
		if (++graph->ncount == graph->nsize) {
			graph->nsize *= 2;
			for(i = 0; i < graph->numAttribs; ++i){
				graph->attribs[i] = realloc(graph->attribs[i], graph->nsize*sizeof(*graph->attribs[i]));
			}	
		} 

	}
	free(line);
	fclose(f);

	//set graph->nx and graph->ny to the correct attribute array locations - this is kinda hard-coded
	for(i = 0; i < graph->numAttribs; ++i){
		if(strcmp(graph->attribNames[i], "x") == 0){
			graph->nx = graph->attribs[i];
		}else if(strcmp(graph->attribNames[i], "y") == 0){
			graph->ny = graph->attribs[i];
		}
	}

	//backup, for if no x and y is explicitely specified, assume first 2 attributes in file are x and y
	if(graph->nx == NULL)
		graph->nx = graph->attribs[0];
	if(graph->ny == NULL)
		graph->ny = graph->attribs[1];
	

	f = fopen(edge_filename, "r");
	assert(f);
	line = NULL;
	size = 0;

	nread = getline(&line, &size, f);
	if (strcmp(line, "src,dst\n") == 0) order = ORDER_SOURCE_TARGET;
	else	fprintf(stderr, "Unknown header line: \"%s\"\n", line);

	while ((nread = getline(&line, &size, f)) > 0) {
		if (n++ == 0) {
			if (strcmp(line, "source,target\n") == 0) order = ORDER_SOURCE_TARGET;
			else {
				fprintf(stderr, "Unknown header line: \"%s\"\n", line);
			}
		} else if (order == ORDER_SOURCE_TARGET) {
			sscanf(line, "%"SCNu16",%"SCNu16,
			       graph->es + graph->ecount,
			       graph->et + graph->ecount);
			if (++graph->ecount == graph->esize) {
				graph->esize *= 2;
				graph->es = realloc(graph->es, graph->esize * sizeof(*graph->es));
				graph->et = realloc(graph->et, graph->esize * sizeof(*graph->et));
			}
		}
	}

	free(line);
	fclose(f);
	
	return 0;
}


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

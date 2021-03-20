#include "voxel_traversal.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define MAX_ITER 100

unsigned to_tile_id(unsigned long resolution, unsigned x, unsigned y){
	return y*resolution+x;
}

Vec_GLuint voxel_traversal(unsigned long RESOLUTION, double x0, double y0, double x1, double y1, double xMin, double yMin, double xMax, double yMax)
{	
	const double VOXEL_SIZE =  ( double ) 1 / ( double ) RESOLUTION;
    
	Vec_GLuint tiles;
	vec_init(&tiles);	
	
	double X  = interpolate(xMin, xMax, 0, RESOLUTION, x0);
	double Y  = interpolate(yMin, yMax, 0, RESOLUTION, y0);
	double FX = interpolate(xMin, xMax, 0, RESOLUTION, x1);
	double FY = interpolate(yMin, yMax, 0, RESOLUTION, y1);
	
	unsigned curr_tile_id = to_tile_id(RESOLUTION, floor(X), floor(Y));
	unsigned dest_tile_id = to_tile_id(RESOLUTION, floor(FX),floor(FY));
    
    printf("xMin: %f, yMin: %f, xMax: %f, yMax: %f\n", xMin, yMin, xMax, yMax);
    printf("v0: (%f,%f), v1: (%f,%f)\n", x0,y0, x1,y1);
    printf("v0: (%f,%f), v1: (%f,%f)\n", X,Y, FX, FY);
    printf("start tile id: %d\n", curr_tile_id);
    printf("dest  tile id: %d\n", dest_tile_id);
	printf("%d: (%f,%f)\n", curr_tile_id, X, Y);

    assert(curr_tile_id < RESOLUTION*RESOLUTION);
	assert(dest_tile_id < RESOLUTION*RESOLUTION);

    vec_push(&tiles, curr_tile_id);
    if(curr_tile_id == dest_tile_id) return tiles;

    int dx =  abs((int)(floor(FX) - floor(X)));
    int dy = -abs((int)(floor(FY) - floor(Y)));

    int sx = (int)X < (int)FX ? 1 : -1;
    int sy = (int)Y < (int)FY ? 1 : -1;
    
    int err = dx + dy;
    int e2; 
    
    int last_tile_id = curr_tile_id;
	int i;

    int loop = 2;

    for (i=0; (i<MAX_ITER) && loop; ++i){
		
        if(curr_tile_id == dest_tile_id || loop != 2)   --loop;

        e2 = 2*err;
        if(e2 - dy > dx - e2){
            err += dy;
            X += sx;
        }else{
            err += dx;
            Y += sy;
        }

        curr_tile_id = to_tile_id(RESOLUTION, floor(X), floor(Y));
	    printf("%d: (%f,%f)\n", curr_tile_id, X, Y);
        if(curr_tile_id >= RESOLUTION*RESOLUTION){
            break;
        }
		
        if(last_tile_id != curr_tile_id){
			vec_push(&tiles, curr_tile_id);
			last_tile_id = curr_tile_id;
		}	
	}

    if(curr_tile_id != dest_tile_id){
        vec_push(&tiles, dest_tile_id);
    }
	
	return tiles;
}

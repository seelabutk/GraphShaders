#include "voxel_traversal.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define swap(x, y) do { typeof(x) tmp = x; x = y; y = tmp; } while (0)

unsigned to_tile_id(unsigned long resolution, unsigned x, unsigned y){
	return y*resolution+x;
}

void swapsort(double *f1, double *f2)
{
    double tmp;

    if (*f1 <= *f2) return;

    tmp = *f1;
    *f1 = *f2;
    *f2 = tmp;    
}

Vec_GLuint voxel_traversal(int xres, int yres, double X, double Y, double FX, double FY, double xMin, double yMin, double xMax, double yMax)
{
    double x1  = interpolate(xMin, xMax, 0, xres, X);
    double y1  = interpolate(yMin, yMax, 0, yres, Y);
    double x2 = interpolate(xMin, xMax, 0, xres, FX);
    double y2 = interpolate(yMin, yMax, 0, yres, FY);
    
    int steep = (abs(y2-y1) > abs(x2-x1));
    
    if(steep){
        swap(x1, y1);
        swap(x2, y2);
        swap(xres, yres);
    }
    if(x1 > x2){
        swap(x1, x2);
        swap(y1, y2);
    }

    double dx = x2 - x1;
    int i, j;
    double cx1, cx2, cy1, cy2, t;
    double xstep = 1.f;
    
    Vec_GLuint tiles;
    vec_init(&tiles);	
	
    for (i = (int)floor(x1/xstep); i <= (int)floor(x2/xstep); i ++)
    {
        if (i < 0) break;
        if (i >= xres) break;

        cx1 = i;
        cx2 = i + xstep;
      
        t = (cx1-x1)/dx;
        cy1 = (1-t)*y1 + t*y2;
        t = (cx2-x1)/dx;
        cy2 = (1-t)*y1 + t*y2;
        
        swapsort(&cy1, &cy2);
        
        if (cy1 < 0) cy1 = 0;
        if (cy1 >= yres) cy1 = yres-1;
        if (cy2 < 0) cy2 = 0;
        if (cy2 >= yres) cy2 = yres-1;        

        for (j = (int)floor(cy1); j <= (int)floor(cy2); j++)
        {
            if (j >= yres) break;
            if (j < 0) break;
            
            int curr_tile_id;            
            if(steep){
                curr_tile_id = to_tile_id(xres, j, i);
                printf(" ==Output== %d: voxel (i, j) = (%d, %d)\n", curr_tile_id, j, i);
            } else {
                curr_tile_id = to_tile_id(xres, i, j);
                printf(" ==Output== %d: voxel (i, j) = (%d, %d)\n", curr_tile_id, i, j);
            }
            vec_push(&tiles, curr_tile_id);
        }
    }

    return tiles;
}

#ifndef _VOXEL_TRAVERSAL_
#define _VOXEL_TRAVERSAL_
#include "vec.h"


static inline double interpolate(double x0, double x1, double y0, double y1, double x){
	//return y0 + ((y1-y0)*(x-x0))/(x1-x0);
    return (y0*(x1-x) + y1*(x-x0))/(x1-x0);
}

Vec_GLuint voxel_traversal(unsigned long RESOLUTION, double x0, double y0, double x1, double y1, double minX, double minY, double maxX, double maxY);
#endif

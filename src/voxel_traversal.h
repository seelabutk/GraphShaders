#ifndef _VOXEL_TRAVERSAL_
#define _VOXEL_TRAVERSAL_
#include <glad/glad.h>
#include "vec.h"

typedef Vec(GLuint) Vec_GLuint;

static inline double interpolate(double x0, double x1, double y0, double y1, double x)
{
	return y0 + ((y1-y0)*(x-x0))/(x1-x0);
}

Vec_GLuint voxel_traversal(unsigned RESOLUTION, double x0, double y0, double x1, double y1, double minX, double minY, double maxX, double maxY);
#endif

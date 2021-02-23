#include "voxel_traversal.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define MAX_ITER 100

unsigned to_tile_id(unsigned resolution, unsigned x, unsigned y)
{
	return y*resolution+x;
}

Vec_GLuint voxel_traversal(unsigned RESOLUTION, double x0, double y0, double x1, double y1, double xMin, double yMin, double xMax, double yMax)
{	
	const double VOXEL_SIZE =  ( double ) 1 / ( double ) RESOLUTION;
	
	Vec_GLuint tiles;
	vec_init(&tiles);	

	if(	   (x0 > xMax) || (y0 > yMax) 
		|| (x1 > xMax) || (y1 > yMax)
		|| (x0 < xMin) || (y0 < yMin)
		|| (x1 < xMin) || (y1 < yMin)
	)	return tiles;
	
	double X = floor ( interpolate(xMin, xMax, 0, RESOLUTION, x0) );
	double Y = floor ( interpolate(yMin, yMax, 0, RESOLUTION, y0) );
	double FX = floor( interpolate(xMin, xMax, 0, RESOLUTION, x1) );
	double FY = floor( interpolate(yMin, yMax, 0, RESOLUTION, y1) );
	
	if(y0 == yMax)	Y  -= 1.0f;
	if(y1 == yMax)  FY -= 1.0f;
	if(x0 == xMax)	X  -= 1.0f;
	if(x1 == xMax)	FY -= 1.0f; 	
	
	unsigned curr_tile_id = to_tile_id(RESOLUTION, floor(X), floor(Y));
	assert(curr_tile_id < RESOLUTION*RESOLUTION);
	vec_push(&tiles, curr_tile_id);

	const double dx = x1 - x0;
	const double dy = y1 - y0;

	const double stepX = dx >= 0 ? 1 : -1;
	const double stepY = dy >= 0 ? 1 : -1;

	const double nextX = stepX >= 0 ? ( X + stepX ) * VOXEL_SIZE : ( X + stepX + 1 ) * VOXEL_SIZE;
	const double nextY = stepY >= 0 ? ( Y + stepY ) * VOXEL_SIZE : ( Y + stepY + 1 ) * VOXEL_SIZE;

	double tMaxX = dx != 0 ? ( nextX - x0 ) : DBL_MAX;
	double tMaxY = dy != 0 ? ( nextY - y0 ) : DBL_MAX;

	const double tDeltaX = dx != 0 ? stepX / ( RESOLUTION * dx ) : DBL_MAX;
	const double tDeltaY = dy != 0 ? stepY / ( RESOLUTION * dy ) : DBL_MAX;

	unsigned i;
	unsigned last_tile_id = !curr_tile_id;
	for ( i = 0; ( i < MAX_ITER ) && !( ( X == FX ) || ( Y == FY ) ); i++ )
	{
		if ( tMaxX < tMaxY )
		{
			tMaxX += tDeltaX;
			X += stepX;
		}
		else
		{
			tMaxY += tDeltaY;
			Y += stepY;
		}
		curr_tile_id = to_tile_id(RESOLUTION, floor(X), floor(Y));
		
		assert(curr_tile_id < RESOLUTION*RESOLUTION);
		
		if(last_tile_id != curr_tile_id){
			vec_push(&tiles, curr_tile_id);
			last_tile_id = curr_tile_id;
		}
	}
	
	return tiles;
}

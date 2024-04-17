/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "tr_local.h"

/*

This file does all of the processing necessary to turn a raw grid of points
read from the map file into a srfGridMesh_t ready for rendering.

The level of detail solution is direction independent, based only on subdivided
distance from the true curve.

Only a single entry point:

srfGridMesh_t *R_SubdividePatchToGrid( int width, int height,
								drawVert_t points[MAX_PATCH_SIZE*MAX_PATCH_SIZE] ) {

*/

/*
=================
R_FreeSurfaceGridMesh
=================
*/
void R_FreeSurfaceGridMesh(srfGridMesh_t *grid)
{
	R_FreeSurfaceGridMesh_plus(grid);
}

/*
=================
R_SubdividePatchToGrid
=================
*/
srfGridMesh_t *R_SubdividePatchToGrid(int width, int height,
									  drawVert_t points[MAX_PATCH_SIZE * MAX_PATCH_SIZE])
{
	R_SubdividePatchToGrid_plus(width, height, points);
}
/*
===============
R_GridInsertColumn
===============
*/
srfGridMesh_t *R_GridInsertColumn(srfGridMesh_t *grid, int column, int row, vec3_t point, float loderror)
{
	R_GridInsertColumn_plus(grid, column, row, point, loderror);
}
/*
===============
R_GridInsertRow
===============
*/
srfGridMesh_t *R_GridInsertRow(srfGridMesh_t *grid, int row, int column, vec3_t point, float loderror)
{
	R_GridInsertRow_plus(grid, row, column, point, loderror);
}
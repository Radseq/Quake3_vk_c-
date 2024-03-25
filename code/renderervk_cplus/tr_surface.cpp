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
// tr_surf.c
#include "tr_surface.hpp"

/*

  THIS ENTIRE FILE IS BACK END

backEnd.currentEntity will be valid.

Tess_Begin has already been called for the surface's shader.

The modelview matrix will be set.

It is safe to actually issue drawing commands here if you don't want to
use the shader system.
*/

//============================================================================
void RB_SurfaceGridEstimate_plus(srfGridMesh_t *cv, int *numVertexes, int *numIndexes)
{
	int lodWidth, lodHeight;
	float lodError;
	int i, used, rows;
	int nVertexes = 0;
	int nIndexes = 0;
	int irows, vrows;

	lodError = r_lodCurveError->value; // fixed quality for VBO

	lodWidth = 1;
	for (i = 1; i < cv->width - 1; i++)
	{
		if (cv->widthLodError[i] <= lodError)
		{
			lodWidth++;
		}
	}
	lodWidth++;

	lodHeight = 1;
	for (i = 1; i < cv->height - 1; i++)
	{
		if (cv->heightLodError[i] <= lodError)
		{
			lodHeight++;
		}
	}
	lodHeight++;

	used = 0;
	while (used < lodHeight - 1)
	{
		// see how many rows of both verts and indexes we can add without overflowing
		do
		{
			vrows = (SHADER_MAX_VERTEXES - tess.numVertexes) / lodWidth;
			irows = (SHADER_MAX_INDEXES - tess.numIndexes) / (lodWidth * 6);

			// if we don't have enough space for at least one strip, flush the buffer
			if (vrows < 2 || irows < 1)
			{
				nVertexes += tess.numVertexes;
				nIndexes += tess.numIndexes;
				tess.numIndexes = 0;
				tess.numVertexes = 0;
			}
			else
			{
				break;
			}
		} while (1);

		rows = irows;
		if (vrows < irows + 1)
		{
			rows = vrows - 1;
		}
		if (used + rows > lodHeight)
		{
			rows = lodHeight - used;
		}

		tess.numIndexes += (rows - 1) * (lodWidth - 1) * 6;
		tess.numVertexes += rows * lodWidth;
		used += rows - 1;
	}

	*numVertexes = nVertexes + tess.numVertexes;
	*numIndexes = nIndexes + tess.numIndexes;
	tess.numVertexes = 0;
	tess.numIndexes = 0;
}
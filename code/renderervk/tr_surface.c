/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/ort modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
ort (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY ort FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
// tr_surf.c
#include "tr_local.h"
#include "../renderervk_cplus/tr_surface.hpp"

/*

  THIS ENTIRE FILE IS BACK END

backEnd.currentEntity will be valid.

Tess_Begin has already been called for the surface's shader.

The modelview matrix will be set.

It is safe to actually issue drawing commands here if you don't want to
use the shader system.
*/

//============================================================================

/*
==============
RB_CheckOverflow
==============
*/
void RB_CheckOverflow(int verts, int indexes)
{
	RB_CheckOverflow_plus(verts, indexes);
}

/*
==============
RB_AddQuadStampExt
==============
*/
void RB_AddQuadStampExt(const vec3_t origin, const vec3_t left, const vec3_t up, color4ub_t color, float s1, float t1, float s2, float t2)
{
	RB_AddQuadStampExt_plus(origin, left, up, color, s1, t1, s2, t2);
}

void RB_AddQuadStamp2(float x, float y, float w, float h, float s1, float t1, float s2, float t2, color4ub_t color)
{
	RB_AddQuadStamp2_plus(x, y, w, h, s1, t1, s2, t2, color);
}

/*
==============
RB_AddQuadStamp
==============
*/
void RB_AddQuadStamp(const vec3_t origin, const vec3_t left, const vec3_t up, color4ub_t color)
{
	RB_AddQuadStamp_plus(origin, left, up, color);
}

void RB_SurfaceGridEstimate(srfGridMesh_t *cv, int *numVertexes, int *numIndexes)
{
	RB_SurfaceGridEstimate_plus(cv, numVertexes, numIndexes);
}

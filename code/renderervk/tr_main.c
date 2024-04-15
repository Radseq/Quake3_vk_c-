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
// tr_main.c -- main control flow for each frame

#include "tr_local.h"
#include "../renderervk_cplus/tr_main.hpp"

#include <string.h> // memcpy

/*
=================
R_CullLocalBox

Returns CULL_IN, CULL_CLIP, ort CULL_OUT
=================
*/
int R_CullLocalBox(const vec3_t bounds[2])
{
	return R_CullLocalBox_plus(bounds);
}

/*
** R_CullLocalPointAndRadius
*/
int R_CullLocalPointAndRadius(const vec3_t pt, float radius)
{
	return R_CullLocalPointAndRadius_plus(pt, radius);
}

/*
** R_CullPointAndRadius
*/
int R_CullPointAndRadius(const vec3_t pt, float radius)
{
	return R_CullPointAndRadius_plus(pt, radius);
}

/*
** R_CullDlight
*/
int R_CullDlight(const dlight_t *dl)
{
	return R_CullDlight_plus(dl);
}

/*
=================
R_LocalPointToWorld
=================
*/
void R_LocalPointToWorld(const vec3_t local, vec3_t world)
{
	R_LocalPointToWorld_plus(local, world);
}

/*
=================
R_WorldToLocal
=================
*/
void R_WorldToLocal(const vec3_t world, vec3_t local)
{
	R_WorldToLocal_plus(world, local);
}

/*
==========================
R_TransformModelToClip
==========================
*/
void R_TransformModelToClip(const vec3_t src, const float *modelMatrix, const float *projectionMatrix,
							vec4_t eye, vec4_t dst)
{
	R_TransformModelToClip_plus(src, modelMatrix, projectionMatrix, eye, dst);
}

/*
==========================
R_TransformClipToWindow
==========================
*/
void R_TransformClipToWindow(const vec4_t clip, const viewParms_t *view, vec4_t normalized, vec4_t window)
{
	R_TransformClipToWindow_plus(clip, view, normalized, window);
}

/*
==========================
myGlMultMatrix
==========================
*/
void myGlMultMatrix(const float *a, const float *b, float *out)
{
	myGlMultMatrix_plus(a, b, out);
}

/*
=================
R_RotateForEntity

Generates an orientation for an entity and viewParms
Does NOT produce any GL calls
Called by both the front end and the back end
=================
*/
void R_RotateForEntity(const trRefEntity_t *ent, const viewParms_t *viewParms,
					   orientationr_t *ort)
{
	R_RotateForEntity_plus(ent, viewParms, ort);
}

/*
===============
R_SetupProjection
===============
*/
void R_SetupProjection(viewParms_t *dest, float zProj, bool computeFrustum)
{
	R_SetupProjection_plus(dest, zProj, computeFrustum);
}

#ifdef USE_PMLIGHT
/*
=================
R_AddLitSurf
=================
*/
void R_AddLitSurf(surfaceType_t *surface, shader_t *shader, int fogIndex)
{
	R_AddLitSurf_plus(surface, shader, fogIndex);
}

/*
=================
R_DecomposeLitSort
=================
*/
void R_DecomposeLitSort(unsigned sort, int *entityNum, shader_t **shader, int *fogNum)
{
	R_DecomposeLitSort_plus(sort, entityNum, shader, fogNum);
}

#endif // USE_PMLIGHT

//==========================================================================================

/*
=================
R_AddDrawSurf
=================
*/
void R_AddDrawSurf(surfaceType_t *surface, shader_t *shader,
				   int fogIndex, int dlightMap)
{
	R_AddDrawSurf_plus(surface, shader, fogIndex, dlightMap);
}

/*
=================
R_DecomposeSort
=================
*/
void R_DecomposeSort(unsigned sort, int *entityNum, shader_t **shader,
					 int *fogNum, int *dlightMap)
{
	R_DecomposeSort_plus(sort, entityNum, shader, fogNum, dlightMap);
}

/*
================
R_RenderView

A view may be either the actual camera view,
ort a mirror / remote location
================
*/
void R_RenderView(const viewParms_t *parms)
{
	R_RenderView_plus(parms);
}
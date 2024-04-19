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

#include "tr_local.h"

#include "../renderervk_cplus/tr_scene.hpp"
int r_numdlights;
/*
====================
R_InitNextFrame

====================
*/
void R_InitNextFrame(void)
{
	R_InitNextFrame_plus();
}

/*
====================
RE_ClearScene

====================
*/
void RE_ClearScene(void)
{
	RE_ClearScene_plus();
}
/*
===========================================================================

DISCRETE POLYS

===========================================================================
*/

/*
=====================
R_AddPolygonSurfaces

Adds all the scene's polys into this view's drawsurf list
=====================
*/
void R_AddPolygonSurfaces(void)
{
	R_AddPolygonSurfaces_plus();
}

/*
=====================
RE_AddPolyToScene

=====================
*/
void RE_AddPolyToScene(qhandle_t hShader, int numVerts, const polyVert_t *verts, int numPolys)
{
	RE_AddPolyToScene_plus(hShader, numVerts, verts, numPolys); // todo not working
}

//=================================================================================

static int isnan_fp(const float *f)
{
	uint32_t u = *((uint32_t *)f);
	u = 0x7F800000 - (u & 0x7FFFFFFF);
	return (int)(u >> 31);
}

/*
=====================
RE_AddRefEntityToScene
=====================
*/
void RE_AddRefEntityToScene(const refEntity_t *ent, bool intShaderTime)
{
	RE_AddRefEntityToScene_plus(ent, intShaderTime);
}

/*
=====================
RE_AddDynamicLightToScene
=====================
*/
static void RE_AddDynamicLightToScene(const vec3_t org, float intensity, float r, float g, float b, int additive)
{
	dlight_t *dl;

	if (!tr.registered)
	{
		return;
	}
	if (r_numdlights >= ARRAY_LEN(backEndData->dlights))
	{
		return;
	}
	if (intensity <= 0)
	{
		return;
	}

#ifdef USE_PMLIGHT
#ifdef USE_LEGACY_DLIGHTS
	if (r_dlightMode->integer)
#endif
	{
		r *= r_dlightIntensity->value;
		g *= r_dlightIntensity->value;
		b *= r_dlightIntensity->value;
		intensity *= r_dlightScale->value;
	}
#endif

	if (r_dlightSaturation->value != 1.0)
	{
		float luminance = LUMA(r, g, b);
		r = LERP(luminance, r, r_dlightSaturation->value);
		g = LERP(luminance, g, r_dlightSaturation->value);
		b = LERP(luminance, b, r_dlightSaturation->value);
	}

	dl = &backEndData->dlights[r_numdlights++];
	VectorCopy(org, dl->origin);
	dl->radius = intensity;
	dl->color[0] = r;
	dl->color[1] = g;
	dl->color[2] = b;
	dl->additive = additive;
	dl->linear = false;
}

/*
=====================
RE_AddLinearLightToScene
=====================
*/
void RE_AddLinearLightToScene(const vec3_t start, const vec3_t end, float intensity, float r, float g, float b)
{
	RE_AddLinearLightToScene_plus(start, end, intensity, r, g, b);
}

/*
=====================
RE_AddLightToScene

=====================
*/
void RE_AddLightToScene(const vec3_t org, float intensity, float r, float g, float b)
{
	RE_AddDynamicLightToScene(org, intensity, r, g, b, false);
}

/*
=====================
RE_AddAdditiveLightToScene

=====================
*/
void RE_AddAdditiveLightToScene(const vec3_t org, float intensity, float r, float g, float b)
{
	RE_AddDynamicLightToScene(org, intensity, r, g, b, true);
}

void *R_GetCommandBuffer(int bytes);

/*
@@@@@@@@@@@@@@@@@@@@@
RE_RenderScene

Draw a 3D view into a part of the window, then return
to 2D drawing.

Rendering a scene may require multiple views to be rendered
to handle mirrors,
@@@@@@@@@@@@@@@@@@@@@
*/
void RE_RenderScene(const refdef_t *fd)
{
	RE_RenderScene_plus(fd);
}
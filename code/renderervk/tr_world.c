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

#ifdef USE_PMLIGHT
bool R_LightCullBounds(const dlight_t *dl, const vec3_t mins, const vec3_t maxs)
{
	return R_LightCullBounds_plus(dl, mins, maxs);
}

static bool R_LightCullFace(const srfSurfaceFace_t *face, const dlight_t *dl)
{
	float d = DotProduct(dl->transformed, face->plane.normal) - face->plane.dist;
	if (dl->linear)
	{
		float d2 = DotProduct(dl->transformed2, face->plane.normal) - face->plane.dist;
		if ((d < -dl->radius) && (d2 < -dl->radius))
			return true;
		if ((d > dl->radius) && (d2 > dl->radius))
			return true;
	}
	else
	{
		if ((d < -dl->radius) || (d > dl->radius))
			return true;
	}

	return false;
}

static bool R_LightCullSurface(const surfaceType_t *surface, const dlight_t *dl)
{
	switch (*surface)
	{
	case SF_FACE:
		return R_LightCullFace((const srfSurfaceFace_t *)surface, dl);
	case SF_GRID:
	{
		const srfGridMesh_t *grid = (const srfGridMesh_t *)surface;
		return R_LightCullBounds_plus(dl, grid->meshBounds[0], grid->meshBounds[1]);
	}
	case SF_TRIANGLES:
	{
		const srfTriangles_t *tris = (const srfTriangles_t *)surface;
		return R_LightCullBounds_plus(dl, tris->bounds[0], tris->bounds[1]);
	}
	default:
		return false;
	};
}
#endif // USE_PMLIGHT

/*
=============================================================

	BRUSH MODELS

=============================================================
*/

/*
=================
R_AddBrushModelSurfaces
=================
*/
void R_AddBrushModelSurfaces(trRefEntity_t *ent)
{
	R_AddBrushModelSurfaces_plus(ent);
}
/*
=============================================================

	WORLD MODEL

=============================================================
*/

/*
=================
R_inPVS
=================
*/
bool R_inPVS(const vec3_t p1, const vec3_t p2)
{
	return R_inPVS_plus(p1, p2);
}

/*
=============
R_AddWorldSurfaces
=============
*/
void R_AddWorldSurfaces(void)
{
	R_AddWorldSurfaces_plus();
}
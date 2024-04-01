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
// tr_flares.c

#include "vk_flares.hpp"

/*
=============================================================================

LIGHT FLARES

A light flare is an effect that takes place inside the eye when bright light
sources are visible.  The size of the flare relative to the screen is nearly
constant, irrespective of distance, but the intensity should be proportional to the
projected area of the light source.

A surface that has been flagged as having a light flare will calculate the depth
buffer value that its midpoint should have when the surface is added.

After all opaque surfaces have been rendered, the depth buffer is read back for
each flare in view.  If the point has not been obscured by a closer surface, the
flare should be drawn.

Surfaces that have a repeated texture should never be flagged as flaring, because
there will only be a single flare added at the midpoint of the polygon.

To prevent abrupt popping, the intensity of the flare is interpolated up and
down as it changes visibility.  This involves scene to scene state, unlike almost
all other aspects of the renderer, and is complicated by the fact that a single
frame may have multiple scenes.

RB_RenderFlares() will be called once per view (twice in a mirrored scene, potentially
up to five or more times in a frame with 3D status bar icons).

=============================================================================
*/

/*
==================
R_ClearFlares
==================
*/
void R_ClearFlares_plus(void)
{
	int i;

	if (!vk.fragmentStores)
		return;

	Com_Memset(r_flareStructs, 0, sizeof(r_flareStructs));
	r_activeFlares = NULL;
	r_inactiveFlares = NULL;

	for (i = 0; i < MAX_FLARES; i++)
	{
		r_flareStructs[i].next = r_inactiveFlares;
		r_inactiveFlares = &r_flareStructs[i];
	}
}

static flare_t *R_SearchFlare(void *surface)
{
	flare_t *f;

	// see if a flare with a matching surface, scene, and view exists
	for (f = r_activeFlares; f; f = f->next)
	{
		if (f->surface == surface && f->frameSceneNum == backEnd.viewParms.frameSceneNum && f->portalView == backEnd.viewParms.portalView)
		{
			return f;
		}
	}

	return NULL;
}

/*
==================
RB_AddFlare_plus

This is called at surface tesselation time
==================
*/
void RB_AddFlare_plus(void *surface, int fogNum, vec3_t point, vec3_t color, vec3_t normal)
{
	int i;
	flare_t *f;
	vec3_t local;
	float d = 1;
	vec4_t eye, clip, normalized, window;

	backEnd.pc.c_flareAdds++;

	if (normal && (normal[0] || normal[1] || normal[2]))
	{
		VectorSubtract(backEnd.viewParms.ort.origin, point, local);
		VectorNormalizeFast_plus(local);
		d = DotProduct(local, normal);
		// If the viewer is behind the flare don't add it.
		if (d < 0)
		{
			return;
		}
	}

	// if the point is off the screen, don't bother adding it
	// calculate screen coordinates and depth
	R_TransformModelToClip_plus(point, backEnd.ort.modelMatrix, backEnd.viewParms.projectionMatrix, eye, clip);

	// check to see if the point is completely off screen
	for (i = 0; i < 3; i++)
	{
		if (clip[i] >= clip[3] || clip[i] <= -clip[3])
		{
			return;
		}
	}

	R_TransformClipToWindow_plus(clip, &backEnd.viewParms, normalized, window);

	if (window[0] < 0 || window[0] >= backEnd.viewParms.viewportWidth || window[1] < 0 || window[1] >= backEnd.viewParms.viewportHeight)
	{
		return; // shouldn't happen, since we check the clip[] above, except for FP rounding
	}

	f = R_SearchFlare(surface);

	// allocate a new one
	if (!f)
	{
		if (!r_inactiveFlares)
		{
			// the list is completely full
			return;
		}
		f = r_inactiveFlares;
		r_inactiveFlares = r_inactiveFlares->next;
		f->next = r_activeFlares;
		r_activeFlares = f;

		f->surface = surface;
		f->frameSceneNum = backEnd.viewParms.frameSceneNum;
		f->portalView = backEnd.viewParms.portalView;
		f->visible = false;
		f->fadeTime = backEnd.refdef.time - 2000;
		f->testCount = 0;
	}
	else
	{
		++f->testCount;
	}

	f->addedFrame = backEnd.viewParms.frameCount;
	f->fogNum = fogNum;

	VectorCopy(point, f->origin);
	VectorCopy(color, f->color);

	// fade the intensity of the flare down as the
	// light surface turns away from the viewer
	VectorScale(f->color, d, f->color);

	// save info needed to test
	f->windowX = backEnd.viewParms.viewportX + window[0];
	f->windowY = backEnd.viewParms.viewportY + window[1];

	f->eyeZ = eye[2];

#ifdef USE_REVERSED_DEPTH
	f->drawZ = (clip[2] + 0.20) / clip[3];
#else
	f->drawZ = (clip[2] - 0.20) / clip[3];
#endif
}

/*
==================
RB_AddDlightFlares
==================
*/
void RB_AddDlightFlares_plus(void)
{
	dlight_t *l;
	int i, j, k;
	fog_t *fog = NULL;

	if (!r_flares->integer)
	{
		return;
	}

	l = backEnd.refdef.dlights;

	if (tr.world)
		fog = tr.world->fogs;

	for (i = 0; i < backEnd.refdef.num_dlights; i++, l++)
	{

		if (fog)
		{
			// find which fog volume the light is in
			for (j = 1; j < tr.world->numfogs; j++)
			{
				fog = &tr.world->fogs[j];
				for (k = 0; k < 3; k++)
				{
					if (l->origin[k] < fog->bounds[0][k] || l->origin[k] > fog->bounds[1][k])
					{
						break;
					}
				}
				if (k == 3)
				{
					break;
				}
			}
			if (j == tr.world->numfogs)
			{
				j = 0;
			}
		}
		else
			j = 0;

		RB_AddFlare_plus((void *)l, j, l->origin, l->color, NULL);
	}
}

/*
===============================================================================

FLARE BACK END

===============================================================================
*/

static float *vk_ortho(float x1, float x2,
					   float y2, float y1,
					   float z1, float z2)
{

	static float m[16] = {0};

	m[0] = 2.0f / (x2 - x1);
	m[5] = 2.0f / (y2 - y1);
	m[10] = 1.0f / (z1 - z2);
	m[12] = -(x2 + x1) / (x2 - x1);
	m[13] = -(y2 + y1) / (y2 - y1);
	m[14] = z1 / (z1 - z2);
	m[15] = 1.0f;

	return m;
}

/*
==================
RB_RenderFlare
==================
*/
static void RB_RenderFlare(flare_t *f)
{
	float size;
	vec3_t color;
	float distance, intensity, factor;
	byte fogFactors[3] = {255, 255, 255};
	color4ub_t c;

	// if ( f->drawIntensity == 0.0 )
	//	return;

	backEnd.pc.c_flareRenders++;

	// We don't want too big values anyways when dividing by distance.
	if (f->eyeZ > -1.0f)
		distance = 1.0f;
	else
		distance = -f->eyeZ;

	// calculate the flare size..
	size = backEnd.viewParms.viewportWidth * (r_flareSize->value / 640.0f + 8 / distance);

	/*
	 * This is an alternative to intensity scaling. It changes the size of the flare on screen instead
	 * with growing distance. See in the description at the top why this is not the way to go.
		// size will change ~ 1/r.
		size = backEnd.viewParms.viewportWidth * (r_flareSize->value / (distance * -2.0f));
	*/

	/*
	 * As flare sizes stay nearly constant with increasing distance we must decrease the intensity
	 * to achieve a reasonable visual result. The intensity is ~ (size^2 / distance^2) which can be
	 * got by considering the ratio of
	 * (flaresurface on screen) : (Surface of sphere defined by flare origin and distance from flare)
	 * An important requirement is:
	 * intensity <= 1 for all distances.
	 *
	 * The formula used here to compute the intensity is as follows:
	 * intensity = flareCoeff * size^2 / (distance + size*sqrt(flareCoeff))^2
	 * As you can see, the intensity will have a max. of 1 when the distance is 0.
	 * The coefficient flareCoeff will determine the falloff speed with increasing distance.
	 */

	factor = distance + size * sqrt(r_flareCoeff->value);

	intensity = r_flareCoeff->value * size * size / (factor * factor);

	VectorScale(f->color, f->drawIntensity * intensity, color);

	// Calculations for fogging
	if (tr.world && f->fogNum > 0 && f->fogNum < tr.world->numfogs)
	{
		tess.numVertexes = 1;
		VectorCopy(f->origin, tess.xyz[0]);
		tess.fogNum = f->fogNum;

		RB_CalcModulateColorsByFog(fogFactors);

		// We don't need to render the flare if colors are 0 anyways.
		if (!(fogFactors[0] || fogFactors[1] || fogFactors[2]))
			return;
	}

	RB_BeginSurface(tr.flareShader, f->fogNum);

	c.rgba[0] = color[0] * fogFactors[0];
	c.rgba[1] = color[1] * fogFactors[1];
	c.rgba[2] = color[2] * fogFactors[2];
	c.rgba[3] = 255;

	RB_AddQuadStamp2(f->windowX - size, f->windowY - size, size * 2, size * 2, 0, 0, 1, 1, c);

	RB_EndSurface();
}

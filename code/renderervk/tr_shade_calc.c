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
// tr_shade_calc.c

#include "tr_local.h"
// -EC-: avoid using ri.ftol
#define WAVEVALUE(table, base, amplitude, phase, freq) ((base) + table[(int64_t)((((phase) + tess.shaderTime * (freq)) * FUNCTABLE_SIZE)) & FUNCTABLE_MASK] * (amplitude))

/*
** RB_CalcStretchTexCoords
*/
void RB_CalcStretchTexCoords(const waveForm_t *wf, float *src, float *dst)
{
	RB_CalcStretchTexCoords_plus(wf, src, dst);
}

/*
====================================================================

DEFORMATIONS

====================================================================
*/

/*
=====================
RB_DeformTessGeometry

=====================
*/
void RB_DeformTessGeometry(void)
{
	RB_DeformTessGeometry_plus();
}

/*
====================================================================

COLORS

====================================================================
*/

/*
** RB_CalcColorFromEntity
*/
void RB_CalcColorFromEntity(unsigned char *dstColors)
{
	RB_CalcColorFromEntity_plus(dstColors);
}

/*
** RB_CalcColorFromOneMinusEntity
*/
void RB_CalcColorFromOneMinusEntity(unsigned char *dstColors)
{
	RB_CalcColorFromOneMinusEntity_plus(dstColors);
}

/*
** RB_CalcAlphaFromEntity
*/
void RB_CalcAlphaFromEntity(unsigned char *dstColors)
{
	RB_CalcAlphaFromEntity_plus(dstColors);
}

/*
** RB_CalcAlphaFromOneMinusEntity
*/
void RB_CalcAlphaFromOneMinusEntity(unsigned char *dstColors)
{
	RB_CalcAlphaFromOneMinusEntity_plus(dstColors);
}

/*
** RB_CalcWaveColor
*/
void RB_CalcWaveColor(const waveForm_t *wf, unsigned char *dstColors)
{
	RB_CalcWaveColor_plus(wf, dstColors);
}

/*
** RB_CalcWaveAlpha
*/
void RB_CalcWaveAlpha(const waveForm_t *wf, unsigned char *dstColors)
{
	RB_CalcWaveAlpha_plus(wf, dstColors);
}

/*
** RB_CalcModulateColorsByFog
*/
void RB_CalcModulateColorsByFog(unsigned char *colors)
{
	RB_CalcModulateColorsByFog_plus(colors);
}

/*
** RB_CalcModulateAlphasByFog
*/
void RB_CalcModulateAlphasByFog(unsigned char *colors)
{
	RB_CalcModulateAlphasByFog_plus(colors);
}

/*
** RB_CalcModulateRGBAsByFog
*/
void RB_CalcModulateRGBAsByFog(unsigned char *colors)
{
	RB_CalcModulateRGBAsByFog_plus(colors);
}

/*
====================================================================

TEX COORDS

====================================================================
*/

/*
========================
RB_CalcFogTexCoords

To do the clipped fog plane really correctly, we should use
projected textures, but I don't trust the drivers and it
doesn't fit our shader data.
========================
*/
void RB_CalcFogTexCoords(float *st)
{
	RB_CalcFogTexCoords_plus(st);
}

/*
========================
RB_CalcFogProgramParms
========================
*/
const fogProgramParms_t *RB_CalcFogProgramParms(void)
{
	return RB_CalcFogProgramParms_plus();
}

/*
========================
RB_CalcEnvironmentTexCoordsFP

Special version for first-person models, borrowed from OpenArena
========================
*/
void RB_CalcEnvironmentTexCoordsFP(float *st, int screenMap)
{
	RB_CalcEnvironmentTexCoordsFP_plus(st, screenMap);
}

/*
** RB_CalcEnvironmentTexCoords
*/
void RB_CalcEnvironmentTexCoords(float *st)
{
	RB_CalcEnvironmentTexCoords_plus(st);
}

/*
** RB_CalcTurbulentTexCoords
*/
void RB_CalcTurbulentTexCoords(const waveForm_t *wf, float *src, float *dst)
{
	RB_CalcTurbulentTexCoords_plus(wf, src, dst);
}

/*
** RB_CalcScaleTexCoords
*/
void RB_CalcScaleTexCoords(const float scale[2], float *src, float *dst)
{
	RB_CalcScaleTexCoords_plus(scale, src, dst);
}

/*
** RB_CalcScrollTexCoords
*/
void RB_CalcScrollTexCoords(const float scrollSpeed[2], float *src, float *dst)
{
	RB_CalcScrollTexCoords_plus(scrollSpeed, src, dst);
}
/*
** RB_CalcTransformTexCoords
*/
void RB_CalcTransformTexCoords(const texModInfo_t *tmi, float *src, float *dst)
{
	RB_CalcTransformTexCoords_plus(tmi, src, dst);
}
/*
** RB_CalcRotateTexCoords
*/
void RB_CalcRotateTexCoords(float degsPerSecond, float *src, float *dst)
{
	RB_CalcRotateTexCoords_plus(degsPerSecond, src, dst);
}

void RB_CalcSpecularAlpha(unsigned char *alphas)
{
	RB_CalcSpecularAlpha_plus(alphas);
}

void RB_CalcDiffuseColor(unsigned char *colors)
{
	RB_CalcDiffuseColor_plus(colors);
	// RB_CalcDiffuseColor_scalar( colors );
}

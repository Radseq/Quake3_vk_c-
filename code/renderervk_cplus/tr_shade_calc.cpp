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
// tr_shade_calc.c

#include "tr_shade_calc.hpp"
#include "tr_noise_cplus.hpp"

// -EC-: avoid using ri.ftol
#define WAVEVALUE(table, base, amplitude, phase, freq) ((base) + table[(int64_t)((((phase) + tess.shaderTime * (freq)) * FUNCTABLE_SIZE)) & FUNCTABLE_MASK] * (amplitude))

static float *TableForFunc(genFunc_t func)
{
	switch (func)
	{
	case GF_SIN:
		return tr.sinTable;
	case GF_TRIANGLE:
		return tr.triangleTable;
	case GF_SQUARE:
		return tr.squareTable;
	case GF_SAWTOOTH:
		return tr.sawToothTable;
	case GF_INVERSE_SAWTOOTH:
		return tr.inverseSawToothTable;
	case GF_NONE:
	default:
		break;
	}

	ri.Error(ERR_DROP, "TableForFunc called with invalid function '%d' in shader '%s'", func, tess.shader->name);
	return NULL;
}

static float EvalWaveForm(const waveForm_t *wf)
{
	float *table;

	table = TableForFunc(wf->func);

	return WAVEVALUE(table, wf->base, wf->amplitude, wf->phase, wf->frequency);
}

void RB_CalcTransformTexCoords_plus(const texModInfo_t *tmi, float *src, float *dst)
{
	int i;

	for (i = 0; i < tess.numVertexes; i++, dst += 2, src += 2)
	{
		const float s = src[0];
		const float t = src[1];

		dst[0] = s * tmi->matrix[0][0] + t * tmi->matrix[1][0] + tmi->translate[0];
		dst[1] = s * tmi->matrix[0][1] + t * tmi->matrix[1][1] + tmi->translate[1];
	}
}

void RB_CalcStretchTexCoords_plus(const waveForm_t *wf, float *src, float *dst)
{
	float p;
	texModInfo_t tmi;

	p = 1.0f / EvalWaveForm(wf);

	tmi.matrix[0][0] = p;
	tmi.matrix[1][0] = 0;
	tmi.translate[0] = 0.5f - 0.5f * p;

	tmi.matrix[0][1] = 0;
	tmi.matrix[1][1] = p;
	tmi.translate[1] = 0.5f - 0.5f * p;

	RB_CalcTransformTexCoords_plus(&tmi, src, dst);
}

void RB_CalcRotateTexCoords_plus(float degsPerSecond, float *src, float *dst)
{
	double timeScale = tess.shaderTime; // -EC- set to double
	double degs;						// -EC- set to double
	int64_t index;
	float sinValue, cosValue;
	texModInfo_t tmi;

	degs = -degsPerSecond * timeScale;
	index = degs * (FUNCTABLE_SIZE / 360.0f);

	sinValue = tr.sinTable[index & FUNCTABLE_MASK];
	cosValue = tr.sinTable[(index + FUNCTABLE_SIZE / 4) & FUNCTABLE_MASK];

	tmi.matrix[0][0] = cosValue;
	tmi.matrix[1][0] = -sinValue;
	tmi.translate[0] = 0.5 - 0.5 * cosValue + 0.5 * sinValue;

	tmi.matrix[0][1] = sinValue;
	tmi.matrix[1][1] = cosValue;
	tmi.translate[1] = 0.5 - 0.5 * sinValue - 0.5 * cosValue;

	RB_CalcTransformTexCoords_plus(&tmi, src, dst);
}

void RB_CalcScrollTexCoords_plus(const float scrollSpeed[2], float *src, float *dst)
{
	int i;
	double timeScale;						 // -EC-: set to double
	double adjustedScrollS, adjustedScrollT; // -EC-: set to double

	timeScale = tess.shaderTime;

	adjustedScrollS = (double)scrollSpeed[0] * timeScale;
	adjustedScrollT = (double)scrollSpeed[1] * timeScale;

	// clamp so coordinates don't continuously get larger, causing problems
	// with hardware limits
	adjustedScrollS = adjustedScrollS - floor(adjustedScrollS);
	adjustedScrollT = adjustedScrollT - floor(adjustedScrollT);

	for (i = 0; i < tess.numVertexes; i++, dst += 2, src += 2)
	{
		dst[0] = src[0] + adjustedScrollS;
		dst[1] = src[1] + adjustedScrollT;
	}
}

void RB_CalcScaleTexCoords_plus(const float scale[2], float *src, float *dst)
{
	int i;

	for (i = 0; i < tess.numVertexes; i++, dst += 2, src += 2)
	{
		dst[0] = src[0] * scale[0];
		dst[1] = src[1] * scale[1];
	}
}

void RB_CalcTurbulentTexCoords_plus(const waveForm_t *wf, float *src, float *dst)
{
	int i;
	double now; // -EC- set to double

	now = (wf->phase + tess.shaderTime * wf->frequency);

	for (i = 0; i < tess.numVertexes; i++, dst += 2, src += 2)
	{
		dst[0] = src[0] + tr.sinTable[((int64_t)(((tess.xyz[i][0] + tess.xyz[i][2]) * 1.0 / 128 * 0.125 + now) * FUNCTABLE_SIZE)) & (FUNCTABLE_MASK)] * wf->amplitude;
		dst[1] = src[1] + tr.sinTable[((int64_t)((tess.xyz[i][1] * 1.0 / 128 * 0.125 + now) * FUNCTABLE_SIZE)) & (FUNCTABLE_MASK)] * wf->amplitude;
	}
}

/*
========================
RB_CalcFogTexCoords

To do the clipped fog plane really correctly, we should use
projected textures, but I don't trust the drivers and it
doesn't fit our shader data.
========================
*/
void RB_CalcFogTexCoords_plus(float *st)
{
	int i;
	float *v;
	float s, t;
	float eyeT;
	bool eyeOutside;
	const fog_t *fog;
	vec3_t local;
	vec4_t fogDistanceVector, fogDepthVector = {0, 0, 0, 0};

	fog = tr.world->fogs + tess.fogNum;

	// all fogging distance is based on world Z units
	VectorSubtract(backEnd.ort.origin, backEnd.viewParms.ort.origin, local);
	fogDistanceVector[0] = -backEnd.ort.modelMatrix[2];
	fogDistanceVector[1] = -backEnd.ort.modelMatrix[6];
	fogDistanceVector[2] = -backEnd.ort.modelMatrix[10];
	fogDistanceVector[3] = DotProduct(local, backEnd.viewParms.ort.axis[0]);

	// scale the fog vectors based on the fog's thickness
	fogDistanceVector[0] *= fog->tcScale;
	fogDistanceVector[1] *= fog->tcScale;
	fogDistanceVector[2] *= fog->tcScale;
	fogDistanceVector[3] *= fog->tcScale;

	// rotate the gradient vector for this orientation
	if (fog->hasSurface)
	{
		fogDepthVector[0] = fog->surface[0] * backEnd.ort.axis[0][0] +
							fog->surface[1] * backEnd.ort.axis[0][1] + fog->surface[2] * backEnd.ort.axis[0][2];
		fogDepthVector[1] = fog->surface[0] * backEnd.ort.axis[1][0] +
							fog->surface[1] * backEnd.ort.axis[1][1] + fog->surface[2] * backEnd.ort.axis[1][2];
		fogDepthVector[2] = fog->surface[0] * backEnd.ort.axis[2][0] +
							fog->surface[1] * backEnd.ort.axis[2][1] + fog->surface[2] * backEnd.ort.axis[2][2];
		fogDepthVector[3] = -fog->surface[3] + DotProduct(backEnd.ort.origin, fog->surface);

		eyeT = DotProduct(backEnd.ort.viewOrigin, fogDepthVector) + fogDepthVector[3];
	}
	else
	{
		eyeT = 1; // non-surface fog always has eye inside
	}

	// see if the viewpoint is outside
	// this is needed for clipping distance even for constant fog

	if (eyeT < 0)
	{
		eyeOutside = true;
	}
	else
	{
		eyeOutside = false;
	}

	fogDistanceVector[3] += 1.0 / 512;

	// calculate density for each point
	for (i = 0, v = tess.xyz[0]; i < tess.numVertexes; i++, v += 4)
	{
		// calculate the length in fog
		s = DotProduct(v, fogDistanceVector) + fogDistanceVector[3];
		t = DotProduct(v, fogDepthVector) + fogDepthVector[3];

		// partially clipped fogs use the T axis
		if (eyeOutside)
		{
			if (t < 1.0)
			{
				t = 1.0 / 32; // point is outside, so no fogging
			}
			else
			{
				t = 1.0 / 32 + 30.0 / 32 * t / (t - eyeT); // cut the distance at the fog plane
			}
		}
		else
		{
			if (t < 0)
			{
				t = 1.0 / 32; // point is outside, so no fogging
			}
			else
			{
				t = 31.0 / 32;
			}
		}

		st[0] = s;
		st[1] = t;
		st += 2;
	}
}

void RB_CalcEnvironmentTexCoords_plus(float *st)
{
	int i;
	const float *v, *normal;
	vec3_t viewer, reflected;
	float d;

	v = tess.xyz[0];
	normal = tess.normal[0];

	for (i = 0; i < tess.numVertexes; i++, v += 4, normal += 4, st += 2)
	{
		VectorSubtract(backEnd.ort.viewOrigin, v, viewer);
		VectorNormalizeFast_plus(viewer);

		d = DotProduct(normal, viewer);

		// reflected[0] = normal[0]*2*d - viewer[0];
		reflected[1] = normal[1] * 2 * d - viewer[1];
		reflected[2] = normal[2] * 2 * d - viewer[2];

		st[0] = 0.5 + reflected[1] * 0.5;
		st[1] = 0.5 - reflected[2] * 0.5;
	}
}

static void RB_CalcEnvironmentTexCoordsFPscr(float *st)
{
	int i;
	const float *v, *normal;
	vec3_t viewer, reflected;
	float d;

	v = tess.xyz[0];
	normal = tess.normal[0];

	for (i = 0; i < tess.numVertexes; i++, v += 4, normal += 4, st += 2)
	{
		VectorSubtract(backEnd.ort.viewOrigin, v, viewer);
		VectorNormalizeFast_plus(viewer);

		d = DotProduct(normal, viewer);

		reflected[1] = normal[1] * 2 * d - viewer[1];
		reflected[2] = normal[2] * 2 * d - viewer[2];

		st[0] = 0.5 - reflected[1] * 0.5;
		st[1] = 0.5 + reflected[2] * 0.5;
	}
}

/*
========================
RB_CalcEnvironmentTexCoordsFP

Special version for first-person models, borrowed from OpenArena
========================
*/
void RB_CalcEnvironmentTexCoordsFP_plus(float *st, int screenMap)
{
	int i;
	const float *v, *normal;
	vec3_t viewer, reflected, where, why, who; // what
	float d;

	if (!backEnd.currentEntity || (backEnd.currentEntity->e.renderfx & RF_FIRST_PERSON) == 0)
	{
		RB_CalcEnvironmentTexCoords_plus(st);
		return;
	}

	if (screenMap && backEnd.viewParms.frameSceneNum == 1)
	{
		RB_CalcEnvironmentTexCoordsFPscr(st);
		return;
	}

	v = tess.xyz[0];
	normal = tess.normal[0];

	for (i = 0; i < tess.numVertexes; i++, v += 4, normal += 4, st += 2)
	{
		// VectorSubtract( backEnd.or.axis[0], v, what );
		VectorSubtract(backEnd.ort.axis[1], v, why);
		VectorSubtract(backEnd.ort.axis[2], v, who);

		VectorSubtract(backEnd.ort.origin, v, where);
		VectorSubtract(backEnd.ort.viewOrigin, v, viewer);

		VectorNormalizeFast_plus(viewer);
		VectorNormalizeFast_plus(where);
		// VectorNormalizeFast( what );
		VectorNormalizeFast_plus(why);
		VectorNormalizeFast_plus(who);

		d = DotProduct(normal, viewer);

		// reflected[0] = normal[0]*2*d - viewer[0] - (where[0] * 5) + (what[0] * 4);
		reflected[1] = normal[1] * 2 * d - viewer[1] - (where[1] * 5) + (why[1] * 4);
		reflected[2] = normal[2] * 2 * d - viewer[2] - (where[2] * 5) + (who[2] * 4);

		st[0] = 0.33 + reflected[1] * 0.33;
		st[1] = 0.33 - reflected[2] * 0.33;
	}
}

const fogProgramParms_t *RB_CalcFogProgramParms_plus()
{
	static fogProgramParms_t parm;
	const fog_t *fog;
	vec3_t local;

	Com_Memset(parm.fogDepthVector, 0, sizeof(parm.fogDepthVector));

	fog = tr.world->fogs + tess.fogNum;

	// all fogging distance is based on world Z units
	VectorSubtract(backEnd.ort.origin, backEnd.viewParms.ort.origin, local);
	parm.fogDistanceVector[0] = -backEnd.ort.modelMatrix[2];
	parm.fogDistanceVector[1] = -backEnd.ort.modelMatrix[6];
	parm.fogDistanceVector[2] = -backEnd.ort.modelMatrix[10];
	parm.fogDistanceVector[3] = DotProduct(local, backEnd.viewParms.ort.axis[0]);

	// scale the fog vectors based on the fog's thickness
	parm.fogDistanceVector[0] *= fog->tcScale;
	parm.fogDistanceVector[1] *= fog->tcScale;
	parm.fogDistanceVector[2] *= fog->tcScale;
	parm.fogDistanceVector[3] *= fog->tcScale;

	// rotate the gradient vector for this orientation
	if (fog->hasSurface)
	{
		parm.fogDepthVector[0] = fog->surface[0] * backEnd.ort.axis[0][0] +
								 fog->surface[1] * backEnd.ort.axis[0][1] + fog->surface[2] * backEnd.ort.axis[0][2];
		parm.fogDepthVector[1] = fog->surface[0] * backEnd.ort.axis[1][0] +
								 fog->surface[1] * backEnd.ort.axis[1][1] + fog->surface[2] * backEnd.ort.axis[1][2];
		parm.fogDepthVector[2] = fog->surface[0] * backEnd.ort.axis[2][0] +
								 fog->surface[1] * backEnd.ort.axis[2][1] + fog->surface[2] * backEnd.ort.axis[2][2];
		parm.fogDepthVector[3] = -fog->surface[3] + DotProduct(backEnd.ort.origin, fog->surface);

		parm.eyeT = DotProduct(backEnd.ort.viewOrigin, parm.fogDepthVector) + parm.fogDepthVector[3];
	}
	else
	{
		parm.eyeT = 1.0f; // non-surface fog always has eye inside
	}

	// see if the viewpoint is outside
	// this is needed for clipping distance even for constant fog

	if (parm.eyeT < 0)
	{
		parm.eyeOutside = true;
	}
	else
	{
		parm.eyeOutside = false;
	}

	parm.fogDistanceVector[3] += 1.0 / 512;
	parm.fogColor = fog->color;

	return &parm;
}

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
// tr_shade.c

#include "tr_shade.hpp"
#include "tr_shade_calc.hpp"
// #include "tr_shade_calc.hpp"

/*

  THIS ENTIRE FILE IS BACK END

  This file deals with applying shaders to surface data in the tess struct.
*/

/*
=============================================================

SURFACE SHADERS

=============================================================
*/
/*
==============
RB_BeginSurface

We must set some things up before beginning any tesselation,
because a surface may be forced to perform a RB_End due
to overflow.
==============
*/
void RB_BeginSurface_plus(shader_t *shader, int fogNum)
{

	shader_t *state;

#ifdef USE_VBO
	if (shader->isStaticShader && !shader->remappedShader)
	{
		tess.allowVBO = true;
	}
	else
	{
		tess.allowVBO = false;
	}
#endif

	if (shader->remappedShader)
	{
		state = shader->remappedShader;
	}
	else
	{
		state = shader;
	}

#ifdef USE_PMLIGHT
	if (tess.fogNum != fogNum)
	{
		tess.dlightUpdateParams = true;
	}
#endif

#ifdef USE_TESS_NEEDS_NORMAL
#ifdef USE_PMLIGHT
	tess.needsNormal = state->needsNormal || tess.dlightPass || r_shownormals->integer;
#else
	tess.needsNormal = state->needsNormal || r_shownormals->integer;
#endif
#endif

#ifdef USE_TESS_NEEDS_ST2
	tess.needsST2 = state->needsST2;
#endif

	tess.numIndexes = 0;
	tess.numVertexes = 0;
	tess.shader = state;
	tess.fogNum = fogNum;

#ifdef USE_LEGACY_DLIGHTS
	tess.dlightBits = 0; // will be OR'd in by surface functions
#endif
	tess.xstages = state->stages;
	tess.numPasses = state->numUnfoggedPasses;

	tess.shaderTime = backEnd.refdef.floatTime - tess.shader->timeOffset;
	if (tess.shader->clampTime && tess.shaderTime >= tess.shader->clampTime)
	{
		tess.shaderTime = tess.shader->clampTime;
	}
}

void R_ComputeTexCoords_plus(const int b, const textureBundle_t *bundle)
{
	int i;
	int tm;
	vec2_t *src, *dst;

	if (!tess.numVertexes)
		return;

	src = dst = tess.svars.texcoords[b];

	//
	// generate the texture coordinates
	//
	switch (bundle->tcGen)
	{
	case TCGEN_IDENTITY:
		src = tess.texCoords00;
		break;
	case TCGEN_TEXTURE:
		src = tess.texCoords[0];
		break;
	case TCGEN_LIGHTMAP:
		src = tess.texCoords[1];
		break;
	case TCGEN_VECTOR:
		for (i = 0; i < tess.numVertexes; i++)
		{
			dst[i][0] = DotProduct(tess.xyz[i], bundle->tcGenVectors[0]);
			dst[i][1] = DotProduct(tess.xyz[i], bundle->tcGenVectors[1]);
		}
		break;
	case TCGEN_FOG:
		RB_CalcFogTexCoords_plus((float *)dst);
		break;
	case TCGEN_ENVIRONMENT_MAPPED:
		RB_CalcEnvironmentTexCoords_plus((float *)dst);
		break;
	case TCGEN_ENVIRONMENT_MAPPED_FP:
		RB_CalcEnvironmentTexCoordsFP_plus((float *)dst, bundle->isScreenMap);
		break;
	case TCGEN_BAD:
		return;
	}

	//
	// alter texture coordinates
	//
	for (tm = 0; tm < bundle->numTexMods; tm++)
	{
		switch (bundle->texMods[tm].type)
		{
		case TMOD_NONE:
			tm = TR_MAX_TEXMODS; // break out of for loop
			break;

		case TMOD_TURBULENT:
			RB_CalcTurbulentTexCoords_plus(&bundle->texMods[tm].wave, (float *)src, (float *)dst);
			src = dst;
			break;

		case TMOD_ENTITY_TRANSLATE:
			RB_CalcScrollTexCoords_plus(backEnd.currentEntity->e.shaderTexCoord, (float *)src, (float *)dst);
			src = dst;
			break;

		case TMOD_SCROLL:
			RB_CalcScrollTexCoords_plus(bundle->texMods[tm].scroll, (float *)src, (float *)dst);
			src = dst;
			break;

		case TMOD_SCALE:
			RB_CalcScaleTexCoords_plus(bundle->texMods[tm].scale, (float *)src, (float *)dst);
			src = dst;
			break;

		case TMOD_OFFSET:
			for (i = 0; i < tess.numVertexes; i++)
			{
				dst[i][0] = src[i][0] + bundle->texMods[tm].offset[0];
				dst[i][1] = src[i][1] + bundle->texMods[tm].offset[1];
			}
			src = dst;
			break;

		case TMOD_SCALE_OFFSET:
			for (i = 0; i < tess.numVertexes; i++)
			{
				dst[i][0] = (src[i][0] * bundle->texMods[tm].scale[0]) + bundle->texMods[tm].offset[0];
				dst[i][1] = (src[i][1] * bundle->texMods[tm].scale[1]) + bundle->texMods[tm].offset[1];
			}
			src = dst;
			break;

		case TMOD_OFFSET_SCALE:
			for (i = 0; i < tess.numVertexes; i++)
			{
				dst[i][0] = (src[i][0] + bundle->texMods[tm].offset[0]) * bundle->texMods[tm].scale[0];
				dst[i][1] = (src[i][1] + bundle->texMods[tm].offset[1]) * bundle->texMods[tm].scale[1];
			}
			src = dst;
			break;

		case TMOD_STRETCH:
			RB_CalcStretchTexCoords_plus(&bundle->texMods[tm].wave, (float *)src, (float *)dst);
			src = dst;
			break;

		case TMOD_TRANSFORM:
			RB_CalcTransformTexCoords_plus(&bundle->texMods[tm], (float *)src, (float *)dst);
			src = dst;
			break;

		case TMOD_ROTATE:
			RB_CalcRotateTexCoords_plus(bundle->texMods[tm].rotateSpeed, (float *)src, (float *)dst);
			src = dst;
			break;

		default:
			ri.Error(ERR_DROP, "ERROR: unknown texmod '%d' in shader '%s'", bundle->texMods[tm].type, tess.shader->name);
			break;
		}
	}

	tess.svars.texcoordPtr[b] = src;
}

void VK_SetFogParams_plus( vkUniform_t *uniform, int *fogStage )
{
	if ( tess.fogNum && tess.shader->fogPass ) {
		const fogProgramParms_t *fp = RB_CalcFogProgramParms_plus();
		// vertex data
		Vector4Copy( fp->fogDistanceVector, uniform->fogDistanceVector );
		Vector4Copy( fp->fogDepthVector, uniform->fogDepthVector );
		uniform->fogEyeT[0] = fp->eyeT;
		if ( fp->eyeOutside ) {
			uniform->fogEyeT[1] = 0.0; // fog eye out
		} else {
			uniform->fogEyeT[1] = 1.0; // fog eye in
		}
		// fragment data
		Vector4Copy( fp->fogColor, uniform->fogColor );
		*fogStage = 1;
	} else {
		*fogStage = 0;
	}
}


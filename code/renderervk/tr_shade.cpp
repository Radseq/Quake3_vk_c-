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
#include "tr_backend.hpp"
#include "tr_shadows.hpp"
#include "vk_vbo.hpp"
#include "vk.hpp"
#include "math.hpp"
#include "vk_descriptors.hpp"
#include "vk_pipeline.hpp"
#include "utils.hpp"

#if defined(USE_AoS_to_SoA_SIMD)
#include "tr_soa_frame.hpp"
#endif

shaderCommands_t tess;

static vkUniform_t uniform;

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
void RB_BeginSurface(shader_t &shader, const int fogNum)
{
	shader_t *state;

#ifdef USE_VBO
	if (shader.isStaticShader && !shader.remappedShader)
	{
		tess.allowVBO = true;
	}
	else
	{
		tess.allowVBO = false;
	}
#endif

	if (shader.remappedShader)
	{
		state = shader.remappedShader;
	}
	else
	{
		state = &shader;
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

void R_ComputeTexCoords(const int b, const textureBundle_t &bundle)
{
	if (!tess.numVertexes)
		return;

	int i;
	int tm;
	vec2_t *src, *dst;

	src = dst = tess.svars.texcoords[b];

	//
	// generate the texture coordinates
	//
	switch (bundle.tcGen)
	{
	case texCoordGen_t::TCGEN_IDENTITY:
		src = tess.texCoords00;
		break;
	case texCoordGen_t::TCGEN_TEXTURE:
		src = tess.texCoords[0];
		break;
	case texCoordGen_t::TCGEN_LIGHTMAP:
		src = tess.texCoords[1];
		break;
	case texCoordGen_t::TCGEN_VECTOR:
		for (i = 0; i < tess.numVertexes; i++)
		{
			dst[i][0] = DotProduct(tess.xyz[i], bundle.tcGenVectors[0]);
			dst[i][1] = DotProduct(tess.xyz[i], bundle.tcGenVectors[1]);
		}
		break;
	case texCoordGen_t::TCGEN_FOG:
		RB_CalcFogTexCoords((float *)dst);
		break;
	case texCoordGen_t::TCGEN_ENVIRONMENT_MAPPED:
		RB_CalcEnvironmentTexCoords((float *)dst);
		break;
	case texCoordGen_t::TCGEN_ENVIRONMENT_MAPPED_FP:
		RB_CalcEnvironmentTexCoordsFP((float *)dst, bundle.isScreenMap);
		break;
	case texCoordGen_t::TCGEN_BAD:
		return;
	}

	//
	// alter texture coordinates
	//
	for (tm = 0; tm < bundle.numTexMods; tm++)
	{
		switch (bundle.texMods[tm].type)
		{
		case texMod_t::TMOD_NONE:
			tm = TR_MAX_TEXMODS; // break out of for loop
			break;

		case texMod_t::TMOD_TURBULENT:
			RB_CalcTurbulentTexCoords(bundle.texMods[tm].wave, (float *)src, (float *)dst);
			src = dst;
			break;

		case texMod_t::TMOD_ENTITY_TRANSLATE:
#if defined(USE_AoS_to_SoA_SIMD)
		{
			const int slot = backEnd.currentModelSlot;
			auto& soa = trsoa::GetFrameSoA();
			if (slot >= 0 && trsoa::SoA_ValidThisFrame(soa))
			{
				const float st[2]{ soa.models.shader.shaderTC0[slot], soa.models.shader.shaderTC1[slot] };
				RB_CalcScrollTexCoords(st, (float*)src, (float*)dst);
				src = dst;
				break;
			}
		}
#endif
			RB_CalcScrollTexCoords(backEnd.currentEntity->e.shaderTexCoord, (float*)src, (float*)dst);
			src = dst;
			break;


		case texMod_t::TMOD_SCROLL:
			RB_CalcScrollTexCoords(bundle.texMods[tm].scroll, (float *)src, (float *)dst);
			src = dst;
			break;

		case texMod_t::TMOD_SCALE:
			RB_CalcScaleTexCoords(bundle.texMods[tm].scale, (float *)src, (float *)dst);
			src = dst;
			break;

		case texMod_t::TMOD_OFFSET:
			for (i = 0; i < tess.numVertexes; i++)
			{
				dst[i][0] = src[i][0] + bundle.texMods[tm].offset[0];
				dst[i][1] = src[i][1] + bundle.texMods[tm].offset[1];
			}
			src = dst;
			break;

		case texMod_t::TMOD_SCALE_OFFSET:
			for (i = 0; i < tess.numVertexes; i++)
			{
				dst[i][0] = (src[i][0] * bundle.texMods[tm].scale[0]) + bundle.texMods[tm].offset[0];
				dst[i][1] = (src[i][1] * bundle.texMods[tm].scale[1]) + bundle.texMods[tm].offset[1];
			}
			src = dst;
			break;

		case texMod_t::TMOD_OFFSET_SCALE:
			for (i = 0; i < tess.numVertexes; i++)
			{
				dst[i][0] = (src[i][0] + bundle.texMods[tm].offset[0]) * bundle.texMods[tm].scale[0];
				dst[i][1] = (src[i][1] + bundle.texMods[tm].offset[1]) * bundle.texMods[tm].scale[1];
			}
			src = dst;
			break;

		case texMod_t::TMOD_STRETCH:
			RB_CalcStretchTexCoords(bundle.texMods[tm].wave, (float *)src, (float *)dst);
			src = dst;
			break;

		case texMod_t::TMOD_TRANSFORM:
			RB_CalcTransformTexCoords(bundle.texMods[tm], (float *)src, (float *)dst);
			src = dst;
			break;

		case texMod_t::TMOD_ROTATE:
			RB_CalcRotateTexCoords(bundle.texMods[tm].rotateSpeed, (float *)src, (float *)dst);
			src = dst;
			break;

		default:
			ri.Error(ERR_DROP, "ERROR: unknown texmod '%d' in shader '%s'", static_cast<int>(bundle.texMods[tm].type), tess.shader->name);
			break;
		}
	}

	tess.svars.texcoordPtr[b] = src;
}

void VK_SetFogParams(vkUniform_t &uniform, int &fogStage)
{
	if (tess.fogNum && static_cast<int>(tess.shader->fogPass))
	{
		fogProgramParms_t fp = {};
		RB_CalcFogProgramParms(fp);
		// vertex data
		Vector4Copy(fp.fogDistanceVector, uniform.fogDistanceVector);
		Vector4Copy(fp.fogDepthVector, uniform.fogDepthVector);
		uniform.fogEyeT[0] = fp.eyeT;
		if (fp.eyeOutside)
		{
			uniform.fogEyeT[1] = 0.0; // fog eye out
		}
		else
		{
			uniform.fogEyeT[1] = 1.0; // fog eye in
		}
		// fragment data
		Vector4Copy(fp.fogColor, uniform.fogColor);
		fogStage = 1;
	}
	else
	{
		fogStage = 0;
	}
}

void R_ComputeColors(const int b, color4ub_t *dest, const shaderStage_t &pStage)
{
	if (tess.numVertexes == 0)
		return;

	int i;

	//
	// rgbGen
	//
	switch (pStage.bundle[b].rgbGen)
	{
	case colorGen_t::CGEN_IDENTITY:
		Com_Memset(dest, 0xff, tess.numVertexes * 4);
		break;
	default:
	case colorGen_t::CGEN_IDENTITY_LIGHTING:
		Com_Memset(dest, tr.identityLightByte, tess.numVertexes * 4);
		break;
	case colorGen_t::CGEN_LIGHTING_DIFFUSE:
		RB_CalcDiffuseColor((unsigned char *)dest);
		break;
	case colorGen_t::CGEN_EXACT_VERTEX:
		Com_Memcpy(dest, tess.vertexColors, tess.numVertexes * sizeof(tess.vertexColors[0]));
		break;
	case colorGen_t::CGEN_CONST:
		for (i = 0; i < tess.numVertexes; i++)
		{
			dest[i] = pStage.bundle[b].constantColor;
		}
		break;
	case colorGen_t::CGEN_VERTEX:
		if (tr.identityLight == 1)
		{
			Com_Memcpy(dest, tess.vertexColors, tess.numVertexes * sizeof(tess.vertexColors[0]));
		}
		else
		{
			for (i = 0; i < tess.numVertexes; i++)
			{
				dest[i].rgba[0] = tess.vertexColors[i].rgba[0] * tr.identityLight;
				dest[i].rgba[1] = tess.vertexColors[i].rgba[1] * tr.identityLight;
				dest[i].rgba[2] = tess.vertexColors[i].rgba[2] * tr.identityLight;
				dest[i].rgba[3] = tess.vertexColors[i].rgba[3];
			}
		}
		break;
	case colorGen_t::CGEN_ONE_MINUS_VERTEX:
		if (tr.identityLight == 1)
		{
			for (i = 0; i < tess.numVertexes; i++)
			{
				dest[i].rgba[0] = 255 - tess.vertexColors[i].rgba[0];
				dest[i].rgba[1] = 255 - tess.vertexColors[i].rgba[1];
				dest[i].rgba[2] = 255 - tess.vertexColors[i].rgba[2];
			}
		}
		else
		{
			for (i = 0; i < tess.numVertexes; i++)
			{
				dest[i].rgba[0] = (255 - tess.vertexColors[i].rgba[0]) * tr.identityLight;
				dest[i].rgba[1] = (255 - tess.vertexColors[i].rgba[1]) * tr.identityLight;
				dest[i].rgba[2] = (255 - tess.vertexColors[i].rgba[2]) * tr.identityLight;
			}
		}
		break;
	case colorGen_t::CGEN_FOG:
	{
		const fog_t *fog = tr.world->fogs + tess.fogNum;

		for (i = 0; i < tess.numVertexes; i++)
		{
			dest[i] = fog->colorInt;
		}
	}
	break;
	case colorGen_t::CGEN_WAVEFORM:
		RB_CalcWaveColor(pStage.bundle[b].rgbWave, dest->rgba);
		break;
	case colorGen_t::CGEN_ENTITY:
		RB_CalcColorFromEntity(dest->rgba);
		break;
	case colorGen_t::CGEN_ONE_MINUS_ENTITY:
		RB_CalcColorFromOneMinusEntity(dest->rgba);
		break;
	}

	//
	// alphaGen
	//
	switch (pStage.bundle[b].alphaGen)
	{
	case alphaGen_t::AGEN_SKIP:
		break;
	case alphaGen_t::AGEN_IDENTITY:
		if ((pStage.bundle[b].rgbGen == colorGen_t::CGEN_VERTEX && tr.identityLight != 1) ||
			pStage.bundle[b].rgbGen != colorGen_t::CGEN_VERTEX)
		{
			for (i = 0; i < tess.numVertexes; i++)
			{
				dest[i].rgba[3] = 255;
			}
		}
		break;
	case alphaGen_t::AGEN_CONST:
		for (i = 0; i < tess.numVertexes; i++)
		{
			dest[i].rgba[3] = pStage.bundle[b].constantColor.rgba[3];
		}
		break;
	case alphaGen_t::AGEN_WAVEFORM:
		RB_CalcWaveAlpha(pStage.bundle[b].alphaWave, dest->rgba);
		break;
	case alphaGen_t::AGEN_LIGHTING_SPECULAR:
		RB_CalcSpecularAlpha(dest->rgba);
		break;
	case alphaGen_t::AGEN_ENTITY:
		RB_CalcAlphaFromEntity(dest->rgba);
		break;
	case alphaGen_t::AGEN_ONE_MINUS_ENTITY:
		RB_CalcAlphaFromOneMinusEntity(dest->rgba);
		break;
	case alphaGen_t::AGEN_VERTEX:
		for (i = 0; i < tess.numVertexes; i++)
		{
			dest[i].rgba[3] = tess.vertexColors[i].rgba[3];
		}
		break;
	case alphaGen_t::AGEN_ONE_MINUS_VERTEX:
		for (i = 0; i < tess.numVertexes; i++)
		{
			dest[i].rgba[3] = 255 - tess.vertexColors[i].rgba[3];
		}
		break;
	case alphaGen_t::AGEN_PORTAL:
	{
		for (i = 0; i < tess.numVertexes; i++)
		{
			unsigned char alpha;
			float len;
			vec3_t v{};

			VectorSubtract(tess.xyz[i], backEnd.viewParms.ort.origin, v);
			len = VectorLength(v) * tess.shader->portalRangeR;

			if (len > 1)
			{
				alpha = 0xff;
			}
			else
			{
				alpha = len * 0xff;
			}

			dest[i].rgba[3] = alpha;
		}
	}
	break;
	}

	//
	// fog adjustment for colors to fade out as fog increases
	//
	if (tess.fogNum)
	{
		switch (pStage.bundle[b].adjustColorsForFog)
		{
		case acff_t::ACFF_MODULATE_RGB:
			RB_CalcModulateColorsByFog(dest->rgba);
			break;
		case acff_t::ACFF_MODULATE_ALPHA:
			RB_CalcModulateAlphasByFog(dest->rgba);
			break;
		case acff_t::ACFF_MODULATE_RGBA:
			RB_CalcModulateRGBAsByFog(dest->rgba);
			break;
		case acff_t::ACFF_NONE:
			break;
		}
	}
}

uint32_t VK_PushUniform(const vkUniform_t &uniform)
{
	const uint32_t offset = vk_inst.cmd->uniform_read_offset = pad_up(vk_inst.cmd->vertex_buffer_offset, vk_inst.uniform_alignment);

	if (static_cast<uint64_t>(offset) + vk_inst.uniform_item_size > vk_inst.geometry_buffer_size)
		return ~0U;

	// push uniform
	Com_Memcpy(vk_inst.cmd->vertex_buffer_ptr + offset, &uniform, sizeof(uniform));
	vk_inst.cmd->vertex_buffer_offset = static_cast<uint64_t>(offset) + vk_inst.uniform_item_size;

	vk_reset_descriptor(VK_DESC_UNIFORM);
	vk_update_descriptor(VK_DESC_UNIFORM, vk_inst.cmd->uniform_descriptor);
	vk_update_descriptor_offset(VK_DESC_UNIFORM, vk_inst.cmd->uniform_read_offset);

	return offset;
}

static void R_BindAnimatedImage(const textureBundle_t &bundle)
{
	int64_t index;
	double v;

	if (bundle.isVideoMap)
	{
		ri.CIN_RunCinematic(bundle.videoMapHandle);
		ri.CIN_UploadCinematic(bundle.videoMapHandle);
		return;
	}

	if (bundle.isScreenMap /*&& backEnd.viewParms.frameSceneNum == 1*/)
	{
		if (!backEnd.screenMapDone)
			Bind(tr.blackImage);
		else
			vk_update_descriptor(glState.currenttmu + VK_DESC_TEXTURE_BASE, vk_inst.screenMap.color_descriptor);
		return;
	}

	if (bundle.numImageAnimations <= 1)
	{
		Bind(bundle.image[0]);
		return;
	}

	// it is necessary to do this messy calc to make sure animations line up
	// exactly with waveforms of the same frequency
	// v = tess.shaderTime * bundle.imageAnimationSpeed * FUNCTABLE_SIZE;
	// index = v;
	// index >>= FUNCTABLE_SIZE2;

	v = tess.shaderTime * bundle.imageAnimationSpeed; // fix for frameloss bug -EC-
	index = v;

	if (index < 0)
	{
		index = 0; // may happen with shader time offsets
	}
	index %= bundle.numImageAnimations;

	Bind(bundle.image[index]);
}

#ifdef USE_PMLIGHT
static void VK_SetLightParams(vkUniform_t &uniform, const dlight_t &dl)
{
	float radius;
	if (!glConfig.deviceSupportsGamma && !vk_inst.fboActive)
		VectorScale(dl.color, 2 * powf(r_intensity->value, r_gamma->value), uniform.light.color);
	else
		VectorCopy(dl.color, uniform.light.color);

	radius = dl.radius;

	// vertex data
	VectorCopy(backEnd.ort.viewOrigin, uniform.eyePos);
	uniform.eyePos[3] = 0.0f;
	VectorCopy(dl.transformed, uniform.light.pos);
	uniform.light.pos[3] = 0.0f;

	// fragment data
	uniform.light.color[3] = 1.0f / Square(radius);

	if (dl.linear)
	{
		vec4_t ab{};
		VectorSubtract(dl.transformed2, dl.transformed, ab);
		ab[3] = 1.0f / DotProduct(ab, ab);
		Vector4Copy(ab, uniform.light.vector);
	}
}
#endif

#ifdef USE_PMLIGHT
void VK_LightingPass(void)
{
	static uint32_t uniform_offset;
	static int fog_stage;
	uint32_t pipeline;
	const shaderStage_t *pStage;
	cullType_t cull;
	int abs_light;

	if (tess.shader->lightingStage < 0)
		return;

	pStage = tess.xstages[tess.shader->lightingStage];

	// we may need to update programs for fog transitions
	if (tess.dlightUpdateParams)
	{

		// fog parameters
		VK_SetFogParams(uniform, fog_stage);
		// light parameters
		VK_SetLightParams(uniform, *tess.light);

		uniform_offset = VK_PushUniform(uniform);

		tess.dlightUpdateParams = false;
	}

	if (uniform_offset == UINT32_MAX)
		return; // no space left...

	cull = tess.shader->cullType;
	if (backEnd.viewParms.portalView == portalView_t::PV_MIRROR)
	{
		switch (cull)
		{
		case cullType_t::CT_FRONT_SIDED:
			cull = cullType_t::CT_BACK_SIDED;
			break;
		case cullType_t::CT_BACK_SIDED:
			cull = cullType_t::CT_FRONT_SIDED;
			break;
		default:
			break;
		}
	}

	abs_light = /* (pStage->stateBits & GLS_ATEST_BITS) && */ (cull == cullType_t::CT_TWO_SIDED) ? 1 : 0;

	if (fog_stage)
		vk_update_descriptor(VK_DESC_FOG_DLIGHT, tr.fogImage->descriptor);

	if (tess.light->linear)
		pipeline = vk_inst.dlight1_pipelines_x[static_cast<int>(cull)][tess.shader->polygonOffset][fog_stage][abs_light];
	else
		pipeline = vk_inst.dlight_pipelines_x[static_cast<int>(cull)][tess.shader->polygonOffset][fog_stage][abs_light];

	SelectTexture(0);
	R_BindAnimatedImage(pStage->bundle[tess.shader->lightingBundle]);

#ifdef USE_VBO
	if (tess.vboIndex == 0)
#endif
	{
		R_ComputeTexCoords(tess.shader->lightingBundle, pStage->bundle[tess.shader->lightingBundle]);
	}

	vk_bind_pipeline(pipeline);
	vk_bind_index();
	vk_bind_lighting(tess.shader->lightingStage, tess.shader->lightingBundle);
	vk_draw_geometry(tess.depthRange, true);
}
#endif // USE_PMLIGHT

static void RB_IterateStagesGeneric(const shaderCommands_t &input, const bool fogCollapse)
{
	int tess_flags;
	int stage;
	uint32_t i;
	uint32_t pipeline;
	int fog_stage;
	bool pushUniform;

	vk_bind_index();

	tess_flags = input.shader->tessFlags;

	pushUniform = false;

#ifdef USE_FOG_COLLAPSE
	if (fogCollapse)
	{
		VK_SetFogParams(uniform, fog_stage);
		VectorCopy(backEnd.ort.viewOrigin, uniform.eyePos);
		vk_update_descriptor(VK_DESC_FOG_COLLAPSE, tr.fogImage->descriptor);
		pushUniform = true;
	}
	else
#endif
	{
		fog_stage = 0;
		if (tess_flags & TESS_VPOS)
		{
			VectorCopy(backEnd.ort.viewOrigin, uniform.eyePos);
			tess_flags &= ~TESS_VPOS;
			pushUniform = true;
		}
	}

	for (stage = 0; stage < MAX_SHADER_STAGES; stage++)
	{
		const shaderStage_t *pStage = tess.xstages[stage];
		if (!pStage)
			break;

#ifdef USE_VBO
		tess.vboStage = stage;
#endif

		tess_flags |= pStage->tessFlags;

		for (i = 0; i < pStage->numTexBundles; i++)
		{
			if (pStage->bundle[i].image[0] != NULL)
			{
				SelectTexture(i);
				R_BindAnimatedImage(pStage->bundle[i]);
				if (tess_flags & (TESS_ST0 << i))
				{
					R_ComputeTexCoords(i, pStage->bundle[i]);
				}
				if (tess_flags & (TESS_RGBA0 << i))
				{
					R_ComputeColors(i, tess.svars.colors[i], *pStage);
				}
				if (tess_flags & (TESS_ENT0 << i) && backEnd.currentEntity)
				{
#if defined(USE_AoS_to_SoA_SIMD)
					const int slot = backEnd.currentModelSlot;
					auto& soa = trsoa::GetFrameSoA();
					if (slot >= 0 && trsoa::SoA_ValidThisFrame(soa))
					{
						color4ub_t c{};
						const std::uint32_t packed = soa.models.shader.shaderColorPacked[slot];
						std::memcpy(&c, &packed, 4);

						uniform.ent.color[i][0] = c.rgba[0] * (1.0f / 255.0f);
						uniform.ent.color[i][1] = c.rgba[1] * (1.0f / 255.0f);
						uniform.ent.color[i][2] = c.rgba[2] * (1.0f / 255.0f);
						uniform.ent.color[i][3] = (pStage->bundle[i].alphaGen == alphaGen_t::AGEN_IDENTITY)
							? 1.0f
							: (c.rgba[3] * (1.0f / 255.0f));

						pushUniform = true;
						continue;
					}
#endif
					uniform.ent.color[i][0] = backEnd.currentEntity->e.shader.rgba[0] / 255.0;
					uniform.ent.color[i][1] = backEnd.currentEntity->e.shader.rgba[1] / 255.0;
					uniform.ent.color[i][2] = backEnd.currentEntity->e.shader.rgba[2] / 255.0;
					uniform.ent.color[i][3] = pStage->bundle[i].alphaGen == alphaGen_t::AGEN_IDENTITY ? 1.0 : (backEnd.currentEntity->e.shader.rgba[3] / 255.0);
					pushUniform = true;
				}

			}
		}

		if (pushUniform)
		{
			pushUniform = false;
			VK_PushUniform(uniform);
		}

		SelectTexture(0);

		if (r_lightmap->integer && pStage->bundle[1].lightmap != LIGHTMAP_INDEX_NONE)
		{
			// SelectTexture( 0 );
			Bind(tr.whiteImage); // replace diffuse texture with a white one thus effectively render only lightmap
		}

		if (backEnd.viewParms.portalView == portalView_t::PV_MIRROR)
		{
			pipeline = pStage->vk_mirror_pipeline[fog_stage];
		}
		else
		{
			pipeline = pStage->vk_pipeline[fog_stage];
		}

		vk_bind_pipeline(pipeline);
		vk_bind_geometry(tess_flags);
		vk_draw_geometry(tess.depthRange, true);

		if (pStage->depthFragment)
		{
			if (backEnd.viewParms.portalView == portalView_t::PV_MIRROR)
				pipeline = pStage->vk_mirror_pipeline_df;
			else
				pipeline = pStage->vk_pipeline_df;
			vk_bind_pipeline(pipeline);
			vk_draw_geometry(tess.depthRange, true);
		}

		// allow skipping out to show just lightmaps during development
		if (r_lightmap->integer && (pStage->bundle[0].lightmap != LIGHTMAP_INDEX_NONE || pStage->bundle[1].lightmap != LIGHTMAP_INDEX_NONE))
			break;

		tess_flags = 0;
	}
	if (pushUniform)
	{
		VK_PushUniform(uniform);
	}
	if (tess_flags) // fog-only shaders?
		vk_bind_geometry(tess_flags);
}

/*
===================
DrawMultitextured

output = t0 * t1 or t0 + t1

t0 = most upstream according to spec
t1 = most downstream according to spec
===================
*/

#ifdef USE_LEGACY_DLIGHTS
/*
===================
ProjectDlightTexture

Perform dynamic lighting with another rendering pass
===================
*/
static bool ProjectDlightTexture(void)
{
	bool rebindIndex = false;

	if (!backEnd.refdef.num_dlights)
	{
		return rebindIndex;
	}

	int i;
	uint32_t l;
	vec3_t origin{};
	float *texCoords;
	byte *colors;
	byte clipBits[SHADER_MAX_VERTEXES]{};
	uint32_t pipeline;

	glIndex_t hitIndexes[SHADER_MAX_INDEXES]{};
	int numIndexes;
	float scale;
	float radius;
	float modulate = 0.0f;

	for (l = 0; l < backEnd.refdef.num_dlights; l++)
	{
		if (!(tess.dlightBits & (1 << l)))
		{
			continue; // this surface definitely doesn't have any of this light
		}

		texCoords = (float *)&tess.svars.texcoords[0][0];
		tess.svars.texcoordPtr[0] = tess.svars.texcoords[0];
		colors = tess.svars.colors[0][0].rgba;
		const dlight_t &dl = backEnd.refdef.dlights[l];
		VectorCopy(dl.transformed, origin);
		radius = dl.radius;
		scale = 1.0f / radius;

		for (i = 0; i < tess.numVertexes; i++, texCoords += 2, colors += 4)
		{
			int clip = 0;
			vec3_t dist{};

			VectorSubtract(origin, tess.xyz[i], dist);

			backEnd.pc.c_dlightVertexes++;

			texCoords[0] = 0.5f + dist[0] * scale;
			texCoords[1] = 0.5f + dist[1] * scale;

			if (!r_dlightBacks->integer &&
				// dist . tess.normal[i]
				(dist[0] * tess.normal[i][0] +
				 dist[1] * tess.normal[i][1] +
				 dist[2] * tess.normal[i][2]) < 0.0f)
			{
				clip = 63;
			}
			else
			{
				if (texCoords[0] < 0.0f)
				{
					clip |= 1;
				}
				else if (texCoords[0] > 1.0f)
				{
					clip |= 2;
				}
				if (texCoords[1] < 0.0f)
				{
					clip |= 4;
				}
				else if (texCoords[1] > 1.0f)
				{
					clip |= 8;
				}

				// modulate the strength based on the height and color
				if (dist[2] > radius)
				{
					clip |= 16;
					modulate = 0.0f;
				}
				else if (dist[2] < -radius)
				{
					clip |= 32;
					modulate = 0.0f;
				}
				else
				{
					//*((int*)&dist[2]) &= 0x7FFFFFFF;
					dist[2] = fabsf(dist[2]);
					if (dist[2] < radius * 0.5f)
					{
						modulate = 1.0 * 255.0;
					}
					else
					{
						modulate = 2.0f * (radius - dist[2]) * scale * 255.0;
					}
				}
			}
			clipBits[i] = clip;
			colors[0] = dl.color[0] * modulate;
			colors[1] = dl.color[1] * modulate;
			colors[2] = dl.color[2] * modulate;
			colors[3] = 255;
		}

		// build a list of triangles that need light
		numIndexes = 0;
		for (i = 0; i < tess.numIndexes; i += 3)
		{
			glIndex_t a, b, c;

			a = tess.indexes[i];
			b = tess.indexes[i + 1];
			c = tess.indexes[i + 2];
			if (clipBits[a] & clipBits[b] & clipBits[c])
			{
				continue; // not lighted
			}
			hitIndexes[numIndexes] = a;
			hitIndexes[numIndexes + 1] = b;
			hitIndexes[numIndexes + 2] = c;
			numIndexes += 3;
		}

		if (numIndexes == 0)
		{
			continue;
		}

		Bind(tr.dlightImage);
		if (numIndexes != tess.numIndexes)
		{
			// re-bind index buffer for later fog pass
			rebindIndex = true;
		}
		pipeline = vk_inst.dlight_pipelines[dl.additive > 0 ? 1 : 0][static_cast<int>(tess.shader->cullType)][tess.shader->polygonOffset];
		vk_bind_pipeline(pipeline);
		vk_bind_index_ext(numIndexes, hitIndexes);
		vk_bind_geometry(TESS_RGBA0 | TESS_ST0);
		vk_draw_geometry(Vk_Depth_Range::DEPTH_RANGE_NORMAL, true);
		backEnd.pc.c_totalIndexes += numIndexes;
		backEnd.pc.c_dlightIndexes += numIndexes;
	}

	return rebindIndex;
}

#endif // USE_LEGACY_DLIGHTS

/*
===================
RB_FogPass

Blends a fog texture on top of everything else
===================
*/
static void RB_FogPass(bool rebindIndex)
{
	uint32_t pipeline = vk_inst.fog_pipelines[static_cast<int>(tess.shader->fogPass) - 1][static_cast<int>(tess.shader->cullType)][tess.shader->polygonOffset];
#ifdef USE_FOG_ONLY
	int fog_stage;

	// fog parameters
	vk_bind_pipeline(pipeline);
	if (rebindIndex)
	{
		vk_bind_index();
	}
	VK_SetFogParams(uniform, fog_stage);
	VK_PushUniform(uniform);
	vk_update_descriptor(VK_DESC_FOG_ONLY, tr.fogImage->descriptor);
	vk_draw_geometry(Vk_Depth_Range::DEPTH_RANGE_NORMAL, true);
#else
	const fog_t *fog = tr.world->fogs + tess.fogNum;
	int i;

	for (i = 0; i < tess.numVertexes; i++)
	{
		tess.svars.colors[0][i] = fog->colorInt;
	}

	RB_CalcFogTexCoords((float *)tess.svars.texcoords[0]);
	tess.svars.texcoordPtr[0] = tess.svars.texcoords[0];
	GL_Bind(tr.fogImage);

	vk_bind_pipeline(pipeline);
	if (rebindIndex)
	{
		vk_bind_index();
	}
	vk_bind_geometry(TESS_ST0 | TESS_RGBA0);
	vk_draw_geometry(DEPTH_RANGE_NORMAL, true);
#endif
}

void RB_StageIteratorGeneric(void)
{
	bool rebindIndex = false;
	bool fogCollapse = false;

#ifdef USE_VBO
	if (tess.vboIndex != 0)
	{
		VBO_PrepareQueues();
		tess.vboStage = 0;
	}
	else
#endif
		RB_DeformTessGeometry();

#ifdef USE_PMLIGHT
	if (tess.dlightPass)
	{
		VK_LightingPass();
		return;
	}
#endif

#ifdef USE_FOG_COLLAPSE
	fogCollapse = tess.fogNum && static_cast<int>(tess.shader->fogPass) && tess.shader->fogCollapse;
#endif

	// call shader function
	RB_IterateStagesGeneric(tess, fogCollapse);

	// now do any dynamic lighting needed
#ifdef USE_LEGACY_DLIGHTS
#ifdef USE_PMLIGHT
	if (r_dlightMode->integer == 0)
#endif
		if (tess.dlightBits && tess.shader->sort <= static_cast<float>(shaderSort_t::SS_OPAQUE) && !(tess.shader->surfaceFlags & (SURF_NODLIGHT | SURF_SKY)))
		{
			if (!fogCollapse)
			{
				rebindIndex = ProjectDlightTexture();
			}
		}
#endif // USE_LEGACY_DLIGHTS

	// now do fog
	if (tess.fogNum && static_cast<int>(tess.shader->fogPass) && !fogCollapse)
	{
		RB_FogPass(rebindIndex);
	}
}

/*
================
DrawTris

Draws triangle outlines for debugging
================
*/
static void DrawTris(const shaderCommands_t &input)
{
	uint32_t pipeline;

	if (r_showtris->integer == 1 && backEnd.drawConsole)
		return;

	if (tess.numIndexes == 0)
		return;

	if (r_fastsky->integer && input.shader->isSky)
		return;

#ifdef USE_VBO
	if (tess.vboIndex)
	{
#ifdef USE_PMLIGHT
		if (tess.dlightPass)
			pipeline = backEnd.viewParms.portalView == portalView_t::PV_MIRROR ? vk_inst.tris_mirror_debug_red_pipeline : vk_inst.tris_debug_red_pipeline;
		else
#endif
			pipeline = backEnd.viewParms.portalView == portalView_t::PV_MIRROR ? vk_inst.tris_mirror_debug_green_pipeline : vk_inst.tris_debug_green_pipeline;
	}
	else
#endif
	{
#ifdef USE_PMLIGHT
		if (tess.dlightPass)
			pipeline = backEnd.viewParms.portalView == portalView_t::PV_MIRROR ? vk_inst.tris_mirror_debug_red_pipeline : vk_inst.tris_debug_red_pipeline;
		else
#endif
			pipeline = backEnd.viewParms.portalView == portalView_t::PV_MIRROR ? vk_inst.tris_mirror_debug_pipeline : vk_inst.tris_debug_pipeline;
	}

	vk_bind_pipeline(pipeline);
	vk_draw_geometry(Vk_Depth_Range::DEPTH_RANGE_ZERO, true);
}

/*
================
DrawNormals

Draws vertex normals for debugging
================
*/
static void DrawNormals(const shaderCommands_t &input)
{
	int i;
#ifdef USE_VBO
	if (tess.vboIndex)
		return; // must be handled specially
#endif

	Bind(tr.whiteImage);

	tess.numIndexes = 0;
	for (i = 0; i < tess.numVertexes; i++)
	{
		VectorMA(tess.xyz[i], 2.0, tess.normal[i], tess.xyz[i + tess.numVertexes]);
		tess.indexes[tess.numIndexes + 0] = i;
		tess.indexes[tess.numIndexes + 1] = i + tess.numVertexes;
		tess.numIndexes += 2;
	}
	tess.numVertexes *= 2;
	Com_Memset(tess.svars.colors[0][0].rgba, tr.identityLightByte, tess.numVertexes * sizeof(color4ub_t));

	vk_bind_pipeline(vk_inst.normals_debug_pipeline);
	vk_bind_index();
	vk_bind_geometry(TESS_XYZ | TESS_ST0 | TESS_RGBA0);
	vk_draw_geometry(Vk_Depth_Range::DEPTH_RANGE_ZERO, true);
}

void RB_EndSurface(void)
{
	const shaderCommands_t &input = tess;

	if (input.numIndexes == 0)
	{
		// VBO_UnBind();
		return;
	}

	if (input.numIndexes > SHADER_MAX_INDEXES)
	{
		ri.Error(ERR_DROP, "RB_EndSurface() - SHADER_MAX_INDEXES hit");
	}

	if (input.numVertexes > SHADER_MAX_VERTEXES)
	{
		ri.Error(ERR_DROP, "RB_EndSurface() - SHADER_MAX_VERTEXES hit");
	}

	if (tess.shader == tr.shadowShader)
	{
		RB_ShadowTessEnd();
		return;
	}

	// for debugging of sort order issues, stop rendering after a given sort value
	if (r_debugSort->integer && r_debugSort->integer < tess.shader->sort && !backEnd.doneSurfaces)
	{
#ifdef USE_VBO
		tess.vboIndex = 0; // VBO_UnBind();
#endif
		return;
	}

	//
	// update performance counters
	//
#ifdef USE_PMLIGHT
	if (tess.dlightPass)
	{
		backEnd.pc.c_lit_batches++;
		backEnd.pc.c_lit_vertices += tess.numVertexes;
		backEnd.pc.c_lit_indices += tess.numIndexes;
	}
	else
#endif
	{
		backEnd.pc.c_shaders++;
		backEnd.pc.c_vertexes += tess.numVertexes;
		backEnd.pc.c_indexes += tess.numIndexes;
	}
	backEnd.pc.c_totalIndexes += tess.numIndexes * tess.numPasses;

	//
	// call off to shader specific tess end function
	//
	tess.shader->optimalStageIteratorFunc();

	//
	// draw debugging stuff
	//
	if (r_showtris->integer)
	{
		DrawTris(input);
	}
	if (r_shownormals->integer)
	{
		DrawNormals(input);
	}

	// clear shader so we can tell we don't have any unclosed surfaces
	tess.numIndexes = 0;
	tess.numVertexes = 0;

#ifdef USE_VBO
	tess.vboIndex = 0;
	// VBO_ClearQueue();
#endif
}

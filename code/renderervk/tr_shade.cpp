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
#include "string_operations.hpp"

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
static ID_INLINE void RB_ResetStageTracking() noexcept
{
	tess.gpuStageIndex = -1;
#ifdef USE_VBO
	tess.vboStage = 0;
#endif
}

static ID_INLINE void RB_SetStageTracking(const int stageIndex) noexcept
{
	tess.gpuStageIndex = stageIndex;
#ifdef USE_VBO
	tess.vboStage = stageIndex;
#endif
}

void RB_BeginSurface(shader_t& shader, const int fogNum)
{
	shader_t* state;

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

	tess.gpuMd3Active = false;
	tess.gpuMd3Surface = nullptr;
	tess.gpuMd3Lod = 0;
	tess.gpuMd3Backlerp = 0.0f;
	tess.gpuMd3OldFrame = 0;
	tess.gpuMd3NewFrame = 0;
	tess.gpuMd3Layout = gpuMd3Layout_t::NONE;
	RB_ResetStageTracking();

	tess.gpuMd3ViewOriginLocal[0] = 0.0f;
	tess.gpuMd3ViewOriginLocal[1] = 0.0f;
	tess.gpuMd3ViewOriginLocal[2] = 0.0f;
	tess.gpuMd3ViewOriginLocal[3] = 0.0f;

	tess.gpuMd3EntOrigin[0] = 0.0f;
	tess.gpuMd3EntOrigin[1] = 0.0f;
	tess.gpuMd3EntOrigin[2] = 0.0f;
	tess.gpuMd3EntOrigin[3] = 0.0f;

	tess.gpuMd3EntAxis1[0] = 0.0f;
	tess.gpuMd3EntAxis1[1] = 0.0f;
	tess.gpuMd3EntAxis1[2] = 0.0f;
	tess.gpuMd3EntAxis1[3] = 0.0f;

	tess.gpuMd3EntAxis2[0] = 0.0f;
	tess.gpuMd3EntAxis2[1] = 0.0f;
	tess.gpuMd3EntAxis2[2] = 0.0f;
	tess.gpuMd3EntAxis2[3] = 0.0f;

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

// GPU-MD3 texcoord skipping is controlled by R_GpuMd3TexCoordsHandledInShader().

struct gpuTcAffine_t
{
	float s_s;
	float s_t;
	float s_o;
	float t_s;
	float t_t;
	float t_o;
};

struct gpuTcProgram_t
{
	gpuTcAffine_t pre{};
	gpuTcAffine_t post{};

	bool useVectorTcGen = false;
	bool useTurbulent = false;

	float turbulentAmplitude = 0.0f;
	float turbulentNow = 0.0f;
};

static ID_INLINE void GpuTcAffineIdentity(gpuTcAffine_t& m) noexcept
{
	m.s_s = 1.0f; m.s_t = 0.0f; m.s_o = 0.0f;
	m.t_s = 0.0f; m.t_t = 1.0f; m.t_o = 0.0f;
}

static ID_INLINE float GpuTcFrac(const float v) noexcept
{
	return static_cast<float>(v - std::floor(v));
}

static ID_INLINE const float* GpuTcTableForFunc(const genFunc_t func) noexcept
{
	static const std::array<const float*, 7> kPtrLUT = {
		/* GF_NONE             */ nullptr,
		/* GF_SIN              */ tr.sinTable.data(),
		/* GF_SQUARE           */ tr.squareTable.data(),
		/* GF_TRIANGLE         */ tr.triangleTable.data(),
		/* GF_SAWTOOTH         */ tr.sawToothTable.data(),
		/* GF_INVERSE_SAWTOOTH */ tr.inverseSawToothTable.data(),
		/* GF_NOISE            */ nullptr
	};

	const auto idx = static_cast<std::uint8_t>(func);
	if (idx >= kPtrLUT.size()) [[unlikely]]
	{
		return nullptr;
	}

	return kPtrLUT[idx];
}

static ID_INLINE float GpuTcEvalWaveForm(const waveForm_t& wf) noexcept
{
	if (wf.func == genFunc_t::GF_NOISE)
	{
		return wf.base +
			R_NoiseGet4f(0.0f, 0.0f, 0.0f,
				(tess.shaderTime + static_cast<double>(wf.phase)) * static_cast<double>(wf.frequency)) *
			wf.amplitude;
	}

	const float* const table = GpuTcTableForFunc(wf.func);
	if (!table)
	{
		return wf.base;
	}

	const int64_t index =
		static_cast<int64_t>(((wf.phase + static_cast<float>(tess.shaderTime) * wf.frequency) * FUNCTABLE_SIZE));

	return wf.base + table[index & FUNCTABLE_MASK] * wf.amplitude;
}

// compose: next(current(tc))
static ID_INLINE void GpuTcAffineCompose(
	gpuTcAffine_t& cur,
	const float n_s_s, const float n_s_t, const float n_s_o,
	const float n_t_s, const float n_t_t, const float n_t_o) noexcept
{
	const gpuTcAffine_t old = cur;

	cur.s_s = n_s_s * old.s_s + n_s_t * old.t_s;
	cur.s_t = n_s_s * old.s_t + n_s_t * old.t_t;
	cur.s_o = n_s_s * old.s_o + n_s_t * old.t_o + n_s_o;

	cur.t_s = n_t_s * old.s_s + n_t_t * old.t_s;
	cur.t_t = n_t_s * old.s_t + n_t_t * old.t_t;
	cur.t_o = n_t_s * old.s_o + n_t_t * old.t_o + n_t_o;
}

static ID_INLINE bool GpuTcComposeAffineMod(gpuTcAffine_t& dst, const texModInfo_t& tm) noexcept
{
	switch (tm.type)
	{
	case texMod_t::TMOD_NONE:
		return true;

	case texMod_t::TMOD_SCROLL:
	{
		const float ds = GpuTcFrac(tm.scroll[0] * static_cast<float>(tess.shaderTime));
		const float dt = GpuTcFrac(tm.scroll[1] * static_cast<float>(tess.shaderTime));
		GpuTcAffineCompose(dst,
			1.0f, 0.0f, ds,
			0.0f, 1.0f, dt);
		return true;
	}

	case texMod_t::TMOD_ENTITY_TRANSLATE:
	{
		const float ds = GpuTcFrac(backEnd.currentEntity->e.shaderTexCoord[0] * static_cast<float>(tess.shaderTime));
		const float dt = GpuTcFrac(backEnd.currentEntity->e.shaderTexCoord[1] * static_cast<float>(tess.shaderTime));
		GpuTcAffineCompose(dst,
			1.0f, 0.0f, ds,
			0.0f, 1.0f, dt);
		return true;
	}

	case texMod_t::TMOD_SCALE:
		GpuTcAffineCompose(dst,
			tm.scale[0], 0.0f, 0.0f,
			0.0f, tm.scale[1], 0.0f);
		return true;

	case texMod_t::TMOD_OFFSET:
		GpuTcAffineCompose(dst,
			1.0f, 0.0f, tm.offset[0],
			0.0f, 1.0f, tm.offset[1]);
		return true;

	case texMod_t::TMOD_SCALE_OFFSET:
		GpuTcAffineCompose(dst,
			tm.scale[0], 0.0f, tm.offset[0],
			0.0f, tm.scale[1], tm.offset[1]);
		return true;

	case texMod_t::TMOD_OFFSET_SCALE:
		GpuTcAffineCompose(dst,
			tm.scale[0], 0.0f, tm.offset[0] * tm.scale[0],
			0.0f, tm.scale[1], tm.offset[1] * tm.scale[1]);
		return true;

	case texMod_t::TMOD_TRANSFORM:
		GpuTcAffineCompose(dst,
			tm.matrix[0][0], tm.matrix[1][0], tm.translate[0],
			tm.matrix[0][1], tm.matrix[1][1], tm.translate[1]);
		return true;

	case texMod_t::TMOD_ROTATE:
	{
		const double degs = -tm.rotateSpeed * tess.shaderTime;
		const float radians = static_cast<float>(degs * (M_PI / 180.0));
		const float s = std::sin(radians);
		const float c = std::cos(radians);

		const float tr0 = 0.5f - 0.5f * c + 0.5f * s;
		const float tr1 = 0.5f - 0.5f * s - 0.5f * c;

		GpuTcAffineCompose(dst,
			c, -s, tr0,
			s, c, tr1);
		return true;
	}

	case texMod_t::TMOD_STRETCH:
	{
		const float p = 1.0f / GpuTcEvalWaveForm(tm.wave);
		GpuTcAffineCompose(dst,
			p, 0.0f, 0.5f - 0.5f * p,
			0.0f, p, 0.5f - 0.5f * p);
		return true;
	}

	default:
		return false;
	}
}

static bool R_CanGpuMd3UsePureAffineTexMods(const textureBundle_t& bundle) noexcept
{
	for (int i = 0; i < bundle.numTexMods; ++i)
	{
		switch (bundle.texMods[i].type)
		{
		case texMod_t::TMOD_NONE:
		case texMod_t::TMOD_SCROLL:
		case texMod_t::TMOD_SCALE:
		case texMod_t::TMOD_OFFSET:
		case texMod_t::TMOD_SCALE_OFFSET:
		case texMod_t::TMOD_OFFSET_SCALE:
		case texMod_t::TMOD_TRANSFORM:
		case texMod_t::TMOD_ROTATE:
		case texMod_t::TMOD_ENTITY_TRANSLATE:
		case texMod_t::TMOD_STRETCH:
			break;

		case texMod_t::TMOD_TURBULENT:
		default:
			return false;
		}
	}
	return true;
}

bool R_CanGpuMd3UseAffineTexMods(const textureBundle_t& bundle) noexcept
{
	const bool isBaseAffineTcGen =
		bundle.tcGen == texCoordGen_t::TCGEN_TEXTURE ||
		bundle.tcGen == texCoordGen_t::TCGEN_VECTOR;

	const bool isEnvTcGen =
		bundle.tcGen == texCoordGen_t::TCGEN_ENVIRONMENT_MAPPED ||
		bundle.tcGen == texCoordGen_t::TCGEN_ENVIRONMENT_MAPPED_FP;

	if (!isBaseAffineTcGen && !isEnvTcGen)
	{
		return false;
	}

	bool seenTurbulent = false;

	for (int i = 0; i < bundle.numTexMods; ++i)
	{
		switch (bundle.texMods[i].type)
		{
		case texMod_t::TMOD_NONE:
		case texMod_t::TMOD_SCROLL:
		case texMod_t::TMOD_SCALE:
		case texMod_t::TMOD_OFFSET:
		case texMod_t::TMOD_SCALE_OFFSET:
		case texMod_t::TMOD_OFFSET_SCALE:
		case texMod_t::TMOD_TRANSFORM:
		case texMod_t::TMOD_ROTATE:
		case texMod_t::TMOD_ENTITY_TRANSLATE:
		case texMod_t::TMOD_STRETCH:
			break;

		case texMod_t::TMOD_TURBULENT:
			if (seenTurbulent)
				return false;

			// Turbulent repurposes tcGenVector uniforms as post-transform storage,
			// so keep vector tcGen on the old path for now. Regular base ST and
			// env-generated ST are both safe, because the shader computes the base
			// coordinates first and only then applies the post-turbulent affine step.
			if (bundle.tcGen == texCoordGen_t::TCGEN_VECTOR)
				return false;

			seenTurbulent = true;
			break;

		default:
			return false;
		}
	}

	return true;
}

static bool R_BuildGpuMd3TcProgram(gpuTcProgram_t& prog, const textureBundle_t& bundle) noexcept
{
	GpuTcAffineIdentity(prog.pre);
	GpuTcAffineIdentity(prog.post);

	prog.useVectorTcGen = false;
	prog.useTurbulent = false;
	prog.turbulentAmplitude = 0.0f;
	prog.turbulentNow = 0.0f;

	if (!R_CanGpuMd3UseAffineTexMods(bundle))
		return false;

	switch (bundle.tcGen)
	{
	case texCoordGen_t::TCGEN_TEXTURE:
		break;

	case texCoordGen_t::TCGEN_VECTOR:
		prog.useVectorTcGen = true;
		break;

	case texCoordGen_t::TCGEN_ENVIRONMENT_MAPPED:
	case texCoordGen_t::TCGEN_ENVIRONMENT_MAPPED_FP:
		// Base env tc is generated directly in the MD3 env vertex shaders.
		// We still want to run the affine/turbulent tcMod program on top of that.
		break;

	default:
		return false;
	}

	gpuTcAffine_t* current = &prog.pre;

	for (int i = 0; i < bundle.numTexMods; ++i)
	{
		const texModInfo_t& tm = bundle.texMods[i];

		if (tm.type == texMod_t::TMOD_TURBULENT)
		{
			prog.useTurbulent = true;
			prog.turbulentAmplitude = tm.wave.amplitude;
			prog.turbulentNow = tm.wave.phase + static_cast<float>(tess.shaderTime) * tm.wave.frequency;
			current = &prog.post;
			continue;
		}

		if (!GpuTcComposeAffineMod(*current, tm))
			return false;
	}

	return true;
}

enum class gpuTcSlot_t : std::uint8_t
{
	bundle0,
	bundle1,
	bundle2
};

static ID_INLINE void VK_SetIdentityTcParamsForSlot(vkUniform_t& u, const gpuTcSlot_t slot) noexcept
{
	vec4_t* mod0 = nullptr;
	vec4_t* mod1 = nullptr;
	vec4_t* gen0 = nullptr;
	vec4_t* gen1 = nullptr;

	switch (slot)
	{
	case gpuTcSlot_t::bundle0:
		mod0 = &u.tcMod0;
		mod1 = &u.tcMod1;
		gen0 = &u.tcGenVector0;
		gen1 = &u.tcGenVector1;
		break;
	case gpuTcSlot_t::bundle1:
		mod0 = &u.tc1Mod0;
		mod1 = &u.tc1Mod1;
		gen0 = &u.tc1GenVector0;
		gen1 = &u.tc1GenVector1;
		break;
	case gpuTcSlot_t::bundle2:
		mod0 = &u.tc2Mod0;
		mod1 = &u.tc2Mod1;
		gen0 = &u.tc2GenVector0;
		gen1 = &u.tc2GenVector1;
		break;
	}

	(*mod0)[0] = 1.0f; (*mod0)[1] = 0.0f; (*mod0)[2] = 0.0f; (*mod0)[3] = 0.0f;
	(*mod1)[0] = 0.0f; (*mod1)[1] = 1.0f; (*mod1)[2] = 0.0f; (*mod1)[3] = 0.0f;

	(*gen0)[0] = 0.0f; (*gen0)[1] = 0.0f; (*gen0)[2] = 0.0f; (*gen0)[3] = 0.0f;
	(*gen1)[0] = 0.0f; (*gen1)[1] = 0.0f; (*gen1)[2] = 0.0f; (*gen1)[3] = 0.0f;
}

static ID_INLINE void VK_SetIdentityTcParams(vkUniform_t& u) noexcept
{
	VK_SetIdentityTcParamsForSlot(u, gpuTcSlot_t::bundle0);
	VK_SetIdentityTcParamsForSlot(u, gpuTcSlot_t::bundle1);
	VK_SetIdentityTcParamsForSlot(u, gpuTcSlot_t::bundle2);
}

static ID_INLINE void VK_SetIdentityGpuMd3DeformParams(vkUniform_t& u) noexcept
{
	u.deform0[0] = 0.0f; u.deform0[1] = 0.0f; u.deform0[2] = 0.0f; u.deform0[3] = 0.0f;
	u.deform1[0] = 0.0f; u.deform1[1] = 0.0f; u.deform1[2] = 0.0f; u.deform1[3] = 0.0f;
}

static ID_INLINE void VK_SetIdentityGpuMd3ColorParams(vkUniform_t& u) noexcept
{
	u.colorMode01[0] = 0.0f;
	u.colorMode01[1] = 0.0f;
	u.colorMode01[2] = 0.0f;
	u.colorMode01[3] = 0.0f;

	u.color1Fixed[0] = 1.0f;
	u.color1Fixed[1] = 1.0f;
	u.color1Fixed[2] = 1.0f;
	u.color1Fixed[3] = 1.0f;

	u.color2Fixed[0] = 1.0f;
	u.color2Fixed[1] = 1.0f;
	u.color2Fixed[2] = 1.0f;
	u.color2Fixed[3] = 1.0f;
}

static ID_INLINE gpuMd3Layout_t VK_GpuMd3LayoutForShaderType(const Vk_Shader_Type shaderType) noexcept
{
	return VK_GpuMd3LayoutForShaderTypeShared(shaderType);
}

static ID_INLINE gpuMd3Layout_t VK_GpuMd3LayoutForStage(const shaderStage_t& stage) noexcept
{
	Vk_Pipeline_Def def{};
	vk_get_pipeline_def(stage.vk_pipeline[0], def);
	return VK_GpuMd3LayoutForShaderType(def.shader_type);
}

static ID_INLINE bool R_IsGpuMd3EnvLayout(const shaderStage_t& stage) noexcept
{
	const gpuMd3Layout_t layout = VK_GpuMd3LayoutForStage(stage);
	return layout == gpuMd3Layout_t::GENERIC_ENV_COLOR ||
		layout == gpuMd3Layout_t::GENERIC_ENV_NO_COLOR;
}

static void VK_SetGpuMd3DeformParams(vkUniform_t& u, const shaderStage_t& stage) noexcept
{
	VK_SetIdentityGpuMd3DeformParams(u);

	if (!tess.gpuMd3Active || !tess.shader || tess.shader->numDeforms != 1)
	{
		return;
	}

	const deformStage_t& ds = tess.shader->deforms[0];

	switch (ds.deformation)
	{
	case deform_t::DEFORM_WAVE:
		if (ds.deformationWave.func == genFunc_t::GF_NONE)
		{
			return;
		}

		u.deform0[0] = 1.0f;
		u.deform0[1] = ds.deformationSpread;
		u.deform0[2] = 0.0f;
		u.deform0[3] = ds.deformationWave.phase + static_cast<float>(tess.shaderTime) * ds.deformationWave.frequency;

		u.deform1[0] = ds.deformationWave.base;
		u.deform1[1] = ds.deformationWave.amplitude;
		u.deform1[2] = static_cast<float>(std::to_underlying(ds.deformationWave.func));
		u.deform1[3] = (ds.deformationWave.frequency != 0.0f) ? 1.0f : 0.0f;
		return;

	case deform_t::DEFORM_BULGE:
		// Bulge uses the model's base ST set, not the stage tcGen result.
		// GPU MD3 env pipelines bind ST as well, so bulge can remain on GPU.
		u.deform0[0] = 2.0f;
		u.deform0[1] = ds.bulgeWidth;
		u.deform0[2] = ds.bulgeHeight;
		u.deform0[3] = static_cast<float>(backEnd.refdef.floatTime * ds.bulgeSpeed);
		return;

	case deform_t::DEFORM_MOVE:
		if (ds.deformationWave.func == genFunc_t::GF_NONE)
		{
			return;
		}

		u.deform0[0] = 3.0f;
		u.deform0[1] = ds.moveVector[0];
		u.deform0[2] = ds.moveVector[1];
		u.deform0[3] = ds.moveVector[2];

		u.deform1[0] = ds.deformationWave.base;
		u.deform1[1] = ds.deformationWave.amplitude;
		u.deform1[2] = static_cast<float>(std::to_underlying(ds.deformationWave.func));
		u.deform1[3] = ds.deformationWave.phase + static_cast<float>(tess.shaderTime) * ds.deformationWave.frequency;
		return;

	case deform_t::DEFORM_NORMALS:
		// Mirror RB_CalcDeformNormals() on the GPU.
		// The shader derives the animated time slice from deform0.w and
		// uses the amplitude from deform0.y.
		u.deform0[0] = 4.0f;
		u.deform0[1] = ds.deformationWave.amplitude;
		u.deform0[2] = 0.0f;
		u.deform0[3] = static_cast<float>(tess.shaderTime * ds.deformationWave.frequency);
		return;

	default:
		(void)stage;
		return;
	}
}
static void VK_SetGpuMd3TcParamsForSlot(vkUniform_t& u, const textureBundle_t& bundle, const gpuTcSlot_t slot) noexcept
{
	VK_SetIdentityTcParamsForSlot(u, slot);

	gpuTcProgram_t prog{};
	if (!R_BuildGpuMd3TcProgram(prog, bundle))
	{
		return;
	}

	vec4_t* mod0 = nullptr;
	vec4_t* mod1 = nullptr;
	vec4_t* gen0 = nullptr;
	vec4_t* gen1 = nullptr;

	switch (slot)
	{
	case gpuTcSlot_t::bundle0:
		mod0 = &u.tcMod0;
		mod1 = &u.tcMod1;
		gen0 = &u.tcGenVector0;
		gen1 = &u.tcGenVector1;
		break;
	case gpuTcSlot_t::bundle1:
		mod0 = &u.tc1Mod0;
		mod1 = &u.tc1Mod1;
		gen0 = &u.tc1GenVector0;
		gen1 = &u.tc1GenVector1;
		break;
	case gpuTcSlot_t::bundle2:
		mod0 = &u.tc2Mod0;
		mod1 = &u.tc2Mod1;
		gen0 = &u.tc2GenVector0;
		gen1 = &u.tc2GenVector1;
		break;
	}

	int flagBits =
		(prog.useVectorTcGen ? 1 : 0) |
		(prog.useTurbulent ? 2 : 0) |
		((slot == gpuTcSlot_t::bundle0) ? 0 : 4);

	if (slot != gpuTcSlot_t::bundle0)
	{
		switch (bundle.tcGen)
		{
		case texCoordGen_t::TCGEN_ENVIRONMENT_MAPPED:
			flagBits |= 8;
			break;

		case texCoordGen_t::TCGEN_ENVIRONMENT_MAPPED_FP:
			flagBits |= bundle.isScreenMap ? 32 : 16;
			break;

		default:
			break;
		}
	}

	(*mod0)[0] = prog.pre.s_s;
	(*mod0)[1] = prog.pre.s_t;
	(*mod0)[2] = prog.pre.s_o;
	(*mod0)[3] = static_cast<float>(flagBits);

	(*mod1)[0] = prog.pre.t_s;
	(*mod1)[1] = prog.pre.t_t;
	(*mod1)[2] = prog.pre.t_o;
	(*mod1)[3] = prog.turbulentAmplitude;

	if (prog.useVectorTcGen)
	{
		(*gen0)[0] = bundle.tcGenVectors[0][0];
		(*gen0)[1] = bundle.tcGenVectors[0][1];
		(*gen0)[2] = bundle.tcGenVectors[0][2];
		(*gen0)[3] = 0.0f;

		(*gen1)[0] = bundle.tcGenVectors[1][0];
		(*gen1)[1] = bundle.tcGenVectors[1][1];
		(*gen1)[2] = bundle.tcGenVectors[1][2];
		(*gen1)[3] = 0.0f;
	}

	if (prog.useTurbulent)
	{
		(*gen0)[0] = prog.post.s_s;
		(*gen0)[1] = prog.post.s_t;
		(*gen0)[2] = prog.post.s_o;
		(*gen0)[3] = prog.turbulentNow;

		(*gen1)[0] = prog.post.t_s;
		(*gen1)[1] = prog.post.t_t;
		(*gen1)[2] = prog.post.t_o;
		(*gen1)[3] = 0.0f;
	}
}

static bool R_GpuMd3SecondaryTexCoordsHandledInShader(
	const shaderStage_t& stage,
	const int bundleIndex,
	const textureBundle_t& bundle) noexcept
{
	if (bundleIndex <= 0 || bundleIndex >= stage.numTexBundles)
		return false;

	if (!bundle.image[0] || bundle.gpuTcGenHandledInShader)
		return false;

	if (!R_CanGpuMd3UseAffineTexMods(bundle))
		return false;

	switch (bundle.tcGen)
	{
	case texCoordGen_t::TCGEN_TEXTURE:
	case texCoordGen_t::TCGEN_VECTOR:
	case texCoordGen_t::TCGEN_ENVIRONMENT_MAPPED:
		return true;

	case texCoordGen_t::TCGEN_ENVIRONMENT_MAPPED_FP:
		return static_cast<int>(stage.gpuEnvBundleIndex) == bundleIndex;

	default:
		return false;
	}
}

static bool R_GpuMd3TexCoordsHandledInShader(const shaderStage_t& stage, const int bundleIndex, const textureBundle_t& bundle) noexcept
{
	if (!tess.gpuMd3Active)
		return false;

	if (bundleIndex == 0)
	{
		if (R_IsGpuMd3EnvLayout(stage))
		{
			return R_CanGpuMd3UseAffineTexMods(bundle) &&
				(bundle.tcGen == texCoordGen_t::TCGEN_ENVIRONMENT_MAPPED ||
					bundle.tcGen == texCoordGen_t::TCGEN_ENVIRONMENT_MAPPED_FP);
		}

		if (!R_CanGpuMd3UseAffineTexMods(bundle))
			return false;

		switch (bundle.tcGen)
		{
		case texCoordGen_t::TCGEN_TEXTURE:
		case texCoordGen_t::TCGEN_VECTOR:
			return true;

		default:
			return false;
		}
	}

	if (bundleIndex <= 0 || bundleIndex >= stage.numTexBundles)
		return false;

	return R_GpuMd3SecondaryTexCoordsHandledInShader(stage, bundleIndex, bundle);
}


void R_ComputeTexCoords(const int b, const textureBundle_t& bundle)
{
	if (!tess.numVertexes)
		return;

	const shaderStage_t* const stage =
		(tess.shader && tess.gpuStageIndex >= 0 && tess.gpuStageIndex < MAX_SHADER_STAGES)
		? tess.shader->stages[tess.gpuStageIndex]
		: nullptr;

	const bool gpuHandled = stage ? R_GpuMd3TexCoordsHandledInShader(*stage, b, bundle) : false;

	if (gpuHandled)
		return;

	int i;
	int tm;
	vec2_t* src;
	vec2_t* dst;
	src = dst = tess.svars.texcoords[b];

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
		RB_CalcFogTexCoords((float*)dst);
		break;
	case texCoordGen_t::TCGEN_ENVIRONMENT_MAPPED:
		RB_CalcEnvironmentTexCoords((float*)dst);
		break;
	case texCoordGen_t::TCGEN_ENVIRONMENT_MAPPED_FP:
		RB_CalcEnvironmentTexCoordsFP((float*)dst, bundle.isScreenMap);
		break;
	case texCoordGen_t::TCGEN_BAD:
		return;
	}

	for (tm = 0; tm < bundle.numTexMods; tm++)
	{
		switch (bundle.texMods[tm].type)
		{
		case texMod_t::TMOD_NONE:
			tm = TR_MAX_TEXMODS;
			break;

		case texMod_t::TMOD_TURBULENT:
			RB_CalcTurbulentTexCoords(bundle.texMods[tm].wave, (float*)src, (float*)dst);
			src = dst;
			break;

		case texMod_t::TMOD_ENTITY_TRANSLATE:
			RB_CalcScrollTexCoords(backEnd.currentEntity->e.shaderTexCoord, (float*)src, (float*)dst);
			src = dst;
			break;

		case texMod_t::TMOD_SCROLL:
			RB_CalcScrollTexCoords(bundle.texMods[tm].scroll, (float*)src, (float*)dst);
			src = dst;
			break;

		case texMod_t::TMOD_SCALE:
			RB_CalcScaleTexCoords(bundle.texMods[tm].scale, (float*)src, (float*)dst);
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
			RB_CalcStretchTexCoords(bundle.texMods[tm].wave, (float*)src, (float*)dst);
			src = dst;
			break;

		case texMod_t::TMOD_TRANSFORM:
			RB_CalcTransformTexCoords(bundle.texMods[tm], (float*)src, (float*)dst);
			src = dst;
			break;

		case texMod_t::TMOD_ROTATE:
			RB_CalcRotateTexCoords(bundle.texMods[tm].rotateSpeed, (float*)src, (float*)dst);
			src = dst;
			break;

		default:
			ri.Error(ERR_DROP, "ERROR: unknown texmod '%d' in shader '%s'",
				static_cast<int>(bundle.texMods[tm].type), tess.shader->name);
			break;
		}
	}

	tess.svars.texcoordPtr[b] = src;
}

void VK_SetFogParams(vkUniform_t& uniform, int& fogStage)
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

void R_ComputeColors(const int b, color4ub_t* dest, const shaderStage_t& pStage)
{
	if (tess.numVertexes == 0)
		return;

	if (tess.gpuMd3Active && b > 0 && R_GpuMd3SecondaryColorHandledInShader(pStage, b, pStage.bundle[b]))
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
		RB_CalcDiffuseColor((unsigned char*)dest);
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
		const fog_t* fog = tr.world->fogs + tess.fogNum;

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

uint32_t VK_PushUniform(const vkUniform_t& uniform)
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

static void R_BindAnimatedImage(const textureBundle_t& bundle)
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
static void VK_SetLightParams(vkUniform_t& uniform, const dlight_t& dl)
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

#ifdef USE_LEGACY_DLIGHTS
static void VK_SetLegacyGpuMd3DlightParams(vkUniform_t& uniform, const dlight_t& dl, const shaderStage_t* stage) noexcept
{
	VectorCopy(dl.transformed, uniform.light.pos);
	uniform.light.pos[3] = dl.radius > 0.0f ? 1.0f / dl.radius : 0.0f;

	uniform.light.color[0] = dl.color[0];
	uniform.light.color[1] = dl.color[1];
	uniform.light.color[2] = dl.color[2];
	uniform.light.color[3] = 1.0f;

	uniform.light.vector[0] = dl.radius;
	uniform.light.vector[1] = r_dlightBacks->integer ? 1.0f : 0.0f;
	uniform.light.vector[2] = dl.radius * 0.5f;
	uniform.light.vector[3] = 0.0f;

	VK_SetIdentityTcParams(uniform);
	VK_SetIdentityGpuMd3DeformParams(uniform);

	if (stage)
	{
		VK_SetGpuMd3DeformParams(uniform, *stage);
	}
}
#endif

#ifdef USE_PMLIGHT
void VK_LightingPass(void)
{
	static uint32_t uniform_offset;
	static int fog_stage;
	uint32_t pipeline;
	const shaderStage_t* pStage;
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

static ID_INLINE void VK_WorldPointToLocal(const vec3_t world, vec3_t local) noexcept
{
	vec3_t delta;
	VectorSubtract(world, backEnd.ort.origin, delta);

	local[0] = DotProduct(delta, backEnd.ort.axis[0]);
	local[1] = DotProduct(delta, backEnd.ort.axis[1]);
	local[2] = DotProduct(delta, backEnd.ort.axis[2]);
}


static void VK_SetGpuMd3EnvParams(vkUniform_t& uniform, const shaderStage_t& stage)
{
	// bazowe wartości
	uniform.eyePos[0] = tess.gpuMd3ViewOriginLocal[0];
	uniform.eyePos[1] = tess.gpuMd3ViewOriginLocal[1];
	uniform.eyePos[2] = tess.gpuMd3ViewOriginLocal[2];
	uniform.eyePos[3] = 0.0f; // regular env

	uniform.light.pos[0] = 0.0f;
	uniform.light.pos[1] = 0.0f;
	uniform.light.pos[2] = 0.0f;
	uniform.light.pos[3] = 0.0f;

	uniform.light.color[0] = 0.0f;
	uniform.light.color[1] = 0.0f;
	uniform.light.color[2] = 0.0f;
	uniform.light.color[3] = 0.0f;

	uniform.light.vector[0] = 0.0f;
	uniform.light.vector[1] = 0.0f;
	uniform.light.vector[2] = 0.0f;
	uniform.light.vector[3] = 0.0f;

	if (!tess.gpuMd3Active)
	{
		return;
	}

	int envBundleIndex = static_cast<int>(stage.gpuEnvBundleIndex);
	if (envBundleIndex < 0 || envBundleIndex >= stage.numTexBundles)
	{
		if ((stage.tessFlags & TESS_ENV) == 0)
		{
			return;
		}

		envBundleIndex = 0;
	}

	const textureBundle_t& envBundle = stage.bundle[envBundleIndex];

	// nie zgadujemy po RF_FIRST_PERSON
	texCoordGen_t tcGen = envBundle.tcGen;

	if (envBundle.gpuTcGenHandledInShader &&
		envBundle.originalTcGen != texCoordGen_t::TCGEN_BAD)
	{
		tcGen = envBundle.originalTcGen;
	}

	// zwykłe environment mapping
	if (tcGen == texCoordGen_t::TCGEN_ENVIRONMENT_MAPPED)
	{
		return;
	}

	// first-person environment mapping
	if (tcGen == texCoordGen_t::TCGEN_ENVIRONMENT_MAPPED_FP)
	{
		uniform.eyePos[3] = 1.0f;

		uniform.light.pos[0] = tess.gpuMd3EntOrigin[0];
		uniform.light.pos[1] = tess.gpuMd3EntOrigin[1];
		uniform.light.pos[2] = tess.gpuMd3EntOrigin[2];
		uniform.light.pos[3] = 0.0f;

		uniform.light.color[0] = tess.gpuMd3EntAxis1[0];
		uniform.light.color[1] = tess.gpuMd3EntAxis1[1];
		uniform.light.color[2] = tess.gpuMd3EntAxis1[2];
		uniform.light.color[3] = 0.0f;

		uniform.light.vector[0] = tess.gpuMd3EntAxis2[0];
		uniform.light.vector[1] = tess.gpuMd3EntAxis2[1];
		uniform.light.vector[2] = tess.gpuMd3EntAxis2[2];
		uniform.light.vector[3] = 0.0f;

		if (envBundle.isScreenMap && backEnd.viewParms.frameSceneNum == 1)
		{
			uniform.light.pos[3] = 1.0f;
		}

		return;
	}
}


static ID_INLINE float VK_GpuMd3Clamp01(const float v) noexcept
{
	if (v < 0.0f)
		return 0.0f;
	if (v > 1.0f)
		return 1.0f;
	return v;
}

static ID_INLINE bool VK_GpuMd3SupportsWaveAlpha(const textureBundle_t& b0) noexcept
{
	return b0.alphaGen == alphaGen_t::AGEN_WAVEFORM;
}

static ID_INLINE bool VK_TryBuildGpuMd3UniformAlpha(float& outAlpha, const textureBundle_t& b0) noexcept
{
	constexpr float kInv255 = 1.0f / 255.0f;

	switch (b0.alphaGen)
	{
	case alphaGen_t::AGEN_IDENTITY:
		outAlpha = 1.0f;
		return true;

	case alphaGen_t::AGEN_CONST:
		outAlpha = b0.constantColor.rgba[3] * kInv255;
		return true;

	case alphaGen_t::AGEN_ENTITY:
		if (backEnd.currentEntity)
		{
			outAlpha = backEnd.currentEntity->e.shader.rgba[3] * kInv255;
			return true;
		}
		return false;

	case alphaGen_t::AGEN_ONE_MINUS_ENTITY:
		if (backEnd.currentEntity)
		{
			outAlpha = (255.0f - backEnd.currentEntity->e.shader.rgba[3]) * kInv255;
			return true;
		}
		return false;

	case alphaGen_t::AGEN_WAVEFORM:
		if (!VK_GpuMd3SupportsWaveAlpha(b0))
			return false;
		outAlpha = VK_GpuMd3Clamp01(GpuTcEvalWaveForm(b0.alphaWave));
		return true;

	default:
		return false;
	}
}

static ID_INLINE float VK_EvalGpuMd3WaveRgb(const waveForm_t& wf) noexcept
{
	float glow = 0.0f;

	if (wf.func == genFunc_t::GF_NOISE)
	{
		glow = wf.base + R_NoiseGet4f(0.0f, 0.0f, 0.0f, (tess.shaderTime + wf.phase) * wf.frequency) * wf.amplitude;
	}
	else
	{
		glow = GpuTcEvalWaveForm(wf) * tr.identityLight;
	}

	return VK_GpuMd3Clamp01(glow);
}


static ID_INLINE bool VK_GpuMd3VertexAlphaSupported(uint32_t& mode, const textureBundle_t& b0) noexcept
{
	switch (b0.alphaGen)
	{
	case alphaGen_t::AGEN_SKIP:
		return true;

	case alphaGen_t::AGEN_VERTEX:
		mode |= GPU_MD3_COLOR_VERTEX_ALPHA;
		return true;

	case alphaGen_t::AGEN_ONE_MINUS_VERTEX:
		mode |= GPU_MD3_COLOR_ONE_MINUS_VERTEX_ALPHA;
		return true;

	case alphaGen_t::AGEN_IDENTITY:
	case alphaGen_t::AGEN_CONST:
		mode |= GPU_MD3_COLOR_UNIFORM_ALPHA;
		return true;

	case alphaGen_t::AGEN_WAVEFORM:
		if (!VK_GpuMd3SupportsWaveAlpha(b0))
			return false;
		mode |= GPU_MD3_COLOR_UNIFORM_ALPHA;
		return true;

	case alphaGen_t::AGEN_LIGHTING_SPECULAR:
		mode |= GPU_MD3_COLOR_UNIFORM_SPECULAR_ALPHA;
		return true;

	case alphaGen_t::AGEN_ENTITY:
	case alphaGen_t::AGEN_ONE_MINUS_ENTITY:
		if (backEnd.currentEntity)
		{
			mode |= GPU_MD3_COLOR_UNIFORM_ALPHA;
			return true;
		}
		return false;

	case alphaGen_t::AGEN_PORTAL:
		mode |= GPU_MD3_COLOR_PORTAL_ALPHA;
		return true;

	default:
		return false;
	}
}

static ID_INLINE uint32_t VK_GpuMd3BuildSecondaryColorMode(const textureBundle_t& bundle) noexcept
{
	uint32_t mode = 0u;

	switch (bundle.rgbGen)
	{
	case colorGen_t::CGEN_IDENTITY:
	case colorGen_t::CGEN_IDENTITY_LIGHTING:
	case colorGen_t::CGEN_CONST:
	case colorGen_t::CGEN_ENTITY:
	case colorGen_t::CGEN_ONE_MINUS_ENTITY:
	case colorGen_t::CGEN_WAVEFORM:
	case colorGen_t::CGEN_FOG:
		mode |= GPU_MD3_COLOR_UNIFORM_SOLID_RGBA;
		break;

	case colorGen_t::CGEN_VERTEX:
		mode |= GPU_MD3_COLOR_VERTEX_RGB;
		break;

	case colorGen_t::CGEN_ONE_MINUS_VERTEX:
		mode |= GPU_MD3_COLOR_ONE_MINUS_VERTEX_RGB;
		break;

	case colorGen_t::CGEN_EXACT_VERTEX:
		mode |= GPU_MD3_COLOR_EXACT_VERTEX_RGB;
		break;

	default:
		return 0u;
	}

	if (!VK_GpuMd3VertexAlphaSupported(mode, bundle))
		return 0u;

	if ((mode & GPU_MD3_COLOR_UNIFORM_SPECULAR_ALPHA) != 0u)
		return 0u;

	return mode;
}

uint32_t R_GpuMd3SecondaryColorMode(const shaderStage_t& stage, uint32_t bundleIndex) noexcept
{
	if (!tess.gpuMd3Active)
		return 0u;

	if (bundleIndex >= static_cast<uint32_t>(stage.numTexBundles))
		return 0u;

	if (bundleIndex == 0u)
	{
		const gpuMd3Layout_t stageLayout = VK_GpuMd3LayoutForStage(stage);
		if (stageLayout != gpuMd3Layout_t::GENERIC_ST_COLOR &&
			stageLayout != gpuMd3Layout_t::GENERIC_ENV_COLOR)
			return 0u;

		const textureBundle_t& b0 = stage.bundle[0];
		uint32_t mode = 0u;

		switch (b0.rgbGen)
		{
		case colorGen_t::CGEN_IDENTITY:
		case colorGen_t::CGEN_IDENTITY_LIGHTING:
		case colorGen_t::CGEN_CONST:
		case colorGen_t::CGEN_ENTITY:
		case colorGen_t::CGEN_ONE_MINUS_ENTITY:
		case colorGen_t::CGEN_WAVEFORM:
		case colorGen_t::CGEN_FOG:
			mode |= GPU_MD3_COLOR_UNIFORM_SOLID_RGBA;
			break;

		case colorGen_t::CGEN_VERTEX:
			mode |= GPU_MD3_COLOR_VERTEX_RGB;
			break;

		case colorGen_t::CGEN_ONE_MINUS_VERTEX:
			mode |= GPU_MD3_COLOR_ONE_MINUS_VERTEX_RGB;
			break;

		case colorGen_t::CGEN_EXACT_VERTEX:
			mode |= GPU_MD3_COLOR_EXACT_VERTEX_RGB;
			break;

		case colorGen_t::CGEN_LIGHTING_DIFFUSE:
			mode |= GPU_MD3_COLOR_UNIFORM_DIFFUSE_RGB;
			break;

		default:
			return 0u;
		}

		if (!VK_GpuMd3VertexAlphaSupported(mode, b0))
			return 0u;

		return mode;
	}

	return VK_GpuMd3BuildSecondaryColorMode(stage.bundle[bundleIndex]);
}

bool R_GpuMd3ColorModeUsesRawVertexColor(uint32_t mode) noexcept
{
	return (mode &
		(GPU_MD3_COLOR_VERTEX_RGB |
		 GPU_MD3_COLOR_ONE_MINUS_VERTEX_RGB |
		 GPU_MD3_COLOR_EXACT_VERTEX_RGB |
		 GPU_MD3_COLOR_ONE_MINUS_VERTEX_ALPHA |
		 GPU_MD3_COLOR_VERTEX_ALPHA)) != 0u;
}

bool R_GpuMd3SecondaryColorHandledInShader(const shaderStage_t& stage, int bundleIndex, const textureBundle_t& bundle) noexcept
{
	if (!tess.gpuMd3Active)
		return false;

	if (bundleIndex <= 0 || bundleIndex >= stage.numTexBundles)
		return false;

	const gpuMd3Layout_t stageLayout = VK_GpuMd3LayoutForStage(stage);
	if (stageLayout != gpuMd3Layout_t::GENERIC_ST_COLOR &&
		stageLayout != gpuMd3Layout_t::GENERIC_ENV_COLOR)
		return false;

	if ((stage.tessFlags & (TESS_ST0 << bundleIndex)) == 0)
		return false;

	if ((stage.tessFlags & (TESS_RGBA0 << bundleIndex)) == 0)
		return false;

	if (!bundle.image[0])
		return false;

	return R_GpuMd3SecondaryColorMode(stage, static_cast<uint32_t>(bundleIndex)) != 0u;
}

static ID_INLINE bool VK_GpuMd3UsesRawVertexColor(const shaderStage_t& stage, const uint32_t bundleIndex) noexcept
{
	return R_GpuMd3ColorModeUsesRawVertexColor(R_GpuMd3SecondaryColorMode(stage, bundleIndex));
}

static ID_INLINE void VK_BuildGpuMd3SolidColorForBundle(vec4_t rgba, const textureBundle_t& b0) noexcept
{
	rgba[0] = 1.0f;
	rgba[1] = 1.0f;
	rgba[2] = 1.0f;
	rgba[3] = 1.0f;

	constexpr float kInv255 = 1.0f / 255.0f;

	switch (b0.rgbGen)
	{
	case colorGen_t::CGEN_IDENTITY:
		rgba[0] = 1.0f;
		rgba[1] = 1.0f;
		rgba[2] = 1.0f;
		rgba[3] = 1.0f;
		break;

	case colorGen_t::CGEN_IDENTITY_LIGHTING:
		rgba[0] = tr.identityLight;
		rgba[1] = tr.identityLight;
		rgba[2] = tr.identityLight;
		rgba[3] = tr.identityLight;
		break;

	case colorGen_t::CGEN_CONST:
		rgba[0] = b0.constantColor.rgba[0] * kInv255;
		rgba[1] = b0.constantColor.rgba[1] * kInv255;
		rgba[2] = b0.constantColor.rgba[2] * kInv255;
		rgba[3] = b0.constantColor.rgba[3] * kInv255;
		break;

	case colorGen_t::CGEN_ENTITY:
		if (backEnd.currentEntity)
		{
			rgba[0] = backEnd.currentEntity->e.shader.rgba[0] * kInv255;
			rgba[1] = backEnd.currentEntity->e.shader.rgba[1] * kInv255;
			rgba[2] = backEnd.currentEntity->e.shader.rgba[2] * kInv255;
			rgba[3] = backEnd.currentEntity->e.shader.rgba[3] * kInv255;
		}
		break;

	case colorGen_t::CGEN_ONE_MINUS_ENTITY:
		if (backEnd.currentEntity)
		{
			rgba[0] = (255.0f - backEnd.currentEntity->e.shader.rgba[0]) * kInv255;
			rgba[1] = (255.0f - backEnd.currentEntity->e.shader.rgba[1]) * kInv255;
			rgba[2] = (255.0f - backEnd.currentEntity->e.shader.rgba[2]) * kInv255;
			rgba[3] = (255.0f - backEnd.currentEntity->e.shader.rgba[3]) * kInv255;
		}
		break;

	case colorGen_t::CGEN_WAVEFORM:
	{
		const float glow = VK_EvalGpuMd3WaveRgb(b0.rgbWave);
		rgba[0] = glow;
		rgba[1] = glow;
		rgba[2] = glow;
		rgba[3] = 1.0f;
	}
	break;

	case colorGen_t::CGEN_FOG:
		if (tr.world && tess.fogNum > 0)
		{
			const fog_t& fog = tr.world->fogs[tess.fogNum];
			rgba[0] = fog.color[0];
			rgba[1] = fog.color[1];
			rgba[2] = fog.color[2];
			rgba[3] = fog.color[3];
		}
		break;

	default:
		break;
	}

	float alpha = 1.0f;
	if (VK_TryBuildGpuMd3UniformAlpha(alpha, b0))
	{
		rgba[3] = alpha;
	}
}

static ID_INLINE void VK_BuildGpuMd3SolidColor(vec4_t rgba, const shaderStage_t& stage) noexcept
{
	VK_BuildGpuMd3SolidColorForBundle(rgba, stage.bundle[0]);
}

static void VK_SetGpuMd3ColorParams(vkUniform_t& uniform, const shaderStage_t& stage) noexcept
{
	const uint32_t colorMode = R_GpuMd3SecondaryColorMode(stage, 0u);

	uniform.colorMode01[0] = static_cast<float>(R_GpuMd3SecondaryColorMode(stage, 1u));
	uniform.colorMode01[1] = static_cast<float>(R_GpuMd3SecondaryColorMode(stage, 2u));
	uniform.colorMode01[2] = 0.0f;
	uniform.colorMode01[3] = 0.0f;
	Vector4Set(uniform.color1Fixed, 1.0f, 1.0f, 1.0f, 1.0f);
	Vector4Set(uniform.color2Fixed, 1.0f, 1.0f, 1.0f, 1.0f);

	// Regular generic MD3 shader reads color mode from light.pos.w.
	// ENV MD3 shader keeps light.pos.w for env FP/screen-map semantics,
	// so it reads the mode from light.vector.w instead.
	uniform.light.vector[3] = static_cast<float>(colorMode);
	const gpuMd3Layout_t stageLayout = VK_GpuMd3LayoutForStage(stage);
	if (stageLayout != gpuMd3Layout_t::GENERIC_ENV_COLOR &&
		stageLayout != gpuMd3Layout_t::GENERIC_ENV_NO_COLOR)
	{
		uniform.light.pos[3] = static_cast<float>(colorMode);
	}
	uniform.light.color[3] = 0.0f;
	uniform.fogEyeT[2] = 0.0f;

	if ((stage.numTexBundles > 1) && (uniform.colorMode01[0] != 0.0f) &&
		((static_cast<uint32_t>(uniform.colorMode01[0]) & GPU_MD3_COLOR_UNIFORM_SOLID_RGBA) != 0u))
	{
		VK_BuildGpuMd3SolidColorForBundle(uniform.color1Fixed, stage.bundle[1]);
	}

	if ((stage.numTexBundles > 2) && (uniform.colorMode01[1] != 0.0f) &&
		((static_cast<uint32_t>(uniform.colorMode01[1]) & GPU_MD3_COLOR_UNIFORM_SOLID_RGBA) != 0u))
	{
		VK_BuildGpuMd3SolidColorForBundle(uniform.color2Fixed, stage.bundle[2]);
	}

	if (colorMode == 0u)
		return;

	if ((colorMode & GPU_MD3_COLOR_UNIFORM_SOLID_RGBA) != 0u)
	{
		vec4_t rgba{};
		VK_BuildGpuMd3SolidColor(rgba, stage);
		uniform.light.pos[0] = rgba[0];
		uniform.light.pos[1] = rgba[1];
		uniform.light.pos[2] = rgba[2];
		uniform.light.color[3] = rgba[3];
		if ((colorMode & GPU_MD3_COLOR_PORTAL_ALPHA) != 0u)
		{
			uniform.fogEyeT[2] = tess.shader ? tess.shader->portalRangeR : 0.0f;
		}
		return;
	}

	if ((colorMode & GPU_MD3_COLOR_UNIFORM_ALPHA) != 0u)
	{
		float alpha = 1.0f;
		if (VK_TryBuildGpuMd3UniformAlpha(alpha, stage.bundle[0]))
		{
			uniform.light.color[3] = alpha;
		}
	}

	if ((colorMode & GPU_MD3_COLOR_PORTAL_ALPHA) != 0u)
	{
		uniform.fogEyeT[2] = tess.shader ? tess.shader->portalRangeR : 0.0f;
	}

	if ((colorMode & GPU_MD3_COLOR_UNIFORM_DIFFUSE_RGB) == 0u || !backEnd.currentEntity)
		return;

	constexpr float kInv255 = 1.0f / 255.0f;
	const trRefEntity_t& ent = *backEnd.currentEntity;

	uniform.light.pos[0] = ent.ambientLight[0] * kInv255;
	uniform.light.pos[1] = ent.ambientLight[1] * kInv255;
	uniform.light.pos[2] = ent.ambientLight[2] * kInv255;

	uniform.light.color[0] = ent.directedLight[0] * kInv255;
	uniform.light.color[1] = ent.directedLight[1] * kInv255;
	uniform.light.color[2] = ent.directedLight[2] * kInv255;

	uniform.light.vector[0] = ent.lightDir[0];
	uniform.light.vector[1] = ent.lightDir[1];
	uniform.light.vector[2] = ent.lightDir[2];
}

static void RB_IterateStagesGeneric(const shaderCommands_t& input, const bool fogCollapse)
{
	int tess_flags;
	int stage;
	uint32_t i;
	uint32_t pipeline;
	int fog_stage = 0;
	bool pushUniform;

	vk_bind_index();

	tess_flags = input.shader->tessFlags;

	pushUniform = false;

#ifdef USE_FOG_COLLAPSE
	if (fogCollapse)
	{
		fog_stage = 1;
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

	RB_ResetStageTracking();

	for (stage = 0; stage < MAX_SHADER_STAGES; stage++)
	{
		const shaderStage_t* pStage = tess.xstages[stage];
		if (!pStage)
			break;

		RB_SetStageTracking(stage);

		tess_flags |= pStage->tessFlags;

		// nowe: zawsze ustaw bazowe tc parametry
		VK_SetIdentityTcParams(uniform);
		VK_SetIdentityGpuMd3DeformParams(uniform);
		VK_SetIdentityGpuMd3ColorParams(uniform);
		pushUniform = true;

		if (tess.gpuMd3Active)
		{
			VK_SetGpuMd3EnvParams(uniform, *pStage);
			VK_SetGpuMd3ColorParams(uniform, *pStage);
			VK_SetGpuMd3TcParamsForSlot(uniform, pStage->bundle[0], gpuTcSlot_t::bundle0);

			if (pStage->numTexBundles > 1 &&
				R_GpuMd3TexCoordsHandledInShader(*pStage, 1, pStage->bundle[1]))
			{
				VK_SetGpuMd3TcParamsForSlot(uniform, pStage->bundle[1], gpuTcSlot_t::bundle1);
			}

			if (pStage->numTexBundles > 2 &&
				R_GpuMd3TexCoordsHandledInShader(*pStage, 2, pStage->bundle[2]))
			{
				VK_SetGpuMd3TcParamsForSlot(uniform, pStage->bundle[2], gpuTcSlot_t::bundle2);
			}
			VK_SetGpuMd3DeformParams(uniform, *pStage);
			pushUniform = true;
		}

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
					const uint32_t gpuMd3ColorMode =
						tess.gpuMd3Active
						? R_GpuMd3SecondaryColorMode(*pStage, static_cast<uint32_t>(i))
						: 0u;

					if (gpuMd3ColorMode == 0u)
					{
						R_ComputeColors(i, tess.svars.colors[i], *pStage);
					}
				}
				if (tess_flags & (TESS_ENT0 << i) && backEnd.currentEntity)
				{
					const float entR = backEnd.currentEntity->e.shader.rgba[0] / 255.0f;
					const float entG = backEnd.currentEntity->e.shader.rgba[1] / 255.0f;
					const float entB = backEnd.currentEntity->e.shader.rgba[2] / 255.0f;
					const float entA = backEnd.currentEntity->e.shader.rgba[3] / 255.0f;

					if (pStage->bundle[i].rgbGen == colorGen_t::CGEN_ONE_MINUS_ENTITY)
					{
						uniform.ent.color[i][0] = 1.0f - entR;
						uniform.ent.color[i][1] = 1.0f - entG;
						uniform.ent.color[i][2] = 1.0f - entB;
					}
					else
					{
						uniform.ent.color[i][0] = entR;
						uniform.ent.color[i][1] = entG;
						uniform.ent.color[i][2] = entB;
					}

					switch (pStage->bundle[i].alphaGen)
					{
					case alphaGen_t::AGEN_IDENTITY:
						uniform.ent.color[i][3] = 1.0f;
						break;

					case alphaGen_t::AGEN_CONST:
						uniform.ent.color[i][3] = pStage->bundle[i].constantColor.rgba[3] / 255.0f;
						break;

					case alphaGen_t::AGEN_ONE_MINUS_ENTITY:
						uniform.ent.color[i][3] = 1.0f - entA;
						break;

					case alphaGen_t::AGEN_SKIP:
						uniform.ent.color[i][3] =
							(pStage->bundle[i].rgbGen == colorGen_t::CGEN_ONE_MINUS_ENTITY) ? (1.0f - entA) : entA;
						break;

					case alphaGen_t::AGEN_ENTITY:
					default:
						uniform.ent.color[i][3] = entA;
						break;
					}

					pushUniform = true;
				}
			}
		}

		//if (tess.gpuMd3Active && (pStage->tessFlags & TESS_ENV))
		//{
		//	VK_SetGpuMd3EnvParams(uniform, *pStage);
		//	pushUniform = true;
		//}

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

	// Po zakończeniu generic passów nie zostawiaj aktywnego stage-derived stanu
	// dla późniejszych fog/dlight/debug drawów.
	RB_ResetStageTracking();

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
	float* texCoords;
	byte* colors;
	byte clipBits[SHADER_MAX_VERTEXES]{};
	uint32_t pipeline;

	glIndex_t hitIndexes[SHADER_MAX_INDEXES]{};
	int numIndexes;
	float scale;
	float radius;
	float modulate = 0.0f;

	const shaderStage_t* const gpuMd3Stage =
		(tess.gpuMd3Active && tess.xstages && tess.numPasses > 0)
		? tess.xstages[0]
		: nullptr;

	for (l = 0; l < backEnd.refdef.num_dlights; l++)
	{
		if (!(tess.dlightBits & (1 << l)))
		{
			continue; // this surface definitely doesn't have any of this light
		}

		const dlight_t& dl = backEnd.refdef.dlights[l];

		if (tess.gpuMd3Active)
		{
			Bind(tr.dlightImage);
			VK_SetLegacyGpuMd3DlightParams(uniform, dl, gpuMd3Stage);
			VK_PushUniform(uniform);

			pipeline = vk_inst.dlight_md3_pipelines[dl.additive > 0 ? 1 : 0][static_cast<int>(tess.shader->cullType)][tess.shader->polygonOffset];
			vk_bind_pipeline(pipeline);
			vk_bind_index();
			vk_bind_geometry(TESS_ST0 | TESS_NNN);
			vk_draw_geometry(Vk_Depth_Range::DEPTH_RANGE_NORMAL, true);

			backEnd.pc.c_dlightVertexes += tess.numVertexes;
			backEnd.pc.c_totalIndexes += tess.numIndexes;
			backEnd.pc.c_dlightIndexes += tess.numIndexes;
			continue;
		}

		texCoords = (float*)&tess.svars.texcoords[0][0];
		tess.svars.texcoordPtr[0] = tess.svars.texcoords[0];
		colors = tess.svars.colors[0][0].rgba;
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
	int fog_stage = 0;

	// fog parameters
	vk_bind_pipeline(pipeline);
	if (rebindIndex)
	{
		vk_bind_index();
	}
	vk_bind_geometry(TESS_XYZ);
	VK_SetFogParams(uniform, fog_stage);
	VK_PushUniform(uniform);
	vk_update_descriptor(VK_DESC_FOG_ONLY, tr.fogImage->descriptor);
	vk_draw_geometry(Vk_Depth_Range::DEPTH_RANGE_NORMAL, true);
#else
	const fog_t* fog = tr.world->fogs + tess.fogNum;
	int i;

	for (i = 0; i < tess.numVertexes; i++)
	{
		tess.svars.colors[0][i] = fog->colorInt;
	}

	RB_CalcFogTexCoords((float*)tess.svars.texcoords[0]);
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
		RB_ResetStageTracking();
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
static void DrawTris(const shaderCommands_t& input)
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
static void DrawNormals(const shaderCommands_t& input)
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
	const shaderCommands_t& input = tess;

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

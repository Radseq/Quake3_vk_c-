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
#include "string_operations.hpp"
#include "tr_shader.hpp"
#include "tr_bsp.hpp"
#include "tr_main.hpp"
#include "tr_shade.hpp"
#include "tr_sky.hpp"
#include "vk.hpp"
#include "tr_image.hpp"
#include "math.hpp"
#include "utils.hpp"

#define generateHashValue Com_GenerateHashValue_cpp

static char *s_shaderText;

static const char *s_extensionOffset;
static int s_extendedShader;

// the shader is parsed into these global variables, then copied into
// dynamically allocated memory if it is valid.
static shaderStage_t stages[MAX_SHADER_STAGES];
static shader_t shader;
static texModInfo_t texMods[MAX_SHADER_STAGES][TR_MAX_TEXMODS + 1]; // reserve one additional texmod for lightmap atlas correction

constexpr int FILE_HASH_SIZE = 1024;
static std::array<shader_t *, FILE_HASH_SIZE> shaderHashTable;

constexpr int MAX_SHADERTEXT_HASH = 2048;
static std::array<const char **, MAX_SHADERTEXT_HASH> shaderTextHashTable;

// tr_shader.c -- this file deals with the parsing and definition of shaders

constexpr collapse_t collapse[] = {
	{0, GLS_DSTBLEND_SRC_COLOR | GLS_SRCBLEND_ZERO, GL_MODULATE, 0},
	{0, GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR, GL_MODULATE, 0},
	{GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR, GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR, GL_MODULATE, GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR},
	{GLS_DSTBLEND_SRC_COLOR | GLS_SRCBLEND_ZERO, GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR, GL_MODULATE, GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR},
	{GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR, GLS_DSTBLEND_SRC_COLOR | GLS_SRCBLEND_ZERO, GL_MODULATE, GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR},
	{GLS_DSTBLEND_SRC_COLOR | GLS_SRCBLEND_ZERO, GLS_DSTBLEND_SRC_COLOR | GLS_SRCBLEND_ZERO, GL_MODULATE, GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR},
	{0, GLS_DSTBLEND_ONE | GLS_SRCBLEND_ONE, GL_ADD, 0},
	{GLS_DSTBLEND_ONE | GLS_SRCBLEND_ONE, GLS_DSTBLEND_ONE | GLS_SRCBLEND_ONE, GL_ADD, GLS_DSTBLEND_ONE | GLS_SRCBLEND_ONE},
	{GLS_DSTBLEND_ONE | GLS_SRCBLEND_SRC_ALPHA, GLS_DSTBLEND_ONE | GLS_SRCBLEND_SRC_ALPHA, GL_BLEND_ALPHA, GLS_DSTBLEND_ONE | GLS_SRCBLEND_ONE},
	{GLS_DSTBLEND_ONE | GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA, GLS_DSTBLEND_ONE | GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA, GL_BLEND_ONE_MINUS_ALPHA, GLS_DSTBLEND_ONE | GLS_SRCBLEND_ONE},
	{0, GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_SRCBLEND_SRC_ALPHA, GL_BLEND_MIX_ALPHA, 0},
	{0, GLS_DSTBLEND_SRC_ALPHA | GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA, GL_BLEND_MIX_ONE_MINUS_ALPHA, 0},
	{0, GLS_DSTBLEND_SRC_ALPHA | GLS_SRCBLEND_DST_COLOR, GL_BLEND_DST_COLOR_SRC_ALPHA, 0},
#if 0
	{ 0, GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_SRCBLEND_SRC_ALPHA, GL_DECAL, 0 },
#endif
	{-1}};

void RE_RemapShader(const char *shaderName, const char *newShaderName, const char *timeOffset)
{
	std::array<char, MAX_QPATH> strippedName;
	int hash;
	shader_t *sh, *sh2;
	qhandle_t h;

	sh = R_FindShaderByName(shaderName);
	if (sh == NULL || sh == tr.defaultShader)
	{
		h = RE_RegisterShaderLightMap(shaderName, 0);
		sh = R_GetShaderByHandle(h);
	}
	if (sh == NULL || sh == tr.defaultShader)
	{
		ri.Printf(PRINT_WARNING, "WARNING: RE_RemapShader: shader %s not found\n", shaderName);
		return;
	}

	sh2 = R_FindShaderByName(newShaderName);
	if (sh2 == NULL || sh2 == tr.defaultShader)
	{
		h = RE_RegisterShaderLightMap(newShaderName, 0);
		sh2 = R_GetShaderByHandle(h);
	}

	if (sh2 == NULL || sh2 == tr.defaultShader)
	{
		ri.Printf(PRINT_WARNING, "WARNING: RE_RemapShader: new shader %s not found\n", newShaderName);
		return;
	}

	// remap all the shaders with the given name
	// even tho they might have different lightmaps
	COM_StripExtension_cpp(shaderName, strippedName);
	hash = generateHashValue(to_str_view(strippedName), FILE_HASH_SIZE);
	for (sh = shaderHashTable[hash]; sh; sh = sh->next)
	{
		if (Q_stricmp_cpp(sh->name, strippedName.data()) == 0)
		{
			if (sh != sh2)
			{
				sh->remappedShader = sh2;
			}
			else
			{
				sh->remappedShader = NULL;
			}
		}
	}

	if (timeOffset)
	{
		sh2->timeOffset = Q_atof_cpp(timeOffset);
	}
}

/*
===============
ParseVector
===============
*/
static bool ParseVector(const char **text, const int count, float *v)
{
	std::string_view token;
	int i;

	// FIXME: spaces are currently required after parens, should change parseext...
	token = COM_ParseExt_cpp(text, false);
	if (token != "(")
	{
		ri.Printf(PRINT_WARNING, "WARNING: missing parenthesis in shader '%s'\n", shader.name);
		return false;
	}

	for (i = 0; i < count; i++)
	{
		token = COM_ParseExt_cpp(text, false);
		if (!token[0])
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing vector element in shader '%s'\n", shader.name);
			return false;
		}
		v[i] = Q_atof_cpp(token);
	}

	token = COM_ParseExt_cpp(text, false);
	if (token != ")")
	{
		ri.Printf(PRINT_WARNING, "WARNING: missing parenthesis in shader '%s'\n", shader.name);
		return false;
	}

	return true;
}

/*
===============
NameToAFunc
===============
*/
static unsigned int NameToAFunc(std::string_view funcname)
{
	if (!Q_stricmp_cpp(funcname, "GT0"))
	{
		return GLS_ATEST_GT_0;
	}
	else if (!Q_stricmp_cpp(funcname, "LT128"))
	{
		return GLS_ATEST_LT_80;
	}
	else if (!Q_stricmp_cpp(funcname, "GE128"))
	{
		return GLS_ATEST_GE_80;
	}

	ri.Printf(PRINT_WARNING, "WARNING: invalid alphaFunc name '%s' in shader '%s'\n", funcname.data(), shader.name);
	return 0;
}

/*
===============
NameToSrcBlendMode
===============
*/
static int NameToSrcBlendMode(std::string_view name)
{
	if (!Q_stricmp_cpp(name, "GL_ONE"))
	{
		return GLS_SRCBLEND_ONE;
	}
	else if (!Q_stricmp_cpp(name, "GL_ZERO"))
	{
		return GLS_SRCBLEND_ZERO;
	}
	else if (!Q_stricmp_cpp(name, "GL_DST_COLOR"))
	{
		return GLS_SRCBLEND_DST_COLOR;
	}
	else if (!Q_stricmp_cpp(name, "GL_ONE_MINUS_DST_COLOR"))
	{
		return GLS_SRCBLEND_ONE_MINUS_DST_COLOR;
	}
	else if (!Q_stricmp_cpp(name, "GL_SRC_ALPHA"))
	{
		return GLS_SRCBLEND_SRC_ALPHA;
	}
	else if (!Q_stricmp_cpp(name, "GL_ONE_MINUS_SRC_ALPHA"))
	{
		return GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA;
	}
	else if (!Q_stricmp_cpp(name, "GL_DST_ALPHA"))
	{
		return GLS_SRCBLEND_DST_ALPHA;
	}
	else if (!Q_stricmp_cpp(name, "GL_ONE_MINUS_DST_ALPHA"))
	{
		return GLS_SRCBLEND_ONE_MINUS_DST_ALPHA;
	}
	else if (!Q_stricmp_cpp(name, "GL_SRC_ALPHA_SATURATE"))
	{
		return GLS_SRCBLEND_ALPHA_SATURATE;
	}

	ri.Printf(PRINT_WARNING, "WARNING: unknown blend mode '%s' in shader '%s', substituting GL_ONE\n", name.data(), shader.name);
	return GLS_SRCBLEND_ONE;
}

/*
===============
NameToDstBlendMode
===============
*/
static int NameToDstBlendMode(std::string_view name)
{
	if (!Q_stricmp_cpp(name, "GL_ONE"))
	{
		return GLS_DSTBLEND_ONE;
	}
	else if (!Q_stricmp_cpp(name, "GL_ZERO"))
	{
		return GLS_DSTBLEND_ZERO;
	}
	else if (!Q_stricmp_cpp(name, "GL_SRC_ALPHA"))
	{
		return GLS_DSTBLEND_SRC_ALPHA;
	}
	else if (!Q_stricmp_cpp(name, "GL_ONE_MINUS_SRC_ALPHA"))
	{
		return GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
	}
	else if (!Q_stricmp_cpp(name, "GL_DST_ALPHA"))
	{
		return GLS_DSTBLEND_DST_ALPHA;
	}
	else if (!Q_stricmp_cpp(name, "GL_ONE_MINUS_DST_ALPHA"))
	{
		return GLS_DSTBLEND_ONE_MINUS_DST_ALPHA;
	}
	else if (!Q_stricmp_cpp(name, "GL_SRC_COLOR"))
	{
		return GLS_DSTBLEND_SRC_COLOR;
	}
	else if (!Q_stricmp_cpp(name, "GL_ONE_MINUS_SRC_COLOR"))
	{
		return GLS_DSTBLEND_ONE_MINUS_SRC_COLOR;
	}

	ri.Printf(PRINT_WARNING, "WARNING: unknown blend mode '%s' in shader '%s', substituting GL_ONE\n", name.data(), shader.name);
	return GLS_DSTBLEND_ONE;
}

/*
===============
NameToGenFunc
===============
*/
static genFunc_t NameToGenFunc(std::string_view funcname)
{
	if (!Q_stricmp_cpp(funcname, "sin"))
	{
		return genFunc_t::GF_SIN;
	}
	else if (!Q_stricmp_cpp(funcname, "square"))
	{
		return genFunc_t::GF_SQUARE;
	}
	else if (!Q_stricmp_cpp(funcname, "triangle"))
	{
		return genFunc_t::GF_TRIANGLE;
	}
	else if (!Q_stricmp_cpp(funcname, "sawtooth"))
	{
		return genFunc_t::GF_SAWTOOTH;
	}
	else if (!Q_stricmp_cpp(funcname, "inversesawtooth"))
	{
		return genFunc_t::GF_INVERSE_SAWTOOTH;
	}
	else if (!Q_stricmp_cpp(funcname, "noise"))
	{
		return genFunc_t::GF_NOISE;
	}

	ri.Printf(PRINT_WARNING, "WARNING: invalid genfunc name '%s' in shader '%s'\n", funcname.data(), shader.name);
	return genFunc_t::GF_SIN;
}

/*
===================
ParseWaveForm
===================
*/
static void ParseWaveForm(const char **text, waveForm_t &wave)
{
	std::string_view token;

	token = COM_ParseExt_cpp(text, false);
	if (token.empty())
	{
		ri.Printf(PRINT_WARNING, "WARNING: missing waveform parm in shader '%s'\n", shader.name);
		return;
	}
	wave.func = NameToGenFunc(token);

	// BASE, AMP, PHASE, FREQ
	token = COM_ParseExt_cpp(text, false);
	if (token.empty())
	{
		ri.Printf(PRINT_WARNING, "WARNING: missing waveform parm in shader '%s'\n", shader.name);
		return;
	}
	wave.base = Q_atof_cpp(token);

	token = COM_ParseExt_cpp(text, false);
	if (token.empty())
	{
		ri.Printf(PRINT_WARNING, "WARNING: missing waveform parm in shader '%s'\n", shader.name);
		return;
	}
	wave.amplitude = Q_atof_cpp(token);

	token = COM_ParseExt_cpp(text, false);
	if (token.empty())
	{
		ri.Printf(PRINT_WARNING, "WARNING: missing waveform parm in shader '%s'\n", shader.name);
		return;
	}
	wave.phase = Q_atof_cpp(token);

	token = COM_ParseExt_cpp(text, false);
	if (token.empty())
	{
		ri.Printf(PRINT_WARNING, "WARNING: missing waveform parm in shader '%s'\n", shader.name);
		return;
	}
	wave.frequency = Q_atof_cpp(token);
}

/*
===================
ParseTexMod
===================
*/
static void ParseTexMod(const char *_text, shaderStage_t &stage)
{
	const char **text = &_text;

	if (stage.bundle[0].numTexMods == TR_MAX_TEXMODS)
	{
		ri.Error(ERR_DROP, "ERROR: too many tcMod stages in shader '%s'", shader.name);
		return;
	}

	texModInfo_t &tmi = stage.bundle[0].texMods[stage.bundle[0].numTexMods];
	stage.bundle[0].numTexMods++;

	std::string_view token = COM_ParseExt_cpp(text, false);

	//
	// turb
	//
	if (!Q_stricmp_cpp(token, "turb"))
	{
		token = COM_ParseExt_cpp(text, false);
		if (token.empty())
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing tcMod turb parms in shader '%s'\n", shader.name);
			return;
		}
		tmi.wave.base = Q_atof_cpp(token);
		token = COM_ParseExt_cpp(text, false);
		if (token.empty())
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing tcMod turb in shader '%s'\n", shader.name);
			return;
		}
		tmi.wave.amplitude = Q_atof_cpp(token);
		token = COM_ParseExt_cpp(text, false);
		if (token.empty())
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing tcMod turb in shader '%s'\n", shader.name);
			return;
		}
		tmi.wave.phase = Q_atof_cpp(token);
		token = COM_ParseExt_cpp(text, false);
		if (token.empty())
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing tcMod turb in shader '%s'\n", shader.name);
			return;
		}
		tmi.wave.frequency = Q_atof_cpp(token);

		tmi.type = texMod_t::TMOD_TURBULENT;
	}
	//
	// scale
	//
	else if (!Q_stricmp_cpp(token, "scale"))
	{
		token = COM_ParseExt_cpp(text, false);
		if (token.empty())
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing scale parms in shader '%s'\n", shader.name);
			return;
		}
		tmi.scale[0] = Q_atof_cpp(token);

		token = COM_ParseExt_cpp(text, false);
		if (token.empty())
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing scale parms in shader '%s'\n", shader.name);
			return;
		}
		tmi.scale[1] = Q_atof_cpp(token);
		tmi.type = texMod_t::TMOD_SCALE;
	}
	//
	// scroll
	//
	else if (!Q_stricmp_cpp(token, "scroll"))
	{
		token = COM_ParseExt_cpp(text, false);
		if (token.empty())
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing scale scroll parms in shader '%s'\n", shader.name);
			return;
		}
		tmi.scroll[0] = Q_atof_cpp(token);
		token = COM_ParseExt_cpp(text, false);
		if (token.empty())
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing scale scroll parms in shader '%s'\n", shader.name);
			return;
		}
		tmi.scroll[1] = Q_atof_cpp(token);
		tmi.type = texMod_t::TMOD_SCROLL;
	}
	//
	// stretch
	//
	else if (!Q_stricmp_cpp(token, "stretch"))
	{
		token = COM_ParseExt_cpp(text, false);
		if (token.empty())
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing stretch parms in shader '%s'\n", shader.name);
			return;
		}
		tmi.wave.func = NameToGenFunc(token);

		token = COM_ParseExt_cpp(text, false);
		if (token.empty())
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing stretch parms in shader '%s'\n", shader.name);
			return;
		}
		tmi.wave.base = Q_atof_cpp(token);

		token = COM_ParseExt_cpp(text, false);
		if (token.empty())
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing stretch parms in shader '%s'\n", shader.name);
			return;
		}
		tmi.wave.amplitude = Q_atof_cpp(token);

		token = COM_ParseExt_cpp(text, false);
		if (token.empty())
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing stretch parms in shader '%s'\n", shader.name);
			return;
		}
		tmi.wave.phase = Q_atof_cpp(token);

		token = COM_ParseExt_cpp(text, false);
		if (token.empty())
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing stretch parms in shader '%s'\n", shader.name);
			return;
		}
		tmi.wave.frequency = Q_atof_cpp(token);

		tmi.type = texMod_t::TMOD_STRETCH;
	}
	//
	// transform
	//
	else if (!Q_stricmp_cpp(token, "transform"))
	{
		token = COM_ParseExt_cpp(text, false);
		if (token.empty())
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing transform parms in shader '%s'\n", shader.name);
			return;
		}
		tmi.matrix[0][0] = Q_atof_cpp(token);

		token = COM_ParseExt_cpp(text, false);
		if (token.empty())
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing transform parms in shader '%s'\n", shader.name);
			return;
		}
		tmi.matrix[0][1] = Q_atof_cpp(token);

		token = COM_ParseExt_cpp(text, false);
		if (token.empty())
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing transform parms in shader '%s'\n", shader.name);
			return;
		}
		tmi.matrix[1][0] = Q_atof_cpp(token);

		token = COM_ParseExt_cpp(text, false);
		if (token.empty())
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing transform parms in shader '%s'\n", shader.name);
			return;
		}
		tmi.matrix[1][1] = Q_atof_cpp(token);

		token = COM_ParseExt_cpp(text, false);
		if (token.empty())
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing transform parms in shader '%s'\n", shader.name);
			return;
		}
		tmi.translate[0] = Q_atof_cpp(token);

		token = COM_ParseExt_cpp(text, false);
		if (token.empty())
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing transform parms in shader '%s'\n", shader.name);
			return;
		}
		tmi.translate[1] = Q_atof_cpp(token);

		tmi.type = texMod_t::TMOD_TRANSFORM;
	}
	//
	// rotate
	//
	else if (!Q_stricmp_cpp(token, "rotate"))
	{
		token = COM_ParseExt_cpp(text, false);
		if (token.empty())
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing tcMod rotate parms in shader '%s'\n", shader.name);
			return;
		}
		tmi.rotateSpeed = Q_atof_cpp(token);
		tmi.type = texMod_t::TMOD_ROTATE;
	}
	//
	// entityTranslate
	//
	else if (!Q_stricmp_cpp(token, "entityTranslate"))
	{
		tmi.type = texMod_t::TMOD_ENTITY_TRANSLATE;
	}
	else
	{
		ri.Printf(PRINT_WARNING, "WARNING: unknown tcMod '%s' in shader '%s'\n", token.data(), shader.name);
	}
}

/*
===================
ParseStage
===================
*/
static bool ParseStage(shaderStage_t &stage, const char **text)
{
	std::string_view token;
	int i, depthMaskBits = GLS_DEPTHMASK_TRUE, blendSrcBits = 0, blendDstBits = 0, atestBits = 0, depthFuncBits = 0;
	bool depthMaskExplicit = false;

	stage.active = false;

	while (1)
	{
		token = COM_ParseExt_cpp(text, true);

		if (token.empty())
		{
			ri.Printf(PRINT_WARNING, "WARNING: no matching '}' found\n");
			return false;
		}

		if (token[0] == '}')
		{
			break;
		}
		//
		// map <name>
		//
		else if (!Q_stricmp_cpp(token, "map"))
		{
			token = COM_ParseExt_cpp(text, false);
			if (token.empty())
			{
				ri.Printf(PRINT_WARNING, "WARNING: missing parameter for 'map' keyword in shader '%s'\n", shader.name);
				return false;
			}

			if (!Q_stricmp_cpp(token, "$whiteimage"))
			{
				stage.bundle[0].image[0] = tr.whiteImage;
				continue;
			}
			else if (!Q_stricmp_cpp(token, "$lightmap"))
			{
				stage.bundle[0].lightmap = LIGHTMAP_INDEX_SHADER; // regular lightmap
				if (shader.lightmapIndex < 0 || !tr.lightmaps)
				{
					stage.bundle[0].image[0] = tr.whiteImage;
				}
				else
				{
					stage.bundle[0].image[0] = tr.lightmaps[shader.lightmapIndex];
				}
				continue;
			}
			else if (Q_stricmpn_cpp(token, "*lightmap", 9) == 0 && token[9] >= '0' && token[9] <= '9')
			{
				std::string_view sub_token = token.substr(9);
				const int lightmapIndex = atoi_from_view(sub_token);
				if (lightmapIndex < 0 || tr.lightmaps == NULL)
				{
					stage.bundle[0].image[0] = tr.whiteImage;
				}
				else
				{
					stage.bundle[0].lightmap = LIGHTMAP_INDEX_OFFSET + lightmapIndex; // custom index
					stage.bundle[0].image[0] = tr.lightmaps[lightmapIndex % tr.lightmapMod];
				}
				continue;
			}
			else
			{
				imgFlags_t flags = imgFlags_t::IMGFLAG_NONE;

				if (!shader.noMipMaps)
					flags = static_cast<imgFlags_t>(flags | imgFlags_t::IMGFLAG_MIPMAP);

				if (!shader.noPicMip)
					flags = static_cast<imgFlags_t>(flags | imgFlags_t::IMGFLAG_PICMIP);

				if (shader.noLightScale)
					flags = static_cast<imgFlags_t>(flags | imgFlags_t::IMGFLAG_NOLIGHTSCALE);

				stage.bundle[0].image[0] = R_FindImageFile(token, flags);

				if (!stage.bundle[0].image[0])
				{
					ri.Printf(PRINT_WARNING, "WARNING: R_FindImageFile could not find '%s' in shader '%s'\n", token.data(), shader.name);
					return false;
				}
			}
		}
		//
		// clampmap <name>
		//
		else if (!Q_stricmp_cpp(token, "clampmap") || (!Q_stricmp_cpp(token, "screenMap") && s_extendedShader))
		{
			imgFlags_t flags;

			if (!Q_stricmp_cpp(token, "screenMap"))
			{
				flags = imgFlags_t::IMGFLAG_NONE;
				if (vk_inst.fboActive)
				{
					stage.bundle[0].isScreenMap = 1;
					shader.hasScreenMap = 1;
				}
			}
			else
			{
				flags = imgFlags_t::IMGFLAG_CLAMPTOEDGE;
			}

			token = COM_ParseExt_cpp(text, false);
			if (token.empty())
			{
				ri.Printf(PRINT_WARNING, "WARNING: missing parameter for '%s' keyword in shader '%s'\n",
						  stage.bundle[0].isScreenMap ? "screenMap" : "clampMap", shader.name);
				return false;
			}

			if (!shader.noMipMaps)
				flags = static_cast<imgFlags_t>(flags | imgFlags_t::IMGFLAG_MIPMAP);

			if (!shader.noPicMip)
				flags = static_cast<imgFlags_t>(flags | imgFlags_t::IMGFLAG_PICMIP);

			if (shader.noLightScale)
				flags = static_cast<imgFlags_t>(flags | imgFlags_t::IMGFLAG_NOLIGHTSCALE);

			stage.bundle[0].image[0] = R_FindImageFile(token, flags);

			if (!stage.bundle[0].image[0])
			{
				ri.Printf(PRINT_WARNING, "WARNING: R_FindImageFile could not find '%s' in shader '%s'\n", token.data(), shader.name);
				return false;
			}
		}
		//
		// animMap <frequency> <image1> .... <imageN>
		//
		else if (!Q_stricmp_cpp(token, "animMap"))
		{
			int totalImages = 0;
			int maxAnimations = s_extendedShader ? MAX_IMAGE_ANIMATIONS : MAX_IMAGE_ANIMATIONS_VQ3;

			token = COM_ParseExt_cpp(text, false);
			if (token.empty())
			{
				ri.Printf(PRINT_WARNING, "WARNING: missing parameter for 'animMap' keyword in shader '%s'\n", shader.name);
				return false;
			}
			stage.bundle[0].imageAnimationSpeed = Q_atof_cpp(token);

			imgFlags_t flags = imgFlags_t::IMGFLAG_NONE;

			if (!shader.noMipMaps)
				flags = static_cast<imgFlags_t>(flags | imgFlags_t::IMGFLAG_MIPMAP);

			if (!shader.noPicMip)
				flags = static_cast<imgFlags_t>(flags | imgFlags_t::IMGFLAG_PICMIP);

			if (shader.noLightScale)
				flags = static_cast<imgFlags_t>(flags | imgFlags_t::IMGFLAG_NOLIGHTSCALE);

			// parse up to MAX_IMAGE_ANIMATIONS animations
			while (1)
			{
				int num;

				token = COM_ParseExt_cpp(text, false);
				if (token.empty())
				{
					break;
				}
				num = stage.bundle[0].numImageAnimations;
				if (num < maxAnimations)
				{
					stage.bundle[0].image[num] = R_FindImageFile(token, flags);
					if (!stage.bundle[0].image[num])
					{
						ri.Printf(PRINT_WARNING, "WARNING: R_FindImageFile could not find '%s' in shader '%s'\n", token.data(), shader.name);
						return false;
					}
					stage.bundle[0].numImageAnimations++;
				}
				totalImages++;
			}

			if (totalImages > maxAnimations)
			{
				ri.Printf(PRINT_WARNING, "WARNING: ignoring excess images for 'animMap' (found %d, max is %d) in shader '%s'\n",
						  totalImages, maxAnimations, shader.name);
			}
		}
		else if (!Q_stricmp_cpp(token, "videoMap"))
		{
			int handle;
			token = COM_ParseExt_cpp(text, false);
			if (token.empty())
			{
				ri.Printf(PRINT_WARNING, "WARNING: missing parameter for 'videoMap' keyword in shader '%s'\n", shader.name);
				return false;
			}
			handle = ri.CIN_PlayCinematic(token.data(), 0, 0, 256, 256, (CIN_loop | CIN_silent | CIN_shader));
			if (handle != -1)
			{
				if (!tr.scratchImage[handle])
				{
					tr.scratchImage[handle] = R_CreateImage(va_cpp("*scratch%i", handle), {}, NULL, 256, 256, static_cast<imgFlags_t>(imgFlags_t::IMGFLAG_CLAMPTOEDGE | imgFlags_t::IMGFLAG_RGB | imgFlags_t::IMGFLAG_NOSCALE));
				}
				stage.bundle[0].isVideoMap = true;
				stage.bundle[0].videoMapHandle = handle;
				stage.bundle[0].image[0] = tr.scratchImage[handle];
			}
			else
			{
				ri.Printf(PRINT_WARNING, "WARNING: could not load '%s' for 'videoMap' keyword in shader '%s'\n", token.data(), shader.name);
			}
		}
		//
		// alphafunc <func>
		//
		else if (!Q_stricmp_cpp(token, "alphaFunc"))
		{
			token = COM_ParseExt_cpp(text, false);
			if (token.empty())
			{
				ri.Printf(PRINT_WARNING, "WARNING: missing parameter for 'alphaFunc' keyword in shader '%s'\n", shader.name);
				return false;
			}

			atestBits = NameToAFunc(token);
		}
		//
		// depthFunc <func>
		//
		else if (!Q_stricmp_cpp(token, "depthfunc"))
		{
			token = COM_ParseExt_cpp(text, false);

			if (token.empty())
			{
				ri.Printf(PRINT_WARNING, "WARNING: missing parameter for 'depthfunc' keyword in shader '%s'\n", shader.name);
				return false;
			}

			if (!Q_stricmp_cpp(token, "lequal"))
			{
				depthFuncBits = 0;
			}
			else if (!Q_stricmp_cpp(token, "equal"))
			{
				depthFuncBits = GLS_DEPTHFUNC_EQUAL;
			}
			else
			{
				ri.Printf(PRINT_WARNING, "WARNING: unknown depthfunc '%s' in shader '%s'\n", token.data(), shader.name);
				continue;
			}
		}
		//
		// detail
		//
		else if (!Q_stricmp_cpp(token, "detail"))
		{
			stage.isDetail = true;
		}
		//
		// blendfunc <srcFactor> <dstFactor>
		// or blendfunc <add|filter|blend>
		//
		else if (!Q_stricmp_cpp(token, "blendfunc"))
		{
			token = COM_ParseExt_cpp(text, false);
			if (token.empty())
			{
				ri.Printf(PRINT_WARNING, "WARNING: missing parm for blendFunc in shader '%s'\n", shader.name);
				continue;
			}
			// check for "simple" blends first
			if (!Q_stricmp_cpp(token, "add"))
			{
				blendSrcBits = GLS_SRCBLEND_ONE;
				blendDstBits = GLS_DSTBLEND_ONE;
			}
			else if (!Q_stricmp_cpp(token, "filter"))
			{
				blendSrcBits = GLS_SRCBLEND_DST_COLOR;
				blendDstBits = GLS_DSTBLEND_ZERO;
			}
			else if (!Q_stricmp_cpp(token, "blend"))
			{
				blendSrcBits = GLS_SRCBLEND_SRC_ALPHA;
				blendDstBits = GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
			}
			else
			{
				// complex double blends
				blendSrcBits = NameToSrcBlendMode(token);

				token = COM_ParseExt_cpp(text, false);
				if (token.empty())
				{
					ri.Printf(PRINT_WARNING, "WARNING: missing parm for blendFunc in shader '%s'\n", shader.name);
					blendDstBits = GLS_DSTBLEND_ONE; // by default
					continue;
				}
				blendDstBits = NameToDstBlendMode(token);
			}

			// clear depth mask for blended surfaces
			if (!depthMaskExplicit)
			{
				depthMaskBits = 0;
			}
		}
		//
		// rgbGen
		//
		else if (!Q_stricmp_cpp(token, "rgbGen"))
		{
			token = COM_ParseExt_cpp(text, false);
			if (token.empty())
			{
				ri.Printf(PRINT_WARNING, "WARNING: missing parameters for rgbGen in shader '%s'\n", shader.name);
				continue;
			}

			if (!Q_stricmp_cpp(token, "wave"))
			{
				ParseWaveForm(text, stage.bundle[0].rgbWave);
				stage.bundle[0].rgbGen = colorGen_t::CGEN_WAVEFORM;
			}
			else if (!Q_stricmp_cpp(token, "const"))
			{
				vec3_t color{};

				ParseVector(text, 3, color);
				stage.bundle[0].constantColor.rgba[0] = 255 * color[0];
				stage.bundle[0].constantColor.rgba[1] = 255 * color[1];
				stage.bundle[0].constantColor.rgba[2] = 255 * color[2];

				stage.bundle[0].rgbGen = colorGen_t::CGEN_CONST;
			}
			else if (!Q_stricmp_cpp(token, "identity"))
			{
				stage.bundle[0].rgbGen = colorGen_t::CGEN_IDENTITY;
			}
			else if (!Q_stricmp_cpp(token, "identityLighting"))
			{
				stage.bundle[0].rgbGen = colorGen_t::CGEN_IDENTITY_LIGHTING;
			}
			else if (!Q_stricmp_cpp(token, "entity"))
			{
				stage.bundle[0].rgbGen = colorGen_t::CGEN_ENTITY;
			}
			else if (!Q_stricmp_cpp(token, "oneMinusEntity"))
			{
				stage.bundle[0].rgbGen = colorGen_t::CGEN_ONE_MINUS_ENTITY;
			}
			else if (!Q_stricmp_cpp(token, "vertex"))
			{
				stage.bundle[0].rgbGen = colorGen_t::CGEN_VERTEX;
				if (stage.bundle[0].alphaGen == alphaGen_t::AGEN_IDENTITY)
				{
					stage.bundle[0].alphaGen = alphaGen_t::AGEN_VERTEX;
				}
			}
			else if (!Q_stricmp_cpp(token, "exactVertex"))
			{
				stage.bundle[0].rgbGen = colorGen_t::CGEN_EXACT_VERTEX;
			}
			else if (!Q_stricmp_cpp(token, "lightingDiffuse"))
			{
				stage.bundle[0].rgbGen = colorGen_t::CGEN_LIGHTING_DIFFUSE;
			}
			else if (!Q_stricmp_cpp(token, "oneMinusVertex"))
			{
				stage.bundle[0].rgbGen = colorGen_t::CGEN_ONE_MINUS_VERTEX;
			}
			else
			{
				ri.Printf(PRINT_WARNING, "WARNING: unknown rgbGen parameter '%s' in shader '%s'\n", token.data(), shader.name);
				continue;
			}
		}
		//
		// alphaGen
		//
		else if (!Q_stricmp_cpp(token, "alphaGen"))
		{
			token = COM_ParseExt_cpp(text, false);
			if (token.empty())
			{
				ri.Printf(PRINT_WARNING, "WARNING: missing parameters for alphaGen in shader '%s'\n", shader.name);
				continue;
			}

			if (!Q_stricmp_cpp(token, "wave"))
			{
				ParseWaveForm(text, stage.bundle[0].alphaWave);
				stage.bundle[0].alphaGen = alphaGen_t::AGEN_WAVEFORM;
			}
			else if (!Q_stricmp_cpp(token, "const"))
			{
				token = COM_ParseExt_cpp(text, false);
				stage.bundle[0].constantColor.rgba[3] = 255 * Q_atof_cpp(token);
				stage.bundle[0].alphaGen = alphaGen_t::AGEN_CONST;
			}
			else if (!Q_stricmp_cpp(token, "identity"))
			{
				stage.bundle[0].alphaGen = alphaGen_t::AGEN_IDENTITY;
			}
			else if (!Q_stricmp_cpp(token, "entity"))
			{
				stage.bundle[0].alphaGen = alphaGen_t::AGEN_ENTITY;
			}
			else if (!Q_stricmp_cpp(token, "oneMinusEntity"))
			{
				stage.bundle[0].alphaGen = alphaGen_t::AGEN_ONE_MINUS_ENTITY;
			}
			else if (!Q_stricmp_cpp(token, "vertex"))
			{
				stage.bundle[0].alphaGen = alphaGen_t::AGEN_VERTEX;
			}
			else if (!Q_stricmp_cpp(token, "lightingSpecular"))
			{
				stage.bundle[0].alphaGen = alphaGen_t::AGEN_LIGHTING_SPECULAR;
			}
			else if (!Q_stricmp_cpp(token, "oneMinusVertex"))
			{
				stage.bundle[0].alphaGen = alphaGen_t::AGEN_ONE_MINUS_VERTEX;
			}
			else if (!Q_stricmp_cpp(token, "portal"))
			{
				stage.bundle[0].alphaGen = alphaGen_t::AGEN_PORTAL;
				token = COM_ParseExt_cpp(text, false);
				if (token.empty())
				{
					shader.portalRange = 256;
					ri.Printf(PRINT_WARNING, "WARNING: missing range parameter for alphaGen portal in shader '%s', defaulting to 256\n", shader.name);
				}
				else
				{
					shader.portalRange = Q_atof_cpp(token);

					if (shader.portalRange < 0.001f)
						shader.portalRangeR = 0.0f;
					else
						shader.portalRangeR = 1.0f / shader.portalRange;
				}
			}
			else
			{
				ri.Printf(PRINT_WARNING, "WARNING: unknown alphaGen parameter '%s' in shader '%s'\n", token.data(), shader.name);
				continue;
			}
		}
		//
		// tcGen <function>
		//
		else if (!Q_stricmp_cpp(token, "texgen") || !Q_stricmp_cpp(token, "tcGen"))
		{
			token = COM_ParseExt_cpp(text, false);
			if (token.empty())
			{
				ri.Printf(PRINT_WARNING, "WARNING: missing texgen parm in shader '%s'\n", shader.name);
				continue;
			}

			if (!Q_stricmp_cpp(token, "environment"))
			{
				const char *t = *text;
				stage.bundle[0].tcGen = texCoordGen_t::TCGEN_ENVIRONMENT_MAPPED;
				token = COM_ParseExt_cpp(text, false);
				if (Q_stricmp_cpp(token, "firstPerson") == 0)
				{
					stage.bundle[0].tcGen = texCoordGen_t::TCGEN_ENVIRONMENT_MAPPED_FP;
				}
				else
				{
					*text = t; // rewind
				}
			}
			else if (!Q_stricmp_cpp(token, "lightmap"))
			{
				stage.bundle[0].tcGen = texCoordGen_t::TCGEN_LIGHTMAP;
			}
			else if (!Q_stricmp_cpp(token, "texture") || !Q_stricmp_cpp(token, "base"))
			{
				stage.bundle[0].tcGen = texCoordGen_t::TCGEN_TEXTURE;
			}
			else if (!Q_stricmp_cpp(token, "vector"))
			{
				ParseVector(text, 3, stage.bundle[0].tcGenVectors[0]);
				ParseVector(text, 3, stage.bundle[0].tcGenVectors[1]);

				stage.bundle[0].tcGen = texCoordGen_t::TCGEN_VECTOR;
			}
			else
			{
				ri.Printf(PRINT_WARNING, "WARNING: unknown texgen parm in shader '%s'\n", shader.name);
			}
		}
		//
		// tcMod <type> <...>
		//
		else if (!Q_stricmp_cpp(token, "tcMod"))
		{
			char buffer[1024] = "";

			while (1)
			{
				token = COM_ParseExt_cpp(text, false);
				if (token.empty())
					break;
				Q_strcat(buffer, sizeof(buffer), token.data());
				Q_strcat(buffer, sizeof(buffer), " ");
			}

			ParseTexMod(buffer, stage);

			continue;
		}
		//
		// depthmask
		//
		else if (!Q_stricmp_cpp(token, "depthwrite"))
		{
			depthMaskBits = GLS_DEPTHMASK_TRUE;
			depthMaskExplicit = true;

			continue;
		}
		else if (!Q_stricmp_cpp(token, "depthFragment") && s_extendedShader)
		{
			stage.depthFragment = true;
		}
		else
		{
			ri.Printf(PRINT_WARNING, "WARNING: unknown parameter '%s' in shader '%s'\n", token.data(), shader.name);
			return false;
		}
	}

	//
	// if cgen isn't explicitly specified, use either identity or identitylighting
	//
	if (stage.bundle[0].rgbGen == colorGen_t::CGEN_BAD)
	{
		if (blendSrcBits == 0 ||
			blendSrcBits == GLS_SRCBLEND_ONE ||
			blendSrcBits == GLS_SRCBLEND_SRC_ALPHA)
		{
			stage.bundle[0].rgbGen = colorGen_t::CGEN_IDENTITY_LIGHTING;
		}
		else
		{
			stage.bundle[0].rgbGen = colorGen_t::CGEN_IDENTITY;
		}
	}

	//
	// implicitly assume that a GL_ONE GL_ZERO blend mask disables blending
	//
	if ((blendSrcBits == GLS_SRCBLEND_ONE) && (blendDstBits == GLS_DSTBLEND_ZERO))
	{
		blendDstBits = blendSrcBits = 0;
		depthMaskBits = GLS_DEPTHMASK_TRUE;
	}

	// decide which agens we can skip
	if (stage.bundle[0].alphaGen == alphaGen_t::AGEN_IDENTITY)
	{
		if (stage.bundle[0].rgbGen == colorGen_t::CGEN_IDENTITY || stage.bundle[0].rgbGen == colorGen_t::CGEN_LIGHTING_DIFFUSE)
		{
			stage.bundle[0].alphaGen = alphaGen_t::AGEN_SKIP;
		}
	}

	if (depthMaskExplicit && shader.sort == static_cast<float>(shaderSort_t::SS_BAD))
	{
		// fix decals on q3wcp18 and other maps
		if (blendSrcBits == GLS_SRCBLEND_SRC_ALPHA && blendDstBits == GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA /*&& stage.rgbGen == colorGen_t::CGEN_VERTEX*/)
		{
			if (stage.bundle[0].alphaGen != alphaGen_t::AGEN_SKIP)
			{
				// q3wcp18 @ "textures/ctf_unified/floor_decal_blue" : alphaGen_t::AGEN_VERTEX, colorGen_t::CGEN_VERTEX
				// check for grates on tscabdm3
				if ( atestBits == 0 ) {
					depthMaskBits &= ~GLS_DEPTHMASK_TRUE;
				}
			}
			else
			{
				// skip for q3wcp14 jumppads and similar
				// q3wcp14 @ "textures/ctf_unified/bounce_blue" : alphaGen_t::AGEN_SKIP, colorGen_t::CGEN_IDENTITY
			}
			shader.sort = shader.polygonOffset ? static_cast<float>(shaderSort_t::SS_DECAL) : static_cast<float>(shaderSort_t::SS_OPAQUE) + 0.01f;
		}
		else if (blendSrcBits == GLS_SRCBLEND_ZERO && blendDstBits == GLS_DSTBLEND_ONE_MINUS_SRC_COLOR && stage.bundle[0].rgbGen == colorGen_t::CGEN_EXACT_VERTEX)
		{
			depthMaskBits &= ~GLS_DEPTHMASK_TRUE;
			shader.sort = static_cast<float>(shaderSort_t::SS_SEE_THROUGH);
		}
	}

	//
	// default texture coordinate generation
	//
	for (i = 0; i < NUM_TEXTURE_BUNDLES; i++)
	{
		if (stage.bundle[i].tcGen == texCoordGen_t::TCGEN_BAD)
		{
			if (stage.bundle[i].lightmap != LIGHTMAP_INDEX_NONE)
			{
				stage.bundle[i].tcGen = texCoordGen_t::TCGEN_LIGHTMAP;
			}
			else
			{
				stage.bundle[i].tcGen = texCoordGen_t::TCGEN_TEXTURE;
			}
		}
	}

	//
	// compute state bits
	//
	stage.stateBits = depthMaskBits |
					  blendSrcBits | blendDstBits |
					  atestBits |
					  depthFuncBits;

	stage.active = true;

	return true;
}

/*
===============
ParseDeform

deformVertexes wave <spread> <waveform> <base> <amplitude> <phase> <frequency>
deformVertexes normal <frequency> <amplitude>
deformVertexes move <vector> <waveform> <base> <amplitude> <phase> <frequency>
deformVertexes bulge <bulgeWidth> <bulgeHeight> <bulgeSpeed>
deformVertexes projectionShadow
deformVertexes autoSprite
deformVertexes autoSprite2
deformVertexes text[0-7]
===============
*/
static void ParseDeform(const char **text)
{
	std::string_view token = COM_ParseExt_cpp(text, false);

	if (token.empty())
	{
		ri.Printf(PRINT_WARNING, "WARNING: missing deform parm in shader '%s'\n", shader.name);
		return;
	}

	if (shader.numDeforms == MAX_SHADER_DEFORMS)
	{
		ri.Printf(PRINT_WARNING, "WARNING: MAX_SHADER_DEFORMS in '%s'\n", shader.name);
		return;
	}

	deformStage_t &ds = shader.deforms[shader.numDeforms];
	shader.numDeforms++;

	if (!Q_stricmp_cpp(token, "projectionShadow"))
	{
		ds.deformation = deform_t::DEFORM_PROJECTION_SHADOW;
		return;
	}

	if (!Q_stricmp_cpp(token, "autosprite"))
	{
		ds.deformation = deform_t::DEFORM_AUTOSPRITE;
		return;
	}

	if (!Q_stricmp_cpp(token, "autosprite2"))
	{
		ds.deformation = deform_t::DEFORM_AUTOSPRITE2;
		return;
	}

	if (!Q_stricmpn_cpp(token, "text", 4))
	{
		int n;

		n = token[4] - '0';
		if (n < 0 || n > 7)
		{
			n = 0;
		}
		ds.deformation = static_cast<deform_t>(static_cast<int>(deform_t::DEFORM_TEXT0) + n);
		return;
	}

	if (!Q_stricmp_cpp(token, "bulge"))
	{
		token = COM_ParseExt_cpp(text, false);
		if (token.empty())
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing deformVertexes bulge parm in shader '%s'\n", shader.name);
			return;
		}
		ds.bulgeWidth = Q_atof_cpp(token);

		token = COM_ParseExt_cpp(text, false);
		if (token.empty())
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing deformVertexes bulge parm in shader '%s'\n", shader.name);
			return;
		}
		ds.bulgeHeight = Q_atof_cpp(token);

		token = COM_ParseExt_cpp(text, false);
		if (token.empty())
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing deformVertexes bulge parm in shader '%s'\n", shader.name);
			return;
		}
		ds.bulgeSpeed = Q_atof_cpp(token);

		ds.deformation = deform_t::DEFORM_BULGE;
		return;
	}

	if (!Q_stricmp_cpp(token, "wave"))
	{
		float f;
		token = COM_ParseExt_cpp(text, false);
		if (token.empty())
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing deformVertexes parm in shader '%s'\n", shader.name);
			return;
		}

		f = Q_atof_cpp(token);
		if (f != 0.0f)
		{
			ds.deformationSpread = 1.0f / f;
		}
		else
		{
			ds.deformationSpread = 100.0f;
			ri.Printf(PRINT_WARNING, "WARNING: illegal div value of 0 in deformVertexes command for shader '%s'\n", shader.name);
		}

		ParseWaveForm(text, ds.deformationWave);
		ds.deformation = deform_t::DEFORM_WAVE;
		return;
	}

	if (!Q_stricmp_cpp(token, "normal"))
	{
		token = COM_ParseExt_cpp(text, false);
		if (token.empty())
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing deformVertexes parm in shader '%s'\n", shader.name);
			return;
		}
		ds.deformationWave.amplitude = Q_atof_cpp(token);

		token = COM_ParseExt_cpp(text, false);
		if (token.empty())
		{
			ri.Printf(PRINT_WARNING, "WARNING: missing deformVertexes parm in shader '%s'\n", shader.name);
			return;
		}
		ds.deformationWave.frequency = Q_atof_cpp(token);

		ds.deformation = deform_t::DEFORM_NORMALS;
		return;
	}

	if (!Q_stricmp_cpp(token, "move"))
	{
		int i;

		for (i = 0; i < 3; i++)
		{
			token = COM_ParseExt_cpp(text, false);
			if (token.empty())
			{
				ri.Printf(PRINT_WARNING, "WARNING: missing deformVertexes parm in shader '%s'\n", shader.name);
				return;
			}
			ds.moveVector[i] = Q_atof_cpp(token);
		}

		ParseWaveForm(text, ds.deformationWave);
		ds.deformation = deform_t::DEFORM_MOVE;
		return;
	}

	ri.Printf(PRINT_WARNING, "WARNING: unknown deformVertexes subtype '%s' found in shader '%s'\n", token.data(), shader.name);
}

/*
===============
ParseSkyParms

skyParms <outerbox> <cloudheight> <innerbox>
===============
*/
static void ParseSkyParms(const char **text)
{
	std::string_view token;
	static const char *suf[6] = {"rt", "bk", "lf", "ft", "up", "dn"};
	char pathname[MAX_QPATH];
	int i;
	imgFlags_t imgFlags = static_cast<imgFlags_t>(imgFlags_t::IMGFLAG_MIPMAP | imgFlags_t::IMGFLAG_PICMIP);

	if (r_neatsky->integer)
	{
		imgFlags = imgFlags_t::IMGFLAG_NONE;
	}

	// outerbox
	token = COM_ParseExt_cpp(text, false);
	if (token.empty())
	{
		ri.Printf(PRINT_WARNING, "WARNING: 'skyParms' missing parameter in shader '%s'\n", shader.name);
		return;
	}
	if (strcmp(token.data(), "-"))
	{
		for (i = 0; i < 6; i++)
		{
			Com_sprintf(pathname, sizeof(pathname), "%s_%s.tga", token.data(), suf[i]);
			shader.sky.outerbox[i] = R_FindImageFile(pathname, static_cast<imgFlags_t>(imgFlags | imgFlags_t::IMGFLAG_CLAMPTOEDGE));

			if (!shader.sky.outerbox[i])
			{
				shader.sky.outerbox[i] = tr.defaultImage;
			}
		}
	}

	// cloudheight
	token = COM_ParseExt_cpp(text, false);
	if (token.empty())
	{
		ri.Printf(PRINT_WARNING, "WARNING: 'skyParms' missing parameter in shader '%s'\n", shader.name);
		return;
	}
	shader.sky.cloudHeight = Q_atof_cpp(token);
	if (shader.sky.cloudHeight == 0.0)
	{
		shader.sky.cloudHeight = 512.0;
	}
	R_InitSkyTexCoords(shader.sky.cloudHeight);

	// innerbox
	token = COM_ParseExt_cpp(text, false);
	if (token.empty())
	{
		ri.Printf(PRINT_WARNING, "WARNING: 'skyParms' missing parameter in shader '%s'\n", shader.name);
		return;
	}
	if (strcmp(token.data(), "-"))
	{
		for (i = 0; i < 6; i++)
		{
			Com_sprintf(pathname, sizeof(pathname), "%s_%s.tga", token.data(), suf[i]);
			shader.sky.innerbox[i] = R_FindImageFile(pathname, imgFlags);
			if (!shader.sky.innerbox[i])
			{
				shader.sky.innerbox[i] = tr.defaultImage;
			}
		}
	}

	shader.isSky = true;
}

/*
=================
ParseSort
=================
*/
static void ParseSort(const char **text)
{
	std::string_view token = COM_ParseExt_cpp(text, false);
	if (token.empty())
	{
		ri.Printf(PRINT_WARNING, "WARNING: missing sort parameter in shader '%s'\n", shader.name);
		return;
	}

	if (!Q_stricmp_cpp(token, "portal"))
	{
		shader.sort = static_cast<float>(shaderSort_t::SS_PORTAL);
	}
	else if (!Q_stricmp_cpp(token, "sky"))
	{
		shader.sort = static_cast<float>(shaderSort_t::SS_ENVIRONMENT);
	}
	else if (!Q_stricmp_cpp(token, "opaque"))
	{
		shader.sort = static_cast<float>(shaderSort_t::SS_OPAQUE);
	}
	else if (!Q_stricmp_cpp(token, "decal"))
	{
		shader.sort = static_cast<float>(shaderSort_t::SS_DECAL);
	}
	else if (!Q_stricmp_cpp(token, "seeThrough"))
	{
		shader.sort = static_cast<float>(shaderSort_t::SS_SEE_THROUGH);
	}
	else if (!Q_stricmp_cpp(token, "banner"))
	{
		shader.sort = static_cast<float>(shaderSort_t::SS_BANNER);
	}
	else if (!Q_stricmp_cpp(token, "additive"))
	{
		shader.sort = static_cast<float>(shaderSort_t::SS_BLEND1);
	}
	else if (!Q_stricmp_cpp(token, "nearest"))
	{
		shader.sort = static_cast<float>(shaderSort_t::SS_NEAREST);
	}
	else if (!Q_stricmp_cpp(token, "underwater"))
	{
		shader.sort = static_cast<float>(shaderSort_t::SS_UNDERWATER);
	}
	else
	{
		shader.sort = Q_atof_cpp(token);
	}
}

// this table is also present in q3map

typedef struct
{
	std::string_view name;
	int clearSolid, surfaceFlags, contents;
} infoParm_t;

static constexpr infoParm_t infoParms[] = {
	// server relevant contents
	{"water", 1, 0, CONTENTS_WATER},
	{"slime", 1, 0, CONTENTS_SLIME}, // mildly damaging
	{"lava", 1, 0, CONTENTS_LAVA},	 // very damaging
	{"playerclip", 1, 0, CONTENTS_PLAYERCLIP},
	{"monsterclip", 1, 0, CONTENTS_MONSTERCLIP},
	{"nodrop", 1, 0, static_cast<int>(CONTENTS_NODROP)}, // don't drop items or leave bodies (death fog, lava, etc)
	{"nonsolid", 1, SURF_NONSOLID, 0},					 // clears the solid flag

	// utility relevant attributes
	{"origin", 1, 0, CONTENTS_ORIGIN},				 // center of rotating brushes
	{"trans", 0, 0, CONTENTS_TRANSLUCENT},			 // don't eat contained surfaces
	{"detail", 0, 0, CONTENTS_DETAIL},				 // don't include in structural bsp
	{"structural", 0, 0, CONTENTS_STRUCTURAL},		 // force into structural bsp even if trans
	{"areaportal", 1, 0, CONTENTS_AREAPORTAL},		 // divides areas
	{"clusterportal", 1, 0, CONTENTS_CLUSTERPORTAL}, // for bots
	{"donotenter", 1, 0, CONTENTS_DONOTENTER},		 // for bots

	{"fog", 1, 0, CONTENTS_FOG},			 // carves surfaces entering
	{"sky", 0, SURF_SKY, 0},				 // emit light from an environment map
	{"lightfilter", 0, SURF_LIGHTFILTER, 0}, // filter light going through it
	{"alphashadow", 0, SURF_ALPHASHADOW, 0}, // test light on a per-pixel basis
	{"hint", 0, SURF_HINT, 0},				 // use as a primary splitter

	// server attributes
	{"slick", 0, SURF_SLICK, 0},
	{"noimpact", 0, SURF_NOIMPACT, 0}, // don't make impact explosions or marks
	{"nomarks", 0, SURF_NOMARKS, 0},   // don't make impact marks, but still explode
	{"ladder", 0, SURF_LADDER, 0},
	{"nodamage", 0, SURF_NODAMAGE, 0},
	{"metalsteps", 0, SURF_METALSTEPS, 0},
	{"flesh", 0, SURF_FLESH, 0},
	{"nosteps", 0, SURF_NOSTEPS, 0},

	// drawsurf attributes
	{"nodraw", 0, SURF_NODRAW, 0},		   // don't generate a drawsurface (or a lightmap)
	{"pointlight", 0, SURF_POINTLIGHT, 0}, // sample lighting at vertexes
	{"nolightmap", 0, SURF_NOLIGHTMAP, 0}, // don't generate a lightmap
	{"nodlight", 0, SURF_NODLIGHT, 0},	   // don't ever add dynamic lights
	{"dust", 0, SURF_DUST, 0}			   // leave a dust trail when walking on this surface
};
/*
===============
ParseSurfaceParm

surfaceparm <name>
===============
*/
static void ParseSurfaceParm(const char **text)
{
	int numInfoParms = arrayLen(infoParms);
	int i;

	auto token = COM_ParseExt_cpp(text, false);
	for (i = 0; i < numInfoParms; i++)
	{
		if (!Q_stricmp_cpp(token, infoParms[i].name))
		{
			shader.surfaceFlags |= infoParms[i].surfaceFlags;
			shader.contentFlags |= infoParms[i].contents;
#if 0
			if (infoParms[i].clearSolid) {
				si->contents &= ~CONTENTS_SOLID;
			}
#endif
			break;
		}
	}
}

enum class resultType : int8_t
{
	res_invalid = -1,
	res_false = 0,
	res_true = 1
};

enum class branchType : int8_t
{
	brIF,
	brELIF,
	brELSE
};

enum class resultMask : int8_t
{
	maskOR,
	maskAND
};

static void derefVariable(std::string_view name, char *buf, const int size)
{
	if (!Q_stricmp_cpp(name, "vid_width"))
	{
		Com_sprintf(buf, size, "%i", glConfig.vidWidth);
		return;
	}
	if (!Q_stricmp_cpp(name, "vid_height"))
	{
		Com_sprintf(buf, size, "%i", glConfig.vidHeight);
		return;
	}
	ri.Cvar_VariableStringBuffer(name.data(), buf, size);
}

/*
===============
ParseCondition

if ( $cvar|<integer value> [<condition> $cvar|<integer value> [ [ || .. ] && .. ] ] )
{ shader stage }
[ else
{ shader stage } ]
===============
*/
static bool ParseCondition(const char **text, resultType *res)
{
	char lval_str[MAX_CVAR_VALUE_STRING];
	char rval_str[MAX_CVAR_VALUE_STRING]{};
	tokenType_t lval_type;
	tokenType_t rval_type;
	const char *token;
	tokenType_t op;
	resultMask rm;
	bool str;
	int r, r0;

	r = 0;					 // resulting value
	rm = resultMask::maskOR; // default mask

	for (;;)
	{
		rval_str[0] = '\0';
		rval_type = TK_GENEGIC;

		// expect l-value at least
		token = COM_ParseComplex_cpp(text, false);
		if (token[0] == '\0')
		{
			ri.Printf(PRINT_WARNING, "WARNING: expecting lvalue for condition in shader %s\n", shader.name);
			return false;
		}

		Q_strncpyz(lval_str, token, sizeof(lval_str));
		lval_type = com_tokentype;

		// get operator
		token = COM_ParseComplex_cpp(text, false);
		if (com_tokentype >= TK_EQ && com_tokentype <= TK_LTE)
		{
			op = com_tokentype;

			// expect r-value
			token = COM_ParseComplex_cpp(text, false);
			if (token[0] == '\0')
			{
				ri.Printf(PRINT_WARNING, "WARNING: expecting rvalue for condition in shader %s\n", shader.name);
				return false;
			}

			Q_strncpyz(rval_str, token, sizeof(rval_str));
			rval_type = com_tokentype;

			// read next token, expect '||', '&&' or ')', allow newlines
			/*token =*/COM_ParseComplex_cpp(text, true);
		}
		else if (com_tokentype == TK_SCOPE_CLOSE || com_tokentype == TK_OR || com_tokentype == TK_AND)
		{
			// no r-value, assume 'not zero' comparison
			op = TK_NEQ;
		}
		else
		{
			ri.Printf(PRINT_WARNING, "WARNING: unexpected operator '%s' for comparison in shader %s\n", token, shader.name);
			return false;
		}

		str = false;

		if (lval_type == TK_QUOTED)
		{
			str = true;
		}
		else
		{
			// dereference l-value
			if (lval_str[0] == '$')
			{
				derefVariable(std::string_view(lval_str + 1), lval_str, sizeof(lval_str));
			}
		}

		if (rval_type == TK_QUOTED)
		{
			str = true;
		}
		else
		{
			// dereference r-value
			if (rval_str[0] == '$')
			{
				derefVariable(std::string_view(rval_str + 1), rval_str, sizeof(rval_str));
			}
		}

		// evaluate expression
		if (str)
		{
			// string comparison
			switch (op)
			{
			case TK_EQ:
				r0 = strcmp(lval_str, rval_str) == 0;
				break;
			case TK_NEQ:
				r0 = strcmp(lval_str, rval_str) != 0;
				break;
			case TK_GT:
				r0 = strcmp(lval_str, rval_str) > 0;
				break;
			case TK_GTE:
				r0 = strcmp(lval_str, rval_str) >= 0;
				break;
			case TK_LT:
				r0 = strcmp(lval_str, rval_str) < 0;
				break;
			case TK_LTE:
				r0 = strcmp(lval_str, rval_str) <= 0;
				break;
			default:
				r0 = 0;
				break;
			}
		}
		else
		{
			// integer comparison
			int lval = atoi(lval_str);
			int rval = atoi(rval_str);
			switch (op)
			{
			case TK_EQ:
				r0 = (lval == rval);
				break;
			case TK_NEQ:
				r0 = (lval != rval);
				break;
			case TK_GT:
				r0 = (lval > rval);
				break;
			case TK_GTE:
				r0 = (lval >= rval);
				break;
			case TK_LT:
				r0 = (lval < rval);
				break;
			case TK_LTE:
				r0 = (lval <= rval);
				break;
			default:
				r0 = 0;
				break;
			}
		}

		if (rm == resultMask::maskOR)
			r |= r0;
		else
			r &= r0;

		if (com_tokentype == TK_OR)
		{
			rm = resultMask::maskOR;
			continue;
		}

		if (com_tokentype == TK_AND)
		{
			rm = resultMask::maskAND;
			continue;
		}

		if (com_tokentype != TK_SCOPE_CLOSE)
		{
			ri.Printf(PRINT_WARNING, "WARNING: expecting ')' in shader %s\n", shader.name);
			return false;
		}

		break;
	}

	if (res)
		*res = r ? resultType::res_true : resultType::res_false;

	return true;
}

/*
=================
FinishStage
=================
*/
static void FinishStage(shaderStage_t &stage)
{
	if (!tr.mergeLightmaps)
	{
		return;
	}

	int n;
	std::size_t i;

	for (i = 0; i < arrayLen(stage.bundle); i++)
	{
		textureBundle_t &bundle = stage.bundle[i];
		// offset lightmap coordinates
		if (bundle.lightmap >= LIGHTMAP_INDEX_OFFSET)
		{
			if (bundle.tcGen == texCoordGen_t::TCGEN_LIGHTMAP)
			{
				texModInfo_t &tmi = bundle.texMods[bundle.numTexMods];
				float x, y;
				const int lightmapIndex = R_GetLightmapCoords(bundle.lightmap - LIGHTMAP_INDEX_OFFSET, x, y);
				// rescale tcMod transform
				for ( n = 0; n < bundle.numTexMods; n++ ) {
					tmi = bundle.texMods[n];
					if ( tmi.type == texMod_t::TMOD_TRANSFORM ) {
						tmi.translate[0] *= tr.lightmapScale[0];
						tmi.translate[1] *= tr.lightmapScale[1];
					}
				}
				bundle.image[0] = tr.lightmaps[lightmapIndex];
				tmi.type = texMod_t::TMOD_OFFSET;
				tmi.offset[0] = x - tr.lightmapOffset[0];
				tmi.offset[1] = y - tr.lightmapOffset[1];
				bundle.numTexMods++;
			}
			continue;
		}
		// adjust texture coordinates to map on proper lightmap
		if (bundle.lightmap == LIGHTMAP_INDEX_SHADER)
		{
			if (bundle.tcGen != texCoordGen_t::TCGEN_LIGHTMAP)
			{
				texModInfo_t &tmi = bundle.texMods[bundle.numTexMods];
				tmi.type = texMod_t::TMOD_SCALE_OFFSET;
				tmi.scale[0] = tr.lightmapScale[0];
				tmi.scale[1] = tr.lightmapScale[1];
				tmi.offset[0] = tr.lightmapOffset[0];
				tmi.offset[1] = tr.lightmapOffset[1];
				bundle.numTexMods++;
			}
			else
			{
				for (n = 0; n < bundle.numTexMods; n++)
				{
					texModInfo_t &tmi = bundle.texMods[n];
					if (tmi.type == texMod_t::TMOD_TRANSFORM)
					{
						tmi.translate[0] *= tr.lightmapScale[0];
						tmi.translate[1] *= tr.lightmapScale[1];
					}
					else
					{
						// TODO: correct other transformations?
					}
				}
			}
			continue;
		}
		// revert lightmap texcoord correction if needed
		if (bundle.lightmap == LIGHTMAP_INDEX_NONE)
		{
			if (bundle.tcGen == texCoordGen_t::TCGEN_LIGHTMAP && shader.lightmapIndex >= 0)
			{
				for (n = bundle.numTexMods; n > 0; --n)
				{
					bundle.texMods[n] = bundle.texMods[n - 1];
				}
				texModInfo_t &tmi = bundle.texMods[0];
				tmi.type = texMod_t::TMOD_OFFSET_SCALE;
				tmi.offset[0] = -tr.lightmapOffset[0];
				tmi.offset[1] = -tr.lightmapOffset[1];
				tmi.scale[0] = 1.0f / tr.lightmapScale[0];
				tmi.scale[1] = 1.0f / tr.lightmapScale[1];
				bundle.numTexMods++;
			}
		}
	}
}

/*
=================
ParseShader

The current text pointer is at the explicit text definition of the
shader.  Parse it into the global shader variable.  Later functions
will optimize it.
=================
*/
static bool ParseShader(const char **text)
{
	resultType res;
	branchType branch;
	std::string_view token;
	int numStages = 0;

	s_extendedShader = (*text >= s_extensionOffset);

	token = COM_ParseExt_cpp(text, true);
	if (token[0] != '{')
	{
		ri.Printf(PRINT_WARNING, "WARNING: expecting '{', found '%s' instead in shader '%s'\n", token.data(), shader.name);
		return false;
	}

	res = resultType::res_invalid;

	while (1)
	{
		// token = COM_ParseExt( text, true );
		token = COM_ParseComplex_cpp(text, true);
		if (token.empty())
		{
			ri.Printf(PRINT_WARNING, "WARNING: no concluding '}' in shader %s\n", shader.name);
			return false;
		}
		// end of shader definition
		if (token[0] == '}')
		{
			break;
		}
		// stage definition
		else if (token[0] == '{')
		{
			if (numStages >= MAX_SHADER_STAGES)
			{
				ri.Printf(PRINT_WARNING, "WARNING: too many stages in shader %s (max is %i)\n", shader.name, MAX_SHADER_STAGES);
				return false;
			}

			if (!ParseStage(stages[numStages], text))
			{
				return false;
			}

			FinishStage(stages[numStages]);
			numStages++;
			continue;
		}
		// skip stuff that only the QuakeEdRadient needs
		else if (!Q_stricmpn_cpp(token, "qer", 3))
		{
			SkipRestOfLine_cpp(text);
			continue;
		}
		// sun parms
		else if (!Q_stricmp_cpp(token, "q3map_sun") || !Q_stricmp_cpp(token, "q3map_sunExt"))
		{
			float a, b;

			token = COM_ParseExt_cpp(text, false);
			tr.sunLight[0] = Q_atof_cpp(token);
			token = COM_ParseExt_cpp(text, false);
			tr.sunLight[1] = Q_atof_cpp(token);
			token = COM_ParseExt_cpp(text, false);
			tr.sunLight[2] = Q_atof_cpp(token);

			VectorNormalize(tr.sunLight);

			token = COM_ParseExt_cpp(text, false);
			a = Q_atof_cpp(token);
			VectorScale(tr.sunLight, a, tr.sunLight);

			token = COM_ParseExt_cpp(text, false);
			a = Q_atof_cpp(token);
			a = a / 180 * PI_cpp;

			token = COM_ParseExt_cpp(text, false);
			b = Q_atof_cpp(token);
			b = b / 180 * PI_cpp;

			tr.sunDirection[0] = cos(a) * cos(b);
			tr.sunDirection[1] = sin(a) * cos(b);
			tr.sunDirection[2] = sin(b);

			SkipRestOfLine_cpp(text);
			continue;
		}
		else if (!Q_stricmp_cpp(token, "deformVertexes"))
		{
			ParseDeform(text);
			continue;
		}
		else if (!Q_stricmp_cpp(token, "tesssize"))
		{
			SkipRestOfLine_cpp(text);
			continue;
		}
		else if (!Q_stricmp_cpp(token, "clampTime"))
		{
			token = COM_ParseExt_cpp(text, false);
			if (token[0])
			{
				shader.clampTime = Q_atof_cpp(token);
			}
		}
		// skip stuff that only the q3map needs
		else if (!Q_stricmpn_cpp(token, "q3map", 5))
		{
			SkipRestOfLine_cpp(text);
			continue;
		}
		// skip stuff that only q3map or the server needs
		else if (!Q_stricmp_cpp(token, "surfaceParm"))
		{
			ParseSurfaceParm(text);
			continue;
		}
		// no mip maps
		else if (!Q_stricmp_cpp(token, "nomipmaps"))
		{
			shader.noMipMaps = 1;
			shader.noPicMip = 1;
			continue;
		}
		// no picmip adjustment
		else if (!Q_stricmp_cpp(token, "nopicmip"))
		{
			shader.noPicMip = 1;
			continue;
		}
		else if (!Q_stricmp_cpp(token, "novlcollapse") && s_extendedShader)
		{
			shader.noVLcollapse = 1;
			continue;
		}
		// polygonOffset
		else if (!Q_stricmp_cpp(token, "polygonOffset"))
		{
			shader.polygonOffset = true;
			continue;
		}
		// entityMergable, allowing sprite surfaces from multiple entities
		// to be merged into one batch.  This is a savings for smoke
		// puffs and blood, but can't be used for anything where the
		// shader calcs (not the surface function) reference the entity color or scroll
		else if (!Q_stricmp_cpp(token, "entityMergable"))
		{
			shader.entityMergable = true;
			continue;
		}
		// fogParms
		else if (!Q_stricmp_cpp(token, "fogParms"))
		{
			if (!ParseVector(text, 3, shader.fogParms.color))
			{
				return false;
			}

			token = COM_ParseExt_cpp(text, false);
			if (!token[0])
			{
				ri.Printf(PRINT_WARNING, "WARNING: missing parm for 'fogParms' keyword in shader '%s'\n", shader.name);
				continue;
			}
			shader.fogParms.depthForOpaque = Q_atof_cpp(token);

			// skip any old gradient directions
			SkipRestOfLine_cpp(text);
			continue;
		}
		// portal
		else if (!Q_stricmp_cpp(token, "portal"))
		{
			shader.sort = static_cast<float>(shaderSort_t::SS_PORTAL);
			continue;
		}
		// skyparms <cloudheight> <outerbox> <innerbox>
		else if (!Q_stricmp_cpp(token, "skyparms"))
		{
			ParseSkyParms(text);
			if (r_neatsky->integer)
			{
				shader.noPicMip = 1;
				shader.noMipMaps = 1;
			}
			continue;
		}
		// light <value> determines flaring in q3map, not needed here
		else if (!Q_stricmp_cpp(token, "light"))
		{
			COM_ParseExt_cpp(text, false);
			continue;
		}
		// cull <face>
		else if (!Q_stricmp_cpp(token, "cull"))
		{
			token = COM_ParseExt_cpp(text, false);
			if (token.empty())
			{
				ri.Printf(PRINT_WARNING, "WARNING: missing cull parms in shader '%s'\n", shader.name);
				continue;
			}

			if (!Q_stricmp_cpp(token, "none") || !Q_stricmp_cpp(token, "twosided") || !Q_stricmp_cpp(token, "disable"))
			{
				shader.cullType = cullType_t::CT_TWO_SIDED;
			}
			else if (!Q_stricmp_cpp(token, "back") || !Q_stricmp_cpp(token, "backside") || !Q_stricmp_cpp(token, "backsided"))
			{
				shader.cullType = cullType_t::CT_BACK_SIDED;
			}
			else
			{
				ri.Printf(PRINT_WARNING, "WARNING: invalid cull parm '%s' in shader '%s'\n", token.data(), shader.name);
			}
			continue;
		}
		// sort
		else if (!Q_stricmp_cpp(token, "sort"))
		{
			ParseSort(text);
			continue;
		}
		// conditional stage definition
		else if ((!Q_stricmp_cpp(token, "if") || !Q_stricmp_cpp(token, "else") || !Q_stricmp_cpp(token, "elif")) && s_extendedShader)
		{
			if (Q_stricmp_cpp(token, "if") == 0)
			{
				branch = branchType::brIF;
			}
			else
			{
				if (res == resultType::res_invalid)
				{
					// we don't have any previous 'if' statements
					ri.Printf(PRINT_WARNING, "WARNING: unexpected '%s' in '%s'\n", token.data(), shader.name);
					return false;
				}
				if (Q_stricmp_cpp(token, "else") == 0)
					branch = branchType::brELSE;
				else
					branch = branchType::brELIF;
			}

			if (branch != branchType::brELSE)
			{ // we can set/update result
				token = COM_ParseComplex_cpp(text, false);
				if (com_tokentype != TK_SCOPE_OPEN)
				{
					ri.Printf(PRINT_WARNING, "WARNING: expecting '(' in '%s'\n", shader.name);
					return false;
				}
				if (!ParseCondition(text, (branch == branchType::brIF || res == resultType::res_true) ? &res : NULL))
				{
					ri.Printf(PRINT_WARNING, "WARNING: error parsing condition in '%s'\n", shader.name);
					return false;
				}
			}

			if (res == resultType::res_false)
			{
				// skip next stage or keyword until newline
				token = COM_ParseExt_cpp(text, true);
				if (token[0] == '{')
					SkipBracedSection_cpp(text, 1 /* depth */);
				else
					SkipRestOfLine_cpp(text);
			}
			else
			{
				// parse next tokens as usual
			}

			if (branch == branchType::brELSE)
				res = resultType::res_invalid; // finalize branch
			else
				res = static_cast<resultType>(static_cast<int>(res) ^ 1); // or toggle for possible "elif" / "else" statements

			continue;
		}
		else
		{
			ri.Printf(PRINT_WARNING, "WARNING: unknown general shader parameter '%s' in '%s'\n", token.data(), shader.name);
			return false;
		}
	}

	//
	// ignore shaders that don't have any stages, unless it is a sky or fog
	//
	if (numStages == 0 && !shader.isSky && !(shader.contentFlags & CONTENTS_FOG))
	{
		return false;
	}

	shader.explicitlyDefined = true;

	return true;
}

/*
========================================================================================

SHADER OPTIMIZATION AND FOGGING

========================================================================================
*/

/*
===================
ComputeStageIteratorFunc

See if we can use on of the simple fastpath stage functions,
otherwise set to the generic stage function
===================
*/
static void ComputeStageIteratorFunc(void)
{
	//
	// see if this should go into the sky path
	//
	if (shader.isSky)
	{
		shader.optimalStageIteratorFunc = RB_StageIteratorSky;
	}
	else
	{
		shader.optimalStageIteratorFunc = RB_StageIteratorGeneric;
	}
}

/*
================
CollapseMultitexture

Attempt to combine two stages into a single multitexture stage
FIXME: I think modulated add + modulated add collapses incorrectly
=================
*/
static int CollapseMultitexture(unsigned int st0bits, shaderStage_t &st0, shaderStage_t &st1, const int num_stages)
{
	// make sure both stages are active
	if (!st0.active || !st1.active)
	{
		return 0;
	}

	if (st0.depthFragment || (st0.stateBits & GLS_ATEST_BITS))
	{
		return 0;
	}

	int abits, bbits;
	int i, mtEnv;
	textureBundle_t tmpBundle;
	bool nonIdenticalColors;
	bool swapLightmap;

	abits = st0bits; // st0.stateBits;
	bbits = st1.stateBits;

	// make sure that both stages have identical state other than blend modes
	if ((abits & ~(GLS_BLEND_BITS | GLS_DEPTHMASK_TRUE)) !=
		(bbits & ~(GLS_BLEND_BITS | GLS_DEPTHMASK_TRUE)))
	{
		return 0;
	}

	abits &= GLS_BLEND_BITS;
	bbits &= GLS_BLEND_BITS;

	// search for a valid multitexture blend function
	for (i = 0; collapse[i].blendA != -1; i++)
	{
		if (abits == collapse[i].blendA && bbits == collapse[i].blendB)
		{
			break;
		}
	}

	// nothing found
	if (collapse[i].blendA == -1)
	{
		return 0;
	}

	mtEnv = collapse[i].multitextureEnv;

	// GL_ADD is a separate extension
	if ( mtEnv == GL_ADD && !glConfig.textureEnvAddAvailable ) {
		return 0;
	}

	if ( mtEnv == GL_ADD && st0.bundle[0].rgbGen != colorGen_t::CGEN_IDENTITY ) {
		mtEnv = GL_ADD_NONIDENTITY;
	}

	if ( st0.mtEnv && st0.mtEnv != mtEnv ) {
		// we don't support different blend modes in 3x mode, yet
		return 0;
	}

	nonIdenticalColors = false;

	// make sure waveforms have identical parameters
	if ((st0.bundle[0].rgbGen != st1.bundle[0].rgbGen) || (st0.bundle[0].alphaGen != st1.bundle[0].alphaGen))
	{
		nonIdenticalColors = true;
	}

	if (st0.bundle[0].rgbGen == colorGen_t::CGEN_WAVEFORM)
	{
		if (memcmp(&st0.bundle[0].rgbWave, &st1.bundle[0].rgbWave, sizeof(stages[0].bundle[0].rgbWave)))
		{
			nonIdenticalColors = true;
		}
	}

	if (st0.bundle[0].alphaGen == alphaGen_t::AGEN_WAVEFORM)
	{
		if (memcmp(&st0.bundle[0].alphaWave, &st1.bundle[0].alphaWave, sizeof(stages[0].bundle[0].alphaWave)))
		{
			nonIdenticalColors = true;
		}
	}

	if (nonIdenticalColors)
	{
		switch (mtEnv)
		{
		case GL_ADD:
		case GL_ADD_NONIDENTITY:
			mtEnv = GL_BLEND_ADD;
			break;
		case GL_MODULATE:
			mtEnv = GL_BLEND_MODULATE;
			break;
		}
	}

	switch (mtEnv)
	{
	case GL_MODULATE:
	case GL_ADD:
		swapLightmap = true;
		break;
	default:
		swapLightmap = false;
		break;
	}

	// make sure that lightmaps are in bundle 1
	if (swapLightmap && st0.bundle[0].lightmap != LIGHTMAP_INDEX_NONE && !st0.mtEnv)
	{
		tmpBundle = st0.bundle[0];
		st0.bundle[0] = st1.bundle[0];
		st0.bundle[1] = tmpBundle;
	}
	else
	{
		if (st0.mtEnv)
			st0.bundle[2] = st1.bundle[0]; // add to third bundle
		else
			st0.bundle[1] = st1.bundle[0];
	}

	if (st0.mtEnv)
	{
		st0.mtEnv3 = mtEnv;
	}
	else
	{
		// set the new blend state bits
		st0.stateBits &= ~GLS_BLEND_BITS;
		st0.stateBits |= collapse[i].multitextureBlend;

		st0.mtEnv = mtEnv;
		shader.multitextureEnv = true;
	}

	st0.numTexBundles++;

	//
	// move down subsequent shaders
	//
	if (num_stages > 2)
	{
		memmove(&st1, &st1 + 1, sizeof(stages[0]) * (num_stages - 2));
	}

	Com_Memset((&st0 + num_stages - 1), 0, sizeof(stages[0]));

	if (vk_inst.maxBoundDescriptorSets >= 8 && num_stages >= 3 && !st0.mtEnv3)
	{
		if (mtEnv == GL_BLEND_ONE_MINUS_ALPHA || mtEnv == GL_BLEND_ALPHA || mtEnv == GL_BLEND_MIX_ALPHA || mtEnv == GL_BLEND_MIX_ONE_MINUS_ALPHA || mtEnv == GL_BLEND_DST_COLOR_SRC_ALPHA)
		{
			// pass original state bits so recursive detection will work for these shaders
			return 1 + CollapseMultitexture(st0bits, st0, st1, num_stages - 1);
		}
		if (abits == 0)
		{
			return 1 + CollapseMultitexture(st0.stateBits, st0, st1, num_stages - 1);
		}
	}

	return 1;
}

#ifdef USE_PMLIGHT

static int tcmodWeight(const textureBundle_t *bundle)
{
	if (bundle->numTexMods == 0)
		return 1;

	return 0;
}

#if 0
static int rgbWeight(const textureBundle_t* bundle) {

	switch (bundle->rgbGen) {
	case colorGen_t::CGEN_EXACT_VERTEX: return 3;
	case colorGen_t::CGEN_VERTEX: return 3;
	case colorGen_t::CGEN_ENTITY: return 2;
	case colorGen_t::CGEN_ONE_MINUS_ENTITY: return 2;
	case colorGen_t::CGEN_CONST: return 1;
	default: return 0;
	}
}
#endif

static const textureBundle_t *lightingBundle(const int stageIndex, const textureBundle_t *selected)
{
	const shaderStage_t *stage = &stages[stageIndex];
	int i;

	for (i = 0; i < static_cast<int>(stage->numTexBundles); i++)
	{
		const textureBundle_t *bundle = &stage->bundle[i];
		if (bundle->lightmap != LIGHTMAP_INDEX_NONE)
		{
			continue;
		}
		if (bundle->image[0] == tr.whiteImage)
		{
			continue;
		}
		if (bundle->tcGen != texCoordGen_t::TCGEN_TEXTURE)
		{
			continue;
		}
		if (selected)
		{
			if (bundle->rgbGen == colorGen_t::CGEN_IDENTITY && (stage->stateBits & GLS_BLEND_BITS) == (GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO))
			{
				// fix for q3wcp17' textures/scanctf2/bounce_white and others
				continue;
			}
			if (tcmodWeight(selected) > tcmodWeight(bundle))
			{
				continue;
			}
			// commented because causes regression in q3dm1' Mouth area
			// if ( rgbWeight( selected ) > rgbWeight( bundle ) ) {
			// continue;
			//}
		}
		shader.lightingStage = stageIndex;
		shader.lightingBundle = i;
		selected = bundle;
	}

	return selected;
}

/*
====================
FindLightingStages

Find proper stage for dlight pass
====================
*/
static void FindLightingStages(void)
{
	const textureBundle_t *bundle;
	int i;

	shader.lightingStage = -1;
	shader.lightingBundle = 0;

	if (shader.isSky || (shader.surfaceFlags & (SURF_NODLIGHT | SURF_SKY)) || shader.sort == static_cast<float>(shaderSort_t::SS_ENVIRONMENT) || shader.sort >= static_cast<float>(shaderSort_t::SS_FOG))
		return;

	bundle = NULL;
	for (i = 0; i < shader.numUnfoggedPasses; i++)
	{
		const shaderStage_t &st = stages[i];
		if (!st.active)
			break;
		if (st.isDetail && shader.lightingStage >= 0)
			continue;
		if ((st.stateBits & GLS_BLEND_BITS) == (GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE))
		{
			if (bundle && bundle->numTexMods)
			{
				// already selected bundle has somewhat non-static tcgen
				// so we may accept this stage
				// this fixes jumppads on lun3dm5
			}
			else
			{
				continue;
			}
		}
		bundle = lightingBundle(i, bundle);
	}
}
#endif

/*
=============
FixRenderCommandList
https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=493
Arnout: this is a nasty issue. Shaders can be registered after drawsurfaces are generated
but before the frame is rendered. This will, for the duration of one frame, cause drawsurfaces
to be rendered with bad shaders. To fix this, need to go through all render commands and fix
sortedIndex.
==============
*/
static void FixRenderCommandList(const int newShader)
{
	renderCommandList_t *cmdList = &backEndData->commands;

	if (cmdList)
	{
		const void *curCmd = cmdList->cmds;

		*((int *)(cmdList->cmds + cmdList->used)) = static_cast<int>(renderCommand_t::RC_END_OF_LIST);

		while (1)
		{
			curCmd = PADP(curCmd, sizeof(void *));

			switch (static_cast<renderCommand_t>(*(const int *)curCmd))
			{
			case renderCommand_t::RC_SET_COLOR:
			{
				const setColorCommand_t *sc_cmd = (const setColorCommand_t *)curCmd;
				curCmd = (const void *)(sc_cmd + 1);
				break;
			}
			case renderCommand_t::RC_STRETCH_PIC:
			{
				const stretchPicCommand_t *sp_cmd = (const stretchPicCommand_t *)curCmd;
				curCmd = (const void *)(sp_cmd + 1);
				break;
			}
			case renderCommand_t::RC_DRAW_SURFS:
			{
				int i;
				drawSurf_t *drawSurf;
				shader_t *sh;
				int fogNum;
				int entityNum;
				int dlightMap;
				int sortedIndex;
				const drawSurfsCommand_t *ds_cmd = (const drawSurfsCommand_t *)curCmd;

				for (i = 0, drawSurf = ds_cmd->drawSurfs; i < ds_cmd->numDrawSurfs; i++, drawSurf++)
				{
					R_DecomposeSort(drawSurf->sort, entityNum, &sh, fogNum, dlightMap);
					sortedIndex = ((drawSurf->sort >> QSORT_SHADERNUM_SHIFT) & SHADERNUM_MASK);
					if (sortedIndex >= newShader)
					{
						sortedIndex = sh->sortedIndex;
						drawSurf->sort = (sortedIndex << QSORT_SHADERNUM_SHIFT) | (entityNum << QSORT_REFENTITYNUM_SHIFT) | (fogNum << QSORT_FOGNUM_SHIFT) | (int)dlightMap;
					}
				}
				curCmd = (const void *)(ds_cmd + 1);
				break;
			}
			case renderCommand_t::RC_DRAW_BUFFER:
			{
				const drawBufferCommand_t *db_cmd = (const drawBufferCommand_t *)curCmd;
				curCmd = (const void *)(db_cmd + 1);
				break;
			}
			case renderCommand_t::RC_SWAP_BUFFERS:
			{
				const swapBuffersCommand_t *sb_cmd = (const swapBuffersCommand_t *)curCmd;
				curCmd = (const void *)(sb_cmd + 1);
				break;
			}
			case renderCommand_t::RC_FINISHBLOOM:
			{
				const finishBloomCommand_t *fb_cmd = (const finishBloomCommand_t *)curCmd;
				curCmd = (const void *)(fb_cmd + 1);
				break;
			}
			case renderCommand_t::RC_COLORMASK:
			{
				const colorMaskCommand_t *cm_cmd = (const colorMaskCommand_t *)curCmd;
				curCmd = (const void *)(cm_cmd + 1);
				break;
			}
			case renderCommand_t::RC_CLEARDEPTH:
			{
				const clearDepthCommand_t *cd_cmd = (const clearDepthCommand_t *)curCmd;
				curCmd = (const void *)(cd_cmd + 1);
				break;
			}
			case renderCommand_t::RC_CLEARCOLOR:
			{
				const clearColorCommand_t *cc_cmd = (const clearColorCommand_t *)curCmd;
				curCmd = (const void *)(cc_cmd + 1);
				break;
			}
			case renderCommand_t::RC_END_OF_LIST:
			default:
				return;
			}
		}
	}
}

static bool EqualACgen(const shaderStage_t *st1, const shaderStage_t *st2)
{
	if (st1 == NULL || st2 == NULL)
	{
		return false;
	}

	if (st1->bundle[0].adjustColorsForFog != st2->bundle[0].adjustColorsForFog)
	{
		return false;
	}

	return true;
}

static bool EqualRGBgen(const shaderStage_t *st1, const shaderStage_t *st2)
{
	if (st1 == NULL || st2 == NULL)
	{
		return false;
	}

	if (st1->bundle[0].rgbGen != st2->bundle[0].rgbGen || st1->active != st2->active)
	{
		return false;
	}

	if (st1->bundle[0].rgbGen == colorGen_t::CGEN_CONST)
	{
		if (st1->bundle[0].constantColor.u32 != st2->bundle[0].constantColor.u32)
		{
			return false;
		}
	}

	if (st1->bundle[0].rgbGen == colorGen_t::CGEN_WAVEFORM)
	{
		if (memcmp(&st1->bundle[0].rgbWave, &st2->bundle[0].rgbWave, sizeof(st1->bundle[0].rgbWave)) != 0)
		{
			return false;
		}
	}

	if (st1->bundle[0].alphaGen != st2->bundle[0].alphaGen)
	{
		return false;
	}

	if (st1->bundle[0].alphaGen == alphaGen_t::AGEN_CONST)
	{
		if (st1->bundle[0].rgbGen != colorGen_t::CGEN_CONST)
		{
			if (st1->bundle[0].constantColor.rgba[3] != st2->bundle[0].constantColor.rgba[3])
			{
				return false;
			}
		}
	}

	if (st1->bundle[0].alphaGen == alphaGen_t::AGEN_WAVEFORM)
	{
		if (memcmp(&st1->bundle[0].alphaWave, &st2->bundle[0].alphaWave, sizeof(st1->bundle[0].alphaWave)) != 0)
		{
			return false;
		}
	}

	return true;
}

static bool EqualTCgen(const int bundle, const shaderStage_t *st1, const shaderStage_t *st2)
{
	int tm;

	if (st1 == NULL || st2 == NULL)
		return false;

	if (st1->active != st2->active)
		return false;

	const textureBundle_t &b1 = st1->bundle[bundle];
	const textureBundle_t &b2 = st2->bundle[bundle];

	if (b1.tcGen != b2.tcGen)
	{
		return false;
	}

	if (b1.tcGen == texCoordGen_t::TCGEN_VECTOR)
	{
		if (memcmp(b1.tcGenVectors, b2.tcGenVectors, sizeof(b1.tcGenVectors)) != 0)
		{
			return false;
		}
	}

	// if ( b1.tcGen == texCoordGen_t::TCGEN_ENVIRONMENT_MAPPED_FP ) {
	//	if ( b1.isScreenMap != b2.isScreenMap ) {
	//		return false;
	//	}
	// }

	// if ( b1.tcGen != texCoordGen_t::TCGEN_LIGHTMAP && b1.lightmap != b2.lightmap && r_mergeLightmaps->integer ) {
	//	return false;
	// }

	if (b1.numTexMods != b2.numTexMods)
	{
		return false;
	}

	for (tm = 0; tm < b1.numTexMods; tm++)
	{
		const texModInfo_t &tm1 = b1.texMods[tm];
		const texModInfo_t &tm2 = b2.texMods[tm];

		if (tm1.type != tm2.type)
		{
			return false;
		}

		if (tm1.type == texMod_t::TMOD_TURBULENT || tm1.type == texMod_t::TMOD_STRETCH)
		{
			if (memcmp(&tm1.wave, &tm2.wave, sizeof(tm1.wave)) != 0)
			{
				return false;
			}
			continue;
		}

		if (tm1.type == texMod_t::TMOD_SCROLL)
		{
			if (memcmp(tm1.scroll, tm2.scroll, sizeof(tm1.scroll)) != 0)
			{
				return false;
			}
			continue;
		}

		if (tm1.type == texMod_t::TMOD_SCALE)
		{
			if (memcmp(tm1.scale, tm2.scale, sizeof(tm1.scale)) != 0)
			{
				return false;
			}
			continue;
		}

		if (tm1.type == texMod_t::TMOD_OFFSET)
		{
			if (memcmp(tm1.offset, tm2.offset, sizeof(tm1.offset)) != 0)
			{
				return false;
			}
			continue;
		}

		if (tm1.type == texMod_t::TMOD_SCALE_OFFSET)
		{
			if (memcmp(tm1.scale, tm2.scale, sizeof(tm1.scale)) != 0)
			{
				return false;
			}
			if (memcmp(tm1.offset, tm2.offset, sizeof(tm1.offset)) != 0)
			{
				return false;
			}
			continue;
		}

		if (tm1.type == texMod_t::TMOD_TRANSFORM)
		{
			if (memcmp(tm1.matrix, tm2.matrix, sizeof(tm1.matrix)) != 0)
			{
				return false;
			}
			if (memcmp(tm1.translate, tm2.translate, sizeof(tm1.translate)) != 0)
			{
				return false;
			}
			continue;
		}

		if (tm1.type == texMod_t::TMOD_ROTATE && tm1.rotateSpeed != tm2.rotateSpeed)
		{
			return false;
		}
	}

	return true;
}

/*
==============
SortNewShader

Positions the most recently created shader in the tr.sortedShaders[]
array so that the shader->sort key is sorted relative to the other
shaders.

Sets shader->sortedIndex
==============
*/
static void SortNewShader(void)
{
	int i;
	float sort;
	shader_t *newShader;

	newShader = tr.shaders[tr.numShaders - 1];
	sort = newShader->sort;

	for (i = tr.numShaders - 2; i >= 0; i--)
	{
		if (tr.sortedShaders[i]->sort <= sort)
		{
			break;
		}
		tr.sortedShaders[i + 1] = tr.sortedShaders[i];
		tr.sortedShaders[i + 1]->sortedIndex++;
	}

	// Arnout: fix rendercommandlist
	// https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=493
	FixRenderCommandList(i + 1);

	newShader->sortedIndex = i + 1;
	tr.sortedShaders[i + 1] = newShader;
}

/*
====================
GeneratePermanentShader
====================
*/
static shader_t *GeneratePermanentShader(void)
{
	shader_t *newShader;
	int i, b;
	int size, hash;

	if (tr.numShaders >= MAX_SHADERS)
	{
		ri.Printf(PRINT_WARNING, "WARNING: GeneratePermanentShader - MAX_SHADERS hit\n");
		return tr.defaultShader;
	}

	newShader = reinterpret_cast<shader_t *>(ri.Hunk_Alloc(sizeof(shader_t), h_low));

	*newShader = shader;

	tr.shaders[tr.numShaders] = newShader;
	newShader->index = tr.numShaders;

	tr.sortedShaders[tr.numShaders] = newShader;
	newShader->sortedIndex = tr.numShaders;

	tr.numShaders++;

	for (i = 0; i < newShader->numUnfoggedPasses; i++)
	{
		if (!stages[i].active)
		{
			break;
		}
		newShader->stages[i] = reinterpret_cast<shaderStage_t *>(ri.Hunk_Alloc(sizeof(stages[i]), h_low));
		*newShader->stages[i] = stages[i];

		for (b = 0; b < NUM_TEXTURE_BUNDLES; b++)
		{
			size = newShader->stages[i]->bundle[b].numTexMods * sizeof(texModInfo_t);
			if (size)
			{
				newShader->stages[i]->bundle[b].texMods = reinterpret_cast<texModInfo_t *>(ri.Hunk_Alloc(size, h_low));
				Com_Memcpy(newShader->stages[i]->bundle[b].texMods, stages[i].bundle[b].texMods, size);
			}
		}
	}

	SortNewShader();

	hash = generateHashValue(newShader->name, FILE_HASH_SIZE);
	newShader->next = shaderHashTable[hash];
	shaderHashTable[hash] = newShader;

	return newShader;
}

/*
=================
VertexLightingCollapse

If vertex lighting is enabled, only render a single
pass, trying to guess which is the correct one to best approximate
what it is supposed to look like.
=================
*/
static void VertexLightingCollapse(void)
{
	int stage;
	int bestImageRank;
	int rank;
	bool vertexColors;

	// if we aren't opaque, just use the first pass
	if (shader.sort == static_cast<float>(shaderSort_t::SS_OPAQUE))
	{

		// pick the best texture for the single pass
		shaderStage_t &bestStage = stages[0];
		bestImageRank = -999999;
		vertexColors = false;

		for (stage = 0; stage < MAX_SHADER_STAGES; stage++)
		{
			shaderStage_t &pStage = stages[stage];

			if (!pStage.active)
			{
				break;
			}
			rank = 0;

			if (pStage.bundle[0].lightmap != LIGHTMAP_INDEX_NONE)
			{
				rank -= 100;
			}
			if (pStage.bundle[0].tcGen != texCoordGen_t::TCGEN_TEXTURE)
			{
				rank -= 5;
			}
			if (pStage.bundle[0].numTexMods)
			{
				rank -= 5;
			}
			if (pStage.bundle[0].rgbGen != colorGen_t::CGEN_IDENTITY && pStage.bundle[0].rgbGen != colorGen_t::CGEN_IDENTITY_LIGHTING)
			{
				rank -= 3;
			}

			if (rank > bestImageRank)
			{
				bestImageRank = rank;
				bestStage = pStage;
			}

			// detect missing vertex colors on ojfc-17 for green/dark pink flags
			if (pStage.bundle[0].rgbGen != colorGen_t::CGEN_IDENTITY || pStage.bundle[0].tcGen == texCoordGen_t::TCGEN_LIGHTMAP || pStage.stateBits & GLS_ATEST_BITS)
			{
				vertexColors = true;
			}
		}

		stages[0].bundle[0] = bestStage.bundle[0];
		stages[0].stateBits &= ~(GLS_DSTBLEND_BITS | GLS_SRCBLEND_BITS);
		stages[0].stateBits |= GLS_DEPTHMASK_TRUE;
		if (shader.lightmapIndex == LIGHTMAP_NONE)
		{
			stages[0].bundle[0].rgbGen = colorGen_t::CGEN_LIGHTING_DIFFUSE;
		}
		else
		{
			if (vertexColors)
			{
				stages[0].bundle[0].rgbGen = colorGen_t::CGEN_EXACT_VERTEX;
			}
			else
			{
				stages[0].bundle[0].rgbGen = colorGen_t::CGEN_IDENTITY_LIGHTING;
			}
		}
		stages[0].bundle[0].alphaGen = alphaGen_t::AGEN_SKIP;
	}
	else
	{
		// don't use a lightmap (tesla coils)
		if (stages[0].bundle[0].lightmap != LIGHTMAP_INDEX_NONE)
		{
			stages[0] = stages[1];
		}

		// if we were in a cross-fade cgen, hack it to normal
		if (stages[0].bundle[0].rgbGen == colorGen_t::CGEN_ONE_MINUS_ENTITY || stages[1].bundle[0].rgbGen == colorGen_t::CGEN_ONE_MINUS_ENTITY)
		{
			stages[0].bundle[0].rgbGen = colorGen_t::CGEN_IDENTITY_LIGHTING;
		}
		if ((stages[0].bundle[0].rgbGen == colorGen_t::CGEN_WAVEFORM && stages[0].bundle[0].rgbWave.func == genFunc_t::GF_SAWTOOTH) && (stages[1].bundle[0].rgbGen == colorGen_t::CGEN_WAVEFORM && stages[1].bundle[0].rgbWave.func == genFunc_t::GF_INVERSE_SAWTOOTH))
		{
			stages[0].bundle[0].rgbGen = colorGen_t::CGEN_IDENTITY_LIGHTING;
		}
		if ((stages[0].bundle[0].rgbGen == colorGen_t::CGEN_WAVEFORM && stages[0].bundle[0].rgbWave.func == genFunc_t::GF_INVERSE_SAWTOOTH) && (stages[1].bundle[0].rgbGen == colorGen_t::CGEN_WAVEFORM && stages[1].bundle[0].rgbWave.func == genFunc_t::GF_SAWTOOTH))
		{
			stages[0].bundle[0].rgbGen = colorGen_t::CGEN_IDENTITY_LIGHTING;
		}
	}

	for (stage = 1; stage < MAX_SHADER_STAGES; stage++)
	{
		shaderStage_t &pStage = stages[stage];

		if (!pStage.active)
		{
			break;
		}

		Com_Memset(&pStage, 0, sizeof(pStage));
	}
}

/*
===============
InitShader
===============
*/
static void InitShader(std::string_view name, const int lightmapIndex)
{
	int i;

	// clear the global shader
	Com_Memset(&shader, 0, sizeof(shader));
	Com_Memset(&stages, 0, sizeof(stages));

	Q_strncpyz(shader.name, name.data(), sizeof(shader.name));
	shader.lightmapIndex = lightmapIndex;

	// we need to know original (unmodified) lightmap index
	// because shader search functions expects this
	// otherwise they will fail and cause massive duplication
	shader.lightmapSearchIndex = shader.lightmapIndex;

	for (i = 0; i < MAX_SHADER_STAGES; i++)
	{
		stages[i].bundle[0].texMods = texMods[i];
	}
}

static void DetectNeeds(void)
{
	int i, n;

	for (i = 0; i < MAX_SHADER_STAGES; i++)
	{
		if (!stages[i].active)
			break;

		for (n = 0; n < NUM_TEXTURE_BUNDLES; n++)
		{
			const texCoordGen_t t = stages[i].bundle[n].tcGen;
			if (t == texCoordGen_t::TCGEN_LIGHTMAP)
			{
				shader.needsST2 = true;
			}
			if (t == texCoordGen_t::TCGEN_ENVIRONMENT_MAPPED || t == texCoordGen_t::TCGEN_ENVIRONMENT_MAPPED_FP)
			{
				shader.needsNormal = true;
			}
			if (stages[i].bundle[n].alphaGen == alphaGen_t::AGEN_LIGHTING_SPECULAR || stages[i].bundle[n].rgbGen == colorGen_t::CGEN_LIGHTING_DIFFUSE)
			{
				shader.needsNormal = true;
			}
		}
#if 0
		t1 = stages[i].bundle[0].tcGen;
		t2 = stages[i].bundle[1].tcGen;

		if (t1 == texCoordGen_t::TCGEN_LIGHTMAP || t2 == texCoordGen_t::TCGEN_LIGHTMAP)
		{
			shader.needsST2 = true;
		}
		if (t1 == texCoordGen_t::TCGEN_ENVIRONMENT_MAPPED || t1 == texCoordGen_t::TCGEN_ENVIRONMENT_MAPPED_FP)
		{
			shader.needsNormal = true;
		}
		if (t2 == texCoordGen_t::TCGEN_ENVIRONMENT_MAPPED || t2 == texCoordGen_t::TCGEN_ENVIRONMENT_MAPPED_FP)
		{
			shader.needsNormal = true;
		}
		if (stages[i].bundle[0].alphaGen == alphaGen_t::AGEN_LIGHTING_SPECULAR || stages[i].bundle[0].rgbGen == colorGen_t::CGEN_LIGHTING_DIFFUSE)
		{
			shader.needsNormal = true;
		}
#endif
	}
	for (i = 0; i < shader.numDeforms; i++)
	{
		if (shader.deforms[i].deformation == deform_t::DEFORM_WAVE || shader.deforms[i].deformation == deform_t::DEFORM_NORMALS || shader.deforms[i].deformation == deform_t::DEFORM_BULGE)
		{
			shader.needsNormal = true;
		}
		if (shader.deforms[i].deformation >= deform_t::DEFORM_TEXT0 && shader.deforms[i].deformation <= deform_t::DEFORM_TEXT7)
		{
			shader.needsNormal = true;
		}
	}
}

struct ShaderDefInfo
{
	uint32_t tessFlags;
	Vk_Shader_Type shaderType;
};

constexpr auto GetShaderDefForBlendMode3(int blendMode)
{
	switch (blendMode)
	{
	case GL_MODULATE:
		return ShaderDefInfo{TESS_RGBA0 | TESS_ST0 | TESS_ST1 | TESS_ST2, Vk_Shader_Type::TYPE_MULTI_TEXTURE_MUL3};
	case GL_ADD:
		return ShaderDefInfo{TESS_RGBA0 | TESS_ST0 | TESS_ST1 | TESS_ST2, Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD3_1_1};
	case GL_ADD_NONIDENTITY:
		return ShaderDefInfo{TESS_RGBA0 | TESS_ST0 | TESS_ST1 | TESS_ST2, Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD3};
	case GL_BLEND_MODULATE:
		return ShaderDefInfo{TESS_RGBA0 | TESS_RGBA1 | TESS_RGBA2 | TESS_ST0 | TESS_ST1 | TESS_ST2, Vk_Shader_Type::TYPE_BLEND3_MUL};
	case GL_BLEND_ADD:
		return ShaderDefInfo{TESS_RGBA0 | TESS_RGBA1 | TESS_RGBA2 | TESS_ST0 | TESS_ST1 | TESS_ST2, Vk_Shader_Type::TYPE_BLEND3_ADD};
	case GL_BLEND_ALPHA:
		return ShaderDefInfo{TESS_RGBA0 | TESS_RGBA1 | TESS_RGBA2 | TESS_ST0 | TESS_ST1 | TESS_ST2, Vk_Shader_Type::TYPE_BLEND3_ALPHA};
	case GL_BLEND_ONE_MINUS_ALPHA:
		return ShaderDefInfo{TESS_RGBA0 | TESS_RGBA1 | TESS_RGBA2 | TESS_ST0 | TESS_ST1 | TESS_ST2, Vk_Shader_Type::TYPE_BLEND3_ONE_MINUS_ALPHA};
	case GL_BLEND_MIX_ONE_MINUS_ALPHA:
		return ShaderDefInfo{TESS_RGBA0 | TESS_RGBA1 | TESS_RGBA2 | TESS_ST0 | TESS_ST1 | TESS_ST2, Vk_Shader_Type::TYPE_BLEND3_MIX_ONE_MINUS_ALPHA};
	case GL_BLEND_MIX_ALPHA:
		return ShaderDefInfo{TESS_RGBA0 | TESS_RGBA1 | TESS_RGBA2 | TESS_ST0 | TESS_ST1 | TESS_ST2, Vk_Shader_Type::TYPE_BLEND3_MIX_ALPHA};
	case GL_BLEND_DST_COLOR_SRC_ALPHA:
		return ShaderDefInfo{TESS_RGBA0 | TESS_RGBA1 | TESS_RGBA2 | TESS_ST0 | TESS_ST1 | TESS_ST2, Vk_Shader_Type::TYPE_BLEND3_DST_COLOR_SRC_ALPHA};
	default:
		return ShaderDefInfo{0, Vk_Shader_Type::TYPE_SIGNLE_TEXTURE}; // Default case
	}
}

/*
=========================
FinishShader

Returns a freshly allocated shader with all the needed info
from the current global working shader
=========================
*/
static shader_t *FinishShader(void)
{
	int stage, i, m;
	uint32_t n;
	bool hasLightmapStage;
	bool vertexLightmap;
	bool colorBlend;
	bool depthMask;
	bool fogCollapse;
	shaderStage_t *lastStage[NUM_TEXTURE_BUNDLES];

	hasLightmapStage = false;
	vertexLightmap = false;
	colorBlend = false;
	depthMask = false;
	fogCollapse = false;

	//
	// set sky stuff appropriate
	//
	if (shader.isSky)
	{
		shader.sort = static_cast<float>(shaderSort_t::SS_ENVIRONMENT);
	}

	//
	// set polygon offset
	//
	if (shader.polygonOffset && shader.sort == static_cast<float>(shaderSort_t::SS_BAD))
	{
		shader.sort = static_cast<float>(shaderSort_t::SS_DECAL);
	}

	//
	// set appropriate stage information
	//
	for (stage = 0; stage < MAX_SHADER_STAGES;)
	{
		shaderStage_t *pStage = &stages[stage];

		if (!pStage->active)
		{
			break;
		}

		// check for a missing texture
		if (pStage->bundle[0].image[0] == NULL)
		{
			ri.Printf(PRINT_WARNING, "Shader %s has a stage with no image\n", shader.name);
			pStage->active = false;
			stage++;
			continue;
		}

		//
		// ditch this stage if it's detail and detail textures are disabled
		//
		if (pStage->isDetail && !r_detailTextures->integer)
		{
			int index;

			for (index = stage + 1; index < MAX_SHADER_STAGES; index++)
			{
				if (!stages[index].active)
					break;
			}

			if (index < MAX_SHADER_STAGES)
				memmove(pStage, pStage + 1, sizeof(*pStage) * (index - stage));
			else
			{
				if (stage + 1 < MAX_SHADER_STAGES)
					memmove(pStage, pStage + 1, sizeof(*pStage) * (index - stage - 1));

				Com_Memset(&stages[index - 1], 0, sizeof(*stages));
			}

			continue;
		}

		//
		// default texture coordinate generation
		//
		if (pStage->bundle[0].lightmap != LIGHTMAP_INDEX_NONE)
		{
			if (pStage->bundle[0].tcGen == texCoordGen_t::TCGEN_BAD)
			{
				pStage->bundle[0].tcGen = texCoordGen_t::TCGEN_LIGHTMAP;
			}
			hasLightmapStage = true;
		}
		else
		{
			if (pStage->bundle[0].tcGen == texCoordGen_t::TCGEN_BAD)
			{
				pStage->bundle[0].tcGen = texCoordGen_t::TCGEN_TEXTURE;
			}
		}

		// not a true lightmap but we want to leave existing
		// behaviour in place and not print out a warning
		// if (pStage.rgbGen == colorGen_t::CGEN_VERTEX) {
		//  vertexLightmap = true;
		//}

		if (pStage->stateBits & GLS_DEPTHMASK_TRUE)
		{
			depthMask = true;
		}

		//
		// determine fog color adjustment
		//
		if ((pStage->stateBits & (GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS)) &&
			(stages[0].stateBits & (GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS)))
		{
			int blendSrcBits = pStage->stateBits & GLS_SRCBLEND_BITS;
			int blendDstBits = pStage->stateBits & GLS_DSTBLEND_BITS;

			// fog color adjustment only works for blend modes that have a contribution
			// that aproaches 0 as the modulate values aproach 0 --
			// GL_ONE, GL_ONE
			// GL_ZERO, GL_ONE_MINUS_SRC_COLOR
			// GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA

			// modulate, additive
			if (((blendSrcBits == GLS_SRCBLEND_ONE) && (blendDstBits == GLS_DSTBLEND_ONE)) ||
				((blendSrcBits == GLS_SRCBLEND_ZERO) && (blendDstBits == GLS_DSTBLEND_ONE_MINUS_SRC_COLOR)))
			{
				pStage->bundle[0].adjustColorsForFog = acff_t::ACFF_MODULATE_RGB;
			}
			// strict blend
			else if ((blendSrcBits == GLS_SRCBLEND_SRC_ALPHA) && (blendDstBits == GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA))
			{
				pStage->bundle[0].adjustColorsForFog = acff_t::ACFF_MODULATE_ALPHA;
			}
			// premultiplied alpha
			else if ((blendSrcBits == GLS_SRCBLEND_ONE) && (blendDstBits == GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA))
			{
				pStage->bundle[0].adjustColorsForFog = acff_t::ACFF_MODULATE_RGBA;
			}
			else
			{
				// we can't adjust this one correctly, so it won't be exactly correct in fog
			}

			colorBlend = true;
		}

		stage++;
	}

	// there are times when you will need to manually apply a sort to
	// opaque alpha tested shaders that have later blend passes
	if (shader.sort == static_cast<float>(shaderSort_t::SS_BAD))
	{
		if (colorBlend)
		{
			// see through item, like a grill or grate
			if (depthMask)
			{
				shader.sort = static_cast<float>(shaderSort_t::SS_SEE_THROUGH);
			}
			else
			{
				shader.sort = static_cast<float>(shaderSort_t::SS_BLEND0);
			}
		}
		else
		{
			shader.sort = static_cast<float>(shaderSort_t::SS_OPAQUE);
		}
	}

	DetectNeeds();

	// fix alphaGen flags to avoid redundant comparisons in R_ComputeColors()
	for (i = 0; i < MAX_SHADER_STAGES; i++)
	{
		shaderStage_t &pStage = stages[i];
		if (!pStage.active)
			break;
		if (pStage.bundle[0].rgbGen == colorGen_t::CGEN_IDENTITY && pStage.bundle[0].alphaGen == alphaGen_t::AGEN_IDENTITY)
			pStage.bundle[0].alphaGen = alphaGen_t::AGEN_SKIP;
		else if (pStage.bundle[0].rgbGen == colorGen_t::CGEN_CONST && pStage.bundle[0].alphaGen == alphaGen_t::AGEN_CONST)
			pStage.bundle[0].alphaGen = alphaGen_t::AGEN_SKIP;
		else if (pStage.bundle[0].rgbGen == colorGen_t::CGEN_VERTEX && pStage.bundle[0].alphaGen == alphaGen_t::AGEN_VERTEX)
			pStage.bundle[0].alphaGen = alphaGen_t::AGEN_SKIP;
	}

	//
	// if we are in r_vertexLight mode, never use a lightmap texture
	//
	if (stage > 1 && ((r_vertexLight->integer && tr.vertexLightingAllowed && !shader.noVLcollapse) || glConfig.hardwareType == GLHW_PERMEDIA2))
	{
		VertexLightingCollapse();
		stage = 1;
		hasLightmapStage = false;
	}

	// whiteimage + "filter" texture == texture
	if (stage > 1 && stages[0].bundle[0].image[0] == tr.whiteImage && stages[0].bundle[0].numImageAnimations <= 1 && stages[0].bundle[0].rgbGen == colorGen_t::CGEN_IDENTITY && stages[0].bundle[0].alphaGen == alphaGen_t::AGEN_SKIP)
	{
		if (stages[1].stateBits == (GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO))
		{
			stages[1].stateBits = stages[0].stateBits & (GLS_DEPTHMASK_TRUE | GLS_DEPTHTEST_DISABLE | GLS_DEPTHFUNC_EQUAL);
			memmove(&stages[0], &stages[1], sizeof(stages[0]) * (stage - 1));
			stages[stage - 1].active = false;
			stage--;
		}
	}

	for (i = 0; i < stage; i++)
	{
		stages[i].numTexBundles = 1;
	}

	//
	// look for multitexture potential
	//
	if (r_ext_multitexture->integer)
	{
		for (i = 0; i < stage - 1; i++)
		{
			stage -= CollapseMultitexture(stages[i + 0].stateBits, stages[i + 0], stages[i + 1], stage - i);
		}
	}

	if (shader.lightmapIndex >= 0 && !hasLightmapStage)
	{
		if (vertexLightmap)
		{
			ri.Printf(PRINT_DEVELOPER, "WARNING: shader '%s' has VERTEX forced lightmap!\n", shader.name);
		}
		else
		{
			ri.Printf(PRINT_DEVELOPER, "WARNING: shader '%s' has lightmap but no lightmap stage!\n", shader.name);
			shader.lightmapIndex = LIGHTMAP_NONE;
		}
	}

	//
	// compute number of passes
	//
	shader.numUnfoggedPasses = stage;

	// fogonly shaders don't have any normal passes
	if (stage == 0 && !shader.isSky)
		shader.sort = static_cast<float>(shaderSort_t::SS_FOG);

	if (shader.sort <= static_cast<float>(shaderSort_t::SS_OPAQUE))
	{
		shader.fogPass = fogPass_t::FP_EQUAL;
	}
	else if (shader.contentFlags & CONTENTS_FOG)
	{
		shader.fogPass = fogPass_t::FP_LE;
	}

#ifdef USE_FOG_COLLAPSE
	if (vk_inst.maxBoundDescriptorSets >= 6 && !(shader.contentFlags & CONTENTS_FOG) && shader.fogPass != fogPass_t::FP_NONE)
	{
		fogCollapse = true;
		if (stage == 1)
		{
			// we can always fog-collapse signle-stage shaders
		}
		else
		{
			if (tr.numFogs)
			{
				// check for (un)acceptable blend modes
				for (i = 0; i < stage; i++)
				{
					const uint32_t blendBits = stages[i].stateBits & GLS_BLEND_BITS;
					switch (blendBits & GLS_SRCBLEND_BITS)
					{
					case GLS_SRCBLEND_DST_COLOR:
					case GLS_SRCBLEND_ONE_MINUS_DST_COLOR:
					case GLS_SRCBLEND_DST_ALPHA:
					case GLS_SRCBLEND_ONE_MINUS_DST_ALPHA:
						fogCollapse = false;
						break;
					}
					switch (blendBits & GLS_DSTBLEND_BITS)
					{
					case GLS_DSTBLEND_DST_ALPHA:
					case GLS_DSTBLEND_ONE_MINUS_DST_ALPHA:
						fogCollapse = false;
						break;
					}
				}
				if (fogCollapse)
				{
					for (i = 1; i < stage; i++)
					{
						const uint32_t blendBits = stages[i].stateBits & GLS_BLEND_BITS;
						if (blendBits == (GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA) || blendBits == (GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA))
						{
							if (stages[i].bundle[0].adjustColorsForFog == acff_t::ACFF_NONE)
							{
								fogCollapse = false;
								break;
							}
						}
					}
				}
				if (fogCollapse)
				{
					// correct add mode
					for (i = 1; i < stage; i++)
					{
						const uint32_t blendBits = stages[i].stateBits & GLS_BLEND_BITS;
						if (blendBits == (GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE))
						{
							stages[i].bundle[0].adjustColorsForFog = acff_t::ACFF_MODULATE_RGBA;
						}
					}
				}
			}
		}
	}
	if (tr.numFogs == 0)
	{
		// if there is no fogs - assume that we can apply all color optimizations without any restrictions
		fogCollapse = true;
	}
#endif

	shader.tessFlags = TESS_XYZ;

	{
		Vk_Pipeline_Def def;

		Com_Memset(&def, 0, sizeof(def));
		def.face_culling = shader.cullType;
		def.polygon_offset = shader.polygonOffset;

		if ((stages[0].stateBits & GLS_DEPTHMASK_TRUE) == 0)
		{
			def.allow_discard = 1;
		}

		for (i = 0; i < stage; i++)
		{
			int env_mask;
			shaderStage_t &pStage = stages[i];
			def.state_bits = pStage.stateBits;

			if (pStage.mtEnv3)
			{
				auto shaderDef = GetShaderDefForBlendMode3(pStage.mtEnv3);
				if (shaderDef.tessFlags == 0)
					break;

				pStage.tessFlags = shaderDef.tessFlags;
				def.shader_type = shaderDef.shaderType;
			}
			else
			{
				switch (pStage.mtEnv)
				{
				case GL_MODULATE:
					pStage.tessFlags = TESS_RGBA0 | TESS_ST0 | TESS_ST1;
					def.shader_type = Vk_Shader_Type::TYPE_MULTI_TEXTURE_MUL2;
					if ((pStage.bundle[0].adjustColorsForFog == acff_t::ACFF_NONE && pStage.bundle[1].adjustColorsForFog == acff_t::ACFF_NONE) || fogCollapse)
					{
						if (pStage.bundle[0].rgbGen == colorGen_t::CGEN_IDENTITY && pStage.bundle[1].rgbGen == colorGen_t::CGEN_IDENTITY)
						{
							if (pStage.bundle[1].alphaGen == alphaGen_t::AGEN_SKIP && pStage.bundle[0].alphaGen == alphaGen_t::AGEN_SKIP)
							{
								pStage.tessFlags = TESS_ST0 | TESS_ST1;
								def.shader_type = Vk_Shader_Type::TYPE_MULTI_TEXTURE_MUL2_IDENTITY;
							}
						}
						else if (pStage.bundle[0].rgbGen == colorGen_t::CGEN_IDENTITY_LIGHTING && pStage.bundle[1].rgbGen == colorGen_t::CGEN_IDENTITY_LIGHTING && pStage.bundle[0].alphaGen == pStage.bundle[1].alphaGen)
						{
							if (pStage.bundle[0].alphaGen == alphaGen_t::AGEN_SKIP || pStage.bundle[0].alphaGen == alphaGen_t::AGEN_IDENTITY)
							{
								pStage.tessFlags = TESS_ST0 | TESS_ST1;
								def.shader_type = Vk_Shader_Type::TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR;
								def.color.rgb = tr.identityLightByte;
								def.color.alpha = pStage.bundle[0].alphaGen == alphaGen_t::AGEN_IDENTITY ? 255 : tr.identityLightByte;
							}
						}
					}
					break;
				case GL_ADD:
					pStage.tessFlags = TESS_RGBA0 | TESS_ST0 | TESS_ST1;
					def.shader_type = Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD2_1_1;
					if ((pStage.bundle[0].adjustColorsForFog == acff_t::ACFF_NONE && pStage.bundle[1].adjustColorsForFog == acff_t::ACFF_NONE) || fogCollapse)
					{
						if (pStage.bundle[0].rgbGen == colorGen_t::CGEN_IDENTITY && pStage.bundle[1].rgbGen == colorGen_t::CGEN_IDENTITY)
						{
							if (pStage.bundle[0].alphaGen == alphaGen_t::AGEN_SKIP && pStage.bundle[1].alphaGen == alphaGen_t::AGEN_SKIP)
							{
								pStage.tessFlags = TESS_ST0 | TESS_ST1;
								def.shader_type = Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD2_IDENTITY;
							}
						}
						else if (pStage.bundle[0].rgbGen == colorGen_t::CGEN_IDENTITY_LIGHTING && pStage.bundle[1].rgbGen == colorGen_t::CGEN_IDENTITY_LIGHTING && pStage.bundle[0].alphaGen == pStage.bundle[1].alphaGen)
						{
							if (pStage.bundle[0].alphaGen == alphaGen_t::AGEN_SKIP || pStage.bundle[0].alphaGen == alphaGen_t::AGEN_IDENTITY)
							{
								pStage.tessFlags = TESS_ST0 | TESS_ST1;
								def.shader_type = Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR;
								def.color.rgb = tr.identityLightByte;
								def.color.alpha = pStage.bundle[0].alphaGen == alphaGen_t::AGEN_IDENTITY ? 255 : tr.identityLightByte;
							}
						}
					}
					break;
				case GL_ADD_NONIDENTITY:
					pStage.tessFlags = TESS_RGBA0 | TESS_ST0 | TESS_ST1;
					def.shader_type = Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD2;
					if ((pStage.bundle[0].adjustColorsForFog == acff_t::ACFF_NONE && pStage.bundle[1].adjustColorsForFog == acff_t::ACFF_NONE) || fogCollapse)
					{
						if (pStage.bundle[0].rgbGen == colorGen_t::CGEN_IDENTITY && pStage.bundle[1].rgbGen == colorGen_t::CGEN_IDENTITY)
						{
							if (pStage.bundle[0].alphaGen == alphaGen_t::AGEN_SKIP && pStage.bundle[1].alphaGen == alphaGen_t::AGEN_SKIP)
							{
								pStage.tessFlags = TESS_ST0 | TESS_ST1;
								def.shader_type = Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD2_IDENTITY;
							}
						}
						else if (pStage.bundle[0].rgbGen == colorGen_t::CGEN_IDENTITY_LIGHTING && pStage.bundle[1].rgbGen == colorGen_t::CGEN_IDENTITY_LIGHTING && pStage.bundle[0].alphaGen == pStage.bundle[1].alphaGen)
						{
							if (pStage.bundle[0].alphaGen == alphaGen_t::AGEN_SKIP || pStage.bundle[0].alphaGen == alphaGen_t::AGEN_IDENTITY)
							{
								pStage.tessFlags = TESS_ST0 | TESS_ST1;
								def.shader_type = Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR;
								def.color.rgb = tr.identityLightByte;
								def.color.alpha = pStage.bundle[0].alphaGen == alphaGen_t::AGEN_IDENTITY ? 255 : tr.identityLightByte;
							}
						}
					}
					break;
					// extended blending modes
				case GL_BLEND_MODULATE:
					pStage.tessFlags = TESS_RGBA0 | TESS_RGBA1 | TESS_ST0 | TESS_ST1;
					def.shader_type = Vk_Shader_Type::TYPE_BLEND2_MUL;
					break;
				case GL_BLEND_ADD:
					pStage.tessFlags = TESS_RGBA0 | TESS_RGBA1 | TESS_ST0 | TESS_ST1;
					def.shader_type = Vk_Shader_Type::TYPE_BLEND2_ADD;
					break;
				case GL_BLEND_ALPHA:
					pStage.tessFlags = TESS_RGBA0 | TESS_RGBA1 | TESS_ST0 | TESS_ST1;
					def.shader_type = Vk_Shader_Type::TYPE_BLEND2_ALPHA;
					break;
				case GL_BLEND_ONE_MINUS_ALPHA:
					pStage.tessFlags = TESS_RGBA0 | TESS_RGBA1 | TESS_ST0 | TESS_ST1;
					def.shader_type = Vk_Shader_Type::TYPE_BLEND2_ONE_MINUS_ALPHA;
					break;
				case GL_BLEND_MIX_ALPHA:
					pStage.tessFlags = TESS_RGBA0 | TESS_RGBA1 | TESS_ST0 | TESS_ST1;
					def.shader_type = Vk_Shader_Type::TYPE_BLEND2_MIX_ALPHA;
					break;
				case GL_BLEND_MIX_ONE_MINUS_ALPHA:
					pStage.tessFlags = TESS_RGBA0 | TESS_RGBA1 | TESS_ST0 | TESS_ST1;
					def.shader_type = Vk_Shader_Type::TYPE_BLEND2_MIX_ONE_MINUS_ALPHA;
					break;
				case GL_BLEND_DST_COLOR_SRC_ALPHA:
					pStage.tessFlags = TESS_RGBA0 | TESS_RGBA1 | TESS_ST0 | TESS_ST1;
					def.shader_type = Vk_Shader_Type::TYPE_BLEND2_DST_COLOR_SRC_ALPHA;
					break;

				default:
					pStage.tessFlags = TESS_RGBA0 | TESS_ST0;
					def.shader_type = Vk_Shader_Type::TYPE_SIGNLE_TEXTURE;
					if (pStage.bundle[0].adjustColorsForFog == acff_t::ACFF_NONE || fogCollapse)
					{
						if (pStage.bundle[0].rgbGen == colorGen_t::CGEN_IDENTITY)
						{
							if (pStage.bundle[0].alphaGen == alphaGen_t::AGEN_SKIP)
							{
								pStage.tessFlags = TESS_ST0;
								def.shader_type = Vk_Shader_Type::TYPE_SIGNLE_TEXTURE_IDENTITY;
							}
						}
						else if (pStage.bundle[0].rgbGen == colorGen_t::CGEN_IDENTITY_LIGHTING)
						{
							if (pStage.bundle[0].alphaGen == alphaGen_t::AGEN_SKIP || pStage.bundle[0].alphaGen == alphaGen_t::AGEN_IDENTITY)
							{
								pStage.tessFlags = TESS_ST0;
								def.shader_type = Vk_Shader_Type::TYPE_SIGNLE_TEXTURE_FIXED_COLOR;
								def.color.rgb = tr.identityLightByte;
								def.color.alpha = pStage.bundle[0].alphaGen == alphaGen_t::AGEN_IDENTITY ? 255 : tr.identityLightByte;
							}
						}
						else if (pStage.bundle[0].rgbGen == colorGen_t::CGEN_ENTITY)
						{
							if (pStage.bundle[0].alphaGen == alphaGen_t::AGEN_ENTITY || pStage.bundle[0].alphaGen == alphaGen_t::AGEN_SKIP || pStage.bundle[0].alphaGen == alphaGen_t::AGEN_IDENTITY)
							{
								pStage.tessFlags = TESS_ST0 | TESS_ENT0;
								def.shader_type = Vk_Shader_Type::TYPE_SIGNLE_TEXTURE_ENT_COLOR;
							}
						}
					}
					break;
				}
			} // switch mtEnv3 / mtEnv

			for (env_mask = 0, n = 0; n < pStage.numTexBundles; n++)
			{
				if (pStage.bundle[n].numTexMods)
				{
					continue;
				}
				if (pStage.bundle[n].tcGen == texCoordGen_t::TCGEN_ENVIRONMENT_MAPPED && (pStage.bundle[n].lightmap == LIGHTMAP_INDEX_NONE || !tr.mergeLightmaps))
				{
					env_mask |= (1 << n);
				}
			}

			if (env_mask == 1 && !pStage.depthFragment)
			{
				if (def.shader_type >= Vk_Shader_Type::TYPE_GENERIC_BEGIN && def.shader_type <= Vk_Shader_Type::TYPE_GENERIC_END)
				{
					int prev = static_cast<int>(def.shader_type);
					def.shader_type = static_cast<Vk_Shader_Type>(prev++); // switch to *_ENV version
					shader.tessFlags |= TESS_NNN | TESS_VPOS;
					pStage.tessFlags &= ~TESS_ST0;
					pStage.tessFlags |= TESS_ENV;
					pStage.bundle[0].tcGen = texCoordGen_t::TCGEN_BAD;
				}
			}

			def.mirror = false;
			pStage.vk_pipeline[0] = vk_find_pipeline_ext(0, def, true);
			def.mirror = true;
			pStage.vk_mirror_pipeline[0] = vk_find_pipeline_ext(0, def, false);

			if (pStage.depthFragment)
			{
				def.mirror = false;
				def.shader_type = Vk_Shader_Type::TYPE_SIGNLE_TEXTURE_DF;
				pStage.vk_pipeline_df = vk_find_pipeline_ext(0, def, true);
				def.mirror = true;
				def.shader_type = Vk_Shader_Type::TYPE_SIGNLE_TEXTURE_DF;
				pStage.vk_mirror_pipeline_df = vk_find_pipeline_ext(0, def, false);
			}

#ifdef USE_FOG_COLLAPSE
			if (fogCollapse && tr.numFogs > 0)
			{
				Vk_Pipeline_Def def;
				Vk_Pipeline_Def def_mirror;

				vk_get_pipeline_def(pStage.vk_pipeline[0], def);
				vk_get_pipeline_def(pStage.vk_mirror_pipeline[0], def_mirror);

				def.fog_stage = 1;
				def_mirror.fog_stage = 1;
				def.acff = static_cast<int>(pStage.bundle[0].adjustColorsForFog);
				def_mirror.acff = static_cast<int>(pStage.bundle[0].adjustColorsForFog);

				pStage.vk_pipeline[1] = vk_find_pipeline_ext(0, def, false);
				pStage.vk_mirror_pipeline[1] = vk_find_pipeline_ext(0, def_mirror, false);

				pStage.bundle[0].adjustColorsForFog = acff_t::ACFF_NONE; // will be handled in shader from now

				shader.fogCollapse = true;
			}
#endif
		}
	}

#ifdef USE_PMLIGHT
	FindLightingStages();
#endif

#if 1
	// try to avoid redundant per-stage computations
	Com_Memset(lastStage, 0, sizeof(lastStage));
	for (i = 0; i < shader.numUnfoggedPasses - 1; i++)
	{
		if (!stages[i + 1].active)
			break;
		for (n = 0; n < NUM_TEXTURE_BUNDLES; n++)
		{
			if (stages[i].bundle[n].image[0] != NULL)
			{
				lastStage[n] = &stages[i];
			}
			if (EqualTCgen(n, lastStage[n], &stages[i + 1]) && (lastStage[n]->tessFlags & (TESS_ST0 << n)))
			{
				stages[i + 1].tessFlags &= ~(TESS_ST0 << n);
			}
			if (EqualRGBgen(lastStage[n], &stages[i + 1]) && EqualACgen(lastStage[n], &stages[i + 1]) && (lastStage[n]->tessFlags & (TESS_RGBA0 << n)))
			{
				stages[i + 1].tessFlags &= ~(TESS_RGBA0 << n);
			}
		}
	}
#endif

	// make sure that amplitude for texMod_t::TMOD_STRETCH is not zero
	for (i = 0; i < shader.numUnfoggedPasses; i++)
	{
		if (!stages[i].active)
		{
			continue;
		}
		for (n = 0; n < stages[i].numTexBundles; n++)
		{
			for (m = 0; m < stages[i].bundle[n].numTexMods; m++)
			{
				if (stages[i].bundle[n].texMods[m].type == texMod_t::TMOD_STRETCH)
				{
					if (fabsf(stages[i].bundle[n].texMods[m].wave.amplitude) < 1e-6f)
					{
						if (stages[i].bundle[n].texMods[m].wave.amplitude >= 0.0f)
						{
							stages[i].bundle[n].texMods[m].wave.amplitude = 1e-6f;
						}
						else
						{
							stages[i].bundle[n].texMods[m].wave.amplitude = -1e-6f;
						}
					}
				}
			}
		}
	}

	// determine which stage iterator function is appropriate
	ComputeStageIteratorFunc();

	return GeneratePermanentShader();
}

//========================================================================================

/*
====================
FindShaderInShaderText

Scans the combined text description of all the shader files for
the given shader name.

return NULL if not found

If found, it will return a valid shader
=====================
*/
static const char *FindShaderInShaderText(std::string_view shadername)
{

	std::string_view token;
	const char *p;
	int i, hash;

	hash = generateHashValue(shadername, MAX_SHADERTEXT_HASH);

	if (shaderTextHashTable[hash])
	{
		for (i = 0; shaderTextHashTable[hash][i]; i++)
		{
			p = shaderTextHashTable[hash][i];
			token = COM_ParseExt_cpp(&p, true);
			if (!Q_stricmp_cpp(token, shadername))
				return p;
		}
	}

	return nullptr;
}

/*
==================
R_FindShaderByName

Will always return a valid shader, but it might be the
default shader if the real one can't be found.
==================
*/
shader_t *R_FindShaderByName(std::string_view name)
{
	std::array<char, MAX_QPATH> strippedName;
	int hash;
	shader_t *sh;

	if (name.empty())
	{
		return tr.defaultShader;
	}

	COM_StripExtension_cpp(name, strippedName);

	hash = generateHashValue(to_str_view(strippedName), FILE_HASH_SIZE);

	//
	// see if the shader is already loaded
	//
	for (sh = shaderHashTable[hash]; sh; sh = sh->next)
	{
		// NOTE: if there was no shader or image available with the name strippedName
		// then a default shader is created with lightmapIndex == LIGHTMAP_NONE, so we
		// have to check all default shaders otherwise for every call to R_FindShader
		// with that same strippedName a new default shader is created.
		if (Q_stricmp_cpp(sh->name, to_str_view(strippedName)) == 0)
		{
			// match found
			return sh;
		}
	}

	return tr.defaultShader;
}

/*
===============
R_CreateDefaultShading
===============
*/
static void R_CreateDefaultShading(image_t &image)
{
	if (shader.lightmapIndex == LIGHTMAP_NONE)
	{
		// dynamic colors at vertexes
		stages[0].bundle[0].image[0] = &image;
		stages[0].active = true;
		stages[0].bundle[0].tcGen = texCoordGen_t::TCGEN_TEXTURE;
		stages[0].bundle[0].rgbGen = colorGen_t::CGEN_LIGHTING_DIFFUSE;
		stages[0].stateBits = GLS_DEFAULT;
	}
	else if (shader.lightmapIndex == LIGHTMAP_BY_VERTEX)
	{
		// explicit colors at vertexes
		stages[0].bundle[0].image[0] = &image;
		stages[0].active = true;
		stages[0].bundle[0].tcGen = texCoordGen_t::TCGEN_TEXTURE;
		stages[0].bundle[0].rgbGen = colorGen_t::CGEN_EXACT_VERTEX;
		stages[0].bundle[0].alphaGen = alphaGen_t::AGEN_SKIP;
		stages[0].stateBits = GLS_DEFAULT;
	}
	else if (shader.lightmapIndex == LIGHTMAP_2D)
	{
		// GUI elements
		stages[0].bundle[0].image[0] = &image;
		stages[0].active = true;
		stages[0].bundle[0].tcGen = texCoordGen_t::TCGEN_TEXTURE;
		stages[0].bundle[0].rgbGen = colorGen_t::CGEN_VERTEX;
		stages[0].bundle[0].alphaGen = alphaGen_t::AGEN_VERTEX;
		stages[0].stateBits = GLS_DEPTHTEST_DISABLE |
							  GLS_SRCBLEND_SRC_ALPHA |
							  GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
	}
	else if (shader.lightmapIndex == LIGHTMAP_WHITEIMAGE)
	{
		// fullbright level
		stages[0].active = true;
		stages[0].bundle[0].image[0] = &image;
		stages[0].bundle[0].tcGen = texCoordGen_t::TCGEN_TEXTURE;
		stages[0].bundle[0].rgbGen = colorGen_t::CGEN_IDENTITY_LIGHTING;
		stages[0].stateBits = GLS_DEFAULT;
	}
	else
	{
		// two pass lightmap
		stages[0].bundle[0].image[0] = tr.lightmaps[shader.lightmapIndex];
		stages[0].bundle[0].lightmap = LIGHTMAP_INDEX_SHADER;
		stages[0].active = true;
		stages[0].bundle[0].tcGen = texCoordGen_t::TCGEN_LIGHTMAP;
		stages[0].bundle[0].rgbGen = colorGen_t::CGEN_IDENTITY; // lightmaps are scaled on creation
		// for identitylight
		stages[0].stateBits = GLS_DEFAULT;

		stages[1].bundle[0].image[0] = &image;
		stages[1].active = true;
		stages[1].bundle[0].tcGen = texCoordGen_t::TCGEN_TEXTURE;
		stages[1].bundle[0].rgbGen = colorGen_t::CGEN_IDENTITY;
		stages[1].stateBits |= GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO;
	}
}

/*
===============
R_FindShader

Will always return a valid shader, but it might be the
default shader if the real one can't be found.

In the interest of not requiring an explicit shader text entry to
be defined for every single image used in the game, three default
shader behaviors can be auto-created for any image:

If lightmapIndex == LIGHTMAP_NONE, then the image will have
dynamic diffuse lighting applied to it, as appropriate for most
entity skin surfaces.

If lightmapIndex == LIGHTMAP_2D, then the image will be used
for 2D rendering unless an explicit shader is found

If lightmapIndex == LIGHTMAP_BY_VERTEX, then the image will use
the vertex rgba modulate values, as appropriate for misc_model
pre-lit surfaces.

Other lightmapIndex values will have a lightmap stage created
and src*dest blending applied with the texture, as appropriate for
most world construction surfaces.

===============
*/
shader_t *R_FindShader(std::string_view name, int lightmapIndex, const bool mipRawImage)
{
	std::array<char, MAX_QPATH> strippedName;
	unsigned long hash;
	const char *shaderText;
	image_t *image;
	shader_t *sh;

	if (name.empty())
	{
		return tr.defaultShader;
	}

	// use (fullbright) vertex lighting if the bsp file doesn't have
	// lightmaps
	if (lightmapIndex >= 0 && lightmapIndex >= tr.numLightmaps)
	{
		lightmapIndex = LIGHTMAP_BY_VERTEX;
	}
	else if (lightmapIndex < LIGHTMAP_2D)
	{
		// negative lightmap indexes cause stray pointers (think tr.lightmaps[lightmapIndex])
		ri.Printf(PRINT_WARNING, "WARNING: shader '%s' has invalid lightmap index of %d\n", name.data(), lightmapIndex);
		lightmapIndex = LIGHTMAP_BY_VERTEX;
	}

	COM_StripExtension_cpp(name, strippedName);

	hash = generateHashValue(to_str_view(strippedName), FILE_HASH_SIZE);

	//
	// see if the shader is already loaded
	//
	for (sh = shaderHashTable[hash]; sh; sh = sh->next)
	{
		// NOTE: if there was no shader or image available with the name strippedName
		// then a default shader is created with lightmapIndex == LIGHTMAP_NONE, so we
		// have to check all default shaders otherwise for every call to R_FindShader
		// with that same strippedName a new default shader is created.
		if ((sh->lightmapSearchIndex == lightmapIndex || sh->defaultShader) && !Q_stricmp_cpp(sh->name, strippedName.data()))
		{
			// match found
			return sh;
		}
	}

	InitShader(strippedName.data(), lightmapIndex);

	// FIXME: set these "need" values appropriately
	// shader.needsNormal = true;
	// shader.needsST1 = true;
	// shader.needsST2 = true;
	// shader.needsColor = true;

	//
	// attempt to define shader from an explicit parameter file
	//
	shaderText = FindShaderInShaderText(strippedName.data());
	if (shaderText)
	{
		// enable this when building a pak file to get a global list
		// of all explicit shaders
		if (r_printShaders->integer)
		{
			ri.Printf(PRINT_ALL, "*SHADER* %s\n", name.data());
		}

		if (!ParseShader(&shaderText))
		{
			// had errors, so use default shader
			shader.defaultShader = true;
		}

		return FinishShader();
	}

	//
	// if not defined in the in-memory shader descriptions,
	// look for a single supported image file
	//
	{
		imgFlags_t flags;

		flags = imgFlags_t::IMGFLAG_NONE;

		if (mipRawImage)
		{
			flags = static_cast<imgFlags_t>(flags | imgFlags_t::IMGFLAG_MIPMAP | imgFlags_t::IMGFLAG_PICMIP);
		}
		else
		{
			flags = static_cast<imgFlags_t>(flags | imgFlags_t::IMGFLAG_CLAMPTOEDGE);
		}

		image = R_FindImageFile(name, flags);
		if (!image)
		{
			ri.Printf(PRINT_DEVELOPER, "Couldn't find image file for shader %s\n", name.data());
			shader.defaultShader = true;
			return FinishShader();
		}
	}

	//
	// create the default shading commands
	//
	R_CreateDefaultShading(*image);

	return FinishShader();
}

qhandle_t RE_RegisterShaderFromImage(std::string_view name, int lightmapIndex, image_t &image, bool mipRawImage)
{
	unsigned long hash;
	shader_t *sh;

	hash = generateHashValue(name, FILE_HASH_SIZE);

	// probably not necessary since this function
	// only gets called from tr_font.c with lightmapIndex == LIGHTMAP_2D
	// but better safe than sorry.
	if (lightmapIndex >= tr.numLightmaps)
	{
		lightmapIndex = LIGHTMAP_WHITEIMAGE;
	}

	//
	// see if the shader is already loaded
	//
	for (sh = shaderHashTable[hash]; sh; sh = sh->next)
	{
		// NOTE: if there was no shader or image available with the name strippedName
		// then a default shader is created with lightmapIndex == LIGHTMAP_NONE, so we
		// have to check all default shaders otherwise for every call to R_FindShader
		// with that same strippedName a new default shader is created.
		if ((sh->lightmapSearchIndex == lightmapIndex || sh->defaultShader) && !Q_stricmp_cpp(sh->name, name))
		{
			// match found
			return sh->index;
		}
	}

	InitShader(name.data(), lightmapIndex);

	// FIXME: set these "need" values appropriately
	// shader.needsNormal = true;
	// shader.needsST1 = true;
	// shader.needsST2 = true;
	// shader.needsColor = true;

	//
	// create the default shading commands
	//
	R_CreateDefaultShading(image);

	sh = FinishShader();

	return sh->index;
}

/*
====================
RE_RegisterShaderLightMap

This is the exported shader entry point for the rest of the system
It will always return an index that will be valid.

This should really only be used for explicit shaders, because there is no
way to ask for different implicit lighting modes (vertex, lightmap, etc)
====================
*/
qhandle_t RE_RegisterShaderLightMap(std::string_view name, int lightmapIndex)
{
	shader_t *sh;

	if (name.length() >= MAX_QPATH)
	{
		ri.Printf(PRINT_ALL, "Shader name exceeds MAX_QPATH\n");
		return 0;
	}

	sh = R_FindShader(name, lightmapIndex, true);

	// we want to return 0 if the shader failed to
	// load for some reason, but R_FindShader should
	// still keep a name allocated for it, so if
	// something calls RE_RegisterShader again with
	// the same name, we don't try looking for it again
	if (sh->defaultShader)
	{
		return 0;
	}

	return sh->index;
}

/*
====================
RE_RegisterShader

This is the exported shader entry point for the rest of the system
It will always return an index that will be valid.

This should really only be used for explicit shaders, because there is no
way to ask for different implicit lighting modes (vertex, lightmap, etc)
====================
*/
qhandle_t RE_RegisterShader(const char *name)
{
	if (!name)
	{
		ri.Printf(PRINT_ALL, "NULL shader\n");
		return 0;
	}

	if (strlen(name) >= MAX_QPATH)
	{
		ri.Printf(PRINT_ALL, "Shader name exceeds MAX_QPATH\n");
		return 0;
	}

	shader_t *sh = R_FindShader(name, LIGHTMAP_2D, true);

	// we want to return 0 if the shader failed to
	// load for some reason, but R_FindShader should
	// still keep a name allocated for it, so if
	// something calls RE_RegisterShader again with
	// the same name, we don't try looking for it again
	if (sh->defaultShader)
	{
		return 0;
	}

	return sh->index;
}

/*
====================
RE_RegisterShaderNoMip

For menu graphics that should never be picmiped
====================
*/
qhandle_t RE_RegisterShaderNoMip(const char *name)
{
	shader_t *sh;

	if (strlen(name) >= MAX_QPATH)
	{
		ri.Printf(PRINT_ALL, "Shader name exceeds MAX_QPATH\n");
		return 0;
	}

	sh = R_FindShader(name, LIGHTMAP_2D, false);

	// we want to return 0 if the shader failed to
	// load for some reason, but R_FindShader should
	// still keep a name allocated for it, so if
	// something calls RE_RegisterShader again with
	// the same name, we don't try looking for it again
	if (sh->defaultShader)
	{
		return 0;
	}

	return sh->index;
}

/*
====================
R_GetShaderByHandle

When a handle is passed in by another module, this range checks
it and returns a valid (possibly default) shader_t to be used internally.
====================
*/
shader_t *R_GetShaderByHandle(qhandle_t hShader)
{
	if (hShader < 0)
	{
		ri.Printf(PRINT_WARNING, "R_GetShaderByHandle: out of range hShader '%d'\n", hShader);
		return tr.defaultShader;
	}
	if (hShader >= tr.numShaders)
	{
		ri.Printf(PRINT_WARNING, "R_GetShaderByHandle: out of range hShader '%d'\n", hShader);
		return tr.defaultShader;
	}
	return tr.shaders[hShader];
}

/*
===============
R_ShaderList_f

Dump information on all valid shaders to the console
A second parameter will cause it to print in sorted order
===============
*/
void R_ShaderList_f(void)
{
	int i;
	int count;
	const shader_t *sh;

	ri.Printf(PRINT_ALL, "-----------------------\n");

	count = 0;
	for (i = 0; i < tr.numShaders; i++)
	{
		if (ri.Cmd_Argc() > 1)
		{
			sh = tr.sortedShaders[i];
		}
		else
		{
			sh = tr.shaders[i];
		}

		ri.Printf(PRINT_ALL, "%i ", sh->numUnfoggedPasses);

		if (sh->lightmapIndex >= 0)
		{
			ri.Printf(PRINT_ALL, "L ");
		}
		else
		{
			ri.Printf(PRINT_ALL, "  ");
		}
		if (sh->multitextureEnv)
		{
			ri.Printf(PRINT_ALL, "MT(x) "); // TODO: per-stage statistics?
		}
		else
		{
			ri.Printf(PRINT_ALL, "      ");
		}
		if (sh->explicitlyDefined)
		{
			ri.Printf(PRINT_ALL, "E ");
		}
		else
		{
			ri.Printf(PRINT_ALL, "  ");
		}

		if (sh->optimalStageIteratorFunc == RB_StageIteratorGeneric)
		{
			ri.Printf(PRINT_ALL, "gen ");
		}
		else if (sh->optimalStageIteratorFunc == RB_StageIteratorSky)
		{
			ri.Printf(PRINT_ALL, "sky ");
		}
		else
		{
			ri.Printf(PRINT_ALL, "    ");
		}

		if (sh->defaultShader)
		{
			ri.Printf(PRINT_ALL, ": %s (DEFAULTED)\n", sh->name);
		}
		else
		{
			ri.Printf(PRINT_ALL, ": %s\n", sh->name);
		}
		count++;
	}
	ri.Printf(PRINT_ALL, "%i total shaders\n", count);
	ri.Printf(PRINT_ALL, "------------------\n");
}

constexpr int MAX_SHADER_FILES = 16384;

static int loadShaderBuffers(char **shaderFiles, const int numShaderFiles, char **buffers)
{
	std::array<char, MAX_QPATH + 8> filename;
	std::array<char, MAX_QPATH> shaderName;
	const char *p;
	std::string_view token;
	long summand, sum = 0;
	int shaderLine;
	int i;
	const char *shaderStart;
	bool denyErrors;

	// load and parse shader files
	for (i = 0; i < numShaderFiles; i++)
	{
		Com_sprintf(filename.data(), sizeof(filename), "scripts/%s", shaderFiles[i]);
		// ri.Printf( PRINT_DEVELOPER, "...loading '%s'\n", filename );
		summand = ri.FS_ReadFile(filename.data(), (void **)&buffers[i]);

		if (!buffers[i])
			ri.Error(ERR_DROP, "Couldn't load %s", filename.data());

		// comment some buggy shaders from pak0
		if (summand == 35910 && strcmp(shaderFiles[i], "sky.shader") == 0)
		{
			if (memcmp(buffers[i] + 0x3D3E, "\tcloudparms ", 12) == 0)
			{
				memcpy(buffers[i] + 0x27D7, "/*", 2);
				memcpy(buffers[i] + 0x2A93, "*/", 2);

				memcpy(buffers[i] + 0x3CA9, "/*", 2);
				memcpy(buffers[i] + 0x3FC2, "*/", 2);
			}
		}
		else if (summand == 116073 && strcmp(shaderFiles[i], "sfx.shader") == 0)
		{
			if (memcmp(buffers[i] + 93457, "textures/sfx/xfinalfog\r\n", 24) == 0)
			{
				memcpy(buffers[i] + 93457, "/*", 2);
				memcpy(buffers[i] + 93663, "*/", 2);
			}
		}

		p = buffers[i];
		COM_BeginParseSession_cpp(filename.data());

		shaderStart = NULL;
		denyErrors = false;

		while (1)
		{
			token = COM_ParseExt_cpp(&p, true);

			if (token.empty())
				break;

			Q_strncpyz_cpp(shaderName, token, sizeof(shaderName));
			shaderLine = COM_GetCurrentParseLine_cpp();

			token = COM_ParseExt_cpp(&p, true);
			if (token[0] != '{' || (token.size() > 1 && token[1] != '\0'))
			{
				ri.Printf(PRINT_DEVELOPER, "File %s: shader \"%s\" "
										   "on line %d missing opening brace",
						  filename.data(), shaderName.data(), shaderLine);
				if (token.empty())
					ri.Printf(PRINT_DEVELOPER, " (found \"%s\" on line %d)\n", token.data(), COM_GetCurrentParseLine_cpp());
				else
					ri.Printf(PRINT_DEVELOPER, "\n");

				if (denyErrors || !p)
				{
					ri.Printf(PRINT_WARNING, "Ignoring entire file '%s' due to error.\n", filename.data());
					ri.FS_FreeFile(buffers[i]);
					buffers[i] = NULL;
					break;
				}

				SkipRestOfLine_cpp(&p);
				shaderStart = p;
				continue;
			}

			if (!SkipBracedSection_cpp(&p, 1))
			{
				ri.Printf(PRINT_WARNING, "WARNING: Ignoring shader file %s. Shader \"%s\" "
										 "on line %d missing closing brace.\n",
						  filename.data(), shaderName.data(), shaderLine);
				ri.FS_FreeFile(buffers[i]);
				buffers[i] = NULL;
				break;
			}

			denyErrors = true;
		}

		if (buffers[i])
		{
			if (shaderStart)
			{
				summand -= (shaderStart - buffers[i]);
				if (summand >= 0)
				{
					memmove(buffers[i], shaderStart, summand + 1);
				}
			}
			// sum += summand;
			sum += COM_Compress(buffers[i]);
		}
	}

	return sum;
}

/*
====================
ScanAndLoadShaderFiles

Finds and loads all .shader files, combining them into
a single large text block that can be scanned for shader names
=====================
*/
static void ScanAndLoadShaderFiles(void)
{
	char **shaderFiles, **shaderxFiles;
	char *buffers[MAX_SHADER_FILES];
	char *xbuffers[MAX_SHADER_FILES];
	int numShaderFiles, numShaderxFiles;
	int i;
	const char *hashMem;
	std::string_view token;
	char *textEnd;
	const char *p, *oldp;
	int hash, size;
	std::array<int, MAX_SHADERTEXT_HASH> shaderTextHashTableSizes;

	long sum = 0;

	// scan for legacy shader files
	shaderFiles = ri.FS_ListFiles("scripts", ".shader", &numShaderFiles);

	if (1)
	{
		// if ARB shaders available - scan for extended shader files
		shaderxFiles = ri.FS_ListFiles("scripts", ".shaderx", &numShaderxFiles);
	}
	else
	{
		shaderxFiles = NULL;
		numShaderxFiles = 0;
	}

	if ((!shaderFiles || !numShaderFiles) && (!shaderxFiles || !numShaderxFiles))
	{
		ri.Printf(PRINT_WARNING, "WARNING: no shader files found\n");
		return;
	}

	if (numShaderFiles > MAX_SHADER_FILES)
	{
		numShaderFiles = MAX_SHADER_FILES;
	}
	if (numShaderxFiles > MAX_SHADER_FILES)
	{
		numShaderxFiles = MAX_SHADER_FILES;
	}

	sum = 0;
	sum += loadShaderBuffers(shaderxFiles, numShaderxFiles, xbuffers);
	sum += loadShaderBuffers(shaderFiles, numShaderFiles, buffers);

	// build single large buffer
	s_shaderText = reinterpret_cast<char *>(ri.Hunk_Alloc(sum + numShaderxFiles * 2 + numShaderFiles * 2 + 1, h_low));
	s_shaderText[0] = s_shaderText[sum + numShaderxFiles * 2 + numShaderFiles * 2] = '\0';

	textEnd = s_shaderText;

	// free in reverse order, so the temp files are all dumped
	// legacy shaders
	for (i = numShaderFiles - 1; i >= 0; i--)
	{
		if (buffers[i])
		{
			textEnd = Q_stradd_large_cpp(textEnd, buffers[i]);
			textEnd = Q_stradd_small(textEnd, "\n");
			ri.FS_FreeFile(buffers[i]);
		}
	}

	// if shader text >= s_extensionOffset then it is an extended shader
	// normal shaders will never encounter that
	s_extensionOffset = textEnd;

	// extended shaders
	for (i = numShaderxFiles - 1; i >= 0; i--)
	{
		if (xbuffers[i])
		{
			textEnd = Q_stradd_large_cpp(textEnd, xbuffers[i]);
			textEnd = Q_stradd_small(textEnd, "\n");
			ri.FS_FreeFile(xbuffers[i]);
		}
	}

	// free up memory
	if (shaderxFiles)
		ri.FS_FreeFileList(shaderxFiles);
	if (shaderFiles)
		ri.FS_FreeFileList(shaderFiles);

	// COM_Compress( s_shaderText );
	shaderTextHashTableSizes.fill(0);
	size = 0;

	p = s_shaderText;
	// look for shader names
	while (1)
	{
		token = COM_ParseExt_cpp(&p, true);
		if (token.empty())
		{
			break;
		}
		hash = generateHashValue(token, MAX_SHADERTEXT_HASH);
		shaderTextHashTableSizes[hash]++;
		size++;
		SkipBracedSection_cpp(&p, 0);
	}

	size += MAX_SHADERTEXT_HASH;

	hashMem = reinterpret_cast<char *>(ri.Hunk_Alloc(size * sizeof(char *), h_low));

	for (i = 0; i < MAX_SHADERTEXT_HASH; i++)
	{
		shaderTextHashTable[i] = (const char **)hashMem;
		hashMem = ((char *)hashMem) + ((shaderTextHashTableSizes[i] + 1) * sizeof(char *));
	}

	p = s_shaderText;
	// look for shader names
	while (1)
	{
		oldp = p;
		token = COM_ParseExt_cpp(&p, true);
		if (token.empty())
		{
			break;
		}

		hash = generateHashValue(token, MAX_SHADERTEXT_HASH);
		shaderTextHashTable[hash][--shaderTextHashTableSizes[hash]] = (char *)oldp;

		SkipBracedSection_cpp(&p, 0);
	}
}

/*
====================
CreateInternalShaders
====================
*/
static void CreateInternalShaders(void)
{
	tr.numShaders = 0;

	// init the default shader
	InitShader("<default>", LIGHTMAP_NONE);
	stages[0].bundle[0].image[0] = tr.defaultImage;
	stages[0].bundle[0].tcGen = texCoordGen_t::TCGEN_TEXTURE;
	stages[0].active = true;
	stages[0].stateBits = GLS_DEFAULT;
	tr.defaultShader = FinishShader();

	InitShader("<white>", LIGHTMAP_NONE);
	stages[0].bundle[0].image[0] = tr.whiteImage;
	stages[0].bundle[0].tcGen = texCoordGen_t::TCGEN_TEXTURE;
	stages[0].active = true;
	stages[0].bundle[0].rgbGen = colorGen_t::CGEN_EXACT_VERTEX;
	stages[0].stateBits = GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
	tr.whiteShader = FinishShader();

	// shadow shader is just a marker
	InitShader("<stencil shadow>", LIGHTMAP_NONE);
	stages[0].bundle[0].image[0] = tr.defaultImage;
	stages[0].bundle[0].tcGen = texCoordGen_t::TCGEN_TEXTURE;
	stages[0].active = true;
	stages[0].stateBits = GLS_DEFAULT;
	shader.sort = static_cast<float>(shaderSort_t::SS_STENCIL_SHADOW);
	tr.shadowShader = FinishShader();

	InitShader("<cinematic>", LIGHTMAP_NONE);
	stages[0].bundle[0].image[0] = tr.defaultImage; // will be updated by specific cinematic images
	stages[0].bundle[0].tcGen = texCoordGen_t::TCGEN_TEXTURE;
	stages[0].active = true;
	stages[0].bundle[0].rgbGen = colorGen_t::CGEN_IDENTITY_LIGHTING;
	stages[0].stateBits = GLS_DEPTHTEST_DISABLE;
	tr.cinematicShader = FinishShader();
}

/*
====================
CreateExternalShaders
====================
*/
static void CreateExternalShaders(void)
{
	tr.projectionShadowShader = R_FindShader("projectionShadow", LIGHTMAP_NONE, true);
	tr.flareShader = R_FindShader("flareShader", LIGHTMAP_NONE, true);

	// Hack to make fogging work correctly on flares. Fog colors are calculated
	// in tr_flare.c already.
	if (!tr.flareShader->defaultShader)
	{
		int index;

		for (index = 0; index < tr.flareShader->numUnfoggedPasses; index++)
		{
			tr.flareShader->stages[index]->bundle[0].adjustColorsForFog = acff_t::ACFF_NONE;
			tr.flareShader->stages[index]->stateBits |= GLS_DEPTHTEST_DISABLE;
		}
	}

	tr.sunShader = R_FindShader("sun", LIGHTMAP_NONE, true);
}

/*
==================
R_InitShaders
==================
*/
void R_InitShaders(void)
{
	ri.Printf(PRINT_ALL, "Initializing Shaders\n");

	shaderHashTable.fill(nullptr);

	CreateInternalShaders();

	ScanAndLoadShaderFiles();

	CreateExternalShaders();
}
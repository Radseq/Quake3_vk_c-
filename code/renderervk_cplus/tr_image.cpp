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
// tr_image.c
#include "tr_image.hpp"
#include <algorithm> // for std::clamp
#include <cstdint>	 // for std::uint32_t

constexpr int FOG_TABLE_SIZE_PLUS = 256;

skin_t *R_GetSkinByHandle_plus(qhandle_t hSkin)
{
	if (hSkin < 1 || hSkin >= tr.numSkins)
	{
		return tr.skins[0];
	}
	return tr.skins[hSkin];
}

int R_SumOfUsedImages_plus()
{
	const image_t *img;
	int i, total = 0;

	for (i = 0; i < tr.numImages; i++)
	{
		img = tr.images[i];
		if (img->frameUsed == tr.frameCount)
		{
			total += img->uploadWidth * img->uploadHeight;
		}
	}

	return total;
}

void R_InitFogTable_plus()
{
	constexpr float Exponent = 0.5f;

	for (int i = 0; i < FOG_TABLE_SIZE_PLUS; ++i)
	{
		float i_normalized = static_cast<float>(i) / (FOG_TABLE_SIZE_PLUS - 1);
		tr.fogTable[i] = std::pow(i_normalized, Exponent);
	}
}

/*
================
R_FogFactor

Returns a 0.0 to 1.0 fog density value
This is called for each texel of the fog texture on startup
and for each vertex of transparent shaders in fog dynamically
================
*/
constexpr float S_Adjustment = 1.0f / 512;
constexpr float T_Min = 1.0f / 32;
constexpr float T_Max = 31.0f / 32;
constexpr float Interpolation_Factor = 30.0f / 32;
constexpr float Interpolation_Scale = 8.0f;

float R_FogFactor_plus(float s, float t)
{
	// Adjust s and t
	s -= S_Adjustment;
	if (s < 0 || t < T_Min)
		return 0;

	if (t < T_Max)
		s *= (t - T_Min) / Interpolation_Factor;

	// Adjust s for interpolation range and clamp
	s = std::clamp(s * Interpolation_Scale, 0.0f, 1.0f);

	// Lookup fog value
	std::uint32_t index = static_cast<std::uint32_t>(s * (FOG_TABLE_SIZE_PLUS - 1));
	return tr.fogTable[index];
}

static byte s_intensitytable[256];
static unsigned char s_gammatable[256];
static unsigned char s_gammatable_linear[256];

void R_SetColorMappings_plus()
{
	int i, j;
	float g;
	int inf;
	int shift;
	bool applyGamma;

	if (!tr.inited)
	{
		// it may be called from window handling functions where gamma flags is now yet known/set
		return;
	}

	// setup the overbright lighting
	// negative value will force gamma in windowed mode
	tr.overbrightBits = abs(r_overBrightBits->integer);

	// never overbright in windowed mode
	if (!glConfig.isFullscreen && r_overBrightBits->integer >= 0 && !vk.fboActive)
	{
		tr.overbrightBits = 0;
		applyGamma = false;
	}
	else
	{
		if (!glConfig.deviceSupportsGamma && !vk.fboActive)
		{
			tr.overbrightBits = 0; // need hardware gamma for overbright
			applyGamma = false;
		}
		else
		{
			applyGamma = true;
		}
	}

	// allow 2 overbright bits in 24 bit, but only 1 in 16 bit
	if (glConfig.colorBits > 16)
	{
		if (tr.overbrightBits > 2)
		{
			tr.overbrightBits = 2;
		}
	}
	else
	{
		if (tr.overbrightBits > 1)
		{
			tr.overbrightBits = 1;
		}
	}
	if (tr.overbrightBits < 0)
	{
		tr.overbrightBits = 0;
	}

	tr.identityLight = 1.0f / (1 << tr.overbrightBits);
	tr.identityLightByte = 255 * tr.identityLight;

	g = r_gamma->value;

	shift = tr.overbrightBits;

	for (i = 0; i < ARRAY_LEN(s_gammatable); i++)
	{
		if (g == 1.0f)
		{
			inf = i;
		}
		else
		{
			inf = 255 * powf(i / 255.0f, 1.0f / g) + 0.5f;
		}
		inf <<= shift;
		if (inf < 0)
		{
			inf = 0;
		}
		if (inf > 255)
		{
			inf = 255;
		}
		s_gammatable[i] = inf;
	}

	for (i = 0; i < ARRAY_LEN(s_intensitytable); i++)
	{
		j = i * r_intensity->value;
		if (j > 255)
		{
			j = 255;
		}
		s_intensitytable[i] = j;
	}

	if (gls.deviceSupportsGamma)
	{
		if (vk.fboActive)
			ri.GLimp_SetGamma(s_gammatable_linear, s_gammatable_linear, s_gammatable_linear);
		else
		{
			if (applyGamma)
			{
				ri.GLimp_SetGamma(s_gammatable, s_gammatable, s_gammatable);
			}
		}
	}
}

void R_SkinList_f_plus()
{
    ri.Printf(PRINT_ALL, "------------------\n");

    for (int i = 0; i < tr.numSkins; ++i)
    {
        const skin_t& skin = *tr.skins[i]; // Use reference instead of pointer


        ri.Printf(PRINT_ALL, "%3i:%s (%d surfaces)\n", i, skin.name, skin.numSurfaces);
        for (int j = 0; j < skin.numSurfaces; ++j)
        {
            const skinSurface_t& surface = skin.surfaces[j]; // Use reference instead of pointer

            // Check if shader is valid before printing
            const char* shaderName = surface.shader ? surface.shader->name : "Unknown";

            ri.Printf(PRINT_ALL, "       %s = %s\n", surface.name, shaderName);
        }
    }

    ri.Printf(PRINT_ALL, "------------------\n");
}
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
#include "tr_bsp.hpp"
#include "tr_local.hpp"
#include "vk.hpp"
#include "tr_shader.hpp"
#include "utils.hpp"
#include "vk_descriptors.hpp"

#define generateHashValue(fname) Com_GenerateHashValue_cpp((fname), FILE_HASH_SIZE)

#include <algorithm> // for std::clamp
#include <cstdint>	 // for std::uint32_t
#include <algorithm>
#include "string_operations.hpp"
#include <string>
#include <span>
#include "vk_pipeline.hpp"

// Note that the ordering indicates the order of preference used
// when there are multiple images of different formats available
static const imageExtToLoaderMap_t imageLoaders[] =
	{
		{"png", R_LoadPNG},
		{"tga", R_LoadTGA},
		{"jpg", R_LoadJPG},
		{"jpeg", R_LoadJPG},
		{"pcx", R_LoadPCX},
		{"bmp", R_LoadBMP}};

static byte s_intensitytable[256];
static unsigned char s_gammatable[256];
static unsigned char s_gammatable_linear[256];

constexpr int FILE_HASH_SIZE = 1024;
static image_t *hashTable[FILE_HASH_SIZE];

static const int numImageLoaders = arrayLen(imageLoaders);

GLint gl_filter_min = GL_LINEAR_MIPMAP_NEAREST;
GLint gl_filter_max = GL_LINEAR;

constexpr textureMode_t modes[6] = { // Texture modes
	{"GL_NEAREST", GL_NEAREST, GL_NEAREST},
	{"GL_LINEAR", GL_LINEAR, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR}};

skin_t *R_GetSkinByHandle(qhandle_t hSkin)
{
	if (hSkin < 1 || hSkin >= tr.numSkins)
	{
		return tr.skins[0];
	}
	return tr.skins[hSkin];
}

int R_SumOfUsedImages()
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

void R_InitFogTable()
{
	constexpr float Exponent = 0.5f;

	for (int i = 0; i < FOG_TABLE_SIZE; ++i)
	{
		float i_normalized = static_cast<float>(i) / (FOG_TABLE_SIZE - 1);
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

float R_FogFactor(float s, float t)
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
	std::uint32_t index = static_cast<std::uint32_t>(s * (FOG_TABLE_SIZE - 1));
	return tr.fogTable[index];
}

void R_SkinList_f()
{
	ri.Printf(PRINT_ALL, "------------------\n");

	for (int i = 0; i < tr.numSkins; ++i)
	{
		const skin_t &skin = *tr.skins[i]; // Use reference instead of pointer

		ri.Printf(PRINT_ALL, "%3i:%s (%d surfaces)\n", i, skin.name, skin.numSurfaces);
		for (int j = 0; j < skin.numSurfaces; ++j)
		{
			const skinSurface_t &surface = skin.surfaces[j]; // Use reference instead of pointer

			// Check if shader is valid before printing
			const char *shaderName = surface.shader ? surface.shader->name : "Unknown";

			ri.Printf(PRINT_ALL, "       %s = %s\n", surface.name, shaderName);
		}
	}

	ri.Printf(PRINT_ALL, "------------------\n");
}

void R_GammaCorrect(byte *buffer, const int bufSize)
{
	if (vk_inst.capture.image)
		return;
	if (!gls.deviceSupportsGamma)
		return;

	for (int i = 0; i < bufSize; i++)
	{
		buffer[i] = s_gammatable[buffer[i]];
	}
}

void R_SetColorMappings()
{
	if (!tr.inited)
	{
		// it may be called from window handling functions where gamma flags is now yet known/set
		return;
	}

	int i, j;
	float g;
	int inf;
	int shift;
	bool applyGamma;

	// setup the overbright lighting
	// negative value will force gamma in windowed mode
	tr.overbrightBits = abs(r_overBrightBits->integer);

	// never overbright in windowed mode
	if (!glConfig.isFullscreen && r_overBrightBits->integer >= 0 && !vk_inst.fboActive)
	{
		tr.overbrightBits = 0;
		applyGamma = false;
	}
	else
	{
		if (!glConfig.deviceSupportsGamma && !vk_inst.fboActive)
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

	for (i = 0; i < static_cast<int>(arrayLen(s_gammatable)); i++)
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

	for (i = 0; i < static_cast<int>(arrayLen(s_intensitytable)); i++)
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
		if (vk_inst.fboActive)
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

/*
================
R_LightScaleTexture

Scale up the pixel values in a texture to increase the
lighting range
================
*/
static void R_LightScaleTexture(byte *in, int inwidth, int inheight, bool only_gamma)
{
	if (in == NULL)
		return;

	if (only_gamma)
	{
		if (!glConfig.deviceSupportsGamma && !vk_inst.fboActive)
		{
			int i, c;
			byte *p;

			p = (byte *)in;

			c = inwidth * inheight;
			for (i = 0; i < c; i++, p += 4)
			{
				p[0] = s_gammatable[p[0]];
				p[1] = s_gammatable[p[1]];
				p[2] = s_gammatable[p[2]];
			}
		}
	}
	else
	{
		int i, c;
		byte *p;

		p = (byte *)in;

		c = inwidth * inheight;

		if (glConfig.deviceSupportsGamma || vk_inst.fboActive)
		{
			for (i = 0; i < c; i++, p += 4)
			{
				p[0] = s_intensitytable[p[0]];
				p[1] = s_intensitytable[p[1]];
				p[2] = s_intensitytable[p[2]];
			}
		}
		else
		{
			for (i = 0; i < c; i++, p += 4)
			{
				p[0] = s_gammatable[s_intensitytable[p[0]]];
				p[1] = s_gammatable[s_intensitytable[p[1]]];
				p[2] = s_gammatable[s_intensitytable[p[2]]];
			}
		}
	}
}

void TextureMode(std::string_view sv_mode)
{
	const textureMode_t *mode{};
	image_t *img;
	int i;

	for (i = 0; i < static_cast<int>(arrayLen(modes)); i++)
	{
		if (!Q_stricmp_cpp(modes[i].name, sv_mode))
		{
			mode = &modes[i];
			break;
		}
	}

	if (mode == nullptr)
	{
		ri.Printf(PRINT_ALL, "bad texture filter name '%s'\n", sv_mode.data());
		return;
	}

	gl_filter_min = mode->minimize;
	gl_filter_max = mode->maximize;

	if ( gl_filter_min == vk_inst.samplers.filter_min && gl_filter_max == vk_inst.samplers.filter_max ) {
		return;
	}

	vk_wait_idle();
	vk_destroy_samplers();

	vk_inst.samplers.filter_min = gl_filter_min;
	vk_inst.samplers.filter_max = gl_filter_max;
	vk_update_attachment_descriptors();
	for ( i = 0; i < tr.numImages; i++ ) 
	{
		img = tr.images[i];
		if (HasFlag(img->flags, imgFlags_t::IMGFLAG_MIPMAP))
		{
			vk_update_descriptor_set(*img, true);
		}
	}
}

void R_ImageList_f(void)
{
	int i, estTotalSize = 0;
	char *name, buf[MAX_QPATH * 2 + 5];

	ri.Printf(PRINT_ALL, "\n -n- --w-- --h-- type  -size- --name-------\n");

	for (i = 0; i < tr.numImages; i++)
	{
		const char *format = "???? ";
		const char *sizeSuffix;
		int estSize;
		int displaySize;

		const image_t *image = tr.images[i];
		estSize = image->uploadHeight * image->uploadWidth;

		switch (image->internalFormat)
		{
		case vk::Format::eB8G8R8A8Unorm:
			format = "BGRA ";
			estSize *= 4;
			break;
		case vk::Format::eR8G8B8A8Unorm:
			format = "RGBA ";
			estSize *= 4;
			break;
		case vk::Format::eR8G8B8Unorm:
			format = "RGB  ";
			estSize *= 3;
			break;
		case vk::Format::eB4G4R4A4UnormPack16:
			format = "RGBA ";
			estSize *= 2;
			break;
		case vk::Format::eA1R5G5B5UnormPack16:
			format = "RGB  ";
			estSize *= 2;
			break;
		default:
			ri.Printf(PRINT_ALL, "Unsupported vk::format of image->internalFormat \n");
			break;
		}

		// mipmap adds about 50%
		if (HasFlag(image->flags, imgFlags_t::IMGFLAG_MIPMAP))
			estSize += estSize / 2;

		sizeSuffix = "b ";
		displaySize = estSize;

		if (displaySize >= 2048)
		{
			displaySize = (displaySize + 1023) / 1024;
			sizeSuffix = "kb";
		}

		if (displaySize >= 2048)
		{
			displaySize = (displaySize + 1023) / 1024;
			sizeSuffix = "Mb";
		}

		if (displaySize >= 2048)
		{
			displaySize = (displaySize + 1023) / 1024;
			sizeSuffix = "Gb";
		}

		if (Q_stricmp_cpp(image->imgName, image->imgName2) == 0)
		{
			name = image->imgName;
		}
		else
		{
			Com_sprintf(buf, sizeof(buf), "%s => " S_COLOR_YELLOW "%s",
						image->imgName, image->imgName2);
			name = buf;
		}

		ri.Printf(PRINT_ALL, " %3i %5i %5i %s %4i%s %s\n", i, image->uploadWidth, image->uploadHeight, format, displaySize, sizeSuffix, name);
		estTotalSize += estSize;
	}

	ri.Printf(PRINT_ALL, " -----------------------\n");
	ri.Printf(PRINT_ALL, " approx %i kbytes\n", (estTotalSize + 1023) / 1024);
	ri.Printf(PRINT_ALL, " %i total images\n\n", tr.numImages);
}

static bool RawImage_HasAlpha(const byte *scan, const int numPixels)
{
	if (!scan)
		return true;

	for (int i = 0; i < numPixels; i++)
	{
		if (scan[i * 4 + 3] != 255)
		{
			return true;
		}
	}

	return false;
}

/*
==================
R_BlendOverTexture

Apply a color blend over a set of pixels
==================
*/
static void R_BlendOverTexture(byte *data, int pixelCount, int mipLevel)
{
	if (data == NULL)
		return;

	if (mipLevel <= 0)
		return;

	static constexpr byte blendColors[][4] = {
		{255, 0, 0, 128},
		{255, 255, 0, 128},
		{0, 255, 0, 128},
		{0, 255, 255, 128},
		{0, 0, 255, 128},
		{255, 0, 255, 128}};

	const byte *blend = blendColors[(mipLevel - 1) % arrayLen(blendColors)];

	int i;
	int inverseAlpha = 255 - blend[3];
	int premult[3]{
		blend[0] * blend[3],
		blend[1] * blend[3],
		blend[2] * blend[3]};

	for (i = 0; i < pixelCount; i++, data += 4)
	{
		data[0] = (data[0] * inverseAlpha + premult[0]) >> 9;
		data[1] = (data[1] * inverseAlpha + premult[1]) >> 9;
		data[2] = (data[2] * inverseAlpha + premult[2]) >> 9;
	}
}

/*
================
ResampleTexture

Used to resample images in a more general than quartering fashion.

This will only be filtered properly if the resampled size
is greater than half the original size.

If a larger shrinking is needed, use the mipmap function
before or after.
================
*/
static void ResampleTexture(unsigned *in, int inwidth, int inheight, unsigned *out,
							int outwidth, int outheight)
{
	int i, j;
	unsigned *inrow, *inrow2;
	unsigned frac, fracstep;
	std::array<unsigned, MAX_TEXTURE_SIZE> p1{};
	std::array<unsigned, MAX_TEXTURE_SIZE> p2{};
	byte *pix1, *pix2, *pix3, *pix4;

	if (outwidth > static_cast<int>(p1.size()))
		ri.Error(ERR_DROP, "ResampleTexture: max width");

	fracstep = inwidth * 0x10000 / outwidth;

	frac = fracstep >> 2;
	for (i = 0; i < outwidth; i++)
	{
		p1[i] = 4 * (frac >> 16);
		frac += fracstep;
	}
	frac = 3 * (fracstep >> 2);
	for (i = 0; i < outwidth; i++)
	{
		p2[i] = 4 * (frac >> 16);
		frac += fracstep;
	}

	for (i = 0; i < outheight; i++, out += outwidth)
	{
		inrow = in + inwidth * (int)((i + 0.25) * inheight / outheight);
		inrow2 = in + inwidth * (int)((i + 0.75) * inheight / outheight);
		for (j = 0; j < outwidth; j++)
		{
			pix1 = (byte *)inrow + p1[j];
			pix2 = (byte *)inrow + p2[j];
			pix3 = (byte *)inrow2 + p1[j];
			pix4 = (byte *)inrow2 + p2[j];
			((byte *)(out + j))[0] = (pix1[0] + pix2[0] + pix3[0] + pix4[0]) >> 2;
			((byte *)(out + j))[1] = (pix1[1] + pix2[1] + pix3[1] + pix4[1]) >> 2;
			((byte *)(out + j))[2] = (pix1[2] + pix2[2] + pix3[2] + pix4[2]) >> 2;
			((byte *)(out + j))[3] = (pix1[3] + pix2[3] + pix3[3] + pix4[3]) >> 2;
		}
	}
}

/*
================
R_MipMap2

Operates in place, quartering the size of the texture
Proper linear filter
================
*/
static void R_MipMap2(unsigned *const out, unsigned *const in, int inWidth, int inHeight)
{
	int i, j, k;
	byte *outpix;
	int inWidthMask, inHeightMask;
	int total;
	int outWidth, outHeight;
	unsigned *temp;

	outWidth = inWidth >> 1;
	outHeight = inHeight >> 1;

	if (out == in)
		temp = static_cast<unsigned int *>(ri.Hunk_AllocateTempMemory(outWidth * outHeight * 4));
	else
		temp = out;

	inWidthMask = inWidth - 1;
	inHeightMask = inHeight - 1;

	for (i = 0; i < outHeight; i++)
	{
		for (j = 0; j < outWidth; j++)
		{
			outpix = (byte *)(temp + i * outWidth + j);
			for (k = 0; k < 4; k++)
			{
				total =
					1 * ((byte *)&in[((i * 2 - 1) & inHeightMask) * inWidth + ((j * 2 - 1) & inWidthMask)])[k] +
					2 * ((byte *)&in[((i * 2 - 1) & inHeightMask) * inWidth + ((j * 2) & inWidthMask)])[k] +
					2 * ((byte *)&in[((i * 2 - 1) & inHeightMask) * inWidth + ((j * 2 + 1) & inWidthMask)])[k] +
					1 * ((byte *)&in[((i * 2 - 1) & inHeightMask) * inWidth + ((j * 2 + 2) & inWidthMask)])[k] +

					2 * ((byte *)&in[((i * 2) & inHeightMask) * inWidth + ((j * 2 - 1) & inWidthMask)])[k] +
					4 * ((byte *)&in[((i * 2) & inHeightMask) * inWidth + ((j * 2) & inWidthMask)])[k] +
					4 * ((byte *)&in[((i * 2) & inHeightMask) * inWidth + ((j * 2 + 1) & inWidthMask)])[k] +
					2 * ((byte *)&in[((i * 2) & inHeightMask) * inWidth + ((j * 2 + 2) & inWidthMask)])[k] +

					2 * ((byte *)&in[((i * 2 + 1) & inHeightMask) * inWidth + ((j * 2 - 1) & inWidthMask)])[k] +
					4 * ((byte *)&in[((i * 2 + 1) & inHeightMask) * inWidth + ((j * 2) & inWidthMask)])[k] +
					4 * ((byte *)&in[((i * 2 + 1) & inHeightMask) * inWidth + ((j * 2 + 1) & inWidthMask)])[k] +
					2 * ((byte *)&in[((i * 2 + 1) & inHeightMask) * inWidth + ((j * 2 + 2) & inWidthMask)])[k] +

					1 * ((byte *)&in[((i * 2 + 2) & inHeightMask) * inWidth + ((j * 2 - 1) & inWidthMask)])[k] +
					2 * ((byte *)&in[((i * 2 + 2) & inHeightMask) * inWidth + ((j * 2) & inWidthMask)])[k] +
					2 * ((byte *)&in[((i * 2 + 2) & inHeightMask) * inWidth + ((j * 2 + 1) & inWidthMask)])[k] +
					1 * ((byte *)&in[((i * 2 + 2) & inHeightMask) * inWidth + ((j * 2 + 2) & inWidthMask)])[k];
				outpix[k] = total / 36;
			}
		}
	}

	if (out == in)
	{
		Com_Memcpy(out, temp, outWidth * outHeight * 4);
		ri.Hunk_FreeTempMemory(temp);
	}
}

/*
================
R_MipMap

Operates in place, quartering the size of the texture
================
*/
static void R_MipMap(byte *out, byte *in, int width, int height)
{
	int i, j;
	int row;

	if (in == NULL)
		return;

	if (!r_simpleMipMaps->integer)
	{
		R_MipMap2((unsigned *)out, (unsigned *)in, width, height);
		return;
	}

	if (width == 1 && height == 1)
	{
		return;
	}

	row = width * 4;
	width >>= 1;
	height >>= 1;

	if (width == 0 || height == 0)
	{
		width += height; // get largest
		for (i = 0; i < width; i++, out += 4, in += 8)
		{
			out[0] = (in[0] + in[4]) >> 1;
			out[1] = (in[1] + in[5]) >> 1;
			out[2] = (in[2] + in[6]) >> 1;
			out[3] = (in[3] + in[7]) >> 1;
		}
		return;
	}

	for (i = 0; i < height; i++, in += row)
	{
		for (j = 0; j < width; j++, out += 4, in += 8)
		{
			out[0] = (in[0] + in[4] + in[row + 0] + in[row + 4]) >> 2;
			out[1] = (in[1] + in[5] + in[row + 1] + in[row + 5]) >> 2;
			out[2] = (in[2] + in[6] + in[row + 2] + in[row + 6]) >> 2;
			out[3] = (in[3] + in[7] + in[row + 3] + in[row + 7]) >> 2;
		}
	}
}

static void generate_image_upload_data(image_t *image, byte *data, Image_Upload_Data *upload_data)
{

	bool mipmap = HasFlag(image->flags, imgFlags_t::IMGFLAG_MIPMAP);
	bool picmip = HasFlag(image->flags, imgFlags_t::IMGFLAG_PICMIP);
	byte *resampled_buffer = NULL;
	int scaled_width, scaled_height;
	int width = image->width;
	int height = image->height;
	unsigned *scaled_buffer;
	int mip_level_size;
	int miplevel = 0;

	Com_Memset(upload_data, 0, sizeof(*upload_data));

	if (HasFlag(image->flags, imgFlags_t::IMGFLAG_NOSCALE))
	{
		//
		// keep original dimensions
		//
		scaled_width = width;
		scaled_height = height;
	}
	else
	{
		//
		// convert to exact power of 2 sizes
		//
		for (scaled_width = 1; scaled_width < width; scaled_width <<= 1)
			;
		for (scaled_height = 1; scaled_height < height; scaled_height <<= 1)
			;

		if (r_roundImagesDown->integer && scaled_width > width)
			scaled_width >>= 1;
		if (r_roundImagesDown->integer && scaled_height > height)
			scaled_height >>= 1;
	}

	//
	// clamp to the current upper OpenGL limit
	// scale both axis down equally so we don't have to
	// deal with a half mip resampling
	//
	while (scaled_width > glConfig.maxTextureSize || scaled_height > glConfig.maxTextureSize)
	{
		scaled_width >>= 1;
		scaled_height >>= 1;
	}

	upload_data->buffer = (byte *)ri.Hunk_AllocateTempMemory(2 * 4 * scaled_width * scaled_height);
	if (data == NULL)
	{
		Com_Memset(upload_data->buffer, 0, 2 * 4 * scaled_width * scaled_height);
	}

	if ((scaled_width != width || scaled_height != height) && data)
	{
		resampled_buffer = (byte *)ri.Hunk_AllocateTempMemory(scaled_width * scaled_height * 4);
		ResampleTexture((unsigned *)data, width, height, (unsigned *)resampled_buffer, scaled_width, scaled_height);
		data = resampled_buffer;
	}

	width = scaled_width;
	height = scaled_height;

	if (data == NULL)
	{
		data = upload_data->buffer;
	}
	else
	{
		if (HasFlag(image->flags, imgFlags_t::IMGFLAG_COLORSHIFT))
		{
			byte *p = data;
			int i, n = width * height;
			for (i = 0; i < n; i++, p += 4)
			{
				R_ColorShiftLightingBytes(p, p, false);
			}
		}
	}

	//
	// perform optional picmip operation
	//
	if (picmip && (tr.mapLoading || r_nomip->integer == 0))
	{
		scaled_width >>= r_picmip->integer;
		scaled_height >>= r_picmip->integer;
		// x >>= r_picmip->integer;
		// y >>= r_picmip->integer;
	}

	//
	// clamp to minimum size
	//
	if (scaled_width < 1)
	{
		scaled_width = 1;
	}
	if (scaled_height < 1)
	{
		scaled_height = 1;
	}

	upload_data->base_level_width = scaled_width;
	upload_data->base_level_height = scaled_height;

	if (scaled_width == width && scaled_height == height && !mipmap)
	{
		upload_data->mip_levels = 1;
		upload_data->buffer_size = scaled_width * scaled_height * 4;

		if (data != NULL)
		{
			Com_Memcpy(upload_data->buffer, data, upload_data->buffer_size);
		}

		if (resampled_buffer != NULL)
		{
			ri.Hunk_FreeTempMemory(resampled_buffer);
		}

		return; // return upload_data;
	}

	// Use the normal mip-mapping to go down from [width, height] to [scaled_width, scaled_height] dimensions.
	while (width > scaled_width || height > scaled_height)
	{
		R_MipMap(data, data, width, height);

		width >>= 1;
		if (width < 1)
			width = 1;

		height >>= 1;
		if (height < 1)
			height = 1;
	}

	// At this point width == scaled_width and height == scaled_height.

	scaled_buffer = (unsigned int *)ri.Hunk_AllocateTempMemory(sizeof(unsigned) * scaled_width * scaled_height);
	Com_Memcpy(scaled_buffer, data, scaled_width * scaled_height * 4);

	if (!HasFlag(image->flags, imgFlags_t::IMGFLAG_NOLIGHTSCALE))
	{
		R_LightScaleTexture((byte *)scaled_buffer, scaled_width, scaled_height, !mipmap);
	}

	mip_level_size = scaled_width * scaled_height * 4;

	Com_Memcpy(upload_data->buffer, scaled_buffer, mip_level_size);
	upload_data->buffer_size = mip_level_size;

	if (mipmap)
	{
		while (scaled_width > 1 && scaled_height > 1)
		{
			R_MipMap((byte *)scaled_buffer, (byte *)scaled_buffer, scaled_width, scaled_height);

			scaled_width >>= 1;
			if (scaled_width < 1)
				scaled_width = 1;

			scaled_height >>= 1;
			if (scaled_height < 1)
				scaled_height = 1;

			miplevel++;
			mip_level_size = scaled_width * scaled_height * 4;

			if (r_colorMipLevels->integer)
			{
				R_BlendOverTexture((byte *)scaled_buffer, scaled_width * scaled_height, miplevel);
			}

			Com_Memcpy(&upload_data->buffer[upload_data->buffer_size], scaled_buffer, mip_level_size);
			upload_data->buffer_size += mip_level_size;
		}
	}

	upload_data->mip_levels = miplevel + 1;

	ri.Hunk_FreeTempMemory(scaled_buffer);

	if (resampled_buffer != NULL)
		ri.Hunk_FreeTempMemory(resampled_buffer);
}

static void upload_vk_image(image_t *image, byte *pic)
{

	Image_Upload_Data upload_data;
	int w, h;

	generate_image_upload_data(image, pic, &upload_data);

	w = upload_data.base_level_width;
	h = upload_data.base_level_height;

	if (r_texturebits->integer > 16 || r_texturebits->integer == 0 || (HasFlag(image->flags, imgFlags_t::IMGFLAG_LIGHTMAP)))
	{
		image->internalFormat = vk::Format::eR8G8B8A8Unorm;
		// image->internalFormat = VK_FORMAT_B8G8R8A8_UNORM;
	}
	else
	{
		bool has_alpha = RawImage_HasAlpha(upload_data.buffer, w * h);
		image->internalFormat = has_alpha ? vk::Format::eB4G4R4A4UnormPack16 : vk::Format::eA1R5G5B5UnormPack16;
	}

	image->uploadWidth = w;
	image->uploadHeight = h;

	vk_create_image(*image, w, h, upload_data.mip_levels);
	vk_upload_image_data(*image, 0, 0, w, h, upload_data.mip_levels, upload_data.buffer, upload_data.buffer_size, false);

	ri.Hunk_FreeTempMemory(upload_data.buffer);
}

/*
================
R_CreateImage

This is the only way any image_t are created
Picture data may be modified in-place during mipmap processing
================
*/
image_t *R_CreateImage(std::string_view name, std::string_view name2, byte *pic, int width, int height, imgFlags_t flags)
{
	image_t *image;
	long hash;
	int namelen, namelen2;

	namelen = name.size() + 1;
	if (namelen > MAX_QPATH)
	{
		ri.Error(ERR_DROP, "R_CreateImage: \"%s\" is too long", name.data());
	}

	if (!name2.empty() && Q_stricmp_cpp(name, name2) != 0)
	{
		// leave only file name
		auto slash_pos = name2.rfind('/'); // Find the last '/'
		if (slash_pos != std::string_view::npos)
		{
			name2 = name2.substr(slash_pos + 1); // Update name2 to the substring after '/'
		}
		namelen2 = name2.size() + 1;
	}
	else
	{
		namelen2 = 0;
	}

	if (tr.numImages == MAX_DRAWIMAGES)
	{
		ri.Error(ERR_DROP, "R_CreateImage: MAX_DRAWIMAGES hit");
	}

	image = static_cast<image_t *>(ri.Hunk_Alloc(sizeof(*image) + namelen + namelen2, h_low));
	image->imgName = (char *)(image + 1);
	strcpy(image->imgName, name.data());
	// std::memcpy(image->imgName, name.data(), name.size());
	if (namelen2)
	{
		image->imgName2 = image->imgName + namelen;
		strcpy(image->imgName2, name2.data());
		// std::memcpy(image->imgName2, name2.data(), name2.size());
	}
	else
	{
		image->imgName2 = image->imgName;
	}

	hash = generateHashValue(name);
	image->next = hashTable[hash];
	hashTable[hash] = image;

	tr.images[tr.numImages++] = image;

	image->flags = flags;
	image->width = width;
	image->height = height;

	if (namelen > 6 && Q_stristr_cpp(image->imgName, "maps/") == image->imgName && Q_stristr_cpp(image->imgName + 6, "/lm_") != NULL)
	{
		// external lightmap atlases stored in maps/<mapname>/lm_XXXX textures
		// image->flags = imgFlags_t::IMGFLAG_NOLIGHTSCALE | imgFlags_t::IMGFLAG_NO_COMPRESSION | imgFlags_t::IMGFLAG_NOSCALE | imgFlags_t::IMGFLAG_COLORSHIFT;
		image->flags = static_cast<imgFlags_t>(image->flags | imgFlags_t::IMGFLAG_NO_COMPRESSION | imgFlags_t::IMGFLAG_NOSCALE);
	}

	if (HasFlag(flags, imgFlags_t::IMGFLAG_CLAMPTOBORDER))
		image->wrapClampMode = vk::SamplerAddressMode::eClampToBorder;
	else if (static_cast<int>(flags & imgFlags_t::IMGFLAG_CLAMPTOEDGE))
		image->wrapClampMode = vk::SamplerAddressMode::eClampToEdge;
	else
		image->wrapClampMode = vk::SamplerAddressMode::eRepeat;

	image->handle = VK_NULL_HANDLE;
	image->view = VK_NULL_HANDLE;
	image->descriptor = VK_NULL_HANDLE;

	upload_vk_image(image, pic);
	return image;
}

/*
=================
R_LoadImage

Loads any of the supported image types into a canonical
32 bit format.
=================
*/
static std::array<char, MAX_QPATH> R_LoadImage(std::string_view name, byte **pic, int *width, int *height)
{
	static std::array<char, MAX_QPATH> localName;
	std::string_view altName;
	// bool orgNameFailed = false;
	int orgLoader = -1;
	int i;

	*pic = NULL;
	*width = 0;
	*height = 0;

	Q_strncpyz_cpp(localName, name, localName.size());

	std::string_view ext = COM_GetExtension_cpp(localName);
	if (!ext.empty())
	{
		// Look for the correct loader and use it
		for (i = 0; i < numImageLoaders; i++)
		{
			if (!Q_stricmp_cpp(ext, imageLoaders[i].ext))
			{
				// Load
				imageLoaders[i].ImageLoader(localName.data(), pic, width, height);
				break;
			}
		}

		// A loader was found
		if (i < numImageLoaders)
		{
			if (*pic == NULL)
			{
				// Loader failed, most likely because the file isn't there;
				// try again without the extension
				// orgNameFailed = true;
				orgLoader = i;
				COM_StripExtension_cpp(name, localName);
			}
			else
			{
				// Something loaded
				return localName;
			}
		}
	}

	// Try and find a suitable match using all
	// the image formats supported
	for (i = 0; i < numImageLoaders; i++)
	{
		if (i == orgLoader)
			continue;

		altName = va_cpp("%s.%s", localName.data(), imageLoaders[i].ext);
		// ri.Printf(PRINT_ALL, "name %s \n", altName.data());

		// Load
		imageLoaders[i].ImageLoader(altName.data(), pic, width, height);

		if (*pic)
		{
#if 0
			if (orgNameFailed)
			{
				ri.Printf(PRINT_DEVELOPER, S_COLOR_YELLOW "WARNING: %s not present, using %s instead\n",
					name, altName);
			}
#endif
			Q_strncpyz_cpp(localName, altName, localName.size());
			break;
		}
	}

	return localName;
}

/*
===============
R_FindImageFile

Finds or loads the given image.
Returns NULL if it fails, not a default image.
==============
*/
image_t *R_FindImageFile(std::string_view name, imgFlags_t flags)
{
	image_t *image;
	std::array<char, MAX_QPATH> strippedName;
	int width, height;
	byte *pic;
	int hash;

	// ri.Printf(PRINT_ALL, "name %s \n", name.data());

	if (name.empty())
	{
		return NULL;
	}

	hash = generateHashValue(name);

	//
	// see if the image is already loaded
	//
	for (image = hashTable[hash]; image; image = image->next)
	{
		if (!Q_stricmp_cpp(name, image->imgName))
		{
			// the white image can be used with any set of parms, but other mismatches are errors
			if (name.compare("*white"))
			{
				if (image->flags != flags)
				{
					ri.Printf(PRINT_DEVELOPER, "WARNING: reused image %s with mixed flags (%i vs %i)\n", name.data(), static_cast<int>(image->flags), static_cast<int>(flags));
				}
			}
			return image;
		}
	}

	if (strrchr_sv(name, '.'))
	{
		// try with stripped extension
		COM_StripExtension_cpp(name, strippedName);
		for (image = hashTable[hash]; image; image = image->next)
		{
			if (!Q_stricmp_cpp(std::string_view(strippedName.data(), strippedName.size()), image->imgName))
			{
				// if ( strcmp( strippedName, "*white" ) ) {
				if (image->flags != flags)
				{
					ri.Printf(PRINT_DEVELOPER, "WARNING: reused image %s with mixed flags (%i vs %i)\n", strippedName.data(), static_cast<int>(image->flags), static_cast<int>(flags));
				}
				//}
				return image;
			}
		}
	}

	//
	// load the pic from disk
	//
	auto localName = R_LoadImage(name, &pic, &width, &height);
	if (pic == nullptr)
	{
		return nullptr;
	}

	if (tr.mapLoading && r_mapGreyScale->value > 0)
	{
		byte *img;
		int i;
		for (i = 0, img = pic; i < width * height; i++, img += 4)
		{
			if (r_mapGreyScale->integer)
			{
				byte luma = LUMA(img[0], img[1], img[2]);
				img[0] = luma;
				img[1] = luma;
				img[2] = luma;
			}
			else
			{
				float luma = LUMA(img[0], img[1], img[2]);
				img[0] = LERP(img[0], luma, r_mapGreyScale->value);
				img[1] = LERP(img[1], luma, r_mapGreyScale->value);
				img[2] = LERP(img[2], luma, r_mapGreyScale->value);
			}
		}
	}

	image = R_CreateImage(name.data(), localName.data(), pic, width, height, flags);
	ri.Free(pic);
	return image;
}

static void R_CreateFogImage(void)
{
	int x, y;
	byte *data;
	float d;

	data = static_cast<byte *>(ri.Hunk_AllocateTempMemory(FOG_S * FOG_T * 4));

	// S is distance, T is depth
	for (x = 0; x < FOG_S; x++)
	{
		for (y = 0; y < FOG_T; y++)
		{
			d = R_FogFactor((x + 0.5f) / FOG_S, (y + 0.5f) / FOG_T);

			data[(y * FOG_S + x) * 4 + 0] =
				data[(y * FOG_S + x) * 4 + 1] =
					data[(y * FOG_S + x) * 4 + 2] = 255;
			data[(y * FOG_S + x) * 4 + 3] = 255 * d;
		}
	}
	tr.fogImage = R_CreateImage("*fog", {}, data, FOG_S, FOG_T, imgFlags_t::IMGFLAG_CLAMPTOEDGE);
	ri.Hunk_FreeTempMemory(data);
}

static void R_CreateDlightImage(void)
{
	int x, y;
	byte data[DLIGHT_SIZE][DLIGHT_SIZE][4]{};
	int b;

	// make a centered inverse-square falloff blob for dynamic lighting
	for (x = 0; x < DLIGHT_SIZE; x++)
	{
		for (y = 0; y < DLIGHT_SIZE; y++)
		{
			float d;

			d = (DLIGHT_SIZE / 2 - 0.5f - x) * (DLIGHT_SIZE / 2 - 0.5f - x) +
				(DLIGHT_SIZE / 2 - 0.5f - y) * (DLIGHT_SIZE / 2 - 0.5f - y);
			b = 4000 / d;
			if (b > 255)
			{
				b = 255;
			}
			else if (b < 75)
			{
				b = 0;
			}
			data[y][x][0] =
				data[y][x][1] =
					data[y][x][2] = b;
			data[y][x][3] = 255;
		}
	}
	tr.dlightImage = R_CreateImage("*dlight", {}, (byte *)data, DLIGHT_SIZE, DLIGHT_SIZE, imgFlags_t::IMGFLAG_CLAMPTOEDGE);
}

// Lookup table for hexadecimal characters
static constexpr std::array<int, 256> CreateHexLookupTable()
{
	std::array<int, 256> table{};
	for (int i = 0; i < 256; ++i)
	{
		if (i >= '0' && i <= '9')
		{
			table[i] = i - '0';
		}
		else if (i >= 'A' && i <= 'F')
		{
			table[i] = 10 + i - 'A';
		}
		else if (i >= 'a' && i <= 'f')
		{
			table[i] = 10 + i - 'a';
		}
		else
		{
			table[i] = -1; // Invalid character
		}
	}
	return table;
}

// Static lookup table initialized at compile time
static constexpr auto HexLookupTable = CreateHexLookupTable();

// Hex function using the lookup table
static constexpr int Hex_cpp(char c)
{
	return HexLookupTable[static_cast<unsigned char>(c)];
}

/*
==================
R_BuildDefaultImage

Create solid color texture from following input formats (hex):
#rgb
#rrggbb
==================
*/
static bool R_BuildDefaultImage(const char *format)
{
	byte data[DEFAULT_SIZE][DEFAULT_SIZE][4]{};
	byte color[4]{};
	int i, len, hex[6]{};
	int x, y;

	if (*format++ != '#')
	{
		return false;
	}

	len = (int)strlen(format);
	if (len <= 0 || len > 6)
	{
		return false;
	}

	for (i = 0; i < len; i++)
	{
		hex[i] = Hex_cpp(format[i]);
		if (hex[i] == -1)
		{
			return false;
		}
	}

	switch (len)
	{
	case 3: // #rgb
		color[0] = hex[0] << 4 | hex[0];
		color[1] = hex[1] << 4 | hex[1];
		color[2] = hex[2] << 4 | hex[2];
		color[3] = 255;
		break;
	case 6: // #rrggbb
		color[0] = hex[0] << 4 | hex[1];
		color[1] = hex[2] << 4 | hex[3];
		color[2] = hex[4] << 4 | hex[5];
		color[3] = 255;
		break;
	default: // unsupported format
		return false;
	}

	for (y = 0; y < DEFAULT_SIZE; y++)
	{
		for (x = 0; x < DEFAULT_SIZE; x++)
		{
			data[x][y][0] = color[0];
			data[x][y][1] = color[1];
			data[x][y][2] = color[2];
			data[x][y][3] = color[3];
		}
	}

	tr.defaultImage = R_CreateImage("*default", {}, (byte *)data, DEFAULT_SIZE, DEFAULT_SIZE, imgFlags_t::IMGFLAG_MIPMAP);

	return true;
}

static void R_CreateDefaultImage(void)
{
	int x;
	byte data[DEFAULT_SIZE][DEFAULT_SIZE][4];

	if (r_defaultImage->string[0])
	{
		// build from format
		if (R_BuildDefaultImage(r_defaultImage->string))
			return;
		// load from external file
		tr.defaultImage = R_FindImageFile(r_defaultImage->string, static_cast<imgFlags_t>(imgFlags_t::IMGFLAG_MIPMAP | imgFlags_t::IMGFLAG_PICMIP));
		if (tr.defaultImage)
			return;
	}

	// the default image will be a box, to allow you to see the mapping coordinates
	Com_Memset(data, 32, sizeof(data));
	for (x = 0; x < DEFAULT_SIZE; x++)
	{
		data[0][x][0] =
			data[0][x][1] =
				data[0][x][2] =
					data[0][x][3] = 255;

		data[x][0][0] =
			data[x][0][1] =
				data[x][0][2] =
					data[x][0][3] = 255;

		data[DEFAULT_SIZE - 1][x][0] =
			data[DEFAULT_SIZE - 1][x][1] =
				data[DEFAULT_SIZE - 1][x][2] =
					data[DEFAULT_SIZE - 1][x][3] = 255;

		data[x][DEFAULT_SIZE - 1][0] =
			data[x][DEFAULT_SIZE - 1][1] =
				data[x][DEFAULT_SIZE - 1][2] =
					data[x][DEFAULT_SIZE - 1][3] = 255;
	}

	tr.defaultImage = R_CreateImage("*default", {}, (byte *)data, DEFAULT_SIZE, DEFAULT_SIZE, imgFlags_t::IMGFLAG_MIPMAP);
}

static void R_CreateBuiltinImages(void)
{
	int x, y;
	byte data[DEFAULT_SIZE][DEFAULT_SIZE][4];

	R_CreateDefaultImage();

	Com_Memset(data, 0, sizeof(data));
	tr.blackImage = R_CreateImage("*black", {}, (byte *)data, 8, 8, imgFlags_t::IMGFLAG_NONE);

	// we use a solid white image instead of disabling texturing
	Com_Memset(data, 255, sizeof(data));
	tr.whiteImage = R_CreateImage("*white", {}, (byte *)data, 8, 8, imgFlags_t::IMGFLAG_NONE);

	// with overbright bits active, we need an image which is some fraction of full color,
	// for default lightmaps, etc
	for (x = 0; x < DEFAULT_SIZE; x++)
	{
		for (y = 0; y < DEFAULT_SIZE; y++)
		{
			data[y][x][0] =
				data[y][x][1] =
					data[y][x][2] = tr.identityLightByte;
			data[y][x][3] = 255;
		}
	}

	tr.identityLightImage = R_CreateImage("*identityLight", {}, (byte *)data, 8, 8, imgFlags_t::IMGFLAG_NONE);

	// for ( x = 0; x < arrayLen2( tr.scratchImage ); x++ ) {
	//  scratchimage is usually used for cinematic drawing
	// tr.scratchImage[x] = R_CreateImage( "*scratch", (byte*)data, DEFAULT_SIZE, DEFAULT_SIZE,
	//	imgFlags_t::IMGFLAG_PICMIP | imgFlags_t::IMGFLAG_CLAMPTOEDGE | imgFlags_t::IMGFLAG_RGB );
	//}

	R_CreateDlightImage();
	R_CreateFogImage();
}

void R_InitImages(void)
{
	// initialize linear gamma table before setting color mappings for the first time
	int i;

	for (i = 0; i < 256; i++)
		s_gammatable_linear[i] = (unsigned char)i;

	memset(hashTable, 0, sizeof(hashTable));
	// std::fill(std::begin(hashTable), std::end(hashTable), nullptr);

	// build brightness translation tables
	R_SetColorMappings();

	// create default texture and white texture
	R_CreateBuiltinImages();

	vk_update_post_process_pipelines();
}

void R_DeleteTextures(void)
{
	if ( tr.numImages == 0 ) {
		return;
	}


	int i;

	vk_wait_idle();

	for (i = 0; i < tr.numImages; i++)
	{
		image_t *img = tr.images[i];
		vk_destroy_image_resources(img->handle, img->view);

		// img->descriptor will be released with pool reset
	}

	Com_Memset(tr.images, 0, sizeof(tr.images));
	Com_Memset(tr.scratchImage, 0, sizeof(tr.scratchImage));
	tr.numImages = 0;

	Com_Memset(glState.currenttextures, 0, sizeof(glState.currenttextures));
}

/*
==================
CommaParse

This is unfortunate, but the skin files aren't
compatible with our normal parsing rules.
==================
*/
static const char *CommaParse(const char **data_p)
{
	int c, len;
	const char *data;
	static char com_token[MAX_TOKEN_CHARS];

	data = *data_p;
	com_token[0] = '\0';

	// make sure incoming data is valid
	if (!data)
	{
		*data_p = NULL;
		return com_token;
	}

	len = 0;

	while (1)
	{
		// skip whitespace
		while ((c = *data) <= ' ')
		{
			if (c == '\0')
			{
				break;
			}
			data++;
		}

		c = *data;

		// skip double slash comments
		if (c == '/' && data[1] == '/')
		{
			data += 2;
			while (*data && *data != '\n')
			{
				data++;
			}
		}
		// skip /* */ comments
		else if (c == '/' && data[1] == '*')
		{
			data += 2;
			while (*data && (*data != '*' || data[1] != '/'))
			{
				data++;
			}
			if (*data)
			{
				data += 2;
			}
		}
		else
		{
			break;
		}
	}

	if (c == '\0')
	{
		return "";
	}

	// handle quoted strings
	if (c == '\"')
	{
		data++;
		while (1)
		{
			c = *data;
			if (c == '\"' || c == '\0')
			{
				if (c == '\"')
					data++;
				com_token[len] = '\0';
				*data_p = data;
				return com_token;
			}
			data++;
			if (len < MAX_TOKEN_CHARS - 1)
			{
				com_token[len] = c;
				len++;
			}
		}
	}

	// parse a regular word
	do
	{
		if (len < MAX_TOKEN_CHARS - 1)
		{
			com_token[len] = c;
			len++;
		}
		data++;
		c = *data;
	} while (c > ' ' && c != ',');

	com_token[len] = '\0';

	*data_p = data;
	return com_token;
}

qhandle_t RE_RegisterSkin(const char *name)
{
	skinSurface_t parseSurfaces[MAX_SKIN_SURFACES]{};
	qhandle_t hSkin;
	skin_t *skin;
	skinSurface_t *surf;
	union
	{
		char *c;
		void *v;
	} text{};
	const char *text_p;
	const char *token;
	char surfName[MAX_QPATH];
	int totalSurfaces;

	if (!name || !name[0])
	{
		ri.Printf(PRINT_DEVELOPER, "Empty name passed to RE_RegisterSkin\n");
		return 0;
	}

	if (strlen(name) >= MAX_QPATH)
	{
		ri.Printf(PRINT_DEVELOPER, "Skin name exceeds MAX_QPATH\n");
		return 0;
	}

	// see if the skin is already loaded
	for (hSkin = 1; hSkin < tr.numSkins; hSkin++)
	{
		skin = tr.skins[hSkin];
		if (!Q_stricmp_cpp(skin->name, name))
		{
			if (skin->numSurfaces == 0)
			{
				return 0; // default skin
			}
			return hSkin;
		}
	}

	// allocate a new skin
	if (tr.numSkins == MAX_SKINS)
	{
		ri.Printf(PRINT_WARNING, "WARNING: RE_RegisterSkin( '%s' ) MAX_SKINS hit\n", name);
		return 0;
	}
	tr.numSkins++;
	skin = reinterpret_cast<skin_t *>(ri.Hunk_Alloc(sizeof(skin_t), h_low));
	tr.skins[hSkin] = skin;
	Q_strncpyz(skin->name, name, sizeof(skin->name));
	skin->numSurfaces = 0;

	// If not a .skin file, load as a single shader
	if (strcmp(name + strlen(name) - 5, ".skin"))
	{
		skin->numSurfaces = 1;
		skin->surfaces = reinterpret_cast<skinSurface_t *>(ri.Hunk_Alloc(sizeof(skinSurface_t), h_low));
		skin->surfaces[0].shader = R_FindShader(name, LIGHTMAP_NONE, true);
		return hSkin;
	}

	// load and parse the skin file
	ri.FS_ReadFile(name, &text.v);
	if (!text.c)
	{
		return 0;
	}

	totalSurfaces = 0;
	text_p = text.c;

	while (text_p && *text_p)
	{
		// get surface name
		token = CommaParse(&text_p);
		Q_strncpyz(surfName, token, sizeof(surfName));

		if (!token[0])
		{
			break;
		}
		// lowercase the surface name so skin compares are faster
		q_strlwr_cpp(std::span(surfName));

		if (*text_p == ',')
		{
			text_p++;
		}

		if (strstr(token, "tag_"))
		{
			continue;
		}

		// parse the shader name
		token = CommaParse(&text_p);

		if (skin->numSurfaces < MAX_SKIN_SURFACES)
		{
			surf = &parseSurfaces[skin->numSurfaces];
			Q_strncpyz(surf->name, surfName, sizeof(surf->name));
			surf->shader = R_FindShader(token, LIGHTMAP_NONE, true);
			skin->numSurfaces++;
		}

		totalSurfaces++;
	}

	ri.FS_FreeFile(text.v);

	if (totalSurfaces > MAX_SKIN_SURFACES)
	{
		ri.Printf(PRINT_WARNING, "WARNING: Ignoring excess surfaces (found %d, max is %d) in skin '%s'!\n",
				  totalSurfaces, MAX_SKIN_SURFACES, name);
	}

	// never let a skin have 0 shaders
	if (skin->numSurfaces == 0)
	{
		return 0; // use default skin
	}

	// copy surfaces to skin
	skin->surfaces = reinterpret_cast<skinSurface_t *>(ri.Hunk_Alloc(skin->numSurfaces * sizeof(skinSurface_t), h_low));
	memcpy(skin->surfaces, parseSurfaces, skin->numSurfaces * sizeof(skinSurface_t));

	return hSkin;
}

void R_InitSkins(void)
{
	skin_t *skin;

	tr.numSkins = 1;

	// make the default skin have all default shaders
	skin = tr.skins[0] = reinterpret_cast<skin_t *>(ri.Hunk_Alloc(sizeof(skin_t), h_low));
	Q_strncpyz(skin->name, "<default skin>", sizeof(skin->name));
	skin->numSurfaces = 1;
	skin->surfaces = reinterpret_cast<skinSurface_t *>(ri.Hunk_Alloc(sizeof(skinSurface_t), h_low));
	skin->surfaces[0].shader = tr.defaultShader;
}
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
#include "tr_cmds.hpp"
#include "tr_bsp.hpp"
#include "tr_local.hpp"
#include "vk.hpp"
#include "tr_shader.hpp"
#include "utils.hpp"
#include "vk_descriptors.hpp"
#include "../renderercommon/tr_image_loaders.h"

#define generateHashValue(fname) Com_GenerateHashValue_cpp((fname), FILE_HASH_SIZE)

#include <algorithm> // for std::clamp
#include <atomic>
#include <condition_variable>
#include <cstdint>	 // for std::uint32_t
#include <cstdarg>
#include <cstdlib>
#include <deque>
#include <memory>
#include <mutex>
#include <new>
#include "string_operations.hpp"
#include <string>
#include <span>
#include <thread>
#include <vector>
#include "vk_pipeline.hpp"

struct ImageDecoderContext
{
	const byte* fileData{};
	int fileSize{};
	std::string* diagnostic{};
	bool useThreadAllocator{};
};

static thread_local ImageDecoderContext* s_imageDecoderContext;

// PNG and PCX use these hooks to decode a main-thread filesystem buffer on a worker.
extern "C" int R_ImageLoaderReadFile(const char* name, void** buffer)
{
	if (!s_imageDecoderContext)
		return ri.FS_ReadFile(name, buffer);

	*buffer = const_cast<byte*>(s_imageDecoderContext->fileData);
	return s_imageDecoderContext->fileSize;
}

extern "C" void R_ImageLoaderFreeFile(void* buffer)
{
	if (!s_imageDecoderContext)
		ri.FS_FreeFile(buffer);
}

extern "C" void* R_ImageLoaderMalloc(const int bytes)
{
	return s_imageDecoderContext && s_imageDecoderContext->useThreadAllocator
		? std::malloc(static_cast<std::size_t>(bytes))
		: ri.Malloc(bytes);
}

extern "C" void R_ImageLoaderFree(void* buffer)
{
	if (s_imageDecoderContext && s_imageDecoderContext->useThreadAllocator)
		std::free(buffer);
	else
		ri.Free(buffer);
}

extern "C" void QDECL R_ImageLoaderPrint(const int level, const char* format, ...)
{
	char message[1024];
	va_list args;
	va_start(args, format);
	Q_vsnprintf(message, sizeof(message), format, args);
	va_end(args);

	if (s_imageDecoderContext)
	{
		if (s_imageDecoderContext->diagnostic)
			*s_imageDecoderContext->diagnostic += message;
		return;
	}

	ri.Printf(static_cast<printParm_t>(level), "%s", message);
}

// Note that the ordering indicates the order of preference used
// when there are multiple images of different formats available
static const imageExtToLoaderMap_t imageLoaders[] =
{
	{"png", R_LoadPNG},
	{"tga", R_LoadTGA},
	{"jpg", R_LoadJPG},
	{"jpeg", R_LoadJPG},
	{"pcx", R_LoadPCX},
	{"bmp", R_LoadBMP} };

static byte s_intensitytable[256];
static unsigned char s_gammatable[256];

// initialize linear gamma table before setting color mappings for the first time
static  std::array<unsigned char, 256> s_gammatable_linear = make_linear_gamma_table();

constexpr int FILE_HASH_SIZE = 1024;
static image_t* hashTable[FILE_HASH_SIZE];

static const int numImageLoaders = arrayLen(imageLoaders);

static void R_WarnMixedImageFlags(const image_t& image, std::string_view name, const imgFlags_t flags)
{
	if (name.compare("*white") == 0)
	{
		return;
	}

	if (image.flags != flags)
	{
		ri.Printf(PRINT_DEVELOPER, "WARNING: reused image %s with mixed flags (%i vs %i)\n",
			name.data(), static_cast<int>(image.flags), static_cast<int>(flags));
	}
}

static image_t* R_FindCachedImage(std::string_view name, const imgFlags_t flags)
{
	const int hash = generateHashValue(name);

	for (image_t* image = hashTable[hash]; image; image = image->next)
	{
		if (!Q_stricmp_cpp(name, image->imgName))
		{
			R_WarnMixedImageFlags(*image, name, flags);
			return image;
		}
	}

	return nullptr;
}

GLint gl_filter_min = std::to_underlying(glCompat::GL_LINEAR_MIPMAP_NEAREST);
GLint gl_filter_max = std::to_underlying(glCompat::GL_LINEAR);

constexpr textureMode_t modes[6] = { // Texture modes
	{"GL_NEAREST", glCompat::GL_NEAREST, glCompat::GL_NEAREST},
	{"GL_LINEAR", glCompat::GL_LINEAR, glCompat::GL_LINEAR},
	{"GL_NEAREST_MIPMAP_NEAREST", glCompat::GL_NEAREST_MIPMAP_NEAREST, glCompat::GL_NEAREST},
	{"GL_LINEAR_MIPMAP_NEAREST", glCompat::GL_LINEAR_MIPMAP_NEAREST, glCompat::GL_LINEAR},
	{"GL_NEAREST_MIPMAP_LINEAR", glCompat::GL_NEAREST_MIPMAP_LINEAR, glCompat::GL_NEAREST},
	{"GL_LINEAR_MIPMAP_LINEAR", glCompat::GL_LINEAR_MIPMAP_LINEAR, glCompat::GL_LINEAR} };

skin_t* R_GetSkinByHandle(qhandle_t hSkin)
{
	if (hSkin < 1 || hSkin >= tr.numSkins)
	{
		return tr.skins[0];
	}
	return tr.skins[hSkin];
}

int R_SumOfUsedImages(const int frameCount)
{
	const image_t* img;
	int i, total = 0;

	for (i = 0; i < tr.numImages; i++)
	{
		img = tr.images[i];
		if (img->frameUsed == frameCount)
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

void R_GammaCorrect(byte* buffer, const int bufSize)
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
	R_SyncRenderThread();
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
			ri.GLimp_SetGamma(s_gammatable_linear.data(), s_gammatable_linear.data(), s_gammatable_linear.data());
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
static void R_LightScaleTexture(byte* in, int inwidth, int inheight, bool only_gamma)
{
	if (in == NULL)
		return;

	if (only_gamma)
	{
		if (!glConfig.deviceSupportsGamma && !vk_inst.fboActive)
		{
			int i, c;
			byte* p;

			p = (byte*)in;

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
		byte* p;

		p = (byte*)in;

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
	const textureMode_t* mode{};
	image_t* img;
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

	gl_filter_min = std::to_underlying(mode->minimize);
	gl_filter_max = std::to_underlying(mode->maximize);

	if (gl_filter_min == vk_inst.samplers.filter_min && gl_filter_max == vk_inst.samplers.filter_max) {
		return;
	}

	vk_wait_idle();
	vk_destroy_samplers();

	vk_inst.samplers.filter_min = gl_filter_min;
	vk_inst.samplers.filter_max = gl_filter_max;
	vk_update_attachment_descriptors();
	for (i = 0; i < tr.numImages; i++)
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
	R_SyncRenderThread();
	int i, estTotalSize = 0;
	char* name, buf[MAX_QPATH * 2 + 5];

	ri.Printf(PRINT_ALL, "\n -n- --w-- --h-- type  -size- --name-------\n");

	for (i = 0; i < tr.numImages; i++)
	{
		const char* format = "???? ";
		const char* sizeSuffix;
		int estSize;
		int displaySize;

		const image_t* image = tr.images[i];
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

static bool RawImage_HasAlpha(const byte* scan, const int numPixels)
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
static void R_BlendOverTexture(byte* data, int pixelCount, int mipLevel)
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
		{255, 0, 255, 128} };

	const byte* blend = blendColors[(mipLevel - 1) % arrayLen(blendColors)];

	int i;
	int inverseAlpha = 255 - blend[3];
	int premult[3]{
		blend[0] * blend[3],
		blend[1] * blend[3],
		blend[2] * blend[3] };

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
static void ResampleTexture(unsigned* in, int inwidth, int inheight, unsigned* out,
	int outwidth, int outheight)
{
	int i, j;
	unsigned* inrow, * inrow2;
	unsigned frac, fracstep;
	std::array<unsigned, MAX_TEXTURE_SIZE> p1{};
	std::array<unsigned, MAX_TEXTURE_SIZE> p2{};
	byte* pix1, * pix2, * pix3, * pix4;

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
			pix1 = (byte*)inrow + p1[j];
			pix2 = (byte*)inrow + p2[j];
			pix3 = (byte*)inrow2 + p1[j];
			pix4 = (byte*)inrow2 + p2[j];
			((byte*)(out + j))[0] = (pix1[0] + pix2[0] + pix3[0] + pix4[0]) >> 2;
			((byte*)(out + j))[1] = (pix1[1] + pix2[1] + pix3[1] + pix4[1]) >> 2;
			((byte*)(out + j))[2] = (pix1[2] + pix2[2] + pix3[2] + pix4[2]) >> 2;
			((byte*)(out + j))[3] = (pix1[3] + pix2[3] + pix3[3] + pix4[3]) >> 2;
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
static void R_MipMap2(unsigned* const out, unsigned* const in, int inWidth, int inHeight)
{
	int i, j, k;
	byte* outpix;
	int inWidthMask, inHeightMask;
	int total;
	int outWidth, outHeight;
	unsigned* temp;

	outWidth = inWidth >> 1;
	outHeight = inHeight >> 1;

	if (out == in)
		temp = static_cast<unsigned int*>(ri.Hunk_AllocateTempMemory(outWidth * outHeight * 4));
	else
		temp = out;

	inWidthMask = inWidth - 1;
	inHeightMask = inHeight - 1;

	for (i = 0; i < outHeight; i++)
	{
		for (j = 0; j < outWidth; j++)
		{
			outpix = (byte*)(temp + i * outWidth + j);
			for (k = 0; k < 4; k++)
			{
				total =
					1 * ((byte*)&in[((i * 2 - 1) & inHeightMask) * inWidth + ((j * 2 - 1) & inWidthMask)])[k] +
					2 * ((byte*)&in[((i * 2 - 1) & inHeightMask) * inWidth + ((j * 2) & inWidthMask)])[k] +
					2 * ((byte*)&in[((i * 2 - 1) & inHeightMask) * inWidth + ((j * 2 + 1) & inWidthMask)])[k] +
					1 * ((byte*)&in[((i * 2 - 1) & inHeightMask) * inWidth + ((j * 2 + 2) & inWidthMask)])[k] +

					2 * ((byte*)&in[((i * 2) & inHeightMask) * inWidth + ((j * 2 - 1) & inWidthMask)])[k] +
					4 * ((byte*)&in[((i * 2) & inHeightMask) * inWidth + ((j * 2) & inWidthMask)])[k] +
					4 * ((byte*)&in[((i * 2) & inHeightMask) * inWidth + ((j * 2 + 1) & inWidthMask)])[k] +
					2 * ((byte*)&in[((i * 2) & inHeightMask) * inWidth + ((j * 2 + 2) & inWidthMask)])[k] +

					2 * ((byte*)&in[((i * 2 + 1) & inHeightMask) * inWidth + ((j * 2 - 1) & inWidthMask)])[k] +
					4 * ((byte*)&in[((i * 2 + 1) & inHeightMask) * inWidth + ((j * 2) & inWidthMask)])[k] +
					4 * ((byte*)&in[((i * 2 + 1) & inHeightMask) * inWidth + ((j * 2 + 1) & inWidthMask)])[k] +
					2 * ((byte*)&in[((i * 2 + 1) & inHeightMask) * inWidth + ((j * 2 + 2) & inWidthMask)])[k] +

					1 * ((byte*)&in[((i * 2 + 2) & inHeightMask) * inWidth + ((j * 2 - 1) & inWidthMask)])[k] +
					2 * ((byte*)&in[((i * 2 + 2) & inHeightMask) * inWidth + ((j * 2) & inWidthMask)])[k] +
					2 * ((byte*)&in[((i * 2 + 2) & inHeightMask) * inWidth + ((j * 2 + 1) & inWidthMask)])[k] +
					1 * ((byte*)&in[((i * 2 + 2) & inHeightMask) * inWidth + ((j * 2 + 2) & inWidthMask)])[k];
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
static void R_MipMap(byte* out, byte* in, int width, int height)
{
	int i, j;
	int row;

	if (in == NULL)
		return;

	if (!r_simpleMipMaps->integer)
	{
		R_MipMap2((unsigned*)out, (unsigned*)in, width, height);
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

struct ImagePrepareSettings
{
	int maxTextureSize{};
	int picMip{};
	int textureBits{};
	int mapOverBrightBits{};
	int overBrightBits{};
	int mapGreyScaleInteger{};
	bool roundImagesDown{};
	bool applyPicMip{};
	bool colorMipLevels{};
	bool simpleMipMaps{};
	bool deviceSupportsGamma{};
	bool fboActive{};
	float mapGreyScale{};
	std::array<byte, 256> gammaTable{};
	std::array<byte, 256> intensityTable{};
};

struct PreparedImageData
{
	std::vector<byte> pixels;
	int sourceWidth{};
	int sourceHeight{};
	int uploadWidth{};
	int uploadHeight{};
	int mipLevels{};
	bool hasAlpha{};
};

static ImagePrepareSettings R_GetImagePrepareSettings()
{
	ImagePrepareSettings settings{};
	settings.maxTextureSize = glConfig.maxTextureSize;
	settings.picMip = r_picmip->integer;
	settings.textureBits = r_texturebits->integer;
	settings.mapOverBrightBits = r_mapOverBrightBits->integer;
	settings.overBrightBits = tr.overbrightBits;
	settings.mapGreyScaleInteger = r_mapGreyScale->integer;
	settings.roundImagesDown = r_roundImagesDown->integer != 0;
	settings.applyPicMip = tr.mapLoading || r_nomip->integer == 0;
	settings.colorMipLevels = r_colorMipLevels->integer != 0;
	settings.simpleMipMaps = r_simpleMipMaps->integer != 0;
	settings.deviceSupportsGamma = glConfig.deviceSupportsGamma;
	settings.fboActive = vk_inst.fboActive;
	settings.mapGreyScale = tr.mapLoading ? r_mapGreyScale->value : 0.0f;
	std::copy(std::begin(s_gammatable), std::end(s_gammatable), settings.gammaTable.begin());
	std::copy(std::begin(s_intensitytable), std::end(s_intensitytable), settings.intensityTable.begin());
	return settings;
}

static void R_MipMapForJob(byte* data, const int width, const int height, const bool simple)
{
	if (simple)
	{
		const int rowBytes = width * 4;
		const int outWidth = width >> 1;
		const int outHeight = height >> 1;
		if (outWidth == 0 || outHeight == 0)
		{
			const int count = outWidth + outHeight;
			for (int i = 0; i < count; ++i)
			{
				for (int channel = 0; channel < 4; ++channel)
					data[i * 4 + channel] = (data[i * 8 + channel] + data[i * 8 + 4 + channel]) >> 1;
			}
			return;
		}

		byte* source = data;
		byte* target = data;
		for (int y = 0; y < outHeight; ++y, source += rowBytes)
		{
			for (int x = 0; x < outWidth; ++x, source += 8, target += 4)
			{
				for (int channel = 0; channel < 4; ++channel)
					target[channel] = (source[channel] + source[4 + channel] +
						source[rowBytes + channel] + source[rowBytes + 4 + channel]) >> 2;
			}
		}
		return;
	}

	const int outWidth = width >> 1;
	const int outHeight = height >> 1;
	if (outWidth == 0 || outHeight == 0)
	{
		const int count = outWidth + outHeight;
		for (int i = 0; i < count; ++i)
		{
			for (int channel = 0; channel < 4; ++channel)
				data[i * 4 + channel] = (data[i * 8 + channel] + data[i * 8 + 4 + channel]) >> 1;
		}
		return;
	}

	std::vector<byte> output(static_cast<std::size_t>(outWidth) * outHeight * 4);
	const int widthMask = width - 1;
	const int heightMask = height - 1;

	for (int y = 0; y < outHeight; ++y)
	{
		for (int x = 0; x < outWidth; ++x)
		{
			byte* target = output.data() + (y * outWidth + x) * 4;
			for (int channel = 0; channel < 4; ++channel)
			{
				int total = 0;
				static constexpr int weights[4][4] = {
					{1, 2, 2, 1}, {2, 4, 4, 2}, {2, 4, 4, 2}, {1, 2, 2, 1}};
				for (int row = 0; row < 4; ++row)
				{
					const int sourceY = (y * 2 + row - 1) & heightMask;
					for (int column = 0; column < 4; ++column)
					{
						const int sourceX = (x * 2 + column - 1) & widthMask;
						total += weights[row][column] * data[(sourceY * width + sourceX) * 4 + channel];
					}
				}
				target[channel] = total / 36;
			}
		}
	}

	std::copy(output.begin(), output.end(), data);
}

static std::vector<byte> R_ResampleTextureForJob(const std::vector<byte>& input,
	const int inputWidth, const int inputHeight, const int outputWidth, const int outputHeight)
{
	std::vector<unsigned> firstColumn(outputWidth);
	std::vector<unsigned> secondColumn(outputWidth);
	const unsigned step = inputWidth * 0x10000 / outputWidth;
	unsigned fraction = step >> 2;
	for (int x = 0; x < outputWidth; ++x, fraction += step)
		firstColumn[x] = 4 * (fraction >> 16);
	fraction = 3 * (step >> 2);
	for (int x = 0; x < outputWidth; ++x, fraction += step)
		secondColumn[x] = 4 * (fraction >> 16);

	std::vector<byte> output(static_cast<std::size_t>(outputWidth) * outputHeight * 4);
	for (int y = 0; y < outputHeight; ++y)
	{
		const byte* firstRow = input.data() + inputWidth * 4 * static_cast<int>((y + 0.25f) * inputHeight / outputHeight);
		const byte* secondRow = input.data() + inputWidth * 4 * static_cast<int>((y + 0.75f) * inputHeight / outputHeight);
		for (int x = 0; x < outputWidth; ++x)
		{
			const byte* first = firstRow + firstColumn[x];
			const byte* second = firstRow + secondColumn[x];
			const byte* third = secondRow + firstColumn[x];
			const byte* fourth = secondRow + secondColumn[x];
			byte* target = output.data() + (y * outputWidth + x) * 4;
			for (int channel = 0; channel < 4; ++channel)
				target[channel] = (first[channel] + second[channel] + third[channel] + fourth[channel]) >> 2;
		}
	}
	return output;
}

static void R_LightScaleTextureForJob(byte* pixels, const int width, const int height,
	const bool onlyGamma, const ImagePrepareSettings& settings)
{
	const int count = width * height;
	for (int i = 0; i < count; ++i, pixels += 4)
	{
		for (int channel = 0; channel < 3; ++channel)
		{
			if (onlyGamma)
			{
				if (!settings.deviceSupportsGamma && !settings.fboActive)
					pixels[channel] = settings.gammaTable[pixels[channel]];
			}
			else if (settings.deviceSupportsGamma || settings.fboActive)
			{
				pixels[channel] = settings.intensityTable[pixels[channel]];
			}
			else
			{
				pixels[channel] = settings.gammaTable[settings.intensityTable[pixels[channel]]];
			}
		}
	}
}

static void R_ApplyMapGreyScale(std::vector<byte>& pixels, const float amount)
{
	if (amount <= 0.0f)
		return;

	for (std::size_t i = 0; i + 3 < pixels.size(); i += 4)
	{
		byte* pixel = pixels.data() + i;
		const float luma = LUMA(pixel[0], pixel[1], pixel[2]);
		if (amount >= 1.0f)
		{
			pixel[0] = pixel[1] = pixel[2] = static_cast<byte>(luma);
		}
		else
		{
			pixel[0] = static_cast<byte>(LERP(pixel[0], luma, amount));
			pixel[1] = static_cast<byte>(LERP(pixel[1], luma, amount));
			pixel[2] = static_cast<byte>(LERP(pixel[2], luma, amount));
		}
	}
}

static void R_ColorShiftForJob(byte* pixel, const ImagePrepareSettings& settings)
{
	const int shift = settings.mapOverBrightBits - settings.overBrightBits;
	int red = shift >= 0 ? pixel[0] << shift : pixel[0] >> -shift;
	int green = shift >= 0 ? pixel[1] << shift : pixel[1] >> -shift;
	int blue = shift >= 0 ? pixel[2] << shift : pixel[2] >> -shift;
	if ((red | green | blue) > 255)
	{
		const int highest = std::max({red, green, blue});
		red = red * 255 / highest;
		green = green * 255 / highest;
		blue = blue * 255 / highest;
	}

	if (settings.mapGreyScaleInteger)
	{
		const byte luma = LUMA(red, green, blue);
		pixel[0] = pixel[1] = pixel[2] = luma;
	}
	else if (settings.mapGreyScale != 0.0f)
	{
		const float amount = std::abs(settings.mapGreyScale);
		const float luma = LUMA(red, green, blue);
		pixel[0] = static_cast<byte>(LERP(red, luma, amount));
		pixel[1] = static_cast<byte>(LERP(green, luma, amount));
		pixel[2] = static_cast<byte>(LERP(blue, luma, amount));
	}
	else
	{
		pixel[0] = static_cast<byte>(red);
		pixel[1] = static_cast<byte>(green);
		pixel[2] = static_cast<byte>(blue);
	}
}

static PreparedImageData R_PrepareImageForUpload(std::vector<byte> source, const int sourceWidth,
	const int sourceHeight, const imgFlags_t flags, const ImagePrepareSettings& settings)
{
	PreparedImageData result{};
	result.sourceWidth = sourceWidth;
	result.sourceHeight = sourceHeight;
	R_ApplyMapGreyScale(source, settings.mapGreyScale);

	int width = sourceWidth;
	int height = sourceHeight;
	int scaledWidth = width;
	int scaledHeight = height;
	const bool mipMap = HasFlag(flags, imgFlags_t::IMGFLAG_MIPMAP);

	if (!HasFlag(flags, imgFlags_t::IMGFLAG_NOSCALE))
	{
		for (scaledWidth = 1; scaledWidth < width; scaledWidth <<= 1) {}
		for (scaledHeight = 1; scaledHeight < height; scaledHeight <<= 1) {}
		if (settings.roundImagesDown && scaledWidth > width) scaledWidth >>= 1;
		if (settings.roundImagesDown && scaledHeight > height) scaledHeight >>= 1;
	}

	while (scaledWidth > settings.maxTextureSize || scaledHeight > settings.maxTextureSize)
	{
		scaledWidth >>= 1;
		scaledHeight >>= 1;
	}

	if (scaledWidth != width || scaledHeight != height)
		source = R_ResampleTextureForJob(source, width, height, scaledWidth, scaledHeight);

	width = scaledWidth;
	height = scaledHeight;
	if (HasFlag(flags, imgFlags_t::IMGFLAG_COLORSHIFT))
	{
		for (int i = 0; i < width * height; ++i)
			R_ColorShiftForJob(source.data() + i * 4, settings);
	}

	if (HasFlag(flags, imgFlags_t::IMGFLAG_PICMIP) && settings.applyPicMip)
	{
		scaledWidth >>= settings.picMip;
		scaledHeight >>= settings.picMip;
	}
	scaledWidth = std::max(1, scaledWidth);
	scaledHeight = std::max(1, scaledHeight);
	if (scaledWidth == width && scaledHeight == height && !mipMap)
	{
		result.uploadWidth = scaledWidth;
		result.uploadHeight = scaledHeight;
		result.mipLevels = 1;
		result.pixels = std::move(source);
		result.hasAlpha = RawImage_HasAlpha(result.pixels.data(), result.uploadWidth * result.uploadHeight);
		return result;
	}

	while (width > scaledWidth || height > scaledHeight)
	{
		R_MipMapForJob(source.data(), width, height, settings.simpleMipMaps);
		width = std::max(1, width >> 1);
		height = std::max(1, height >> 1);
	}

	std::vector<byte> level(source.begin(), source.begin() + static_cast<std::size_t>(scaledWidth) * scaledHeight * 4);
	if (!HasFlag(flags, imgFlags_t::IMGFLAG_NOLIGHTSCALE))
		R_LightScaleTextureForJob(level.data(), scaledWidth, scaledHeight, !mipMap, settings);

	result.uploadWidth = scaledWidth;
	result.uploadHeight = scaledHeight;
	result.mipLevels = 1;
	result.pixels = level;

	if (mipMap)
	{
		while (scaledWidth > 1 && scaledHeight > 1)
		{
			R_MipMapForJob(level.data(), scaledWidth, scaledHeight, settings.simpleMipMaps);
			scaledWidth = std::max(1, scaledWidth >> 1);
			scaledHeight = std::max(1, scaledHeight >> 1);
			const std::size_t levelSize = static_cast<std::size_t>(scaledWidth) * scaledHeight * 4;
			if (settings.colorMipLevels)
				R_BlendOverTexture(level.data(), scaledWidth * scaledHeight, result.mipLevels);
			result.pixels.insert(result.pixels.end(), level.begin(), level.begin() + levelSize);
			++result.mipLevels;
		}
	}

	result.hasAlpha = RawImage_HasAlpha(result.pixels.data(), result.uploadWidth * result.uploadHeight);
	return result;
}

static void generate_image_upload_data(image_t* image, byte* data, Image_Upload_Data* upload_data)
{

	bool mipmap = HasFlag(image->flags, imgFlags_t::IMGFLAG_MIPMAP);
	bool picmip = HasFlag(image->flags, imgFlags_t::IMGFLAG_PICMIP);
	byte* resampled_buffer = NULL;
	int scaled_width, scaled_height;
	int width = image->width;
	int height = image->height;
	unsigned* scaled_buffer;
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

	upload_data->buffer = (byte*)ri.Hunk_AllocateTempMemory(2 * 4 * scaled_width * scaled_height);
	if (data == NULL)
	{
		Com_Memset(upload_data->buffer, 0, 2 * 4 * scaled_width * scaled_height);
	}

	if ((scaled_width != width || scaled_height != height) && data)
	{
		resampled_buffer = (byte*)ri.Hunk_AllocateTempMemory(scaled_width * scaled_height * 4);
		ResampleTexture((unsigned*)data, width, height, (unsigned*)resampled_buffer, scaled_width, scaled_height);
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
			byte* p = data;
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

	scaled_buffer = (unsigned int*)ri.Hunk_AllocateTempMemory(sizeof(unsigned) * scaled_width * scaled_height);
	Com_Memcpy(scaled_buffer, data, scaled_width * scaled_height * 4);

	if (!HasFlag(image->flags, imgFlags_t::IMGFLAG_NOLIGHTSCALE))
	{
		R_LightScaleTexture((byte*)scaled_buffer, scaled_width, scaled_height, !mipmap);
	}

	mip_level_size = scaled_width * scaled_height * 4;

	Com_Memcpy(upload_data->buffer, scaled_buffer, mip_level_size);
	upload_data->buffer_size = mip_level_size;

	if (mipmap)
	{
		while (scaled_width > 1 && scaled_height > 1)
		{
			R_MipMap((byte*)scaled_buffer, (byte*)scaled_buffer, scaled_width, scaled_height);

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
				R_BlendOverTexture((byte*)scaled_buffer, scaled_width * scaled_height, miplevel);
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

static void upload_vk_image(image_t* image, byte* pic)
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

static void R_UploadPreparedImage(image_t& image, const PreparedImageData& data, const int textureBits)
{
	image.width = data.sourceWidth;
	image.height = data.sourceHeight;
	image.uploadWidth = data.uploadWidth;
	image.uploadHeight = data.uploadHeight;
	image.internalFormat = (textureBits > 16 || textureBits == 0 || HasFlag(image.flags, imgFlags_t::IMGFLAG_LIGHTMAP))
		? vk::Format::eR8G8B8A8Unorm
		: (data.hasAlpha ? vk::Format::eB4G4R4A4UnormPack16 : vk::Format::eA1R5G5B5UnormPack16);

	vk_create_image(image, data.uploadWidth, data.uploadHeight, data.mipLevels);
	vk_upload_image_data(image, 0, 0, data.uploadWidth, data.uploadHeight, data.mipLevels,
		const_cast<byte*>(data.pixels.data()), static_cast<int>(data.pixels.size()), false);
}

static image_t* R_AllocateImage(std::string_view name, std::string_view name2,
	const int width, const int height, const imgFlags_t flags)
{
	const int nameLength = static_cast<int>(name.size()) + 1;
	if (nameLength > MAX_QPATH)
		ri.Error(ERR_DROP, "R_CreateImage: \"%s\" is too long", name.data());

	int secondNameLength = 0;
	if (!name2.empty() && Q_stricmp_cpp(name, name2) != 0)
	{
		const auto slashPos = name2.rfind('/');
		if (slashPos != std::string_view::npos)
			name2 = name2.substr(slashPos + 1);
		secondNameLength = static_cast<int>(name2.size()) + 1;
	}

	if (tr.numImages == MAX_DRAWIMAGES)
		ri.Error(ERR_DROP, "R_CreateImage: MAX_DRAWIMAGES hit");

	void* imageMemory = ri.Hunk_Alloc(sizeof(image_t) + nameLength + secondNameLength, h_low);
	auto* image = ::new (imageMemory) image_t{};
	image->imgName = reinterpret_cast<char*>(image + 1);
	std::memcpy(image->imgName, name.data(), name.size());
	image->imgName[name.size()] = '\0';
	if (secondNameLength)
	{
		image->imgName2 = image->imgName + nameLength;
		std::memcpy(image->imgName2, name2.data(), name2.size());
		image->imgName2[name2.size()] = '\0';
	}
	else
	{
		image->imgName2 = image->imgName;
	}

	const long hash = generateHashValue(name);
	image->next = hashTable[hash];
	hashTable[hash] = image;
	tr.images[tr.numImages++] = image;
	image->flags = flags;
	image->width = width;
	image->height = height;

	if (nameLength > 6 && Q_stristr_cpp(image->imgName, "maps/") == image->imgName &&
		Q_stristr_cpp(image->imgName + 6, "/lm_") != nullptr)
	{
		image->flags = static_cast<imgFlags_t>(image->flags |
			imgFlags_t::IMGFLAG_NO_COMPRESSION | imgFlags_t::IMGFLAG_NOSCALE);
	}

	if (HasFlag(flags, imgFlags_t::IMGFLAG_CLAMPTOBORDER))
		image->wrapClampMode = vk::SamplerAddressMode::eClampToBorder;
	else if (HasFlag(flags, imgFlags_t::IMGFLAG_CLAMPTOEDGE))
		image->wrapClampMode = vk::SamplerAddressMode::eClampToEdge;
	else
		image->wrapClampMode = vk::SamplerAddressMode::eRepeat;

	return image;
}

/*
================
R_CreateImage

Synchronous image creation path.
Picture data may be modified in-place during mipmap processing
================
*/
image_t* R_CreateImage(std::string_view name, std::string_view name2, byte* pic, int width, int height, imgFlags_t flags)
{
	image_t* image = R_AllocateImage(name, name2, width, height, flags);
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
static std::array<char, MAX_QPATH> R_LoadImage(std::string_view name, byte** pic, int* width, int* height)
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

struct ImageLoadJob
{
	image_t* image{};
	std::array<char, MAX_QPATH> fileName{};
	int loaderIndex{-1};
	imgFlags_t flags{};
	ImagePrepareSettings settings{};
	std::vector<byte> fileData;
	std::vector<byte> decodedPixels;
	PreparedImageData prepared;
	std::string diagnostic;
	int width{};
	int height{};
	bool decodeInWorker{};
	bool success{};
	std::atomic<bool> done{};
};

static std::mutex s_imageLoadMutex;
static std::condition_variable s_imageLoadCv;
static std::deque<ImageLoadJob*> s_imageWorkQueue;
static std::vector<std::unique_ptr<ImageLoadJob>> s_imageLoadJobs;
static std::vector<std::jthread> s_imageLoadWorkers;
static std::size_t s_pendingImageJobs;
static unsigned s_imageLoadThreadCount;
static std::size_t s_nextImageUpload;
static int64_t s_imageLoadStartTime;
static bool s_imageLoadQueueActive;
static bool s_stopImageWorkers;

static bool R_CanDecodeImageInWorker(const int loaderIndex)
{
	return imageLoaders[loaderIndex].ImageLoader == R_LoadPNG ||
		imageLoaders[loaderIndex].ImageLoader == R_LoadPCX;
}

static bool R_ReadImageFileData(const char* fileName, std::vector<byte>& data)
{
	void* fileBuffer = nullptr;
	const int fileSize = ri.FS_ReadFile(fileName, &fileBuffer);
	if (!fileBuffer || fileSize <= 0)
	{
		if (fileBuffer)
			ri.FS_FreeFile(fileBuffer);
		return false;
	}

	const auto* bytes = static_cast<const byte*>(fileBuffer);
	data.assign(bytes, bytes + fileSize);
	ri.FS_FreeFile(fileBuffer);
	return true;
}

static bool R_TryPrepareImageJobInput(ImageLoadJob& job, const char* fileName, const int loaderIndex)
{
	Q_strncpyz(job.fileName.data(), fileName, job.fileName.size());
	job.loaderIndex = loaderIndex;
	job.decodeInWorker = R_CanDecodeImageInWorker(loaderIndex);

	if (imageLoaders[loaderIndex].ImageLoader == R_LoadJPG)
	{
		// JPEG decoding lives in the client import and cannot consume our memory buffer.
		byte* pixels = nullptr;
		imageLoaders[loaderIndex].ImageLoader(fileName, &pixels, &job.width, &job.height);
		if (!pixels)
			return false;
		const std::size_t pixelBytes = static_cast<std::size_t>(job.width) * job.height * 4;
		job.decodedPixels.assign(pixels, pixels + pixelBytes);
		ri.Free(pixels);
		return true;
	}

	if (!R_ReadImageFileData(fileName, job.fileData))
		return false;
	if (job.decodeInWorker)
		return true;

	byte* pixels = nullptr;
	ImageDecoderContext context{job.fileData.data(), static_cast<int>(job.fileData.size()), nullptr, false};
	s_imageDecoderContext = &context;
	imageLoaders[loaderIndex].ImageLoader(fileName, &pixels, &job.width, &job.height);
	s_imageDecoderContext = nullptr;
	if (!pixels)
		return false;

	const std::size_t pixelBytes = static_cast<std::size_t>(job.width) * job.height * 4;
	job.decodedPixels.assign(pixels, pixels + pixelBytes);
	ri.Free(pixels);
	std::vector<byte>().swap(job.fileData);
	return true;
}

static bool R_PrepareImageJobInput(std::string_view name, ImageLoadJob& job)
{
	std::array<char, MAX_QPATH> baseName{};
	std::array<char, MAX_QPATH> candidate{};
	Q_strncpyz_cpp(baseName, name, baseName.size());

	int originalLoader = -1;
	const std::string_view extension = COM_GetExtension_cpp(baseName);
	if (!extension.empty())
	{
		for (int i = 0; i < numImageLoaders; ++i)
		{
			if (Q_stricmp_cpp(extension, imageLoaders[i].ext) != 0)
				continue;

			if (R_TryPrepareImageJobInput(job, baseName.data(), i))
				return true;
			originalLoader = i;
			COM_StripExtension_cpp(name, baseName);
			break;
		}
	}

	for (int i = 0; i < numImageLoaders; ++i)
	{
		if (i == originalLoader)
			continue;
		Com_sprintf(candidate.data(), candidate.size(), "%s.%s", baseName.data(), imageLoaders[i].ext);
		if (R_TryPrepareImageJobInput(job, candidate.data(), i))
			return true;
	}

	return false;
}

static void R_ProcessImageJob(ImageLoadJob& job)
{
	if (job.decodeInWorker)
	{
		byte* pixels = nullptr;
		ImageDecoderContext context{job.fileData.data(), static_cast<int>(job.fileData.size()), &job.diagnostic, true};
		s_imageDecoderContext = &context;
		imageLoaders[job.loaderIndex].ImageLoader(job.fileName.data(), &pixels, &job.width, &job.height);
		if (pixels && job.width > 0 && job.height > 0)
		{
			const std::size_t pixelBytes = static_cast<std::size_t>(job.width) * job.height * 4;
			job.decodedPixels.assign(pixels, pixels + pixelBytes);
		}
		R_ImageLoaderFree(pixels);
		s_imageDecoderContext = nullptr;
		std::vector<byte>().swap(job.fileData);
	}

	if (job.decodedPixels.empty() || job.width <= 0 || job.height <= 0)
		return;

	job.prepared = R_PrepareImageForUpload(std::move(job.decodedPixels), job.width, job.height,
		job.flags, job.settings);
	job.success = !job.prepared.pixels.empty();
}

static void R_ImageLoadWorker()
{
	for (;;)
	{
		ImageLoadJob* job;
		{
			std::unique_lock lock(s_imageLoadMutex);
			s_imageLoadCv.wait(lock, [] { return s_stopImageWorkers || !s_imageWorkQueue.empty(); });
			if (s_stopImageWorkers && s_imageWorkQueue.empty())
				return;
			job = s_imageWorkQueue.front();
			s_imageWorkQueue.pop_front();
		}

		R_ProcessImageJob(*job);
		job->done.store(true, std::memory_order_release);

		{
			std::lock_guard lock(s_imageLoadMutex);
			--s_pendingImageJobs;
		}
		s_imageLoadCv.notify_all();
	}
}

static void R_UploadFinishedImageJobs()
{
	while (s_nextImageUpload < s_imageLoadJobs.size())
	{
		ImageLoadJob& job = *s_imageLoadJobs[s_nextImageUpload];
		if (!job.done.load(std::memory_order_acquire))
			return;

		if (!job.diagnostic.empty())
			ri.Printf(PRINT_WARNING, "%s", job.diagnostic.c_str());

		if (job.success)
		{
			R_UploadPreparedImage(*job.image, job.prepared, job.settings.textureBits);
		}
		else
		{
			ri.Printf(PRINT_WARNING, "WARNING: failed to prepare image %s\n", job.fileName.data());
			PreparedImageData fallback{};
			fallback.pixels = {255, 0, 255, 255};
			fallback.sourceWidth = fallback.sourceHeight = 1;
			fallback.uploadWidth = fallback.uploadHeight = 1;
			fallback.mipLevels = 1;
			R_UploadPreparedImage(*job.image, fallback, 32);
		}

		std::vector<byte>().swap(job.prepared.pixels);
		++s_nextImageUpload;
	}
}

void R_BeginParallelImageLoads()
{
	if (s_imageLoadQueueActive)
		return;

	const int requestedThreads = ri.Cvar_Get("r_imageLoadThreads", "0", CVAR_ARCHIVE)->integer;
	unsigned threadCount = requestedThreads > 0
		? static_cast<unsigned>(std::clamp(requestedThreads, 1, 32))
		: std::min(8u, std::max(1u, std::thread::hardware_concurrency()));

	s_stopImageWorkers = false;
	s_pendingImageJobs = 0;
	s_nextImageUpload = 0;
	s_imageLoadQueueActive = true;
	s_imageLoadThreadCount = threadCount;
	s_imageLoadStartTime = ri.Microseconds();
	s_imageLoadWorkers.reserve(threadCount);
	for (unsigned i = 0; i < threadCount; ++i)
		s_imageLoadWorkers.emplace_back(R_ImageLoadWorker);
}

void R_FinishParallelImageLoads()
{
	if (!s_imageLoadQueueActive)
		return;

	{
		std::unique_lock lock(s_imageLoadMutex);
		s_imageLoadCv.wait(lock, [] { return s_pendingImageJobs == 0; });
		s_stopImageWorkers = true;
	}
	s_imageLoadCv.notify_all();
	for (auto& worker : s_imageLoadWorkers)
		worker.join();
	s_imageLoadWorkers.clear();
	s_imageLoadQueueActive = false;

	R_UploadFinishedImageJobs();

	const std::size_t workerDecoded = static_cast<std::size_t>(std::count_if(
		s_imageLoadJobs.begin(), s_imageLoadJobs.end(),
		[](const auto& job) { return job->decodeInWorker; }));
	const double elapsedMilliseconds = (ri.Microseconds() - s_imageLoadStartTime) / 1000.0;
	ri.Printf(PRINT_DEVELOPER,
		"Image preparation: %zu texture(s), %zu decoded on workers, %u worker(s), %.1f ms.\n",
		s_imageLoadJobs.size(), workerDecoded, s_imageLoadThreadCount, elapsedMilliseconds);
	s_imageLoadJobs.clear();
	s_imageWorkQueue.clear();
}

static image_t* R_QueueImageLoad(std::string_view name, const imgFlags_t flags)
{
	R_UploadFinishedImageJobs();

	auto job = std::make_unique<ImageLoadJob>();
	if (!R_PrepareImageJobInput(name, *job))
		return nullptr;

	job->image = R_AllocateImage(name, to_str_view(job->fileName), 0, 0, flags);
	job->flags = job->image->flags;
	job->settings = R_GetImagePrepareSettings();
	image_t* image = job->image;

	{
		std::lock_guard lock(s_imageLoadMutex);
		s_imageWorkQueue.push_back(job.get());
		s_imageLoadJobs.push_back(std::move(job));
		++s_pendingImageJobs;
	}
	s_imageLoadCv.notify_one();
	return image;
}

/*
===============
R_FindImageFile

Finds or loads the given image.
Returns NULL if it fails, not a default image.
==============
*/
image_t* R_FindImageFile(std::string_view name, imgFlags_t flags)
{
	image_t* image;
	std::array<char, MAX_QPATH> strippedName;
	std::array<char, MAX_QPATH> nameWithExt;
	int width, height;
	byte* pic;

	// ri.Printf(PRINT_ALL, "name %s \n", name.data());

	if (name.empty())
	{
		return NULL;
	}

	//
	// see if the image is already loaded
	//
	if ((image = R_FindCachedImage(name, flags)) != nullptr)
	{
		return image;
	}

	if (strrchr_sv(name, '.'))
	{
		// try with stripped extension
		COM_StripExtension_cpp(name, strippedName);
		if ((image = R_FindCachedImage(to_str_view(strippedName), flags)) != nullptr)
		{
			return image;
		}
	}
	else
	{
		for (int i = 0; i < numImageLoaders; ++i)
		{
			Com_sprintf(nameWithExt.data(), nameWithExt.size(), "%s.%s", name.data(), imageLoaders[i].ext);
			if ((image = R_FindCachedImage(to_str_view(nameWithExt), flags)) != nullptr)
			{
				return image;
			}
		}
	}

	if (s_imageLoadQueueActive)
		return R_QueueImageLoad(name, flags);

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
		byte* img;
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
	byte* data;
	float d;

	data = static_cast<byte*>(ri.Hunk_AllocateTempMemory(FOG_S * FOG_T * 4));

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

// make a centered inverse-square falloff blob for dynamic lighting
static consteval std::array<byte, DLIGHT_SIZE* DLIGHT_SIZE * 4> MakeDlightBlobRGBA() {
	std::array<byte, DLIGHT_SIZE* DLIGHT_SIZE * 4> out{};

	for (int y = 0; y < DLIGHT_SIZE; ++y) {
		for (int x = 0; x < DLIGHT_SIZE; ++x) {
			// denom == 4*d from the original float code (exact for DLIGHT_SIZE=16)
			const int ox = (DLIGHT_SIZE - 1) - 2 * x; // 15,13,...,-15
			const int oy = (DLIGHT_SIZE - 1) - 2 * y;
			const int denom = ox * ox + oy * oy;      // minimum is 2, never 0

			int b = 16000 / denom;                   // == int(4000 / d) for this grid

			if (b > 255)       b = 255;
			else if (b < 75)   b = 0;

			const std::size_t base = (static_cast<std::size_t>(y) * DLIGHT_SIZE + static_cast<std::size_t>(x)) * 4u;
			out[base + 0] = static_cast<byte>(b);
			out[base + 1] = static_cast<byte>(b);
			out[base + 2] = static_cast<byte>(b);
			out[base + 3] = 255;
		}
	}

	return out;
}

static constexpr auto kDlightBlobRGBA = MakeDlightBlobRGBA();

static void R_CreateDlightImage(void)
{
	tr.dlightImage = R_CreateImage("*dlight", {}, (byte*)kDlightBlobRGBA.data(), DLIGHT_SIZE, DLIGHT_SIZE, imgFlags_t::IMGFLAG_CLAMPTOEDGE);
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
static bool R_BuildDefaultImage(const char* format)
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

	tr.defaultImage = R_CreateImage("*default", {}, (byte*)data, DEFAULT_SIZE, DEFAULT_SIZE, imgFlags_t::IMGFLAG_MIPMAP);

	return true;
}

static consteval std::array<byte, DEFAULT_SIZE* DEFAULT_SIZE * 4> MakeDefaultImageData() {
	std::array<byte, DEFAULT_SIZE* DEFAULT_SIZE * 4> out{};
	out.fill(32); // same as Com_Memset(data, 32, sizeof(data))

	auto set_px = [&](int y, int x, byte v) consteval {
		const std::size_t base = (static_cast<std::size_t>(y) * DEFAULT_SIZE + static_cast<std::size_t>(x)) * 4u;
		out[base + 0] = v;
		out[base + 1] = v;
		out[base + 2] = v;
		out[base + 3] = v;
		};

	for (int x = 0; x < DEFAULT_SIZE; ++x) {
		set_px(0, x, 255);                // top
		set_px(x, 0, 255);                // left
		set_px(DEFAULT_SIZE - 1, x, 255);                // bottom
		set_px(x, DEFAULT_SIZE - 1, 255);                // right
	}

	return out;
}
static constexpr auto kDefaultImageData = MakeDefaultImageData();

static void R_CreateDefaultImage(void)
{
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

	tr.defaultImage = R_CreateImage("*default", {}, (byte*)kDefaultImageData.data(), DEFAULT_SIZE, DEFAULT_SIZE, imgFlags_t::IMGFLAG_MIPMAP);
}

static void R_CreateBuiltinImages(void)
{
	int x, y;
	byte data[DEFAULT_SIZE][DEFAULT_SIZE][4];

	R_CreateDefaultImage();

	Com_Memset(data, 0, sizeof(data));
	tr.blackImage = R_CreateImage("*black", {}, (byte*)data, 8, 8, imgFlags_t::IMGFLAG_NONE);

	// we use a solid white image instead of disabling texturing
	Com_Memset(data, 255, sizeof(data));
	tr.whiteImage = R_CreateImage("*white", {}, (byte*)data, 8, 8, imgFlags_t::IMGFLAG_NONE);

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

	tr.identityLightImage = R_CreateImage("*identityLight", {}, (byte*)data, 8, 8, imgFlags_t::IMGFLAG_NONE);

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
	R_FinishParallelImageLoads();

	if (tr.numImages == 0) {
		return;
	}


	int i;

	vk_wait_idle();

	for (i = 0; i < tr.numImages; i++)
	{
		image_t* img = tr.images[i];
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
static const char* CommaParse(const char** data_p)
{
	int c, len;
	const char* data;
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

qhandle_t RE_RegisterSkin(const char* name)
{
	R_SyncRenderThread();
	skinSurface_t parseSurfaces[MAX_SKIN_SURFACES]{};
	qhandle_t hSkin;
	skin_t* skin;
	skinSurface_t* surf;
	union
	{
		char* c;
		void* v;
	} text{};
	const char* text_p;
	const char* token;
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
	skin = reinterpret_cast<skin_t*>(ri.Hunk_Alloc(sizeof(skin_t), h_low));
	tr.skins[hSkin] = skin;
	Q_strncpyz(skin->name, name, sizeof(skin->name));
	skin->numSurfaces = 0;

	// If not a .skin file, load as a single shader
	if (strcmp(name + strlen(name) - 5, ".skin"))
	{
		skin->numSurfaces = 1;
		skin->surfaces = reinterpret_cast<skinSurface_t*>(ri.Hunk_Alloc(sizeof(skinSurface_t), h_low));
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
	skin->surfaces = reinterpret_cast<skinSurface_t*>(ri.Hunk_Alloc(skin->numSurfaces * sizeof(skinSurface_t), h_low));
	memcpy(skin->surfaces, parseSurfaces, skin->numSurfaces * sizeof(skinSurface_t));

	return hSkin;
}

void R_InitSkins(void)
{
	skin_t* skin;

	tr.numSkins = 1;

	// make the default skin have all default shaders
	skin = tr.skins[0] = reinterpret_cast<skin_t*>(ri.Hunk_Alloc(sizeof(skin_t), h_low));
	Q_strncpyz(skin->name, "<default skin>", sizeof(skin->name));
	skin->numSurfaces = 1;
	skin->surfaces = reinterpret_cast<skinSurface_t*>(ri.Hunk_Alloc(sizeof(skinSurface_t), h_low));
	skin->surfaces[0].shader = tr.defaultShader;
}

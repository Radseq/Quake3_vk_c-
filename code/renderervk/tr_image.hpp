#ifndef TR_IMAGE_HPP
#define TR_IMAGE_HPP

#include "tr_local.hpp"

/*
================
return a hash value for the filename
================
*/

typedef struct
{
	std::string_view name;
	glCompat minimize;
	glCompat maximize;
} textureMode_t;

typedef struct
{
	byte *buffer;
	int buffer_size;
	int mip_levels;
	int base_level_width;
	int base_level_height;
} Image_Upload_Data;

typedef struct
{
	const char *ext;
	void (*ImageLoader)(const char *, unsigned char **, int *, int *);
} imageExtToLoaderMap_t;

constexpr int DLIGHT_SIZE = 16;
constexpr int FOG_S = 256;
constexpr int FOG_T = 32;
constexpr int DEFAULT_SIZE = 16;

skin_t *R_GetSkinByHandle(qhandle_t hSkin);

static ID_INLINE uint32_t R_SkinSurfaceNameHash(const char *const name) noexcept
{
	uint32_t hash = 2166136261u;
	for (const auto *p = reinterpret_cast<const unsigned char *>(name); *p; ++p)
	{
		hash ^= *p;
		hash *= 16777619u;
	}
	return hash;
}

static ID_INLINE shader_t *R_FindSkinSurfaceShader(const skin_t &skin, const char *const surfaceName) noexcept
{
	if (!surfaceName || !surfaceName[0])
		return nullptr;

	if (!skin.surfaceHashTable)
	{
		for (int i = 0; i < skin.numSurfaces; ++i)
		{
			if (strcmp(skin.surfaces[i].name, surfaceName) == 0)
				return skin.surfaces[i].shader;
		}
		return nullptr;
	}

	const uint32_t hash = R_SkinSurfaceNameHash(surfaceName);
	uint32_t slot = hash & skin.surfaceHashMask;

	for (uint32_t probe = 0; probe <= skin.surfaceHashMask; ++probe)
	{
		const uint16_t entry = skin.surfaceHashTable[slot];
		if (entry == 0)
			return nullptr;

		const skinSurface_t &surface = skin.surfaces[entry - 1u];
		if (strcmp(surface.name, surfaceName) == 0)
			return surface.shader;

		slot = (slot + 1u) & skin.surfaceHashMask;
	}

	return nullptr;
}
int R_SumOfUsedImages(int frameCount);
inline int R_SumOfUsedImages() { return R_SumOfUsedImages(tr.frameCount); }
void R_InitFogTable();
float R_FogFactor(float s, float t);

void R_SkinList_f();
void R_GammaCorrect(byte *buffer, const int bufSize);
void TextureMode(std::string_view sv_mode);
void R_ImageList_f(void);
image_t *R_CreateImage(std::string_view name, std::string_view name2, byte *pic, int width, int height, imgFlags_t flags);
image_t *R_FindImageFile(std::string_view name, imgFlags_t flags);
void R_BeginParallelImageLoads();
void R_FinishParallelImageLoads();
void R_SetColorMappings(void);
void R_InitImages(void);
void R_DeleteTextures(void);
void R_InitSkins(void);
qhandle_t RE_RegisterSkin(const char *name);

#endif // TR_IMAGE_HPP

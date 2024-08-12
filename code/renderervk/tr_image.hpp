#ifndef TR_IMAGE_HPP
#define TR_IMAGE_HPP

#include "tr_local.hpp"
#include <string_view>

/*
================
return a hash value for the filename
================
*/

typedef struct
{
	std::string_view name;
	GLint minimize, maximize;
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
	std::string_view ext;
	void (*ImageLoader)(const char *, unsigned char **, int *, int *);
} imageExtToLoaderMap_t;

constexpr int DLIGHT_SIZE = 16;
constexpr int FOG_S = 256;
constexpr int FOG_T = 32;
constexpr int DEFAULT_SIZE = 16;

skin_t *R_GetSkinByHandle(qhandle_t hSkin);
int R_SumOfUsedImages();
void R_InitFogTable();
float R_FogFactor(float s, float t);

void R_SkinList_f();
void R_GammaCorrect(byte *buffer, int bufSize);
void TextureMode(std::string_view texture);
void R_ImageList_f(void);
image_t *R_CreateImage(const char *name, const char *name2, byte *pic, int width, int height, imgFlags_t flags);
image_t *R_FindImageFile(const char *name, imgFlags_t flags);
void R_SetColorMappings(void);
void R_InitImages(void);
void R_DeleteTextures(void);
void R_InitSkins(void);
qhandle_t RE_RegisterSkin(const char *name);

#endif // TR_IMAGE_HPP

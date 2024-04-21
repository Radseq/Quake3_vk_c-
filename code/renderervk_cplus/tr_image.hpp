#ifndef TR_IMAGE_HPP
#define TR_IMAGE_HPP

#include "q_shared.hpp"

#ifdef __cplusplus
extern "C"
{
#endif

#include "../renderervk/tr_local.h"
#include "tr_image_bmp.hpp"
#include "tr_image_jpg.hpp"
#include "tr_image_png.hpp"
#include "tr_image_tga.hpp"
#include "tr_image_pcx.hpp"

	static byte s_intensitytable[256];
	static unsigned char s_gammatable[256];
	static unsigned char s_gammatable_linear[256];

#define FILE_HASH_SIZE 1024
	static image_t *hashTable[FILE_HASH_SIZE];

/*
================
return a hash value for the filename
================
*/
#define generateHashValue(fname) Com_GenerateHashValue((fname), FILE_HASH_SIZE)

	typedef struct
	{
		const char *name;
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
		const char *ext;
		void (*ImageLoader)(const char *, unsigned char **, int *, int *);
	} imageExtToLoaderMap_t;

#define DLIGHT_SIZE 16
#define FOG_S 256
#define FOG_T 32
#define DEFAULT_SIZE 16

	skin_t *R_GetSkinByHandle_plus(qhandle_t hSkin);
	int R_SumOfUsedImages_plus();
	void R_InitFogTable_plus();
	float R_FogFactor_plus(float s, float t);

	void R_SkinList_f_plus();
	void R_GammaCorrect_plus(byte *buffer, int bufSize);
	void TextureMode_plus(const char *string);
	void R_ImageList_f_plus(void);
	image_t *R_CreateImage_plus(const char *name, const char *name2, byte *pic, int width, int height, imgFlags_t flags);
	image_t *R_FindImageFile_plus(const char *name, imgFlags_t flags);
	void R_SetColorMappings_plus(void);
	void R_InitImages_plus(void);
	void R_DeleteTextures_plus(void);
	void R_InitSkins_plus(void);
	qhandle_t RE_RegisterSkin_plus(const char *name);

#ifdef __cplusplus
}
#endif

#endif // TR_IMAGE_HPP

#ifndef TR_FONT_HPP
#define TR_FONT_HPP

#include "q_shared.hpp"
#include "../renderervk/tr_public.h"

#ifdef __cplusplus
extern "C"
{
#endif

	extern void R_IssuePendingRenderCommands(void);
	extern qhandle_t RE_RegisterShaderNoMip(const char *name);

#ifdef BUILD_FREETYPE
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_ERRORS_H
#include FT_SYSTEM_H
#include FT_IMAGE_H
#include FT_OUTLINE_H

#define _FLOOR(x) ((x) & -64)
#define _CEIL(x) (((x) + 63) & -64)
#define _TRUNC(x) ((x) >> 6)

	FT_Library ftLibrary = NULL;
#endif

#define MAX_FONTS 6

	static int registeredFontCount = 0;
	static fontInfo_t registeredFont[MAX_FONTS];

	static int fdOffset;
	static byte *fdFile;

	typedef union
	{
		byte fred[4];
		float ffred;
	} poor;

	//void RE_RegisterFont_plus(const char *fontName, int pointSize, fontInfo_t *font);
	void R_InitFreeType_plus();
	void R_DoneFreeType_plus();

#ifdef __cplusplus
}
#endif

#endif // TR_FONT_HPP

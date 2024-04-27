#ifndef TR_FONT_HPP
#define TR_FONT_HPP


#ifdef __cplusplus
extern "C"
{
#endif

#include "tr_public.hpp"

	extern void R_IssuePendingRenderCommands(void);
	extern qhandle_t RE_RegisterShaderNoMip(const char *name);

	void RE_RegisterFont(const char *fontName, int pointSize, fontInfo_t *font);
	void R_InitFreeType();
	void R_DoneFreeType();
	//float readFloat(void);

#ifdef __cplusplus
}
#endif

#endif // TR_FONT_HPP

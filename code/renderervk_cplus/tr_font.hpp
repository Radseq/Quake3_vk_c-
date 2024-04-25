#ifndef TR_FONT_HPP
#define TR_FONT_HPP

#include "q_shared.hpp"

#ifdef __cplusplus
extern "C"
{
#endif

#include "tr_public.hpp"

	extern void R_IssuePendingRenderCommands(void);
	extern qhandle_t RE_RegisterShaderNoMip_plus(const char *name);

	void RE_RegisterFont_plus(const char *fontName, int pointSize, fontInfo_t *font);
	void R_InitFreeType_plus();
	void R_DoneFreeType_plus();
	//float readFloat_plus(void);

#ifdef __cplusplus
}
#endif

#endif // TR_FONT_HPP

#ifndef TR_FONT_HPP
#define TR_FONT_HPP

#include "tr_public.hpp"

extern void R_IssuePendingRenderCommands(void);

void RE_RegisterFont(const char *fontName, int pointSize, fontInfo_t *font);
void R_InitFreeType();
void R_DoneFreeType();
// float readFloat(void);

#endif // TR_FONT_HPP

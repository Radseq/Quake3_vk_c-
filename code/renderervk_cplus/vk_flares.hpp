#ifndef VK_FLARES_HPP
#define VK_FLARES_HPP

#include "tr_local.hpp"

void R_ClearFlares(void);
void RB_AddFlare(void *surface, int fogNum, vec3_t point, vec3_t color, vec3_t normal);
void RB_AddDlightFlares(void);
void RB_RenderFlares(void);

#endif // VK_FLARES_HPP

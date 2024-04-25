#ifndef VK_FLARES_HPP
#define VK_FLARES_HPP

#include "q_math.hpp"
#include "tr_main.hpp"
#include "tr_light.hpp"
#include "q_shared.hpp"

void R_ClearFlares_plus(void);
void RB_AddFlare_plus(void *surface, int fogNum, vec3_t point, vec3_t color, vec3_t normal);
void RB_AddDlightFlares_plus(void);
void RB_RenderFlares_plus(void);

#endif // VK_FLARES_HPP

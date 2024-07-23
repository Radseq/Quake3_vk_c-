#ifndef TR_SKY_HPP
#define TR_SKY_HPP

#include "tr_local.hpp"

void R_InitSkyTexCoords(float heightCloud);
void RB_DrawSun(float scale, shader_t &shader);
void RB_StageIteratorSky(void);

#endif // TR_SKY_HPP
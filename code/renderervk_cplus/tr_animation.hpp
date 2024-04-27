#ifndef TR_ANIMATION_HPP
#define TR_ANIMATION_HPP

#include "tr_light.hpp"
#include "tr_main.hpp"
#include "tr_local.hpp"

void R_MDRAddAnimSurfaces(trRefEntity_t *ent);
void RB_MDRSurfaceAnim(mdrSurface_t *surface);
void MC_UnCompress(float mat[3][4], const unsigned char *comp);

#endif // TR_ANIMATION_HPP

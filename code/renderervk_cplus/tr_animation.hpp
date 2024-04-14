#ifndef TR_ANIMATION_HPP
#define TR_ANIMATION_HPP

#ifdef __cplusplus
extern "C"
{
#endif

#include "../renderervk/tr_local.h"
#include "tr_light.hpp"
#include "tr_main.hpp"

    void R_MDRAddAnimSurfaces_plus(trRefEntity_t *ent);
    void RB_MDRSurfaceAnim_plus(mdrSurface_t *surface);
    void MC_UnCompress_plus(float mat[3][4], const unsigned char *comp);

#ifdef __cplusplus
}
#endif

#endif // TR_ANIMATION_HPP

#ifndef TR_WORLD_HPP
#define TR_WORLD_HPP

#include "../renderervk/tr_local.h"
#include "q_math.hpp"
#include "tr_main.hpp"
#include "tr_light.hpp"

#ifdef __cplusplus
extern "C"
{
#endif

      //void R_AddWorldSurface_plus(msurface_t *surf, int dlightBits);
      void R_AddWorldSurfaces_plus();

      //void R_AddBrushModelSurfaces_plus(trRefEntity_t *ent);

#ifdef __cplusplus
}
#endif

#endif // TR_WORLD_HPP

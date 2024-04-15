#ifndef TR_WORLD_HPP
#define TR_WORLD_HPP

#include "q_math.hpp"
#include "tr_main.hpp"
#include "tr_light.hpp"
#include "tr_model.hpp"

#ifdef __cplusplus
extern "C"
{
#endif

#include "../renderervk/tr_local.h"

      void R_AddWorldSurface_plus(msurface_t *surf, int dlightBits);
      void R_AddWorldSurfaces_plus();

      void R_AddBrushModelSurfaces_plus(trRefEntity_t *ent);
      bool R_inPVS_plus(const vec3_t p1, const vec3_t p2);

#ifdef USE_PMLIGHT
      bool R_LightCullBounds_plus(const dlight_t *dl, const vec3_t mins, const vec3_t maxs);
#endif // USE_PMLIGHT

#ifdef __cplusplus
}
#endif

#endif // TR_WORLD_HPP

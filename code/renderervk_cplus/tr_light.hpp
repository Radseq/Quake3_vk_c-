#ifndef TR_LIGHT_HPP
#define TR_LIGHT_HPP

#ifdef __cplusplus
extern "C"
{
#endif

#include "../qcommon/q_shared.h"
#include "q_math.hpp"
#include "tr_local.hpp"

    void R_TransformDlights_plus(int count, dlight_t *dl, orientationr_t *ort);
#ifdef USE_LEGACY_DLIGHTS
    void R_DlightBmodel_plus(bmodel_t *bmodel);
#endif // USE_LEGACY_DLIGHTS

    int R_LightForPoint_plus(vec3_t point, vec3_t ambientLight, vec3_t directedLight, vec3_t lightDir);
    void R_SetupEntityLighting_plus(const trRefdef_t *refdef, trRefEntity_t *ent);

#ifdef __cplusplus
}
#endif

#endif // TR_LIGHT_HPP
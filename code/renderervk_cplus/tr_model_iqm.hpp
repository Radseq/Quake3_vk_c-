#ifndef TR_MODEL_IQM_HPP
#define TR_MODEL_IQM_HPP

#include "../renderervk/tr_local.h"
#include "tr_main.hpp"
#include "tr_shader.hpp"
#include "tr_light.hpp"
#include "tr_image.hpp"

#ifdef __cplusplus
extern "C"
{
#endif

#define LL(x) x = LittleLong(x)

    // 3x4 identity matrix
    static float identityMatrix[12] = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0};

    void R_AddIQMSurfaces_plus(trRefEntity_t *ent);

#ifdef __cplusplus
}
#endif

#endif // TR_MODEL_IQM_HPP
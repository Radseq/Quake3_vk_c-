#ifndef TR_IMAGE_HPP
#define TR_IMAGE_HPP

#include "../renderervk/tr_local.h"
#include "q_shared.hpp"

#ifdef __cplusplus
extern "C"
{
#endif

    skin_t *R_GetSkinByHandle_plus(qhandle_t hSkin);
    int R_SumOfUsedImages_plus();
    void R_InitFogTable_plus();
    float R_FogFactor_plus(float s, float t);

    void R_SetColorMappings_plus();
    void R_SkinList_f_plus();

#ifdef __cplusplus
}
#endif

#endif // TR_IMAGE_HPP

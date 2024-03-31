#ifndef TR_MODEL_HPP
#define TR_MODEL_HPP

#include "../renderervk/tr_local.h"
#include "q_shared.hpp"
#include "tr_model_iqm.hpp"

#ifdef __cplusplus
extern "C"
{
#endif

    model_t *R_GetModelByHandle_plus(qhandle_t index);
    void R_ModelBounds_plus(qhandle_t handle, vec3_t mins, vec3_t maxs);
    void R_ModelInit_plus();
    void R_Modellist_f_plus();
    // todo not working???
    model_t *R_AllocModel_plus();
    int R_LerpTag_plus(orientation_t *tag, qhandle_t handle, int startFrame, int endFrame,
                       float frac, const char *tagName);

#ifdef __cplusplus
}
#endif

#endif // TR_MODEL_HPP
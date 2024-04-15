#ifndef TR_MODEL_HPP
#define TR_MODEL_HPP

#include "q_shared.hpp"
#include "tr_model_iqm.hpp"

#ifdef __cplusplus
extern "C"
{
#endif

#include "../renderervk/tr_local.h"



    model_t *R_GetModelByHandle_plus(qhandle_t index);
    void R_ModelBounds_plus(qhandle_t handle, vec3_t mins, vec3_t maxs);
    void R_ModelInit_plus();
    void R_Modellist_f_plus();

    model_t *R_AllocModel_plus();
    qhandle_t RE_RegisterModel_plus(const char *name);
    int R_LerpTag_plus(orientation_t *tag, qhandle_t handle, int startFrame, int endFrame,
                       float frac, const char *tagName);
    void RE_BeginRegistration_plus(glconfig_t *glconfigOut);

#ifdef __cplusplus
}
#endif

#endif // TR_MODEL_HPP
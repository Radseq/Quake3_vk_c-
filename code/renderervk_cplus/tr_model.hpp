#ifndef TR_MODEL_HPP
#define TR_MODEL_HPP


#include "tr_model_iqm.hpp"

#include "tr_local.hpp"

model_t *R_GetModelByHandle(qhandle_t index);
void R_ModelBounds(qhandle_t handle, vec3_t mins, vec3_t maxs);
void R_ModelInit();
void R_Modellist_f();

model_t *R_AllocModel();
qhandle_t RE_RegisterModel(const char *name);
int R_LerpTag(orientation_t *tag, qhandle_t handle, int startFrame, int endFrame,
                   float frac, const char *tagName);
void RE_BeginRegistration(glconfig_t *glconfigOut);

#endif // TR_MODEL_HPP
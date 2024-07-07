#ifndef TR_MODEL_IQM_HPP
#define TR_MODEL_IQM_HPP

#include "tr_local.hpp"

void R_AddIQMSurfaces(trRefEntity_t &ent);
int R_IQMLerpTag(orientation_t *tag, iqmData_t *data,
                 int startFrame, int endFrame,
                 float frac, const char *tagName);

void RB_IQMSurfaceAnim(const surfaceType_t *surface);
bool R_LoadIQM(model_t *mod, void *buffer, int filesize, const char *mod_name);

#endif // TR_MODEL_IQM_HPP
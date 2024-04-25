#ifndef TR_MODEL_IQM_HPP
#define TR_MODEL_IQM_HPP

#include "tr_main.hpp"
#include "tr_shader.hpp"
#include "tr_light.hpp"
#include "tr_image.hpp"

#include "tr_local.hpp"

void R_AddIQMSurfaces_plus(trRefEntity_t *ent);
int R_IQMLerpTag_plus(orientation_t *tag, iqmData_t *data,
                      int startFrame, int endFrame,
                      float frac, const char *tagName);

void RB_IQMSurfaceAnim_plus(const surfaceType_t *surface);
bool R_LoadIQM_plus(model_t *mod, void *buffer, int filesize, const char *mod_name);

#endif // TR_MODEL_IQM_HPP
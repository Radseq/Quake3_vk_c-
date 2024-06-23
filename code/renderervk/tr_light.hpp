#ifndef TR_LIGHT_HPP
#define TR_LIGHT_HPP

#include "tr_local.hpp"

void R_TransformDlights(int count, dlight_t *dl, orientationr_t *ort);
#ifdef USE_LEGACY_DLIGHTS
void R_DlightBmodel(bmodel_t *bmodel);
#endif // USE_LEGACY_DLIGHTS

int R_LightForPoint(vec3_t point, vec3_t ambientLight, vec3_t directedLight, vec3_t lightDir);
void R_SetupEntityLighting(const trRefdef_t *refdef, trRefEntity_t *ent);

#endif // TR_LIGHT_HPP
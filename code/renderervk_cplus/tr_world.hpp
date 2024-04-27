#ifndef TR_WORLD_HPP
#define TR_WORLD_HPP

#include "tr_main.hpp"
#include "tr_light.hpp"
#include "tr_model.hpp"

#include "tr_local.hpp"

void R_AddWorldSurfaces();

void R_AddBrushModelSurfaces(trRefEntity_t *ent);
bool R_inPVS(const vec3_t p1, const vec3_t p2);

#ifdef USE_PMLIGHT
bool R_LightCullBounds(const dlight_t *dl, const vec3_t mins, const vec3_t maxs);
#endif // USE_PMLIGHT

#endif // TR_WORLD_HPP

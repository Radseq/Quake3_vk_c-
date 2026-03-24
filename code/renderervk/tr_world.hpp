#ifndef TR_WORLD_HPP
#define TR_WORLD_HPP

#include "tr_local.hpp"

void R_AddWorldSurfaces();

void R_AddBrushModelSurfaces(trRefEntity_t& ent);
bool R_inPVS(const vec3_t p1, const vec3_t p2);

#ifdef USE_PMLIGHT
bool R_LightCullBounds(const dlight_t& dl, const vec3_t& mins, const vec3_t& maxs);
#endif // USE_PMLIGHT

ID_INLINE std::uint32_t& R_SurfaceViewCount(msurface_t& surf) noexcept;

#ifdef USE_PMLIGHT
ID_INLINE std::uint32_t& R_SurfaceVisibleCount(msurface_t& surf) noexcept;
ID_INLINE std::uint32_t& R_SurfaceLightCount(msurface_t& surf) noexcept;
#endif


void R_EnsureSurfaceRuntimeState();

#endif // TR_WORLD_HPP

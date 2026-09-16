#ifndef TR_WORLD_HPP
#define TR_WORLD_HPP

#include "tr_local.hpp"

void R_AddWorldSurfaces();

void R_AddBrushModelSurfaces(trRefEntity_t& ent);
bool R_inPVS(const vec3_t p1, const vec3_t p2);

#ifdef USE_PMLIGHT
bool R_LightCullBounds(const dlight_t& dl, const vec3_t& mins, const vec3_t& maxs);
#endif // USE_PMLIGHT

extern std::uint32_t *tr_surfaceViewCounts;
#ifdef USE_PMLIGHT
extern std::uint32_t *tr_surfaceVisibleCounts;
extern std::uint32_t *tr_surfaceLightCounts;
#endif

ID_INLINE std::size_t R_SurfaceRuntimeIndex(const msurface_t& surf) noexcept
{
	return static_cast<std::size_t>(&surf - tr.world->surfaces);
}

ID_INLINE std::uint32_t& R_SurfaceViewCount(msurface_t& surf) noexcept
{
	return tr_surfaceViewCounts[R_SurfaceRuntimeIndex(surf)];
}

#ifdef USE_PMLIGHT
ID_INLINE std::uint32_t& R_SurfaceVisibleCount(msurface_t& surf) noexcept
{
	return tr_surfaceVisibleCounts[R_SurfaceRuntimeIndex(surf)];
}

ID_INLINE std::uint32_t& R_SurfaceLightCount(msurface_t& surf) noexcept
{
	return tr_surfaceLightCounts[R_SurfaceRuntimeIndex(surf)];
}
#endif


void R_EnsureSurfaceRuntimeState();

#endif // TR_WORLD_HPP

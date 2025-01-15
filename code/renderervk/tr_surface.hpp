#ifndef TR_SURFACE_HPP
#define TR_SURFACE_HPP

#include "tr_local.hpp"

void RB_CheckOverflow(const int verts, const int indexes);
#define RB_CHECKOVERFLOW(v, i) RB_CheckOverflow(v, i)

void RB_SurfaceGridEstimate(srfGridMesh_t &cv, int *numVertexes, int *numIndexes);

void RB_AddQuadStampExt(const vec3_t& origin, const vec3_t& left, const vec3_t& up, const color4ub_t& color,
	const float s1, const float t1, const float s2, const float t2);
void RB_AddQuadStamp2(float x, float y, float w, float h, float s1, float t1, float s2, float t2, color4ub_t color);
void RB_AddQuadStamp(const vec3_t &origin, const vec3_t &left, const vec3_t &up, const color4ub_t &color);

#endif // TR_SURFACE_HPP
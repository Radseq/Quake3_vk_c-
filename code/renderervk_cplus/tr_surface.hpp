#ifndef TR_SURFACE_HPP
#define TR_SURFACE_HPP

#include "../renderervk/tr_local.h"

#ifdef __cplusplus
extern "C"
{
#endif

    void RB_CheckOverflow_plus(int verts, int indexes);
    void RB_AddQuadStampExt_plus(const vec3_t origin, const vec3_t left, const vec3_t up, color4ub_t color, float s1, float t1, float s2, float t2);
    void RB_AddQuadStamp2_plus(float x, float y, float w, float h, float s1, float t1, float s2, float t2, color4ub_t color);
    void RB_AddQuadStamp_plus(const vec3_t origin, const vec3_t left, const vec3_t up, color4ub_t color);
    void RB_SurfaceGridEstimate_plus(srfGridMesh_t *cv, int *numVertexes, int *numIndexes);



#ifdef __cplusplus
}
#endif

#endif // TR_SURFACE_HPP
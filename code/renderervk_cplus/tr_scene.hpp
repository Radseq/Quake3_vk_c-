#ifndef TR_SCENE_HPP
#define TR_SCENE_HPP

#ifdef __cplusplus
extern "C"
{
#endif

#include "tr_local.hpp"

    void R_InitNextFrame_plus();
    void RE_ClearScene_plus();
    void R_AddPolygonSurfaces_plus();
    void RE_AddPolyToScene_plus(qhandle_t hShader, int numVerts, const polyVert_t *verts, int numPolys);
    void RE_AddRefEntityToScene_plus(const refEntity_t *ent, bool intShaderTime);
    void RE_AddLinearLightToScene_plus(const vec3_t start, const vec3_t end, float intensity, float r, float g, float b);
    void RE_AddLightToScene_plus(const vec3_t org, float intensity, float r, float g, float b);
    void RE_AddAdditiveLightToScene_plus(const vec3_t org, float intensity, float r, float g, float b);

    void *R_GetCommandBuffer_plus(int bytes);

    void RE_RenderScene_plus(const refdef_t *fd);

#ifdef __cplusplus
}
#endif

#endif // TR_SCENE_HPP
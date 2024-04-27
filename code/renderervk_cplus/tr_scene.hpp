#ifndef TR_SCENE_HPP
#define TR_SCENE_HPP

#ifdef __cplusplus
extern "C"
{
#endif

#include "tr_local.hpp"

    void R_InitNextFrame();
    void RE_ClearScene();
    void R_AddPolygonSurfaces();
    void RE_AddPolyToScene(qhandle_t hShader, int numVerts, const polyVert_t *verts, int numPolys);
    void RE_AddRefEntityToScene(const refEntity_t *ent, bool intShaderTime);
    void RE_AddLinearLightToScene(const vec3_t start, const vec3_t end, float intensity, float r, float g, float b);
    void RE_AddLightToScene(const vec3_t org, float intensity, float r, float g, float b);
    void RE_AddAdditiveLightToScene(const vec3_t org, float intensity, float r, float g, float b);

    void *R_GetCommandBuffer(int bytes);

    void RE_RenderScene(const refdef_t *fd);

#ifdef __cplusplus
}
#endif

#endif // TR_SCENE_HPP
#ifndef TR_SHADE_HPP
#define TR_SHADE_HPP

#ifdef __cplusplus
extern "C"
{
#endif
#include "../renderervk/tr_local.h"

    void RB_BeginSurface_plus(shader_t *shader, int fogNum);
    void R_ComputeTexCoords_plus(const int b, const textureBundle_t *bundle);
    void VK_SetFogParams_plus(vkUniform_t *uniform, int *fogStage);
    void R_ComputeColors_plus(const int b, color4ub_t *dest, const shaderStage_t *pStage);

#ifdef __cplusplus
}
#endif

#endif // TR_SHADE_HPP
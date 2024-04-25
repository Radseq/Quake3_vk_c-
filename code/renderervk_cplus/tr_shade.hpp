#ifndef TR_SHADE_HPP
#define TR_SHADE_HPP

#ifdef __cplusplus
extern "C"
{
#endif
#include "tr_local.hpp"

    void RB_BeginSurface_plus(shader_t *shader, int fogNum);
    void R_ComputeTexCoords_plus(const int b, const textureBundle_t *bundle);
    void VK_SetFogParams_plus(vkUniform_t *uniform, int *fogStage);
    void R_ComputeColors_plus(const int b, color4ub_t *dest, const shaderStage_t *pStage);
    uint32_t VK_PushUniform_plus(const vkUniform_t *uniform);
#ifdef USE_PMLIGHT
    void VK_LightingPass_plus(void);
#endif // USE_PMLIGHT
    void RB_StageIteratorGeneric_plus(void);
    void RB_EndSurface_plus(void);

#ifdef __cplusplus
}
#endif

#endif // TR_SHADE_HPP
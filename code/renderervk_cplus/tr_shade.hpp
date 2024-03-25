#ifndef TR_SHADE_HPP
#define TR_SHADE_HPP

#include "../renderervk/tr_local.h"

#ifdef __cplusplus
extern "C"
{
#endif

    void RB_BeginSurface_plus(shader_t *shader, int fogNum);
    void R_ComputeTexCoords_plus(const int b, const textureBundle_t *bundle);
    void VK_SetFogParams_plus(vkUniform_t *uniform, int *fogStage);

#ifdef __cplusplus
}
#endif

#endif // TR_SHADE_HPP
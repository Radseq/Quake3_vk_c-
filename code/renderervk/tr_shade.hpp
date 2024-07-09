#ifndef TR_SHADE_HPP
#define TR_SHADE_HPP

#include "tr_local.hpp"
#include "vk.hpp"

void RB_BeginSurface(shader_t *shader, int fogNum);
void R_ComputeTexCoords(const int b, const textureBundle_t &bundle);
void VK_SetFogParams(vkUniform_t &uniform, int *fogStage);
void R_ComputeColors(const int b, color4ub_t *dest, const shaderStage_t &pStage);
uint32_t VK_PushUniform(const vkUniform_t &uniform);
#ifdef USE_PMLIGHT
void VK_LightingPass(void);
#endif // USE_PMLIGHT
void RB_StageIteratorGeneric(void);
void RB_EndSurface(void);

#endif // TR_SHADE_HPP
#ifndef TR_SHADE_CALC_HPP
#define TR_SHADE_CALC_HPP

#include "../renderervk/tr_local.h"
#include "q_shared.hpp"

#ifdef __cplusplus
extern "C"
{
#endif
    void RB_CalcStretchTexCoords_plus(const waveForm_t *wf, float *src, float *dst);
    void RB_CalcTransformTexCoords_plus(const texModInfo_t *tmi, float *src, float *dst);
    void RB_CalcRotateTexCoords_plus(float degsPerSecond, float *src, float *dst);
    void RB_CalcScrollTexCoords_plus(const float scrollSpeed[2], float *src, float *dst);
    void RB_CalcScaleTexCoords_plus(const float scale[2], float *src, float *dst);
    void RB_CalcTurbulentTexCoords_plus(const waveForm_t *wf, float *src, float *dst);
    // void RB_CalcSpecularAlpha_plus(unsigned char *alphas);
    void RB_CalcFogTexCoords_plus(float *st);
    void RB_CalcEnvironmentTexCoords_plus(float *st);
    void RB_CalcEnvironmentTexCoordsFP_plus(float *st, int screenMap);
    const fogProgramParms_t *RB_CalcFogProgramParms_plus();
    uint32_t VK_PushUniform_plus( const vkUniform_t *uniform );

#ifdef __cplusplus
}
#endif

#endif // TR_SHADE_CALC_HPP
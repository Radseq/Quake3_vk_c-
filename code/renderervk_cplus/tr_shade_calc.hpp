#ifndef TR_SHADE_CALC_HPP
#define TR_SHADE_CALC_HPP

#include "../renderervk/tr_local.h"


#ifdef __cplusplus
extern "C"
{
#endif
    void RB_CalcStretchTexCoords_plus(const waveForm_t *wf, float *src, float *dst);
    void RB_CalcTransformTexCoords_plus(const texModInfo_t *tmi, float *src, float *dst);
    void RB_CalcRotateTexCoords_plus(float degsPerSecond, float *src, float *dst);
    void RB_CalcScrollTexCoords_plus(const float scrollSpeed[2], float *src, float *dst);
    void RB_CalcScaleTexCoords_plus( const float scale[2], float *src, float *dst );
    void RB_CalcTurbulentTexCoords_plus( const waveForm_t *wf, float *src, float *dst );

#ifdef __cplusplus
}
#endif

#endif // TR_SHADE_CALC_HPP
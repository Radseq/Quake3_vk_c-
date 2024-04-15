#ifndef TR_SHADE_CALC_HPP
#define TR_SHADE_CALC_HPP

#include "q_shared.hpp"
#include "tr_image.hpp"
// #include "tr_surface.hpp"
// #include "tr_shadows.hpp"

#ifdef __cplusplus
extern "C"
{
#endif

#include "../renderervk/tr_local.h"
#include "tr_noise_cplus.hpp"

#define WAVEVALUE(table, base, amplitude, phase, freq) ((base) + table[(int64_t)((((phase) + tess.shaderTime * (freq)) * FUNCTABLE_SIZE)) & FUNCTABLE_MASK] * (amplitude))

    void RB_CalcStretchTexCoords_plus(const waveForm_t *wf, float *src, float *dst);
    void RB_CalcTransformTexCoords_plus(const texModInfo_t *tmi, float *src, float *dst);
    void RB_CalcRotateTexCoords_plus(float degsPerSecond, float *src, float *dst);
    void RB_CalcScrollTexCoords_plus(const float scrollSpeed[2], float *src, float *dst);
    void RB_CalcScaleTexCoords_plus(const float scale[2], float *src, float *dst);
    void RB_CalcTurbulentTexCoords_plus(const waveForm_t *wf, float *src, float *dst);
    void RB_CalcSpecularAlpha_plus(unsigned char *alphas);
    void RB_CalcFogTexCoords_plus(float *st);
    void RB_CalcEnvironmentTexCoords_plus(float *st);
    void RB_CalcEnvironmentTexCoordsFP_plus(float *st, int screenMap);
    const fogProgramParms_t *RB_CalcFogProgramParms_plus();

    void RB_CalcWaveColor_plus(const waveForm_t *wf, unsigned char *dstColors);
    void RB_CalcColorFromEntity_plus(unsigned char *dstColors);
    void RB_CalcColorFromOneMinusEntity_plus(unsigned char *dstColors);
    void RB_CalcWaveAlpha_plus(const waveForm_t *wf, unsigned char *dstColors);
    void RB_CalcAlphaFromEntity_plus(unsigned char *dstColors);
    void RB_CalcAlphaFromOneMinusEntity_plus(unsigned char *dstColors);
    void RB_CalcModulateColorsByFog_plus(unsigned char *colors);
    void RB_CalcModulateAlphasByFog_plus(unsigned char *colors);
    void RB_CalcModulateRGBAsByFog_plus(unsigned char *colors);
    void RB_CalcDiffuseColor_plus(unsigned char *colors);
    void RB_DeformTessGeometry_plus(void);

#ifdef __cplusplus
}
#endif

#endif // TR_SHADE_CALC_HPP
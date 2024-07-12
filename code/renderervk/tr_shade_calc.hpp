#ifndef TR_SHADE_CALC_HPP
#define TR_SHADE_CALC_HPP

#include "tr_local.hpp"

void RB_CalcStretchTexCoords(const waveForm_t &wf, float *src, float *dst);
void RB_CalcTransformTexCoords(const texModInfo_t &tmi, float *src, float *dst);
void RB_CalcRotateTexCoords(float degsPerSecond, float *src, float *dst);
void RB_CalcScrollTexCoords(const float scrollSpeed[2], float *src, float *dst);
void RB_CalcScaleTexCoords(const float scale[2], float *src, float *dst);
void RB_CalcTurbulentTexCoords(const waveForm_t &wf, float *src, float *dst);
void RB_CalcSpecularAlpha(unsigned char *alphas);
void RB_CalcFogTexCoords(float *st);
void RB_CalcEnvironmentTexCoords(float *st);
void RB_CalcEnvironmentTexCoordsFP(float *st, int screenMap);
const fogProgramParms_t *RB_CalcFogProgramParms();

void RB_CalcWaveColor(const waveForm_t &wf, unsigned char *dstColors);
void RB_CalcColorFromEntity(unsigned char *dstColors);
void RB_CalcColorFromOneMinusEntity(unsigned char *dstColors);
void RB_CalcWaveAlpha(const waveForm_t &wf, unsigned char *dstColors);
void RB_CalcAlphaFromEntity(unsigned char *dstColors);
void RB_CalcAlphaFromOneMinusEntity(unsigned char *dstColors);
void RB_CalcModulateColorsByFog(unsigned char *colors);
void RB_CalcModulateAlphasByFog(unsigned char *colors);
void RB_CalcModulateRGBAsByFog(unsigned char *colors);
void RB_CalcDiffuseColor(unsigned char *colors);
void RB_DeformTessGeometry(void);

#endif // TR_SHADE_CALC_HPP
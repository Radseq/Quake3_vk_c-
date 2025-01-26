#ifndef TR_MAIN_HPP
#define TR_MAIN_HPP

#include "tr_local.hpp"

void R_AddDrawSurf(surfaceType_t &surface, shader_t &shader,
                   const int fogIndex, const int dlightMap);
int R_CullDlight(const dlight_t &dl);
int R_CullLocalBox(const vec3_t bounds[2]);

int R_CullLocalPointAndRadius(const vec3_t &pt, const float radius);

void R_TransformModelToClip(const vec3_t src, const float *modelMatrix, const float *projectionMatrix,
							vec4_t &eye, vec4_t &dst);

void R_TransformClipToWindow(const vec4_t &clip, const viewParms_t &view, vec4_t &normalized, vec4_t &window);
void myGlMultMatrix(const float *a, const float *b, float *out);
void myGlMultMatrix_SIMD(const float* a, const float* b, float* out);
void R_RotateForEntity(const trRefEntity_t &ent, const viewParms_t &viewParms,
                       orientationr_t &orient);

void R_SetupProjection(viewParms_t &dest, const float zProj, const bool computeFrustum);

int R_CullPointAndRadius(const vec3_t &pt, const float radius);
void R_LocalPointToWorld(const vec3_t &local, vec3_t &world);

void R_WorldToLocal(const vec3_t &world, vec3_t &local);

#ifdef USE_PMLIGHT

// Philip Erdelsky gets all the credit for this one...

void R_AddLitSurf(surfaceType_t &surface, shader_t &shader, const int fogIndex);

void R_DecomposeLitSort(unsigned sort, int &entityNum, shader_t **shader, int &fogNum);

#endif // USE_PMLIGHT

void R_DecomposeSort(unsigned sort, int &entityNum, shader_t **shader,
                     int &fogNum, int &dlightMap);
void R_RenderView(const viewParms_t &parms);

#endif // TR_MAIN_HPP

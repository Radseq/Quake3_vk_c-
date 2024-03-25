#ifndef TR_MAIN_HPP
#define TR_MAIN_HPP

#include "../renderervk/tr_local.h"
#include "q_math.hpp"

#ifdef __cplusplus
extern "C"
{
#endif

    static const float s_flipMatrix_plus[16] = {
        // convert from our coordinate system (looking down X)
        // to OpenGL's coordinate system (looking down -Z)
        0, 0, -1, 0,
        -1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 0, 1};

    void R_AddDrawSurf_plus(surfaceType_t *surface, shader_t *shader,
                            int fogIndex, int dlightMap);
    int R_CullDlight_plus(const dlight_t *dl);
    int R_CullLocalBox_plus(const vec3_t bounds[2]);

    int R_CullLocalPointAndRadius_plus(const vec3_t pt, float radius);

    void R_TransformModelToClip_plus(const vec3_t src, const float *modelMatrix, const float *projectionMatrix,
                                     vec4_t eye, vec4_t dst);

    void R_TransformClipToWindow_plus(const vec4_t clip, const viewParms_t *view, vec4_t normalized, vec4_t window);
    void myGlMultMatrix_plus(const float *a, const float *b, float *out);
    void R_RotateForEntity_plus(const trRefEntity_t *ent, const viewParms_t *viewParms,
                                orientationr_t *orient);

    void R_SetupProjection_plus(viewParms_t *dest, float zProj, bool computeFrustum);

#ifdef USE_PMLIGHT

    typedef struct litSurf_tape_s
    {
        struct litSurf_s *first;
        struct litSurf_s *last;
        unsigned count;
    } litSurf_tape_t;

    // Philip Erdelsky gets all the credit for this one...

    void R_AddLitSurf_plus(surfaceType_t *surface, shader_t *shader, int fogIndex);

    void R_DecomposeLitSort_plus(unsigned sort, int *entityNum, shader_t **shader, int *fogNum);

#endif // USE_PMLIGHT

    void R_DecomposeSort_plus(unsigned sort, int *entityNum, shader_t **shader,
                              int *fogNum, int *dlightMap);
    void R_RenderView_plus(const viewParms_t *parms);

#ifdef __cplusplus
}
#endif

#endif // TR_MAIN_HPP

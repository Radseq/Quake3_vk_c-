// todo this file should not be there i think

#ifndef Q_MATH_HPP
#define Q_MATH_HPP

#ifdef __cplusplus
extern "C"
{
#endif

#include "../qcommon/q_shared.h"

    void ClearBounds_plus(vec3_t mins, vec3_t maxs);
    int BoxOnPlaneSide_plus(vec3_t emins, vec3_t emaxs, struct cplane_s *p);
    void SetPlaneSignbits_plus(cplane_t *out);
    bool PlaneFromPoints_plus(vec4_t plane, const vec3_t a, const vec3_t b, const vec3_t c);
    void RotatePointAroundVector_plus(vec3_t dst, const vec3_t dir, const vec3_t point,
                                      float degrees);
    vec_t VectorNormalize_plus(vec3_t v);
    void PerpendicularVector_plus(vec3_t dst, const vec3_t src);

    void MatrixMultiply_plus(float in1[3][3], float in2[3][3], float out[3][3]);
    void ProjectPointOnPlane_plus(vec3_t dst, const vec3_t p, const vec3_t normal);
    void AxisCopy_plus(vec3_t in[3], vec3_t out[3]);
    float Q_fabs_plus(float f);
    float Q_rsqrt_plus(float number);
    vec_t VectorNormalize2_plus(const vec3_t v, vec3_t out);

    float RadiusFromBounds_plus(const vec3_t mins, const vec3_t maxs);
    void AddPointToBounds_plus(const vec3_t v, vec3_t mins, vec3_t maxs);
    float Q_acos_plus(float c);
    int Q_log2_plus(int val);
    void AxisClear_plus(vec3_t axis[3]);

#ifdef __cplusplus
}
#endif

#endif // Q_MATH_HPP
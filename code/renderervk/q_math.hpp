#ifndef Q_MATH_HPP
#define Q_MATH_HPP

#include "q_shared.hpp"
#include "../qcommon/q_shared_test.h"

vec_t VectorNormalize_plus(vec3_t &v);
vec_t VectorNormalize_plus(vec4_t &v);
void PerpendicularVector(vec3_t &dst, const vec3_t &src);
void MatrixMultiply(float in1[3][3], float in2[3][3], float out[3][3]);
float Q_fabs(float f);
void SetPlaneSignbits(struct cplane_s *out);
int BoxOnPlaneSide_plus(const vec3_t &emins, const vec3_t &emaxs, struct cplane_s &plane);
int BoxOnPlaneSide(vec3_t emins, vec3_t emaxs, struct cplane_s *p);
void AxisClear(vec3_t axis[3]);
float Q_atof(const char *str);
void RotatePointAroundVector(vec3_t &dst, const vec3_t &dir, const vec3_t &point,
                             float degrees);
void AxisCopy(vec3_t in[3], vec3_t out[3]);
float RadiusFromBounds(const vec3_t &mins, const vec3_t &maxs);

bool PlaneFromPoints(vec4_t &plane, const vec3_t &a, const vec3_t &b, const vec3_t &c);

void ClearBounds_plus(vec_t *mins, vec_t *maxs);

// fast vector normalize routine that does not check to make sure
// that length != 0, nor does it return length, uses rsqrt approximation
static inline void VectorNormalizeFast(vec3_t v)
{
    float ilength;

    ilength = Q_rsqrt(DotProduct(v, v));

    v[0] *= ilength;
    v[1] *= ilength;
    v[2] *= ilength;
}

vec_t VectorNormalize2_plus(const vec3_t &v, vec3_t &out);
vec_t VectorNormalize2(const vec3_t v, vec3_t out);
void MakeNormalVectors(const vec3_t &forward, vec3_t &right, vec3_t &up);

void AddPointToBounds_plus(const vec_t *v, vec_t *mins, vec_t *maxs);

int Q_log2(int val);

float Q_acos(float c);

#endif // Q_MATH_HPP

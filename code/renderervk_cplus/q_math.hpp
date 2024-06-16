#ifndef Q_MATH_HPP
#define Q_MATH_HPP

#include "q_shared.hpp"
#include "../qcommon/q_shared_test.h"

vec_t VectorNormalize(vec3_t v);
void PerpendicularVector(vec3_t dst, const vec3_t src);
void MatrixMultiply(float in1[3][3], float in2[3][3], float out[3][3]);
void AngleVectors(const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up);
float Q_fabs(float f);
void SetPlaneSignbits_plus(struct cplane_s *out);
int BoxOnPlaneSide_plus(vec3_t emins, vec3_t emaxs, struct cplane_s *plane);

#endif // Q_MATH_HPP

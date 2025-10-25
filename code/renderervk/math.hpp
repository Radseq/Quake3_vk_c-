#ifndef MATH_HPP
#define MATH_HPP

#include "tr_local.hpp"
#include <numbers> // For std::numbers::pi
constexpr double PI_cpp = 3.14159265358979323846;

constexpr size_t DegreeToRadiansTableSize = 361;

/*
===============
R_ClampDenorm

Clamp fp values that may result in denormalization after further multiplication
===============
*/
float R_ClampDenorm(float v);

consteval auto generateDegreeToRadiansLookupTable()
{
    std::array<float, DegreeToRadiansTableSize> table{};
    for (size_t i = 0; i < DegreeToRadiansTableSize; ++i)
        table[i] = static_cast<float>(i) * PI_cpp / 180.0f;
    return table;
}

constexpr auto DEG_TO_RAD_LUT = generateDegreeToRadiansLookupTable();

constexpr float deg2rad(float degrees) noexcept
{
    return (degrees * PI_cpp) / 180.0f;
}

int BoxOnPlaneSide_cpp(const vec3_t &emins, const vec3_t &emaxs, cplane_s &p);

void RotatePointAroundVector_cpp(vec3_t &dst, const vec3_t &dir, const vec3_t &point, float degrees);

// vec_t VectorNormalize(vec3_t v);
// void PerpendicularVector(vec3_t dst, const vec3_t src);
// void MatrixMultiply(float in1[3][3], float in2[3][3], float out[3][3]);
// void AngleVectors(const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up);
// float Q_fabs(float f);
// void SetPlaneSignbits(struct cplane_s *out);
// void AxisClear(vec3_t axis[3]);
// float Q_atof(const char *str);
// void RotatePointAroundVector(vec3_t dst, const vec3_t dir, const vec3_t point,
//                              float degrees);
// void AxisCopy(vec3_t in[3], vec3_t out[3]);
// float RadiusFromBounds(const vec3_t mins, const vec3_t maxs);

// bool PlaneFromPoints(vec4_t plane, const vec3_t a, const vec3_t b, const vec3_t c);

inline void ClearBounds_cpp(vec3_t &mins, vec3_t &maxs) {
    mins[0] = mins[1] = mins[2] = 99999.f;
    maxs[0] = maxs[1] = maxs[2] = -99999.f;
}

// // fast vector normalize routine that does not check to make sure
// // that length != 0, nor does it return length, uses rsqrt approximation

// vec_t VectorNormalize2(const vec3_t v, vec3_t out);
// void MakeNormalVectors(const vec3_t forward, vec3_t right, vec3_t up);

// int Q_log2(int val);

// void PerpendicularVector( vec3_t dst, const vec3_t src );

// float Q_acos(float c);

#endif // MATH_HPP

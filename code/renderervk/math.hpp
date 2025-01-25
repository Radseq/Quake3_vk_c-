#ifndef MATH_HPP
#define MATH_HPP

#include "tr_local.hpp"

constexpr vec3cpp_t	vec3cpp_origin_cpp = { 0,0,0 };

template <typename CPPArray, typename CTypeArray>
CTypeArray* to_c_array(CPPArray&& cpp_array) noexcept {
	return reinterpret_cast<CTypeArray*>(std::forward<CPPArray>(cpp_array).data());
}

const vec3cpp_t& toArrayConstRef(const float* arr);

const vec4cpp_t& toArray4ConstRef(const float* arr);

const vec3cpp_t& toArrayConstRef(const vec3_t(&arr)[3]);

const vec3cpp_t& toArrayConstRef(vec3_t(&arr)[3]);

vec3cpp_t& toArrayRef(vec3_t& vec);

vec3_t& toArrayRef(vec3cpp_t& vec);

vec3cpp_t& toArrayFloatRef(float(&vec)[3]);

vec3cpp_t& toArrayRef(const float* arr);

// Dot product of 3D vectors - RVO works here since we return a scalar by value
float DotProduct_cpp(const float* a, const float* b);

float DotProduct_cpp(const float* a, const vec3cpp_t& b);

// Subtract one 3D vector from another - Return by value for RVO
vec3cpp_t VectorSubtract_cpp(const float* a, const float* b);

// Add two 3D vectors - Return by value for RVO
vec3cpp_t VectorAdd_cpp(const float* a, const float* b);

vec3cpp_t VectorAdd_cpp(const float* a, const vec3cpp_t& b);

// Copy a 3D vector - Return by value for RVO
vec3cpp_t VectorCopy_cpp(const float* a);

// Copy a 3D vector - Return by value for RVO
vec4cpp_t VectorCopy_cppToVec4(const float* a);

// Scale a 3D vector by a scalar - Return by value for RVO
vec3cpp_t VectorScale_cpp(const float* v, float s);

// Multiply and add two vectors: o = v + b * s - Return by value for RVO
vec3cpp_t VectorMA_cpp(const float* v, float s, const float* b);

// Dot product of 4D vectors - Return by value for RVO
float DotProduct4_cpp(const float* a, const float* b);

// Scale a 4D vector by a scalar - Return by value for RVO
vec4cpp_t VectorScale4_cpp(const float* a, float b);

// Dot product of 3D vectors - RVO works here since we return a scalar by value
float DotProduct_cpp(const vec3cpp_t& a, const vec3cpp_t& b);

// Dot product of 3D vectors - RVO works here since we return a scalar by value
float DotProduct_cpp(const std::span<float>& a, const vec3cpp_t& b);

// Dot product of 3D vectors - RVO works here since we return a scalar by value
float DotProduct_cpp(const std::span<float>& a, const std::span<float>& b);

// Dot product of 3D vectors - RVO works here since we return a scalar by value
float DotProduct_cpp_v2(const std::span<vec_tcpp>& a, const vec3cpp_t& b);

// Dot product of 3D vectors - RVO works here since we return a scalar by value
float DotProduct_cpp_v2(const std::array<vec_tcpp, 4>& a, const vec3cpp_t& b);

// Dot product of 3D vectors - RVO works here since we return a scalar by value
float DotProduct_cpp(const vec3cpp_t& a, const std::span<float>& b);

// Subtract one 3D vector from another - Return by value for RVO
vec3cpp_t VectorSubtract_cpp(const vec3cpp_t& a, const vec3cpp_t& b);

vec3cpp_t VectorSubtract_cpp(float* a, const vec3cpp_t& b);

// Dot product of 3D vectors - RVO works here since we return a scalar by value
vec3cpp_t VectorSubtract_cpp(const std::span<float>& a, const vec3cpp_t& b);

// Dot product of 3D vectors - RVO works here since we return a scalar by value
vec3cpp_t VectorSubtract_cpp(const vec3cpp_t& a, const std::span<float>& b);


// Add two 3D vectors - Return by value for RVO
vec3cpp_t VectorAdd_cpp(const vec3cpp_t& a, const vec3cpp_t& b);

// Add two 3D vectors - Return by value for RVO
vec4cpp_t VectorAdd_to4vec_cpp(const vec3cpp_t& a, const vec3cpp_t& b);

// Add two 3D vectors - Return by value for RVO
vec3cpp_t VectorAdd_cpp(const std::span<float>& a, const vec3cpp_t& b);

// Copy a 3D vector - Return by value for RVO
vec3cpp_t VectorCopy_cpp(const vec3cpp_t& a);

// Copy a 3D vector - Return by value for RVO
vec4cpp_t VectorCopy_to4vec_cpp(const vec3cpp_t& a);

// Copy a 3D vector - Return by value for RVO
vec4cpp_t VectorCopy_cppToVec4(const vec3cpp_t& a);

// Copy a 3D vector
void VectorCopy_cpp_wrong(const vec3cpp_t& a, vec3_t& result);


// Scale a 3D vector by a scalar - Return by value for RVO
vec3cpp_t VectorScale_cpp(const vec3cpp_t& v, float s);


// Scale a 3D vector by a scalar - Return by value for RVO
vec4cpp_t VectorScale_to4vec_cpp(const vec3cpp_t& v, float s);

// Scale a 3D vector by a scalar
void VectorScale_cpp_wrong(const vec3cpp_t& v, float s, vec3_t& result);

// Multiply and add two vectors: o = v + b * s - Return by value for RVO
vec3cpp_t VectorMA_cpp(const vec3cpp_t& v, float s, const vec3cpp_t& b);

// Multiply and add two vectors: o = v + b * s - Return by value for RVO
vec4cpp_t VectorMA_to4vec_cpp(const vec3cpp_t& v, float s, const vec3cpp_t& b);

// Multiply and add two vectors: o = v + b * s - Return by value for RVO
vec4cpp_t VectorMA_to4vec_cpp(const vec4cpp_t& v, float s, const vec3cpp_t& b);

// Multiply and add two vectors: o = v + b * s
void VectorMA_cpp_wrong(const vec3cpp_t& v, float s, const vec3cpp_t& b, vec3_t& result);

// Dot product of 4D vectors - Return by value for RVO
float DotProduct4_cpp(const vec4cpp_t& a, const vec4cpp_t& b);

// Scale a 4D vector by a scalar - Return by value for RVO
vec4cpp_t VectorScale4_cpp(const vec4cpp_t& a, float b);

// Copy a 4D vector - Return by value for RVO
vec4cpp_t VectorCopy_cpp(const vec4cpp_t& a);
vec_tcpp VectorNormalize2_cpp(const vec3cpp_t v, vec3cpp_t& out);

vec_tcpp VectorNormalize2_cpp(const vec3cpp_t v, vec3_t& out);

static ID_INLINE vec_tcpp VectorLengthSquared_cpp(const vec3cpp_t v) {
	return v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
}

#if !defined( Q3_VM ) || ( defined( Q3_VM ) && defined( __Q3_VM_MATH ) )
static ID_INLINE int VectorCompare_cpp(const vec3cpp_t v1, const vec3cpp_t v2) {
	if (v1[0] != v2[0] || v1[1] != v2[1] || v1[2] != v2[2]) {
		return 0;
	}
	return 1;
}


static ID_INLINE vec_t VectorLength_cpp(const vec3cpp_t& v) {
	return (vec_t)sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

static ID_INLINE vec3cpp_t CrossProduct_cpp(const float* v1, const float* v2) {
	return {
		 v1[1] * v2[2] - v1[2] * v2[1],
		 v1[2] * v2[0] - v1[0] * v2[2],
		 v1[0] * v2[1] - v1[1] * v2[0]
	};
}

static ID_INLINE vec3cpp_t CrossProduct_cpp(const vec3cpp_t& v1, const vec3cpp_t& v2) {
	return {
		 v1[1] * v2[2] - v1[2] * v2[1],
		 v1[2] * v2[0] - v1[0] * v2[2],
		 v1[0] * v2[1] - v1[1] * v2[0]
	};
}

static ID_INLINE vec3cpp_t CrossProduct_cpp(const std::span<float>& v1, const vec3cpp_t& v2) {
	return {
		 v1[1] * v2[2] - v1[2] * v2[1],
		 v1[2] * v2[0] - v1[0] * v2[2],
		 v1[0] * v2[1] - v1[1] * v2[0]
	};
}

void VectorInverse_cpp(vec3cpp_t& v);

// fast vector normalize routine that does not check to make sure
// that length != 0, nor does it return length, uses rsqrt approximation
static ID_INLINE void VectorNormalizeFast_cpp(vec3cpp_t& v)
{
	float ilength = Q_rsqrt(DotProduct_cpp(v, v));

	v[0] *= ilength;
	v[1] *= ilength;
	v[2] *= ilength;
}

// fast vector normalize routine that does not check to make sure
// that length != 0, nor does it return length, uses rsqrt approximation
static ID_INLINE void VectorNormalizeFast_cpp(vec4cpp_t& v)
{
	float ilength = Q_rsqrt(DotProduct_cpp(std::span<float>(v.data(), 3), std::span<float>(v.data(), 3)));

	v[0] *= ilength;
	v[1] *= ilength;
	v[2] *= ilength;
}

#endif

///*
//=====================
//PlaneFromPoints
//
//Returns false if the triangle is degenerate.
//The normal will point out of the clock for clockwise ordered points
//=====================
//*/
//bool PlaneFromPoints_cpp(vec4cpp_t plane, const vec3cpp_t a, const vec3cpp_t b, const vec3cpp_t c) {
//	auto d1 = VectorSubtract_cpp(b, a);
//	auto d2 = VectorSubtract_cpp(c, a);
//	arr4ToConstArray3(plane) = CrossProduct_cpp(d2, d1);
//	if (VectorNormalize_cpp(plane) == 0) {
//		return false;
//	}
//
//	plane[3] = DotProduct_cpp(a, arr4ToArray3(plane.data()));
//	return true;
//}


vec_tcpp VectorNormalize_cpp(vec3cpp_t& v);

void ClearBounds_cpp(vec3cpp_t& mins, vec3cpp_t& maxs);

void AddPointToBounds_cpp(const vec3cpp_t& v, vec3cpp_t& mins, vec3cpp_t& maxs);

void ProjectPointOnPlane_cpp(vec3cpp_t& dst, const vec3cpp_t& p, const vec3cpp_t& normal);
void ProjectPointOnPlane_cpp(vec3cpp_t& dst, const vec3cpp_t& p, const float* normal);

/*
** assumes "src" is normalized
*/
void PerpendicularVector_cpp(vec3cpp_t& dst, const vec3cpp_t& src);

void PerpendicularVector_cpp(vec3cpp_t& dst, const float* src);

/*
================
MakeNormalVectors_cpp

Given a normalized forward vector, create two
other perpendicular vectors
================
*/
void MakeNormalVectors_cpp(const vec3cpp_t& forward, vec3cpp_t& right, vec3cpp_t& up);

/*
===============
RotatePointAroundVector

This is not implemented very well...
===============
*/
vec3cpp_t RotatePointAroundVector_cpp(const vec3cpp_t& dir, const vec3cpp_t& point, const float degrees);

int BoxOnPlaneSide_cpp(const vec3_t& emins, const vec3_t& emaxs, cplane_s& p);

// vec_t VectorNormalize_cpp(vec3cpp_t v);
// void PerpendicularVector_cpp(vec3cpp_t dst, const vec3cpp_t src);
// void MatrixMultiply(float in1[3][3], float in2[3][3], float out[3][3]);
// void AngleVectors(const vec3cpp_t angles, vec3cpp_t forward, vec3cpp_t right, vec3cpp_t up);
// float Q_fabs(float f);
// void SetPlaneSignbits(struct cplane_s *out);
// void AxisClear(vec3cpp_t axis[3]);
// float Q_atof(const char *str);
// void RotatePointAroundVector(vec3cpp_t dst, const vec3cpp_t dir, const vec3cpp_t point,
//                              float degrees);
// void AxisCopy(vec3cpp_t in[3], vec3cpp_t out[3]);
// float RadiusFromBounds(const vec3cpp_t mins, const vec3cpp_t maxs);

// bool PlaneFromPoints(vec4cpp_t plane, const vec3cpp_t a, const vec3cpp_t b, const vec3cpp_t c);

// void ClearBounds_cpp(vec3cpp_t mins, vec3cpp_t maxs);

// // fast vector normalize routine that does not check to make sure
// // that length != 0, nor does it return length, uses rsqrt approximation


// vec_t VectorNormalize2_cpp(const vec3cpp_t v, vec3cpp_t out);
// void MakeNormalVectors_cpp(const vec3cpp_t forward, vec3cpp_t right, vec3cpp_t up);

// int Q_log2(int val);

// void PerpendicularVector_cpp( vec3cpp_t dst, const vec3cpp_t src );

// float Q_acos(float c);

#endif // MATH_HPP

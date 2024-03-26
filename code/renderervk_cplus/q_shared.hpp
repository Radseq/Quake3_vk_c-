/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
//
#ifndef Q_SHARED_HPP
#define Q_SHARED_HPP

#include "q_math.hpp"

#ifdef __cplusplus
extern "C"
{
#endif

#define ARRAY_LEN(x) (sizeof(x) / sizeof(*(x)))

#define STRARRAY_LEN(x) (ARRAY_LEN(x) - 1)

	// fast vector normalize routine that does not check to make sure
	// that length != 0, nor does it return length, uses rsqrt approximation
	static inline void VectorNormalizeFast_plus(vec3_t v)
	{
		float ilength;

		ilength = Q_rsqrt_plus(DotProduct(v, v));

		v[0] *= ilength;
		v[1] *= ilength;
		v[2] *= ilength;
	}

	static inline void VectorInverse_plus(vec3_t v)
	{
		v[0] = -v[0];
		v[1] = -v[1];
		v[2] = -v[2];
	}

	static inline void CrossProduct_plus(const vec3_t v1, const vec3_t v2, vec3_t cross)
	{
		cross[0] = v1[1] * v2[2] - v1[2] * v2[1];
		cross[1] = v1[2] * v2[0] - v1[0] * v2[2];
		cross[2] = v1[0] * v2[1] - v1[1] * v2[0];
	}

	int Q_stricmp_plus(const char *s1, const char *s2);

	static inline vec_t VectorLength_plus(const vec3_t v)
	{
		return (vec_t)sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
	}

	inline float DotProduct_plus(const float *x, const float *y)
	{
		return x[0] * y[0] + x[1] * y[1] + x[2] * y[2];
	}

	inline void VectorSubtract_plus(const float *a, const float *b, float *c)
	{
		c[0] = a[0] - b[0];
		c[1] = a[1] - b[1];
		c[2] = a[2] - b[2];
	}

	inline void VectorAdd_plus(const float *a, const float *b, float *c)
	{
		c[0] = a[0] + b[0];
		c[1] = a[1] + b[1];
		c[2] = a[2] + b[2];
	}

	inline void VectorCopy_plus(const float *a, float *b)
	{
		b[0] = a[0];
		b[1] = a[1];
		b[2] = a[2];
	}

	inline void VectorScale_plus(const float *v, float s, float *o)
	{
		o[0] = v[0] * s;
		o[1] = v[1] * s;
		o[2] = v[2] * s;
	}

	inline void VectorMA_plus(const float *v, float s, const float *b, float *o)
	{
		o[0] = v[0] + b[0] * s;
		o[1] = v[1] + b[1] * s;
		o[2] = v[2] + b[2] * s;
	}

	inline float DotProduct4_plus(const float *a, const float *b)
	{
		return a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
	}

	inline void VectorScale4_plus(const float *a, float b, float *c)
	{
		c[0] = a[0] * b;
		c[1] = a[1] * b;
		c[2] = a[2] * b;
		c[3] = a[3] * b;
	}


#ifdef __cplusplus
}
#endif

#endif // Q_SHARED_HPP

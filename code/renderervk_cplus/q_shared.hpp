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

#ifdef __cplusplus
}
#endif

#endif // Q_SHARED_HPP

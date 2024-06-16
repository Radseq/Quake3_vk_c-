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
#ifndef __Q_SHARED_H_TEST
#define __Q_SHARED_H_TEST

#define QDECL

#if defined(__GNUC__) || defined(__clang__)
#define NORETURN __attribute__((noreturn))
#define NORETURN_PTR __attribute__((noreturn))
#elif defined(_MSC_VER)
#define NORETURN __declspec(noreturn)
// __declspec doesn't work on function pointers
#define NORETURN_PTR /* nothing */
#else
#define NORETURN	 /* nothing */
#define NORETURN_PTR /* nothing */
#endif

#if defined(__GNUC__) || defined(__clang__)
#define FORMAT_PRINTF(x, y) __attribute__((format(printf, x, y)))
#else
#define FORMAT_PRINTF(x, y) /* nothing */
#endif

typedef unsigned char byte;

typedef float vec_t;
typedef vec_t vec2_t[2];
typedef vec_t vec3_t[3];
typedef vec_t vec4_t[4];
typedef vec_t vec5_t[5];


// plane_t structure
// !!! if this is changed, it must be changed in asm code too !!!
typedef struct cplane_s
{
	vec3_t normal;
	float dist;
	byte type;	   // for fast side tests: 0,1,2 = axial, 3 = nonaxial
	byte signbits; // signx + (signy<<1) + (signz<<2), used as lookup during collision
	byte pad[2];
} cplane_t;

typedef enum
{
	ERR_FATAL,			  // exit the entire game with a popup window
	ERR_DROP,			  // print to console and disconnect from game
	ERR_SERVERDISCONNECT, // don't kill server
	ERR_DISCONNECT,		  // client disconnected from the server
	ERR_NEED_CD			  // pop up the need-cd dialog
} errorParm_t;

typedef union floatint_u
{
	int32_t i;
	unsigned int u;
	float f;
	byte b[4];
} floatint_t;

typedef enum
{
	TK_GENEGIC = 0, // for single-char tokens
	TK_STRING,
	TK_QUOTED,
	TK_EQ,
	TK_NEQ,
	TK_GT,
	TK_GTE,
	TK_LT,
	TK_LTE,
	TK_MATCH,
	TK_OR,
	TK_AND,
	TK_SCOPE_OPEN,
	TK_SCOPE_CLOSE,
	TK_NEWLINE,
	TK_EOF,
} tokenType_t;

#ifdef __cplusplus
extern "C"
{
#endif

	extern tokenType_t com_tokentype;

	void NORETURN FORMAT_PRINTF(2, 3) QDECL Com_Error(errorParm_t level, const char *fmt, ...);
	int QDECL Com_sprintf(char *dest, int size, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
	const char *QDECL va(const char *format, ...) __attribute__((format(printf, 1, 2)));

#ifdef __cplusplus
}
#endif

#endif // __Q_SHARED_H_TEST

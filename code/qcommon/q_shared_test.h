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

// Ignore __attribute__ on non-gcc/clang platforms
#if !defined(__GNUC__) && !defined(__clang__)
#ifndef __attribute__
#define __attribute__(x)
#endif
#endif

#ifdef __GNUC__
#define UNUSED_VAR __attribute__((unused))
#else
#define UNUSED_VAR
#endif

#if (defined _MSC_VER)
#define Q_EXPORT __declspec(dllexport)
#elif (defined __SUNPRO_C)
#define Q_EXPORT __global
#elif ((__GNUC__ >= 3) && (!__EMX__) && (!sun))
#define Q_EXPORT __attribute__((visibility("default")))
#else
#define Q_EXPORT
#endif

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

/**********************************************************************
  VM Considerations

  The VM can not use the standard system headers because we aren't really
  using the compiler they were meant for.  We use bg_lib.h which contains
  prototypes for the functions we define for our own use in bg_lib.c.

  When writing mods, please add needed headers HERE, do not start including
  stuff like <stdio.h> in the various .c files that make up each of the VMs
  since you will be including system headers files can will have issues.

  Remember, if you use a C library function that is not defined in bg_lib.c,
  you will have to add your own version for support in the VM.

 **********************************************************************/

#ifdef Q3_VM
typedef int intptr_t;
#else
#if defined(_MSC_VER) && !defined(__clang__)
typedef __int64 int64_t;
typedef __int32 int32_t;
typedef __int16 int16_t;
typedef __int8 int8_t;
typedef unsigned __int64 uint64_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int16 uint16_t;
typedef unsigned __int8 uint8_t;
#else
#include <stdint.h>
#endif

#ifdef _WIN32
// vsnprintf is ISO/IEC 9899:1999
// abstracting this to make it portable
int Q_vsnprintf(char *str, size_t size, const char *format, va_list ap);
#else
#define Q_vsnprintf vsnprintf
#endif
#endif

#if defined(_WIN32)
#if !defined(_MSC_VER)
// use GCC/Clang functions
#define Q_setjmp __builtin_setjmp
#define Q_longjmp __builtin_longjmp
#elif idx64 && (_MSC_VER >= 1910)
// use custom setjmp()/longjmp() implementations
#define Q_setjmp Q_setjmp_c
#define Q_longjmp Q_longjmp_c
int Q_setjmp_c(void *);
int Q_longjmp_c(void *, int);
#else // !idx64 || MSVC<2017
#define Q_setjmp setjmp
#define Q_longjmp longjmp
#endif
#else // !_WIN32
#define Q_setjmp setjmp
#define Q_longjmp longjmp
#endif

#ifdef __GNUC__
#define QALIGN(x) __attribute__((aligned(x)))
#else
#define QALIGN(x)
#endif

#ifndef __cplusplus
typedef unsigned char bool;
#define true 1
#define false 0
#endif

typedef unsigned char byte;

typedef float vec_t;
typedef vec_t vec2_t[2];
typedef vec_t vec3_t[3];
typedef vec_t vec4_t[4];
typedef vec_t vec5_t[5];

typedef int qhandle_t;
typedef int sfxHandle_t;
typedef int fileHandle_t;
typedef int clipHandle_t;

#define MAX_QPATH 64 // max length of a quake game pathname
#ifdef PATH_MAX
#define MAX_OSPATH PATH_MAX
#else
#define MAX_OSPATH 256 // max length of a filesystem pathname
#endif

#define MAX_CVAR_VALUE_STRING 256

#define MAX_MAP_AREA_BYTES 32 // bit vector of area visibility
// the game guarantees that no string from the network will ever
// exceed MAX_STRING_CHARS
#define MAX_STRING_CHARS 1024 // max length of a string passed to Cmd_TokenizeString
#define BIG_INFO_STRING 8192  // used for system info key only

#define PAD(base, alignment) (((base) + (alignment) - 1) & ~((alignment) - 1))
#define PADLEN(base, alignment) (PAD((base), (alignment)) - (base))

#define PADP(base, alignment) ((void *)PAD((intptr_t)(base), (alignment)))

#define VectorSubtract(a, b, c) ((c)[0] = (a)[0] - (b)[0], (c)[1] = (a)[1] - (b)[1], (c)[2] = (a)[2] - (b)[2])
#define Square(x) ((x) * (x))

#define CIN_system 1
#define CIN_loop 2
#define CIN_hold 4
#define CIN_silent 8
#define CIN_shader 16

#define PLANE_NON_AXIAL 3

#ifndef MAX
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#endif

#ifndef MIN
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#endif

#define MAX_QINT 0x7fffffff
#define MIN_QINT (-MAX_QINT - 1)

/*
==========================================================

CVARS (console variables)

Many variables can be used for cheating purposes, so when
cheats is zero, force all unspecified variables to their
default values.
==========================================================
*/

#define CVAR_ARCHIVE 0x0001		 // set to cause it to be saved to vars.rc
								 // used for system variables, not for player
								 // specific configurations
#define CVAR_USERINFO 0x0002	 // sent to server on connect or change
#define CVAR_SERVERINFO 0x0004	 // sent in response to front end requests
#define CVAR_SYSTEMINFO 0x0008	 // these cvars will be duplicated on all clients
#define CVAR_INIT 0x0010		 // don't allow change from console at all,
								 // but can be set from the command line
#define CVAR_LATCH 0x0020		 // will only change when C code next does
								 // a Cvar_Get(), so it can't be changed
								 // without proper initialization.  modified
								 // will be set, even though the value hasn't
								 // changed yet
#define CVAR_ROM 0x0040			 // display only, cannot be set by user at all
#define CVAR_USER_CREATED 0x0080 // created by a set command
#define CVAR_TEMP 0x0100		 // can be set even when cheats are disabled, but is not archived
#define CVAR_CHEAT 0x0200		 // can not be changed if cheats are disabled
#define CVAR_NORESTART 0x0400	 // do not clear when a cvar_restart is issued

#define CVAR_SERVER_CREATED 0x0800 // cvar was created by a server the client connected to.
#define CVAR_VM_CREATED 0x1000	   // cvar was created exclusively in one of the VMs.
#define CVAR_PROTECTED 0x2000	   // prevent modifying this var from VMs or the server

#define CVAR_NODEFAULT 0x4000 // do not write to config if matching with default value

#define CVAR_PRIVATE 0x8000 // can't be read from VM

#define CVAR_DEVELOPER 0x10000	   // can be set only in developer mode
#define CVAR_NOTABCOMPLETE 0x20000 // no tab completion in console

#define CVAR_ARCHIVE_ND (CVAR_ARCHIVE | CVAR_NODEFAULT)

// These flags are only returned by the Cvar_Flags() function
#define CVAR_MODIFIED 0x40000000	// Cvar was modified
#define CVAR_NONEXISTENT 0x80000000 // Cvar doesn't exist.

#define S_COLOR_YELLOW "^3"
#define S_COLOR_CYAN "^5"

/*
=================
PlaneTypeForNormal
=================
*/

// plane types are used to speed some tests
// 0-2 are axial planes
#define PLANE_X 0
#define PLANE_Y 1
#define PLANE_Z 2

#define PlaneTypeForNormal(x) (x[0] == 1.0 ? PLANE_X : (x[1] == 1.0 ? PLANE_Y : (x[2] == 1.0 ? PLANE_Z : PLANE_NON_AXIAL)))


/*
** float Q_rsqrt( float number )
*/
float inline Q_rsqrt(float number)
{
#if defined(_MSC_SSE2)
	float ret;
	_mm_store_ss(&ret, _mm_rsqrt_ss(_mm_load_ss(&number)));
	return ret;
#elif defined(_GCC_SSE2)
	/* writing it this way allows gcc to recognize that rsqrt can be used with -ffast-math */
	return 1.0f / sqrtf(number);
#else
	floatint_t t;
	float x2, y;
	const float threehalfs = 1.5F;

	x2 = number * 0.5F;
	t.f = number;
	t.i = 0x5f3759df - (t.i >> 1); // what the fuck?
	y = t.f;
	y = y * (threehalfs - (x2 * y * y)); // 1st iteration
										 //	y  = y * ( threehalfs - ( x2 * y * y ) );   // 2nd iteration, this can be removed

	return y;
#endif
}

#define ColorIndex(c) (((c) - '0') & 7)

#define DEG2RAD(a) (((a) * M_PI) / 180.0F)

#define Vector4Copy(a, b) ((b)[0] = (a)[0], (b)[1] = (a)[1], (b)[2] = (a)[2], (b)[3] = (a)[3])
#define Vector4Set(v, x, y, z, w) ((v)[0] = (x), (v)[1] = (y), (v)[2] = (z), v[3] = (w))
#define VectorSet(v, x, y, z) ((v)[0] = (x), (v)[1] = (y), (v)[2] = (z))
#define VectorNegate(a, b) ((b)[0] = -(a)[0], (b)[1] = -(a)[1], (b)[2] = -(a)[2])
#define VectorClear(a) ((a)[0] = (a)[1] = (a)[2] = 0)

#define VectorScale4(a, b, c) ((c)[0] = (a)[0] * (b), (c)[1] = (a)[1] * (b), (c)[2] = (a)[2] * (b), (c)[3] = (a)[3] * (b))
#define DotProduct(x, y) ((x)[0] * (y)[0] + (x)[1] * (y)[1] + (x)[2] * (y)[2])

#define VectorAdd(a, b, c) ((c)[0] = (a)[0] + (b)[0], (c)[1] = (a)[1] + (b)[1], (c)[2] = (a)[2] + (b)[2])
#define VectorCopy(a, b) ((b)[0] = (a)[0], (b)[1] = (a)[1], (b)[2] = (a)[2])
#define VectorScale(v, s, o) ((o)[0] = (v)[0] * (s), (o)[1] = (v)[1] * (s), (o)[2] = (v)[2] * (s))
#define VectorMA(v, s, b, o) ((o)[0] = (v)[0] + (b)[0] * (s), (o)[1] = (v)[1] + (b)[1] * (s), (o)[2] = (v)[2] + (b)[2] * (s))

#define DotProduct4(a, b) ((a)[0] * (b)[0] + (a)[1] * (b)[1] + (a)[2] * (b)[2] + (a)[3] * (b)[3])

#if !defined(Q3_VM) || (defined(Q3_VM) && defined(__Q3_VM_MATH))
static ID_INLINE int VectorCompare(const vec3_t v1, const vec3_t v2)
{
	if (v1[0] != v2[0] || v1[1] != v2[1] || v1[2] != v2[2])
	{
		return 0;
	}
	return 1;
}

static ID_INLINE vec_t VectorLength(const vec3_t v)
{
	return (vec_t)sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

static ID_INLINE vec_t VectorLengthSquared(const vec3_t v)
{
	return (v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

static ID_INLINE vec_t Distance(const vec3_t p1, const vec3_t p2)
{
	vec3_t v;

	VectorSubtract(p2, p1, v);
	return VectorLength(v);
}

static ID_INLINE vec_t DistanceSquared(const vec3_t p1, const vec3_t p2)
{
	vec3_t v;

	VectorSubtract(p2, p1, v);
	return v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
}

static ID_INLINE void VectorInverse(vec3_t v)
{
	v[0] = -v[0];
	v[1] = -v[1];
	v[2] = -v[2];
}

static ID_INLINE void CrossProduct(const vec3_t v1, const vec3_t v2, vec3_t cross)
{
	cross[0] = v1[1] * v2[2] - v1[2] * v2[1];
	cross[1] = v1[2] * v2[0] - v1[0] * v2[2];
	cross[2] = v1[0] * v2[1] - v1[1] * v2[0];
}

#else
int VectorCompare(const vec3_t v1, const vec3_t v2);

vec_t VectorLength(const vec3_t v);

vec_t VectorLengthSquared(const vec3_t v);

vec_t Distance(const vec3_t p1, const vec3_t p2);

vec_t DistanceSquared(const vec3_t p1, const vec3_t p2);

void VectorNormalizeFast(vec3_t v);

void VectorInverse(vec3_t v);

void CrossProduct(const vec3_t v1, const vec3_t v2, vec3_t cross);

#endif

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

typedef union
{
	byte rgba[4];
	unsigned int u32;
} color4ub_t;

typedef enum
{
	PRINT_ALL,
	PRINT_DEVELOPER, // only print when "developer 1"
	PRINT_WARNING,
	PRINT_ERROR
} printParm_t;

typedef enum
{
	h_high,
	h_low,
	h_dontcare
} ha_pref;

typedef enum
{
	CV_NONE = 0,
	CV_FLOAT,
	CV_INTEGER,
	CV_FSPATH,
	CV_MAX,
} cvarValidator_t;

typedef enum
{
	CVG_NONE = 0,
	CVG_RENDERER,
	CVG_SERVER,
	CVG_MAX,
} cvarGroup_t;

// nothing outside the Cvar_*() functions should modify these fields!
typedef struct cvar_s cvar_t;

struct cvar_s
{
	char *name;
	char *string;
	char *resetString;	 // cvar_restart will reset to this value
	char *latchedString; // for CVAR_LATCH vars
	int flags;
	bool modified;		   // set each time the cvar is changed
	int modificationCount; // incremented each time the cvar is changed
	float value;		   // Q_atof( string )
	int integer;		   // atoi( string )
	cvarValidator_t validator;
	char *mins;
	char *maxs;
	char *description;

	cvar_t *next;
	cvar_t *prev;
	cvar_t *hashNext;
	cvar_t *hashPrev;
	int hashIndex;
	cvarGroup_t group; // to track changes
};

// parameters for command buffer stuffing
typedef enum
{
	EXEC_NOW, // don't return until completed, a VM should NEVER use this,
			  // because some commands might cause the VM to be unloaded...
	EXEC_INSERT, // insert at current position, but don't run yet
	EXEC_APPEND	 // add to end of the command buffer (normal case)
} cbufExec_t;

// real time
//=============================================

typedef struct qtime_s
{
	int tm_sec;	  /* seconds after the minute - [0,59] */
	int tm_min;	  /* minutes after the hour - [0,59] */
	int tm_hour;  /* hours since midnight - [0,23] */
	int tm_mday;  /* day of the month - [1,31] */
	int tm_mon;	  /* months since January - [0,11] */
	int tm_year;  /* years since 1900 */
	int tm_wday;  /* days since Sunday - [0,6] */
	int tm_yday;  /* days since January 1 - [0,365] */
	int tm_isdst; /* daylight savings time flag */
} qtime_t;

// cinematic states
typedef enum
{
	FMV_IDLE,
	FMV_PLAY, // play
	FMV_EOF,  // all other conditions, i.e. stop/EOF/abort
	FMV_ID_BLT,
	FMV_ID_IDLE,
	FMV_LOOPED,
	FMV_ID_WAIT
} e_status;

extern vec4_t colorBlack;
extern const vec3_t vec3_origin;

#ifdef __cplusplus
extern "C"
{
#endif

	extern tokenType_t com_tokentype;

	void NORETURN FORMAT_PRINTF(2, 3) QDECL Com_Error(errorParm_t level, const char *fmt, ...);
	int QDECL Com_sprintf(char *dest, int size, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
	const char *QDECL va(const char *format, ...) __attribute__((format(printf, 1, 2)));

	int Q_stricmp(const char *s1, const char *s2);
	void Q_strncpyz(char *dest, const char *src, int destsize);

#ifdef __cplusplus
}
#endif

#endif // __Q_SHARED_H_TEST

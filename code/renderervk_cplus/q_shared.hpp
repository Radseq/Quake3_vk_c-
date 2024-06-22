#ifndef Q_SHARED_HPP
#define Q_SHARED_HPP

//#include <cstdint>
#include <math.h>
#include "q_platform.hpp"
#include "../qcommon/q_shared_test.h"

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

extern const unsigned char locase[256];

// angle indexes
#define PITCH 0 // up / down
#define YAW 1   // left / right
#define ROLL 2  // fall over

#define MAX_TOKEN_CHARS 1024 // max length of an individual token
#define NUMVERTEXNORMALS 162

#define COLOR_WHITE '7'
#define ColorIndex(c) (((c) - '0') & 7)

typedef unsigned char byte;

#define ARRAY_LEN(x) (sizeof(x) / sizeof(*(x)))
#define DEG2RAD(a) (((a) * M_PI) / 180.0F)
#define RAD2DEG(a) (((a) * 180.0f) / M_PI)

#define DotProduct(x, y) ((x)[0] * (y)[0] + (x)[1] * (y)[1] + (x)[2] * (y)[2])
#define VectorSubtract(a, b, c) ((c)[0] = (a)[0] - (b)[0], (c)[1] = (a)[1] - (b)[1], (c)[2] = (a)[2] - (b)[2])
#define VectorAdd(a, b, c) ((c)[0] = (a)[0] + (b)[0], (c)[1] = (a)[1] + (b)[1], (c)[2] = (a)[2] + (b)[2])
#define VectorCopy(a, b) ((b)[0] = (a)[0], (b)[1] = (a)[1], (b)[2] = (a)[2])
#define VectorScale(v, s, o) ((o)[0] = (v)[0] * (s), (o)[1] = (v)[1] * (s), (o)[2] = (v)[2] * (s))
#define VectorMA(v, s, b, o) ((o)[0] = (v)[0] + (b)[0] * (s), (o)[1] = (v)[1] + (b)[1] * (s), (o)[2] = (v)[2] + (b)[2] * (s))

#define DotProduct4(a, b) ((a)[0] * (b)[0] + (a)[1] * (b)[1] + (a)[2] * (b)[2] + (a)[3] * (b)[3])
#define VectorScale4(a, b, c) ((c)[0] = (a)[0] * (b), (c)[1] = (a)[1] * (b), (c)[2] = (a)[2] * (b), (c)[3] = (a)[3] * (b))
#define VectorClear(a) ((a)[0] = (a)[1] = (a)[2] = 0)
#define VectorNegate(a, b) ((b)[0] = -(a)[0], (b)[1] = -(a)[1], (b)[2] = -(a)[2])
#define VectorSet(v, x, y, z) ((v)[0] = (x), (v)[1] = (y), (v)[2] = (z))
#define Vector4Set(v, x, y, z, w) ((v)[0] = (x), (v)[1] = (y), (v)[2] = (z), v[3] = (w))
#define Vector4Copy(a, b) ((b)[0] = (a)[0], (b)[1] = (a)[1], (b)[2] = (a)[2], (b)[3] = (a)[3])


typedef float vec_t;
typedef vec_t vec2_t[2];
typedef vec_t vec3_t[3];
typedef vec_t vec4_t[4];
typedef vec_t vec5_t[5];

char *Q_strlwr(char *s1);
const char *COM_GetExtension(const char *name);
void COM_StripExtension(const char *in, char *out, int destsize);
// void Q_strncpyz(char *dest, const char *src, int destsize);
const char *COM_ParseExt(const char **data_p, bool allowLineBreak);
void COM_BeginParseSession(const char *name);
int COM_GetCurrentParseLine(void);
bool SkipBracedSection(const char **program, int depth);
void Q_strcat(char *dest, int size, const char *src);
char *COM_ParseComplex(const char **data_p, bool allowLineBreak);
char *Q_stradd(char *dst, const char *src);
int Q_stricmpn(const char *s1, const char *s2, int n);
// int Q_stricmp(const char *s1, const char *s2);
void SkipRestOfLine(const char **data);
int COM_Compress(char *data_p);
char *COM_SkipPath(char *pathname);
const char *COM_Parse(const char **data_p);
int Com_Split(char *in, char **out, int outsz, int delim);
int Q_strncmp(const char *s1, const char *s2, int n);
const char *Q_stristr(const char *s, const char *find);

unsigned int crc32_buffer(const byte *buf, unsigned int len);
unsigned long Com_GenerateHashValue(const char *fname, const unsigned int size);

static inline void CrossProduct_plus(const vec3_t v1, const vec3_t v2, vec3_t cross)
{
    cross[0] = v1[1] * v2[2] - v1[2] * v2[1];
    cross[1] = v1[2] * v2[0] - v1[0] * v2[2];
    cross[2] = v1[0] * v2[1] - v1[1] * v2[0];
}

static inline vec_t VectorLength_plus(const vec3_t v)
{
    return (vec_t)sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

// typedef int		qhandle_t;

// typedef struct {
//   int height;       // number of scan lines
//   int top;          // top of glyph in buffer
//   int bottom;       // bottom of glyph in buffer
//   int pitch;        // width for copying
//   int xSkip;        // x adjustment
//   int imageWidth;   // width of actual image
//   int imageHeight;  // height of actual image
//   float s;          // x offset in image where glyph starts
//   float t;          // y offset in image where glyph starts
//   float s2;
//   float t2;
//   qhandle_t glyph;  // handle to the shader with the glyph
//   char shaderName[32];
// } glyphInfo_t;

// #define	MAX_QPATH			64		// max length of a quake game pathname
// #define GLYPH_START 0
// #define GLYPH_END 255

// #define GLYPHS_PER_FONT GLYPH_END - GLYPH_START + 1

// typedef struct
// {
//     glyphInfo_t glyphs[GLYPHS_PER_FONT];
//     float glyphScale;
//     char name[MAX_QPATH];
// } fontInfo_t;

#endif // Q_SHARED_HPP

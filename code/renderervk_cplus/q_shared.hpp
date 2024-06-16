#ifndef Q_SHARED_HPP
#define Q_SHARED_HPP

#include <cstdint>
#include <math.h>
#include "q_platform.hpp"
#include "../qcommon/q_shared_test.h"

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
void Q_strncpyz(char *dest, const char *src, int destsize);
const char *COM_ParseExt(const char **data_p, bool allowLineBreak);
void COM_BeginParseSession(const char *name);
int COM_GetCurrentParseLine(void);
bool SkipBracedSection(const char **program, int depth);
void Q_strcat(char *dest, int size, const char *src);
char *COM_ParseComplex(const char **data_p, bool allowLineBreak);
char *Q_stradd(char *dst, const char *src);
int Q_stricmpn(const char *s1, const char *s2, int n);
int Q_stricmp(const char *s1, const char *s2);
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

#endif // Q_SHARED_HPP

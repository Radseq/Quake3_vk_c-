#ifndef Q_SHARED_HPP
#define Q_SHARED_HPP

// #include <cstdint>
#include <math.h>
#include "../qcommon/q_platform.h"

#include "../qcommon/surfaceflags.h" // shared with the q3map utility

#include "../qcommon/q_shared_test.h"

#include <string.h>

#ifdef __GNUC__
#define QALIGN(x) __attribute__((aligned(x)))
#else
#define QALIGN(x)
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

#define Com_Memset memset
#define Com_Memcpy memcpy

extern tokenType_t com_tokentype;
extern const byte locase[256];

// angle indexes
#define PITCH 0 // up / down
#define YAW 1   // left / right
#define ROLL 2  // fall over

#define MAX_TOKEN_CHARS 1024 // max length of an individual token
#define NUMVERTEXNORMALS 162

#define COLOR_WHITE '7'

typedef unsigned char byte;
typedef vec_t quat_t[4];

#define ARRAY_LEN(x) (sizeof(x) / sizeof(*(x)))

#define MAX_UINT ((unsigned)(~0))

#define LUMA(red, green, blue) (0.2126f * (red) + 0.7152f * (green) + 0.0722f * (blue))
#define LERP(a, b, w) ((a) * (1.0f - (w)) + (b) * (w))
#define QuatCopy(a, b) ((b)[0] = (a)[0], (b)[1] = (a)[1], (b)[2] = (a)[2], (b)[3] = (a)[3])

#ifndef SGN
#define SGN(x) (((x) >= 0) ? !!(x) : -1)
#endif

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

int LongSwap(int l);

unsigned int crc32_buffer(const byte *buf, unsigned int len);
unsigned long Com_GenerateHashValue(const char *fname, const unsigned int size);

#endif // Q_SHARED_HPP

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
// q_shared.c -- stateless support routines that are included in each code dll
#include "q_shared.hpp"

int Q_stricmp_plus(const char *s1, const char *s2)
{
	if (s1 == s2)
		return 0;
	if (s1 == nullptr)
		return -1;
	if (s2 == nullptr)
		return 1;

	unsigned char c1, c2;
	do
	{
		c1 = *s1++;
		c2 = *s2++;

		// Convert uppercase to lowercase
		if (c1 >= 'A' && c1 <= 'Z')
			c1 += ('a' - 'A');
		if (c2 >= 'A' && c2 <= 'Z')
			c2 += ('a' - 'A');

		if (c1 != c2)
			return c1 < c2 ? -1 : 1;

	} while (c1 != '\0');

	return 0;
}

/*
============
va

does a varargs printf into a temp buffer, so I don't need to have
varargs versions of all text functions.
FIXME: make this buffer size safe someday
============
*/
const char *QDECL va_plus(const char *format, ...)
{
	char *buf;
	va_list argptr;
	static int index = 0;
	static char string[2][32000]; // in case va is called by nested functions

	buf = string[index];
	index ^= 1;

	va_start(argptr, format);
	vsprintf(buf, format, argptr);
	va_end(argptr);

	return buf;
}

char *Q_stradd_plus(char *dst, const char *src)
{
	char c;
	while ((c = *src++) != '\0')
		*dst++ = c;
	*dst = '\0';
	return dst;
}

#if defined(_DEBUG) && defined(_WIN32)
#include <windows.h>
#endif

int QDECL Com_sprintf_plus(char *dest, int size, const char *fmt, ...)
{
	int len;
	va_list argptr;
	char bigbuffer[32000]; // big, but small enough to fit in PPC stack

	if (!dest)
	{
		// Com_Error( ERR_FATAL, "Com_sprintf: NULL dest" );
#if defined(_DEBUG) && defined(_WIN32)
		DebugBreak();
#endif
		return 0;
	}

	va_start(argptr, fmt);
	len = vsprintf(bigbuffer, fmt, argptr);
	va_end(argptr);

	if (len >= sizeof(bigbuffer) || len < 0)
	{
		// Com_Error( ERR_FATAL, "Com_sprintf: overflowed bigbuffer" );
#if defined(_DEBUG) && defined(_WIN32)
		DebugBreak();
#endif
		return 0;
	}

	if (len >= size)
	{
		// Com_Printf( S_COLOR_YELLOW "Com_sprintf: overflow of %i in %i\n", len, size );
#if defined(_DEBUG) && defined(_WIN32)
		DebugBreak();
#endif
		len = size - 1;
	}

	// Q_strncpyz( dest, bigbuffer, size );
	// strncpy( dest, bigbuffer, len );
	memcpy(dest, bigbuffer, len);
	dest[len] = '\0';

	return len;
}

int Q_strncmp_plus(const char *s1, const char *s2, int n)
{
	int c1, c2;

	do
	{
		c1 = *s1++;
		c2 = *s2++;

		if (!n--)
		{
			return 0; // strings are equal until end point
		}

		if (c1 != c2)
		{
			return c1 < c2 ? -1 : 1;
		}
	} while (c1);

	return 0; // strings are equal
}

/*
=============
Q_strncpyz

Safe strncpy that ensures a trailing zero
=============
*/
void Q_strncpyz_plus(char *dest, const char *src, int destsize)
{
	if (!dest)
	{

		printf("FATAL: Q_strncpyz: NULL dest"); // todo fix me, use Com_Error( ERR_FATAL, "Q_strncpyz: NULL dest" ); instead
	}

	if (!src)
	{
		printf("FATAL: Q_strncpyz: NULL src"); // todo fix me, use Com_Error( ERR_FATAL, "Q_strncpyz: NULL src" ); instead
	}

	if (destsize < 1)
	{
		printf("FATAL: Q_strncpyz: destsize < 1"); // todo fix me, use Com_Error( ERR_FATAL, "Q_strncpyz: destsize < 1" ); instead
	}
#if 1
	// do not fill whole remaining buffer with zeros
	// this is obvious behavior change but actually it may affect only buggy QVMs
	// which passes overlapping or short buffers to cvar reading routines
	// what is rather good than bad because it will no longer cause overwrites, maybe
	while (--destsize > 0 && (*dest++ = *src++) != '\0')
		;
	*dest = '\0';
#else
	strncpy(dest, src, destsize - 1);
	dest[destsize - 1] = '\0';
#endif
}

int LongSwap_plus(int l)
{
	byte b1, b2, b3, b4;

	b1 = l & 255;
	b2 = (l >> 8) & 255;
	b3 = (l >> 16) & 255;
	b4 = (l >> 24) & 255;

	return ((int)b1 << 24) + ((int)b2 << 16) + ((int)b3 << 8) + b4;
}

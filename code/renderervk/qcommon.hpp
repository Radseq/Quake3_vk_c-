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
// qcommon.h -- definitions common between client and server, but not game.or ref modules
#ifndef Q_COMMON_HPP
#define Q_COMMON_HPP

#include "../qcommon/qfiles.h"

#define XSTRING(x)	STRING(x)
#define STRING(x)	#x

// TTimo
// centralized and cleaned, that's the max string you can send to a Com_Printf / Com_DPrintf (above gets truncated)
// bump to 8192 as 4096 may be not enough to print some data like gl extensions - CE
#define	MAXPRINTMSG	8192

// AVI files have the start of pixel lines 4 byte-aligned
#define AVI_LINE_PADDING 4

static inline unsigned int log2pad_plus(unsigned int v, int roundup)
{
    unsigned int x = 1;

    while (x < v)
        x <<= 1;

    if (roundup == 0)
    {
        if (x > v)
        {
            x >>= 1;
        }
    }

    return x;
}

#endif // Q_COMMON_HPP

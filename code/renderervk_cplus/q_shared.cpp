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

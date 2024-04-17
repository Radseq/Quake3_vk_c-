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
// tr_map.c

#include "tr_local.h"
#include "../renderervk_cplus/tr_bsp.hpp"
#include "vk.h"
/*

Loads and prepares a map file for scene rendering.

A single entry point:

void RE_LoadWorldMap( const char *name );

*/

//===============================================================================

/*
===============
R_ColorShiftLightingBytes
===============
*/
void R_ColorShiftLightingBytes(const byte in[4], byte out[4], bool hasAlpha)
{
	R_ColorShiftLightingBytes_plus(in[4], out[4], hasAlpha);
}

int R_GetLightmapCoords(const int lightmapIndex, float *x, float *y)
{
	return R_GetLightmapCoords_plus(lightmapIndex, x, y);
}

/*
=================
RE_SetWorldVisData

This is called by the clipmodel subsystem so we can share the 1.8 megs of
space in big maps...
=================
*/
void RE_SetWorldVisData(const byte *vis)
{
	RE_SetWorldVisData_plus(vis);
}

/*
=================
RE_GetEntityToken
=================
*/
bool RE_GetEntityToken(char *buffer, int size)
{
	return RE_GetEntityToken_plus(buffer, size);
}
/*
=================
RE_LoadWorldMap

Called directly from cgame
=================
*/
void RE_LoadWorldMap(const char *name)
{
	RE_LoadWorldMap_plus(name);
}
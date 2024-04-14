/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/ort modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
ort (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY ort FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
#include "tr_local.h"

/*
** GL_Bind
*/
void Bind(image_t *image)
{
	Bind_plus(image);
}

/*
** GL_SelectTexture
*/
void SelectTexture(int unit)
{
	SelectTexture_plus(unit);
}

/*
** GL_Cull
*/
void GL_Cull(cullType_t cullType)
{
	GL_Cull_plus(cullType);
}

/*
=============
RE_StretchRaw

FIXME: not exactly backend
Stretches a raw 32 bit power of 2 bitmap image over the given screen rectangle.
Used for cinematics.
=============
*/
void RE_StretchRaw(int x, int y, int w, int h, int cols, int rows, byte *data, int client, bool dirty)
{
	RE_StretchRaw_plus(x, y, w, h, cols, rows, data, client, dirty);
}

void RE_UploadCinematic(int w, int h, int cols, int rows, byte *data, int client, bool dirty)
{
	RE_UploadCinematic_plus(w, h, cols, rows, data, client, dirty);
}

/*
===============
RB_ShowImages

Draw all the images to the screen, on top of whatever
was there.  This is used to test for texture thrashing.

Also called by RE_EndRegistration
===============
*/
void RB_ShowImages(void)
{
	RB_ShowImages_plus();
}

/*
====================
RB_ExecuteRenderCommands
====================
*/
void RB_ExecuteRenderCommands(const void *data)
{
	RB_ExecuteRenderCommands_plus(data);
}
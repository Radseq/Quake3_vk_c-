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
// tr_image.c
#include "tr_local.h"
#include "../renderervk_cplus/tr_image.hpp"


/*
================
R_CreateImage

This is the only way any image_t are created
Picture data may be modified in-place during mipmap processing
================
*/
image_t *R_CreateImage(const char *name, const char *name2, byte *pic, int width, int height, imgFlags_t flags)
{
	return R_CreateImage_plus(name, name2, pic, width, height, flags);
}
/*
===============
R_FindImageFile

Finds or loads the given image.
Returns NULL if it fails, not a default image.
==============
*/
image_t *R_FindImageFile(const char *name, imgFlags_t flags)
{
	return R_FindImageFile_plus(name, flags);
}

/*
===============
R_InitImages
===============
*/
void R_InitImages(void)
{
	R_InitImages_plus();
}

/*
===============
R_DeleteTextures
===============
*/
void R_DeleteTextures(void)
{
	R_DeleteTextures_plus();
}

/*
===============
RE_RegisterSkin
===============
*/
qhandle_t RE_RegisterSkin(const char *name)
{
	return RE_RegisterSkin_plus(name);
}

/*
===============
R_InitSkins
===============
*/
void R_InitSkins(void)
{
	R_InitSkins_plus();
}

/*
===============
R_GetSkinByHandle
===============
*/
skin_t *R_GetSkinByHandle(qhandle_t hSkin)
{
	return R_GetSkinByHandle_plus(hSkin);
}

/*
===============
R_SkinList_f
===============
*/
void R_SkinList_f(void)
{
	R_SkinList_f_plus();
}

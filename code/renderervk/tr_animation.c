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

#include "tr_local.h"

/*

All bones should be an identity orientation to display the mesh exactly
as it is specified.

For all other frames, the bones represent the transformation from the
orientation of the bone in the base frame to the orientation in this
frame.

*/

// copied and adapted from tr_mesh.c

/*
==============
R_MDRAddAnimSurfaces
==============
*/

// much stuff in there is just copied from R_AddMd3Surfaces in tr_mesh.c

void R_MDRAddAnimSurfaces(trRefEntity_t *ent)
{
	R_MDRAddAnimSurfaces_plus(ent);
}
/*
==============
RB_MDRSurfaceAnim
==============
*/
void RB_MDRSurfaceAnim(mdrSurface_t *surface)
{
	RB_MDRSurfaceAnim_plus(surface);
}

void MC_UnCompress(float mat[3][4], const unsigned char *comp)
{
	MC_UnCompress_plus(mat, comp);
}
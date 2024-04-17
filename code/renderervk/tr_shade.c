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
// tr_shade.c

#include "tr_local.h"

/*

  THIS ENTIRE FILE IS BACK END

  This file deals with applying shaders to surface data in the tess struct.
*/

/*
=============================================================

SURFACE SHADERS

=============================================================
*/

/*
==============
RB_BeginSurface

We must set some things up before beginning any tesselation,
because a surface may be forced to perform a RB_End due
to overflow.
==============
*/
void RB_BeginSurface(shader_t *shader, int fogNum)
{
	RB_BeginSurface_plus(shader, fogNum);
}

/*
===================
DrawMultitextured

output = t0 * t1 ort t0 + t1

t0 = most upstream according to spec
t1 = most downstream according to spec
===================
*/

/*
===============
R_ComputeColors
===============
*/
void R_ComputeColors(const int b, color4ub_t *dest, const shaderStage_t *pStage)
{
	R_ComputeColors_plus(b, dest, pStage);
}

/*
===============
R_ComputeTexCoords
===============
*/
void R_ComputeTexCoords(const int b, const textureBundle_t *bundle)
{
	R_ComputeTexCoords_plus(b, bundle);
}

void VK_SetFogParams(vkUniform_t *uniform, int *fogStage)
{
	VK_SetFogParams_plus(uniform, fogStage);
}

uint32_t VK_PushUniform(const vkUniform_t *uniform)
{
	return VK_PushUniform_plus(uniform);
}

#ifdef USE_PMLIGHT
void VK_LightingPass(void)
{
	VK_LightingPass_plus();
}
#endif // USE_PMLIGHT

void RB_StageIteratorGeneric(void)
{
	RB_StageIteratorGeneric_plus();
}

/*
** RB_EndSurface
*/
void RB_EndSurface(void)
{
	RB_EndSurface_plus();
}
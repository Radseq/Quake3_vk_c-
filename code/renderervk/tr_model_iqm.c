/*
===========================================================================
Copyright (C) 2011 Thilo Schulz <thilo@tjps.eu>
Copyright (C) 2011 Matthias Bentrup <matthias.bentrup@googlemail.com>
Copyright (C) 2011-2019 Zack Middleton <zturtleman@gmail.com>

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
=================
R_LoadIQM

Load an IQM model and compute the joint matrices for every frame.
=================
*/
bool R_LoadIQM(model_t *mod, void *buffer, int filesize, const char *mod_name)
{
	return R_LoadIQM_plus(mod, buffer, filesize, mod_name);
}

/*
=================
R_AddIQMSurfaces

Add all surfaces of this model
=================
*/
void R_AddIQMSurfaces(trRefEntity_t *ent)
{
	R_AddIQMSurfaces_plus(ent);
}

/*
=================
RB_AddIQMSurfaces

Compute vertices for this model surface
=================
*/
void RB_IQMSurfaceAnim(const surfaceType_t *surface)
{
	RB_IQMSurfaceAnim_plus(surface);
}

int R_IQMLerpTag(orientation_t *tag, iqmData_t *data,
				 int startFrame, int endFrame,
				 float frac, const char *tagName)
{
	return R_IQMLerpTag_plus(tag, data, startFrame, endFrame, frac, tagName);
}

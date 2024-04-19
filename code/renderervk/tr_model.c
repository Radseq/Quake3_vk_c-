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
// tr_models.c -- model loading and caching

#include "tr_local.h"

#define LL(x) x = LittleLong(x)
/*
** R_GetModelByHandle
*/
model_t *R_GetModelByHandle(qhandle_t index)
{
	model_t *mod;

	// out of range gets the default model
	if (index < 1 || index >= tr.numModels)
	{
		return tr.models[0];
	}

	mod = tr.models[index];

	return mod;
}

//===============================================================================

/*
** R_AllocModel
*/
model_t *R_AllocModel(void)
{
	model_t *mod;

	if (tr.numModels >= MAX_MOD_KNOWN)
	{
		return NULL;
	}

	mod = ri.Hunk_Alloc(sizeof(*tr.models[tr.numModels]), h_low);
	mod->index = tr.numModels;
	tr.models[tr.numModels] = mod;
	tr.numModels++;

	return mod;
}

/*
====================
RE_RegisterModel

Loads in a model for the given name

Zero will be returned if the model fails to load.
An entry will be retained for failed models as an
optimization to prevent disk rescanning if they are
asked for again.
====================
*/
qhandle_t RE_RegisterModel(const char *name)
{
	return RE_RegisterModel_plus(name);
}
/*

/*
** RE_BeginRegistration
*/
void RE_BeginRegistration(glconfig_t *glconfigOut)
{
	RE_BeginRegistration_plus(glconfigOut);
}
//=============================================================================

/*
===============
R_ModelInit
===============
*/
void R_ModelInit(void)
{
	R_ModelInit_plus();
}

/*
================
R_Modellist_f
================
*/
void R_Modellist_f(void)
{
	R_Modellist_f_plus();
}

//=============================================================================

/*
================
R_GetTag
================
*/
static md3Tag_t *R_GetTag(md3Header_t *mod, int frame, const char *tagName)
{
	md3Tag_t *tag;
	int i;

	if (frame >= mod->numFrames)
	{
		// it is possible to have a bad frame while changing models, so don't error
		frame = mod->numFrames - 1;
	}

	tag = (md3Tag_t *)((byte *)mod + mod->ofsTags) + frame * mod->numTags;
	for (i = 0; i < mod->numTags; i++, tag++)
	{
		if (!strcmp(tag->name, tagName))
		{
			return tag; // found it
		}
	}

	return NULL;
}

static md3Tag_t *R_GetAnimTag(mdrHeader_t *mod, int framenum, const char *tagName, md3Tag_t *dest)
{
	int i, j, k;
	int frameSize;
	mdrFrame_t *frame;
	mdrTag_t *tag;

	if (framenum >= mod->numFrames)
	{
		// it is possible to have a bad frame while changing models, so don't error
		framenum = mod->numFrames - 1;
	}

	tag = (mdrTag_t *)((byte *)mod + mod->ofsTags);
	for (i = 0; i < mod->numTags; i++, tag++)
	{
		if (!strcmp(tag->name, tagName))
		{
			Q_strncpyz(dest->name, tag->name, sizeof(dest->name));

			// uncompressed model...
			//
			frameSize = (intptr_t)(&((mdrFrame_t *)0)->bones[mod->numBones]);
			frame = (mdrFrame_t *)((byte *)mod + mod->ofsFrames + framenum * frameSize);

			for (j = 0; j < 3; j++)
			{
				for (k = 0; k < 3; k++)
					dest->axis[j][k] = frame->bones[tag->boneIndex].matrix[k][j];
			}

			dest->origin[0] = frame->bones[tag->boneIndex].matrix[0][3];
			dest->origin[1] = frame->bones[tag->boneIndex].matrix[1][3];
			dest->origin[2] = frame->bones[tag->boneIndex].matrix[2][3];

			return dest;
		}
	}

	return NULL;
}

/*
================
R_LerpTag
================
*/
int R_LerpTag(orientation_t *tag, qhandle_t handle, int startFrame, int endFrame,
			  float frac, const char *tagName)
{
	md3Tag_t *start, *end;
	md3Tag_t start_space, end_space;
	int i;
	float frontLerp, backLerp;
	model_t *model;

	model = R_GetModelByHandle(handle);
	if (!model->md3[0])
	{
		if (model->type == MOD_MDR)
		{
			start = R_GetAnimTag((mdrHeader_t *)model->modelData, startFrame, tagName, &start_space);
			end = R_GetAnimTag((mdrHeader_t *)model->modelData, endFrame, tagName, &end_space);
		}
		else if (model->type == MOD_IQM)
		{
			return R_IQMLerpTag_plus(tag, model->modelData,
								startFrame, endFrame,
								frac, tagName);
		}
		else
		{
			start = end = NULL;
		}
	}
	else
	{
		start = R_GetTag(model->md3[0], startFrame, tagName);
		end = R_GetTag(model->md3[0], endFrame, tagName);
	}

	if (!start || !end)
	{
		AxisClear(tag->axis);
		VectorClear(tag->origin);
		return false;
	}

	frontLerp = frac;
	backLerp = 1.0f - frac;

	for (i = 0; i < 3; i++)
	{
		tag->origin[i] = start->origin[i] * backLerp + end->origin[i] * frontLerp;
		tag->axis[0][i] = start->axis[0][i] * backLerp + end->axis[0][i] * frontLerp;
		tag->axis[1][i] = start->axis[1][i] * backLerp + end->axis[1][i] * frontLerp;
		tag->axis[2][i] = start->axis[2][i] * backLerp + end->axis[2][i] * frontLerp;
	}
	VectorNormalize(tag->axis[0]);
	VectorNormalize(tag->axis[1]);
	VectorNormalize(tag->axis[2]);
	return true;
}

/*
====================
R_ModelBounds
====================
*/
void R_ModelBounds(qhandle_t handle, vec3_t mins, vec3_t maxs)
{
	R_ModelBounds_plus(handle, mins, maxs);
}
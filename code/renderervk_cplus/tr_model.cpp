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

#include "tr_model.hpp"

model_t *R_GetModelByHandle_plus(qhandle_t index)
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

void R_ModelBounds_plus(qhandle_t handle, vec3_t mins, vec3_t maxs)
{
	model_t *model;

	model = R_GetModelByHandle_plus(handle);

	if (model->type == MOD_BRUSH)
	{
		VectorCopy(model->bmodel->bounds[0], mins);
		VectorCopy(model->bmodel->bounds[1], maxs);

		return;
	}
	else if (model->type == MOD_MESH)
	{
		md3Header_t *header;
		md3Frame_t *frame;

		header = model->md3[0];
		frame = (md3Frame_t *)((byte *)header + header->ofsFrames);

		VectorCopy(frame->bounds[0], mins);
		VectorCopy(frame->bounds[1], maxs);

		return;
	}
	else if (model->type == MOD_MDR)
	{
		mdrHeader_t *header;
		mdrFrame_t *frame;

		header = (mdrHeader_t *)model->modelData;
		frame = (mdrFrame_t *)((byte *)header + header->ofsFrames);

		VectorCopy(frame->bounds[0], mins);
		VectorCopy(frame->bounds[1], maxs);

		return;
	}
	else if (model->type == MOD_IQM)
	{
		iqmData_t *iqmData;

		iqmData = static_cast<iqmData_t *>(model->modelData);

		if (iqmData->bounds)
		{
			VectorCopy(iqmData->bounds, mins);
			VectorCopy(iqmData->bounds + 3, maxs);
			return;
		}
	}

	VectorClear(mins);
	VectorClear(maxs);
}

// todo fix me, dont work
model_t *R_AllocModel_plus()
{
	model_t *mod;

	if (tr.numModels >= MAX_MOD_KNOWN)
	{
		return NULL;
	}

	mod = static_cast<model_t *>(ri.Hunk_Alloc(sizeof(*tr.models[tr.numModels]), h_low));
	mod->index = tr.numModels;
	tr.models[tr.numModels] = mod;
	tr.numModels++;

	return mod;
}

//=============================================================================

/*
===============
R_ModelInit
===============
*/
void R_ModelInit_plus()
{
	model_t *mod;

	// leave a space for NULL model
	tr.numModels = 0;

	mod = R_AllocModel_plus();
	mod->type = MOD_BAD;
}

/*
================
R_Modellist_f
================
*/
void R_Modellist_f_plus()
{
	int i, j;
	model_t *mod;
	int total;
	int lods;

	total = 0;
	for (i = 1; i < tr.numModels; i++)
	{
		mod = tr.models[i];
		lods = 1;
		for (j = 1; j < MD3_MAX_LODS; j++)
		{
			if (mod->md3[j] && mod->md3[j] != mod->md3[j - 1])
			{
				lods++;
			}
		}
		ri.Printf(PRINT_ALL, "%8i : (%i) %s\n", mod->dataSize, lods, mod->name);
		total += mod->dataSize;
	}
	ri.Printf(PRINT_ALL, "%8i : Total models\n", total);

#if 0 // not working right with new hunk
	if ( tr.world ) {
		ri.Printf( PRINT_ALL, "\n%8i : %s\n", tr.world->dataSize, tr.world->name );
	}
#endif
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
			Q_strncpyz_plus(dest->name, tag->name, sizeof(dest->name));

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

static md3Tag_t *R_GetTag( md3Header_t *mod, int frame, const char *tagName ) {
	md3Tag_t		*tag;
	int				i;

	if ( frame >= mod->numFrames ) {
		// it is possible to have a bad frame while changing models, so don't error
		frame = mod->numFrames - 1;
	}

	tag = (md3Tag_t *)((byte *)mod + mod->ofsTags) + frame * mod->numTags;
	for ( i = 0 ; i < mod->numTags ; i++, tag++ ) {
		if ( !strcmp( tag->name, tagName ) ) {
			return tag;	// found it
		}
	}

	return NULL;
}

int R_LerpTag_plus(orientation_t *tag, qhandle_t handle, int startFrame, int endFrame,
			  float frac, const char *tagName)
{
	md3Tag_t *start, *end;
	md3Tag_t start_space, end_space;
	int i;
	float frontLerp, backLerp;
	model_t *model;

	model = R_GetModelByHandle_plus(handle);
	if (!model->md3[0])
	{
		if (model->type == MOD_MDR)
		{
			start = R_GetAnimTag((mdrHeader_t *)model->modelData, startFrame, tagName, &start_space);
			end = R_GetAnimTag((mdrHeader_t *)model->modelData, endFrame, tagName, &end_space);
		}
		else if (model->type == MOD_IQM)
		{
			return R_IQMLerpTag_plus(tag, static_cast<iqmData_t *>(model->modelData),
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
		AxisClear_plus(tag->axis);
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
	VectorNormalize_plus(tag->axis[0]);
	VectorNormalize_plus(tag->axis[1]);
	VectorNormalize_plus(tag->axis[2]);
	return true;
}
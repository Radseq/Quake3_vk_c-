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


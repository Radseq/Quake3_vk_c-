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

#include "tr_model_iqm.hpp"

static int R_CullIQM(const iqmData_t *data, const trRefEntity_t *ent)
{
	vec3_t bounds[2];
	vec_t *oldBounds, *newBounds;
	int i;

	if (!data->bounds)
	{
		tr.pc.c_box_cull_md3_clip++;
		return CULL_CLIP;
	}

	// compute bounds pointers
	oldBounds = data->bounds + 6 * ent->e.oldframe;
	newBounds = data->bounds + 6 * ent->e.frame;

	// calculate a bounding box in the current coordinate system
	for (i = 0; i < 3; i++)
	{
		bounds[0][i] = oldBounds[i] < newBounds[i] ? oldBounds[i] : newBounds[i];
		bounds[1][i] = oldBounds[i + 3] > newBounds[i + 3] ? oldBounds[i + 3] : newBounds[i + 3];
	}

	switch (R_CullLocalBox_plus(bounds))
	{
	case CULL_IN:
		tr.pc.c_box_cull_md3_in++;
		return CULL_IN;
	case CULL_CLIP:
		tr.pc.c_box_cull_md3_clip++;
		return CULL_CLIP;
	case CULL_OUT:
	default:
		tr.pc.c_box_cull_md3_out++;
		return CULL_OUT;
	}
}

static int R_ComputeIQMFogNum(const iqmData_t *data, const trRefEntity_t *ent)
{
	int i, j;
	const fog_t *fog;
	const vec_t *bounds;
	const vec_t defaultBounds[6] = {-8, -8, -8, 8, 8, 8};
	vec3_t diag, center;
	vec3_t localOrigin;
	vec_t radius;

	if (tr.refdef.rdflags & RDF_NOWORLDMODEL)
	{
		return 0;
	}

	// FIXME: non-normalized axis issues
	if (data->bounds)
	{
		bounds = data->bounds + 6 * ent->e.frame;
	}
	else
	{
		bounds = defaultBounds;
	}
	VectorSubtract(bounds + 3, bounds, diag);
	VectorMA(bounds, 0.5f, diag, center);
	VectorAdd(ent->e.origin, center, localOrigin);
	radius = 0.5f * VectorLength(diag);

	for (i = 1; i < tr.world->numfogs; i++)
	{
		fog = &tr.world->fogs[i];
		for (j = 0; j < 3; j++)
		{
			if (localOrigin[j] - radius >= fog->bounds[1][j])
			{
				break;
			}
			if (localOrigin[j] + radius <= fog->bounds[0][j])
			{
				break;
			}
		}
		if (j == 3)
		{
			return i;
		}
	}

	return 0;
}

/*
=================
R_AddIQMSurfaces

Add all surfaces of this model
=================
*/
void R_AddIQMSurfaces_plus(trRefEntity_t *ent)
{
	iqmData_t *data;
	srfIQModel_t *surface;
	int i, j;
	bool personalModel;
	int cull;
	int fogNum;
	shader_t *shader;
	const skin_t *skin;

	data = static_cast<iqmData_t *>(tr.currentModel->modelData);
	surface = data->surfaces;

	// don't add third_person objects if not in a portal
	personalModel = (ent->e.renderfx & RF_THIRD_PERSON) && (tr.viewParms.portalView == PV_NONE);

	if (ent->e.renderfx & RF_WRAP_FRAMES)
	{
		ent->e.frame %= data->num_frames;
		ent->e.oldframe %= data->num_frames;
	}

	//
	// Validate the frames so there is no chance of a crash.
	// This will write directly into the entity structure, so
	// when the surfaces are rendered, they don't need to be
	// range checked again.
	//
	if ((ent->e.frame >= data->num_frames) || (ent->e.frame < 0) || (ent->e.oldframe >= data->num_frames) || (ent->e.oldframe < 0))
	{
		ri.Printf(PRINT_DEVELOPER, "R_AddIQMSurfaces: no such frame %d to %d for '%s'\n",
				  ent->e.oldframe, ent->e.frame,
				  tr.currentModel->name);
		ent->e.frame = 0;
		ent->e.oldframe = 0;
	}

	//
	// cull the entire model if merged bounding box of both frames
	// is outside the view frustum.
	//
	cull = R_CullIQM(data, ent);
	if (cull == CULL_OUT)
	{
		return;
	}

	//
	// set up lighting now that we know we aren't culled
	//
	if (!personalModel || r_shadows->integer > 1)
	{
		R_SetupEntityLighting_plus(&tr.refdef, ent);
	}

	//
	// see if we are in a fog volume
	//
	fogNum = R_ComputeIQMFogNum(data, ent);

	for (i = 0; i < data->num_surfaces; i++)
	{
		if (ent->e.customShader)
			shader = R_GetShaderByHandle_plus(ent->e.customShader);
		else if (ent->e.customSkin > 0 && ent->e.customSkin < tr.numSkins)
		{
			skin = R_GetSkinByHandle_plus(ent->e.customSkin);
			shader = tr.defaultShader;

			for (j = 0; j < skin->numSurfaces; j++)
			{
				if (!strcmp(skin->surfaces[j].name, surface->name))
				{
					shader = skin->surfaces[j].shader;
					break;
				}
			}
		}
		else
		{
			shader = surface->shader;
		}

		// we will add shadows even if the main object isn't visible in the view

		// stencil shadows can't do personal models unless I polyhedron clip
		if (!personalModel && r_shadows->integer == 2 && fogNum == 0 && !(ent->e.renderfx & (RF_NOSHADOW | RF_DEPTHHACK)) && shader->sort == SS_OPAQUE)
		{
			R_AddDrawSurf_plus(reinterpret_cast<surfaceType_t *>(surface), tr.shadowShader, 0, 0);
		}

		// projection shadows work fine with personal models
		if (r_shadows->integer == 3 && fogNum == 0 && (ent->e.renderfx & RF_SHADOW_PLANE) && shader->sort == SS_OPAQUE)
		{
			R_AddDrawSurf_plus(reinterpret_cast<surfaceType_t *>(surface), tr.projectionShadowShader, 0, 0);
		}

		if (!personalModel)
		{
			R_AddDrawSurf_plus(reinterpret_cast<surfaceType_t *>(surface), shader, fogNum, 0);
			tr.needScreenMap |= shader->hasScreenMap;
		}

		surface++;
	}
}
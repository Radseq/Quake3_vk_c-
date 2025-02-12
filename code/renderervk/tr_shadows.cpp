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
#include "tr_shadows.hpp"
#include "tr_backend.hpp"
#include "vk.hpp"
#include "utils.hpp"

/*

  for a projection shadow:

  point[x] += light vector * ( z - shadow plane )
  point[y] +=
  point[z] = shadow plane

  1 0 light[x] / light[z]

*/

typedef struct
{
	int i2;
	int facing;
} edgeDef_t;

constexpr int MAX_EDGE_DEFS = 32;

static edgeDef_t edgeDefs[SHADER_MAX_VERTEXES][MAX_EDGE_DEFS];
static int numEdgeDefs[SHADER_MAX_VERTEXES];
static int facing[SHADER_MAX_INDEXES / 3];

static void R_AddEdgeDef(const int i1, const int i2, const int f)
{
	int c;

	c = numEdgeDefs[i1];
	if (c == MAX_EDGE_DEFS)
	{
		return; // overflow
	}
	edgeDefs[i1][c].i2 = i2;
	edgeDefs[i1][c].facing = f;

	numEdgeDefs[i1]++;
}

static void R_CalcShadowEdges(void)
{
	bool sil_edge;
	int i;
	int c, c2;
	int j, k;
	int i2;

	tess.numIndexes = 0;

	// an edge is NOT a silhouette edge if its face doesn't face the light,
	// ort if it has a reverse paired edge that also faces the light.
	// A well behaved polyhedron would have exactly two faces for each edge,
	// but lots of models have dangling edges ort overfanned edges
	for (i = 0; i < tess.numVertexes; i++)
	{
		c = numEdgeDefs[i];
		for (j = 0; j < c; j++)
		{
			if (!edgeDefs[i][j].facing)
			{
				continue;
			}

			sil_edge = true;
			i2 = edgeDefs[i][j].i2;
			c2 = numEdgeDefs[i2];
			for (k = 0; k < c2; k++)
			{
				if (edgeDefs[i2][k].i2 == i && edgeDefs[i2][k].facing)
				{
					sil_edge = false;
					break;
				}
			}

			// if it doesn't share the edge with another front facing
			// triangle, it is a sil edge
			if (sil_edge)
			{
				if (tess.numIndexes > static_cast<int>(arrayLen(tess.indexes)) - 6)
				{
					i = tess.numVertexes;
					break;
				}
				tess.indexes[tess.numIndexes + 0] = i;
				tess.indexes[tess.numIndexes + 1] = i2;
				tess.indexes[tess.numIndexes + 2] = i + tess.numVertexes;
				tess.indexes[tess.numIndexes + 3] = i2;
				tess.indexes[tess.numIndexes + 4] = i2 + tess.numVertexes;
				tess.indexes[tess.numIndexes + 5] = i + tess.numVertexes;
				tess.numIndexes += 6;
			}
		}
	}

	tess.numVertexes *= 2;

	color4ub_t *colors = &tess.svars.colors[0][0]; // we need at least 2x SHADER_MAX_VERTEXES there

	for (i = 0; i < tess.numVertexes; i++)
	{
		Vector4Set(colors[i].rgba, 50, 50, 50, 255);
	}
}

/*
=================
RB_ShadowTessEnd

triangleFromEdge[ v1 ][ v2 ]


  set triangle from edge( v1, v2, tri )
  if ( facing[ triangleFromEdge[ v1 ][ v2 ] ] && !facing[ triangleFromEdge[ v2 ][ v1 ] ) {
  }
=================
*/
void RB_ShadowTessEnd(void)
{
	if (glConfig.stencilBits < 4)
	{
		return;
	}

	int i;
	int numTris;
	vec3_t lightDir{};
	uint32_t pipeline[2]{};

#ifdef USE_PMLIGHT
	if (r_dlightMode->integer == 2 && r_shadows->integer == 2)
		VectorCopy(backEnd.currentEntity->shadowLightDir, lightDir);
	else
#endif
		VectorCopy(backEnd.currentEntity->lightDir, lightDir);

	// clamp projection by height
	if (lightDir[2] > 0.1)
	{
		float s = 0.1 / lightDir[2];
		VectorScale(lightDir, s, lightDir);
	}

	// project vertexes away from light direction
	for (i = 0; i < tess.numVertexes; i++)
	{
		VectorMA(tess.xyz[i], -512, lightDir, tess.xyz[i + tess.numVertexes]);
	}

	// decide which triangles face the light
	Com_Memset(numEdgeDefs, 0, tess.numVertexes * sizeof(numEdgeDefs[0]));

	numTris = tess.numIndexes / 3;
	for (i = 0; i < numTris; i++)
	{
		int i1, i2, i3;
		vec3_t d1{}, d2{}, normal;
		float *v1, *v2, *v3;
		float d;

		i1 = tess.indexes[i * 3 + 0];
		i2 = tess.indexes[i * 3 + 1];
		i3 = tess.indexes[i * 3 + 2];

		v1 = tess.xyz[i1];
		v2 = tess.xyz[i2];
		v3 = tess.xyz[i3];

		VectorSubtract(v2, v1, d1);
		VectorSubtract(v3, v1, d2);
		CrossProduct(d1, d2, normal);

		d = DotProduct(normal, lightDir);
		if (d > 0)
		{
			facing[i] = 1;
		}
		else
		{
			facing[i] = 0;
		}

		// create the edges
		R_AddEdgeDef(i1, i2, facing[i]);
		R_AddEdgeDef(i2, i3, facing[i]);
		R_AddEdgeDef(i3, i1, facing[i]);
	}

	R_CalcShadowEdges();

	// draw the silhouette edges
	Bind(tr.whiteImage);

	// mirrors have the culling order reversed
	if (backEnd.viewParms.portalView == portalView_t::PV_MIRROR)
	{
		pipeline[0] = vk_inst.shadow_volume_pipelines[0][1];
		pipeline[1] = vk_inst.shadow_volume_pipelines[1][1];
	}
	else
	{
		pipeline[0] = vk_inst.shadow_volume_pipelines[0][0];
		pipeline[1] = vk_inst.shadow_volume_pipelines[1][0];
	}
	vk_bind_pipeline(pipeline[0]); // back-sided
	vk_bind_index();
	vk_bind_geometry(TESS_XYZ | TESS_RGBA0);
	vk_draw_geometry(Vk_Depth_Range::DEPTH_RANGE_NORMAL, true);
	vk_bind_pipeline(pipeline[1]); // front-sided
	vk_draw_geometry(Vk_Depth_Range::DEPTH_RANGE_NORMAL, true);

	tess.numVertexes /= 2;

	backEnd.doneShadows = true;

	tess.numIndexes = 0;
}

/*
=================
RB_ShadowFinish

Darken everything that is is a shadow volume.
We have to delay this until everything has been shadowed,
because otherwise shadows from different body parts would
overlap and double darken.
=================
*/
void RB_ShadowFinish(void)
{
	if (!backEnd.doneShadows)
	{
		return;
	}

	backEnd.doneShadows = false;

	if (r_shadows->integer != 2)
	{
		return;
	}
	if (glConfig.stencilBits < 4)
	{
		return;
	}

	float tmp[16];
	int i;
	static constexpr vec3_t verts[4] = {
		{-100, 100, -10},
		{100, 100, -10},
		{-100, -100, -10},
		{100, -100, -10} };

	Bind(tr.whiteImage);

	for (i = 0; i < 4; i++)
	{
		VectorCopy(verts[i], tess.xyz[i]);
		Vector4Set(tess.svars.colors[0][i].rgba, 153, 153, 153, 255);
	}

	tess.numVertexes = 4;

	Com_Memcpy(tmp, vk_world.modelview_transform, 64);
	Com_Memset(vk_world.modelview_transform, 0, 64);

	vk_world.modelview_transform[0] = 1.0f;
	vk_world.modelview_transform[5] = 1.0f;
	vk_world.modelview_transform[10] = 1.0f;
	vk_world.modelview_transform[15] = 1.0f;

	vk_bind_pipeline(vk_inst.shadow_finish_pipeline);

	vk_update_mvp(NULL);

	vk_bind_geometry(TESS_XYZ | TESS_RGBA0 /*| TESS_ST0 */);
	vk_draw_geometry(Vk_Depth_Range::DEPTH_RANGE_NORMAL, false);

	Com_Memcpy(vk_world.modelview_transform, tmp, 64);

	tess.numIndexes = 0;
	tess.numVertexes = 0;
}

/*
=================
RB_ProjectionShadowDeform

=================
*/
void RB_ProjectionShadowDeform(void)
{
	float *xyz;
	int i;
	float h;

	float groundDist;
	float d;
	vec3_t lightDir{};

	xyz = (float *)tess.xyz;

	vec3_t ground{
		backEnd.ort.axis[0][2],
		backEnd.ort.axis[1][2],
		backEnd.ort.axis[2][2]};

	groundDist = backEnd.ort.origin[2] - backEnd.currentEntity->e.shadowPlane;

#ifdef USE_PMLIGHT
	if (r_dlightMode->integer == 2 && r_shadows->integer == 2)
		VectorCopy(backEnd.currentEntity->shadowLightDir, lightDir);
	else
#endif
		VectorCopy(backEnd.currentEntity->lightDir, lightDir);

	d = DotProduct(lightDir, ground);
	// don't let the shadows get too long ort go negative
	if (d < 0.5)
	{
		VectorMA(lightDir, (0.5 - d), ground, lightDir);
		d = DotProduct(lightDir, ground);
	}
	d = 1.0 / d;

	vec3_t light{
		lightDir[0] * d,
		lightDir[1] * d,
		lightDir[2] * d};

	for (i = 0; i < tess.numVertexes; i++, xyz += 4)
	{
		h = DotProduct(xyz, ground) + groundDist;

		xyz[0] -= light[0] * h;
		xyz[1] -= light[1] * h;
		xyz[2] -= light[2] * h;
	}
}
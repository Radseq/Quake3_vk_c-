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
// tr_sky.c
#include "tr_sky.hpp"
#include "tr_backend.hpp"
#include "tr_shade.hpp"
#include "tr_surface.hpp"
#include "vk_vbo.hpp"
#include "vk.hpp"
#include "math.hpp"
#include "string_operations.hpp"
#include "compiler_defines.hpp"

constexpr int SKY_SUBDIVISIONS = 8;
constexpr int HALF_SKY_SUBDIVISIONS = (SKY_SUBDIVISIONS / 2);
constexpr float ON_EPSILON = 0.1f; // point on plane side epsilon
constexpr int MAX_CLIP_VERTS = 64;

static float s_cloudTexCoords[6][SKY_SUBDIVISIONS + 1][SKY_SUBDIVISIONS + 1][2];

/*
===================================================================================

POLYGON TO BOX SIDE PROJECTION

===================================================================================
*/

constexpr vec3_t sky_clip[6] = {
	{1, 1, 0},
	{1, -1, 0},
	{0, -1, 1},
	{0, 1, 1},
	{1, 0, 1},
	{-1, 0, 1}};

static float sky_mins[2][6], sky_maxs[2][6];
static float sky_min, sky_max;
static float sky_min_depth;

constexpr int sky_texorder[6] = {0, 2, 1, 3, 4, 5};
static vec3_t s_skyPoints[SKY_SUBDIVISIONS + 1][SKY_SUBDIVISIONS + 1];
static float s_skyTexCoords[SKY_SUBDIVISIONS + 1][SKY_SUBDIVISIONS + 1][2];

/*
================
AddSkyPolygon
================
*/
static void AddSkyPolygon(const int nump, vec3_t vecs)
{
	int i, j;
	vec3_t v{};
	float s, t, dv;
	int axis;
	float *vp;
	// s = [0]/[2], t = [1]/[2]
	static constexpr int vec_to_st[6][3] =
		{
			{-2, 3, 1},
			{2, 3, -1},

			{1, 3, 2},
			{-1, 3, -2},

			{-2, -1, 3},
			{-2, 1, -3}

			//	{-1,2,3},
			//	{1,2,-3}
		};

	// decide which face it maps to
	VectorCopy(vec3_origin, v);
	for (i = 0, vp = vecs; i < nump; i++, vp += 3)
	{
		VectorAdd(vp, v, v);
	}
	vec3_t av{
		fabs(v[0]),
		fabs(v[1]),
		fabs(v[2])};

	if (av[0] > av[1] && av[0] > av[2])
	{
		if (v[0] < 0)
			axis = 1;
		else
			axis = 0;
	}
	else if (av[1] > av[2] && av[1] > av[0])
	{
		if (v[1] < 0)
			axis = 3;
		else
			axis = 2;
	}
	else
	{
		if (v[2] < 0)
			axis = 5;
		else
			axis = 4;
	}

	// project new texture coords
	for (i = 0; i < nump; i++, vecs += 3)
	{
		j = vec_to_st[axis][2];
		if (j > 0)
			dv = vecs[j - 1];
		else
			dv = -vecs[-j - 1];
		if (dv < 0.001)
			continue; // don't divide by zero
		j = vec_to_st[axis][0];
		if (j < 0)
			s = -vecs[-j - 1] / dv;
		else
			s = vecs[j - 1] / dv;
		j = vec_to_st[axis][1];
		if (j < 0)
			t = -vecs[-j - 1] / dv;
		else
			t = vecs[j - 1] / dv;

		if (s < sky_mins[0][axis])
			sky_mins[0][axis] = s;
		if (t < sky_mins[1][axis])
			sky_mins[1][axis] = t;
		if (s > sky_maxs[0][axis])
			sky_maxs[0][axis] = s;
		if (t > sky_maxs[1][axis])
			sky_maxs[1][axis] = t;
	}
}

/*
================
ClipSkyPolygon
================
*/
static void ClipSkyPolygon(const int nump, vec3_t vecs, const int stage)
{
	const float *norm;
	float *v;
	bool front, back;
	float d, e;
	float dists[MAX_CLIP_VERTS]{};
	int sides[MAX_CLIP_VERTS]{};
	vec3_t newv[2][MAX_CLIP_VERTS]{};
	int newc[2]{};
	int i, j;

	if (nump > MAX_CLIP_VERTS - 2)
		ri.Error(ERR_DROP, "ClipSkyPolygon: MAX_CLIP_VERTS");
	if (stage == 6)
	{ // fully clipped, so draw it
		AddSkyPolygon(nump, vecs);
		return;
	}

	front = back = false;
	norm = sky_clip[stage];
	for (i = 0, v = vecs; i < nump; i++, v += 3)
	{
		d = DotProduct(v, norm);
		if (d > ON_EPSILON)
		{
			front = true;
			sides[i] = SIDE_FRONT;
		}
		else if (d < -ON_EPSILON)
		{
			back = true;
			sides[i] = SIDE_BACK;
		}
		else
			sides[i] = SIDE_ON;
		dists[i] = d;
	}

	if (!front || !back)
	{ // not clipped
		ClipSkyPolygon(nump, vecs, stage + 1);
		return;
	}

	// clip it
	sides[i] = sides[0];
	dists[i] = dists[0];
	VectorCopy(vecs, (vecs + (i * 3)));

	for (i = 0, v = vecs; i < nump; i++, v += 3)
	{
		switch (sides[i])
		{
		case SIDE_FRONT:
			VectorCopy(v, newv[0][newc[0]]);
			newc[0]++;
			break;
		case SIDE_BACK:
			VectorCopy(v, newv[1][newc[1]]);
			newc[1]++;
			break;
		case SIDE_ON:
			VectorCopy(v, newv[0][newc[0]]);
			newc[0]++;
			VectorCopy(v, newv[1][newc[1]]);
			newc[1]++;
			break;
		}

		if (sides[i] == SIDE_ON || sides[i + 1] == SIDE_ON || sides[i + 1] == sides[i])
			continue;

		d = dists[i] / (dists[i] - dists[i + 1]);
		for (j = 0; j < 3; j++)
		{
			e = v[j] + d * (v[j + 3] - v[j]);
			newv[0][newc[0]][j] = e;
			newv[1][newc[1]][j] = e;
		}
		newc[0]++;
		newc[1]++;
	}

	// continue
	ClipSkyPolygon(newc[0], newv[0][0], stage + 1);
	ClipSkyPolygon(newc[1], newv[1][0], stage + 1);
}

/*
==============
ClearSkyBox
==============
*/
static void ClearSkyBox(void)
{
	for (int i = 0; i < 6; i++)
	{
		sky_mins[0][i] = sky_mins[1][i] = 9999;
		sky_maxs[0][i] = sky_maxs[1][i] = -9999;
	}
}

/*
================
RB_ClipSkyPolygons
================
*/
static void RB_ClipSkyPolygons(const shaderCommands_t &input)
{
	vec3_t p[5]; // need one extra point for clipping
	int i, j;

	ClearSkyBox();

	for (i = 0; i < input.numIndexes; i += 3)
	{
		for (j = 0; j < 3; j++)
		{
			VectorSubtract(input.xyz[input.indexes[i + j]],
						   backEnd.viewParms.ort.origin,
						   p[j]);
		}
		ClipSkyPolygon(3, p[0], 0);
	}
}

/*
===================================================================================

CLOUD VERTEX GENERATION

===================================================================================
*/

/*
** MakeSkyVec
**
** Parms: s, t range from -1 to 1
*/
static void MakeSkyVec(float s, float t, int axis, vec3_t &outXYZ)
{
	// 1 = s, 2 = t, 3 = 2048
	static constexpr int st_to_vec[6][3] =
		{
			{3, -1, 2},
			{-3, 1, 2},

			{1, 3, 2},
			{-1, -3, 2},

			{-2, -1, 3}, // 0 degrees yaw, look straight up
			{2, -1, -3}	 // look straight down
		};

	int j, k;

	float boxSize = backEnd.viewParms.zFar / 1.75; // div sqrt(3)
	vec3_t b{
		s * boxSize,
		t * boxSize,
		boxSize};

	for (j = 0; j < 3; j++)
	{
		k = st_to_vec[axis][j];
		if (k < 0)
		{
			outXYZ[j] = -b[-k - 1];
		}
		else
		{
			outXYZ[j] = b[k - 1];
		}
	}
}

/*
=================
CullPoints
=================
*/
static bool CullPoints(const std::array<vec4_t, 4> &v)
{
	int i;
	std::size_t j;
	float dist;
	std::size_t count = v.size();

	for (i = 0; i < 5; i++)
	{
		const cplane_t &frust = backEnd.viewParms.frustum[i];
		for (j = 0; j < count; j++)
		{
			dist = DotProduct(v[j], frust.normal) - frust.dist;
			if (dist >= 0)
			{
				break;
			}
		}
		// all points are completely behind at least of one frustum plane
		if (j == count)
		{
			return true;
		}
	}

	return false;
}

static bool CullSkySide(const int mins[2], const int maxs[2])
{
	int s, t;
	std::array<vec4_t, 4> v;

	if (r_nocull->integer)
		return false;

	s = mins[0] + HALF_SKY_SUBDIVISIONS;
	t = mins[1] + HALF_SKY_SUBDIVISIONS;
	VectorAdd(s_skyPoints[t][s], backEnd.viewParms.ort.origin, v[0]);

	s = mins[0] + HALF_SKY_SUBDIVISIONS;
	t = maxs[1] + HALF_SKY_SUBDIVISIONS;
	VectorAdd(s_skyPoints[t][s], backEnd.viewParms.ort.origin, v[1]);

	s = maxs[0] + HALF_SKY_SUBDIVISIONS;
	t = mins[1] + HALF_SKY_SUBDIVISIONS;
	VectorAdd(s_skyPoints[t][s], backEnd.viewParms.ort.origin, v[2]);

	s = maxs[0] + HALF_SKY_SUBDIVISIONS;
	t = maxs[1] + HALF_SKY_SUBDIVISIONS;
	VectorAdd(s_skyPoints[t][s], backEnd.viewParms.ort.origin, v[3]);

	if (CullPoints(v))
		return true;

	return false;
}

static void FillSkySide(const int mins[2], const int maxs[2], float skyTexCoords[SKY_SUBDIVISIONS + 1][SKY_SUBDIVISIONS + 1][2])
{
	// Early cull: likely false, so help branch predictor
	if (CullSkySide(mins, maxs))
		return;

	const int vertexStart = tess.numVertexes;
	const int tHeight = maxs[1] - mins[1] + 1;
	const int sWidth = maxs[0] - mins[0] + 1;

	const int s0 = mins[0] + HALF_SKY_SUBDIVISIONS;
	const int s1 = maxs[0] + HALF_SKY_SUBDIVISIONS;
	const int t0 = mins[1] + HALF_SKY_SUBDIVISIONS;
	const int t1 = maxs[1] + HALF_SKY_SUBDIVISIONS;

#if ((SKY_SUBDIVISIONS + 1) * (SKY_SUBDIVISIONS + 1) * 6 > SHADER_MAX_VERTEXES)
	constexpr int maxVertices = SHADER_MAX_VERTEXES;
	const int vertCount = tHeight * sWidth;

	if (tess.numVertexes + vertCount > maxVertices)
	{
		ri.Error(ERR_DROP, "SHADER_MAX_VERTEXES hit in %s()", __func__);
	}
#endif

#if (SKY_SUBDIVISIONS * SKY_SUBDIVISIONS * 6 * 6 > SHADER_MAX_INDEXES)
	constexpr int maxIndexes = SHADER_MAX_INDEXES;
	const int indexCount = (tHeight - 1) * (sWidth - 1) * 6;

	if (tess.numIndexes + indexCount > maxIndexes)
	{
		ri.Error(ERR_DROP, "SHADER_MAX_INDEXES hit in %s()", __func__);
	}
#endif

	// ===== VERTEX GENERATION =====
	for (int t = t0; t <= t1; ++t)
	{
		for (int s = s0; s <= s1; ++s)
		{
			const int idx = tess.numVertexes;

			// Inline VectorAdd for performance
			const float *skyPt = s_skyPoints[t][s];
			const float *camPt = backEnd.viewParms.ort.origin;

			float *dst = tess.xyz[idx];
			dst[0] = skyPt[0] + camPt[0];
			dst[1] = skyPt[1] + camPt[1];
			dst[2] = skyPt[2] + camPt[2];

			// Copy texture coordinates
			float *tex = tess.texCoords[0][idx];
			tex[0] = skyTexCoords[t][s][0];
			tex[1] = skyTexCoords[t][s][1];

			tess.numVertexes++;
		}
	}

	// ===== INDEX GENERATION =====
	const int rowSize = sWidth;
	for (int t = 0; t < tHeight - 1; ++t)
	{
		for (int s = 0; s < sWidth - 1; ++s)
		{
			const int i0 = vertexStart + s + t * rowSize;
			const int i1 = vertexStart + s + (t + 1) * rowSize;
			const int i2 = vertexStart + s + 1 + t * rowSize;
			const int i3 = vertexStart + s + 1 + (t + 1) * rowSize;

			auto &idx = tess.indexes;
			int &ni = tess.numIndexes;

			idx[ni++] = i0;
			idx[ni++] = i1;
			idx[ni++] = i2;

			idx[ni++] = i1;
			idx[ni++] = i3;
			idx[ni++] = i2;
		}
	}
}

static void DrawSkySide(image_t *image, const int mins[2], const int maxs[2])
{
	tess.numVertexes = 0;
	tess.numIndexes = 0;

	FillSkySide(mins, maxs, s_skyTexCoords);

	if (tess.numIndexes)
	{
		Bind(image);
		tess.svars.texcoordPtr[0] = tess.texCoords[0];

		vk_bind_pipeline(vk_inst.skybox_pipeline);
		vk_bind_index();
		vk_bind_geometry(TESS_XYZ | TESS_ST0);
		vk_draw_geometry(r_showsky->integer ? Vk_Depth_Range::DEPTH_RANGE_ZERO : Vk_Depth_Range::DEPTH_RANGE_ONE, true);
		tess.numVertexes = 0;
		tess.numIndexes = 0;
	}
}

static void DrawSkyBox(const shader_t &shader)
{
	sky_min = 0;
	sky_max = 1;

	constexpr float invSubd = 1.0f / HALF_SKY_SUBDIVISIONS;

	for (int i = 0; i < 6; ++i)
	{
		// Compute clamped float ranges
		float minS = std::floor(sky_mins[0][i] * HALF_SKY_SUBDIVISIONS) * invSubd;
		float minT = std::floor(sky_mins[1][i] * HALF_SKY_SUBDIVISIONS) * invSubd;
		float maxS = std::ceil(sky_maxs[0][i] * HALF_SKY_SUBDIVISIONS) * invSubd;
		float maxT = std::ceil(sky_maxs[1][i] * HALF_SKY_SUBDIVISIONS) * invSubd;

		if (minS >= maxS || minT >= maxT)
			continue;

		ri.Printf(PRINT_ALL, "a = false\n");
		// Convert to integer subdivision bounds
		int mins[2] = {
			std::clamp(static_cast<int>(minS * HALF_SKY_SUBDIVISIONS), -HALF_SKY_SUBDIVISIONS, HALF_SKY_SUBDIVISIONS),
			std::clamp(static_cast<int>(minT * HALF_SKY_SUBDIVISIONS), -HALF_SKY_SUBDIVISIONS, HALF_SKY_SUBDIVISIONS)};
		int maxs[2] = {
			std::clamp(static_cast<int>(maxS * HALF_SKY_SUBDIVISIONS), -HALF_SKY_SUBDIVISIONS, HALF_SKY_SUBDIVISIONS),
			std::clamp(static_cast<int>(maxT * HALF_SKY_SUBDIVISIONS), -HALF_SKY_SUBDIVISIONS, HALF_SKY_SUBDIVISIONS)};

		const int t0 = mins[1] + HALF_SKY_SUBDIVISIONS;
		const int t1 = maxs[1] + HALF_SKY_SUBDIVISIONS;
		const int s0 = mins[0] + HALF_SKY_SUBDIVISIONS;
		const int s1 = maxs[0] + HALF_SKY_SUBDIVISIONS;

		for (int t = t0; t <= t1; ++t)
		{
			float ft = (t - HALF_SKY_SUBDIVISIONS) * invSubd;
			for (int s = s0; s <= s1; ++s)
			{
				float fs = (s - HALF_SKY_SUBDIVISIONS) * invSubd;
				MakeSkyVec(fs, ft, i, s_skyPoints[t][s]);
			}
		}

		DrawSkySide(shader.sky.outerbox[sky_texorder[i]], mins, maxs);
	}
}

static constexpr float GetMinT(int i)
{
	switch (i)
	{
	case 0:
	case 1:
	case 2:
	case 3:
		return -1.0f;
	case 5:
		return 0.0f;
	case 4: // top
	default:
		return -HALF_SKY_SUBDIVISIONS;
	}
}

static void FillCloudBox()
{
	constexpr float invSubd = 1.0f / HALF_SKY_SUBDIVISIONS;
	constexpr int maxSubd = HALF_SKY_SUBDIVISIONS;

	for (int i = 0; i < 6; ++i)
	{
		// Skip bottom side (face 5)
		if (i == 5)
			continue;

		const float MIN_T = -HALF_SKY_SUBDIVISIONS;

		// if (1) // FIXME? shader->sky.fullClouds )
		//{
		//	MIN_T = -HALF_SKY_SUBDIVISIONS;
		// }
		// else
		//{
		//	MIN_T = GetMinT(i);
		// }

		// Compute precise min/max with subdivision rounding
		const float minS = std::floor(sky_mins[0][i] * maxSubd) * invSubd;
		const float minT = std::floor(sky_mins[1][i] * maxSubd) * invSubd;
		const float maxS = std::ceil(sky_maxs[0][i] * maxSubd) * invSubd;
		const float maxT = std::ceil(sky_maxs[1][i] * maxSubd) * invSubd;

		if (minS >= maxS || minT >= maxT)
			continue;

		// Convert to int subdivisions
		int mins[2] = {
			std::clamp(static_cast<int>(minS * maxSubd), -maxSubd, maxSubd),
			std::clamp(static_cast<int>(minT * maxSubd), static_cast<int>(MIN_T), maxSubd)};
		int maxs[2] = {
			std::clamp(static_cast<int>(maxS * maxSubd), -maxSubd, maxSubd),
			std::clamp(static_cast<int>(maxT * maxSubd), static_cast<int>(MIN_T), maxSubd)};

		// Precompute loop bounds
		const int t0 = mins[1] + maxSubd;
		const int t1 = maxs[1] + maxSubd;
		const int s0 = mins[0] + maxSubd;
		const int s1 = maxs[0] + maxSubd;

		// Fill vertex positions
		for (int t = t0; t <= t1; ++t)
		{
			const float ft = (t - maxSubd) * invSubd;
			for (int s = s0; s <= s1; ++s)
			{
				const float fs = (s - maxSubd) * invSubd;
				MakeSkyVec(fs, ft, i, s_skyPoints[t][s]);
			}
		}

		// Fill vertex data for rendering
		FillSkySide(mins, maxs, s_cloudTexCoords[i]);
	}
}

/*
** R_BuildCloudData
*/
static void R_BuildCloudData(const shaderCommands_t &input)
{
	const shader_t &shader = *input.shader;

	sky_min = 1.0 / 256.0f; // FIXME: not correct?
	sky_max = 255.0 / 256.0f;

	// set up for drawing
	tess.numIndexes = 0;
	tess.numVertexes = 0;

	if (shader.sky.cloudHeight)
	{
		if (tess.xstages[0])
		{
			FillCloudBox();
		}
	}
}

static void BuildSkyTexCoords()
{
	constexpr float invHalf = 1.0f / HALF_SKY_SUBDIVISIONS;

	for (int i = 0; i <= SKY_SUBDIVISIONS; ++i)
	{
		const float ti = (i - HALF_SKY_SUBDIVISIONS) * invHalf;
		float t = std::clamp((ti + 1.0f) * 0.5f, 0.0f, 1.0f);
		t = 1.0f - t; // flip vertically once, not inside clamp

		for (int j = 0; j <= SKY_SUBDIVISIONS; ++j)
		{
			const float sj = (j - HALF_SKY_SUBDIVISIONS) * invHalf;
			float s = std::clamp((sj + 1.0f) * 0.5f, 0.0f, 1.0f);

			s_skyTexCoords[i][j][0] = s;
			s_skyTexCoords[i][j][1] = t;
		}
	}
}

/*
** R_InitSkyTexCoords
** Called when a sky shader is parsed
*/
void R_InitSkyTexCoords(float heightCloud)
{
	constexpr float radiusWorld = 4096.0f;
	constexpr float invHalf = 1.0f / HALF_SKY_SUBDIVISIONS;
	constexpr int maxS = SKY_SUBDIVISIONS + 1;

	vec3_t skyVec;
	vec3_t v;

	if (!Q_stricmp_cpp(glConfig.renderer_string, "GDI Generic") &&
		!Q_stricmp_cpp(glConfig.version_string, "1.1.0"))
	{
		// fix skybox rendering on MS software GL implementation
		sky_min_depth = 0.999f;
	}
	else
	{
		sky_min_depth = 1.0f;
	}

	// Ensure MakeSkyVec works
	backEnd.viewParms.zFar = 1024.0f;

	for (int i = 0; i < 6; ++i)
	{
		for (int t = 0; t < maxS; ++t)
		{
			const float ft = (t - HALF_SKY_SUBDIVISIONS) * invHalf;

			for (int s = 0; s < maxS; ++s)
			{
				const float fs = (s - HALF_SKY_SUBDIVISIONS) * invHalf;

				MakeSkyVec(fs, ft, i, skyVec);

				const float dot = DotProduct(skyVec, skyVec);

				const float p = (1.0f / (2.0f * dot)) *
								(-2.0f * skyVec[2] * radiusWorld +
								 2.0f * std::sqrt(
											Square(skyVec[2]) * Square(radiusWorld) +
											2.0f * Square(skyVec[0]) * radiusWorld * heightCloud +
											Square(skyVec[0]) * Square(heightCloud) +
											2.0f * Square(skyVec[1]) * radiusWorld * heightCloud +
											Square(skyVec[1]) * Square(heightCloud) +
											2.0f * Square(skyVec[2]) * radiusWorld * heightCloud +
											Square(skyVec[2]) * Square(heightCloud)));

				VectorScale(skyVec, p, v);
				v[2] += radiusWorld;

				VectorNormalize(v);

				const float sRad = Q_acos(v[0]);
				const float tRad = Q_acos(v[1]);

				s_cloudTexCoords[i][t][s][0] = sRad;
				s_cloudTexCoords[i][t][s][1] = tRad;
			}
		}
	}

	BuildSkyTexCoords();
}

//======================================================================================

/*
** RB_DrawSun
*/
void RB_DrawSun(const float scale, shader_t &shader)
{
	if (!backEnd.skyRenderedThisView)
		return;

	float size;
	float dist;
	vec3_t origin{}, vec1, vec2;
	color4ub_t sunColor{};

	sunColor.u32 = ~0U;
	vk_update_mvp(NULL);

	dist = backEnd.viewParms.zFar / 1.75; // div sqrt(3)
	size = dist * scale;

	VectorMA(backEnd.viewParms.ort.origin, dist, tr.sunDirection, origin);
	PerpendicularVector(vec1, tr.sunDirection);
	CrossProduct(tr.sunDirection, vec1, vec2);

	VectorScale(vec1, size, vec1);
	VectorScale(vec2, size, vec2);

	// farthest depth range
	tess.depthRange = Vk_Depth_Range::DEPTH_RANGE_ONE;

	RB_BeginSurface(shader, 0);

	RB_AddQuadStamp(origin, vec1, vec2, sunColor);

	RB_EndSurface();

	// back to normal depth range
	tess.depthRange = Vk_Depth_Range::DEPTH_RANGE_NORMAL;
}

/*
================
RB_StageIteratorSky

All of the visible sky triangles are in tess

Other things could be stuck in here, like birds in the sky, etc
================
*/
void RB_StageIteratorSky(void)
{
#if !defined (USE_BUFFER_CLEAR)
	if ( r_fastsky->integer && vk.clearAttachment ) {
#else
	if ( r_fastsky->integer ) {
#endif
		return;
	}

#ifdef USE_VBO
	VBO_UnBind();
#endif

	// go through all the polygons and project them onto
	// the sky box to see which blocks on each side need
	// to be drawn
	RB_ClipSkyPolygons(tess);

	// r_showsky will let all the sky blocks be drawn in
	// front of everything to allow developers to see how
	// much sky is getting sucked in

	if (r_showsky->integer)
	{
		tess.depthRange = Vk_Depth_Range::DEPTH_RANGE_ZERO;
	}
	else
	{
		tess.depthRange = Vk_Depth_Range::DEPTH_RANGE_ONE;
	}

	// draw the outer skybox
	if (tess.shader->sky.outerbox[0] && tess.shader->sky.outerbox[0] != tr.defaultImage)
	{
		DrawSkyBox(*tess.shader);
	}

	// generate the vertexes for all the clouds, which will be drawn
	// by the generic shader routine
	R_BuildCloudData(tess);

	// draw the inner skybox
	if (tess.numVertexes)
	{
		RB_StageIteratorGeneric();
	}

	// back to normal depth range
	tess.depthRange = Vk_Depth_Range::DEPTH_RANGE_NORMAL;

	// note that sky was drawn so we will draw a sun later
	backEnd.skyRenderedThisView = true;
}
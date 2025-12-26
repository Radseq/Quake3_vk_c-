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

#include "tr_curve.hpp"
#include "math.hpp"

/*

This file does all of the processing necessary to turn a raw grid of points
read from the map file into a srfGridMesh_t ready for rendering.

The level of detail solution is direction independent, based only on subdivided
distance from the true curve.

Only a single entry point:

srfGridMesh_t *R_SubdividePatchToGrid( int width, int height,
								drawVert_t points[MAX_PATCH_SIZE*MAX_PATCH_SIZE] ) {

*/

/*
============
LerpDrawVert
============
*/
static void LerpDrawVert(const drawVert_t& a, const drawVert_t& b, drawVert_t& out) noexcept
{
	constexpr float kHalf = 0.5f;

	out.xyz[0] = (a.xyz[0] + b.xyz[0]) * kHalf;
	out.xyz[1] = (a.xyz[1] + b.xyz[1]) * kHalf;
	out.xyz[2] = (a.xyz[2] + b.xyz[2]) * kHalf;

	out.st[0] = (a.st[0] + b.st[0]) * kHalf;
	out.st[1] = (a.st[1] + b.st[1]) * kHalf;

	out.lightmap[0] = (a.lightmap[0] + b.lightmap[0]) * kHalf;
	out.lightmap[1] = (a.lightmap[1] + b.lightmap[1]) * kHalf;

	std::uint32_t ca, cb;
	std::memcpy(&ca, a.color.rgba, 4);
	std::memcpy(&cb, b.color.rgba, 4);

	constexpr std::uint32_t mask = 0xFEFEFEFEu;
	const std::uint32_t avg = (ca & cb) + (((ca ^ cb) & mask) >> 1);

	std::memcpy(out.color.rgba, &avg, 4);
}

/*
============
Transpose
============
*/
static void Transpose(const int width, const int height,
	drawVert_t ctrl[MAX_GRID_SIZE][MAX_GRID_SIZE]) noexcept
{
	// Square: in-place transpose
	if (width == height)
	{
		for (int i = 0; i < width; ++i)
		{
			for (int j = i + 1; j < width; ++j)
			{
				const drawVert_t tmp = ctrl[i][j];
				ctrl[i][j] = ctrl[j][i];
				ctrl[j][i] = tmp;
			}
		}
		return;
	}

	const int m = (width < height) ? width : height;

	// Swap overlap square region
	for (int i = 0; i < m; ++i)
	{
		for (int j = i + 1; j < m; ++j)
		{
			const drawVert_t tmp = ctrl[i][j];
			ctrl[i][j] = ctrl[j][i];
			ctrl[j][i] = tmp;
		}
	}

	// Copy the "tail"
	if (width > height)
	{
		for (int i = 0; i < height; ++i)
		{
			for (int j = height; j < width; ++j)
			{
				ctrl[j][i] = ctrl[i][j];
			}
		}
	}
	else // height > width
	{
		for (int i = 0; i < width; ++i)
		{
			for (int j = width; j < height; ++j)
			{
				ctrl[i][j] = ctrl[j][i];
			}
		}
	}
}

/*
=================
MakeMeshNormals

Handles all the complicated wrapping and degenerate cases
=================
*/
static void MakeMeshNormals(const int width, const int height, drawVert_t ctrl[MAX_GRID_SIZE][MAX_GRID_SIZE])
{
	int i, j, k, dist;
	vec3_t normal;
	vec3_t base{};
	vec3_t delta{};
	int x, y;
	vec3_t around[8]{}, temp{};
	bool good[8]{};
	bool wrapWidth, wrapHeight;
	float len;
	static constexpr int neighbors[8][2] = {
		{0, 1}, {1, 1}, {1, 0}, {1, -1}, {0, -1}, {-1, -1}, {-1, 0}, {-1, 1}};

	wrapWidth = false;
	for (i = 0; i < height; i++)
	{
		VectorSubtract(ctrl[i][0].xyz, ctrl[i][width - 1].xyz, delta);
		len = VectorLengthSquared(delta);
		if (len > 1.0)
		{
			break;
		}
	}
	if (i == height)
	{
		wrapWidth = true;
	}

	wrapHeight = false;
	for (i = 0; i < width; i++)
	{
		VectorSubtract(ctrl[0][i].xyz, ctrl[height - 1][i].xyz, delta);
		len = VectorLengthSquared(delta);
		if (len > 1.0)
		{
			break;
		}
	}
	if (i == width)
	{
		wrapHeight = true;
	}

	for (i = 0; i < width; i++)
	{
		for (j = 0; j < height; j++)
		{
			drawVert_t &dv = ctrl[j][i];
			VectorCopy(dv.xyz, base);
			for (k = 0; k < 8; k++)
			{
				VectorClear(around[k]);
				good[k] = false;

				for (dist = 1; dist <= 3; dist++)
				{
					x = i + neighbors[k][0] * dist;
					y = j + neighbors[k][1] * dist;
					if (wrapWidth)
					{
						if (x < 0)
						{
							x = width - 1 + x;
						}
						else if (x >= width)
						{
							x = 1 + x - width;
						}
					}
					if (wrapHeight)
					{
						if (y < 0)
						{
							y = height - 1 + y;
						}
						else if (y >= height)
						{
							y = 1 + y - height;
						}
					}

					if (x < 0 || x >= width || y < 0 || y >= height)
					{
						break; // edge of patch
					}
					VectorSubtract(ctrl[y][x].xyz, base, temp);
					if (VectorNormalize(temp) < 0.001f)
					{
						continue; // degenerate edge, get more dist
					}
					else
					{
						good[k] = true;
						VectorCopy(temp, around[k]);
						break; // good edge
					}
				}
			}

			vec3_t sum{};
			for (k = 0; k < 8; k++)
			{
				if (!good[k] || !good[(k + 1) & 7])
				{
					continue; // didn't get two points
				}
				CrossProduct(around[(k + 1) & 7], around[k], normal);
				if (VectorNormalize(normal) < 0.001f)
				{
					continue;
				}
				VectorAdd(normal, sum, sum);
			}

			VectorNormalize2(sum, dv.normal);
			for ( k = 0; k < 3; k++ ) {
				dv.normal[k] = R_ClampDenorm( dv.normal[k] );
			}
		}
	}
}

/*
============
InvertCtrl
============
*/
static void InvertCtrl(const int width, const int height, drawVert_t ctrl[MAX_GRID_SIZE][MAX_GRID_SIZE])
{
	int i, j;
	drawVert_t temp;

	for (i = 0; i < height; i++)
	{
		for (j = 0; j < width / 2; j++)
		{
			temp = ctrl[i][j];
			ctrl[i][j] = ctrl[i][width - 1 - j];
			ctrl[i][width - 1 - j] = temp;
		}
	}
}

/*
=================
InvertErrorTable
=================
*/
static void InvertErrorTable(float errorTable[2][MAX_GRID_SIZE], const int width, const int height)
{
	int i;
	float copy[2][MAX_GRID_SIZE];

	Com_Memcpy(copy, errorTable, sizeof(copy));

	for (i = 0; i < width; i++)
	{
		errorTable[1][i] = copy[0][i]; //[width-1-i];
	}

	for (i = 0; i < height; i++)
	{
		errorTable[0][i] = copy[1][height - 1 - i];
	}
}

/*
==================
PutPointsOnCurve
==================
*/
static void PutPointsOnCurve(drawVert_t ctrl[MAX_GRID_SIZE][MAX_GRID_SIZE],
							 const int width, const int height)
{
	int i, j;
	drawVert_t prev, next;

	for (i = 0; i < width; i++)
	{
		for (j = 1; j < height; j += 2)
		{
			LerpDrawVert(ctrl[j][i], ctrl[j + 1][i], prev);
			LerpDrawVert(ctrl[j][i], ctrl[j - 1][i], next);
			LerpDrawVert(prev, next, ctrl[j][i]);
		}
	}

	for (j = 0; j < height; j++)
	{
		for (i = 1; i < width; i += 2)
		{
			LerpDrawVert(ctrl[j][i], ctrl[j][i + 1], prev);
			LerpDrawVert(ctrl[j][i], ctrl[j][i - 1], next);
			LerpDrawVert(prev, next, ctrl[j][i]);
		}
	}
}

/*
=================
R_CreateSurfaceGridMesh
=================
*/
static srfGridMesh_t *R_CreateSurfaceGridMesh(const int width, const int height,
											  drawVert_t ctrl[MAX_GRID_SIZE][MAX_GRID_SIZE], float errorTable[2][MAX_GRID_SIZE])
{
	int i, j, size;
	drawVert_t *vert;
	vec3_t tmpVec{};
	srfGridMesh_t *grid;

	// copy the results out to a grid
	size = (width * height - 1) * sizeof(drawVert_t) + sizeof(*grid);

#ifdef PATCH_STITCHING
	grid = static_cast<srfGridMesh_t *>(ri.Malloc(size)) /*ri.Hunk_Alloc*/;
	Com_Memset(grid, 0, size);

	grid->widthLodError = static_cast<float *>(/*ri.Hunk_Alloc*/ ri.Malloc(width * 4));
	Com_Memcpy(grid->widthLodError, errorTable[0], width * 4);

	grid->heightLodError = /*ri.Hunk_Alloc*/ static_cast<float *>(ri.Malloc(height * 4));
	Com_Memcpy(grid->heightLodError, errorTable[1], height * 4);
#else
	grid = ri.Hunk_Alloc(size);
	Com_Memset(grid, 0, size);

	grid->widthLodError = ri.Hunk_Alloc(width * 4);
	Com_Memcpy(grid->widthLodError, errorTable[0], width * 4);

	grid->heightLodError = ri.Hunk_Alloc(height * 4);
	Com_Memcpy(grid->heightLodError, errorTable[1], height * 4);
#endif

	grid->width = width;
	grid->height = height;
	grid->surfaceType = surfaceType_t::SF_GRID;
	ClearBounds_cpp(grid->meshBounds[0], grid->meshBounds[1]);
	for (i = 0; i < width; i++)
	{
		for (j = 0; j < height; j++)
		{
			vert = &grid->verts[j * width + i];
			*vert = ctrl[j][i];
			AddPointToBounds(vert->xyz, grid->meshBounds[0], grid->meshBounds[1]);
		}
	}

	// compute local origin and bounds
	VectorAdd(grid->meshBounds[0], grid->meshBounds[1], grid->localOrigin);
	VectorScale(grid->localOrigin, 0.5f, grid->localOrigin);
	VectorSubtract(grid->meshBounds[0], grid->localOrigin, tmpVec);
	grid->meshRadius = VectorLength(tmpVec);

	VectorCopy(grid->localOrigin, grid->lodOrigin);
	grid->lodRadius = grid->meshRadius;
	//
	return grid;
}

void R_FreeSurfaceGridMesh(srfGridMesh_t &grid)
{
	ri.Free(grid.widthLodError);
	ri.Free(grid.heightLodError);
	ri.Free(&grid);
}

/*
=================
R_SubdividePatchToGrid
=================
*/
srfGridMesh_t *R_SubdividePatchToGrid(int width, int height,
	std::span<const drawVert_t> points)
{
	int i, j, k, l;
	drawVert_t prev{};
	drawVert_t next{};
	drawVert_t mid{};
	float len, maxLen;
	int n;
	int t;
	drawVert_t ctrl[MAX_GRID_SIZE][MAX_GRID_SIZE]{};
	//std::array<std::array<drawVert_t, MAX_GRID_SIZE>, MAX_GRID_SIZE> ctrl{};
	float errorTable[2][MAX_GRID_SIZE]{};

	for (i = 0; i < width; i++)
	{
		for (j = 0; j < height; j++)
		{
			ctrl[j][i] = points[j * width + i];
		}
	}

	for (n = 0; n < 2; n++)
	{

		for (j = 0; j < MAX_GRID_SIZE; j++)
		{
			errorTable[n][j] = 0;
		}

		// horizontal subdivisions
		for (j = 0; j + 2 < width; j += 2)
		{
			// check subdivided midpoints against control points

			// FIXME: also check midpoints of adjacent patches against the control points
			// this would basically stitch all patches in the same LOD group together.

			maxLen = 0;
			for (i = 0; i < height; i++)
			{
				vec3_t midxyz{};
				vec3_t midxyz2{};
				vec3_t dir{};
				vec3_t projected{};
				float d;

				// calculate the point on the curve
				for (l = 0; l < 3; l++)
				{
					midxyz[l] = (ctrl[i][j].xyz[l] + ctrl[i][j + 1].xyz[l] * 2 + ctrl[i][j + 2].xyz[l]) * 0.25f;
				}

				// see how far off the line it is
				// using dist-from-line will not account for internal
				// texture warping, but it gives a lot less polygons than
				// dist-from-midpoint
				VectorSubtract(midxyz, ctrl[i][j].xyz, midxyz);
				VectorSubtract(ctrl[i][j + 2].xyz, ctrl[i][j].xyz, dir);
				VectorNormalize(dir);

				d = DotProduct(midxyz, dir);
				VectorScale(dir, d, projected);
				VectorSubtract(midxyz, projected, midxyz2);
				len = VectorLengthSquared(midxyz2); // we will do the sqrt later
				if (len > maxLen)
				{
					maxLen = len;
				}
			}

			maxLen = sqrtf(maxLen);

			// if all the points are on the lines, remove the entire columns
			if (maxLen < 0.1f)
			{
				errorTable[n][j + 1] = 999;
				continue;
			}

			// see if we want to insert subdivided columns
			if (width + 2 > MAX_GRID_SIZE)
			{
				errorTable[n][j + 1] = 1.0f / maxLen;
				continue; // can't subdivide any more
			}

			if (maxLen <= r_subdivisions->value)
			{
				errorTable[n][j + 1] = 1.0f / maxLen;
				continue; // didn't need subdivision
			}

			errorTable[n][j + 2] = 1.0f / maxLen;

			// insert two columns and replace the peak
			width += 2;
			for (i = 0; i < height; i++)
			{
				LerpDrawVert(ctrl[i][j], ctrl[i][j + 1], prev);
				LerpDrawVert(ctrl[i][j + 1], ctrl[i][j + 2], next);
				LerpDrawVert(prev, next, mid);

				for (k = width - 1; k > j + 3; k--)
				{
					ctrl[i][k] = ctrl[i][k - 2];
				}
				ctrl[i][j + 1] = prev;
				ctrl[i][j + 2] = mid;
				ctrl[i][j + 3] = next;
			}

			// back up and recheck this set again, it may need more subdivision
			j -= 2;
		}

		Transpose(width, height, ctrl);
		t = width;
		width = height;
		height = t;
	}

	// put all the approximating points on the curve
	PutPointsOnCurve(ctrl, width, height);

	// cull out any rows or columns that are colinear
	for (i = 1; i < width - 1; i++)
	{
		if (errorTable[0][i] != 999)
		{
			continue;
		}
		for (j = i + 1; j < width; j++)
		{
			for (k = 0; k < height; k++)
			{
				ctrl[k][j - 1] = ctrl[k][j];
			}
			errorTable[0][j - 1] = errorTable[0][j];
		}
		width--;
	}

	for (i = 1; i < height - 1; i++)
	{
		if (errorTable[1][i] != 999)
		{
			continue;
		}
		for (j = i + 1; j < height; j++)
		{
			for (k = 0; k < width; k++)
			{
				ctrl[j - 1][k] = ctrl[j][k];
			}
			errorTable[1][j - 1] = errorTable[1][j];
		}
		height--;
	}

#if 1
	// flip for longest tristrips as an optimization
	// the results should be visually identical with or
	// without this step
	if (height > width)
	{
		Transpose(width, height, ctrl);
		InvertErrorTable(errorTable, width, height);
		t = width;
		width = height;
		height = t;
		InvertCtrl(width, height, ctrl);
	}
#endif

	// calculate normals
	MakeMeshNormals(width, height, ctrl);

	return R_CreateSurfaceGridMesh(width, height, ctrl, errorTable);
}

/*
===============
R_GridInsertColumn
===============
*/
srfGridMesh_t *R_GridInsertColumn(srfGridMesh_t &grid, const int column, const int row, const vec3_t &point, const float loderror)
{
	int i, j;
	int width, height, oldwidth;
	drawVert_t ctrl[MAX_GRID_SIZE][MAX_GRID_SIZE]{};
	float errorTable[2][MAX_GRID_SIZE]{};
	float lodRadius;
	vec3_t lodOrigin{};

	oldwidth = 0;
	width = grid.width + 1;
	if (width > MAX_GRID_SIZE)
		return NULL;
	height = grid.height;
	for (i = 0; i < width; i++)
	{
		if (i == column)
		{
			// insert new column
			for (j = 0; j < grid.height; j++)
			{
				LerpDrawVert(grid.verts[j * grid.width + i - 1], grid.verts[j * grid.width + i], ctrl[j][i]);
				if (j == row)
					VectorCopy(point, ctrl[j][i].xyz);
			}
			errorTable[0][i] = loderror;
			continue;
		}
		errorTable[0][i] = grid.widthLodError[oldwidth];
		for (j = 0; j < grid.height; j++)
		{
			ctrl[j][i] = grid.verts[j * grid.width + oldwidth];
		}
		oldwidth++;
	}
	for (j = 0; j < grid.height; j++)
	{
		errorTable[1][j] = grid.heightLodError[j];
	}
	// put all the approximating points on the curve
	// PutPointsOnCurve( ctrl, width, height );
	// calculate normals
	MakeMeshNormals(width, height, ctrl);

	VectorCopy(grid.lodOrigin, lodOrigin);
	lodRadius = grid.lodRadius;
	// free the old grid
	R_FreeSurfaceGridMesh(grid);
	// create a new grid
	grid = *R_CreateSurfaceGridMesh(width, height, ctrl, errorTable);
	grid.lodRadius = lodRadius;
	VectorCopy(lodOrigin, grid.lodOrigin);
	return &grid;
}

/*
===============
R_GridInsertRow
===============
*/
srfGridMesh_t *R_GridInsertRow(srfGridMesh_t &grid, const int row, const int column, const vec3_t &point, const float loderror)
{
	int i, j;
	int width, height, oldheight;
	drawVert_t ctrl[MAX_GRID_SIZE][MAX_GRID_SIZE]{};
	float errorTable[2][MAX_GRID_SIZE]{};
	float lodRadius;
	vec3_t lodOrigin{};

	oldheight = 0;
	width = grid.width;
	height = grid.height + 1;
	if (height > MAX_GRID_SIZE)
		return NULL;
	for (i = 0; i < height; i++)
	{
		if (i == row)
		{
			// insert new row
			for (j = 0; j < grid.width; j++)
			{
				LerpDrawVert(grid.verts[(i - 1) * grid.width + j], grid.verts[i * grid.width + j], ctrl[i][j]);
				if (j == column)
					VectorCopy(point, ctrl[i][j].xyz);
			}
			errorTable[1][i] = loderror;
			continue;
		}
		errorTable[1][i] = grid.heightLodError[oldheight];
		for (j = 0; j < grid.width; j++)
		{
			ctrl[i][j] = grid.verts[oldheight * grid.width + j];
		}
		oldheight++;
	}
	for (j = 0; j < grid.width; j++)
	{
		errorTable[0][j] = grid.widthLodError[j];
	}
	// put all the approximating points on the curve
	// PutPointsOnCurve( ctrl, width, height );
	// calculate normals
	MakeMeshNormals(width, height, ctrl);

	VectorCopy(grid.lodOrigin, lodOrigin);
	lodRadius = grid.lodRadius;
	// free the old grid
	R_FreeSurfaceGridMesh(grid);
	// create a new grid
	grid = *R_CreateSurfaceGridMesh(width, height, ctrl, errorTable);
	grid.lodRadius = lodRadius;
	VectorCopy(lodOrigin, grid.lodOrigin);
	return &grid;
}

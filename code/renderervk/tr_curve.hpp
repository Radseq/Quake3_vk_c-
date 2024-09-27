#ifndef TR_CURVE_HPP
#define TR_CURVE_HPP

#include "tr_local.hpp"

void R_FreeSurfaceGridMesh(srfGridMesh_t &grid);
srfGridMesh_t *R_SubdividePatchToGrid(int width, int height,
									  drawVert_t points[MAX_PATCH_SIZE * MAX_PATCH_SIZE]);
srfGridMesh_t *R_GridInsertColumn(srfGridMesh_t &grid, int column, int row, const vec3_t &point, float loderror);
srfGridMesh_t *R_GridInsertRow(srfGridMesh_t &grid, int row, int column, const vec3_t &point, float loderror);

#endif // TR_CURVE_HPP

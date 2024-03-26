#ifndef TR_CURVE_HPP
#define TR_CURVE_HPP

#include "../renderervk/tr_local.h"
#include "q_math.hpp"

#ifdef __cplusplus
extern "C"
{
#endif

	void R_FreeSurfaceGridMesh_plus(srfGridMesh_t *grid);
	srfGridMesh_t *R_SubdividePatchToGrid_plus(int width, int height,
										  drawVert_t points[MAX_PATCH_SIZE * MAX_PATCH_SIZE]);
	srfGridMesh_t *R_GridInsertColumn_plus(srfGridMesh_t *grid, int column, int row, vec3_t point, float loderror);
	srfGridMesh_t *R_GridInsertRow_plus(srfGridMesh_t *grid, int row, int column, vec3_t point, float loderror);

#ifdef __cplusplus
}
#endif

#endif // TR_CURVE_HPP

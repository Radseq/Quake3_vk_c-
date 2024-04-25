#ifndef TR_CURVE_HPP
#define TR_CURVE_HPP

#ifdef __cplusplus
extern "C"
{
#endif
#include "q_math.hpp"
#include "tr_local.hpp"

	void R_FreeSurfaceGridMesh_plus(srfGridMesh_t *grid);
	srfGridMesh_t *R_SubdividePatchToGrid_plus(int width, int height,
											   drawVert_t points[MAX_PATCH_SIZE * MAX_PATCH_SIZE]);
	srfGridMesh_t *R_GridInsertColumn_plus(srfGridMesh_t *grid, int column, int row, vec3_t point, float loderror);
	srfGridMesh_t *R_GridInsertRow_plus(srfGridMesh_t *grid, int row, int column, vec3_t point, float loderror);

#ifdef __cplusplus
}
#endif

#endif // TR_CURVE_HPP

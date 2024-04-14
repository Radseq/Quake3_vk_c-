#ifndef TR_SKY_HPP
#define TR_SKY_HPP

#ifdef __cplusplus
extern "C"
{
#endif

#include "../renderervk/tr_local.h"

#define SKY_SUBDIVISIONS 8
#define HALF_SKY_SUBDIVISIONS (SKY_SUBDIVISIONS / 2)

	static float s_cloudTexCoords[6][SKY_SUBDIVISIONS + 1][SKY_SUBDIVISIONS + 1][2];

	/*
	===================================================================================

	POLYGON TO BOX SIDE PROJECTION

	===================================================================================
	*/

	static const vec3_t sky_clip[6] =
		{
			{1, 1, 0},
			{1, -1, 0},
			{0, -1, 1},
			{0, 1, 1},
			{1, 0, 1},
			{-1, 0, 1}};

	static float sky_mins[2][6], sky_maxs[2][6];
	static float sky_min, sky_max;
	static float sky_min_depth;

#define ON_EPSILON 0.1f // point on plane side epsilon
#define MAX_CLIP_VERTS 64

	static const int sky_texorder[6] = {0, 2, 1, 3, 4, 5};
	static vec3_t s_skyPoints[SKY_SUBDIVISIONS + 1][SKY_SUBDIVISIONS + 1];
	static float s_skyTexCoords[SKY_SUBDIVISIONS + 1][SKY_SUBDIVISIONS + 1][2];

	// void R_InitSkyTexCoords_plus(float heightCloud);
	// void RB_DrawSun_plus(float scale, shader_t *shader);
	// void RB_StageIteratorSky_plus(void);

#ifdef __cplusplus
}
#endif

#endif // TR_SKY_HPP
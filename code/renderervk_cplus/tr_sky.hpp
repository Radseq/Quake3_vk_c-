#ifndef TR_SKY_HPP
#define TR_SKY_HPP

#ifdef __cplusplus
extern "C"
{
#endif

#include "tr_local.hpp"

	void R_InitSkyTexCoords_plus(float heightCloud);
	void RB_DrawSun_plus(float scale, shader_t *shader);
	void RB_StageIteratorSky_plus(void);

#ifdef __cplusplus
}
#endif

#endif // TR_SKY_HPP
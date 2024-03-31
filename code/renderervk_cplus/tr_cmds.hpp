#ifndef TR_CMDS_HPP
#define TR_CMDS_HPP

#include "../renderervk/tr_local.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define MODE_RED_CYAN 1
#define MODE_RED_BLUE 2
#define MODE_RED_GREEN 3
#define MODE_GREEN_MAGENTA 4
#define MODE_MAX MODE_GREEN_MAGENTA

	void *R_GetCommandBuffer_plus(int bytes);
	void R_AddDrawSurfCmd_plus(drawSurf_t *drawSurfs, int numDrawSurfs);
	void RE_SetColor_plus(const float *rgba);
	void RE_StretchPic_plus(float x, float y, float w, float h,
							float s1, float t1, float s2, float t2, qhandle_t hShader);
	void RE_BeginFrame_plus(stereoFrame_t stereoFrame);
	void RE_TakeVideoFrame_plus(int width, int height,
								byte *captureBuffer, byte *encodeBuffer, bool motionJpeg);
	void RE_ThrottleBackend_plus();
	void RE_FinishBloom_plus();
	bool RE_CanMinimize_plus();
	const glconfig_t *RE_GetConfig_plus();
	void RE_VertexLighting_plus(bool allowed);

#ifdef __cplusplus
}
#endif

#endif // TR_CMDS_HPP

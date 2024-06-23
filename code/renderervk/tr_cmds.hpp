#ifndef TR_CMDS_HPP
#define TR_CMDS_HPP

#include "tr_local.hpp"

void *R_GetCommandBuffer(int bytes);
void R_AddDrawSurfCmd(drawSurf_t *drawSurfs, int numDrawSurfs);
void RE_SetColor(const float *rgba);
void RE_StretchPic(float x, float y, float w, float h,
				   float s1, float t1, float s2, float t2, qhandle_t hShader);
void RE_BeginFrame(stereoFrame_t stereoFrame);
void RE_TakeVideoFrame(int width, int height,
					   byte *captureBuffer, byte *encodeBuffer, bool motionJpeg);
void RE_ThrottleBackend();
void RE_FinishBloom();
bool RE_CanMinimize();
const glconfig_t *RE_GetConfig();
void RE_VertexLighting(bool allowed);
void RE_EndFrame(int *frontEndMsec, int *backEndMsec);

#endif // TR_CMDS_HPP

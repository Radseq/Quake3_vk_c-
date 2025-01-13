#ifndef TR_BACKEND_HPP
#define TR_BACKEND_HPP

#include "tr_local.hpp"

void Bind(image_t *image);
void SelectTexture(int unit);
void GL_Cull(cullType_t cullType);
void RE_StretchRaw(const int x, const int y, const int w, const int h,
	const int cols, const int rows, byte* data, const int client, const bool dirty);
void RE_UploadCinematic(const int w, const int h, const int cols, const int rows, byte* data, const int client, const bool dirty);
void RB_ShowImages(void);
void RB_ExecuteRenderCommands(const void *data);

#endif // TR_BACKEND_HPP

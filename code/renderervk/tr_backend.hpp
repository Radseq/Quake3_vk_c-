#ifndef TR_BACKEND_HPP
#define TR_BACKEND_HPP

#include "tr_local.hpp"

void Bind(image_t *image);
void SelectTexture(int unit);
void GL_Cull(cullType_t cullType);
void RE_StretchRaw(int x, int y, int w, int h, int cols, int rows, byte *data, int client, bool dirty);
void RE_UploadCinematic(int w, int h, int cols, int rows, byte *data, int client, bool dirty);
void RB_ShowImages(void);
void RB_ExecuteRenderCommands(const void *data);

#endif // TR_BACKEND_HPP

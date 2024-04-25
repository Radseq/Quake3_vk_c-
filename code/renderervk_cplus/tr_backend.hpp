#ifndef TR_BACKEND_HPP
#define TR_BACKEND_HPP

#ifdef __cplusplus
extern "C"
{
#endif

#include "tr_local.hpp"

    void Bind_plus(image_t *image);
    void SelectTexture_plus(int unit);
    void GL_Cull_plus(cullType_t cullType);
    void RE_StretchRaw_plus(int x, int y, int w, int h, int cols, int rows, byte *data, int client, bool dirty);
    void RE_UploadCinematic_plus(int w, int h, int cols, int rows, byte *data, int client, bool dirty);
    void RB_ShowImages_plus(void);
    void RB_ExecuteRenderCommands_plus(const void *data);

#ifdef __cplusplus
}
#endif

#endif // TR_BACKEND_HPP

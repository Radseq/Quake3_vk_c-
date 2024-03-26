#ifndef TR_BACKEND_HPP
#define TR_BACKEND_HPP

#include "../renderervk/tr_local.h"


#ifdef __cplusplus
extern "C"
{
#endif

    // void Bind_plus(image_t *image);
    void SelectTexture_plus(int unit);
    void GL_Cull_plus(cullType_t cullType);

#ifdef __cplusplus
}
#endif

#endif // TR_BACKEND_HPP

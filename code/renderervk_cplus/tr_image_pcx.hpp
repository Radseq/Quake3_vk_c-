#ifndef TR_IMAGE_PCX_HPP
#define TR_IMAGE_PCX_HPP

#ifdef __cplusplus
extern "C"
{
#endif
#include "../qcommon/q_shared.h"
#include "../renderervk/tr_public.h"
    /*
    ========================================================================

    PCX files are used for 8 bit images

    ========================================================================
    */

    void R_LoadPCX_plus(const char *filename, byte **pic, int *width, int *height);

#ifdef __cplusplus
}
#endif

#endif // TR_IMAGE_PCX_HPP
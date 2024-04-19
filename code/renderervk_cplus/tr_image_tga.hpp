#ifndef TR_IMAGE_TGA_HPP
#define TR_IMAGE_TGA_HPP

#ifdef __cplusplus
extern "C"
{
#endif

#include "q_shared.hpp"
#include "../renderervk/tr_public.h"

    /*
    ========================================================================

    TGA files are used for 24/32 bit images

    ========================================================================
    */

    void R_LoadTGA_plus(const char *name, byte **pic, int *width, int *height);

#ifdef __cplusplus
}
#endif

#endif // TR_IMAGE_TGA_HPP

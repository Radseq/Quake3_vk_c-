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

    typedef struct _TargaHeader
    {
        unsigned char id_length, colormap_type, image_type;
        unsigned short colormap_index, colormap_length;
        unsigned char colormap_size;
        unsigned short x_origin, y_origin, width, height;
        unsigned char pixel_size, attributes;
    } TargaHeader;

    void R_LoadTGA_plus(const char *name, byte **pic, int *width, int *height);

#ifdef __cplusplus
}
#endif

#endif // TR_IMAGE_TGA_HPP

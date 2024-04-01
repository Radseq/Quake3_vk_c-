#ifndef TR_IMAGE_PCX_HPP
#define TR_IMAGE_PCX_HPP

#include "../qcommon/q_shared.h"
#include "../renderervk/tr_public.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /*
    ========================================================================

    PCX files are used for 8 bit images

    ========================================================================
    */

    typedef struct
    {
        char manufacturer;
        char version;
        char encoding;
        char bits_per_pixel;
        unsigned short xmin, ymin, xmax, ymax;
        unsigned short hres, vres;
        unsigned char palette[48];
        char reserved;
        char color_planes;
        unsigned short bytes_per_line;
        unsigned short palette_type;
        unsigned short hscreensize, vscreensize;
        char filler[54];
        unsigned char data[];
    } pcx_t;

    void R_LoadPCX_plus(const char *filename, byte **pic, int *width, int *height);

#ifdef __cplusplus
}
#endif

#endif // TR_IMAGE_PCX_HPP

#ifndef TR_IMAGE_JPG_HPP
#define TR_IMAGE_JPG_HPP

extern "C"
{
#include "../qcommon/tr_public.h"
}


void R_LoadJPG(const char *filename, unsigned char **pic, int *width, int *height);

#endif // TR_IMAGE_JPG_HPP

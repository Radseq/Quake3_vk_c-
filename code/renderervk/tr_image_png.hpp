#ifndef TR_IMAGE_PNG_HPP
#define TR_IMAGE_PNG_HPP

extern "C"
{
#include "../qcommon/tr_public.h"
}


void R_LoadPNG(const char *name, byte **pic, int *width, int *height);

#endif // TR_IMAGE_PNG_HPP

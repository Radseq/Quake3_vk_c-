#ifndef TR_IMAGE_PCX_HPP
#define TR_IMAGE_PCX_HPP

#include "../qcommon/tr_public.h"
/*
========================================================================

PCX files are used for 8 bit images

========================================================================
*/

void R_LoadPCX(const char *filename, byte **pic, int *width, int *height);

#endif // TR_IMAGE_PCX_HPP

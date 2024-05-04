#ifndef TR_IMAGE_PCX_HPP
#define TR_IMAGE_PCX_HPP

#include "tr_public.hpp"
/*
========================================================================

PCX files are used for 8 bit images

========================================================================
*/

void R_LoadPCX(const char *filename, byte **pic, int *width, int *height);

#endif // TR_IMAGE_PCX_HPP

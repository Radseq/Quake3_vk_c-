#ifndef TR_IMAGE_TGA_HPP
#define TR_IMAGE_TGA_HPP

#include "tr_public.hpp"

/*
========================================================================

TGA files are used for 24/32 bit images

========================================================================
*/

void R_LoadTGA(const char *name, byte **pic, int *width, int *height);

#endif // TR_IMAGE_TGA_HPP

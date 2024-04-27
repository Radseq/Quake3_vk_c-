#ifndef TR_IMAGE_PNG_HPP
#define TR_IMAGE_PNG_HPP

#ifdef __cplusplus
extern "C"
{
#endif

#include "tr_public.hpp"
#include "q_platform.hpp"

    void R_LoadPNG(const char *name, byte **pic, int *width, int *height);

#ifdef __cplusplus
}
#endif

#endif // TR_IMAGE_PNG_HPP

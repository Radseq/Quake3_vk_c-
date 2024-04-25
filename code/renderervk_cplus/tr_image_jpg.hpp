#ifndef TR_IMAGE_JPG_HPP
#define TR_IMAGE_JPG_HPP

#ifdef __cplusplus
extern "C"
{
#endif

#include "../qcommon/q_shared.h"
#include "tr_public.hpp"

    void R_LoadJPG_plus(const char *filename, unsigned char **pic, int *width, int *height);

#ifdef __cplusplus
}
#endif

#endif // TR_IMAGE_JPG_HPP

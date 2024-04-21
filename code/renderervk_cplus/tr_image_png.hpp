#ifndef TR_IMAGE_PNG_HPP
#define TR_IMAGE_PNG_HPP

#ifdef __cplusplus
extern "C"
{
#endif

#include "q_shared.hpp"
#include "../renderervk/tr_public.h"
#include "q_platform.hpp"
#include "puff.hpp"

    void R_LoadPNG_plus(const char *name, byte **pic, int *width, int *height);

#ifdef __cplusplus
}
#endif

#endif // TR_IMAGE_PNG_HPP

#ifndef TR_IMAGE_BMP_HPP
#define TR_IMAGE_BMP_HPP

#ifdef __cplusplus
extern "C"
{
#endif

#include "../qcommon/q_shared.h"
#include "../renderervk/tr_public.h"

	void R_LoadBMP_plus(const char *name, byte **pic, int *width, int *height);

#ifdef __cplusplus
}
#endif

#endif // TR_IMAGE_BMP_HPP

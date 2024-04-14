#ifndef TR_IMAGE_BMP_HPP
#define TR_IMAGE_BMP_HPP

#ifdef __cplusplus
extern "C"
{
#endif

#include "../qcommon/q_shared.h"
#include "../renderervk/tr_public.h"

	typedef struct
	{
		char id[2];
		unsigned fileSize;
		unsigned reserved0;
		unsigned bitmapDataOffset;
		unsigned bitmapHeaderSize;
		unsigned width;
		unsigned height;
		unsigned short planes;
		unsigned short bitsPerPixel;
		unsigned compression;
		unsigned bitmapDataSize;
		unsigned hRes;
		unsigned vRes;
		unsigned colors;
		unsigned importantColors;
		unsigned char palette[256][4];
	} BMPHeader_t;

	void R_LoadBMP_plus(const char *name, byte **pic, int *width, int *height);

#ifdef __cplusplus
}
#endif

#endif // TR_IMAGE_BMP_HPP

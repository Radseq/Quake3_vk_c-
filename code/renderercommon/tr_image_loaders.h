#ifndef TR_IMAGE_LOADERS_H
#define TR_IMAGE_LOADERS_H

#include "../qcommon/q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

int R_ImageLoaderReadFile(const char *name, void **buffer);
void R_ImageLoaderFreeFile(void *buffer);
void *R_ImageLoaderMalloc(int bytes);
void R_ImageLoaderFree(void *buffer);
void QDECL R_ImageLoaderPrint(int level, const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif

#ifndef TR_SHADER_HPP
#define TR_SHADER_HPP

#include "tr_local.hpp"

/*
================
return a hash value for the filename
================
*/
#ifdef __GNUCC__
#warning TODO: check if long is ok here
#endif

#define generateHashValue Com_GenerateHashValue

typedef struct
{
    int blendA;
    int blendB;

    int multitextureEnv;
    int multitextureBlend;
} collapse_t;

shader_t *R_GetShaderByHandle(qhandle_t hShader);
void RE_RemapShader(const char *shaderName, const char *newShaderName, const char *timeOffset);
shader_t *R_FindShaderByName(const char *name);
shader_t *R_FindShader(const char *name, int lightmapIndex, bool mipRawImage);

qhandle_t RE_RegisterShaderFromImage(const char *name, int lightmapIndex, image_t *image, bool mipRawImage);
qhandle_t RE_RegisterShaderLightMap(const char *name, int lightmapIndex);
qhandle_t RE_RegisterShader(const char *name);
qhandle_t RE_RegisterShaderNoMip(const char *name);
void R_ShaderList_f(void);
void R_InitShaders(void);

#endif // TR_SHADER_HPP
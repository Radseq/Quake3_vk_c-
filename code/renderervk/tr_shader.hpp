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

typedef struct
{
    int blendA;
    int blendB;

    int multitextureEnv;
    int multitextureBlend;
} collapse_t;

shader_t *R_GetShaderByHandle(qhandle_t hShader);
shader_t *R_FindShaderByName(std::string_view name);
shader_t *R_FindShader(std::string_view name, int lightmapIndex, bool mipRawImage);

qhandle_t RE_RegisterShaderFromImage(std::string_view name, int lightmapIndex, image_t &image, bool mipRawImage);
qhandle_t RE_RegisterShaderLightMap(std::string_view name, int lightmapIndex);

#ifdef __cplusplus
extern "C"
{
#endif
    qhandle_t RE_RegisterShaderNoMip(const char *name);
    qhandle_t RE_RegisterShader(const char *name);
    void RE_RemapShader(const char *shaderName, const char *newShaderName, const char *timeOffset);
#ifdef __cplusplus
}
#endif

void R_ShaderList_f(void);
void R_InitShaders(void);

#endif // TR_SHADER_HPP
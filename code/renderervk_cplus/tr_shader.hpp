#ifndef TR_SHADER_HPP
#define TR_SHADER_HPP

#ifdef __cplusplus
extern "C"
{
#endif

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

    shader_t *R_GetShaderByHandle_plus(qhandle_t hShader);
    void RE_RemapShader_plus(const char *shaderName, const char *newShaderName, const char *timeOffset);
    shader_t *R_FindShaderByName_plus(const char *name);
    shader_t *R_FindShader_plus(const char *name, int lightmapIndex, bool mipRawImage);

    qhandle_t RE_RegisterShaderFromImage_plus(const char *name, int lightmapIndex, image_t *image, bool mipRawImage);
    qhandle_t RE_RegisterShaderLightMap_plus(const char *name, int lightmapIndex);
    qhandle_t RE_RegisterShader_plus(const char *name);
    qhandle_t RE_RegisterShaderNoMip_plus(const char *name);
    void R_ShaderList_f_plus(void);
    void R_InitShaders_plus(void);

#ifdef __cplusplus
}
#endif

#endif // TR_SHADER_HPP
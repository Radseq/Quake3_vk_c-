#ifndef TR_SHADER_HPP
#define TR_SHADER_HPP

#ifdef __cplusplus
extern "C"
{
#endif

#include "../renderervk/tr_local.h"

#define TEST
#define TEST_A

    static char *s_shaderText;

    static const char *s_extensionOffset;
    static int s_extendedShader;

    // the shader is parsed into these global variables, then copied into
    // dynamically allocated memory if it is valid.
    static shaderStage_t stages[MAX_SHADER_STAGES];
    static shader_t shader;
    static texModInfo_t texMods[MAX_SHADER_STAGES][TR_MAX_TEXMODS + 1]; // reserve one additional texmod for lightmap atlas correction

#define FILE_HASH_SIZE 1024
    static shader_t *shaderHashTable[FILE_HASH_SIZE];

#define MAX_SHADERTEXT_HASH 2048
    static const char **shaderTextHashTable[MAX_SHADERTEXT_HASH];

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

    static const collapse_t collapse[] = {
        {0, GLS_DSTBLEND_SRC_COLOR | GLS_SRCBLEND_ZERO,
         GL_MODULATE, 0},

        {0, GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR,
         GL_MODULATE, 0},

        {GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR, GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR,
         GL_MODULATE, GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR},

        {GLS_DSTBLEND_SRC_COLOR | GLS_SRCBLEND_ZERO, GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR,
         GL_MODULATE, GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR},

        {GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR, GLS_DSTBLEND_SRC_COLOR | GLS_SRCBLEND_ZERO,
         GL_MODULATE, GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR},

        {GLS_DSTBLEND_SRC_COLOR | GLS_SRCBLEND_ZERO, GLS_DSTBLEND_SRC_COLOR | GLS_SRCBLEND_ZERO,
         GL_MODULATE, GLS_DSTBLEND_ZERO | GLS_SRCBLEND_DST_COLOR},

        {0, GLS_DSTBLEND_ONE | GLS_SRCBLEND_ONE,
         GL_ADD, 0},

        {GLS_DSTBLEND_ONE | GLS_SRCBLEND_ONE, GLS_DSTBLEND_ONE | GLS_SRCBLEND_ONE,
         GL_ADD, GLS_DSTBLEND_ONE | GLS_SRCBLEND_ONE},

        {GLS_DSTBLEND_ONE | GLS_SRCBLEND_SRC_ALPHA, GLS_DSTBLEND_ONE | GLS_SRCBLEND_SRC_ALPHA,
         GL_BLEND_ALPHA, GLS_DSTBLEND_ONE | GLS_SRCBLEND_ONE},

        {GLS_DSTBLEND_ONE | GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA, GLS_DSTBLEND_ONE | GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA,
         GL_BLEND_ONE_MINUS_ALPHA, GLS_DSTBLEND_ONE | GLS_SRCBLEND_ONE},

        {0, GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_SRCBLEND_SRC_ALPHA,
         GL_BLEND_MIX_ALPHA, 0},

        {0, GLS_DSTBLEND_SRC_ALPHA | GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA,
         GL_BLEND_MIX_ONE_MINUS_ALPHA, 0},

        {0, GLS_DSTBLEND_SRC_ALPHA | GLS_SRCBLEND_DST_COLOR,
         GL_BLEND_DST_COLOR_SRC_ALPHA, 0},

#if 0
	{ 0, GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_SRCBLEND_SRC_ALPHA,
		GL_DECAL, 0 },
#endif
        {-1}};

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
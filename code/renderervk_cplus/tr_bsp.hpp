#ifndef TR_BSP_HPP
#define TR_BSP_HPP

#ifdef __cplusplus
extern "C"
{
#endif

#include "../renderervk/tr_local.h"

    static world_t s_worldData;
    static byte *fileBase;

    static int c_gridVerts;

#define LIGHTMAP_SIZE 128
#define LIGHTMAP_BORDER 2
#define LIGHTMAP_LEN (LIGHTMAP_SIZE + LIGHTMAP_BORDER * 2)

    static const int lightmapFlags = IMGFLAG_NOLIGHTSCALE | IMGFLAG_NO_COMPRESSION | IMGFLAG_LIGHTMAP | IMGFLAG_NOSCALE;

    static int lightmapWidth;
    static int lightmapHeight;
    static int lightmapCountX;
    static int lightmapCountY;

    void R_ColorShiftLightingBytes_plus(const byte in[4], byte out[4], bool hasAlpha);
    int R_GetLightmapCoords_plus(const int lightmapIndex, float *x, float *y);
    void RE_SetWorldVisData_plus(const byte *vis);
    bool RE_GetEntityToken_plus(char *buffer, int size);
    void RE_LoadWorldMap_plus(const char *name);

#ifdef __cplusplus
}
#endif

#endif // TR_BSP_HPP

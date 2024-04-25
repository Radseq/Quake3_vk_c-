#ifndef TR_BSP_HPP
#define TR_BSP_HPP

#ifdef __cplusplus
extern "C"
{
#endif

#include "tr_local.hpp"

    void R_ColorShiftLightingBytes_plus(const byte in[4], byte out[4], bool hasAlpha);
    int R_GetLightmapCoords_plus(const int lightmapIndex, float *x, float *y);
    void RE_SetWorldVisData_plus(const byte *vis);
    bool RE_GetEntityToken_plus(char *buffer, int size);
    void RE_LoadWorldMap_plus(const char *name);

#ifdef __cplusplus
}
#endif

#endif // TR_BSP_HPP

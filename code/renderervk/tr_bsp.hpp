#ifndef TR_BSP_HPP
#define TR_BSP_HPP

#include "tr_local.hpp"


void R_ColorShiftLightingBytes(const byte in[4], byte out[4], const bool hasAlpha);
int R_GetLightmapCoords(const int lightmapIndex, float &x, float &y);
void RE_SetWorldVisData(const byte *vis);
bool RE_GetEntityToken(char *buffer, int size);
void RE_LoadWorldMap(const char *name);

#endif // TR_BSP_HPP

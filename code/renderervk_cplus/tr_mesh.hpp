#ifndef TR_MESH_HPP
#define TR_MESH_HPP

#ifdef __cplusplus
extern "C"
{
#endif

#include "../renderervk/tr_local.h"

    int R_ComputeLOD_plus(trRefEntity_t *ent);
    void R_AddMD3Surfaces_plus(trRefEntity_t *ent);

#ifdef __cplusplus
}
#endif

#endif // TR_MESH_HPP

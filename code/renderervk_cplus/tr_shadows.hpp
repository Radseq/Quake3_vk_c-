#ifndef TR_SHADOWS_HPP
#define TR_SHADOWS_HPP

#include "../renderervk/tr_local.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /*

      for a projection shadow:

      point[x] += light vector * ( z - shadow plane )
      point[y] +=
      point[z] = shadow plane

      1 0 light[x] / light[z]

    */

    typedef struct
    {
        int i2;
        int facing;
    } edgeDef_t;

#define MAX_EDGE_DEFS 32

    static edgeDef_t edgeDefs[SHADER_MAX_VERTEXES][MAX_EDGE_DEFS];
    static int numEdgeDefs[SHADER_MAX_VERTEXES];
    static int facing[SHADER_MAX_INDEXES / 3];

    void RB_ShadowTessEnd_plus(void);
    void RB_ShadowFinish_plus(void);
    void RB_ProjectionShadowDeform_plus(void);

#ifdef __cplusplus
}
#endif

#endif // TR_SHADOWS_HPP
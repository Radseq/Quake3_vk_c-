#ifndef TR_SHADOWS_HPP
#define TR_SHADOWS_HPP

#ifdef __cplusplus
extern "C"
{
#endif

#include "../renderervk/tr_local.h"

    void RB_ShadowTessEnd_plus(void);
    void RB_ShadowFinish_plus(void);
    void RB_ProjectionShadowDeform_plus(void);

#ifdef __cplusplus
}
#endif

#endif // TR_SHADOWS_HPP
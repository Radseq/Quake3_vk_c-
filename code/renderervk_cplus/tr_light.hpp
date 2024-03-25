#ifndef TR_LIGHT_HPP
#define TR_LIGHT_HPP

#include "../qcommon/q_shared.h"
#include "../renderervk/tr_local.h"

#ifdef __cplusplus
extern "C"
{
#endif

    void R_TransformDlights_plus(int count, dlight_t *dl, orientationr_t *ort);

#ifdef __cplusplus
}
#endif

#endif // TR_LIGHT_HPP
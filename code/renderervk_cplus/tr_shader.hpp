#ifndef TR_SHADER_HPP
#define TR_SHADER_HPP

#ifdef __cplusplus
extern "C"
{
#endif

#include "../renderervk/tr_local.h"

    shader_t *R_GetShaderByHandle_plus(qhandle_t hShader);

#ifdef __cplusplus
}
#endif

#endif // TR_SHADER_HPP
#ifndef TR_MARKS_HPP
#define TR_MARKS_HPP

#ifdef __cplusplus
extern "C"
{
#endif
#include "../renderervk/tr_local.h"

    int R_MarkFragments_plus(int numPoints, const vec3_t *points, const vec3_t projection,
                             int maxPoints, vec3_t pointBuffer, int maxFragments, markFragment_t *fragmentBuffer);

#ifdef __cplusplus
}
#endif

#endif // TR_MARKS_HPP
#ifndef TR_MARKS_HPP
#define TR_MARKS_HPP

#ifdef __cplusplus
extern "C"
{
#endif
#include "../renderervk/tr_local.h"

#define SIDE_FRONT 0
#define SIDE_BACK 1
#define SIDE_ON 2

#define MAX_VERTS_ON_POLY 64

#define MARKER_OFFSET 0 // 1

    int R_MarkFragments_plus(int numPoints, const vec3_t *points, const vec3_t projection,
                             int maxPoints, vec3_t pointBuffer, int maxFragments, markFragment_t *fragmentBuffer);

#ifdef __cplusplus
}
#endif

#endif // TR_MARKS_HPP

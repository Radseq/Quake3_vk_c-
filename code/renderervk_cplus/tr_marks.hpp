#ifndef TR_MARKS_HPP
#define TR_MARKS_HPP

#include "tr_local.hpp"

int R_MarkFragments_plus(int numPoints, const vec3_t *points, const vec3_t projection,
                         int maxPoints, vec3_t pointBuffer, int maxFragments, markFragment_t *fragmentBuffer);

#endif // TR_MARKS_HPP

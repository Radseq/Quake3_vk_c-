#ifndef VK_VBO_HPP
#define VK_VBO_HPP

#include "tr_local.hpp"

#ifdef USE_VBO
void VBO_Cleanup_plus();
void VBO_PushData_plus(int itemIndex, shaderCommands_t *input);
void VBO_UnBind_plus();
void R_BuildWorldVBO_plus(msurface_t *surf, int surfCount);
void qsort_int_plus(int *arr, const int n);
void VBO_QueueItem_plus(int itemIndex);
void VBO_ClearQueue_plus();
void VBO_Flush_plus();
void VBO_RenderIBOItems_plus();
void VBO_PrepareQueues_plus();

#endif // USE_VBO

#endif // VK_VBO_HPP

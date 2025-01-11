#ifndef VK_VBO_HPP
#define VK_VBO_HPP

#include "tr_local.hpp"

#ifdef USE_VBO
void VBO_Cleanup();
void VBO_PushData(const int itemIndex, shaderCommands_t &input);
void VBO_UnBind();
void R_BuildWorldVBO(msurface_t &surf, const int surfCount);
void VBO_QueueItem(const int itemIndex);
void VBO_ClearQueue();
void VBO_Flush();
void VBO_RenderIBOItems();
void VBO_PrepareQueues();

#endif // USE_VBO

#endif // VK_VBO_HPP

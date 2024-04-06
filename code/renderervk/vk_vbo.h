#ifndef VK_VBO_H
#define VK_VBO_H

#include "tr_local.h"

#ifdef USE_VBO
#define MAX_VBO_STAGES MAX_SHADER_STAGES

#define MIN_IBO_RUN 320

      //[ibo]: [index0][index1][index2]
      //[vbo]: [index0][vertex0...][index1][vertex1...][index2][vertex2...]

      typedef struct vbo_item_s
      {
            int index_offset; // device-local, relative to current shader
            int soft_offset;  // host-visible, absolute
            int num_indexes;
            int num_vertexes;
      } vbo_item_t;

      typedef struct ibo_item_s
      {
            int offset;
            int length;
      } ibo_item_t;

      typedef struct vbo_s
      {
            byte *vbo_buffer;
            int vbo_offset;
            int vbo_size;

            byte *ibo_buffer;
            int ibo_offset;
            int ibo_size;

            uint32_t soft_buffer_indexes;
            uint32_t soft_buffer_offset;

            ibo_item_t *ibo_items;
            int ibo_items_count;

            vbo_item_t *items;
            int items_count;

            int *items_queue;
            int items_queue_count;

      } vbo_t;

      static vbo_t world_vbo;

      void VBO_Cleanup();
      void VBO_PushData(int itemIndex, shaderCommands_t *input);
      void VBO_UnBind();
      void R_BuildWorldVBO(msurface_t *surf, int surfCount);
      void qsort_int(int *arr, const int n);
      void VBO_QueueItem(int itemIndex);
      void VBO_ClearQueue();
      void VBO_Flush();
      void VBO_RenderIBOItems();
      void VBO_PrepareQueues();

#endif // USE_VBO

#endif // VK_VBO_H

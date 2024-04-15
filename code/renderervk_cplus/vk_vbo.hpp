#ifndef VK_VBO_HPP
#define VK_VBO_HPP

#ifdef __cplusplus
extern "C"
{
#endif

#include "../renderervk/tr_local.h"

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

#ifdef __cplusplus
}
#endif

#endif // VK_VBO_HPP

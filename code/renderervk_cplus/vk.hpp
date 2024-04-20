#ifndef VK_HPP
#define VK_HPP

#ifdef __cplusplus
extern "C"
{
#endif
#include "../renderervk/vulkan/vulkan.h"
#include "../renderervk/tr_common.h"

#include "../renderervk/tr_local.h"

#define MAX_SWAPCHAIN_IMAGES 8
#define MIN_SWAPCHAIN_IMAGES_IMM 3
#define MIN_SWAPCHAIN_IMAGES_FIFO 3
#define MIN_SWAPCHAIN_IMAGES_FIFO_0 4
#define MIN_SWAPCHAIN_IMAGES_MAILBOX 3

#define MAX_VK_SAMPLERS 32
#define MAX_VK_PIPELINES (1024 + 128)

#define VERTEX_BUFFER_SIZE (4 * 1024 * 1024)
#define IMAGE_CHUNK_SIZE (32 * 1024 * 1024)
#define MAX_IMAGE_CHUNKS 48

#define NUM_COMMAND_BUFFERS 2 // number of command buffers / render semaphores / framebuffer sets

#define USE_REVERSED_DEPTH
      // #define USE_BUFFER_CLEAR

#define VK_NUM_BLOOM_PASSES 4

#define USE_DEDICATED_ALLOCATION
// #define MIN_IMAGE_ALIGN (128*1024)
#define MAX_ATTACHMENTS_IN_POOL (8 + VK_NUM_BLOOM_PASSES * 2) // depth + msaa + msaa-resolve + depth-resolve + screenmap.msaa + screenmap.resolve + screenmap.depth + bloom_extract + blur pairs

#define VK_DESC_STORAGE 0
#define VK_DESC_UNIFORM 1
#define VK_DESC_TEXTURE0 2
#define VK_DESC_TEXTURE1 3
#define VK_DESC_TEXTURE2 4
#define VK_DESC_FOG_COLLAPSE 5
#define VK_DESC_COUNT 6

#define VK_DESC_TEXTURE_BASE VK_DESC_TEXTURE0
#define VK_DESC_FOG_ONLY VK_DESC_TEXTURE1
#define VK_DESC_FOG_DLIGHT VK_DESC_TEXTURE1

#define TESS_XYZ (1)
#define TESS_RGBA0 (2)
#define TESS_RGBA1 (4)
#define TESS_RGBA2 (8)
#define TESS_ST0 (16)
#define TESS_ST1 (32)
#define TESS_ST2 (64)
#define TESS_NNN (128)
#define TESS_VPOS (256)  // uniform with eyePos
#define TESS_ENV (512)   // mark shader stage with environment mapping
#define TESS_ENT0 (1024) // uniform with ent.color[0]
#define TESS_ENT1 (2048) // uniform with ent.color[1]
#define TESS_ENT2 (4096) // uniform with ent.color[2]

      //
      // Initialization.
      //

      // Initializes VK_Instance structure.
      // After calling this function we get fully functional vulkan subsystem.
      void vk_initialize_plus(void);

      // Called after initialization or renderer restart
      void vk_init_descriptors_plus(void);

      // Shutdown vulkan subsystem by releasing resources acquired by Vk_Instance.
      void vk_shutdown_plus(refShutdownCode_t code);

      // Releases vulkan resources allocated during program execution.
      // This effectively puts vulkan subsystem into initial state (the state we have after vk_initialize call).
      void vk_release_resources_plus(void);

      void vk_wait_idle_plus(void);

      //
      // Resources allocation.
      //
      void vk_create_image_plus(image_t *image, int width, int height, int mip_levels);
      void vk_upload_image_data_plus(image_t *image, int x, int y, int width, int height, int miplevels, byte *pixels, int size, bool update);
      void vk_update_descriptor_set_plus(image_t *image, bool mipmap);
      void vk_destroy_image_resources_plus(VkImage *image, VkImageView *imageView);

      uint32_t vk_find_pipeline_ext_plus(uint32_t base, const Vk_Pipeline_Def *def, bool use);
      void vk_get_pipeline_def_plus(uint32_t pipeline, Vk_Pipeline_Def *def);

      void vk_create_post_process_pipeline_plus(int program_index, uint32_t width, uint32_t height);
      void vk_create_pipelines_plus(void);

      //
      // Rendering setup.
      //

      void vk_clear_color_plus(const vec4_t color);
      void vk_clear_depth_plus(bool clear_stencil);
      void vk_begin_frame_plus(void);
      void vk_end_frame_plus(void);
      void vk_present_frame_plus(void);

      void vk_end_render_pass_plus(void);
      void vk_begin_main_render_pass_plus(void);

      void vk_bind_pipeline_plus(uint32_t pipeline);
      void vk_bind_index_plus(void);
      void vk_bind_index_ext_plus(const int numIndexes, const uint32_t *indexes);
      void vk_bind_geometry_plus(uint32_t flags);
      void vk_bind_lighting_plus(int stage, int bundle);
      void vk_draw_geometry_plus(Vk_Depth_Range depth_range, bool indexed);

      void vk_read_pixels_plus(byte *buffer, uint32_t width, uint32_t height); // screenshots
      bool vk_bloom_plus(void);
      
      void vk_update_mvp_plus(const float *m);

      uint32_t vk_tess_index_plus(uint32_t numIndexes, const void *src);
      void vk_bind_index_buffer_plus(VkBuffer buffer, uint32_t offset);
      void vk_draw_indexed_plus(uint32_t indexCount, uint32_t firstIndex);

      void vk_reset_descriptor_plus(int index);
      void vk_update_descriptor_plus(int index, VkDescriptorSet descriptor);
      void vk_update_descriptor_offset_plus(int index, uint32_t offset);
      void vk_update_uniform_descriptor_plus(VkDescriptorSet descriptor, VkBuffer buffer);

      void vk_update_post_process_pipelines_plus(void);

      const char *vk_format_string_plus(VkFormat format);

      void VBO_PrepareQueues_plus(void);
      void VBO_RenderIBOItems_plus(void);
      void VBO_ClearQueue_plus(void);

      VkPipeline create_pipeline_plus(const Vk_Pipeline_Def *def, renderPass_t renderPassIndex);

#ifdef USE_VBO
      void vk_release_vbo_plus(void);
      bool vk_alloc_vbo_plus(const byte *vbo_data, int vbo_size);
#endif

      void vk_create_blur_pipeline_plus(uint32_t index, uint32_t width, uint32_t height, bool horizontal_pass);
      uint32_t vk_alloc_pipeline_plus(const Vk_Pipeline_Def *def);

      VkPipeline vk_gen_pipeline_plus(uint32_t index);
      void vk_update_mvp_plus(const float *m);

      void vk_bind_descriptor_sets_plus(void);
      void vk_begin_post_bloom_render_pass_plus(void);
      void vk_begin_bloom_extract_render_pass_plus(void);

      void vk_begin_blur_render_pass_plus(uint32_t index);

#ifdef __cplusplus
}
#endif

#endif // VK_HPP

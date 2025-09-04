#ifndef VK_HPP
#define VK_HPP

#include "tr_common.hpp"
#include "tr_local.hpp"

#include "tr_image.hpp"

#define USE_VBO
#define USE_PMLIGHT
#define USE_LEGACY_DLIGHTS

#define USE_REVERSED_DEPTH
#define USE_DEDICATED_ALLOCATION

// this structure must be in sync with shader uniforms!
typedef struct vkUniform_s
{
      // light/env parameters:
      vec4_t eyePos; // vertex
      union
      {
            struct
            {
                  vec4_t pos;    // vertex: light origin
                  vec4_t color;  // fragment: rgb + 1/(r*r)
                  vec4_t vector; // fragment: linear dynamic light
            } light;
            struct
            {
                  vec4_t color[3]; // ent.color[3]
            } ent;
      };
      // fog parameters:
      vec4_t fogDistanceVector; // vertex
      vec4_t fogDepthVector;    // vertex
      vec4_t fogEyeT;           // vertex
      vec4_t fogColor;          // fragment
} vkUniform_t;

constexpr int TESS_XYZ = 1;
constexpr int TESS_RGBA0 = 2;
constexpr int TESS_RGBA1 = 4;
constexpr int TESS_RGBA2 = 8;
constexpr int TESS_ST0 = 16;
constexpr int TESS_ST1 = 32;
constexpr int TESS_ST2 = 64;
constexpr int TESS_NNN = 128;
constexpr int TESS_VPOS = 256;  // uniform with eyePos
constexpr int TESS_ENV = 512;   // mark shader stage with environment mapping
constexpr int TESS_ENT0 = 1024; // uniform with ent.color[0]
constexpr int TESS_ENT1 = 2048; // uniform with ent.color[1]
constexpr int TESS_ENT2 = 4096; // uniform with ent.color[2]

//
// Initialization.
//

// Initializes VK_Instance structure.
// After calling this function we get fully functional vulkan subsystem.
void vk_initialize(void);

// Called after initialization or renderer restart
void vk_init_descriptors(void);

// Shutdown vulkan subsystem by releasing resources acquired by Vk_Instance.
void vk_shutdown(const refShutdownCode_t code);

// Releases vulkan resources allocated during program execution.
// This effectively puts vulkan subsystem into initial state (the state we have after vk_initialize call).
void vk_release_resources(void);

void vk_wait_idle(void);
void vk_queue_wait_idle( void );

//
// Resources allocation.
//
void vk_create_image(image_t &image, const int width, const int height, const int mip_levels);
void vk_upload_image_data(image_t &image, int x, int y, int width, int height, int miplevels, byte *pixels, int size, bool update);
void vk_update_descriptor_set(const image_t &image, const bool mipmap);
void vk_destroy_image_resources(vk::Image &image, vk::ImageView &imageView);
void vk_update_attachment_descriptors( void );
void vk_destroy_samplers( void );

uint32_t vk_find_pipeline_ext(const uint32_t base, const Vk_Pipeline_Def &def, bool use);
void vk_get_pipeline_def(const uint32_t pipeline, Vk_Pipeline_Def &def);

void vk_create_post_process_pipeline(const int program_index, const uint32_t width, const uint32_t height);
void vk_create_pipelines(void);

//
// Rendering setup.
//

void vk_clear_color(const vec4_t &color);
void vk_clear_depth(const bool clear_stencil);
void vk_begin_frame(void);
void vk_end_frame(void);
void vk_present_frame(void);

void vk_end_render_pass(void);
void vk_begin_main_render_pass(void);

void vk_bind_pipeline(const uint32_t pipeline);
void vk_bind_index(void);
void vk_bind_index_ext(const int numIndexes, const uint32_t *indexes);
void vk_bind_geometry(const uint32_t flags);
void vk_bind_lighting(const int stage, const int bundle);
void vk_draw_geometry(const Vk_Depth_Range depth_range, const bool indexed);
void vk_draw_dot( uint32_t storage_offset );

void vk_read_pixels(byte *buffer, const uint32_t width, const uint32_t height); // screenshots
bool vk_bloom(void);

void vk_update_mvp(const float *m);

uint32_t vk_tess_index(const uint32_t numIndexes, const void *src);
void vk_bind_index_buffer(const vk::Buffer &buffer, const uint32_t offset);
#ifdef USE_VBO
void vk_draw_indexed(const uint32_t indexCount, const uint32_t firstIndex);
#endif
void vk_reset_descriptor(const int index);
void vk_update_descriptor(const int index, const vk::DescriptorSet &descriptor);
void vk_update_descriptor_offset(const int index, const uint32_t offset);
void vk_update_uniform_descriptor(const vk::DescriptorSet &descriptor, const vk::Buffer &buffer);

void vk_update_post_process_pipelines(void);

void VBO_PrepareQueues(void);
void VBO_RenderIBOItems(void);
void VBO_ClearQueue(void);

vk::Pipeline create_pipeline(const Vk_Pipeline_Def &def, const renderPass_t renderPassIndex, uint32_t def_index);

#ifdef USE_VBO
void vk_release_vbo(void);
bool vk_alloc_vbo(const byte *vbo_data, const uint32_t vbo_size);
#endif

void vk_create_blur_pipeline(const uint32_t index, const uint32_t width, const uint32_t height, const bool horizontal_pass);
//uint32_t vk_alloc_pipeline(const Vk_Pipeline_Def &def);

vk::Pipeline vk_gen_pipeline(const uint32_t index);
void vk_bind_descriptor_sets(void);
void vk_begin_post_bloom_render_pass(void);
void vk_begin_bloom_extract_render_pass(void);

void vk_begin_blur_render_pass(const uint32_t index);

// Vk_Instance contains engine-specific vulkan resources that persist entire renderer lifetime.
// This structure is initialized/deinitialized by vk_initialize/vk_shutdown functions correspondingly.

// Vk_World contains vulkan resources/state requested by the game code.
// It is reinitialized on a map change.

//extern Vk_Instance vk_inst; // shouldn't be cleared during ref re-init
//extern Vk_World vk_world;   // this data is cleared during ref re-init

#endif // VK_HPP

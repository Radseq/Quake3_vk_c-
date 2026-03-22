#ifndef VK_HPP
#define VK_HPP

#include "tr_common.hpp"
#include "tr_local.hpp"

#include "tr_image.hpp"

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
void vk_destroy_image_resources(vk::Image &image, vk::ImageView &imageView);
void vk_destroy_samplers( void );

//
// Rendering setup.
//

void vk_clear_color(const vec4_t &color);
void vk_clear_depth(const bool clear_stencil);
void vk_begin_frame(void);
void vk_end_frame(void);
void vk_present_frame(void);

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

void VBO_PrepareQueues(void);
void VBO_RenderIBOItems(void);
void VBO_ClearQueue(void);

#ifdef USE_VBO
void vk_release_vbo(void);
bool vk_alloc_vbo(const byte *vbo_data, const uint32_t vbo_size);
#endif

bool vk_alloc_static_model_buffer(
	const void* src,
	const uint32_t size,
	const vk::BufferUsageFlags usage,
	gpuBuffer_t& out);

// Vk_Instance contains engine-specific vulkan resources that persist entire renderer lifetime.
// This structure is initialized/deinitialized by vk_initialize/vk_shutdown functions correspondingly.

// Vk_World contains vulkan resources/state requested by the game code.
// It is reinitialized on a map change.

//extern Vk_Instance vk_inst; // shouldn't be cleared during ref re-init
//extern Vk_World vk_world;   // this data is cleared during ref re-init

#endif // VK_HPP

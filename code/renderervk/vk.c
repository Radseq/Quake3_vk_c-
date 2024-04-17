#include "tr_local.h"
#include "vk.h"

#if defined(_DEBUG)
#if defined(_WIN32)
#define USE_VK_VALIDATION
#include <windows.h> // for win32 debug callback
#endif
#endif

////////////////////////////////////////////////////////////////////////////

// forward declaration
VkPipeline create_pipeline(const Vk_Pipeline_Def *def, renderPass_t renderPassIndex);

#define CASE_STR(x) \
	case (x):       \
		return #x

const char *vk_format_string(VkFormat format)
{
	static char buf[16];

	switch (format)
	{
		// color formats
		CASE_STR(VK_FORMAT_R5G5B5A1_UNORM_PACK16);
		CASE_STR(VK_FORMAT_B5G5R5A1_UNORM_PACK16);
		CASE_STR(VK_FORMAT_R5G6B5_UNORM_PACK16);
		CASE_STR(VK_FORMAT_B5G6R5_UNORM_PACK16);
		CASE_STR(VK_FORMAT_B8G8R8A8_SRGB);
		CASE_STR(VK_FORMAT_R8G8B8A8_SRGB);
		CASE_STR(VK_FORMAT_B8G8R8A8_SNORM);
		CASE_STR(VK_FORMAT_R8G8B8A8_SNORM);
		CASE_STR(VK_FORMAT_B8G8R8A8_UNORM);
		CASE_STR(VK_FORMAT_R8G8B8A8_UNORM);
		CASE_STR(VK_FORMAT_B4G4R4A4_UNORM_PACK16);
		CASE_STR(VK_FORMAT_R4G4B4A4_UNORM_PACK16);
		CASE_STR(VK_FORMAT_R16G16B16A16_UNORM);
		CASE_STR(VK_FORMAT_A2B10G10R10_UNORM_PACK32);
		CASE_STR(VK_FORMAT_A2R10G10B10_UNORM_PACK32);
		CASE_STR(VK_FORMAT_B10G11R11_UFLOAT_PACK32);
		// depth formats
		CASE_STR(VK_FORMAT_D16_UNORM);
		CASE_STR(VK_FORMAT_D16_UNORM_S8_UINT);
		CASE_STR(VK_FORMAT_X8_D24_UNORM_PACK32);
		CASE_STR(VK_FORMAT_D24_UNORM_S8_UINT);
		CASE_STR(VK_FORMAT_D32_SFLOAT);
		CASE_STR(VK_FORMAT_D32_SFLOAT_S8_UINT);
	default:
		Com_sprintf(buf, sizeof(buf), "#%i", format);
		return buf;
	}
}

#undef CASE_STR



/*
static VkFlags get_composite_alpha( VkCompositeAlphaFlagsKHR flags )
{
	const VkCompositeAlphaFlagBitsKHR compositeFlags[] = {
		VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR
	};
	int i;

	for ( i = 1; i < ARRAY_LEN( compositeFlags ); i++ ) {
		if ( flags & compositeFlags[i] ) {
			return compositeFlags[i];
		}
	}

	return compositeFlags[0];
}
*/

#define INIT_INSTANCE_FUNCTION(func)                                            \
	q##func = /*(PFN_ ## func)*/ ri.VK_GetInstanceProcAddr(vk_instance, #func); \
	if (q##func == NULL)                                                        \
	{                                                                           \
		ri.Error(ERR_FATAL, "Failed to find entrypoint %s", #func);             \
	}

#define INIT_INSTANCE_FUNCTION_EXT(func) \
	q##func = /*(PFN_ ## func)*/ ri.VK_GetInstanceProcAddr(vk_instance, #func);

#define INIT_DEVICE_FUNCTION(func)                                  \
	q##func = (PFN_##func)qvkGetDeviceProcAddr(vk.device, #func);   \
	if (q##func == NULL)                                            \
	{                                                               \
		ri.Error(ERR_FATAL, "Failed to find entrypoint %s", #func); \
	}

#define INIT_DEVICE_FUNCTION_EXT(func) \
	q##func = (PFN_##func)qvkGetDeviceProcAddr(vk.device, #func);

#undef INIT_INSTANCE_FUNCTION
#undef INIT_DEVICE_FUNCTION
#undef INIT_DEVICE_FUNCTION_EXT

void vk_update_uniform_descriptor(VkDescriptorSet descriptor, VkBuffer buffer)
{
	vk_update_uniform_descriptor_plus(descriptor, buffer);
}

void vk_init_descriptors(void)
{
	vk_init_descriptors_plus();
}

#ifdef USE_VBO
void vk_release_vbo(void)
{
	vk_release_vbo_plus();
}

bool vk_alloc_vbo(const byte *vbo_data, int vbo_size)
{
	return vk_alloc_vbo_plus(vbo_data, vbo_size);
}
#endif

#include "shaders/spirv/shader_data.c"

void vk_create_blur_pipeline(uint32_t index, uint32_t width, uint32_t height, bool horizontal_pass);

void vk_update_post_process_pipelines(void)
{
	vk_update_post_process_pipelines_plus();
}

void vk_initialize(void)
{
	vk_initialize_plus();
}

void vk_create_pipelines(void)
{
	vk_create_pipelines_plus();
}

void vk_shutdown(refShutdownCode_t code)
{
	vk_shutdown_plus(code);
}

void vk_wait_idle(void)
{
	vk_wait_idle_plus();
}

void vk_release_resources(void)
{
	vk_release_resources_plus();
}

void vk_create_image(image_t *image, int width, int height, int mip_levels)
{
	vk_create_image_plus(image, width, height, mip_levels);
}

void vk_upload_image_data(image_t *image, int x, int y, int width, int height, int mipmaps, byte *pixels, int size, bool update)
{
	vk_upload_image_data_plus(image, x, y, width, height, mipmaps, pixels, size, update);
}

void vk_update_descriptor_set(image_t *image, bool mipmap)
{
	vk_update_descriptor_set_plus(image, mipmap);
}

void vk_destroy_image_resources(VkImage *image, VkImageView *imageView)
{
	vk_destroy_image_resources_plus(image, imageView);
}

void vk_create_post_process_pipeline(int program_index, uint32_t width, uint32_t height)
{
	vk_create_post_process_pipeline_plus(program_index, width, height);
}

void vk_create_blur_pipeline(uint32_t index, uint32_t width, uint32_t height, bool horizontal_pass)
{
	vk_create_blur_pipeline_plus(index, width, height, horizontal_pass);
}

VkPipeline create_pipeline(const Vk_Pipeline_Def *def, renderPass_t renderPassIndex)
{
	return create_pipeline_plus(def, renderPassIndex);
}

uint32_t vk_alloc_pipeline(const Vk_Pipeline_Def *def)
{
	return vk_alloc_pipeline_plus(def);
}

VkPipeline vk_gen_pipeline(uint32_t index)
{
	return vk_gen_pipeline_plus(index);
}

uint32_t vk_find_pipeline_ext(uint32_t base, const Vk_Pipeline_Def *def, bool use)
{
	return vk_find_pipeline_ext_plus(base, def, use);
}

void vk_get_pipeline_def(uint32_t pipeline, Vk_Pipeline_Def *def)
{
	vk_get_pipeline_def_plus(pipeline, def);
}

void vk_clear_color(const vec4_t color)
{
	vk_clear_color_plus(color);
}

void vk_clear_depth(bool clear_stencil)
{
	vk_clear_depth_plus(clear_stencil);
}

void vk_update_mvp(const float *m)
{
	vk_update_mvp_plus(m);
}

uint32_t vk_tess_index(uint32_t numIndexes, const void *src)
{
	return vk_tess_index_plus(numIndexes, src);
}

void vk_bind_index_buffer(VkBuffer buffer, uint32_t offset)
{
	vk_bind_index_buffer_plus(buffer, offset);
}

void vk_draw_indexed(uint32_t indexCount, uint32_t firstIndex)
{
	vk_draw_indexed_plus(indexCount, firstIndex);
}

void vk_bind_index(void)
{
	vk_bind_index_plus();
}

void vk_bind_index_ext(const int numIndexes, const uint32_t *indexes)
{
	vk_bind_index_ext_plus(numIndexes, indexes);
}

void vk_bind_geometry(uint32_t flags)
{
	vk_bind_geometry_plus(flags);
}

void vk_bind_lighting(int stage, int bundle)
{
	vk_bind_lighting_plus(stage, bundle);
}

void vk_reset_descriptor(int index)
{
	vk_reset_descriptor_plus(index);
}

void vk_update_descriptor(int index, VkDescriptorSet descriptor)
{
	vk_update_descriptor_plus(index, descriptor);
}

void vk_update_descriptor_offset(int index, uint32_t offset)
{
	vk_update_descriptor_offset_plus(index, offset);
}

void vk_bind_descriptor_sets(void)
{
	vk_bind_descriptor_sets_plus();
}
void vk_bind_pipeline(uint32_t pipeline)
{
	vk_bind_pipeline_plus(pipeline);
}

void vk_draw_geometry(Vk_Depth_Range depth_range, bool indexed)
{
	vk_draw_geometry_plus(depth_range, indexed);
}

void vk_begin_main_render_pass(void)
{
	vk_begin_main_render_pass_plus();
}

void vk_begin_post_bloom_render_pass(void)
{
	vk_begin_post_bloom_render_pass_plus();
}

void vk_begin_bloom_extract_render_pass(void)
{
	vk_begin_bloom_extract_render_pass_plus();
}

void vk_begin_blur_render_pass(uint32_t index)
{
	vk_begin_blur_render_pass_plus();
}

void vk_end_render_pass(void)
{
	vk_end_render_pass_plus();
}

#ifndef UINT64_MAX
#define UINT64_MAX 0xFFFFFFFFFFFFFFFFULL
#endif

void vk_begin_frame(void)
{
	vk_begin_frame_plus();
}

void vk_end_frame(void)
{
	vk_end_frame_plus();
}

void vk_present_frame(void)
{
	vk_present_frame_plus();
}

void vk_read_pixels(byte *buffer, uint32_t width, uint32_t height)
{
	vk_read_pixels_plus(buffer, width, height);
}

bool vk_bloom(void)
{
	return vk_bloom_plus();
}
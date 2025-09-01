#include "vk.hpp"
#include <stdexcept>
#include <algorithm>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_handles.hpp>
#include "math.hpp"
#include "tr_local.hpp"
#include "utils.hpp"

#if defined(_DEBUG)
#if defined(_WIN32)
#define USE_VK_VALIDATION
#include <windows.h> // for win32 debug callback
#endif

#endif

#include <unordered_map>
#include <numeric>
#include "string_operations.hpp"

static vk::SampleCountFlagBits vkSamples = vk::SampleCountFlagBits::e1;
static vk::SampleCountFlagBits vkMaxSamples = vk::SampleCountFlagBits::e1;

static vk::Instance vk_instance = nullptr;
static VkSurfaceKHR vk_surfaceC = VK_NULL_HANDLE;
static vk::SurfaceKHR vk_surface = nullptr;

constexpr int defaultVulkanApiVersion = VK_API_VERSION_1_0;
static int vulkanApiVersion = defaultVulkanApiVersion;

constexpr int MIN_SWAPCHAIN_IMAGES = 2;
constexpr int MIN_SWAPCHAIN_IMAGES_FIFO = 3;

constexpr int  VERTEX_BUFFER_SIZE     = (4 * 1024 * 1024);  /* by default */
constexpr int  VERTEX_BUFFER_SIZE_HI  = (8 * 1024 * 1024);

constexpr int  STAGING_BUFFER_SIZE    = (2 * 1024 * 1024);  /* by default */
constexpr int  STAGING_BUFFER_SIZE_HI = (24 * 1024 * 1024); /* enough for max.texture size upload with all mip levels at once */

constexpr int IMAGE_CHUNK_SIZE = (32 * 1024 * 1024);

#ifdef USE_VK_VALIDATION
VkDebugReportCallbackEXT vk_debug_callback = VK_NULL_HANDLE;
#endif

//
// Vulkan API functions used by the renderer.
//

static PFN_vkGetDeviceProcAddr qvkGetDeviceProcAddr;

#ifdef USE_VK_VALIDATION
static PFN_vkCreateDebugReportCallbackEXT qvkCreateDebugReportCallbackEXT;
static PFN_vkDestroyDebugReportCallbackEXT qvkDestroyDebugReportCallbackEXT;
#endif

static PFN_vkDebugMarkerSetObjectNameEXT qvkDebugMarkerSetObjectNameEXT;

#ifdef USE_VK_VALIDATION
#define VK_CHECK(function_call)                                                                        \
	{                                                                                                  \
		try                                                                                            \
		{                                                                                              \
			function_call;                                                                             \
		}                                                                                              \
		catch (vk::SystemError & err)                                                                  \
		{                                                                                              \
			ri.Error(ERR_FATAL, "Vulkan error in function: %s, what: %s", #function_call, err.what()); \
		}                                                                                              \
	}

#define VK_CHECK_ASSIGN(var, function_call)                                                            \
	{                                                                                                  \
		try                                                                                            \
		{                                                                                              \
			var = function_call;                                                                       \
		}                                                                                              \
		catch (vk::SystemError & err)                                                                  \
		{                                                                                              \
			ri.Error(ERR_FATAL, "Vulkan error in function: %s, what: %s", #function_call, err.what()); \
		}                                                                                              \
	}

#else

template <typename T>
inline void vkCheckFunctionCall(const vk::ResultValue<T> res, const char *funcName)
{
	if (static_cast<int>(res.result) < 0)
	{
		ri.Error(ERR_FATAL, "Vulkan: %s returned %s", funcName, vk::to_string(res.result).data());
	}
}

static inline void vkCheckFunctionCall(const vk::Result res, const char *funcName)
{
	if (static_cast<int>(res) < 0)
	{
		ri.Error(ERR_FATAL, "Vulkan: %s returned %s", funcName, vk::to_string(res).data());
	}
}

#define VK_CHECK_ASSIGN(var, function_call)                 \
	{                                                       \
		auto result = function_call;                        \
		vkCheckFunctionCall(result.result, #function_call); \
		var = result.value;                                 \
	}

#define VK_CHECK(function_call)                             \
	{                                                       \
		vkCheckFunctionCall(function_call, #function_call); \
	}

#endif

static constexpr vk::SampleCountFlagBits convertToVkSampleCountFlagBits(const int sampleCountInt)
{
	switch (sampleCountInt)
	{
	case 1:
		return vk::SampleCountFlagBits::e1;
	case 2:
		return vk::SampleCountFlagBits::e2;
	case 4:
		return vk::SampleCountFlagBits::e4;
	case 8:
		return vk::SampleCountFlagBits::e8;
	case 16:
		return vk::SampleCountFlagBits::e16;
	case 32:
		return vk::SampleCountFlagBits::e32;
	case 64:
		return vk::SampleCountFlagBits::e64;
	default:
		// Handle unsupported sample count
		// For example, throw an exception or return a default value
		ri.Printf(PRINT_DEVELOPER, "Wrong vk_inst.simple count, use 1 bit: %d\n", sampleCountInt);
		return vk::SampleCountFlagBits::e1;
	}
}

////////////////////////////////////////////////////////////////////////////

static inline uint32_t find_memory_type(const uint32_t memory_type_bits, const vk::MemoryPropertyFlags properties)
{
	uint32_t i;

	auto memory_properties = vk_inst.physical_device.getMemoryProperties();

	for (i = 0; i < memory_properties.memoryTypeCount; i++)
	{
		if ((memory_type_bits & (1 << i)) != 0 &&
			(memory_properties.memoryTypes[i].propertyFlags & properties) == properties)
		{
			return i;
		}
	}
	ri.Error(ERR_FATAL, "Vulkan: failed to find matching memory type with requested properties");
	return ~0U;
}

static inline uint32_t find_memory_type2(const uint32_t memory_type_bits, const vk::MemoryPropertyFlags properties, vk::MemoryPropertyFlags *outprops)
{
	const vk::PhysicalDeviceMemoryProperties memory_properties = vk_inst.physical_device.getMemoryProperties();

	for (uint32_t i = 0; i < memory_properties.memoryTypeCount; i++)
	{
		if ((memory_type_bits & (1 << i)) != 0 && (memory_properties.memoryTypes[i].propertyFlags & properties) == properties)
		{
			if (outprops)
			{
				*outprops = memory_properties.memoryTypes[i].propertyFlags;
			}
			return i;
		}
	}

	return ~0U;
}

static vk::CommandBuffer begin_command_buffer(void)
{
	vk::CommandBuffer command_buffer;

	vk::CommandBufferAllocateInfo alloc_info{
		vk_inst.command_pool,
		vk::CommandBufferLevel::ePrimary,
		1,
		nullptr};

	VK_CHECK(vk_inst.device.allocateCommandBuffers(&alloc_info, &command_buffer));

	vk::CommandBufferBeginInfo begin_info{
		vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
		nullptr,
		nullptr};

	VK_CHECK(command_buffer.begin(begin_info));

	return command_buffer;
}

static void end_command_buffer(const vk::CommandBuffer &command_buffer, const char *location)
{
#ifdef USE_UPLOAD_QUEUE
	const vk::PipelineStageFlags wait_dst_stage_mask = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
	vk::Semaphore waits{};
#endif

	VK_CHECK(command_buffer.end());

	vk::SubmitInfo submit_info{0,
							   nullptr,
							   nullptr,
							   1,
							   &command_buffer,
							   0,
							   nullptr};
#ifdef USE_UPLOAD_QUEUE
    if (vk_inst.rendering_finished != vk::Semaphore{}) {
        waits = vk_inst.rendering_finished;
        vk_inst.rendering_finished = vk::Semaphore{};   // clear to "null"

		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = &waits;
		submit_info.pWaitDstStageMask = &wait_dst_stage_mask;
    } else
#endif
    {
        //submitInfo.setCommandBuffers(cmdbuf);
    }

	VK_CHECK(vk_inst.queue.submit(submit_info, nullptr));
	
	vk_queue_wait_idle();

	vk_inst.device.freeCommandBuffers(vk_inst.command_pool,
									  1,
									  &command_buffer);
}

static constexpr vk::PipelineStageFlagBits get_src_stage(const vk::ImageLayout old_layout) noexcept
{
	switch (old_layout)
	{
	case vk::ImageLayout::eUndefined:
		return vk::PipelineStageFlagBits::eTopOfPipe;
	case vk::ImageLayout::eTransferDstOptimal:
	case vk::ImageLayout::eTransferSrcOptimal:
		return vk::PipelineStageFlagBits::eTransfer;
	case vk::ImageLayout::eShaderReadOnlyOptimal:
		return vk::PipelineStageFlagBits::eFragmentShader;
	case vk::ImageLayout::ePresentSrcKHR:
		return vk::PipelineStageFlagBits::eTransfer;
	default:
		return vk::PipelineStageFlagBits::eAllCommands;
	}
}

static constexpr vk::AccessFlags get_src_access_mask(const vk::ImageLayout old_layout)
{
	switch (old_layout)
	{
	case vk::ImageLayout::eUndefined:
		return vk::AccessFlagBits::eNone;
	case vk::ImageLayout::eTransferDstOptimal:
		return vk::AccessFlagBits::eTransferWrite;
	case vk::ImageLayout::eTransferSrcOptimal:
		return vk::AccessFlagBits::eTransferRead;
	case vk::ImageLayout::eShaderReadOnlyOptimal:
		return vk::AccessFlagBits::eShaderRead;
	case vk::ImageLayout::ePresentSrcKHR:
		return vk::AccessFlagBits::eNone;
	default:
		return vk::AccessFlagBits::eNone;
	}
}

static constexpr vk::PipelineStageFlagBits get_dst_stage(const vk::ImageLayout new_layout)
{
	switch (new_layout)
	{
	case vk::ImageLayout::eColorAttachmentOptimal:
		return vk::PipelineStageFlagBits::eColorAttachmentOutput;
	case vk::ImageLayout::eDepthStencilAttachmentOptimal:
		return vk::PipelineStageFlagBits::eEarlyFragmentTests;
	case vk::ImageLayout::ePresentSrcKHR:
		return vk::PipelineStageFlagBits::eTransfer;
	case vk::ImageLayout::eTransferSrcOptimal:
		return vk::PipelineStageFlagBits::eTransfer;
	case vk::ImageLayout::eTransferDstOptimal:
		return vk::PipelineStageFlagBits::eTransfer;
	case vk::ImageLayout::eShaderReadOnlyOptimal:
		return vk::PipelineStageFlagBits::eFragmentShader;
	default:
		return vk::PipelineStageFlagBits::eAllCommands;
	}
}

static constexpr vk::AccessFlags get_dst_access_mask(const vk::ImageLayout new_layout)
{
	switch (new_layout)
	{
	case vk::ImageLayout::eColorAttachmentOptimal:
		return vk::AccessFlagBits::eColorAttachmentWrite;
	case vk::ImageLayout::eDepthStencilAttachmentOptimal:
		return vk::AccessFlagBits::eDepthStencilAttachmentWrite;
	case vk::ImageLayout::ePresentSrcKHR:
		return vk::AccessFlagBits::eNone;
	case vk::ImageLayout::eTransferSrcOptimal:
		return vk::AccessFlagBits::eTransferRead;
	case vk::ImageLayout::eTransferDstOptimal:
		return vk::AccessFlagBits::eTransferWrite;
	case vk::ImageLayout::eShaderReadOnlyOptimal:
		return vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eInputAttachmentRead;
	default:
		return vk::AccessFlagBits::eNone;
	}
}

static void record_image_layout_transition(const vk::CommandBuffer &command_buffer, const vk::Image &image,
										   const vk::ImageAspectFlags image_aspect_flags, 
										   const vk::ImageLayout old_layout, 
										   const vk::ImageLayout new_layout, 
    vk::PipelineStageFlags src_stage_override = {},  // empty => no override
    vk::PipelineStageFlags dst_stage_override = {}   // empty => no override
)
{
	vk::ImageMemoryBarrier barrier{get_src_access_mask(old_layout),
								   get_dst_access_mask(new_layout),
								   old_layout,
								   new_layout,
								   vk::QueueFamilyIgnored,
								   vk::QueueFamilyIgnored,
								   image,
								   {image_aspect_flags,
									0,
									vk::RemainingMipLevels,
									0,
									vk::RemainingArrayLayers},
								   nullptr};

    vk::PipelineStageFlags stageFlag = vk::PipelineStageFlags{ get_src_stage(old_layout) };
    vk::PipelineStageFlags destFlag = vk::PipelineStageFlags{ get_dst_stage(new_layout) };


	//auto stageFlag = get_src_stage(old_layout);
	if (stageFlag & vk::PipelineStageFlagBits::eAllCommands)
		ri.Error(ERR_DROP, "unsupported old layout %i", (int)old_layout);

	if (old_layout == vk::ImageLayout::eUndefined)
	{
		if (src_stage_override)
			stageFlag = src_stage_override;
	}

	//auto destFlag = get_dst_stage(new_layout);
	if (destFlag & vk::PipelineStageFlagBits::eAllCommands)
		ri.Error(ERR_DROP, "unsupported new layout %i", (int)new_layout);

	if(dst_stage_override)
		destFlag = dst_stage_override;

	command_buffer.pipelineBarrier(stageFlag,
								   destFlag,
								   {},
								   0,
								   nullptr,
								   0,
								   nullptr,
								   1,
								   &barrier);
}

// debug markers
#ifdef USE_VK_VALIDATION
#define SET_OBJECT_NAME(obj, objName, objType) vk_set_object_name((uint64_t)(obj), (objName), (objType))

static void vk_set_object_name(uint64_t obj, const char *objName, VkDebugReportObjectTypeEXT objType)
{
	if (qvkDebugMarkerSetObjectNameEXT && obj)
	{
		VkDebugMarkerObjectNameInfoEXT info{
			VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT,
			nullptr,
			objType,
			obj,
			objName};
		qvkDebugMarkerSetObjectNameEXT(vk_inst.device, &info);
	}
}
#endif

static void vk_create_swapchain(const vk::PhysicalDevice &physical_device, const vk::Device &device, const vk::SurfaceKHR &surface, const vk::SurfaceFormatKHR &surface_format, vk::SwapchainKHR *swapchain, bool verbose)
{

	vk::SurfaceCapabilitiesKHR surface_caps;
	vk::Extent2D image_extent;
	uint32_t present_mode_count, i;
	vk::PresentModeKHR present_mode;
	// vk::PresentModeKHR *present_modes;
	bool mailbox_supported = false;
	bool immediate_supported = false;
	bool fifo_relaxed_supported = false;
	int v;

	VK_CHECK_ASSIGN(surface_caps, physical_device.getSurfaceCapabilitiesKHR(surface));

	image_extent = surface_caps.currentExtent;
	if (image_extent.width == 0xffffffff && image_extent.height == 0xffffffff)
	{
		image_extent.width = MIN(surface_caps.maxImageExtent.width, MAX(surface_caps.minImageExtent.width, (uint32_t)glConfig.vidWidth));
		image_extent.height = MIN(surface_caps.maxImageExtent.height, MAX(surface_caps.minImageExtent.height, (uint32_t)glConfig.vidHeight));
	}

	vk_inst.clearAttachment = true;

	if (!vk_inst.fboActive)
	{
		// vk::ImageUsageFlagBits::eTransferDst is required by image clear operations.
		if ((surface_caps.supportedUsageFlags & vk::ImageUsageFlagBits::eTransferDst) == vk::ImageUsageFlags{})
		{
			vk_inst.clearAttachment = false;
			ri.Printf(PRINT_WARNING, "vk::ImageUsageFlagBits::eTransferDst is not supported by the swapchain, \\r_clear might not work\n" );
		}

		// vk::ImageUsageFlagBits::eTransferSrc is required in order to take screenshots.
		if ((surface_caps.supportedUsageFlags & vk::ImageUsageFlagBits::eTransferSrc) == vk::ImageUsageFlags{})
		{
			ri.Error(ERR_FATAL, "create_swapchain: vk::ImageUsageFlagBits::eTransferSrc is not supported by the swapchain");
		}
	}

	// determine present mode and swapchain image count
	std::vector<vk::PresentModeKHR> present_modes;
	VK_CHECK_ASSIGN(present_modes, physical_device.getSurfacePresentModesKHR(surface));
	present_mode_count = present_modes.size();
	// VK_CHECK(qvkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, nullptr));

	// present_modes = (VkPresentModeKHR *)ri.Malloc(present_mode_count * sizeof(VkPresentModeKHR));
	// VK_CHECK(qvkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, present_modes));

	if ( verbose ) {
		ri.Printf( PRINT_ALL, "...presentation modes:" );
	}
	for (i = 0; i < present_mode_count; i++)
	{
		if ( verbose ) {
			ri.Printf(PRINT_ALL, " %s", vk::to_string(present_modes[i]).data());
		}
		if (present_modes[i] == vk::PresentModeKHR::eMailbox)
			mailbox_supported = true;
		else if (present_modes[i] == vk::PresentModeKHR::eImmediate)
			immediate_supported = true;
		else if (present_modes[i] == vk::PresentModeKHR::eFifoRelaxed)
			fifo_relaxed_supported = true;
	}
	if ( verbose ) {
		ri.Printf( PRINT_ALL, "\n" );
	}

	uint32_t image_count = MAX(MIN_SWAPCHAIN_IMAGES, surface_caps.minImageCount);
	if (surface_caps.maxImageCount > 0)
	{
		image_count = MIN(image_count, surface_caps.maxImageCount);
	}

	if (immediate_supported)
	{
		present_mode = vk::PresentModeKHR::eImmediate;
	}
	else if (mailbox_supported)
	{
		present_mode = vk::PresentModeKHR::eMailbox;
	}
	else if (fifo_relaxed_supported)
	{
		present_mode = vk::PresentModeKHR::eFifoRelaxed;
	}
	else
	{
		present_mode = vk::PresentModeKHR::eFifo;
	}

	if ((v = ri.Cvar_VariableIntegerValue("r_swapInterval")) != 0)
	{
		if (v == 2 && mailbox_supported)
			present_mode = vk::PresentModeKHR::eMailbox;
		else if (fifo_relaxed_supported)
			present_mode = vk::PresentModeKHR::eFifoRelaxed;
		else
			present_mode = vk::PresentModeKHR::eFifo;
	}

	if (surface_caps.maxImageCount == 0 && present_mode == vk::PresentModeKHR::eFifo)
	{
		image_count = MAX(MIN_SWAPCHAIN_IMAGES_FIFO, surface_caps.minImageCount);
	}

	if ( verbose ) {
		ri.Printf(PRINT_ALL, "...selected presentation mode: %s, image count: %i\n", vk::to_string(present_mode).data(), image_count);
	}

	// create swap chain
	vk::SwapchainCreateInfoKHR desc{{},
									surface,
									image_count,
									surface_format.format,
									surface_format.colorSpace,
									image_extent,
									1,
									vk::ImageUsageFlagBits::eColorAttachment,
									vk::SharingMode::eExclusive,
									0,
									nullptr,
									surface_caps.currentTransform,
									vk::CompositeAlphaFlagBitsKHR::eOpaque,
									present_mode,
									vk::True,
									nullptr,
									nullptr};

	if (!vk_inst.fboActive)
	{
		desc.imageUsage |= vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc;
	}

	VK_CHECK_ASSIGN(*swapchain, device.createSwapchainKHR(desc));

	std::vector<vk::Image> swapchainImages;
	VK_CHECK_ASSIGN(swapchainImages, vk_inst.device.getSwapchainImagesKHR(vk_inst.swapchain));
	vk_inst.swapchain_image_count = MIN(swapchainImages.size(), MAX_SWAPCHAIN_IMAGES);

	std::copy(swapchainImages.begin(), swapchainImages.end(), vk_inst.swapchain_images);

	for (i = 0; i < vk_inst.swapchain_image_count; i++)
	{
		vk::ImageViewCreateInfo view{{},
									 vk_inst.swapchain_images[i],
									 vk::ImageViewType::e2D,
									 vk_inst.present_format.format,
									 {},
									 {vk::ImageAspectFlagBits::eColor,
									  0,
									  1,
									  0,
									  1},
									 nullptr};

		VK_CHECK_ASSIGN(vk_inst.swapchain_image_views[i], vk_inst.device.createImageView(view));
#ifdef USE_VK_VALIDATION
		SET_OBJECT_NAME(VkImage(vk_inst.swapchain_images[i]), va("swapchain image %i", i), VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT);
		SET_OBJECT_NAME(VkImageView(vk_inst.swapchain_image_views[i]), va("swapchain image %i", i), VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT);
#endif
	}

	for ( i = 0; i < vk_inst.swapchain_image_count; i++ ) {
		VK_CHECK_ASSIGN(vk_inst.swapchain_rendering_finished[i], vk_inst.device.createSemaphore({}));
		#ifdef USE_VK_VALIDATION
		SET_OBJECT_NAME( VkSemaphore(vk_inst.swapchain_rendering_finished[i]), va( "swapchain_rendering_finished semaphore %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_SEMAPHORE_EXT );
		#endif
	}

	if (vk_inst.initSwapchainLayout != vk::ImageLayout::eUndefined)
	{
		vk::CommandBuffer command_buffer = begin_command_buffer();

		for (i = 0; i < vk_inst.swapchain_image_count; i++)
		{
			record_image_layout_transition(command_buffer, vk_inst.swapchain_images[i],
										   vk::ImageAspectFlagBits::eColor,
										   vk::ImageLayout::eUndefined, vk_inst.initSwapchainLayout);
		}

		end_command_buffer(command_buffer, __func__);
	}
}

static void vk_create_render_passes(void)
{
	vk::Format depth_format = vk_inst.depth_format;
	vk::Device device = vk_inst.device;
	vk::AttachmentDescription attachments[3]; // color | depth | msaa color
	vk::AttachmentReference colorRef0, depthRef0;
	vk::SubpassDependency deps[3]{};
	vk::RenderPassCreateInfo desc{};

	vk::AttachmentReference colorResolveRef = {0, vk::ImageLayout::eColorAttachmentOptimal}; // Not UNDEFINED

	if (r_fbo->integer == 0)
	{
		// presentation
		attachments[0].format = vk_inst.present_format.format;
		attachments[0].samples = vk::SampleCountFlagBits::e1;
#ifdef USE_BUFFER_CLEAR
		attachments[0].loadOp = vk::AttachmentLoadOp::eClear;
#else
		attachments[0].loadOp = vk::AttachmentLoadOp::eDontCare; // Assuming this will be completely overwritten
#endif
		attachments[0].storeOp = vk::AttachmentStoreOp::eStore; // needed for presentation
		attachments[0].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
		attachments[0].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
		attachments[0].initialLayout = vk_inst.initSwapchainLayout;
		attachments[0].finalLayout = vk::ImageLayout::ePresentSrcKHR;
	}
	else
	{
		// resolve/color buffer
		attachments[0].format = vk_inst.color_format;
		attachments[0].samples = vk::SampleCountFlagBits::e1;

#ifdef USE_BUFFER_CLEAR
		if (vk_inst.msaaActive)
			attachments[0].loadOp = vk::AttachmentLoadOp::eDontCare; // Assuming this will be completely overwritten
		else
			attachments[0].loadOp = vk::AttachmentLoadOp::eClear;
#else
		attachments[0].loadOp = vk::AttachmentLoadOp::eDontCare; // Assuming this will be completely overwritten
#endif

		attachments[0].storeOp = vk::AttachmentStoreOp::eStore; // needed for next render pass
		attachments[0].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
		attachments[0].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
		attachments[0].initialLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
		attachments[0].finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
	}

	// depth buffer
	attachments[1].format = depth_format;
	attachments[1].samples = vkSamples;
	attachments[1].loadOp = vk::AttachmentLoadOp::eClear; // Need empty depth buffer before use
	attachments[1].stencilLoadOp = glConfig.stencilBits ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eDontCare;
	if (r_bloom->integer)
	{
		attachments[1].storeOp = vk::AttachmentStoreOp::eStore; // keep it for post-bloom pass
		attachments[1].stencilStoreOp = glConfig.stencilBits ? vk::AttachmentStoreOp::eStore : vk::AttachmentStoreOp::eDontCare;
	}
	else
	{
		attachments[1].storeOp = vk::AttachmentStoreOp::eDontCare;
		attachments[1].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
	}
	attachments[1].initialLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
	attachments[1].finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

	colorRef0 = {0, vk::ImageLayout::eColorAttachmentOptimal};
	depthRef0 = {1, vk::ImageLayout::eDepthStencilAttachmentOptimal};

	vk::SubpassDescription subpass{{},
								   vk::PipelineBindPoint::eGraphics,
								   0,
								   nullptr,
								   1,
								   &colorRef0,
								   &colorResolveRef,
								   &depthRef0,
								   0,
								   nullptr};

	desc = {{}, 2, attachments, 1, &subpass, 0, nullptr};

	if (vk_inst.msaaActive)
	{
		attachments[2].format = vk_inst.color_format;
		attachments[2].samples = vkSamples;
#ifdef USE_BUFFER_CLEAR
		attachments[2].loadOp = vk::AttachmentLoadOp::eClear;
#else
		attachments[2].loadOp = vk::AttachmentLoadOp::eDontCare;
#endif
		if (r_bloom->integer)
		{
			attachments[2].storeOp = vk::AttachmentStoreOp::eStore; // keep it for post-bloom pass
		}
		else
		{
			attachments[2].storeOp = vk::AttachmentStoreOp::eDontCare; // Intermediate storage (not written)
		}
		attachments[2].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
		attachments[2].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
		attachments[2].initialLayout = vk::ImageLayout::eColorAttachmentOptimal;
		attachments[2].finalLayout = vk::ImageLayout::eColorAttachmentOptimal;

		desc.attachmentCount = 3;

		colorRef0.attachment = 2; // msaa image attachment
		colorRef0.layout = vk::ImageLayout::eColorAttachmentOptimal;

		colorResolveRef.attachment = 0; // resolve image attachment
		colorResolveRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

		subpass.pResolveAttachments = &colorResolveRef;
	}
	else
	{
		// Don't use resolve attachments when MSAA is disabled
		subpass.pResolveAttachments = nullptr;
	}

	Com_Memset(&deps, 0, sizeof(deps));

	deps[2].srcSubpass = VK_SUBPASS_EXTERNAL;
	deps[2].dstSubpass = 0;
	deps[2].srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput; // What pipeline stage is waiting on the dependency
	deps[2].dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput; // What pipeline stage is waiting on the dependency
	deps[2].srcAccessMask = vk::AccessFlagBits::eColorAttachmentRead;		  // What access scopes are influence the dependency
	deps[2].dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;		  // What access scopes are waiting on the dependency
	deps[2].dependencyFlags = {};

	if (r_fbo->integer == 0)
	{
		desc.dependencyCount = 1;
		desc.pDependencies = &deps[2];
		VK_CHECK_ASSIGN(vk_inst.render_pass.main, device.createRenderPass(desc));
#ifdef USE_VK_VALIDATION
		SET_OBJECT_NAME(VkRenderPass(vk_inst.render_pass.main), "render pass - main", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT);
#endif
		return;
	}

	deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	deps[0].dstSubpass = 0;
	deps[0].srcStageMask = vk::PipelineStageFlagBits::eFragmentShader;											  // What pipeline stage must have completed for the dependency
	deps[0].dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;									  // What pipeline stage is waiting on the dependency
	deps[0].srcAccessMask = vk::AccessFlagBits::eShaderRead;													  // What access scopes are influence the dependency
	deps[0].dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite; // What access scopes are waiting on the dependency
	deps[0].dependencyFlags = vk::DependencyFlagBits::eByRegion;												  // Only need the current fragment (or tile) synchronized, not the whole framebuffer

	deps[1].srcSubpass = 0;
	deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	deps[1].srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput; // Fragment data has been written
	deps[1].dstStageMask = vk::PipelineStageFlagBits::eFragmentShader;		  // Don't start shading until data is available
	deps[1].srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;		  // Waiting for color data to be written
	deps[1].dstAccessMask = vk::AccessFlagBits::eShaderRead;				  // Don't read things from the shader before ready
	deps[1].dependencyFlags = vk::DependencyFlagBits::eByRegion;			  // Only need the current fragment (or tile) synchronized, not the whole framebuffer

	desc.dependencyCount = 2;
	desc.pDependencies = &deps[0];

	VK_CHECK_ASSIGN(vk_inst.render_pass.main, device.createRenderPass(desc));
#ifdef USE_VK_VALIDATION
	SET_OBJECT_NAME(VkRenderPass(vk_inst.render_pass.main), "render pass - main", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT);
#endif
	if (r_bloom->integer)
	{
		// post-bloom pass
		// color buffer
		attachments[0].loadOp = vk::AttachmentLoadOp::eLoad;
		// depth buffer
		attachments[1].loadOp = vk::AttachmentLoadOp::eLoad;
		attachments[1].storeOp = vk::AttachmentStoreOp::eDontCare;
		attachments[1].stencilLoadOp = glConfig.stencilBits ? vk::AttachmentLoadOp::eLoad : vk::AttachmentLoadOp::eDontCare;

		if (vk_inst.msaaActive)
		{
			// msaa render target
			attachments[2].loadOp = vk::AttachmentLoadOp::eLoad;
			attachments[2].storeOp = vk::AttachmentStoreOp::eDontCare;
		}

		VK_CHECK_ASSIGN(vk_inst.render_pass.post_bloom, device.createRenderPass(desc));
#ifdef USE_VK_VALIDATION
		SET_OBJECT_NAME(VkRenderPass(vk_inst.render_pass.post_bloom), "render pass - post_bloom", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT);
#endif

		// bloom extraction, using resolved/main fbo as a source
		desc.attachmentCount = 1;

		colorRef0.attachment = 0;
		colorRef0.layout = vk::ImageLayout::eColorAttachmentOptimal;

		subpass = {{},
				   vk::PipelineBindPoint::eGraphics,
				   0,
				   nullptr,
				   1,
				   &colorRef0,
				   nullptr,
				   nullptr,
				   0,
				   nullptr};

		attachments[0].format = vk_inst.bloom_format;
		attachments[0].samples = vk::SampleCountFlagBits::e1;
		attachments[0].loadOp = vk::AttachmentLoadOp::eDontCare; // Assuming this will be completely overwritten
		attachments[0].storeOp = vk::AttachmentStoreOp::eStore;	 // needed for next render pass
		attachments[0].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
		attachments[0].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
		attachments[0].initialLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
		attachments[0].finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

		VK_CHECK_ASSIGN(vk_inst.render_pass.bloom_extract, device.createRenderPass(desc));
#ifdef USE_VK_VALIDATION
		SET_OBJECT_NAME(VkRenderPass(vk_inst.render_pass.bloom_extract), "render pass - bloom_extract", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT);
#endif

		for (auto &blur : vk_inst.render_pass.blur)
		{
			VK_CHECK_ASSIGN(blur, device.createRenderPass(desc));
#ifdef USE_VK_VALIDATION
			SET_OBJECT_NAME(VkRenderPass(blur), "render pass - blur", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT);
#endif
		}
	}

	if (vk_inst.capture.image)
	{

		subpass = {{},
				   vk::PipelineBindPoint::eGraphics,
				   0,
				   nullptr,
				   1,
				   &colorRef0,
				   nullptr,
				   nullptr,
				   0,
				   nullptr};

		attachments[0].format = vk_inst.capture_format;
		attachments[0].samples = vk::SampleCountFlagBits::e1;
		attachments[0].loadOp = vk::AttachmentLoadOp::eDontCare; // this will be completely overwritten
		attachments[0].storeOp = vk::AttachmentStoreOp::eStore;	 // needed for next render pass
		attachments[0].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
		attachments[0].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
		attachments[0].initialLayout = vk::ImageLayout::eUndefined;
		attachments[0].finalLayout = vk::ImageLayout::eTransferSrcOptimal;

		colorRef0.attachment = 0;
		colorRef0.layout = vk::ImageLayout::eColorAttachmentOptimal;

		desc.sType = vk::StructureType::eRenderPassCreateInfo;
		desc.pNext = nullptr;
		desc.pAttachments = attachments;
		desc.attachmentCount = 1;
		desc.pSubpasses = &subpass;
		desc.subpassCount = 1;

		VK_CHECK_ASSIGN(vk_inst.render_pass.capture, device.createRenderPass(desc));
#ifdef USE_VK_VALIDATION
		SET_OBJECT_NAME(VkRenderPass(vk_inst.render_pass.capture), "render pass - capture", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT);
#endif
	}

	colorRef0.attachment = 0;
	colorRef0.layout = vk::ImageLayout::eColorAttachmentOptimal;

	desc.attachmentCount = 1;

	subpass = {{},
			   vk::PipelineBindPoint::eGraphics,
			   0,
			   nullptr,
			   1,
			   &colorRef0,
			   nullptr,
			   nullptr,
			   0,
			   nullptr};

	// gamma post-processing
	attachments[0].format = vk_inst.present_format.format;
	attachments[0].samples = vk::SampleCountFlagBits::e1;
	attachments[0].loadOp = vk::AttachmentLoadOp::eDontCare;
	attachments[0].storeOp = vk::AttachmentStoreOp::eStore; // needed for presentation
	attachments[0].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
	attachments[0].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
	attachments[0].initialLayout = vk_inst.initSwapchainLayout;
	attachments[0].finalLayout = vk::ImageLayout::ePresentSrcKHR;

	desc.dependencyCount = 1;
	desc.pDependencies = &deps[2];

	VK_CHECK_ASSIGN(vk_inst.render_pass.gamma, device.createRenderPass(desc));
#ifdef USE_VK_VALIDATION
	SET_OBJECT_NAME(VkRenderPass(vk_inst.render_pass.gamma), "render pass - gamma", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT);
#endif
	desc.dependencyCount = 2;
	desc.pDependencies = &deps[0];

	// screenmap resolve/color buffer
	attachments[0].format = vk_inst.color_format;
	attachments[0].samples = vk::SampleCountFlagBits::e1;
#ifdef USE_BUFFER_CLEAR
	if (vk_inst.screenMapSamples > vk::SampleCountFlagBits::e1)
		attachments[0].loadOp = vk::AttachmentLoadOp::eDontCare;
	else
		attachments[0].loadOp = vk::AttachmentLoadOp::eClear;
#else
	attachments[0].loadOp = vk::AttachmentLoadOp::eDontCare; // Assuming this will be completely overwritten
#endif
	attachments[0].storeOp = vk::AttachmentStoreOp::eStore; // needed for next render pass
	attachments[0].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
	attachments[0].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
	attachments[0].initialLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
	attachments[0].finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

	// screenmap depth buffer
	attachments[1].format = depth_format;
	attachments[1].samples = vk_inst.screenMapSamples;
	attachments[1].loadOp = vk::AttachmentLoadOp::eClear; // Need empty depth buffer before use
	attachments[1].storeOp = vk::AttachmentStoreOp::eDontCare;
	attachments[1].stencilLoadOp = vk::AttachmentLoadOp::eClear;
	attachments[1].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
	attachments[1].initialLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
	attachments[1].finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

	colorRef0.attachment = 0;
	colorRef0.layout = vk::ImageLayout::eColorAttachmentOptimal;

	depthRef0.attachment = 1;
	depthRef0.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

	subpass = {{},
			   vk::PipelineBindPoint::eGraphics,
			   0,
			   nullptr,
			   1,
			   &colorRef0,
			   nullptr,
			   &depthRef0,
			   0,
			   nullptr};

	desc = {{}, 1, attachments, 1, &subpass, 2, deps};

	if (vk_inst.screenMapSamples > vk::SampleCountFlagBits::e1)
	{
		attachments[2].format = vk_inst.color_format;
		attachments[2].samples = vk_inst.screenMapSamples;
#ifdef USE_BUFFER_CLEAR
		attachments[2].loadOp = vk::AttachmentLoadOp::eClear;
#else
		attachments[2].loadOp = vk::AttachmentLoadOp::eDontCare;
#endif
		attachments[2].storeOp = vk::AttachmentStoreOp::eDontCare;
		attachments[2].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
		attachments[2].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
		attachments[2].initialLayout = vk::ImageLayout::eColorAttachmentOptimal;
		attachments[2].finalLayout = vk::ImageLayout::eColorAttachmentOptimal;

		desc.attachmentCount = 3;

		colorRef0.attachment = 2; // screenmap msaa image attachment
		colorRef0.layout = vk::ImageLayout::eColorAttachmentOptimal;

		colorResolveRef.attachment = 0; // screenmap resolve image attachment
		colorResolveRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

		subpass.pResolveAttachments = &colorResolveRef;
	}

	VK_CHECK_ASSIGN(vk_inst.render_pass.screenmap, device.createRenderPass(desc));
#ifdef USE_VK_VALIDATION
	SET_OBJECT_NAME(VkRenderPass(vk_inst.render_pass.screenmap), "render pass - screenmap", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT);
#endif
}

static void allocate_and_bind_image_memory(const vk::Image &image)
{
	vk::DeviceSize alignment;
	ImageChunk *chunk = nullptr;
	int i;

	vk::MemoryRequirements memory_requirements;
	memory_requirements = vk_inst.device.getImageMemoryRequirements(image);
	if (memory_requirements.size > vk_inst.image_chunk_size)
	{
		ri.Error(ERR_FATAL, "Vulkan: could not allocate memory, image is too large (%ikbytes).",
				 (int)(memory_requirements.size / 1024));
	}

	// Try to find an existing chunk of sufficient capacity.
	alignment = memory_requirements.alignment;
	for (i = 0; i < vk_world.num_image_chunks; i++)
	{
		// ensure that memory region has proper alignment
		vk::DeviceSize offset = PAD(vk_world.image_chunks[i].used, alignment);

		if (offset + memory_requirements.size <= vk_inst.image_chunk_size)
		{
			chunk = &vk_world.image_chunks[i];
			chunk->used = offset + memory_requirements.size;
			break;
		}
	}

	// Allocate a new chunk in case we couldn't find suitable existing chunk.
	if (chunk == nullptr)
	{
		vk::DeviceMemory memory;

		if (vk_world.num_image_chunks >= MAX_IMAGE_CHUNKS)
		{
			ri.Error(ERR_FATAL, "Vulkan: image chunk limit has been reached");
		}

		vk::MemoryAllocateInfo alloc_info{vk_inst.image_chunk_size,
										  find_memory_type(memory_requirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal),
										  nullptr};

		VK_CHECK_ASSIGN(memory, vk_inst.device.allocateMemory(alloc_info));

		chunk = &vk_world.image_chunks[vk_world.num_image_chunks];
		chunk->memory = memory;
		chunk->used = memory_requirements.size;
#ifdef USE_VK_VALIDATION
		SET_OBJECT_NAME(VkDeviceMemory(memory), va("image memory chunk %i", vk_world.num_image_chunks), VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT);
#endif
		vk_world.num_image_chunks++;
	}

	VK_CHECK(vk_inst.device.bindImageMemory(image, chunk->memory, chunk->used - memory_requirements.size));
}

static void vk_clean_staging_buffer(void)
{
	if (vk_inst.staging_buffer.handle != vk::Buffer())
	{
		vk_inst.device.destroyBuffer(vk_inst.staging_buffer.handle, nullptr);
		vk_inst.staging_buffer.handle = vk::Buffer();
	}
	// if (vk_world.staging_buffer_memory)
	// {
	// 	vk_inst.device.freeMemory(vk_world.staging_buffer_memory, nullptr);
	// 	vk_world.staging_buffer_memory = nullptr;
	// }
	if (vk_inst.staging_buffer.memory != vk::DeviceMemory())
	{
		vk_inst.device.freeMemory(vk_inst.staging_buffer.memory, nullptr);
		vk_inst.staging_buffer.memory = vk::DeviceMemory();
	}
	vk_inst.staging_buffer.ptr = NULL;
	vk_inst.staging_buffer.size = 0;
#ifdef USE_UPLOAD_QUEUE
	vk_inst.staging_buffer.offset = 0;
#endif
}

#ifdef USE_UPLOAD_QUEUE
static bool vk_wait_staging_buffer()
{
    if (vk_inst.aux_fence_wait)
    {
        const uint64_t timeout_ns = 5ull * 1000ull * 1000ull * 1000ull;

        vk::Result res = vk_inst.device.waitForFences(vk_inst.aux_fence, /*waitAll*/ vk::True, timeout_ns);
        if (res != vk::Result::eSuccess) {
            ri.Error(ERR_FATAL, "vkWaitForFences() failed with %s at %s",
                     vk::to_string(res).data(), __func__);
        }

        VK_CHECK(vk_inst.device.resetFences(vk_inst.aux_fence));

        // flags==0 in C → empty Flags in hpp
        VK_CHECK(vk_inst.staging_command_buffer.reset(vk::CommandBufferResetFlags{}));
		vk_inst.staging_buffer.offset = 0; // FIXME: is this correct?
		vk_inst.aux_fence_wait = false;
		return true;
	} else {
		return false;
	}
}

static void vk_flush_staging_buffer(bool final)
{
	if ( vk_inst.staging_buffer.offset == 0 ) {
		return;
	}

	vk::Semaphore waits{};

    constexpr vk::PipelineStageFlags wait_dst_stage_mask =
        vk::PipelineStageFlagBits::eColorAttachmentOutput;

	vk_inst.staging_buffer.offset = 0;

    VK_CHECK(vk_inst.staging_command_buffer.end());

	vk::SubmitInfo submit_info{0,
							   nullptr,
							   nullptr,
							   1,
							   &vk_inst.staging_command_buffer,
							   0,
							   nullptr};

    if (vk_inst.rendering_finished != vk::Semaphore{}) {
        // first call after previous queue submission?
        waits = vk_inst.rendering_finished;
        vk_inst.rendering_finished = vk::Semaphore{}; // clear to null

		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = &waits;
		submit_info.pWaitDstStageMask = &wait_dst_stage_mask;
    }

    if (vk_inst.image_uploaded) {
        ri.Error(ERR_FATAL, "Vulkan: incorrect state during image upload");
    }

    if (final)
    {
        // final submission before recording
        submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = &vk_inst.image_uploaded2;
        vk_inst.image_uploaded = vk_inst.image_uploaded2;

        VK_CHECK(vk_inst.queue.submit(submit_info, vk_inst.aux_fence));
        vk_inst.aux_fence_wait = true;
    }
    else
    {
        // submit and block until it finishes, then reset fence/cmd buffer
        VK_CHECK(vk_inst.queue.submit(submit_info, vk_inst.aux_fence));

        vk::Result res = vk_inst.device.waitForFences(vk_inst.aux_fence, vk::True, 5 * 1000000000ULL);
        if (res != vk::Result::eSuccess) {
            ri.Error(ERR_FATAL, "vkWaitForFences() failed with %s at %s",
                     vk::to_string(res).data(), __func__);
        }

        VK_CHECK(vk_inst.device.resetFences(vk_inst.aux_fence));
        VK_CHECK(vk_inst.staging_command_buffer.reset(vk::CommandBufferResetFlags{}));
    }
}
#endif // USE_UPLOAD_QUEUE

static void vk_alloc_staging_buffer(const vk::DeviceSize size)
{
	void *data;

	vk_clean_staging_buffer();

	vk_inst.staging_buffer.size = MAX( size, STAGING_BUFFER_SIZE );
	vk_inst.staging_buffer.size = PAD( vk_inst.staging_buffer.size, 1024 * 1024 );

	// if (vk_world.staging_buffer)
	// 	vk_inst.device.destroyBuffer(vk_world.staging_buffer);

	// if (vk_world.staging_buffer_memory)
	// 	vk_inst.device.freeMemory(vk_world.staging_buffer_memory);

	vk::BufferCreateInfo buffer_desc{{},
									 vk_inst.staging_buffer.size,
									 vk::BufferUsageFlagBits::eTransferSrc,
									 vk::SharingMode::eExclusive,
									 0,
									 nullptr,
									 nullptr};

	VK_CHECK_ASSIGN( vk_inst.staging_buffer.handle, vk_inst.device.createBuffer(buffer_desc));

	vk::MemoryRequirements memory_requirements;
	memory_requirements = vk_inst.device.getBufferMemoryRequirements(vk_inst.staging_buffer.handle);

	uint32_t memory_type = find_memory_type(memory_requirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

	vk::MemoryAllocateInfo alloc_info{memory_requirements.size,
									  memory_type,
									  nullptr};

	VK_CHECK_ASSIGN(vk_inst.staging_buffer.memory, vk_inst.device.allocateMemory(alloc_info));
	VK_CHECK(vk_inst.device.bindBufferMemory(vk_inst.staging_buffer.handle, vk_inst.staging_buffer.memory, 0));

	VK_CHECK(vk_inst.device.mapMemory(vk_inst.staging_buffer.memory, 0, vk::WholeSize, {}, &data));

	vk_inst.staging_buffer.ptr = (byte*)data;
#ifdef USE_UPLOAD_QUEUE
	vk_inst.staging_buffer.offset = 0;
#endif
#ifdef USE_VK_VALIDATION
	SET_OBJECT_NAME(VkBuffer(vk_inst.staging_buffer.handle), "staging buffer", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT);
	SET_OBJECT_NAME(VkDeviceMemory(vk_inst.staging_buffer.memory), "staging buffer memory", VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT);
#endif
}

#ifdef USE_VK_VALIDATION
static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT object_type, uint64_t object, size_t location,
													 int32_t message_code, const char *layer_prefix, const char *message, void *user_data)
{
#ifdef _WIN32
	MessageBoxA(0, message, layer_prefix, MB_ICONWARNING);
	OutputDebugString(message);
	OutputDebugString("\n");
	DebugBreak();
#endif
	return vk::False;
}
#endif

static bool used_instance_extension(const std::string_view ext)
{
	// Find the last underscore
	std::size_t pos = ext.find_last_of('_');

	// allow all VK_*_surface extensions
	if (pos != std::string_view::npos && Q_stricmp_cpp(ext.substr(pos + 1), "surface") == 0)
		return true;

	if (Q_stricmp_cpp(ext, vk::KHRDisplayExtensionName) == 0)
		return true; // needed for KMSDRM instances/devices?

	if (Q_stricmp_cpp(ext, vk::KHRSwapchainExtensionName) == 0)
		return true;

#ifdef USE_VK_VALIDATION
	if (Q_stricmp_cpp(ext, vk::EXTDebugUtilsExtensionName) == 0)
		return true;
#endif

	if (vk::apiVersionMajor(vulkanApiVersion) == 1 && vk::apiVersionMinor(vulkanApiVersion) == 0)
	{
		if (Q_stricmp_cpp(ext, vk::KHRGetPhysicalDeviceProperties2ExtensionName) == 0)
			return true;
	}

	if (Q_stricmp_cpp(ext, vk::KHRPortabilityEnumerationExtensionName) == 0)
		return true;

	return false;
}

static void create_instance(void)
{
#ifdef USE_VK_VALIDATION
	// const char *validation_layer_name = "VK_LAYER_LUNARG_standard_validation"; // deprecated
	const char *validation_layer_name = "VK_LAYER_KHRONOS_validation";
#endif

	vk::InstanceCreateFlags flags = {};
	uint32_t i, n, count, extension_count;

	extension_count = 0;
	std::vector<vk::ExtensionProperties> extension_properties;
	VK_CHECK_ASSIGN(extension_properties, vk::enumerateInstanceExtensionProperties());

	count = extension_properties.size();
	std::vector<const char *> extension_names(count);

	for (i = 0; i < count; i++)
	{
		const char *ext = extension_properties[i].extensionName.data();

		if (!used_instance_extension(ext))
		{
			continue;
		}

		// search for duplicates
		for (n = 0; n < extension_count; n++)
		{
			if (Q_stricmp_cpp(ext, extension_names[n]) == 0)
			{
				break;
			}
		}
		if (n != extension_count)
		{
			continue;
		}

		extension_names[extension_count++] = ext;

		if (Q_stricmp_cpp(ext, vk::KHRPortabilityEnumerationExtensionName) == 0)
		{
			flags |= vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR;
		}

		ri.Printf(PRINT_DEVELOPER, "instance extension: %s\n", ext);
	}

	vk::ApplicationInfo appInfo{
		nullptr,
		vk::makeApiVersion(0, 1, 0, 0),
		nullptr,
		vk::makeApiVersion(0, 1, 0, 0),
		vk::makeApiVersion(0, 1, 0, 0),
		nullptr};

#ifdef USE_VK_VALIDATION
	appInfo.apiVersion = vk::makeApiVersion(0, 1, 1, 0);
#endif

	// create instance
	vk::InstanceCreateInfo desc{flags,
								&appInfo,
								0,
								nullptr,
								extension_count,
								extension_names.data(),
								nullptr};

#ifdef USE_VK_VALIDATION
	desc.enabledLayerCount = 1;
	desc.ppEnabledLayerNames = &validation_layer_name;

	VK_CHECK_ASSIGN(vk_instance, createInstance(desc));
#else
	VK_CHECK_ASSIGN(vk_instance, createInstance(desc));
#endif

	VK_CHECK_ASSIGN(vulkanApiVersion, vk::enumerateInstanceVersion());
}

static vk::Format get_depth_format(const vk::PhysicalDevice &physical_device)
{
	vk::FormatProperties props;
	std::array<vk::Format, 2> formats{};

	if (glConfig.stencilBits > 0)
	{
		formats[0] = glConfig.depthBits == 16 ? vk::Format::eD16UnormS8Uint : vk::Format::eD24UnormS8Uint;
		formats[1] = vk::Format::eD32SfloatS8Uint;
		glConfig.stencilBits = 8;
	}
	else
	{
		formats[0] = glConfig.depthBits == 16 ? vk::Format::eD16Unorm : vk::Format::eX8D24UnormPack32;
		formats[1] = vk::Format::eD32Sfloat;
		glConfig.stencilBits = 0;
	}
	for (const auto &format : formats)
	{
		props = physical_device.getFormatProperties(format);

		// qvkGetPhysicalDeviceFormatProperties(physical_device, formats[i], &props);
		if ((props.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment) != vk::FormatFeatureFlags{})
		{
			return format;
		}
	}

	ri.Error(ERR_FATAL, "get_depth_format: failed to find depth attachment format");
	return vk::Format::eUndefined; // never get here
}

// Check if we can use vkCmdBlitImage for the given source and destination image formats.
static bool vk_blit_enabled(const vk::PhysicalDevice &physical_device, const vk::Format srcFormat, const vk::Format dstFormat)
{
	vk::FormatProperties formatProps = physical_device.getFormatProperties(srcFormat);
	if ((formatProps.optimalTilingFeatures & vk::FormatFeatureFlagBits::eBlitSrc) == vk::FormatFeatureFlags{})
	{
		return false;
	}

	formatProps = physical_device.getFormatProperties(dstFormat);
	if ((formatProps.linearTilingFeatures & vk::FormatFeatureFlagBits::eBlitDst) == vk::FormatFeatureFlags{})
	{
		return false;
	}

	return true;
}

inline static vk::Format get_hdr_format(const vk::Format base_format)
{
	if (r_fbo->integer == 0)
	{
		return base_format;
	}

	switch (r_hdr->integer)
	{
	case -1:
		return vk::Format::eB4G4R4A4UnormPack16;
	case 1:
		return vk::Format::eR16G16B16A16Unorm;
	default:
		return base_format;
	}
}

typedef struct
{
	int bits;
	vk::Format rgb;
	vk::Format bgr;
} present_format_t;

static constexpr present_format_t present_formats[] = {
	//{12, vk::Format::eB4G4R4A4UnormPack16, VK_FORMAT_R4G4B4A4_UNORM_PACK16},
	//{15, vk::Format::eB5G5R5A1UnormPack16, vk::Format::eR5G5B5A1UnormPack16},
	{16, vk::Format::eB5G6R5UnormPack16, vk::Format::eR5G6B5UnormPack16},
	{24, vk::Format::eB8G8R8A8Unorm, vk::Format::eR8G8B8A8Unorm},
	{30, vk::Format::eA2B10G10R10UnormPack32, vk::Format::eA2R10G10B10UnormPack32},
	//{32, VK_FORMAT_B10G11R11_UFLOAT_PACK32, VK_FORMAT_B10G11R11_UFLOAT_PACK32}
};

static void get_present_format(const int present_bits, vk::Format &bgr, vk::Format &rgb)
{
	const present_format_t *pf, *sel;
	std::size_t i;

	sel = nullptr;
	pf = present_formats;
	for (i = 0; i < arrayLen(present_formats); i++, pf++)
	{
		if (pf->bits <= present_bits)
		{
			sel = pf;
		}
	}
	if (!sel)
	{
		bgr = vk::Format::eB8G8R8A8Unorm;
		rgb = vk::Format::eR8G8B8A8Unorm;
	}
	else
	{
		bgr = sel->bgr;
		rgb = sel->rgb;
	}
}

static bool vk_select_surface_format(const vk::PhysicalDevice &physical_device, const vk::SurfaceKHR &surface)
{
	vk::Format base_bgr, base_rgb;
	vk::Format ext_bgr, ext_rgb;

	uint32_t format_count;

	std::vector<vk::SurfaceFormatKHR> candidates;

	VK_CHECK_ASSIGN(candidates, physical_device.getSurfaceFormatsKHR(surface));

	format_count = candidates.size();

	if (format_count == 0)
	{
		ri.Printf(PRINT_ERROR, "...no surface formats found\n");
		return false;
	}

	get_present_format(24, base_bgr, base_rgb);

	if (r_fbo->integer)
	{
		get_present_format(r_presentBits->integer, ext_bgr, ext_rgb);
	}
	else
	{
		ext_bgr = base_bgr;
		ext_rgb = base_rgb;
	}

	if (format_count == 1 && candidates[0].format == vk::Format::eUndefined)
	{
		// special case that means we can choose any format
		vk_inst.base_format.format = base_bgr;
		vk_inst.base_format.colorSpace = vk::ColorSpaceKHR::eVkColorspaceSrgbNonlinear;
		vk_inst.present_format.format = ext_bgr;
		vk_inst.present_format.colorSpace = vk::ColorSpaceKHR::eVkColorspaceSrgbNonlinear;
	}
	else
	{
		uint32_t i;
		for (i = 0; i < format_count; i++)
		{
			if ((candidates[i].format == base_bgr || candidates[i].format == base_rgb) && candidates[i].colorSpace == vk::ColorSpaceKHR::eVkColorspaceSrgbNonlinear)
			{
				vk_inst.base_format = candidates[i];
				break;
			}
		}
		if (i == format_count)
		{
			vk_inst.base_format = candidates[0];
		}
		for (i = 0; i < format_count; i++)
		{
			if ((candidates[i].format == ext_bgr || candidates[i].format == ext_rgb) && candidates[i].colorSpace == vk::ColorSpaceKHR::eVkColorspaceSrgbNonlinear)
			{
				vk_inst.present_format = candidates[i];
				break;
			}
		}
		if (i == format_count)
		{
			vk_inst.present_format = vk_inst.base_format;
		}
	}

	if (!r_fbo->integer)
	{
		vk_inst.present_format = vk_inst.base_format;
	}

	return true;
}

static void setup_surface_formats(const vk::PhysicalDevice &physical_device)
{
	vk_inst.depth_format = get_depth_format(physical_device);

	vk_inst.color_format = get_hdr_format(vk_inst.base_format.format);

	vk_inst.capture_format = vk::Format::eR8G8B8A8Unorm;

	vk_inst.bloom_format = vk_inst.base_format.format;

	vk_inst.blitEnabled = vk_blit_enabled(physical_device, vk_inst.color_format, vk_inst.capture_format);

	if (!vk_inst.blitEnabled)
	{
		vk_inst.capture_format = vk_inst.color_format;
	}
}

static const char *renderer_name(const vk::PhysicalDeviceProperties &props)
{
	static char buf[sizeof(props.deviceName) + 64];
	const char *device_type;

	switch (props.deviceType)
	{
	case vk::PhysicalDeviceType::eIntegratedGpu:
		device_type = "Integrated";
		break;
	case vk::PhysicalDeviceType::eDiscreteGpu:
		device_type = "Discrete";
		break;
	case vk::PhysicalDeviceType::eVirtualGpu:
		device_type = "Virtual";
		break;
	case vk::PhysicalDeviceType::eCpu:
		device_type = "CPU";
		break;
	default:
		device_type = "OTHER";
		break;
	}

	// Com_sprintf(buf, sizeof(buf), "%s %s, 0x%04x",
	// 			device_type, props.deviceName, props.deviceID);

	char charArray[256];
	std::memcpy(charArray, props.deviceName.data(), 256);

	Com_sprintf(buf, sizeof(buf), "%s %s, 0x%04x",
				device_type, charArray, props.deviceID);

	return buf;
}

static bool vk_create_device(const vk::PhysicalDevice &physical_device, const int device_index)
{

#ifdef USE_VK_VALIDATION
	vk::PhysicalDeviceTimelineSemaphoreFeatures timeline_semaphore;
	vk::PhysicalDeviceVulkanMemoryModelFeatures memory_model;
	vk::PhysicalDeviceBufferDeviceAddressFeatures devaddr_features;
	vk::PhysicalDevice8BitStorageFeatures storage_8bit_features;
#endif

	ri.Printf(PRINT_ALL, "...selected physical device: %i\n", device_index);

	// select surface format
	if (!vk_select_surface_format(physical_device, vk_surface))
	{
		return false;
	}

	setup_surface_formats(physical_device);

	// select queue family
	{
		std::vector<vk::QueueFamilyProperties> queue_families;
		uint32_t i;

		queue_families = physical_device.getQueueFamilyProperties();

		// select queue family with presentation and graphics support
		vk_inst.queue_family_index = ~0U;
		for (i = 0; i < queue_families.size(); i++)
		{
			vk::Bool32 presentation_supported;
			VK_CHECK_ASSIGN(presentation_supported, physical_device.getSurfaceSupportKHR(i, vk_surface));

			if (presentation_supported && (queue_families[i].queueFlags & vk::QueueFlagBits::eGraphics) != vk::QueueFlags{})
			{
				vk_inst.queue_family_index = i;
				break;
			}
		}

		if (vk_inst.queue_family_index == ~0U)
		{
			ri.Printf(PRINT_ERROR, "...failed to find graphics queue family\n");

			return false;
		}
	}

	// create VkDevice
	{
		std::vector<const char *> device_extension_list;
		// const char *device_extension_list[4];
		// uint32_t device_extension_count = 0;
		const char *end;
		char *str;
		const float priority = 1.0;

		bool swapchainSupported = false;
		bool dedicatedAllocation = false;
		bool memoryRequirements2 = false;
#ifdef USE_VK_VALIDATION
		bool debugMarker = false;
		bool timelineSemaphore = false;
		bool memoryModel = false;
		bool devAddrFeat = false;
		bool storage8bit = false;
		const void **pNextPtr;
#endif

		uint32_t i, len;

		std::vector<vk::ExtensionProperties> extension_properties;
		VK_CHECK_ASSIGN(extension_properties, physical_device.enumerateDeviceExtensionProperties());

		// fill glConfig.extensions_string
		str = glConfig.extensions_string;
		*str = '\0';
		end = &glConfig.extensions_string[sizeof(glConfig.extensions_string) - 1];

		for (i = 0; i < extension_properties.size(); i++)
		{
			auto ext = to_str_view(extension_properties[i].extensionName);

			// add this device extension to glConfig
			if (i != 0)
			{
				if (str + 1 >= end)
					continue;
				str = Q_stradd(str, " ");
			}
			len = (uint32_t)ext.size();
			if (str + len >= end)
				continue;
			str = Q_stradd(str, ext.data());

			if (vk::apiVersionMajor(vulkanApiVersion) == 1 && vk::apiVersionMinor(vulkanApiVersion) == 0)
			{
				if (ext == vk::KHRDedicatedAllocationExtensionName)
				{
					dedicatedAllocation = true;
					continue;
				}
				else if (ext == vk::KHRGetMemoryRequirements2ExtensionName)
				{
					memoryRequirements2 = true;

					device_extension_list.push_back(vk::KHRDedicatedAllocationExtensionName);
					device_extension_list.push_back(vk::KHRGetMemoryRequirements2ExtensionName);
					continue;
				}
			}

			if (ext == vk::KHRSwapchainExtensionName)
			{
				swapchainSupported = true;
			}
#ifdef USE_VK_VALIDATION
			else if (ext == vk::EXTDebugUtilsExtensionName)
			{
				debugMarker = true;
			}
			else if (ext == vk::KHRTimelineSemaphoreExtensionName)
			{
				timelineSemaphore = true;
			}
			else if (ext == vk::KHRVulkanMemoryModelExtensionName)
			{
				memoryModel = true;
			}
			else if (ext == vk::KHRBufferDeviceAddressExtensionName)
			{
				devAddrFeat = true;
			}
			else if (ext == vk::KHR8BitStorageExtensionName)
			{
				storage8bit = true;
			}
#endif
		}

		if (!swapchainSupported)
		{
			ri.Printf(PRINT_ERROR, "...required device extension is not available: %s\n", vk::KHRSwapchainExtensionName);
			return false;
		}

		if (!memoryRequirements2)
			dedicatedAllocation = false;

		vk_inst.dedicatedAllocation = dedicatedAllocation;

#ifndef USE_DEDICATED_ALLOCATION
		vk_inst.dedicatedAllocation = false;
#endif

		device_extension_list.push_back(vk::KHRSwapchainExtensionName);

#ifdef USE_VK_VALIDATION
		if (debugMarker)
		{
			device_extension_list.push_back(vk::EXTDebugUtilsExtensionName);
			vk_inst.debugMarkers = true;
		}
		if (timelineSemaphore)
		{
			device_extension_list.push_back(vk::KHRTimelineSemaphoreExtensionName);
		}

		if (memoryModel)
		{
			device_extension_list.push_back(vk::KHRVulkanMemoryModelExtensionName);
		}
		if (devAddrFeat)
		{
			device_extension_list.push_back(vk::KHRBufferDeviceAddressExtensionName);
		}
		if (storage8bit)
		{
			device_extension_list.push_back(vk::KHR8BitStorageExtensionName);
		}
#endif // _DEBUG

		vk::PhysicalDeviceFeatures device_features;
		device_features = physical_device.getFeatures();
		if (device_features.fillModeNonSolid == vk::False)
		{
			ri.Printf(PRINT_ERROR, "...fillModeNonSolid feature is not supported\n");
			return false;
		}

		vk::DeviceQueueCreateInfo queue_desc{{},
											 vk_inst.queue_family_index,
											 1,
											 &priority,
											 nullptr};

		vk::PhysicalDeviceFeatures features{};

#ifdef USE_VK_VALIDATION
		if (device_features.shaderInt64)
		{
			features.shaderInt64 = vk::True;
		}
#endif

		features.fillModeNonSolid = vk::True;

		if (device_features.wideLines)
		{ // needed for RB_SurfaceAxis
			features.wideLines = vk::True;
			vk_inst.wideLines = true;
		}

		if (device_features.fragmentStoresAndAtomics && device_features.vertexPipelineStoresAndAtomics)
		{
			features.vertexPipelineStoresAndAtomics = vk::True;
			features.fragmentStoresAndAtomics = vk::True;
			vk_inst.fragmentStores = true;
		}

		if (r_ext_texture_filter_anisotropic->integer && device_features.samplerAnisotropy)
		{
			features.samplerAnisotropy = vk::True;
			vk_inst.samplerAnisotropy = true;
		}

		vk::DeviceCreateInfo device_desc{{},
										 1,
										 &queue_desc,
										 0,
										 nullptr,
										 (uint32_t)device_extension_list.size(),
										 device_extension_list.data(),
										 &features,
										 nullptr};

#ifdef USE_VK_VALIDATION
		pNextPtr = (const void **)&device_desc.pNext;
		if (timelineSemaphore)
		{
			*pNextPtr = &timeline_semaphore;
			timeline_semaphore.pNext = nullptr;
			timeline_semaphore.sType = vk::StructureType::ePhysicalDeviceTimelineSemaphoreFeatures;
			timeline_semaphore.timelineSemaphore = vk::True;
			pNextPtr = (const void **)&timeline_semaphore.pNext;
		}
		if (memoryModel)
		{
			*pNextPtr = &memory_model;
			memory_model.pNext = nullptr;
			memory_model.sType = vk::StructureType::ePhysicalDeviceVulkanMemoryModelFeatures;
			memory_model.vulkanMemoryModel = VK_TRUE;
			memory_model.vulkanMemoryModelAvailabilityVisibilityChains = VK_FALSE;
			memory_model.vulkanMemoryModelDeviceScope = VK_TRUE;
			pNextPtr = (const void **)&memory_model.pNext;
		}
		if (devAddrFeat)
		{
			*pNextPtr = &devaddr_features;
			devaddr_features.pNext = nullptr;
			devaddr_features.sType = vk::StructureType::ePhysicalDeviceBufferDeviceAddressFeatures;
			devaddr_features.bufferDeviceAddress = VK_TRUE;
			devaddr_features.bufferDeviceAddressCaptureReplay = VK_FALSE;
			devaddr_features.bufferDeviceAddressMultiDevice = VK_FALSE;
			pNextPtr = (const void **)&devaddr_features.pNext;
		}
		if (storage8bit)
		{
			*pNextPtr = &storage_8bit_features;
			storage_8bit_features.pNext = nullptr;
			storage_8bit_features.sType = vk::StructureType::ePhysicalDevice8BitStorageFeatures;
			storage_8bit_features.storageBuffer8BitAccess = VK_TRUE;
			storage_8bit_features.storagePushConstant8 = VK_FALSE;
			storage_8bit_features.uniformAndStorageBuffer8BitAccess = VK_TRUE;
			pNextPtr = (const void **)&storage_8bit_features.pNext;
		}
#endif

		VK_CHECK_ASSIGN(vk_inst.device, physical_device.createDevice(device_desc));
	}

	return true;
}

#define INIT_INSTANCE_FUNCTION(func)                                                       \
	q##func = reinterpret_cast<PFN_##func>(ri.VK_GetInstanceProcAddr(vk_instance, #func)); \
	if (q##func == nullptr)                                                                \
	{                                                                                      \
		ri.Error(ERR_FATAL, "Failed to find entrypoint %s", #func);                        \
		exit(EXIT_FAILURE);                                                                \
	}

#define INIT_INSTANCE_FUNCTION_EXT(func) \
	q##func = /*(PFN_ ## func)*/ ri.VK_GetInstanceProcAddr(VkInstance(vk_instance), #func);

#define INIT_DEVICE_FUNCTION(func)                                     \
	q##func = (PFN_##func)qvkGetDeviceProcAddr(vk_inst.device, #func); \
	if (q##func == nullptr)                                            \
	{                                                                  \
		ri.Error(ERR_FATAL, "Failed to find entrypoint %s", #func);    \
	}

#define INIT_DEVICE_FUNCTION_EXT(func) \
	q##func = (PFN_##func)qvkGetDeviceProcAddr(vk_inst.device, #func);

static void vk_destroy_instance(void)
{
	if (vk_surface)
	{
		vk_instance.destroySurfaceKHR(vk_surface);

		vk_surface = VK_NULL_HANDLE;
	}

#ifdef USE_VK_VALIDATION
	if (vk_debug_callback)
	{
		if (qvkDestroyDebugReportCallbackEXT != nullptr)
		{
			qvkDestroyDebugReportCallbackEXT(vk_instance, vk_debug_callback, nullptr);
		}
		vk_debug_callback = VK_NULL_HANDLE;
	}
#endif

	if (vk_instance)
	{
		vk_instance.destroy();

		vk_instance = nullptr;
	}
}

static void init_vulkan_library(void)
{
	uint32_t device_count;
	int device_index;
	uint32_t i;

	vk_inst = Vk_Instance{};

	if (!vk_instance)
	{

		// force cleanup
		vk_destroy_instance();

		// Get instance level functions.
		create_instance();

		INIT_INSTANCE_FUNCTION(vkGetDeviceProcAddr)

#ifdef USE_VK_VALIDATION
		// INIT_INSTANCE_FUNCTION_EXT(vkCreateDebugReportCallbackEXT)
		// INIT_INSTANCE_FUNCTION_EXT(vkDestroyDebugReportCallbackEXT)

		// Create debug callback.
		if (qvkCreateDebugReportCallbackEXT && qvkDestroyDebugReportCallbackEXT)
		{
			VkDebugReportCallbackCreateInfoEXT desc;
			desc.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
			desc.pNext = nullptr;
			desc.flags = VK_DEBUG_REPORT_WARNING_BIT_EXT |
						 VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT |
						 VK_DEBUG_REPORT_ERROR_BIT_EXT;
			desc.pfnCallback = &debug_callback;
			desc.pUserData = nullptr;

			VK_CHECK(qvkCreateDebugReportCallbackEXT(vk_instance, &desc, nullptr, &vk_debug_callback));
		}
#endif

		// create surface
		if (!ri.VK_CreateSurface(vk_instance, &vk_surfaceC))
		{
			ri.Error(ERR_FATAL, "Error creating Vulkan surface");
			return;
		}
		vk_surface = vk::SurfaceKHR(vk_surfaceC);
	}

	std::vector<vk::PhysicalDevice> physical_devices;
	VK_CHECK_ASSIGN(physical_devices, vk_instance.enumeratePhysicalDevices());

	device_count = physical_devices.size();

	if (device_count == 0)
	{
		ri.Error(ERR_FATAL, "Vulkan: no physical devices found");
		return;
	}

	// initial physical device index
	device_index = r_device->integer;

	ri.Printf(PRINT_ALL, ".......................\nAvailable physical devices:\n");

	for (i = 0; i < device_count; i++)
	{
		vk::PhysicalDeviceProperties props = physical_devices[i].getProperties();
		ri.Printf(PRINT_ALL, " %i: %s\n", i, renderer_name(props));

		if (device_index == -1 && props.deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
		{
			device_index = i;
		}
		else if (device_index == -2 && props.deviceType == vk::PhysicalDeviceType::eIntegratedGpu)
		{
			device_index = i;
		}
	}
	ri.Printf(PRINT_ALL, ".......................\n");

	vk_inst.physical_device = nullptr;
	for (i = 0; i < device_count; i++, device_index++)
	{
		if (device_index >= static_cast<int>(device_count) || device_index < 0)
		{
			device_index = 0;
		}

		if (vk_create_device(physical_devices[i], device_index))
		{
			vk_inst.physical_device = physical_devices[device_index];
			break;
		}
	}

	// ri.Free(physical_devices);

	if (!vk_inst.physical_device)
	{
		ri.Error(ERR_FATAL, "Vulkan: unable to find any suitable physical device");
		return;
	}

	//
	// Get device level functions.
	//

	if (vk_inst.debugMarkers)
	{
		INIT_DEVICE_FUNCTION_EXT(vkDebugMarkerSetObjectNameEXT)
	}
}

#undef INIT_INSTANCE_FUNCTION
#undef INIT_DEVICE_FUNCTION
#undef INIT_DEVICE_FUNCTION_EXT

static void deinit_instance_functions(void)
{

	// instance functions:
	qvkGetDeviceProcAddr = nullptr;

#ifdef USE_VK_VALIDATION
	qvkCreateDebugReportCallbackEXT = nullptr;
	qvkDestroyDebugReportCallbackEXT = nullptr;
#endif
}

static void deinit_device_functions(void)
{
	// device functions:
	qvkDebugMarkerSetObjectNameEXT = nullptr;
}

static vk::ShaderModule SHADER_MODULE(const uint8_t *bytes, const int count)
{
	vk::ShaderModule module;

	if (count % 4 != 0)
	{
		ri.Error(ERR_FATAL, "Vulkan: SPIR-V binary buffer size is not a multiple of 4");
	}

	vk::ShaderModuleCreateInfo desc{{},
									(std::size_t)count,
									(const uint32_t *)bytes,
									nullptr};

	VK_CHECK_ASSIGN(module, vk_inst.device.createShaderModule(desc));

	return module;
}

static vk::DescriptorSetLayout vk_create_layout_binding(const int binding, const vk::DescriptorType type, const vk::ShaderStageFlags flags)
{

	vk::DescriptorSetLayoutBinding bind{(uint32_t)binding,
										type,
										1,
										flags,
										nullptr};

	vk::DescriptorSetLayoutCreateInfo desc{{},
										   1,
										   &bind,
										   nullptr};

	vk::DescriptorSetLayout setLayoutResult;
	VK_CHECK_ASSIGN(setLayoutResult, vk_inst.device.createDescriptorSetLayout(desc));
	return setLayoutResult;
}

void vk_update_uniform_descriptor(const vk::DescriptorSet &descriptor, const vk::Buffer &buffer)
{
	vk::DescriptorBufferInfo info{buffer, 0, sizeof(vkUniform_t)};

	vk::WriteDescriptorSet desc{descriptor,
								0,
								0,
								1,
								vk::DescriptorType::eUniformBufferDynamic,
								nullptr,
								&info,
								nullptr,
								nullptr};

	vk_inst.device.updateDescriptorSets(desc, nullptr);
}

static vk::Sampler vk_find_sampler(const Vk_Sampler_Def &def)
{
	int i;

	// Look for sampler among existing samplers.
	for ( i = 0; i < vk_inst.samplers.count; i++ ) {
		const Vk_Sampler_Def *cur_def = &vk_inst.samplers.def[i];
		if ( memcmp( cur_def, &def, sizeof( def ) ) == 0 ) {
			return vk_inst.samplers.handle[i];
		}
	}

	vk::SamplerAddressMode address_mode;
	vk::Sampler sampler;
	vk::Filter mag_filter;
	vk::Filter min_filter;
	vk::SamplerMipmapMode mipmap_mode;
	float maxLod;

	// Create new sampler.
	if ( vk_inst.samplers.count >= MAX_VK_SAMPLERS ) {
		ri.Error( ERR_DROP, "vk_find_sampler: MAX_VK_SAMPLERS hit\n" );
		// return VK_NULL_HANDLE;
	}

	address_mode = def.address_mode;

	if (def.gl_mag_filter == GL_NEAREST)
	{
		mag_filter = vk::Filter::eNearest;
	}
	else if (def.gl_mag_filter == GL_LINEAR)
	{
		mag_filter = vk::Filter::eLinear;
	}
	else
	{
		ri.Error(ERR_FATAL, "vk_find_sampler: invalid gl_mag_filter");
		return nullptr;
	}

	maxLod = vk_inst.maxLod;

	if (def.gl_min_filter == GL_NEAREST)
	{
		min_filter = vk::Filter::eNearest;
		mipmap_mode = vk::SamplerMipmapMode::eNearest;
		maxLod = 0.25f; // used to emulate OpenGL's GL_LINEAR/GL_NEAREST minification filter
	}
	else if (def.gl_min_filter == GL_LINEAR)
	{
		min_filter = vk::Filter::eLinear;
		mipmap_mode = vk::SamplerMipmapMode::eNearest;
		maxLod = 0.25f; // used to emulate OpenGL's GL_LINEAR/GL_NEAREST minification filter
	}
	else if (def.gl_min_filter == GL_NEAREST_MIPMAP_NEAREST)
	{
		min_filter = vk::Filter::eNearest;
		mipmap_mode = vk::SamplerMipmapMode::eNearest;
	}
	else if (def.gl_min_filter == GL_LINEAR_MIPMAP_NEAREST)
	{
		min_filter = vk::Filter::eLinear;
		mipmap_mode = vk::SamplerMipmapMode::eNearest;
	}
	else if (def.gl_min_filter == GL_NEAREST_MIPMAP_LINEAR)
	{
		min_filter = vk::Filter::eNearest;
		mipmap_mode = vk::SamplerMipmapMode::eLinear;
	}
	else if (def.gl_min_filter == GL_LINEAR_MIPMAP_LINEAR)
	{
		min_filter = vk::Filter::eLinear;
		mipmap_mode = vk::SamplerMipmapMode::eLinear;
	}
	else
	{
		ri.Error(ERR_FATAL, "vk_find_sampler: invalid gl_min_filter");
		return nullptr;
	}

	if (def.max_lod_1_0)
	{
		maxLod = 1.0f;
	}

	vk::SamplerCreateInfo desc{{},
							   mag_filter,
							   min_filter,
							   mipmap_mode,
							   address_mode,
							   address_mode,
							   address_mode,
							   0.0f,
							   {},
							   {},
							   vk::False,
							   vk::CompareOp::eAlways,
							   0.0f,
							   (maxLod == vk_inst.maxLod) ? VK_LOD_CLAMP_NONE : maxLod,
							   vk::BorderColor::eFloatTransparentBlack,
							   vk::False,
							   nullptr};

	if (def.noAnisotropy || mipmap_mode == vk::SamplerMipmapMode::eNearest || mag_filter == vk::Filter::eNearest)
	{
		desc.anisotropyEnable = vk::False;
		desc.maxAnisotropy = 1.0f;
	}
	else
	{
		desc.anisotropyEnable = (r_ext_texture_filter_anisotropic->integer && vk_inst.samplerAnisotropy) ? vk::True : vk::False;
		if (desc.anisotropyEnable)
		{
			desc.maxAnisotropy = MIN(r_ext_max_anisotropy->integer, vk_inst.maxAnisotropy);
		}
	}

	VK_CHECK_ASSIGN(sampler, vk_inst.device.createSampler(desc));
#ifdef USE_VK_VALIDATION
	SET_OBJECT_NAME(VkSampler(sampler), va("image sampler %i", vk_inst.samplers.count), VK_DEBUG_REPORT_OBJECT_TYPE_SAMPLER_EXT);
#endif
	vk_inst.samplers.def[ vk_inst.samplers.count ] = def;
	vk_inst.samplers.handle[ vk_inst.samplers.count ] = sampler;
	vk_inst.samplers.count++;

	return sampler;
}

void vk_destroy_samplers( void )
{
	int i;

	for ( i = 0; i < vk_inst.samplers.count; i++ ) {
		vk_inst.device.destroySampler(vk_inst.samplers.handle[i]);
		memset( &vk_inst.samplers.def[i], 0x0, sizeof( vk_inst.samplers.def[i] ) );
		vk_inst.samplers.handle[i] = nullptr;
	}

	vk_inst.samplers.count = 0;
}


void vk_update_attachment_descriptors( void )
{

	if (vk_inst.color_image_view)
	{

		Vk_Sampler_Def sd{vk::SamplerAddressMode::eClampToEdge, vk_inst.blitFilter, vk_inst.blitFilter, true, true};

		vk::DescriptorImageInfo info{vk_find_sampler(sd),
									 vk_inst.color_image_view,
									 vk::ImageLayout::eShaderReadOnlyOptimal};

		vk::WriteDescriptorSet desc{vk_inst.color_descriptor,
									0,
									0,
									1,
									vk::DescriptorType::eCombinedImageSampler,
									&info,
									nullptr,
									nullptr,
									nullptr};

		vk_inst.device.updateDescriptorSets(desc, nullptr);

		// screenmap
		sd.gl_mag_filter = sd.gl_min_filter = GL_LINEAR;
		sd.max_lod_1_0 = false;
		sd.noAnisotropy = true;

		info.sampler = vk_find_sampler(sd);

		info.imageView = vk_inst.screenMap.color_image_view;
		desc.dstSet = vk_inst.screenMap.color_descriptor;

		vk_inst.device.updateDescriptorSets(desc, nullptr);

		// bloom images
		if (r_bloom->integer)
		{
			uint32_t i;
			for (i = 0; i < vk_inst.bloom_image_descriptor.size(); i++)
			{
				info.imageView = vk_inst.bloom_image_view[i];
				desc.dstSet = vk_inst.bloom_image_descriptor[i];

				vk_inst.device.updateDescriptorSets(desc, nullptr);
			}
		}
	}
}

void vk_init_descriptors(void)
{
	uint32_t i;

	vk::DescriptorSetAllocateInfo alloc{vk_inst.descriptor_pool,
										1,
										&vk_inst.set_layout_storage,
										nullptr};

	VK_CHECK(vk_inst.device.allocateDescriptorSets(&alloc, &vk_inst.storage.descriptor));

	vk::DescriptorBufferInfo info{vk_inst.storage.buffer,
								  0,
								  sizeof(uint32_t)};

	vk::WriteDescriptorSet desc{vk_inst.storage.descriptor,
								0,
								0,
								1,
								vk::DescriptorType::eStorageBufferDynamic,
								nullptr,
								&info,
								nullptr,
								nullptr};

	vk_inst.device.updateDescriptorSets(desc, nullptr);

	// allocated and update descriptor set
	for (i = 0; i < NUM_COMMAND_BUFFERS; i++)
	{
		alloc.descriptorPool = vk_inst.descriptor_pool;
		alloc.descriptorSetCount = 1;
		alloc.pSetLayouts = &vk_inst.set_layout_uniform;

		VK_CHECK(vk_inst.device.allocateDescriptorSets(&alloc, &vk_inst.tess[i].uniform_descriptor));

		vk_update_uniform_descriptor(vk_inst.tess[i].uniform_descriptor, vk_inst.tess[i].vertex_buffer);
#ifdef USE_VK_VALIDATION
		SET_OBJECT_NAME(VkDescriptorSet(vk_inst.tess[i].uniform_descriptor), va("uniform descriptor %i", i), VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT);
#endif
	}

	if (vk_inst.color_image_view)
	{
		auto layout_sampler = vk_inst.set_layout_sampler;

		alloc.descriptorPool = vk_inst.descriptor_pool;
		alloc.descriptorSetCount = 1;
		alloc.pSetLayouts = &layout_sampler;

		VK_CHECK(vk_inst.device.allocateDescriptorSets(&alloc, &vk_inst.color_descriptor));

		if (r_bloom->integer)
		{
			// Allocate all bloom descriptors in a single call
			alloc.descriptorSetCount = vk_inst.bloom_image_descriptor.size();
			VK_CHECK(vk_inst.device.allocateDescriptorSets(&alloc, vk_inst.bloom_image_descriptor.data()));
		}

		alloc.descriptorSetCount = 1;

		VK_CHECK(vk_inst.device.allocateDescriptorSets(&alloc, &vk_inst.screenMap.color_descriptor));

		vk_update_attachment_descriptors();
	}
}

static void vk_release_geometry_buffers(void)
{
	int i;

	for (i = 0; i < NUM_COMMAND_BUFFERS; i++)
	{
		vk_inst.device.destroyBuffer(vk_inst.tess[i].vertex_buffer);
		vk_inst.tess[i].vertex_buffer = nullptr;
	}

	vk_inst.device.freeMemory(vk_inst.geometry_buffer_memory);
	vk_inst.geometry_buffer_memory = nullptr;
}

static void vk_create_geometry_buffers(const vk::DeviceSize size)
{
	vk::MemoryRequirements vb_memory_requirements{};
	uint32_t memory_type_bits;
	uint32_t memory_type;
	void *data;
	int i;

	vk::BufferCreateInfo desc{{},
							  {},
							  {},
							  vk::SharingMode::eExclusive,
							  0,
							  nullptr,
							  nullptr};

	for (i = 0; i < NUM_COMMAND_BUFFERS; i++)
	{
		desc.size = size;
		desc.usage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eUniformBuffer;

		VK_CHECK_ASSIGN(vk_inst.tess[i].vertex_buffer, vk_inst.device.createBuffer(desc));
		vb_memory_requirements = vk_inst.device.getBufferMemoryRequirements(vk_inst.tess[i].vertex_buffer);
	}

	memory_type_bits = vb_memory_requirements.memoryTypeBits;
	memory_type = find_memory_type(memory_type_bits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

	vk::MemoryAllocateInfo alloc_info{vb_memory_requirements.size * NUM_COMMAND_BUFFERS,
									  memory_type,
									  nullptr};

	VK_CHECK_ASSIGN(vk_inst.geometry_buffer_memory, vk_inst.device.allocateMemory(alloc_info));

	VK_CHECK(vk_inst.device.mapMemory(vk_inst.geometry_buffer_memory, 0, vk::WholeSize, {}, &data));

	vk::DeviceSize vertex_buffer_offset{0};

	for (i = 0; i < NUM_COMMAND_BUFFERS; i++)
	{
		VK_CHECK(vk_inst.device.bindBufferMemory(vk_inst.tess[i].vertex_buffer, vk_inst.geometry_buffer_memory, vertex_buffer_offset));
		vk_inst.tess[i].vertex_buffer_ptr = (byte *)data + vertex_buffer_offset;
		vk_inst.tess[i].vertex_buffer_offset = 0;
		vertex_buffer_offset += vb_memory_requirements.size;
#ifdef USE_VK_VALIDATION
		SET_OBJECT_NAME(VkBuffer(vk_inst.tess[i].vertex_buffer), va("geometry buffer %i", i), VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT);
#endif
	}
#ifdef USE_VK_VALIDATION
	SET_OBJECT_NAME(VkDeviceMemory(vk_inst.geometry_buffer_memory), "geometry buffer memory", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT);
#endif
	vk_inst.geometry_buffer_size = vb_memory_requirements.size;

	vk_inst.stats = {};
}

static void vk_create_storage_buffer(const uint32_t size)
{
	vk::BufferCreateInfo bufferCreateInfo{
		{},
		size,
		vk::BufferUsageFlagBits::eStorageBuffer,
		vk::SharingMode::eExclusive};

	// Create the buffer
	VK_CHECK_ASSIGN(vk_inst.storage.buffer, vk_inst.device.createBuffer(bufferCreateInfo));

	// Get memory requirements for the buffer
	vk::MemoryRequirements memoryRequirements = vk_inst.device.getBufferMemoryRequirements(vk_inst.storage.buffer);
	uint32_t memoryTypeBits = memoryRequirements.memoryTypeBits;

	// Find a suitable memory type
	uint32_t memoryType = find_memory_type(memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

	vk::MemoryAllocateInfo memoryAllocateInfo{
		memoryRequirements.size,
		memoryType};

	// Allocate memory for the buffer
	VK_CHECK_ASSIGN(vk_inst.storage.memory, vk_inst.device.allocateMemory(memoryAllocateInfo));

	// Bind the memory to the buffer
	VK_CHECK(vk_inst.device.bindBufferMemory(vk_inst.storage.buffer, vk_inst.storage.memory, 0));

	// Map the memory and initialize it
	void *mappedMemory;
	VK_CHECK_ASSIGN(mappedMemory, vk_inst.device.mapMemory(vk_inst.storage.memory, 0, vk::WholeSize, {}));
	std::memset(mappedMemory, 0, size);								// Initialize the memory
	vk_inst.storage.buffer_ptr = static_cast<byte *>(mappedMemory); // Store the pointer to the buffer

	// Unmap the memory
	vk_inst.device.unmapMemory(vk_inst.storage.memory);
#ifdef USE_VK_VALIDATION
	// Set object names for debugging
	SET_OBJECT_NAME(VkBuffer(vk_inst.storage.buffer), "storage buffer", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT);
	SET_OBJECT_NAME(VkDescriptorSet(vk_inst.storage.descriptor), "storage buffer", VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT);
	SET_OBJECT_NAME(VkDeviceMemory(vk_inst.storage.memory), "storage buffer memory", VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT);
#endif
}

#ifdef USE_VBO
void vk_release_vbo(void)
{
	if (vk_inst.vbo.vertex_buffer)
		vk_inst.device.destroyBuffer(vk_inst.vbo.vertex_buffer);
	vk_inst.vbo.vertex_buffer = nullptr;

	if (vk_inst.vbo.buffer_memory)
		vk_inst.device.freeMemory(vk_inst.vbo.buffer_memory);
	vk_inst.vbo.buffer_memory = nullptr;
}

bool vk_alloc_vbo(const byte *vbo_data, const uint32_t vbo_size)
{
	vk::MemoryRequirements vb_mem_reqs;
	vk::DeviceSize vertex_buffer_offset;
	vk::DeviceSize allocationSize;
	uint32_t memory_type_bits;
	vk::CommandBuffer command_buffer;
	vk::BufferCopy copyRegion[1];
	vk::DeviceSize uploadDone;

	vk_release_vbo();

	vk::BufferCreateInfo desc{
		{},
		vbo_size,
		vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndexBuffer,

		vk::SharingMode::eExclusive,
		0,
		nullptr,
		nullptr};

	VK_CHECK_ASSIGN(vk_inst.vbo.vertex_buffer, vk_inst.device.createBuffer(desc));

	// memory requirements
	vb_mem_reqs = vk_inst.device.getBufferMemoryRequirements(vk_inst.vbo.vertex_buffer);

	vertex_buffer_offset = 0;
	allocationSize = vertex_buffer_offset + vb_mem_reqs.size;
	memory_type_bits = vb_mem_reqs.memoryTypeBits;

	vk::MemoryAllocateInfo alloc_info{allocationSize,
									  find_memory_type(memory_type_bits, vk::MemoryPropertyFlagBits::eDeviceLocal),
									  nullptr};

	VK_CHECK_ASSIGN(vk_inst.vbo.buffer_memory, vk_inst.device.allocateMemory(alloc_info));
	VK_CHECK(vk_inst.device.bindBufferMemory(vk_inst.vbo.vertex_buffer, vk_inst.vbo.buffer_memory, vertex_buffer_offset));

	// staging buffers

	// utilize existing staging buffer
	vk_flush_staging_buffer( false );
	uploadDone = 0;
	while ( uploadDone < vbo_size ) {
		vk::DeviceSize uploadSize = vk_inst.staging_buffer.size;
		if ( uploadDone + uploadSize > vbo_size ) {
			uploadSize = vbo_size - uploadDone;
		}
		memcpy(vk_inst.staging_buffer.ptr + 0, vbo_data + uploadDone, uploadSize);
		command_buffer = begin_command_buffer();
		copyRegion[0].srcOffset = 0;
		copyRegion[0].dstOffset = uploadDone;
		copyRegion[0].size = uploadSize;
		command_buffer.copyBuffer(vk_inst.staging_buffer.handle, vk_inst.vbo.vertex_buffer,1, &copyRegion[0]);
		end_command_buffer( command_buffer, __func__ );
		uploadDone += uploadSize;
	}
#ifdef USE_VK_VALIDATION
	SET_OBJECT_NAME(VkBuffer(vk_inst.vbo.vertex_buffer), "static VBO", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT);
	SET_OBJECT_NAME(VkDeviceMemory(vk_inst.vbo.buffer_memory), "static VBO memory", VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT);
#endif
	return true;
}
#endif

#include "../renderervk/shaders/spirv/shader_data.c"
#include "tr_main.hpp"
#define SHADER_MODULE(name) SHADER_MODULE(name, sizeof(name))

static void vk_create_shader_modules(void)
{
#ifdef USE_VK_VALIDATION
	int i, j, k, l;
#endif
	vk_inst.modules.vert.gen[0][0][0][0] = SHADER_MODULE(vert_tx0);
	vk_inst.modules.vert.gen[0][0][0][1] = SHADER_MODULE(vert_tx0_fog);
	vk_inst.modules.vert.gen[0][0][1][0] = SHADER_MODULE(vert_tx0_env);
	vk_inst.modules.vert.gen[0][0][1][1] = SHADER_MODULE(vert_tx0_env_fog);

	vk_inst.modules.vert.gen[1][0][0][0] = SHADER_MODULE(vert_tx1);
	vk_inst.modules.vert.gen[1][0][0][1] = SHADER_MODULE(vert_tx1_fog);
	vk_inst.modules.vert.gen[1][0][1][0] = SHADER_MODULE(vert_tx1_env);
	vk_inst.modules.vert.gen[1][0][1][1] = SHADER_MODULE(vert_tx1_env_fog);

	vk_inst.modules.vert.gen[1][1][0][0] = SHADER_MODULE(vert_tx1_cl);
	vk_inst.modules.vert.gen[1][1][0][1] = SHADER_MODULE(vert_tx1_cl_fog);
	vk_inst.modules.vert.gen[1][1][1][0] = SHADER_MODULE(vert_tx1_cl_env);
	vk_inst.modules.vert.gen[1][1][1][1] = SHADER_MODULE(vert_tx1_cl_env_fog);

	vk_inst.modules.vert.gen[2][0][0][0] = SHADER_MODULE(vert_tx2);
	vk_inst.modules.vert.gen[2][0][0][1] = SHADER_MODULE(vert_tx2_fog);
	vk_inst.modules.vert.gen[2][0][1][0] = SHADER_MODULE(vert_tx2_env);
	vk_inst.modules.vert.gen[2][0][1][1] = SHADER_MODULE(vert_tx2_env_fog);

	vk_inst.modules.vert.gen[2][1][0][0] = SHADER_MODULE(vert_tx2_cl);
	vk_inst.modules.vert.gen[2][1][0][1] = SHADER_MODULE(vert_tx2_cl_fog);
	vk_inst.modules.vert.gen[2][1][1][0] = SHADER_MODULE(vert_tx2_cl_env);
	vk_inst.modules.vert.gen[2][1][1][1] = SHADER_MODULE(vert_tx2_cl_env_fog);
#ifdef USE_VK_VALIDATION
	for (i = 0; i < 3; i++)
	{
		const char *tx[] = {"single", "double", "triple"};
		const char *cl[] = {"", "+cl"};
		const char *env[] = {"", "+env"};
		const char *fog[] = {"", "+fog"};
		for (j = 0; j < 2; j++)
		{
			for (k = 0; k < 2; k++)
			{
				for (l = 0; l < 2; l++)
				{
					const char *s = va("%s-texture%s%s%s vertex module", tx[i], cl[j], env[k], fog[l]);
					SET_OBJECT_NAME(VkShaderModule(vk_inst.modules.vert.gen[i][j][k][l]), s, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT);
				}
			}
		}
	}
#endif
	// specialized depth-fragment shader
	vk_inst.modules.frag.gen0_df = SHADER_MODULE(frag_tx0_df);
#ifdef USE_VK_VALIDATION
	SET_OBJECT_NAME(VkShaderModule(vk_inst.modules.frag.gen0_df), "single-texture df fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT);
#endif
	// fixed-color (1.0) shader modules
	vk_inst.modules.vert.ident1[0][0][0] = SHADER_MODULE(vert_tx0_ident1);
	vk_inst.modules.vert.ident1[0][0][1] = SHADER_MODULE(vert_tx0_ident1_fog);
	vk_inst.modules.vert.ident1[0][1][0] = SHADER_MODULE(vert_tx0_ident1_env);
	vk_inst.modules.vert.ident1[0][1][1] = SHADER_MODULE(vert_tx0_ident1_env_fog);
	vk_inst.modules.vert.ident1[1][0][0] = SHADER_MODULE(vert_tx1_ident1);
	vk_inst.modules.vert.ident1[1][0][1] = SHADER_MODULE(vert_tx1_ident1_fog);
	vk_inst.modules.vert.ident1[1][1][0] = SHADER_MODULE(vert_tx1_ident1_env);
	vk_inst.modules.vert.ident1[1][1][1] = SHADER_MODULE(vert_tx1_ident1_env_fog);
#ifdef USE_VK_VALIDATION
	for (i = 0; i < 2; i++)
	{
		const char *tx[] = {"single", "double"};
		const char *env[] = {"", "+env"};
		const char *fog[] = {"", "+fog"};
		for (j = 0; j < 2; j++)
		{
			for (k = 0; k < 2; k++)
			{
				const char *s = va("%s-texture identity%s%s vertex module", tx[i], env[j], fog[k]);
				SET_OBJECT_NAME(VkShaderModule(vk_inst.modules.vert.ident1[i][j][k]), s, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT);
			}
		}
	}
#endif

	vk_inst.modules.frag.ident1[0][0] = SHADER_MODULE(frag_tx0_ident1);
	vk_inst.modules.frag.ident1[0][1] = SHADER_MODULE(frag_tx0_ident1_fog);
	vk_inst.modules.frag.ident1[1][0] = SHADER_MODULE(frag_tx1_ident1);
	vk_inst.modules.frag.ident1[1][1] = SHADER_MODULE(frag_tx1_ident1_fog);
#ifdef USE_VK_VALIDATION
	for (i = 0; i < 2; i++)
	{
		const char *tx[] = {"single", "double"};
		const char *fog[] = {"", "+fog"};
		for (j = 0; j < 2; j++)
		{
			const char *s = va("%s-texture identity%s fragment module", tx[i], fog[j]);
			SET_OBJECT_NAME(VkShaderModule(vk_inst.modules.frag.ident1[i][j]), s, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT);
		}
	}
#endif
	vk_inst.modules.vert.fixed[0][0][0] = SHADER_MODULE(vert_tx0_fixed);
	vk_inst.modules.vert.fixed[0][0][1] = SHADER_MODULE(vert_tx0_fixed_fog);
	vk_inst.modules.vert.fixed[0][1][0] = SHADER_MODULE(vert_tx0_fixed_env);
	vk_inst.modules.vert.fixed[0][1][1] = SHADER_MODULE(vert_tx0_fixed_env_fog);
	vk_inst.modules.vert.fixed[1][0][0] = SHADER_MODULE(vert_tx1_fixed);
	vk_inst.modules.vert.fixed[1][0][1] = SHADER_MODULE(vert_tx1_fixed_fog);
	vk_inst.modules.vert.fixed[1][1][0] = SHADER_MODULE(vert_tx1_fixed_env);
	vk_inst.modules.vert.fixed[1][1][1] = SHADER_MODULE(vert_tx1_fixed_env_fog);
#ifdef USE_VK_VALIDATION
	for (i = 0; i < 2; i++)
	{
		const char *tx[] = {"single", "double"};
		const char *env[] = {"", "+env"};
		const char *fog[] = {"", "+fog"};
		for (j = 0; j < 2; j++)
		{
			for (k = 0; k < 2; k++)
			{
				const char *s = va("%s-texture fixed-color%s%s vertex module", tx[i], env[j], fog[k]);
				SET_OBJECT_NAME(VkShaderModule(vk_inst.modules.vert.fixed[i][j][k]), s, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT);
			}
		}
	}
#endif
	vk_inst.modules.frag.fixed[0][0] = SHADER_MODULE(frag_tx0_fixed);
	vk_inst.modules.frag.fixed[0][1] = SHADER_MODULE(frag_tx0_fixed_fog);
	vk_inst.modules.frag.fixed[1][0] = SHADER_MODULE(frag_tx1_fixed);
	vk_inst.modules.frag.fixed[1][1] = SHADER_MODULE(frag_tx1_fixed_fog);
#ifdef USE_VK_VALIDATION
	for (i = 0; i < 2; i++)
	{
		const char *tx[] = {"single", "double"};
		const char *fog[] = {"", "+fog"};
		for (j = 0; j < 2; j++)
		{
			const char *s = va("%s-texture fixed-color%s fragment module", tx[i], fog[j]);
			SET_OBJECT_NAME(VkShaderModule(vk_inst.modules.frag.fixed[i][j]), s, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT);
		}
	}
#endif
	vk_inst.modules.frag.ent[0][0] = SHADER_MODULE(frag_tx0_ent);
	vk_inst.modules.frag.ent[0][1] = SHADER_MODULE(frag_tx0_ent_fog);
	// vk_inst.modules.frag.ent[1][0] = SHADER_MODULE( frag_tx1_ent );
	// vk_inst.modules.frag.ent[1][1] = SHADER_MODULE( frag_tx1_ent_fog );

#ifdef USE_VK_VALIDATION
	for (i = 0; i < 1; i++)
	{
		const char *tx[] = {"single" /*, "double" */};
		const char *fog[] = {"", "+fog"};
		for (j = 0; j < 2; j++)
		{
			const char *s = va("%s-texture entity-color%s fragment module", tx[i], fog[j]);
			SET_OBJECT_NAME(VkShaderModule(vk_inst.modules.frag.ent[i][j]), s, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT);
		}
	}
#endif
	vk_inst.modules.frag.gen[0][0][0] = SHADER_MODULE(frag_tx0);
	vk_inst.modules.frag.gen[0][0][1] = SHADER_MODULE(frag_tx0_fog);

	vk_inst.modules.frag.gen[1][0][0] = SHADER_MODULE(frag_tx1);
	vk_inst.modules.frag.gen[1][0][1] = SHADER_MODULE(frag_tx1_fog);

	vk_inst.modules.frag.gen[1][1][0] = SHADER_MODULE(frag_tx1_cl);
	vk_inst.modules.frag.gen[1][1][1] = SHADER_MODULE(frag_tx1_cl_fog);

	vk_inst.modules.frag.gen[2][0][0] = SHADER_MODULE(frag_tx2);
	vk_inst.modules.frag.gen[2][0][1] = SHADER_MODULE(frag_tx2_fog);

	vk_inst.modules.frag.gen[2][1][0] = SHADER_MODULE(frag_tx2_cl);
	vk_inst.modules.frag.gen[2][1][1] = SHADER_MODULE(frag_tx2_cl_fog);
#ifdef USE_VK_VALIDATION
	for (i = 0; i < 3; i++)
	{
		const char *tx[] = {"single", "double", "triple"};
		const char *cl[] = {"", "+cl"};
		const char *fog[] = {"", "+fog"};
		for (j = 0; j < 2; j++)
		{
			for (k = 0; k < 2; k++)
			{
				const char *s = va("%s-texture%s%s fragment module", tx[i], cl[j], fog[k]);
				SET_OBJECT_NAME(VkShaderModule(vk_inst.modules.frag.gen[i][j][k]), s, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT);
			}
		}
	}
#endif
	vk_inst.modules.vert.light[0] = SHADER_MODULE(vert_light);
	vk_inst.modules.vert.light[1] = SHADER_MODULE(vert_light_fog);
#ifdef USE_VK_VALIDATION
	SET_OBJECT_NAME(VkShaderModule(vk_inst.modules.vert.light[0]), "light vertex module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT);
	SET_OBJECT_NAME(VkShaderModule(vk_inst.modules.vert.light[1]), "light fog vertex module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT);
#endif
	vk_inst.modules.frag.light[0][0] = SHADER_MODULE(frag_light);
	vk_inst.modules.frag.light[0][1] = SHADER_MODULE(frag_light_fog);
	vk_inst.modules.frag.light[1][0] = SHADER_MODULE(frag_light_line);
	vk_inst.modules.frag.light[1][1] = SHADER_MODULE(frag_light_line_fog);
#ifdef USE_VK_VALIDATION
	SET_OBJECT_NAME(VkShaderModule(vk_inst.modules.frag.light[0][0]), "light fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT);
	SET_OBJECT_NAME(VkShaderModule(vk_inst.modules.frag.light[0][1]), "light fog fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT);
	SET_OBJECT_NAME(VkShaderModule(vk_inst.modules.frag.light[1][0]), "linear light fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT);
	SET_OBJECT_NAME(VkShaderModule(vk_inst.modules.frag.light[1][1]), "linear light fog fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT);
#endif
	vk_inst.modules.color_fs = SHADER_MODULE(color_frag_spv);
	vk_inst.modules.color_vs = SHADER_MODULE(color_vert_spv);
#ifdef USE_VK_VALIDATION
	SET_OBJECT_NAME(VkShaderModule(vk_inst.modules.color_vs), "single-color vertex module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT);
	SET_OBJECT_NAME(VkShaderModule(vk_inst.modules.color_fs), "single-color fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT);
#endif
	vk_inst.modules.fog_vs = SHADER_MODULE(fog_vert_spv);
	vk_inst.modules.fog_fs = SHADER_MODULE(fog_frag_spv);
#ifdef USE_VK_VALIDATION
	SET_OBJECT_NAME(VkShaderModule(vk_inst.modules.fog_vs), "fog-only vertex module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT);
	SET_OBJECT_NAME(VkShaderModule(vk_inst.modules.fog_fs), "fog-only fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT);
#endif
	vk_inst.modules.dot_vs = SHADER_MODULE(dot_vert_spv);
	vk_inst.modules.dot_fs = SHADER_MODULE(dot_frag_spv);
#ifdef USE_VK_VALIDATION
	SET_OBJECT_NAME(VkShaderModule(vk_inst.modules.dot_vs), "dot vertex module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT);
	SET_OBJECT_NAME(VkShaderModule(vk_inst.modules.dot_fs), "dot fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT);
#endif
	vk_inst.modules.bloom_fs = SHADER_MODULE(bloom_frag_spv);
	vk_inst.modules.blur_fs = SHADER_MODULE(blur_frag_spv);
	vk_inst.modules.blend_fs = SHADER_MODULE(blend_frag_spv);
#ifdef USE_VK_VALIDATION
	SET_OBJECT_NAME(VkShaderModule(vk_inst.modules.bloom_fs), "bloom extraction fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT);
	SET_OBJECT_NAME(VkShaderModule(vk_inst.modules.blur_fs), "gaussian blur fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT);
	SET_OBJECT_NAME(VkShaderModule(vk_inst.modules.blend_fs), "final bloom blend fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT);
#endif
	vk_inst.modules.gamma_fs = SHADER_MODULE(gamma_frag_spv);
	vk_inst.modules.gamma_vs = SHADER_MODULE(gamma_vert_spv);
#ifdef USE_VK_VALIDATION
	SET_OBJECT_NAME(VkShaderModule(vk_inst.modules.gamma_fs), "gamma post-processing fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT);
	SET_OBJECT_NAME(VkShaderModule(vk_inst.modules.gamma_vs), "gamma post-processing vertex module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT);
#endif
}

static void vk_alloc_persistent_pipelines(void)
{
	unsigned int state_bits;
	Vk_Pipeline_Def def{};

	// skybox
	{
		def.shader_type = Vk_Shader_Type::TYPE_SIGNLE_TEXTURE_FIXED_COLOR;
		def.color.rgb = tr.identityLightByte;
		def.color.alpha = tr.identityLightByte;
		def.face_culling = cullType_t::CT_FRONT_SIDED;
		def.polygon_offset = false;
		def.mirror = false;
		vk_inst.skybox_pipeline = vk_find_pipeline_ext(0, def, true);
	}

	// stencil shadows
	{
		cullType_t cull_types[2] = {cullType_t::CT_FRONT_SIDED, cullType_t::CT_BACK_SIDED};
		bool mirror_flags[2] = {false, true};
		int i, j;

		def = {};
		def.polygon_offset = false;
		def.state_bits = 0;
		def.shader_type = Vk_Shader_Type::TYPE_SIGNLE_TEXTURE;
		def.shadow_phase = Vk_Shadow_Phase::SHADOW_EDGES;

		for (i = 0; i < 2; i++)
		{
			def.face_culling = cull_types[i];
			for (j = 0; j < 2; j++)
			{
				def.mirror = mirror_flags[j];
				vk_inst.shadow_volume_pipelines[i][j] = vk_find_pipeline_ext(0, def, r_shadows->integer ? true : false);
			}
		}
	}
	{
		def = {};
		def.face_culling = cullType_t::CT_FRONT_SIDED;
		def.polygon_offset = false;
		def.state_bits = GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO;
		def.shader_type = Vk_Shader_Type::TYPE_SIGNLE_TEXTURE;
		def.mirror = false;
		def.shadow_phase = Vk_Shadow_Phase::SHADOW_FS_QUAD;
		def.primitives = Vk_Primitive_Topology::TRIANGLE_STRIP;
		vk_inst.shadow_finish_pipeline = vk_find_pipeline_ext(0, def, r_shadows->integer ? true : false);
	}

	// fog and dlights
	{
		unsigned int fog_state_bits[2] = {
			GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_DEPTHFUNC_EQUAL, // fogPass == FP_EQUAL
			GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA						 // fogPass == FP_LE
		};
		unsigned int dlight_state_bits[2] = {
			GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL, // modulated
			GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL		 // additive
		};
		bool polygon_offset[2] = {false, true};
		int i, j, k;

		#ifdef USE_PMLIGHT
				int l;
		#endif

		def = {};
		def.shader_type = Vk_Shader_Type::TYPE_SIGNLE_TEXTURE;
		def.mirror = false;

		for (i = 0; i < 2; i++)
		{
			unsigned fog_state = fog_state_bits[i];
			unsigned dlight_state = dlight_state_bits[i];

			for (j = 0; j < 3; j++)
			{
				def.face_culling = static_cast<cullType_t>(j); // cullType_t value

				for (k = 0; k < 2; k++)
				{
					def.polygon_offset = polygon_offset[k];
#ifdef USE_FOG_ONLY
					def.shader_type = Vk_Shader_Type::TYPE_FOG_ONLY;
#else
					def.shader_type = Vk_Shader_Type::TYPE_SIGNLE_TEXTURE;
#endif
					def.state_bits = fog_state;
					vk_inst.fog_pipelines[i][j][k] = vk_find_pipeline_ext(0, def, true);

					def.shader_type = Vk_Shader_Type::TYPE_SIGNLE_TEXTURE;
					def.state_bits = dlight_state;
#ifdef USE_LEGACY_DLIGHTS
#ifdef USE_PMLIGHT
					vk_inst.dlight_pipelines[i][j][k] = vk_find_pipeline_ext(0, def, r_dlightMode->integer == 0 ? true : false);
#else
					vk_inst.dlight_pipelines[i][j][k] = vk_find_pipeline_ext(0, def, true);
#endif
#endif
				}
			}
		}

#ifdef USE_PMLIGHT
		def.state_bits = GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL;
		// def.shader_type = Vk_Shader_Type::TYPE_SIGNLE_TEXTURE_LIGHTING;
		for (i = 0; i < 3; i++)
		{ // cullType
			def.face_culling = static_cast<cullType_t>(i);
			for (j = 0; j < 2; j++)
			{ // polygonOffset
				def.polygon_offset = polygon_offset[j];
				for (k = 0; k < 2; k++)
				{
					def.fog_stage = k; // fogStage
					for (l = 0; l < 2; l++)
					{
						def.abs_light = l;
						def.shader_type = Vk_Shader_Type::TYPE_SIGNLE_TEXTURE_LIGHTING;
						vk_inst.dlight_pipelines_x[i][j][k][l] = vk_find_pipeline_ext(0, def, false);
						def.shader_type = Vk_Shader_Type::TYPE_SIGNLE_TEXTURE_LIGHTING_LINEAR;
						vk_inst.dlight1_pipelines_x[i][j][k][l] = vk_find_pipeline_ext(0, def, false);
					}
				}
			}
		}
#endif // USE_PMLIGHT
	}

	// RT_BEAM surface
	{
		def = {};
		def.state_bits = GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE;
		def.face_culling = cullType_t::CT_FRONT_SIDED;
		def.primitives = Vk_Primitive_Topology::TRIANGLE_STRIP;
		vk_inst.surface_beam_pipeline = vk_find_pipeline_ext(0, def, false);
	}

	// axis for missing models
	{
		def = {};
		def.state_bits = GLS_DEFAULT;
		def.shader_type = Vk_Shader_Type::TYPE_SIGNLE_TEXTURE;
		def.face_culling = cullType_t::CT_TWO_SIDED;
		def.primitives = Vk_Primitive_Topology::LINE_LIST;
		if (vk_inst.wideLines)
			def.line_width = 3;
		vk_inst.surface_axis_pipeline = vk_find_pipeline_ext(0, def, false);
	}

	// flare visibility test dot
	if (vk_inst.fragmentStores)
	{
		def = {};
		// def.state_bits = GLS_DEFAULT;
		def.face_culling = cullType_t::CT_TWO_SIDED;
		def.shader_type = Vk_Shader_Type::TYPE_DOT;
		def.primitives = Vk_Primitive_Topology::POINT_LIST;
		vk_inst.dot_pipeline = vk_find_pipeline_ext(0, def, true);
	}

	// DrawTris()
	state_bits = GLS_POLYMODE_LINE | GLS_DEPTHMASK_TRUE;
	{
		def = {};
		def.state_bits = state_bits;
		def.shader_type = Vk_Shader_Type::TYPE_COLOR_WHITE;
		def.face_culling = cullType_t::CT_FRONT_SIDED;
		vk_inst.tris_debug_pipeline = vk_find_pipeline_ext(0, def, false);
	}
	{
		def = {};
		def.state_bits = state_bits;
		def.shader_type = Vk_Shader_Type::TYPE_COLOR_WHITE;
		def.face_culling = cullType_t::CT_BACK_SIDED;
		vk_inst.tris_mirror_debug_pipeline = vk_find_pipeline_ext(0, def, false);
	}
	{
		def = {};
		def.state_bits = state_bits;
		def.shader_type = Vk_Shader_Type::TYPE_COLOR_GREEN;
		def.face_culling = cullType_t::CT_FRONT_SIDED;
		vk_inst.tris_debug_green_pipeline = vk_find_pipeline_ext(0, def, false);
	}
	{
		def = {};
		def.state_bits = state_bits;
		def.shader_type = Vk_Shader_Type::TYPE_COLOR_GREEN;
		def.face_culling = cullType_t::CT_BACK_SIDED;
		vk_inst.tris_mirror_debug_green_pipeline = vk_find_pipeline_ext(0, def, false);
	}
	{
		def = {};
		def.state_bits = state_bits;
		def.shader_type = Vk_Shader_Type::TYPE_COLOR_RED;
		def.face_culling = cullType_t::CT_FRONT_SIDED;
		vk_inst.tris_debug_red_pipeline = vk_find_pipeline_ext(0, def, false);
	}
	{
		def = {};
		def.state_bits = state_bits;
		def.shader_type = Vk_Shader_Type::TYPE_COLOR_RED;
		def.face_culling = cullType_t::CT_BACK_SIDED;
		vk_inst.tris_mirror_debug_red_pipeline = vk_find_pipeline_ext(0, def, false);
	}

	// DrawNormals()
	{
		def = {};
		def.state_bits = GLS_DEPTHMASK_TRUE;
		def.shader_type = Vk_Shader_Type::TYPE_SIGNLE_TEXTURE;
		def.primitives = Vk_Primitive_Topology::LINE_LIST;
		vk_inst.normals_debug_pipeline = vk_find_pipeline_ext(0, def, false);
	}

	// RB_DebugPolygon()
	{
		def = {};
		def.state_bits = GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE;
		def.shader_type = Vk_Shader_Type::TYPE_SIGNLE_TEXTURE;
		vk_inst.surface_debug_pipeline_solid = vk_find_pipeline_ext(0, def, false);
	}
	{
		def = {};
		def.state_bits = GLS_POLYMODE_LINE | GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE;
		def.shader_type = Vk_Shader_Type::TYPE_SIGNLE_TEXTURE;
		def.primitives = Vk_Primitive_Topology::LINE_LIST;
		vk_inst.surface_debug_pipeline_outline = vk_find_pipeline_ext(0, def, false);
	}

	// RB_ShowImages
	{
		def = {};
		def.state_bits = GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
		def.shader_type = Vk_Shader_Type::TYPE_SIGNLE_TEXTURE;
		def.primitives = Vk_Primitive_Topology::TRIANGLE_STRIP;
		vk_inst.images_debug_pipeline = vk_find_pipeline_ext(0, def, false);
		def.state_bits = GLS_DEPTHTEST_DISABLE;
		def.shader_type = Vk_Shader_Type::TYPE_COLOR_BLACK;
		def.primitives = Vk_Primitive_Topology::TRIANGLE_STRIP;
		vk_inst.images_debug_pipeline2 = vk_find_pipeline_ext( 0, def, false );
	}
}

static void set_shader_stage_desc(vk::PipelineShaderStageCreateInfo &desc, const vk::ShaderStageFlagBits stage, const vk::ShaderModule &shader_module, const char *entry)
{
	desc.pNext = nullptr;
	desc.flags = {};
	desc.stage = stage;
	desc.module = shader_module;
	desc.pName = entry;
	desc.pSpecializationInfo = nullptr;
}

void vk_create_blur_pipeline(const uint32_t index, const uint32_t width, const uint32_t height, const bool horizontal_pass)
{
	vk::Pipeline *pipeline = &vk_inst.blur_pipeline[index];

	if (*pipeline)
	{
		VK_CHECK(vk_inst.device.waitIdle());
		vk_inst.device.destroyPipeline(*pipeline);
		*pipeline = nullptr;
	}

	vk::PipelineVertexInputStateCreateInfo vertex_input_state{};

	// shaders
	std::array<vk::PipelineShaderStageCreateInfo, 2> shader_stages;
	set_shader_stage_desc(shader_stages[0], vk::ShaderStageFlagBits::eVertex, vk_inst.modules.gamma_vs, "main");
	set_shader_stage_desc(shader_stages[1], vk::ShaderStageFlagBits::eFragment, vk_inst.modules.blur_fs, "main");

	std::array<float, 3> frag_spec_data{{1.2f / (float)width, 1.2f / (float)height, 1.0}}; // x-offset, y-offset, correction

	if (horizontal_pass)
	{
		frag_spec_data[1] = 0.0;
	}
	else
	{
		frag_spec_data[0] = 0.0;
	}

	std::array<vk::SpecializationMapEntry, 3> spec_entries = {{
		{0, 0 * sizeof(frag_spec_data[0]), sizeof(frag_spec_data[0])},
		{1, 1 * sizeof(frag_spec_data[1]), sizeof(frag_spec_data[1])},
		{2, 2 * sizeof(frag_spec_data[2]), sizeof(frag_spec_data[2])},
	}};

	vk::SpecializationInfo frag_spec_info{
		static_cast<uint32_t>(spec_entries.size()), spec_entries.data(), sizeof(frag_spec_data),
		&frag_spec_data[0]};

	shader_stages[1].pSpecializationInfo = &frag_spec_info;

	//
	// Primitive assembly.
	//
	vk::PipelineInputAssemblyStateCreateInfo input_assembly_state{{},
																  vk::PrimitiveTopology::eTriangleStrip,
																  vk::False,
																  nullptr};

	//
	// Viewport.
	//
	vk::Viewport viewport{0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f};
	vk::Rect2D scissor{{(int32_t)viewport.x, (int32_t)viewport.y}, {(uint32_t)viewport.width, (uint32_t)viewport.height}};

	vk::PipelineViewportStateCreateInfo viewport_state{{}, 1, &viewport, 1, &scissor};

	//
	// Rasterization.
	//
	vk::PipelineRasterizationStateCreateInfo rasterization_state{
		{},
		vk::False,
		vk::False,
		vk::PolygonMode::eFill,
		vk::CullModeFlagBits::eNone,
		vk::FrontFace::eClockwise,
		vk::False,
		0.0f,
		0.0f,
		0.0f,
		1.0f,
		nullptr};

	vk::PipelineMultisampleStateCreateInfo multisample_state{{},
															 vk::SampleCountFlagBits::e1,
															 vk::False,
															 1.0f,
															 nullptr,
															 vk::False,
															 vk::False,
															 nullptr};

	vk::PipelineColorBlendAttachmentState attachment_blend_state{
		vk::False,
		{},
		{},
		{},
		{},
		{},
		{},
		vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
			vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA};

	vk::PipelineColorBlendStateCreateInfo blend_state{
		{},
		vk::False,
		vk::LogicOp::eCopy,
		1,
		&attachment_blend_state,
		{0.0f, 0.0f, 0.0f, 0.0f},
		nullptr};

	vk::GraphicsPipelineCreateInfo create_info{
		{},
		static_cast<uint32_t>(shader_stages.size()),
		shader_stages.data(),
		&vertex_input_state,
		&input_assembly_state,
		nullptr,
		&viewport_state,
		&rasterization_state,
		&multisample_state,
		nullptr,
		&blend_state,
		nullptr,
		vk_inst.pipeline_layout_post_process,
		vk_inst.render_pass.blur[index],
		0,
		nullptr,
		-1,
		nullptr};

#ifdef USE_VK_VALIDATION
	try
	{
		auto createGraphicsPipelineResult = vk_inst.device.createGraphicsPipeline(nullptr, create_info);
		if (static_cast<int>(createGraphicsPipelineResult.result) < 0)
		{
			ri.Error(ERR_FATAL, "Vulkan: %s returned %s", "vk_create_blur_pipeline -> createGraphicsPipeline", vk::to_string(createGraphicsPipelineResult.result).data());
		}
		else
		{
			*pipeline = createGraphicsPipelineResult.value;
			SET_OBJECT_NAME(VkPipeline(&pipeline), va("%s blur pipeline %i", horizontal_pass ? "horizontal" : "vertical", index / 2 + 1), VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT);
		}
	}
	catch (vk::SystemError &err)
	{
		ri.Error(ERR_FATAL, "Vulkan error in function: %s, what: %s", "vk_create_blur_pipeline -> createGraphicsPipeline", err.what());
	}

#else
	VK_CHECK_ASSIGN(*pipeline, vk_inst.device.createGraphicsPipeline(vk_inst.pipelineCache, create_info));
#endif
}

void vk_update_post_process_pipelines(void)
{
	if (vk_inst.fboActive)
	{
		// update gamma shader
		vk_create_post_process_pipeline(0, 0, 0);
		if (vk_inst.capture.image)
		{
			// update capture pipeline
			vk_create_post_process_pipeline(3, gls.captureWidth, gls.captureHeight);
		}
		if (r_bloom->integer)
		{
			// update bloom shaders
			uint32_t width = gls.captureWidth;
			uint32_t height = gls.captureHeight;
			uint32_t i;

			vk_create_post_process_pipeline(1, width, height); // bloom extraction

			for (i = 0; i < vk_inst.blur_pipeline.size(); i += 2)
			{
				width /= 2;
				height /= 2;
				vk_create_blur_pipeline(i + 0, width, height, true);  // horizontal
				vk_create_blur_pipeline(i + 1, width, height, false); // vertical
			}

			vk_create_post_process_pipeline(2, glConfig.vidWidth, glConfig.vidHeight); // bloom blending
		}
	}
}

typedef struct vk_attach_desc_s
{
	vk::Image descriptor = {};
	vk::ImageView *image_view = nullptr;
	vk::ImageUsageFlags usage = {};
	vk::MemoryRequirements reqs = {};
	uint32_t memoryTypeIndex = 0;
	vk::DeviceSize memory_offset = 0;
	// for layout transition:
	vk::ImageAspectFlags aspect_flags = {};
	vk::ImageLayout image_layout = vk::ImageLayout::eUndefined;
	vk::Format image_format = vk::Format::eUndefined;
} vk_attach_desc_t;

static std::array<vk_attach_desc_t, MAX_ATTACHMENTS_IN_POOL> attachments{};
static uint32_t num_attachments = 0;

static void vk_clear_attachment_pool(void)
{
	num_attachments = 0;
}

static void vk_alloc_attachments(void)
{
	if (num_attachments == 0)
	{
		return;
	}

	if (vk_inst.image_memory_count >= arrayLen(vk_inst.image_memory))
	{
		ri.Error(ERR_DROP, "vk_inst.image_memory_count == %i", (int)arrayLen(vk_inst.image_memory));
	}

	uint32_t memoryTypeBits = ~0U;
	vk::DeviceSize offset = 0;

	for (uint32_t i = 0; i < num_attachments; i++)
	{
#ifdef MIN_IMAGE_ALIGN
		vk::DeviceSize alignment = std::max(attachments[i].reqs.alignment, MIN_IMAGE_ALIGN);
#else
		vk::DeviceSize alignment = attachments[i].reqs.alignment;
#endif
		memoryTypeBits &= attachments[i].reqs.memoryTypeBits;
		offset = PAD(offset, alignment);
		attachments[i].memory_offset = offset;
		offset += attachments[i].reqs.size;
#ifdef DEBUG
		ri.Printf(PRINT_ALL, S_COLOR_CYAN "[%i] type %i, size %i, align %i\n", i,
				  attachments[i].reqs.memoryTypeBits,
				  (int)attachments[i].reqs.size,
				  (int)attachments[i].reqs.alignment);
#endif
	}

	uint32_t memoryTypeIndex;
	if (num_attachments == 1 && (attachments[0].usage & vk::ImageUsageFlagBits::eTransientAttachment))
	{
		memoryTypeIndex = find_memory_type2(memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal | vk::MemoryPropertyFlagBits::eLazilyAllocated, nullptr);
		if (memoryTypeIndex == ~0U)
		{
			memoryTypeIndex = find_memory_type(memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);
		}
	}
	else
	{
		memoryTypeIndex = find_memory_type(memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);
	}

#ifdef DEBUG
	ri.Printf(PRINT_ALL, "memory type bits: %04x\n", memoryTypeBits);
	ri.Printf(PRINT_ALL, "memory type index: %04x\n", memoryTypeIndex);
	ri.Printf(PRINT_ALL, "total size: %i\n", (int)offset);
#endif

	vk::MemoryAllocateInfo alloc_info = {
		offset,
		memoryTypeIndex,
	};

	vk::MemoryDedicatedAllocateInfoKHR alloc_info2{};
	if (num_attachments == 1 && vk_inst.dedicatedAllocation)
	{
		alloc_info2 = {attachments[0].descriptor, {}};
		alloc_info.pNext = &alloc_info2;
	}

	// Allocate memory
	vk::DeviceMemory memory;
	VK_CHECK_ASSIGN(memory, vk_inst.device.allocateMemory(alloc_info));

	vk_inst.image_memory[vk_inst.image_memory_count++] = memory;

	// Bind memory and create image views
	for (uint32_t i = 0; i < num_attachments; i++)
	{
		VK_CHECK(vk_inst.device.bindImageMemory(attachments[i].descriptor, memory, attachments[i].memory_offset));

		vk::ImageViewCreateInfo view_desc{{},
										  attachments[i].descriptor,
										  vk::ImageViewType::e2D,
										  attachments[i].image_format,
										  {},
										  {
											  attachments[i].aspect_flags,
											  0,
											  1,
											  0,
											  1,
										  },
										  nullptr};

		// Create the image view and assign it to the pointer
		VK_CHECK_ASSIGN(*attachments[i].image_view, vk_inst.device.createImageView(view_desc));
	}

	// Perform layout transition
	vk::CommandBuffer command_buffer = begin_command_buffer();
	for (uint32_t i = 0; i < num_attachments; i++)
	{
		record_image_layout_transition(command_buffer,
									   attachments[i].descriptor,
									   vk::ImageAspectFlags(attachments[i].aspect_flags),
									   vk::ImageLayout::eUndefined, // old_layout
									   vk::ImageLayout(attachments[i].image_layout));
	}
	end_command_buffer(command_buffer, __func__);

	num_attachments = 0;
}

static void vk_add_attachment_desc(const vk::Image &desc, vk::ImageView &image_view, vk::ImageUsageFlags usage,
								   vk::MemoryRequirements *reqs, vk::Format image_format,
								   vk::ImageAspectFlags aspect_flags, vk::ImageLayout image_layout)
{
	if (num_attachments >= attachments.size())
	{
		ri.Error(ERR_FATAL, "Attachments array overflow");
	}
	else
	{
		attachments[num_attachments].descriptor = desc;
		attachments[num_attachments].image_view = &image_view;
		attachments[num_attachments].usage = usage;
		attachments[num_attachments].reqs = *reqs;
		attachments[num_attachments].aspect_flags = aspect_flags;
		attachments[num_attachments].image_layout = image_layout;
		attachments[num_attachments].image_format = image_format;
		attachments[num_attachments].memory_offset = 0;
		num_attachments++;
	}
}

static void vk_get_image_memory_requirements(const vk::Image &image, vk::MemoryRequirements *memory_requirements)
{
	vk::detail::DispatchLoaderDynamic dldi;

	// Correct way to initialize DispatchLoaderDynamic
	dldi.init(vk_instance, vk_inst.device);

	if (vk_inst.dedicatedAllocation)
	{
		vk::MemoryDedicatedRequirementsKHR mem_req2 = {};
		vk::MemoryRequirements2KHR memory_requirements2 = {};
		vk::ImageMemoryRequirementsInfo2KHR image_requirements2 = {};

		// Initialize the structures directly with the appropriate values
		image_requirements2.image = image;
		memory_requirements2.pNext = &mem_req2;

		// Call the Vulkan-Hpp function to get image memory requirements
		vk_inst.device.getImageMemoryRequirements2KHR(&image_requirements2, &memory_requirements2, dldi);

		*memory_requirements = memory_requirements2.memoryRequirements;
	}
	else
	{
		// Get memory requirements using vk::Device's getImageMemoryRequirements method
		*memory_requirements = vk_inst.device.getImageMemoryRequirements(image);
	}
}

static void create_color_attachment(uint32_t width, uint32_t height, vk::SampleCountFlagBits samples,
									vk::Format format, vk::ImageUsageFlags usage, vk::Image &image,
									vk::ImageView &image_view, vk::ImageLayout image_layout, bool multisample)
{
	vk::MemoryRequirements memory_requirements;

	if (multisample && !(usage & vk::ImageUsageFlagBits::eSampled))
		usage |= vk::ImageUsageFlagBits::eTransientAttachment;

	// create color image
	vk::ImageCreateInfo create_desc{{},
									vk::ImageType::e2D,
									format,
									{width, height, 1},
									1,
									1,
									samples,
									vk::ImageTiling::eOptimal,
									usage,
									vk::SharingMode::eExclusive,
									0,
									nullptr,
									vk::ImageLayout::eUndefined,
									nullptr};

	// VK_CHECK(qvkCreateImage(vk_inst.device, &create_desc, nullptr, image));
	VK_CHECK_ASSIGN(image, vk_inst.device.createImage(create_desc));

	vk_get_image_memory_requirements(image, &memory_requirements);

	vk_add_attachment_desc(image, image_view, usage, &memory_requirements, format, vk::ImageAspectFlagBits::eColor, image_layout);
}

static void create_depth_attachment(uint32_t width, uint32_t height, vk::SampleCountFlagBits samples,
									vk::Image &image, vk::ImageView &image_view, bool allowTransient)
{
	vk::MemoryRequirements memory_requirements;
	vk::ImageAspectFlags image_aspect_flags;

	vk::ImageUsageFlags usage{vk::ImageUsageFlagBits::eDepthStencilAttachment};
	if (allowTransient)
	{
		usage |= vk::ImageUsageFlagBits::eTransientAttachment;
	}

	// create depth image
	vk::ImageCreateInfo create_desc{{},
									vk::ImageType::e2D,
									vk_inst.depth_format,
									{width, height, 1},
									1,
									1,
									samples,
									vk::ImageTiling::eOptimal,
									usage,
									vk::SharingMode::eExclusive,
									0,
									nullptr,
									vk::ImageLayout::eUndefined,
									nullptr};

	image_aspect_flags = vk::ImageAspectFlagBits::eDepth;
	if (glConfig.stencilBits)
		image_aspect_flags |= vk::ImageAspectFlagBits::eStencil;

	// VK_CHECK(qvkCreateImage(vk_inst.device, &create_desc, nullptr, image));
	VK_CHECK_ASSIGN(image, vk_inst.device.createImage(create_desc));

	vk_get_image_memory_requirements(image, &memory_requirements);

	vk_add_attachment_desc(image, image_view, create_desc.usage, &memory_requirements, vk_inst.depth_format,
						   image_aspect_flags, vk::ImageLayout::eDepthStencilAttachmentOptimal);
}

static void vk_create_attachments(void)
{
	uint32_t i;

	vk_clear_attachment_pool();

	// It looks like resulting performance depends from order you're creating/allocating
	// memory for attachments in vulkan i.e. similar images grouped together will provide best results
	// so [resolve0][resolve1][msaa0][msaa1][depth0][depth1] is most optimal
	// while cases like [resolve0][depth0][color0][...] is the worst

	// TODO: preallocate first image chunk in attachment' memory pool?
	if (vk_inst.fboActive)
	{

		vk::ImageUsageFlags usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled;

		// bloom
		if (r_bloom->integer)
		{
			uint32_t width = gls.captureWidth;
			uint32_t height = gls.captureHeight;

			create_color_attachment(width, height, vk::SampleCountFlagBits::e1, vk_inst.bloom_format,
									usage, vk_inst.bloom_image[0], vk_inst.bloom_image_view[0],
									vk::ImageLayout::eShaderReadOnlyOptimal, false);

			for (i = 1; i < arrayLen(vk_inst.bloom_image); i += 2)
			{
				width /= 2;
				height /= 2;
				create_color_attachment(width, height, vk::SampleCountFlagBits::e1, vk_inst.bloom_format,
										usage, vk_inst.bloom_image[i + 0], vk_inst.bloom_image_view[i + 0], vk::ImageLayout::eShaderReadOnlyOptimal, false);

				create_color_attachment(width, height, vk::SampleCountFlagBits::e1, vk_inst.bloom_format,
										usage, vk_inst.bloom_image[i + 1], vk_inst.bloom_image_view[i + 1], vk::ImageLayout::eShaderReadOnlyOptimal, false);
			}
		}

		// post-processing/msaa-resolve
		create_color_attachment(glConfig.vidWidth, glConfig.vidHeight, vk::SampleCountFlagBits::e1, vk_inst.color_format,
								usage | vk::ImageUsageFlagBits::eTransferSrc, vk_inst.color_image, vk_inst.color_image_view, vk::ImageLayout::eShaderReadOnlyOptimal, false);

		// screenmap-msaa
		if (vk_inst.screenMapSamples > vk::SampleCountFlagBits::e1)
		{
			create_color_attachment(vk_inst.screenMapWidth, vk_inst.screenMapHeight, vk_inst.screenMapSamples, vk_inst.color_format,
									vk::ImageUsageFlagBits::eColorAttachment, vk_inst.screenMap.color_image_msaa, vk_inst.screenMap.color_image_view_msaa, vk::ImageLayout::eColorAttachmentOptimal, true);
		}

		// screenmap/msaa-resolve
		create_color_attachment(vk_inst.screenMapWidth, vk_inst.screenMapHeight, vk::SampleCountFlagBits::e1, vk_inst.color_format,
								usage, vk_inst.screenMap.color_image, vk_inst.screenMap.color_image_view, vk::ImageLayout::eShaderReadOnlyOptimal, false);

		// screenmap depth
		create_depth_attachment(vk_inst.screenMapWidth, vk_inst.screenMapHeight, vk_inst.screenMapSamples, vk_inst.screenMap.depth_image, vk_inst.screenMap.depth_image_view, true);

		if (vk_inst.msaaActive)
		{
			create_color_attachment(glConfig.vidWidth, glConfig.vidHeight, vkSamples, vk_inst.color_format,
									vk::ImageUsageFlagBits::eColorAttachment, vk_inst.msaa_image, vk_inst.msaa_image_view, vk::ImageLayout::eColorAttachmentOptimal, true);
		}

		if (r_ext_supersample->integer)
		{
			// capture buffer
			usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc;
			create_color_attachment(gls.captureWidth, gls.captureHeight, vk::SampleCountFlagBits::e1, vk_inst.capture_format,
									usage, vk_inst.capture.image, vk_inst.capture.image_view, vk::ImageLayout::eTransferSrcOptimal, false);
		}
	} // if ( vk_inst.fboActive )

	// vk_alloc_attachments();

	create_depth_attachment(glConfig.vidWidth, glConfig.vidHeight, vkSamples, vk_inst.depth_image, vk_inst.depth_image_view,
							(vk_inst.fboActive && r_bloom->integer) ? false : true);

	vk_alloc_attachments();
#ifdef USE_VK_VALIDATION
	for (i = 0; i < vk_inst.image_memory_count; i++)
	{
		SET_OBJECT_NAME(VkDeviceMemory(vk_inst.image_memory[i]), va("framebuffer memory chunk %i", i), VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT);
	}

	SET_OBJECT_NAME(VkImage(vk_inst.depth_image), "depth attachment", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT);
	SET_OBJECT_NAME(VkImageView(vk_inst.depth_image_view), "depth attachment", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT);

	SET_OBJECT_NAME(VkImage(vk_inst.color_image), "color attachment", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT);
	SET_OBJECT_NAME(VkImageView(vk_inst.color_image_view), "color attachment", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT);

	SET_OBJECT_NAME(VkImage(vk_inst.capture.image), "capture image", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT);
	SET_OBJECT_NAME(VkImageView(vk_inst.capture.image_view), "capture image view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT);

	for (i = 0; i < arrayLen(vk_inst.bloom_image); i++)
	{
		SET_OBJECT_NAME(VkImage(vk_inst.bloom_image[i]), va("bloom attachment %i", i), VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT);
		SET_OBJECT_NAME(VkImageView(vk_inst.bloom_image_view[i]), va("bloom attachment %i", i), VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT);
	}
#endif
}

static void vk_create_framebuffers(void)
{
	std::array<vk::ImageView, 3> attachments;

	auto create_framebuffer = [&](vk::RenderPass renderPass, uint32_t attachmentCount,
								  uint32_t width, uint32_t height, const std::string &name,
								  vk::Framebuffer &framebuffer)
	{
		vk::FramebufferCreateInfo framebufferInfo{{},
												  renderPass,
												  attachmentCount,
												  attachments.data(),
												  width,
												  height,
												  1};

		VK_CHECK_ASSIGN(framebuffer, vk_inst.device.createFramebuffer(framebufferInfo));
#ifdef USE_VK_VALIDATION
		SET_OBJECT_NAME(VkFramebuffer(framebuffer), name.c_str(), VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT);
#endif
	};

	for (uint32_t n = 0; n < vk_inst.swapchain_image_count; n++)
	{
		if (r_fbo->integer == 0)
		{
			// Main framebuffer
			attachments[0] = vk_inst.swapchain_image_views[n];
			attachments[1] = vk_inst.depth_image_view;
			create_framebuffer(vk_inst.render_pass.main, 2, gls.windowWidth, gls.windowHeight, va("framebuffer - main %i", n), vk_inst.framebuffers.main[n]);
		}
		else
		{
			if (n == 0)
			{
				// Main framebuffer
				attachments[0] = vk_inst.color_image_view;
				attachments[1] = vk_inst.depth_image_view;
				if (vk_inst.msaaActive)
				{
					attachments[2] = vk_inst.msaa_image_view;
					create_framebuffer(vk_inst.render_pass.main, 3, glConfig.vidWidth, glConfig.vidHeight, "framebuffer - main", vk_inst.framebuffers.main[n]);
				}
				else
				{
					create_framebuffer(vk_inst.render_pass.main, 2, glConfig.vidWidth, glConfig.vidHeight, "framebuffer - main", vk_inst.framebuffers.main[n]);
				}
			}
			else
			{
				vk_inst.framebuffers.main[n] = vk_inst.framebuffers.main[0];
			}

			// Gamma correction framebuffer
			attachments[0] = vk_inst.swapchain_image_views[n];
			create_framebuffer(vk_inst.render_pass.gamma, 1, gls.windowWidth, gls.windowHeight, "framebuffer - gamma-correction", vk_inst.framebuffers.gamma[n]);
		}
	}

	if (vk_inst.fboActive)
	{
		// Screenmap framebuffer
		attachments[0] = vk_inst.screenMap.color_image_view;
		attachments[1] = vk_inst.screenMap.depth_image_view;
		if (vk_inst.screenMapSamples > vk::SampleCountFlagBits::e1)
		{
			attachments[2] = vk_inst.screenMap.color_image_view_msaa;
			create_framebuffer(vk_inst.render_pass.screenmap, 3, vk_inst.screenMapWidth, vk_inst.screenMapHeight, "framebuffer - screenmap", vk_inst.framebuffers.screenmap);
		}
		else
		{
			create_framebuffer(vk_inst.render_pass.screenmap, 2, vk_inst.screenMapWidth, vk_inst.screenMapHeight, "framebuffer - screenmap", vk_inst.framebuffers.screenmap);
		}

		if (vk_inst.capture.image)
		{
			// Capture framebuffer
			attachments[0] = vk_inst.capture.image_view;
			create_framebuffer(vk_inst.render_pass.capture, 1, gls.captureWidth, gls.captureHeight, "framebuffer - capture", vk_inst.framebuffers.capture);
		}

		if (r_bloom->integer)
		{
			uint32_t width = gls.captureWidth;
			uint32_t height = gls.captureHeight;

			// Bloom extraction framebuffer
			attachments[0] = vk_inst.bloom_image_view[0];
			create_framebuffer(vk_inst.render_pass.bloom_extract, 1, width, height, "framebuffer - bloom extraction", vk_inst.framebuffers.bloom_extract);

			// Blur framebuffers
			for (uint32_t n = 0; n < arrayLen(vk_inst.framebuffers.blur); n += 2)
			{
				width /= 2;
				height /= 2;

				attachments[0] = vk_inst.bloom_image_view[n + 1];
				create_framebuffer(vk_inst.render_pass.blur[n], 1, width, height, va("framebuffer - blur %i", n), vk_inst.framebuffers.blur[n]);

				attachments[0] = vk_inst.bloom_image_view[n + 2];
				create_framebuffer(vk_inst.render_pass.blur[n], 1, width, height, va("framebuffer - blur %i", n + 1), vk_inst.framebuffers.blur[n + 1]);
			}
		}
	}
}

static void vk_create_sync_primitives(void)
{
	uint32_t i;

	#ifdef USE_UPLOAD_QUEUE
		VK_CHECK_ASSIGN(vk_inst.image_uploaded2, vk_inst.device.createSemaphore({}));
	#endif

	// all commands submitted
	for (i = 0; i < NUM_COMMAND_BUFFERS; i++)
	{
		vk::SemaphoreCreateInfo desc{{}, nullptr};

		// swapchain image acquired
		VK_CHECK_ASSIGN(vk_inst.tess[i].image_acquired, vk_inst.device.createSemaphore(desc));

#ifdef USE_UPLOAD_QUEUE
		VK_CHECK_ASSIGN(vk_inst.tess[i].rendering_finished2 , vk_inst.device.createSemaphore(desc));
#endif

		vk::FenceCreateInfo fence_desc{};
		//fence_desc.flags = vk::FenceCreateFlagBits::eSignaled; // so it can be used to start rendering

		VK_CHECK_ASSIGN(vk_inst.tess[i].rendering_finished_fence, vk_inst.device.createFence(fence_desc));
		vk_inst.tess[i].waitForFence = false;
#ifdef USE_VK_VALIDATION
		SET_OBJECT_NAME(VkSemaphore(vk_inst.tess[i].image_acquired), va("image_acquired semaphore %i", i), VK_DEBUG_REPORT_OBJECT_TYPE_SEMAPHORE_EXT);
#ifdef USE_UPLOAD_QUEUE
		SET_OBJECT_NAME(VkSemaphore(vk_inst.tess[i].rendering_finished2), va( "rendering_finished2 semaphore %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_SEMAPHORE_EXT );
#endif
		SET_OBJECT_NAME(VkFence(vk_inst.tess[i].rendering_finished_fence), va("rendering_finished fence %i", i), VK_DEBUG_REPORT_OBJECT_TYPE_FENCE_EXT);
#endif
	}
	vk::FenceCreateInfo fence_desc{};

#ifdef USE_UPLOAD_QUEUE
	VK_CHECK_ASSIGN(vk_inst.aux_fence, vk_inst.device.createFence(fence_desc));
	vk_inst.rendering_finished = vk::Semaphore{};
	vk_inst.image_uploaded = vk::Semaphore{};
	vk_inst.aux_fence_wait = false;
#endif

#ifdef USE_VK_VALIDATION
	SET_OBJECT_NAME(VkFence(vk_inst.aux_fence), "aux fence", VK_DEBUG_REPORT_OBJECT_TYPE_FENCE_EXT);
#endif
}

static void vk_destroy_sync_primitives(void)
{
	uint32_t i;

#ifdef USE_UPLOAD_QUEUE
	vk_inst.device.destroySemaphore(vk_inst.image_uploaded2);
#endif

	for (i = 0; i < NUM_COMMAND_BUFFERS; i++)
	{
		vk_inst.device.destroySemaphore(vk_inst.tess[i].image_acquired);
#ifdef USE_UPLOAD_QUEUE
		vk_inst.device.destroySemaphore(vk_inst.tess[i].rendering_finished2);
#endif
		vk_inst.device.destroyFence(vk_inst.tess[i].rendering_finished_fence);
		vk_inst.tess[i].waitForFence = false;
		vk_inst.tess[i].swapchain_image_acquired = false;
	}

#ifdef USE_UPLOAD_QUEUE
	vk_inst.device.destroyFence(vk_inst.aux_fence);
	vk_inst.rendering_finished = nullptr;
	vk_inst.image_uploaded = nullptr;
#endif
}

static void vk_destroy_framebuffers(void)
{
	uint32_t n;

	for (n = 0; n < vk_inst.swapchain_image_count; n++)
	{
		if (vk_inst.framebuffers.main[n])
		{
			if (!vk_inst.fboActive || n == 0)
			{
				vk_inst.device.destroyFramebuffer(vk_inst.framebuffers.main[n]);
			}
			vk_inst.framebuffers.main[n] = nullptr;
		}
		if (vk_inst.framebuffers.gamma[n])
		{
			vk_inst.device.destroyFramebuffer(vk_inst.framebuffers.gamma[n]);
			vk_inst.framebuffers.gamma[n] = nullptr;
		}
	}

	if (vk_inst.framebuffers.bloom_extract)
	{
		vk_inst.device.destroyFramebuffer(vk_inst.framebuffers.bloom_extract);
		vk_inst.framebuffers.bloom_extract = nullptr;
	}

	if (vk_inst.framebuffers.screenmap)
	{
		vk_inst.device.destroyFramebuffer(vk_inst.framebuffers.screenmap);
		vk_inst.framebuffers.screenmap = nullptr;
	}

	if (vk_inst.framebuffers.capture)
	{
		vk_inst.device.destroyFramebuffer(vk_inst.framebuffers.capture);
		vk_inst.framebuffers.capture = nullptr;
	}

	for (n = 0; n < arrayLen(vk_inst.framebuffers.blur); n++)
	{
		if (vk_inst.framebuffers.blur[n])
		{
			vk_inst.device.destroyFramebuffer(vk_inst.framebuffers.blur[n]);
			vk_inst.framebuffers.blur[n] = nullptr;
		}
	}
}

static void vk_destroy_swapchain(void)
{
	uint32_t i;

	for (i = 0; i < vk_inst.swapchain_image_count; i++)
	{
		if (vk_inst.swapchain_image_views[i])
		{
			vk_inst.device.destroyImageView(vk_inst.swapchain_image_views[i]);
			vk_inst.swapchain_image_views[i] = nullptr;
		}
		if ( vk_inst.swapchain_rendering_finished[i] != vk::Semaphore() ) {
			vk_inst.device.destroySemaphore(vk_inst.swapchain_rendering_finished[i]);
			vk_inst.swapchain_rendering_finished[i] = nullptr;
		}
	}

	vk_inst.device.destroySwapchainKHR(vk_inst.swapchain);
}

static void vk_destroy_attachments(void);
static void vk_destroy_render_passes(void);
static void vk_destroy_pipelines(bool resetCount);

static void vk_restart_swapchain(const char *funcname, vk::Result res)
{
	uint32_t i;
#ifdef _DEBUG
	ri.Printf( PRINT_WARNING, "%s(%s): restarting swapchain...\n", funcname, vk::to_string( res ) );
#else
	ri.Printf(PRINT_WARNING, "%s(): restarting swapchain...\n", funcname );
#endif

	vk_wait_idle();

	for (i = 0; i < NUM_COMMAND_BUFFERS; i++)
	{
		vk_inst.tess[i].command_buffer.reset();
	}

#ifdef USE_UPLOAD_QUEUE
		 vk_inst.staging_command_buffer.reset();
#endif

	vk_destroy_pipelines(false);
	vk_destroy_framebuffers();
	vk_destroy_render_passes();
	vk_destroy_attachments();
	vk_destroy_swapchain();
	vk_destroy_sync_primitives();

	vk_select_surface_format(vk_inst.physical_device, vk_surface);
	setup_surface_formats(vk_inst.physical_device);

	vk_create_sync_primitives();

	vk_create_swapchain(vk_inst.physical_device, vk_inst.device, vk_surface, vk_inst.present_format, &vk_inst.swapchain, false);
	vk_create_attachments();
	vk_create_render_passes();
	vk_create_framebuffers();

	vk_update_attachment_descriptors();

	vk_update_post_process_pipelines();
}

static void vk_set_render_scale(void)
{
	if (gls.windowWidth != static_cast<uint32_t>(glConfig.vidWidth) || gls.windowHeight != static_cast<uint32_t>(glConfig.vidHeight))
	{
		if (r_renderScale->integer > 0)
		{
			int scaleMode = r_renderScale->integer - 1;
			if (scaleMode & 1)
			{
				// preserve aspect ratio (black bars on sides)
				float windowAspect = (float)gls.windowWidth / (float)gls.windowHeight;
				float renderAspect = (float)glConfig.vidWidth / (float)glConfig.vidHeight;
				if (windowAspect >= renderAspect)
				{
					float scale = (float)gls.windowHeight / (float)glConfig.vidHeight;
					int bias = (gls.windowWidth - scale * (float)glConfig.vidWidth) / 2;
					vk_inst.blitX0 += bias;
				}
				else
				{
					float scale = (float)gls.windowWidth / (float)glConfig.vidWidth;
					int bias = (gls.windowHeight - scale * (float)glConfig.vidHeight) / 2;
					vk_inst.blitY0 += bias;
				}
			}
			// linear filtering
			if (scaleMode & 2)
				vk_inst.blitFilter = GL_LINEAR;
			else
				vk_inst.blitFilter = GL_NEAREST;
		}

		vk_inst.windowAdjusted = true;
	}

	if (r_fbo->integer && r_ext_supersample->integer && !r_renderScale->integer)
	{
		vk_inst.blitFilter = GL_LINEAR;
	}
}

static constexpr int sampleCountFlagsToInt(vk::SampleCountFlags sampleCountFlags)
{
	if (sampleCountFlags & vk::SampleCountFlagBits::e1)
		return 1;
	if (sampleCountFlags & vk::SampleCountFlagBits::e2)
		return 2;
	if (sampleCountFlags & vk::SampleCountFlagBits::e4)
		return 4;
	if (sampleCountFlags & vk::SampleCountFlagBits::e8)
		return 8;
	if (sampleCountFlags & vk::SampleCountFlagBits::e16)
		return 16;
	if (sampleCountFlags & vk::SampleCountFlagBits::e32)
		return 32;
	if (sampleCountFlags & vk::SampleCountFlagBits::e64)
		return 64;
	return 0; // Default case for unsupported or undefined sample counts
}

void vk_initialize(void)
{
	char buf[64], driver_version[64];
	const char *vendor_name;
	vk::PhysicalDeviceProperties props;
	uint32_t major;
	uint32_t minor;
	uint32_t patch;
	uint32_t maxSize;
	uint32_t i;

	init_vulkan_library();

	vk_inst.queue = vk_inst.device.getQueue(vk_inst.queue_family_index, 0);

	props = vk_inst.physical_device.getProperties();

	vk_inst.cmd = vk_inst.tess + 0;
	vk_inst.uniform_alignment = props.limits.minUniformBufferOffsetAlignment;
	vk_inst.uniform_item_size = PAD(sizeof(vkUniform_t), vk_inst.uniform_alignment);

	// for flare visibility tests
	vk_inst.storage_alignment = MAX(props.limits.minStorageBufferOffsetAlignment, sizeof(uint32_t));

	vk_inst.maxAnisotropy = props.limits.maxSamplerAnisotropy;

	vk_inst.blitFilter = GL_NEAREST;
	vk_inst.windowAdjusted = false;
	vk_inst.blitX0 = vk_inst.blitY0 = 0;

	vk_set_render_scale();

	if (r_fbo->integer)
	{
		vk_inst.fboActive = true;
		if (r_ext_multisample->integer)
		{
			vk_inst.msaaActive = true;
		}
	}
	else
	{
		vk_inst.fboActive = false;
	}

	// multisampling

	vkMaxSamples = convertToVkSampleCountFlagBits(MIN(sampleCountFlagsToInt(props.limits.sampledImageColorSampleCounts),
													  sampleCountFlagsToInt(props.limits.sampledImageDepthSampleCounts)));

	if (/*vk_inst.fboActive &&*/ vk_inst.msaaActive)
	{
		vk::SampleCountFlags mask = vkMaxSamples;
		vkSamples = convertToVkSampleCountFlagBits(MAX(static_cast<int>(log2pad_plus(r_ext_multisample->integer, 1)), sampleCountFlagsToInt(vk::SampleCountFlagBits::e2)));
		while (vkSamples > mask)
		{
			int shiftAmount = 1;
			vkSamples = static_cast<vk::SampleCountFlagBits>(sampleCountFlagsToInt(vkSamples) >> shiftAmount);
		}
		ri.Printf(PRINT_ALL, "...using %ix MSAA\n", sampleCountFlagsToInt(vkSamples));
	}
	else
	{
		vkSamples = vk::SampleCountFlagBits::e1;
	}

	vk_inst.screenMapSamples = vk::SampleCountFlagBits(MIN((int)vkMaxSamples, (int)vk::SampleCountFlagBits::e4));

	vk_inst.screenMapWidth = (float)glConfig.vidWidth / 16.0;
	if (vk_inst.screenMapWidth < 4)
		vk_inst.screenMapWidth = 4;

	vk_inst.screenMapHeight = static_cast<uint32_t>(glConfig.vidHeight) / 16.0;
	if (vk_inst.screenMapHeight < 4)
		vk_inst.screenMapHeight = 4;

	vk_inst.defaults.geometry_size = VERTEX_BUFFER_SIZE;
	vk_inst.defaults.staging_size = STAGING_BUFFER_SIZE;

	// get memory size & defaults
	{
		const vk::PhysicalDeviceMemoryProperties props =
			vk_inst.physical_device.getMemoryProperties();

		vk::DeviceSize maxDedicatedSize = 0; // largest device-local heap size
		vk::DeviceSize maxBARSize       = 0; // largest host-coherent + device-local heap size

		for (uint32_t i = 0; i < props.memoryTypeCount; ++i)
		{
			const auto& type = props.memoryTypes[i];
			const auto& heap = props.memoryHeaps[type.heapIndex];

			// Device-local memory type?
			const bool typeIsDeviceLocal =
				static_cast<bool>(type.propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal);

			// (Optional) confirm the heap is marked device-local too
			const bool heapIsDeviceLocal =
				static_cast<bool>(heap.flags & vk::MemoryHeapFlagBits::eDeviceLocal);

			if (typeIsDeviceLocal && heapIsDeviceLocal) {
				if (heap.size > maxDedicatedSize) {
					maxDedicatedSize = heap.size;
				}
			}

			// BAR-like memory: host-coherent + device-local on the same type/heap
			const auto required = vk::MemoryPropertyFlagBits::eHostCoherent
								| vk::MemoryPropertyFlagBits::eDeviceLocal;

			const bool hasBARProps =
				(type.propertyFlags & required) == required;

			if (hasBARProps) {
				if (heap.size > maxBARSize) {
					maxBARSize = heap.size;
				}
			}
		}

		auto toMB = [](vk::DeviceSize bytes) -> int {
			constexpr vk::DeviceSize MB = 1024ull * 1024ull;
			return static_cast<int>((bytes + (MB - 1)) / MB);
		};

		if (maxDedicatedSize != 0) {
			ri.Printf(PRINT_ALL, "...device memory size: %iMB\n", toMB(maxDedicatedSize));
		}

		if (maxBARSize != 0) {
			if (maxBARSize >= 128ull * 1024ull * 1024ull) {
				// use larger buffers to avoid potential reallocations
				vk_inst.defaults.geometry_size = VERTEX_BUFFER_SIZE_HI;
				vk_inst.defaults.staging_size  = STAGING_BUFFER_SIZE_HI;
			}
		#ifdef _DEBUG
			ri.Printf(PRINT_ALL, "...BAR memory size: %iMB\n", toMB(maxBARSize));
		#endif
		}
	}


	// fill glConfig information

	// maxTextureSize must not exceed IMAGE_CHUNK_SIZE
	maxSize = sqrtf(IMAGE_CHUNK_SIZE / 4);
	// round down to next power of 2
	glConfig.maxTextureSize = MIN(props.limits.maxImageDimension2D, log2pad_plus(maxSize, 0));

	if (glConfig.maxTextureSize > MAX_TEXTURE_SIZE)
		glConfig.maxTextureSize = MAX_TEXTURE_SIZE; // ResampleTexture() relies on that maximum

	// default chunk size, may be doubled on demand
	vk_inst.image_chunk_size = IMAGE_CHUNK_SIZE;

	vk_inst.maxLod = 1 + Q_log2(glConfig.maxTextureSize);

	if (props.limits.maxPerStageDescriptorSamplers != 0xFFFFFFFF)
		glConfig.numTextureUnits = props.limits.maxPerStageDescriptorSamplers;
	else
		glConfig.numTextureUnits = props.limits.maxBoundDescriptorSets;
	if (glConfig.numTextureUnits > MAX_TEXTURE_UNITS)
		glConfig.numTextureUnits = MAX_TEXTURE_UNITS;

	vk_inst.maxBoundDescriptorSets = props.limits.maxBoundDescriptorSets;

	if ( r_ext_texture_env_add->integer != 0 )
		glConfig.textureEnvAddAvailable = true;
	else
		glConfig.textureEnvAddAvailable = false;

	glConfig.textureCompression = TC_NONE;

	major = VK_VERSION_MAJOR(props.apiVersion);
	minor = VK_VERSION_MINOR(props.apiVersion);
	patch = VK_VERSION_PATCH(props.apiVersion);

	// decode driver version
	switch (props.vendorID)
	{
	case 0x10DE: // NVidia
		Com_sprintf(driver_version, sizeof(driver_version), "%i.%i.%i.%i",
					(props.driverVersion >> 22) & 0x3FF,
					(props.driverVersion >> 14) & 0x0FF,
					(props.driverVersion >> 6) & 0x0FF,
					(props.driverVersion >> 0) & 0x03F);
		break;
#ifdef _WIN32
	case 0x8086: // Intel
		Com_sprintf(driver_version, sizeof(driver_version), "%i.%i",
					(props.driverVersion >> 14),
					(props.driverVersion >> 0) & 0x3FFF);
		break;
#endif
	default:
		Com_sprintf(driver_version, sizeof(driver_version), "%i.%i.%i",
					(props.driverVersion >> 22),
					(props.driverVersion >> 12) & 0x3FF,
					(props.driverVersion >> 0) & 0xFFF);
	}

	Com_sprintf(glConfig.version_string, sizeof(glConfig.version_string), "API: %i.%i.%i, Driver: %s",
				major, minor, patch, driver_version);

	vk_inst.offscreenRender = true;

	if (props.vendorID == 0x1002)
	{
		vendor_name = "Advanced Micro Devices, Inc.";
	}
	else if (props.vendorID == 0x106B)
	{
		vendor_name = "Apple Inc.";
	}
	else if (props.vendorID == 0x10DE)
	{
		// https://github.com/SaschaWillems/Vulkan/issues/493
		// we can't render to offscreen presentation surfaces on nvidia
		vk_inst.offscreenRender = false;
		vendor_name = "NVIDIA";
	}
	else if (props.vendorID == 0x14E4)
	{
		vendor_name = "Broadcom Inc.";
	}
	else if (props.vendorID == 0x1AE0)
	{
		vendor_name = "Google Inc.";
	}
	else if (props.vendorID == 0x8086)
	{
		vendor_name = "Intel Corporation";
	}
	else if (props.vendorID == VK_VENDOR_ID_MESA)
	{
		vendor_name = "MESA";
	}
	else
	{
		Com_sprintf(buf, sizeof(buf), "VendorID: %04x", props.vendorID);
		vendor_name = buf;
	}

	Q_strncpyz(glConfig.vendor_string, vendor_name, sizeof(glConfig.vendor_string));
	Q_strncpyz(glConfig.renderer_string, renderer_name(props), sizeof(glConfig.renderer_string));
#ifdef USE_VK_VALIDATION
	SET_OBJECT_NAME((intptr_t) static_cast<VkDevice>(vk_inst.device), glConfig.renderer_string, VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_EXT);
#endif

	// do early texture mode setup to avoid redundant descriptor updates in GL_SetDefaultState()
	vk_inst.samplers.filter_min = -1;
	vk_inst.samplers.filter_max = -1;
	TextureMode( r_textureMode->string );
	r_textureMode->modified = false;

	//
	// Sync primitives.
	//
	vk_create_sync_primitives();

	//
	// Command pool.
	//
	{
		vk::CommandPoolCreateInfo desc{vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer, vk_inst.queue_family_index};
		VK_CHECK_ASSIGN(vk_inst.command_pool, vk_inst.device.createCommandPool(desc));
#ifdef USE_VK_VALIDATION
		SET_OBJECT_NAME(VkCommandPool(vk_inst.command_pool), "command pool", VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_POOL_EXT);
#endif
	}

#ifdef USE_UPLOAD_QUEUE
{
    vk::CommandBufferAllocateInfo allocInfo{vk_inst.command_pool,vk::CommandBufferLevel::ePrimary,1};
	VK_CHECK(vk_inst.device.allocateCommandBuffers(&allocInfo, &vk_inst.staging_command_buffer));
}
#endif

	//
	// Command buffers and color attachments.
	//
	vk::CommandBufferAllocateInfo alloc_info{
		vk_inst.command_pool,			  // Command pool to allocate from
		vk::CommandBufferLevel::ePrimary, // Level of the command buffers
		NUM_COMMAND_BUFFERS				  // Number of command buffers to allocate
	};

	// Allocate all command buffers at once
	std::array<vk::CommandBuffer, NUM_COMMAND_BUFFERS> command_buffers;
	VK_CHECK(vk_inst.device.allocateCommandBuffers(&alloc_info, command_buffers.data()));

	// Assign the command buffers to the array
	for (i = 0; i < NUM_COMMAND_BUFFERS; i++)
	{
		vk_inst.tess[i].command_buffer = command_buffers[i];

		// SET_OBJECT_NAME( vk_inst.tess[i].command_buffer, va( "command buffer %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_BUFFER_EXT );
	}

	//
	// Descriptor pool.
	//
	{
		// Use std::array for pool sizes
		std::array<vk::DescriptorPoolSize, 3> pool_sizes = {
			vk::DescriptorPoolSize{
				vk::DescriptorType::eCombinedImageSampler,
				MAX_DRAWIMAGES + 1 + 1 + 1 + VK_NUM_BLOOM_PASSES * 2 // color, screenmap, bloom descriptors
			},
			vk::DescriptorPoolSize{
				vk::DescriptorType::eUniformBufferDynamic,
				NUM_COMMAND_BUFFERS},
			vk::DescriptorPoolSize{
				vk::DescriptorType::eStorageBufferDynamic,
				1}};

		// Calculate maxSets by summing the descriptor counts
		uint32_t maxSets = std::accumulate(pool_sizes.begin(), pool_sizes.end(), 0,
										   [](uint32_t sum, const vk::DescriptorPoolSize &poolSize)
										   {
											   return sum + poolSize.descriptorCount;
										   });

		// Create the DescriptorPoolCreateInfo struct
		vk::DescriptorPoolCreateInfo desc{
			vk::DescriptorPoolCreateFlags{},		  // flags
			maxSets,								  // maxSets
			static_cast<uint32_t>(pool_sizes.size()), // poolSizeCount
			pool_sizes.data()						  // pPoolSizes
		};

		// Create the descriptor pool
		VK_CHECK_ASSIGN(vk_inst.descriptor_pool, vk_inst.device.createDescriptorPool(desc));

		// Optional: If you need error handling
		// if (!vk_inst.descriptor_pool) {
		//     throw std::runtime_error("Failed to create descriptor pool!");
		// }
	}

	//
	// Descriptor set layout.
	//
	vk_inst.set_layout_sampler = vk_create_layout_binding(0, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment);
	vk_inst.set_layout_uniform = vk_create_layout_binding(0, vk::DescriptorType::eUniformBufferDynamic, vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex);
	vk_inst.set_layout_storage = vk_create_layout_binding(0, vk::DescriptorType::eStorageBufferDynamic, vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eVertex);
	// vk_create_layout_binding( 0, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, vk::ShaderStageFlagBits::eFragment, &vk_inst.set_layout_input );

	//
	// Pipeline layouts.
	//
	{
		std::array<vk::DescriptorSetLayout, 6> set_layouts = {
			vk_inst.set_layout_uniform, // fog/dlight parameters
			vk_inst.set_layout_sampler, // diffuse
			vk_inst.set_layout_sampler, // lightmap / fog-only
			vk_inst.set_layout_sampler, // blend
			vk_inst.set_layout_sampler	// collapsed fog texture
		};

		// Push constant range
		vk::PushConstantRange push_range{
			vk::ShaderStageFlagBits::eVertex, // stageFlags
			0,								  // offset
			64								  // size (16 floats)
		};

		// Pipeline layout creation info for standard pipelines
		vk::PipelineLayoutCreateInfo desc{
			vk::PipelineLayoutCreateFlags{},															  // flags
			(vk_inst.maxBoundDescriptorSets >= VK_DESC_COUNT) ? static_cast<uint32_t>(VK_DESC_COUNT) : 4, // setLayoutCount
			set_layouts.data(),																			  // pSetLayouts
			1,																							  // pushConstantRangeCount
			&push_range																					  // pPushConstantRanges
		};

		// Create the pipeline layout
		VK_CHECK_ASSIGN(vk_inst.pipeline_layout, vk_inst.device.createPipelineLayout(desc));

		// flare test pipeline
		set_layouts[0] = vk_inst.set_layout_storage; // dynamic storage buffer

		desc.setLayoutCount = 1;
		VK_CHECK_ASSIGN(vk_inst.pipeline_layout_storage, vk_inst.device.createPipelineLayout(desc));


		//	VK_CHECK( qvkCreatePipelineLayout( vk_inst.device, &desc, nullptr, &vk_inst.pipeline_layout_storage ) );

		// post-processing pipeline
		set_layouts = {
			vk_inst.set_layout_sampler, // sampler
			vk_inst.set_layout_sampler, // sampler
			vk_inst.set_layout_sampler, // sampler
			vk_inst.set_layout_sampler	// sampler
		};

		desc.setLayoutCount = 1;
		desc.pushConstantRangeCount = 0;
		desc.pPushConstantRanges = nullptr;
		VK_CHECK_ASSIGN(vk_inst.pipeline_layout_post_process, vk_inst.device.createPipelineLayout(desc));

		desc.setLayoutCount = VK_NUM_BLOOM_PASSES;

		VK_CHECK_ASSIGN(vk_inst.pipeline_layout_blend, vk_inst.device.createPipelineLayout(desc));
#ifdef USE_VK_VALIDATION
		SET_OBJECT_NAME(VkPipelineLayout(vk_inst.pipeline_layout), "pipeline layout - main", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT);
		SET_OBJECT_NAME(VkPipelineLayout(vk_inst.pipeline_layout_post_process), "pipeline layout - post-processing", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT);
		SET_OBJECT_NAME(VkPipelineLayout(vk_inst.pipeline_layout_blend), "pipeline layout - blend", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT);
#endif
	}

	vk_inst.geometry_buffer_size_new = VERTEX_BUFFER_SIZE;
	vk_create_geometry_buffers(vk_inst.geometry_buffer_size_new);
	vk_inst.geometry_buffer_size_new = 0;

	vk_create_storage_buffer(MAX_FLARES * vk_inst.storage_alignment);

	vk_create_shader_modules();

	{
		vk::PipelineCacheCreateInfo ci{};
		// ci.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
		VK_CHECK_ASSIGN(vk_inst.pipelineCache, vk_inst.device.createPipelineCache(ci));
	}

	vk_inst.renderPassIndex = renderPass_t::RENDER_PASS_MAIN; // default render pass

	// swapchain
	vk_inst.initSwapchainLayout = vk::ImageLayout::ePresentSrcKHR;
	// vk_inst.initSwapchainLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	vk_create_swapchain(vk_inst.physical_device, vk_inst.device, vk_surface, vk_inst.present_format,
						&vk_inst.swapchain, true);

	// color/depth attachments
	vk_create_attachments();

	// renderpasses
	vk_create_render_passes();

	// framebuffers for each swapchain image
	vk_create_framebuffers();

	// preallocate staging buffer?
	// preallocate staging buffer
	if ( vk_inst.defaults.staging_size == STAGING_BUFFER_SIZE_HI ) {
		vk_alloc_staging_buffer( vk_inst.defaults.staging_size );
	}


	vk_inst.active = true;
}

void vk_create_pipelines(void)
{
	vk_alloc_persistent_pipelines();

	vk_inst.pipelines_world_base = vk_inst.pipelines_count;
}

static void vk_destroy_attachments(void)
{
	uint32_t i;

	if (vk_inst.bloom_image[0])
	{
		for (i = 0; i < arrayLen(vk_inst.bloom_image); i++)
		{
			vk_inst.device.destroyImage(vk_inst.bloom_image[i]);
			vk_inst.device.destroyImageView(vk_inst.bloom_image_view[i]);
			vk_inst.bloom_image[i] = nullptr;
			vk_inst.bloom_image_view[i] = nullptr;
		}
	}

	if (vk_inst.color_image)
	{
		vk_inst.device.destroyImage(vk_inst.color_image);
		vk_inst.device.destroyImageView(vk_inst.color_image_view);
		vk_inst.color_image = nullptr;
		vk_inst.color_image_view = nullptr;
	}

	if (vk_inst.msaa_image)
	{
		vk_inst.device.destroyImage(vk_inst.msaa_image);
		vk_inst.device.destroyImageView(vk_inst.msaa_image_view);
		vk_inst.msaa_image = nullptr;
		vk_inst.msaa_image_view = nullptr;
	}

	vk_inst.device.destroyImage(vk_inst.depth_image);
	vk_inst.device.destroyImageView(vk_inst.depth_image_view);
	vk_inst.depth_image = nullptr;
	vk_inst.depth_image_view = nullptr;

	if (vk_inst.screenMap.color_image)
	{
		vk_inst.device.destroyImage(vk_inst.screenMap.color_image);
		vk_inst.device.destroyImageView(vk_inst.screenMap.color_image_view);
		vk_inst.screenMap.color_image = nullptr;
		vk_inst.screenMap.color_image_view = nullptr;
	}

	if (vk_inst.screenMap.color_image_msaa)
	{
		vk_inst.device.destroyImage(vk_inst.screenMap.color_image_msaa);
		vk_inst.device.destroyImageView(vk_inst.screenMap.color_image_view_msaa);
		vk_inst.screenMap.color_image_msaa = nullptr;
		vk_inst.screenMap.color_image_view_msaa = nullptr;
	}

	if (vk_inst.screenMap.depth_image)
	{
		vk_inst.device.destroyImage(vk_inst.screenMap.depth_image);
		vk_inst.device.destroyImageView(vk_inst.screenMap.depth_image_view);
		vk_inst.screenMap.depth_image = nullptr;
		vk_inst.screenMap.depth_image_view = nullptr;
	}

	if (vk_inst.capture.image)
	{
		vk_inst.device.destroyImage(vk_inst.capture.image);
		vk_inst.device.destroyImageView(vk_inst.capture.image_view);
		vk_inst.capture.image = nullptr;
		vk_inst.capture.image_view = nullptr;
	}

	for (i = 0; i < vk_inst.image_memory_count; i++)
	{
		vk_inst.device.freeMemory(vk_inst.image_memory[i]);
	}

	vk_inst.image_memory_count = 0;
}

static void vk_destroy_render_passes()
{
	// Destroy the main render pass
	if (vk_inst.render_pass.main)
	{
		vk_inst.device.destroyRenderPass(vk_inst.render_pass.main);
		vk_inst.render_pass.main = nullptr;
	}

	// Destroy the bloom extract render pass
	if (vk_inst.render_pass.bloom_extract)
	{
		vk_inst.device.destroyRenderPass(vk_inst.render_pass.bloom_extract);
		vk_inst.render_pass.bloom_extract = nullptr;
	}

	for (std::size_t i = 0; i < arrayLen(vk_inst.render_pass.blur); i++)
	{
		if (vk_inst.render_pass.blur[i])
		{
			vk_inst.device.destroyRenderPass(vk_inst.render_pass.blur[i]);
			vk_inst.render_pass.blur[i] = nullptr;
		}
	}

	// Destroy the post-bloom render pass
	if (vk_inst.render_pass.post_bloom)
	{
		vk_inst.device.destroyRenderPass(vk_inst.render_pass.post_bloom);
		vk_inst.render_pass.post_bloom = nullptr;
	}

	// Destroy the screenmap render pass
	if (vk_inst.render_pass.screenmap)
	{
		vk_inst.device.destroyRenderPass(vk_inst.render_pass.screenmap);
		vk_inst.render_pass.screenmap = nullptr;
	}

	// Destroy the gamma render pass
	if (vk_inst.render_pass.gamma)
	{
		vk_inst.device.destroyRenderPass(vk_inst.render_pass.gamma);
		vk_inst.render_pass.gamma = nullptr;
	}

	// Destroy the capture render pass
	if (vk_inst.render_pass.capture)
	{
		vk_inst.device.destroyRenderPass(vk_inst.render_pass.capture);
		vk_inst.render_pass.capture = nullptr;
	}
}

static void vk_destroy_pipelines(bool resetCounter)
{
	uint32_t i, j;

	// Destroy all pipelines in the vk_inst.pipelines array
	for (i = 0; i < vk_inst.pipelines_count; ++i)
	{
		for (j = 0; j < RENDER_PASS_COUNT; ++j)
		{
			if (vk_inst.pipelines[i].handle[j])
			{
				vk_inst.device.destroyPipeline(vk_inst.pipelines[i].handle[j]);
				vk_inst.pipelines[i].handle[j] = nullptr;
				--vk_inst.pipeline_create_count;
			}
		}
	}

	// Reset the pipeline count and clear the memory if requested
	if (resetCounter)
	{
		// std::memset(&vk_inst.pipelines, 0, sizeof(vk_inst.pipelines));
		std::fill(std::begin(vk_inst.pipelines), std::end(vk_inst.pipelines), VK_Pipeline_t{});
		vk_inst.pipelines_count = 0;
	}

	// Destroy the gamma pipeline if it exists
	if (vk_inst.gamma_pipeline)
	{
		vk_inst.device.destroyPipeline(vk_inst.gamma_pipeline);
		vk_inst.gamma_pipeline = nullptr;
	}

	// Destroy the capture pipeline if it exists
	if (vk_inst.capture_pipeline)
	{
		vk_inst.device.destroyPipeline(vk_inst.capture_pipeline);
		vk_inst.capture_pipeline = nullptr;
	}

	// Destroy the bloom extract pipeline if it exists
	if (vk_inst.bloom_extract_pipeline)
	{
		vk_inst.device.destroyPipeline(vk_inst.bloom_extract_pipeline);
		vk_inst.bloom_extract_pipeline = nullptr;
	}

	// Destroy the bloom blend pipeline if it exists
	if (vk_inst.bloom_blend_pipeline)
	{
		vk_inst.device.destroyPipeline(vk_inst.bloom_blend_pipeline);
		vk_inst.bloom_blend_pipeline = nullptr;
	}

	// Destroy the blur pipelines in the vk_inst.blur_pipeline array
	for (i = 0; i < std::size(vk_inst.blur_pipeline); ++i)
	{
		if (vk_inst.blur_pipeline[i])
		{
			vk_inst.device.destroyPipeline(vk_inst.blur_pipeline[i]);
			vk_inst.blur_pipeline[i] = nullptr;
		}
	}
}

template <class T>
inline void reset_to_default(T& x) noexcept { x = T{}; }

static void reset_vk_instance(Vk_Instance& s) noexcept
{
    // Core device & swapchain
	reset_to_default(s.physical_device);
	reset_to_default(s.base_format);
	reset_to_default(s.present_format);
    s.queue_family_index = 0;
	reset_to_default(s.device);
	reset_to_default(s.queue);

	reset_to_default(s.swapchain);
    s.swapchain_image_count = 0;
    std::fill(std::begin(s.swapchain_images),        std::end(s.swapchain_images),        vk::Image{});
    std::fill(std::begin(s.swapchain_image_views),   std::end(s.swapchain_image_views),   vk::ImageView{});
    std::fill(std::begin(s.swapchain_rendering_finished), std::end(s.swapchain_rendering_finished), vk::Semaphore{});

    // Pools / staging
	reset_to_default(s.command_pool);
#ifdef USE_UPLOAD_QUEUE
	reset_to_default(s.staging_command_buffer);
#endif

    // Image memory pool
    std::fill(std::begin(s.image_memory), std::end(s.image_memory), vk::DeviceMemory{});
    s.image_memory_count = 0;

    // Render passes
	reset_to_default(s.render_pass.main);
	reset_to_default(s.render_pass.screenmap);
	reset_to_default(s.render_pass.gamma);
	reset_to_default(s.render_pass.capture);
	reset_to_default(s.render_pass.bloom_extract);
	for (auto& rp : s.render_pass.blur) reset_to_default(rp);
	reset_to_default(s.render_pass.post_bloom);

    // Descriptor infra
	reset_to_default(s.descriptor_pool);
	reset_to_default(s.set_layout_sampler);
	reset_to_default(s.set_layout_uniform);
	reset_to_default(s.set_layout_storage);

    // Pipeline layouts
    reset_to_default(s.pipeline_layout);
    reset_to_default(s.pipeline_layout_storage);
    reset_to_default(s.pipeline_layout_post_process);
    reset_to_default(s.pipeline_layout_blend);

    // Global descriptors & images
    reset_to_default(s.color_descriptor);

    reset_to_default(s.color_image);
	reset_to_default(s.color_image_view);
    std::fill(std::begin(s.bloom_image),      std::end(s.bloom_image),      vk::Image{});
    std::fill(std::begin(s.bloom_image_view), std::end(s.bloom_image_view), vk::ImageView{});
    s.bloom_image_descriptor.fill({});  // std::array

    reset_to_default(s.depth_image);
	reset_to_default(s.depth_image_view);
    reset_to_default(s.msaa_image);
	reset_to_default(s.msaa_image_view);

    // screenMap
    reset_to_default(s.screenMap.color_descriptor);
    reset_to_default(s.screenMap.color_image);
    reset_to_default(s.screenMap.color_image_view);
    reset_to_default(s.screenMap.color_image_msaa);
    reset_to_default(s.screenMap.color_image_view_msaa);
    reset_to_default(s.screenMap.depth_image);
    reset_to_default(s.screenMap.depth_image_view);

    // capture
    reset_to_default(s.capture.image);
    reset_to_default(s.capture.image_view);

    // Framebuffers
    for (auto& fb : s.framebuffers.blur)  reset_to_default(fb);
    reset_to_default(s.framebuffers.bloom_extract);
    for (auto& fb : s.framebuffers.main)  reset_to_default(fb);
    for (auto& fb : s.framebuffers.gamma) reset_to_default(fb);
    reset_to_default(s.framebuffers.screenmap);
    reset_to_default(s.framebuffers.capture  );

#ifdef USE_UPLOAD_QUEUE
    reset_to_default(s.rendering_finished);
    reset_to_default(s.image_uploaded2   );
    reset_to_default(s.image_uploaded    );
#endif

    // Per-frame cmd/tess
    for (auto& t : s.tess) t = vk_tess_t{};  // default each slot
    s.cmd = nullptr;
    s.cmd_index = 0;

    // Storage descriptor
    reset_to_default(s.storage.buffer     );
    s.storage.buffer_ptr  = nullptr;
    reset_to_default(s.storage.memory     );
    reset_to_default(s.storage.descriptor );

    // Alignments
    s.uniform_item_size   = 0;
    s.uniform_alignment   = 0;
    s.storage_alignment   = 0;

    // VBO
    reset_to_default(s.vbo.vertex_buffer);
    reset_to_default(s.vbo.buffer_memory);

    // Geometry pool
    reset_to_default(s.geometry_buffer_memory);
    s.geometry_buffer_size     = {};
    s.geometry_buffer_size_new = {};

    // Stats
    s.stats.vertex_buffer_max = {};
    s.stats.push_size         = 0;
    s.stats.push_size_max     = 0;

    // Shader modules
    for (auto& a : s.modules.vert.gen)     for (auto& b : a) for (auto& c : b) for (auto& d : c) reset_to_default(d);
    for (auto& a : s.modules.vert.ident1)  for (auto& b : a) for (auto& c : b) reset_to_default(c);
    for (auto& a : s.modules.vert.fixed)   for (auto& b : a) for (auto& c : b) reset_to_default(c);
    for (auto& m : s.modules.vert.light)   reset_to_default(m);

    reset_to_default(s.modules.frag.gen0_df);
    for (auto& a : s.modules.frag.gen)     for (auto& b : a) for (auto& c : b) reset_to_default(c);
    for (auto& a : s.modules.frag.ident1)  for (auto& b : a) reset_to_default(b);
    for (auto& a : s.modules.frag.fixed)   for (auto& b : a) reset_to_default(b);
    for (auto& a : s.modules.frag.ent)     for (auto& b : a) reset_to_default(b);
    for (auto& a : s.modules.frag.light)   for (auto& b : a) reset_to_default(b);

    reset_to_default(s.modules.color_fs);
    reset_to_default(s.modules.color_vs);
    reset_to_default(s.modules.bloom_fs);
    reset_to_default(s.modules.blur_fs );
    reset_to_default(s.modules.blend_fs);
    reset_to_default(s.modules.gamma_fs);
    reset_to_default(s.modules.gamma_vs);
    reset_to_default(s.modules.fog_fs  );
    reset_to_default(s.modules.fog_vs  );
    reset_to_default(s.modules.dot_fs  );
    reset_to_default(s.modules.dot_vs  );

    // Pipeline cache / pipelines
    reset_to_default(s.pipelineCache);
    for (auto& p : s.pipelines) p = VK_Pipeline_t{};  // reset defs/handles
    s.pipelines_count = 0;
    s.pipelines_world_base = 0;
    s.pipeline_create_count = 0;

    // Standard pipeline indices
    s.skybox_pipeline = 0;
    for (auto& a : s.shadow_volume_pipelines) for (auto& b : a) b = 0;
    s.shadow_finish_pipeline = 0;
    for (auto& a : s.fog_pipelines) for (auto& b : a) for (auto& c : b) c = 0;
#ifdef USE_LEGACY_DLIGHTS
    for (auto& a : s.dlight_pipelines) for (auto& b : a) for (auto& c : b) c = 0;
#endif
#ifdef USE_PMLIGHT
    for (auto& a : s.dlight_pipelines_x)  for (auto& b : a) for (auto& c : b) for (auto& d : c) d = 0;
    for (auto& a : s.dlight1_pipelines_x) for (auto& b : a) for (auto& c : b) for (auto& d : c) d = 0;
#endif
    s.tris_debug_pipeline              = 0;
    s.tris_mirror_debug_pipeline       = 0;
    s.tris_debug_green_pipeline        = 0;
    s.tris_mirror_debug_green_pipeline = 0;
    s.tris_debug_red_pipeline          = 0;
    s.tris_mirror_debug_red_pipeline   = 0;
    s.normals_debug_pipeline           = 0;
    s.surface_debug_pipeline_solid     = 0;
    s.surface_debug_pipeline_outline   = 0;
    s.images_debug_pipeline            = 0;
    s.images_debug_pipeline2           = 0;
    s.surface_beam_pipeline            = 0;
    s.surface_axis_pipeline            = 0;
    s.dot_pipeline                     = 0;

    // Runtime pipelines
    reset_to_default(s.gamma_pipeline         );
    reset_to_default(s.capture_pipeline       );
    reset_to_default(s.bloom_extract_pipeline );
    s.blur_pipeline.fill({});
    reset_to_default(s.bloom_blend_pipeline);

    // Misc state
    s.frame_count       = 0;
    s.active            = false;
    s.wideLines         = false;
    s.samplerAnisotropy = false;
    s.fragmentStores    = false;
    s.dedicatedAllocation = false;
    s.debugMarkers      = false;

    s.maxAnisotropy = 0.0f;
    s.maxLod        = 0.0f;

    s.color_format  = {};
    s.capture_format= {};
    s.depth_format  = {};
    s.bloom_format  = {};
    s.initSwapchainLayout = {};

    s.clearAttachment = false;
    s.fboActive       = false;
    s.blitEnabled     = false;
    s.msaaActive      = false;
    s.offscreenRender = false;

    s.windowAdjusted = false;
    s.blitX0 = 0;
    s.blitY0 = 0;
    s.blitFilter = 0;

    s.renderWidth  = 0;
    s.renderHeight = 0;
    s.renderScaleX = 0.0f;
    s.renderScaleY = 0.0f;

    s.renderPassIndex = renderPass_t{}; // typically RENDER_PASS_MAIN=0

    s.screenMapWidth  = 0;
    s.screenMapHeight = 0;
    s.screenMapSamples = {};
    s.image_chunk_size = 0;

    s.maxBoundDescriptorSets = 0;

#ifdef USE_UPLOAD_QUEUE
    reset_to_default(s.aux_fence);
    s.aux_fence_wait = false;
#endif

    // staging buffer wrapper
    reset_to_default(s.staging_buffer.handle);
    reset_to_default(s.staging_buffer.memory);
    s.staging_buffer.size   = {};
    s.staging_buffer.ptr    = nullptr;
#ifdef USE_UPLOAD_QUEUE
    s.staging_buffer.offset = {};
#endif

    // samplers wrapper
    s.samplers.count = 0;
    std::fill(std::begin(s.samplers.def),    std::end(s.samplers.def),    Vk_Sampler_Def{});
    std::fill(std::begin(s.samplers.handle), std::end(s.samplers.handle), vk::Sampler{});
    s.samplers.filter_min = 0;
    s.samplers.filter_max = 0;

    // defaults
    s.defaults.staging_size  = {};
    s.defaults.geometry_size = {};
}


void vk_shutdown(const refShutdownCode_t code)
{
	uint32_t i, j, k, l;

	if (!vk_inst.device)
	{
		// Not fully initialized
		goto __cleanup;
	}

	vk_destroy_framebuffers();
	vk_destroy_pipelines(true); // Reset counter
	vk_destroy_render_passes();
	vk_destroy_attachments();
	vk_destroy_swapchain();

	if (vk_inst.pipelineCache)
	{
		vk_inst.device.destroyPipelineCache(vk_inst.pipelineCache);
		vk_inst.pipelineCache = nullptr;
	}

	vk_inst.device.destroyCommandPool(vk_inst.command_pool);
	vk_inst.device.destroyDescriptorPool(vk_inst.descriptor_pool);

	vk_inst.device.destroyDescriptorSetLayout(vk_inst.set_layout_sampler);
	vk_inst.device.destroyDescriptorSetLayout(vk_inst.set_layout_uniform);
	vk_inst.device.destroyDescriptorSetLayout(vk_inst.set_layout_storage);

	vk_inst.device.destroyPipelineLayout(vk_inst.pipeline_layout);
	vk_inst.device.destroyPipelineLayout(vk_inst.pipeline_layout_storage);
	vk_inst.device.destroyPipelineLayout(vk_inst.pipeline_layout_post_process);
	vk_inst.device.destroyPipelineLayout(vk_inst.pipeline_layout_blend);

#ifdef USE_VBO
	vk_release_vbo();
#endif

	vk_clean_staging_buffer();

	vk_release_geometry_buffers();

	vk_destroy_samplers();

	vk_destroy_sync_primitives();

	vk_inst.device.destroyBuffer(vk_inst.storage.buffer);
	vk_inst.device.freeMemory(vk_inst.storage.memory);

	// Destroy shader modules with nested loops for vk_inst.modules.vert.gen
	for (i = 0; i < 3; ++i)
	{
		for (j = 0; j < 2; ++j)
		{
			for (k = 0; k < 2; ++k)
			{
				for (l = 0; l < 2; ++l)
				{
					if (vk_inst.modules.vert.gen[i][j][k][l])
					{
						vk_inst.device.destroyShaderModule(vk_inst.modules.vert.gen[i][j][k][l]);
						vk_inst.modules.vert.gen[i][j][k][l] = nullptr;
					}
				}
			}
		}
	}

	// Destroy shader modules with nested loops for vk_inst.modules.frag.gen
	for (i = 0; i < 3; ++i)
	{
		for (j = 0; j < 2; ++j)
		{
			for (k = 0; k < 2; ++k)
			{
				if (vk_inst.modules.frag.gen[i][j][k])
				{
					vk_inst.device.destroyShaderModule(vk_inst.modules.frag.gen[i][j][k]);
					vk_inst.modules.frag.gen[i][j][k] = nullptr;
				}
			}
		}
	}

	// Destroy shader modules for vk_inst.modules.vert.light and vk_inst.modules.frag.light
	for (i = 0; i < 2; ++i)
	{
		if (vk_inst.modules.vert.light[i])
		{
			vk_inst.device.destroyShaderModule(vk_inst.modules.vert.light[i]);
			vk_inst.modules.vert.light[i] = nullptr;
		}

		for (j = 0; j < 2; ++j)
		{
			if (vk_inst.modules.frag.light[i][j])
			{
				vk_inst.device.destroyShaderModule(vk_inst.modules.frag.light[i][j]);
				vk_inst.modules.frag.light[i][j] = nullptr;
			}
		}
	}

	// Destroy shader modules for vk_inst.modules.vert.ident1 and vk_inst.modules.frag.ident1
	for (i = 0; i < 2; ++i)
	{
		for (j = 0; j < 2; ++j)
		{
			for (k = 0; k < 2; ++k)
			{
				vk_inst.device.destroyShaderModule(vk_inst.modules.vert.ident1[i][j][k]);
				vk_inst.modules.vert.ident1[i][j][k] = nullptr;
			}
			vk_inst.device.destroyShaderModule(vk_inst.modules.frag.ident1[i][j]);
			vk_inst.modules.frag.ident1[i][j] = nullptr;
		}
	}

	// Destroy shader modules for vk_inst.modules.vert.fixed and vk_inst.modules.frag.fixed
	for (i = 0; i < 2; ++i)
	{
		for (j = 0; j < 2; ++j)
		{
			for (k = 0; k < 2; ++k)
			{
				vk_inst.device.destroyShaderModule(vk_inst.modules.vert.fixed[i][j][k]);
				vk_inst.modules.vert.fixed[i][j][k] = nullptr;
			}
			vk_inst.device.destroyShaderModule(vk_inst.modules.frag.fixed[i][j]);
			vk_inst.modules.frag.fixed[i][j] = nullptr;
		}
	}

	// Destroy shader modules for vk_inst.modules.frag.ent
	for (i = 0; i < 1; ++i)
	{
		for (j = 0; j < 2; ++j)
		{
			vk_inst.device.destroyShaderModule(vk_inst.modules.frag.ent[i][j]);
			vk_inst.modules.frag.ent[i][j] = nullptr;
		}
	}

	vk_inst.device.destroyShaderModule(vk_inst.modules.frag.gen0_df);
	vk_inst.device.destroyShaderModule(vk_inst.modules.color_fs);
	vk_inst.device.destroyShaderModule(vk_inst.modules.color_vs);

	vk_inst.device.destroyShaderModule(vk_inst.modules.fog_vs);
	vk_inst.device.destroyShaderModule(vk_inst.modules.fog_fs);

	vk_inst.device.destroyShaderModule(vk_inst.modules.dot_vs);
	vk_inst.device.destroyShaderModule(vk_inst.modules.dot_fs);

	vk_inst.device.destroyShaderModule(vk_inst.modules.bloom_fs);
	vk_inst.device.destroyShaderModule(vk_inst.modules.blur_fs);
	vk_inst.device.destroyShaderModule(vk_inst.modules.blend_fs);

	vk_inst.device.destroyShaderModule(vk_inst.modules.gamma_vs);
	vk_inst.device.destroyShaderModule(vk_inst.modules.gamma_fs);

__cleanup:
	if (vk_inst.device)
	{
		vk_inst.device.destroy();
	}

	deinit_device_functions();

	reset_vk_instance(vk_inst);

	vk_world = Vk_World{};

	if (code != REF_KEEP_CONTEXT)
	{
		vk_destroy_instance();
		deinit_instance_functions();
	}
}

void vk_wait_idle(void)
{
	VK_CHECK(vk_inst.device.waitIdle());
}

void vk_queue_wait_idle( void )
{
	VK_CHECK(  vk_inst.queue.waitIdle() );
}

void vk_release_resources(void)
{
	uint32_t i;
	int j;

	vk_wait_idle();

	// Free image chunk memory
	for (i = 0; i < static_cast<uint32_t>(vk_world.num_image_chunks); ++i)
	{
		vk_inst.device.freeMemory(vk_world.image_chunks[i].memory);
	}

	vk_clean_staging_buffer();

	// Destroy samplers
	// for (i = 0; i < static_cast<uint32_t>(vk_world.num_samplers); ++i)
	// {
	// 	vk_inst.device.destroySampler(vk_world.samplers[i]);
	// 	vk_world.samplers[i] = nullptr;
	// }

	// Destroy pipelines
	for (i = vk_inst.pipelines_world_base; i < vk_inst.pipelines_count; ++i)
	{
		for (j = 0; j < RENDER_PASS_COUNT; ++j)
		{
			if (vk_inst.pipelines[i].handle[j])
			{
				vk_inst.device.destroyPipeline(vk_inst.pipelines[i].handle[j]);
				vk_inst.pipelines[i].handle[j] = nullptr;
				--vk_inst.pipeline_create_count;
			}
		}
		vk_inst.pipelines[i] = {};
	}
	vk_inst.pipelines_count = vk_inst.pipelines_world_base;

	// Reset descriptor pool
	vk_inst.device.resetDescriptorPool(vk_inst.descriptor_pool);

	// Adjust image chunk size based on usage
	if (vk_world.num_image_chunks > 1)
	{
		vk_inst.image_chunk_size = (IMAGE_CHUNK_SIZE * 2);
	}
#if 0 // do not reduce chunk size
	else if (vk_world.num_image_chunks == 1)
	{
		if (vk_world.image_chunks[0].used < (IMAGE_CHUNK_SIZE - (IMAGE_CHUNK_SIZE / 10)))
		{
			vk_inst.image_chunk_size = IMAGE_CHUNK_SIZE;
		}
	}
#endif

	vk_world = Vk_World{};

	// Reset geometry buffers offsets
	for (i = 0; i < NUM_COMMAND_BUFFERS; ++i)
	{
		vk_inst.tess[i].uniform_read_offset = 0;
		vk_inst.tess[i].vertex_buffer_offset = 0;
	}

	// std::fill_n(vk_inst.cmd->buf_offset, sizeof(vk_inst.cmd->buf_offset), vk::DeviceSize{0});
	// std::fill_n(vk_inst.cmd->vbo_offset, sizeof(vk_inst.cmd->vbo_offset), vk::DeviceSize{0});
	Com_Memset(vk_inst.cmd->buf_offset, 0, sizeof(vk_inst.cmd->buf_offset));
	Com_Memset(vk_inst.cmd->vbo_offset, 0, sizeof(vk_inst.cmd->vbo_offset));

	vk_inst.stats = {};
}

#if 0
static void record_buffer_memory_barrier(const vk::CommandBuffer &cb, const vk::Buffer &buffer,
										 vk::DeviceSize size, vk::DeviceSize offset, vk::PipelineStageFlags src_stages,
										 vk::PipelineStageFlags dst_stages, vk::AccessFlags src_access,
										 vk::AccessFlags dst_access)
{
	vk::BufferMemoryBarrier barrier = {src_access,
									   dst_access,
									   vk::QueueFamilyIgnored,
									   vk::QueueFamilyIgnored,
									   buffer,
									   offset,
									   size,
									   nullptr};

	cb.pipelineBarrier(
		src_stages,
		dst_stages,
		vk::DependencyFlags(), // No dependencies
		{},					   // No memory barriers
		{barrier},			   // Single buffer memory barrier
		{}					   // No image memory barriers
	);
}
#endif

void vk_create_image(image_t &image, const int width, const int height, const int mip_levels)
{
	if (image.handle)
	{
		vk_inst.device.destroyImage(image.handle);
		image.handle = nullptr;
	}

	if (image.view)
	{
		vk_inst.device.destroyImageView(image.view);
		image.view = nullptr;
	}

	// create image

	vk::ImageCreateInfo desc{{},
							 vk::ImageType::e2D,
							 image.internalFormat,
							 {(uint32_t)width, (uint32_t)height, 1},
							 (uint32_t)mip_levels,
							 1,
							 vk::SampleCountFlagBits::e1,
							 vk::ImageTiling::eOptimal,
							 vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
							 vk::SharingMode::eExclusive,
							 0,
							 nullptr,
							 vk::ImageLayout::eUndefined,
							 nullptr};

	VK_CHECK_ASSIGN(image.handle, vk_inst.device.createImage(desc));

	allocate_and_bind_image_memory(image.handle);

	// create image view
	vk::ImageViewCreateInfo descImageView{{},
										  image.handle,
										  vk::ImageViewType::e2D,
										  image.internalFormat,
										  {},
										  {vk::ImageAspectFlagBits::eColor,
										   0,
										   vk::RemainingMipLevels,
										   0,
										   1},
										  nullptr};

	VK_CHECK_ASSIGN(image.view, vk_inst.device.createImageView(descImageView));

	// create associated descriptor set
	if (!image.descriptor)
	{
		vk::DescriptorSetAllocateInfo descSetAlloc{vk_inst.descriptor_pool,
												   1,
												   &vk_inst.set_layout_sampler,
												   nullptr};

		VK_CHECK(vk_inst.device.allocateDescriptorSets(&descSetAlloc, &image.descriptor));
	}

	vk_update_descriptor_set(image, mip_levels > 1 ? true : false);
#ifdef USE_VK_VALIDATION
	SET_OBJECT_NAME(VkImage(image.handle), image.imgName, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT);
	SET_OBJECT_NAME(VkImageView(image.view), image.imgName, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT);
	SET_OBJECT_NAME(VkDescriptorSet(image.descriptor), image.imgName, VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT);
#endif
}

static byte *resample_image_data(vk::Format target_format, byte *data, const int data_size, int *bytes_per_pixel)
{
	byte *buffer;
	uint16_t *p;
	int i, n;

	switch (target_format)
	{
	case vk::Format::eB4G4R4A4UnormPack16:
		buffer = (byte *)ri.Hunk_AllocateTempMemory(data_size / 2);
		p = (uint16_t *)buffer;
		for (i = 0; i < data_size; i += 4, p++)
		{
			byte r = data[i + 0];
			byte g = data[i + 1];
			byte b = data[i + 2];
			byte a = data[i + 3];
			*p = (uint32_t)((a / 255.0) * 15.0 + 0.5) |
				 ((uint32_t)((r / 255.0) * 15.0 + 0.5) << 4) |
				 ((uint32_t)((g / 255.0) * 15.0 + 0.5) << 8) |
				 ((uint32_t)((b / 255.0) * 15.0 + 0.5) << 12);
		}
		*bytes_per_pixel = 2;
		return buffer; // must be freed after upload!

	case vk::Format::eA1R5G5B5UnormPack16:
		buffer = (byte *)ri.Hunk_AllocateTempMemory(data_size / 2);
		p = (uint16_t *)buffer;
		for (i = 0; i < data_size; i += 4, p++)
		{
			byte r = data[i + 0];
			byte g = data[i + 1];
			byte b = data[i + 2];
			*p = (uint32_t)((b / 255.0) * 31.0 + 0.5) |
				 ((uint32_t)((g / 255.0) * 31.0 + 0.5) << 5) |
				 ((uint32_t)((r / 255.0) * 31.0 + 0.5) << 10) |
				 (1 << 15);
		}
		*bytes_per_pixel = 2;
		return buffer; // must be freed after upload!

	case vk::Format::eB8G8R8A8Unorm:
		buffer = (byte *)ri.Hunk_AllocateTempMemory(data_size);
		for (i = 0; i < data_size; i += 4)
		{
			buffer[i + 0] = data[i + 2];
			buffer[i + 1] = data[i + 1];
			buffer[i + 2] = data[i + 0];
			buffer[i + 3] = data[i + 3];
		}
		*bytes_per_pixel = 4;
		return buffer;

	case vk::Format::eR8G8B8Unorm:
	{
		buffer = (byte *)ri.Hunk_AllocateTempMemory((data_size * 3) / 4);
		for (i = 0, n = 0; i < data_size; i += 4, n += 3)
		{
			buffer[n + 0] = data[i + 0];
			buffer[n + 1] = data[i + 1];
			buffer[n + 2] = data[i + 2];
		}
		*bytes_per_pixel = 3;
		return buffer;
	}

	default:
		*bytes_per_pixel = 4;
		return data;
	}
}

void vk_upload_image_data(image_t &image, int x, int y, int width, int height, int mipmaps, byte *pixels, int size, bool update)
{
	vk::CommandBuffer command_buffer;
	constexpr std::size_t max_regions = 16; // Assuming a maximum of 16 regions
	std::array<vk::BufferImageCopy, max_regions> regions;
	byte *buf;
	int n = 0;
	int buffer_size = 0;
	uint32_t num_regions = 0;

	buf = resample_image_data( image.internalFormat, pixels, size, &n /*bpp*/ );
	while (true)
	{
		if (num_regions >= max_regions)
		{
			break; // Avoid overflow
		}

		vk::BufferImageCopy region = {static_cast<vk::DeviceSize>(buffer_size),
									  0,
									  0,
									  {vk::ImageAspectFlagBits::eColor,
									   num_regions,
									   0,
									   1},
									  {x, y, 0},
									  {(uint32_t)width, (uint32_t)height, 1}};

		regions[num_regions] = region;
		num_regions++;

		buffer_size += width * height * n;

		if (num_regions >= (uint32_t)mipmaps || (width == 1 && height == 1) || num_regions >= (uint32_t)max_regions)
			break;

		x >>= 1;
		y >>= 1;

		width >>= 1;
		if (width < 1)
			width = 1;

		height >>= 1;
		if (height < 1)
			height = 1;
	}

#ifdef USE_UPLOAD_QUEUE
	if ( vk_wait_staging_buffer() ) {
		// wait for vkQueueSubmit() completion before new upload
	}

	if ( vk_inst.staging_buffer.size - vk_inst.staging_buffer.offset < buffer_size ) {
		// try to flush staging buffer and reset offset
		vk_flush_staging_buffer( false );
	}

	if ( vk_inst.staging_buffer.size /* - vk_world.staging_buffer_offset */ < buffer_size ) {
		// if still not enough - reallocate staging buffer
		vk_alloc_staging_buffer( buffer_size );
	}

	for ( n = 0; n < num_regions; n++ ) {
		regions[n].bufferOffset += vk_inst.staging_buffer.offset;
	}

	Com_Memcpy( vk_inst.staging_buffer.ptr + vk_inst.staging_buffer.offset, buf, buffer_size );

	if ( vk_inst.staging_buffer.offset == 0 ) {
		// pInheritanceInfo defaults to nullptr; only need the usage flag.
		vk::CommandBufferBeginInfo beginInfo{ vk::CommandBufferUsageFlagBits::eOneTimeSubmit };
		VK_CHECK(vk_inst.staging_command_buffer.begin(beginInfo));
	}
	//ri.Printf( PRINT_WARNING, "batch @%6i + %i %s \n", (int)vk_world.staging_buffer_offset, (int)buffer_size, image->imgName );
	vk_inst.staging_buffer.offset += buffer_size;

	command_buffer = vk_inst.staging_command_buffer;
	if ( update ) {
		record_image_layout_transition( command_buffer, image.handle, 
			vk::ImageAspectFlagBits::eColor, 
			vk::ImageLayout::eShaderReadOnlyOptimal, 
		    vk::ImageLayout::eTransferDstOptimal);
	} else {
		record_image_layout_transition( command_buffer, image.handle, 
			vk::ImageAspectFlagBits::eColor, 
			vk::ImageLayout::eUndefined, 
			vk::ImageLayout::eTransferDstOptimal, 
			vk::PipelineStageFlagBits::eHost);
	}

	command_buffer.copyBufferToImage(
		vk_inst.staging_buffer.handle,
		image.handle,
		vk::ImageLayout::eTransferDstOptimal,
		num_regions,
		regions.data());

	// final transition after upload comleted
	record_image_layout_transition( command_buffer, image.handle, 
		vk::ImageAspectFlagBits::eColor, 
		vk::ImageLayout::eTransferDstOptimal, 
		vk::ImageLayout::eShaderReadOnlyOptimal);
#else
	if ( vk_world.staging_buffer_size < buffer_size ) {
		vk_alloc_staging_buffer( buffer_size );
	}

	Com_Memcpy( vk_world.staging_buffer_ptr, buf, buffer_size );

	command_buffer = begin_command_buffer();
	
	//record_buffer_memory_barrier( staging_command_buffer, vk_world.staging_buffer, VK_WHOLE_SIZE, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT );

	// ecord_buffer_memory_barrier(command_buffer, vk_world.staging_buffer, vk::WholeSize, vk::PipelineStageFlagBits::eHost, vk::PipelineStageFlagBits::eTransfer, vk::AccessFlagBits::eHostWrite, vk::AccessFlagBits::eTransferRead);

	if (update)
	{
		record_image_layout_transition(command_buffer, image.handle, vk::ImageAspectFlagBits::eColor,
									   vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageLayout::eTransferDstOptimal);
	}
	else
	{
		record_image_layout_transition(command_buffer, image.handle, vk::ImageAspectFlagBits::eColor,
									   vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal, vk::PipelineStageFlagBits::eHost);
	}

	command_buffer.copyBufferToImage(
		vk_inst.staging_buffer.handle,
		image.handle,
		vk::ImageLayout::eTransferDstOptimal,
		num_regions,
		regions.data());

	record_image_layout_transition(command_buffer, image.handle, vk::ImageAspectFlagBits::eColor, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
	end_command_buffer( command_buffer, __func__ );
#endif

	if (buf != pixels)
	{
		ri.Hunk_FreeTempMemory(buf);
	}
}

void vk_update_descriptor_set(const image_t &image, const bool mipmap)
{
	Vk_Sampler_Def sampler_def = {};

	sampler_def.address_mode = image.wrapClampMode;

	if (mipmap)
	{
		sampler_def.gl_mag_filter = gl_filter_max;
		sampler_def.gl_min_filter = gl_filter_min;
	}
	else
	{
		sampler_def.gl_mag_filter = GL_LINEAR;
		sampler_def.gl_min_filter = GL_LINEAR;
		// no anisotropy without mipmaps
		sampler_def.noAnisotropy = true;
	}

	vk::DescriptorImageInfo image_info{vk_find_sampler(sampler_def),
									   image.view,
									   vk::ImageLayout::eShaderReadOnlyOptimal};

	vk::WriteDescriptorSet descriptor_write{image.descriptor,
											0,
											0,
											1,
											vk::DescriptorType::eCombinedImageSampler,
											&image_info,
											nullptr,
											nullptr,
											nullptr};
	vk_inst.device.updateDescriptorSets(descriptor_write, nullptr);
}

void vk_destroy_image_resources(vk::Image &image, vk::ImageView &imageView)
{

	if (image)
	{
		vk_inst.device.destroyImage(image);
		image = nullptr;
	}

	if (imageView)
	{
		vk_inst.device.destroyImageView(imageView);
		imageView = nullptr;
	}
}

// Define a struct to hold the RGB values
struct ColorDepth
{
	int r;
	int g;
	int b;
};

static constexpr ColorDepth GetColorDepthForFormat(vk::Format format)
{
	switch (format)
	{
	case vk::Format::eB8G8R8A8Unorm:
	case vk::Format::eB8G8R8A8Srgb:
	case vk::Format::eR8G8B8A8Unorm:
	case vk::Format::eR8G8B8A8Srgb:
	case vk::Format::eR8G8B8A8Snorm:
	case vk::Format::eA8B8G8R8UnormPack32:
	case vk::Format::eA8B8G8R8SnormPack32:
	case vk::Format::eA8B8G8R8SrgbPack32:
	case vk::Format::eB8G8R8A8Snorm:
		return {255, 255, 255};

	case vk::Format::eA2B10G10R10UnormPack32:
	case vk::Format::eA2R10G10B10UnormPack32:
		return {1023, 1023, 1023};

	case vk::Format::eR16G16B16A16Unorm:
	case vk::Format::eR16G16B16A16Snorm:
		return {65535, 65535, 65535};

	case vk::Format::eR5G6B5UnormPack16:
	case vk::Format::eB5G6R5UnormPack16:
		return {31, 63, 31};

	case vk::Format::eR4G4B4A4UnormPack16:
	case vk::Format::eB4G4R4A4UnormPack16:
		return {15, 15, 15};

	case vk::Format::eA1R5G5B5UnormPack16:
	case vk::Format::eR5G5B5A1UnormPack16:
	case vk::Format::eB5G5R5A1UnormPack16:
		return {31, 31, 31};

	default:
		return {-1, -1, -1};
	}
}

static bool vk_surface_format_color_depth(const vk::Format format, int *r, int *g, int *b)
{
	ColorDepth depth = GetColorDepthForFormat(format);

	if (depth.r == -1 && depth.g == -1 && depth.b == -1) // Default case
	{
		*r = 255;
		*g = 255;
		*b = 255;
		return false;
	}

	*r = depth.r;
	*g = depth.g;
	*b = depth.b;
	return true;
}

void vk_create_post_process_pipeline(const int program_index, const uint32_t width, const uint32_t height)
{
	vk::Pipeline *pipeline;
	vk::ShaderModule fsmodule;
	vk::RenderPass renderpass;
	vk::PipelineLayout layout;
	vk::SampleCountFlagBits samples;
#ifdef USE_VK_VALIDATION
	const char *pipeline_name;
#endif
	bool blend = false;

	struct FragSpecData
	{
		float gamma;
		float overbright;
		float greyscale;
		float bloom_threshold;
		float bloom_intensity;
		int bloom_threshold_mode;
		int bloom_modulate;
		int dither;
		int depth_r;
		int depth_g;
		int depth_b;
	};

	switch (program_index)
	{
	case 1: // bloom extraction
		pipeline = &vk_inst.bloom_extract_pipeline;
		fsmodule = vk_inst.modules.bloom_fs;
		renderpass = vk_inst.render_pass.bloom_extract;
		layout = vk_inst.pipeline_layout_post_process;
		samples = vk::SampleCountFlagBits::e1;
#ifdef USE_VK_VALIDATION
		pipeline_name = "bloom extraction pipeline";
#endif
		blend = false;
		break;
	case 2: // final bloom blend
		pipeline = &vk_inst.bloom_blend_pipeline;
		fsmodule = vk_inst.modules.blend_fs;
		renderpass = vk_inst.render_pass.post_bloom;
		layout = vk_inst.pipeline_layout_blend;
		samples = vkSamples;
#ifdef USE_VK_VALIDATION
		pipeline_name = "bloom blend pipeline";
#endif
		blend = true;
		break;
	case 3: // capture buffer extraction
		pipeline = &vk_inst.capture_pipeline;
		fsmodule = vk_inst.modules.gamma_fs;
		renderpass = vk_inst.render_pass.capture;
		layout = vk_inst.pipeline_layout_post_process;
		samples = vk::SampleCountFlagBits::e1;
#ifdef USE_VK_VALIDATION
		pipeline_name = "capture buffer pipeline";
#endif
		blend = false;
		break;
	default: // gamma correction
		pipeline = &vk_inst.gamma_pipeline;
		fsmodule = vk_inst.modules.gamma_fs;
		renderpass = vk_inst.render_pass.gamma;
		layout = vk_inst.pipeline_layout_post_process;
		samples = vk::SampleCountFlagBits::e1;
#ifdef USE_VK_VALIDATION
		pipeline_name = "gamma-correction pipeline";
#endif
		blend = false;
		break;
	}

	if (*pipeline)
	{
		VK_CHECK(vk_inst.device.waitIdle());
		vk_inst.device.destroyPipeline(*pipeline);
		*pipeline = nullptr;
	}

	vk::PipelineVertexInputStateCreateInfo vertex_input_state{};

	// shaders
	std::array<vk::PipelineShaderStageCreateInfo, 2> shader_stages;
	set_shader_stage_desc(shader_stages[0], vk::ShaderStageFlagBits::eVertex, vk_inst.modules.gamma_vs, "main");
	set_shader_stage_desc(shader_stages[1], vk::ShaderStageFlagBits::eFragment, fsmodule, "main");

	FragSpecData frag_spec_data{
		1.0f / (r_gamma->value),
		(float)(1 << tr.overbrightBits),
		r_greyscale->value,
		r_bloom_threshold->value,
		r_bloom_intensity->value,
		r_bloom_threshold_mode->integer,
		r_bloom_modulate->integer,
		r_dither->integer,
		0,
		0,
		0};

	if (!vk_surface_format_color_depth(vk_inst.present_format.format, &frag_spec_data.depth_r, &frag_spec_data.depth_g, &frag_spec_data.depth_b))
		ri.Printf(PRINT_ALL, "Format %s not recognized, dither to assume 8bpc\n", vk::to_string(vk_inst.base_format.format).data());

	std::array<vk::SpecializationMapEntry, 11> spec_entries = {{
		{0, offsetof(FragSpecData, gamma), sizeof(frag_spec_data.gamma)},
		{1, offsetof(FragSpecData, overbright), sizeof(frag_spec_data.overbright)},
		{2, offsetof(FragSpecData, greyscale), sizeof(frag_spec_data.greyscale)},
		{3, offsetof(FragSpecData, bloom_threshold), sizeof(frag_spec_data.bloom_threshold)},
		{4, offsetof(FragSpecData, bloom_intensity), sizeof(frag_spec_data.bloom_intensity)},
		{5, offsetof(FragSpecData, bloom_threshold_mode), sizeof(frag_spec_data.bloom_threshold_mode)},
		{6, offsetof(FragSpecData, bloom_modulate), sizeof(frag_spec_data.bloom_modulate)},
		{7, offsetof(FragSpecData, dither), sizeof(frag_spec_data.dither)},
		{8, offsetof(FragSpecData, depth_r), sizeof(frag_spec_data.depth_r)},
		{9, offsetof(FragSpecData, depth_g), sizeof(frag_spec_data.depth_g)},
		{10, offsetof(FragSpecData, depth_b), sizeof(frag_spec_data.depth_b)},
	}};

	vk::SpecializationInfo frag_spec_info{
		static_cast<uint32_t>(spec_entries.size()),
		spec_entries.data(),
		sizeof(frag_spec_data),
		&frag_spec_data};
	shader_stages[1].pSpecializationInfo = &frag_spec_info;

	//
	// Primitive assembly.
	//
	vk::PipelineInputAssemblyStateCreateInfo input_assembly_state{{}, vk::PrimitiveTopology::eTriangleStrip, vk::False};

	//
	// Viewport.
	//
	vk::Viewport viewport{
		program_index == 0 ? 0.0f + vk_inst.blitX0 : 0.0f,
		program_index == 0 ? 0.0f + vk_inst.blitY0 : 0.0f,
		program_index == 0 ? gls.windowWidth - vk_inst.blitX0 * 2 : static_cast<float>(width),
		program_index == 0 ? gls.windowHeight - vk_inst.blitY0 * 2 : static_cast<float>(height),
		0.0f,
		1.0f};

	vk::Rect2D scissor{
		{static_cast<int32_t>(viewport.x), static_cast<int32_t>(viewport.y)},
		{static_cast<uint32_t>(viewport.width), static_cast<uint32_t>(viewport.height)}};

	vk::PipelineViewportStateCreateInfo viewport_state{{}, 1, &viewport, 1, &scissor};

	//
	// Rasterization.
	//
	vk::PipelineRasterizationStateCreateInfo rasterization_state{{},
																 vk::False,
																 vk::False,
																 vk::PolygonMode::eFill,
																 vk::CullModeFlagBits::eNone,
																 vk::FrontFace::eClockwise,
																 vk::False,
																 0.0f,
																 0.0f,
																 0.0f,
																 1.0f,
																 nullptr};

	vk::PipelineMultisampleStateCreateInfo multisample_state{{},
															 samples,
															 vk::False,
															 1.0f,
															 nullptr,
															 vk::False,
															 vk::False,
															 nullptr};

	vk::PipelineColorBlendAttachmentState attachment_blend_state{
		blend,
		blend == true ? vk::BlendFactor::eOne : vk::BlendFactor::eZero,
		blend == true ? vk::BlendFactor::eOne : vk::BlendFactor::eZero,
		vk::BlendOp::eAdd,
		vk::BlendFactor::eZero,
		vk::BlendFactor::eZero,
		vk::BlendOp::eAdd,
		vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
			vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA};

	std::array<float, 4> blendConstants{};

	vk::PipelineColorBlendStateCreateInfo blend_state{{},
													  vk::False,
													  vk::LogicOp::eCopy,
													  1,
													  &attachment_blend_state,
													  blendConstants};

	vk::PipelineDepthStencilStateCreateInfo depth_stencil_state{{},
																vk::False,
																vk::False,
																vk::CompareOp::eNever,
																vk::False,
																vk::False,
																{},
																{},
																0.0f,
																1.0f,
																nullptr};

	vk::GraphicsPipelineCreateInfo pipeline_create_info{
		{},
		static_cast<uint32_t>(shader_stages.size()),
		shader_stages.data(),
		&vertex_input_state,
		&input_assembly_state,
		nullptr,
		&viewport_state,
		&rasterization_state,
		&multisample_state,
		&depth_stencil_state,
		&blend_state,
		nullptr,
		layout,
		renderpass,
		0,
		nullptr,
		-1};

#ifdef USE_VK_VALIDATION
	try
	{
		auto createGraphicsPipelineResult = vk_inst.device.createGraphicsPipeline(nullptr, pipeline_create_info);
		if (static_cast<int>(createGraphicsPipelineResult.result) < 0)
		{
			ri.Error(ERR_FATAL, "Vulkan: %s returned %s", "vk_create_post_process_pipeline -> createGraphicsPipeline", vk::to_string(createGraphicsPipelineResult.result).data());
		}
		else
		{
			*pipeline = createGraphicsPipelineResult.value;
			SET_OBJECT_NAME(VkPipeline(&pipeline), pipeline_name, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT);
		}
	}
	catch (vk::SystemError &err)
	{
		ri.Error(ERR_FATAL, "Vulkan error in function: %s, what: %s", "vk_create_post_process_pipeline -> createGraphicsPipeline", err.what());
	}

#else
	VK_CHECK_ASSIGN(*pipeline, vk_inst.device.createGraphicsPipeline(nullptr, pipeline_create_info));
#endif
}

static vk::VertexInputBindingDescription bindingsCpp[8];
static vk::VertexInputAttributeDescription attribsCpp[8];
static uint32_t num_binds;
static uint32_t num_attrs;

static void push_bind(const uint32_t binding, const uint32_t stride)
{
	bindingsCpp[num_binds].binding = binding;
	bindingsCpp[num_binds].stride = stride;
	bindingsCpp[num_binds].inputRate = vk::VertexInputRate::eVertex;
	num_binds++;
}

static void push_attr(const uint32_t location, const uint32_t binding, const vk::Format format)
{
	attribsCpp[num_attrs].location = location;
	attribsCpp[num_attrs].binding = binding;
	attribsCpp[num_attrs].format = format;
	attribsCpp[num_attrs].offset = 0;
	num_attrs++;
}

static constexpr vk::PrimitiveTopology GetTopologyByPrimitivies(const Vk_Pipeline_Def &def)
{
	switch (def.primitives)
	{
	case Vk_Primitive_Topology::LINE_LIST:
		return vk::PrimitiveTopology::eLineList;
	case Vk_Primitive_Topology::POINT_LIST:
		return vk::PrimitiveTopology::ePointList;
	case Vk_Primitive_Topology::TRIANGLE_STRIP:
		return vk::PrimitiveTopology::eTriangleStrip;
	default:
		return vk::PrimitiveTopology::eTriangleList;
	}
}

static void GetCullModeByFaceCulling(const Vk_Pipeline_Def &def, vk::CullModeFlags &cullMode)
{
	switch (def.face_culling)
	{
	case cullType_t::CT_TWO_SIDED:
		cullMode = vk::CullModeFlagBits::eNone;
		break;
	case cullType_t::CT_FRONT_SIDED:
		cullMode = (def.mirror ? vk::CullModeFlagBits::eFront : vk::CullModeFlagBits::eBack);
		break;
	case cullType_t::CT_BACK_SIDED:
		cullMode = (def.mirror ? vk::CullModeFlagBits::eBack : vk::CullModeFlagBits::eFront);
		break;
	default:
		ri.Error(ERR_DROP, "create_pipeline: invalid face culling mode %i\n", static_cast<int>(def.face_culling));
		break;
	}
}

static vk::PipelineColorBlendAttachmentState createBlendAttachmentState(const uint32_t state_bits, const Vk_Pipeline_Def &def)
{
	// Determine whether blending is enabled
	bool blendEnable = (state_bits & (GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS)) ? vk::True : vk::False;

	// Initialize colorWriteMask based on shader phase/type
	vk::ColorComponentFlags colorWriteMask =
		(def.shadow_phase == Vk_Shadow_Phase::SHADOW_EDGES || def.shader_type == Vk_Shader_Type::TYPE_SIGNLE_TEXTURE_DF) ? vk::ColorComponentFlags(0) : vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

	vk::BlendFactor srcColorBlendFactor = {};
	vk::BlendFactor dstColorBlendFactor = {};

	if (blendEnable)
	{
		switch (state_bits & GLS_SRCBLEND_BITS)
		{
		case GLS_SRCBLEND_ZERO:
			srcColorBlendFactor = vk::BlendFactor::eZero;
			break;
		case GLS_SRCBLEND_ONE:
			srcColorBlendFactor = vk::BlendFactor::eOne;
			break;
		case GLS_SRCBLEND_DST_COLOR:
			srcColorBlendFactor = vk::BlendFactor::eDstColor;
			break;
		case GLS_SRCBLEND_ONE_MINUS_DST_COLOR:
			srcColorBlendFactor = vk::BlendFactor::eOneMinusDstColor;
			break;
		case GLS_SRCBLEND_SRC_ALPHA:
			srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
			break;
		case GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA:
			srcColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
			break;
		case GLS_SRCBLEND_DST_ALPHA:
			srcColorBlendFactor = vk::BlendFactor::eDstAlpha;
			break;
		case GLS_SRCBLEND_ONE_MINUS_DST_ALPHA:
			srcColorBlendFactor = vk::BlendFactor::eOneMinusDstAlpha;
			break;
		case GLS_SRCBLEND_ALPHA_SATURATE:
			srcColorBlendFactor = vk::BlendFactor::eSrcAlphaSaturate;
			break;
		default:
			ri.Error(ERR_DROP, "create_pipeline: invalid src blend state bits\n");
			break;
		}

		switch (state_bits & GLS_DSTBLEND_BITS)
		{
		case GLS_DSTBLEND_ZERO:
			dstColorBlendFactor = vk::BlendFactor::eZero;
			break;
		case GLS_DSTBLEND_ONE:
			dstColorBlendFactor = vk::BlendFactor::eOne;
			break;
		case GLS_DSTBLEND_SRC_COLOR:
			dstColorBlendFactor = vk::BlendFactor::eSrcColor;
			break;
		case GLS_DSTBLEND_ONE_MINUS_SRC_COLOR:
			dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcColor;
			break;
		case GLS_DSTBLEND_SRC_ALPHA:
			dstColorBlendFactor = vk::BlendFactor::eSrcAlpha;
			break;
		case GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA:
			dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
			break;
		case GLS_DSTBLEND_DST_ALPHA:
			dstColorBlendFactor = vk::BlendFactor::eDstAlpha;
			break;
		case GLS_DSTBLEND_ONE_MINUS_DST_ALPHA:
			dstColorBlendFactor = vk::BlendFactor::eOneMinusDstAlpha;
			break;
		default:
			ri.Error(ERR_DROP, "create_pipeline: invalid dst blend state bits\n");
			break;
		}
	}

	// Create the blend attachment state
	vk::PipelineColorBlendAttachmentState attachmentBlendState{
		blendEnable,
		srcColorBlendFactor,
		dstColorBlendFactor,
		vk::BlendOp::eAdd,
		srcColorBlendFactor,
		dstColorBlendFactor,
		vk::BlendOp::eAdd,
		colorWriteMask,
	};

	return attachmentBlendState;
}

vk::Pipeline create_pipeline(const Vk_Pipeline_Def &def, const renderPass_t renderPassIndex, uint32_t def_index)
{
	vk::ShaderModule *vs_module = nullptr;
	vk::ShaderModule *fs_module = nullptr;
	// int32_t vert_spec_data[1]; // clippping
	floatint_t frag_spec_data[11]{}; // 0:alpha-test-func, 1:alpha-test-value, 2:depth-fragment, 3:alpha-to-coverage, 4:color_mode, 5:abs_light, 6:multitexture mode, 7:discard mode, 8: ident.color, 9 - ident.alpha, 10 - acff

	vk::DynamicState dynamic_state_array[] = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};

	vk::Pipeline pipeline;
	vk::Bool32 alphaToCoverage = vk::False;
	unsigned int atest_bits;
	unsigned int state_bits = def.state_bits;

	switch (def.shader_type)
	{

	case Vk_Shader_Type::TYPE_SIGNLE_TEXTURE_LIGHTING:
		vs_module = &vk_inst.modules.vert.light[0];
		fs_module = &vk_inst.modules.frag.light[0][0];
		break;

	case Vk_Shader_Type::TYPE_SIGNLE_TEXTURE_LIGHTING_LINEAR:
		vs_module = &vk_inst.modules.vert.light[0];
		fs_module = &vk_inst.modules.frag.light[1][0];
		break;

	case Vk_Shader_Type::TYPE_SIGNLE_TEXTURE_DF:
		state_bits |= GLS_DEPTHMASK_TRUE;
		vs_module = &vk_inst.modules.vert.ident1[0][0][0];
		fs_module = &vk_inst.modules.frag.gen0_df;
		break;

	case Vk_Shader_Type::TYPE_SIGNLE_TEXTURE_FIXED_COLOR:
		vs_module = &vk_inst.modules.vert.fixed[0][0][0];
		fs_module = &vk_inst.modules.frag.fixed[0][0];
		break;

	case Vk_Shader_Type::TYPE_SIGNLE_TEXTURE_FIXED_COLOR_ENV:
		vs_module = &vk_inst.modules.vert.fixed[0][1][0];
		fs_module = &vk_inst.modules.frag.fixed[0][0];
		break;

	case Vk_Shader_Type::TYPE_SIGNLE_TEXTURE_ENT_COLOR:
		vs_module = &vk_inst.modules.vert.fixed[0][0][0];
		fs_module = &vk_inst.modules.frag.ent[0][0];
		break;

	case Vk_Shader_Type::TYPE_SIGNLE_TEXTURE_ENT_COLOR_ENV:
		vs_module = &vk_inst.modules.vert.fixed[0][1][0];
		fs_module = &vk_inst.modules.frag.ent[0][0];
		break;

	case Vk_Shader_Type::TYPE_SIGNLE_TEXTURE:
		vs_module = &vk_inst.modules.vert.gen[0][0][0][0];
		fs_module = &vk_inst.modules.frag.gen[0][0][0];
		break;

	case Vk_Shader_Type::TYPE_SIGNLE_TEXTURE_ENV:
		vs_module = &vk_inst.modules.vert.gen[0][0][1][0];
		fs_module = &vk_inst.modules.frag.gen[0][0][0];
		break;

	case Vk_Shader_Type::TYPE_SIGNLE_TEXTURE_IDENTITY:
		vs_module = &vk_inst.modules.vert.ident1[0][0][0];
		fs_module = &vk_inst.modules.frag.ident1[0][0];
		break;

	case Vk_Shader_Type::TYPE_SIGNLE_TEXTURE_IDENTITY_ENV:
		vs_module = &vk_inst.modules.vert.ident1[0][1][0];
		fs_module = &vk_inst.modules.frag.ident1[0][0];
		break;

	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD2_IDENTITY:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_MUL2_IDENTITY:
		vs_module = &vk_inst.modules.vert.ident1[1][0][0];
		fs_module = &vk_inst.modules.frag.ident1[1][0];
		break;

	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD2_IDENTITY_ENV:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_MUL2_IDENTITY_ENV:
		vs_module = &vk_inst.modules.vert.ident1[1][1][0];
		fs_module = &vk_inst.modules.frag.ident1[1][0];
		break;

	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR:
		vs_module = &vk_inst.modules.vert.fixed[1][0][0];
		fs_module = &vk_inst.modules.frag.fixed[1][0];
		break;

	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR_ENV:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR_ENV:
		vs_module = &vk_inst.modules.vert.fixed[1][1][0];
		fs_module = &vk_inst.modules.frag.fixed[1][0];
		break;

	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_MUL2:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD2_1_1:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD2:
		vs_module = &vk_inst.modules.vert.gen[1][0][0][0];
		fs_module = &vk_inst.modules.frag.gen[1][0][0];
		break;

	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_MUL2_ENV:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD2_1_1_ENV:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD2_ENV:
		vs_module = &vk_inst.modules.vert.gen[1][0][1][0];
		fs_module = &vk_inst.modules.frag.gen[1][0][0];
		break;

	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_MUL3:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD3_1_1:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD3:
		vs_module = &vk_inst.modules.vert.gen[2][0][0][0];
		fs_module = &vk_inst.modules.frag.gen[2][0][0];
		break;

	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_MUL3_ENV:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD3_1_1_ENV:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD3_ENV:
		vs_module = &vk_inst.modules.vert.gen[2][0][1][0];
		fs_module = &vk_inst.modules.frag.gen[2][0][0];
		break;

	case Vk_Shader_Type::TYPE_BLEND2_ADD:
	case Vk_Shader_Type::TYPE_BLEND2_MUL:
	case Vk_Shader_Type::TYPE_BLEND2_ALPHA:
	case Vk_Shader_Type::TYPE_BLEND2_ONE_MINUS_ALPHA:
	case Vk_Shader_Type::TYPE_BLEND2_MIX_ALPHA:
	case Vk_Shader_Type::TYPE_BLEND2_MIX_ONE_MINUS_ALPHA:
	case Vk_Shader_Type::TYPE_BLEND2_DST_COLOR_SRC_ALPHA:
		vs_module = &vk_inst.modules.vert.gen[1][1][0][0];
		fs_module = &vk_inst.modules.frag.gen[1][1][0];
		break;

	case Vk_Shader_Type::TYPE_BLEND2_ADD_ENV:
	case Vk_Shader_Type::TYPE_BLEND2_MUL_ENV:
	case Vk_Shader_Type::TYPE_BLEND2_ALPHA_ENV:
	case Vk_Shader_Type::TYPE_BLEND2_ONE_MINUS_ALPHA_ENV:
	case Vk_Shader_Type::TYPE_BLEND2_MIX_ALPHA_ENV:
	case Vk_Shader_Type::TYPE_BLEND2_MIX_ONE_MINUS_ALPHA_ENV:
	case Vk_Shader_Type::TYPE_BLEND2_DST_COLOR_SRC_ALPHA_ENV:
		vs_module = &vk_inst.modules.vert.gen[1][1][1][0];
		fs_module = &vk_inst.modules.frag.gen[1][1][0];
		break;

	case Vk_Shader_Type::TYPE_BLEND3_ADD:
	case Vk_Shader_Type::TYPE_BLEND3_MUL:
	case Vk_Shader_Type::TYPE_BLEND3_ALPHA:
	case Vk_Shader_Type::TYPE_BLEND3_ONE_MINUS_ALPHA:
	case Vk_Shader_Type::TYPE_BLEND3_MIX_ALPHA:
	case Vk_Shader_Type::TYPE_BLEND3_MIX_ONE_MINUS_ALPHA:
	case Vk_Shader_Type::TYPE_BLEND3_DST_COLOR_SRC_ALPHA:
		vs_module = &vk_inst.modules.vert.gen[2][1][0][0];
		fs_module = &vk_inst.modules.frag.gen[2][1][0];
		break;

	case Vk_Shader_Type::TYPE_BLEND3_ADD_ENV:
	case Vk_Shader_Type::TYPE_BLEND3_MUL_ENV:
	case Vk_Shader_Type::TYPE_BLEND3_ALPHA_ENV:
	case Vk_Shader_Type::TYPE_BLEND3_ONE_MINUS_ALPHA_ENV:
	case Vk_Shader_Type::TYPE_BLEND3_MIX_ALPHA_ENV:
	case Vk_Shader_Type::TYPE_BLEND3_MIX_ONE_MINUS_ALPHA_ENV:
	case Vk_Shader_Type::TYPE_BLEND3_DST_COLOR_SRC_ALPHA_ENV:
		vs_module = &vk_inst.modules.vert.gen[2][1][1][0];
		fs_module = &vk_inst.modules.frag.gen[2][1][0];
		break;

	case Vk_Shader_Type::TYPE_COLOR_BLACK:
	case Vk_Shader_Type::TYPE_COLOR_WHITE:
	case Vk_Shader_Type::TYPE_COLOR_GREEN:
	case Vk_Shader_Type::TYPE_COLOR_RED:
		vs_module = &vk_inst.modules.color_vs;
		fs_module = &vk_inst.modules.color_fs;
		break;

	case Vk_Shader_Type::TYPE_FOG_ONLY:
		vs_module = &vk_inst.modules.fog_vs;
		fs_module = &vk_inst.modules.fog_fs;
		break;

	case Vk_Shader_Type::TYPE_DOT:
		vs_module = &vk_inst.modules.dot_vs;
		fs_module = &vk_inst.modules.dot_fs;
		break;

	default:
		ri.Error(ERR_DROP, "create_pipeline_plus: unknown shader type %i\n", static_cast<int>(def.shader_type));
		return nullptr;
	}

	if (def.fog_stage)
	{
		switch (def.shader_type)
		{
		case Vk_Shader_Type::TYPE_FOG_ONLY:
		case Vk_Shader_Type::TYPE_DOT:
		case Vk_Shader_Type::TYPE_SIGNLE_TEXTURE_DF:
		case Vk_Shader_Type::TYPE_COLOR_BLACK:
		case Vk_Shader_Type::TYPE_COLOR_WHITE:
		case Vk_Shader_Type::TYPE_COLOR_GREEN:
		case Vk_Shader_Type::TYPE_COLOR_RED:
			break;
		default:
			// switch to fogged modules
			vs_module++;
			fs_module++;
			break;
		}
	}

	std::array<vk::PipelineShaderStageCreateInfo, 2> shader_stages;
	set_shader_stage_desc(shader_stages[0], vk::ShaderStageFlagBits::eVertex, *vs_module, "main");
	set_shader_stage_desc(shader_stages[1], vk::ShaderStageFlagBits::eFragment, *fs_module, "main");

	// Com_Memset( vert_spec_data, 0, sizeof( vert_spec_data ) );

	// vert_spec_data[0] = def.clipping_plane ? 1 : 0;

	// fragment shader specialization data
	atest_bits = state_bits & GLS_ATEST_BITS;
	switch (atest_bits)
	{
	case GLS_ATEST_GT_0:
		frag_spec_data[0].i = 1; // not equal
		frag_spec_data[1].f = 0.0f;
		break;
	case GLS_ATEST_LT_80:
		frag_spec_data[0].i = 2; // less than
		frag_spec_data[1].f = 0.5f;
		break;
	case GLS_ATEST_GE_80:
		frag_spec_data[0].i = 3; // greater or equal
		frag_spec_data[1].f = 0.5f;
		break;
	default:
		frag_spec_data[0].i = 0;
		frag_spec_data[1].f = 0.0f;
		break;
	};

	// depth fragment threshold
	frag_spec_data[2].f = 0.85f;

#if 0
	if (r_ext_alpha_to_coverage->integer && vkSamples != vk::SampleCountFlagBits::e1 && frag_spec_data[0].i) {
		frag_spec_data[3].i = 1;
		alphaToCoverage = vk::True;
	}
#endif

	// constant color
	switch (def.shader_type)
	{
	default:
		frag_spec_data[4].i = 0;
		break;
	case Vk_Shader_Type::TYPE_COLOR_WHITE:
		frag_spec_data[4].i = 1;
		break;
	case Vk_Shader_Type::TYPE_COLOR_GREEN:
		frag_spec_data[4].i = 2;
		break;
	case Vk_Shader_Type::TYPE_COLOR_RED:
		frag_spec_data[4].i = 3;
		break;
	}

	// abs lighting
	switch (def.shader_type)
	{
	case Vk_Shader_Type::TYPE_SIGNLE_TEXTURE_LIGHTING:
	case Vk_Shader_Type::TYPE_SIGNLE_TEXTURE_LIGHTING_LINEAR:
		frag_spec_data[5].i = def.abs_light ? 1 : 0;
	default:
		break;
	}

	// multutexture mode
	switch (def.shader_type)
	{
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_MUL2_IDENTITY:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_MUL2_IDENTITY_ENV:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR_ENV:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_MUL2:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_MUL2_ENV:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_MUL3:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_MUL3_ENV:
	case Vk_Shader_Type::TYPE_BLEND2_MUL:
	case Vk_Shader_Type::TYPE_BLEND2_MUL_ENV:
	case Vk_Shader_Type::TYPE_BLEND3_MUL:
	case Vk_Shader_Type::TYPE_BLEND3_MUL_ENV:
		frag_spec_data[6].i = 0;
		break;

	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD2_IDENTITY:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD2_IDENTITY_ENV:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR_ENV:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD2_1_1:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD2_1_1_ENV:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD3_1_1:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD3_1_1_ENV:
		frag_spec_data[6].i = 1;
		break;

	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD2:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD2_ENV:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD3:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD3_ENV:
	case Vk_Shader_Type::TYPE_BLEND2_ADD:
	case Vk_Shader_Type::TYPE_BLEND2_ADD_ENV:
	case Vk_Shader_Type::TYPE_BLEND3_ADD:
	case Vk_Shader_Type::TYPE_BLEND3_ADD_ENV:
		frag_spec_data[6].i = 2;
		break;

	case Vk_Shader_Type::TYPE_BLEND2_ALPHA:
	case Vk_Shader_Type::TYPE_BLEND2_ALPHA_ENV:
	case Vk_Shader_Type::TYPE_BLEND3_ALPHA:
	case Vk_Shader_Type::TYPE_BLEND3_ALPHA_ENV:
		frag_spec_data[6].i = 3;
		break;

	case Vk_Shader_Type::TYPE_BLEND2_ONE_MINUS_ALPHA:
	case Vk_Shader_Type::TYPE_BLEND2_ONE_MINUS_ALPHA_ENV:
	case Vk_Shader_Type::TYPE_BLEND3_ONE_MINUS_ALPHA:
	case Vk_Shader_Type::TYPE_BLEND3_ONE_MINUS_ALPHA_ENV:
		frag_spec_data[6].i = 4;
		break;

	case Vk_Shader_Type::TYPE_BLEND2_MIX_ALPHA:
	case Vk_Shader_Type::TYPE_BLEND2_MIX_ALPHA_ENV:
	case Vk_Shader_Type::TYPE_BLEND3_MIX_ALPHA:
	case Vk_Shader_Type::TYPE_BLEND3_MIX_ALPHA_ENV:
		frag_spec_data[6].i = 5;
		break;

	case Vk_Shader_Type::TYPE_BLEND2_MIX_ONE_MINUS_ALPHA:
	case Vk_Shader_Type::TYPE_BLEND2_MIX_ONE_MINUS_ALPHA_ENV:
	case Vk_Shader_Type::TYPE_BLEND3_MIX_ONE_MINUS_ALPHA:
	case Vk_Shader_Type::TYPE_BLEND3_MIX_ONE_MINUS_ALPHA_ENV:
		frag_spec_data[6].i = 6;
		break;

	case Vk_Shader_Type::TYPE_BLEND2_DST_COLOR_SRC_ALPHA:
	case Vk_Shader_Type::TYPE_BLEND2_DST_COLOR_SRC_ALPHA_ENV:
	case Vk_Shader_Type::TYPE_BLEND3_DST_COLOR_SRC_ALPHA:
	case Vk_Shader_Type::TYPE_BLEND3_DST_COLOR_SRC_ALPHA_ENV:
		frag_spec_data[6].i = 7;
		break;

	default:
		break;
	}

	frag_spec_data[8].f = ((float)def.color.rgb) / 255.0;
	frag_spec_data[9].f = ((float)def.color.alpha) / 255.0;

	if (def.fog_stage)
	{
		frag_spec_data[10].i = def.acff;
	}
	else
	{
		frag_spec_data[10].i = 0;
	}

	//
	// vertex module specialization data
	//
#if 0
	spec_entries[0].constantID = 0; // clip_plane
	spec_entries[0].offset = 0 * sizeof(int32_t);
	spec_entries[0].size = sizeof(int32_t);

	vert_spec_info.mapEntryCount = 1;
	vert_spec_info.pMapEntries = spec_entries + 0;
	vert_spec_info.dataSize = 1 * sizeof(int32_t);
	vert_spec_info.pData = &vert_spec_data[0];
	shader_stages[0].pSpecializationInfo = &vert_spec_info;
#endif
	shader_stages[0].pSpecializationInfo = nullptr;

	//
	// fragment module specialization data
	//
	std::array<vk::SpecializationMapEntry, 11> spec_entries = {{
		{0, 0 * sizeof(int32_t), sizeof(int32_t)},	// alpha-test-function
		{1, 1 * sizeof(int32_t), sizeof(float)},	// alpha-test-value
		{2, 2 * sizeof(int32_t), sizeof(float)},	// depth-fragment
		{3, 3 * sizeof(int32_t), sizeof(int32_t)},	// alpha-to-coverage
		{4, 4 * sizeof(int32_t), sizeof(int32_t)},	// color_mode
		{5, 5 * sizeof(int32_t), sizeof(int32_t)},	// abs_light
		{6, 6 * sizeof(int32_t), sizeof(int32_t)},	// multitexture mode
		{7, 7 * sizeof(int32_t), sizeof(int32_t)},	// discard mode
		{8, 8 * sizeof(int32_t), sizeof(float)},	// fixed color
		{9, 9 * sizeof(int32_t), sizeof(float)},	// fixed alpha
		{10, 10 * sizeof(int32_t), sizeof(int32_t)} // acff
	}};

	vk::SpecializationInfo frag_spec_info{static_cast<uint32_t>(spec_entries.size()),
										  spec_entries.data(),
										  (7 * sizeof(int32_t)) + (4 * sizeof(float)),
										  &frag_spec_data[0]};

	shader_stages[1].pSpecializationInfo = &frag_spec_info;

	//
	// Vertex input
	//
	num_binds = num_attrs = 0;
	switch (def.shader_type)
	{

	case Vk_Shader_Type::TYPE_FOG_ONLY:
	case Vk_Shader_Type::TYPE_DOT:
		push_bind(0, sizeof(vec4_t)); // xyz array
		push_attr(0, 0, vk::Format::eR32G32B32A32Sfloat);
		break;

	case Vk_Shader_Type::TYPE_COLOR_BLACK:
	case Vk_Shader_Type::TYPE_COLOR_WHITE:
	case Vk_Shader_Type::TYPE_COLOR_GREEN:
	case Vk_Shader_Type::TYPE_COLOR_RED:
		push_bind(0, sizeof(vec4_t)); // xyz array
		push_attr(0, 0, vk::Format::eR32G32B32A32Sfloat);
		break;

	case Vk_Shader_Type::TYPE_SIGNLE_TEXTURE_DF:
	case Vk_Shader_Type::TYPE_SIGNLE_TEXTURE_IDENTITY:
	case Vk_Shader_Type::TYPE_SIGNLE_TEXTURE_FIXED_COLOR:
	case Vk_Shader_Type::TYPE_SIGNLE_TEXTURE_ENT_COLOR:
		push_bind(0, sizeof(vec4_t)); // xyz array
		push_bind(2, sizeof(vec2_t)); // st0 array
		push_attr(0, 0, vk::Format::eR32G32B32A32Sfloat);
		push_attr(2, 2, vk::Format::eR32G32Sfloat);
		break;

	case Vk_Shader_Type::TYPE_SIGNLE_TEXTURE:
		push_bind(0, sizeof(vec4_t));	  // xyz array
		push_bind(1, sizeof(color4ub_t)); // color array
		push_bind(2, sizeof(vec2_t));	  // st0 array
		push_attr(0, 0, vk::Format::eR32G32B32A32Sfloat);
		push_attr(1, 1, vk::Format::eR8G8B8A8Unorm);
		push_attr(2, 2, vk::Format::eR32G32Sfloat);
		break;

	case Vk_Shader_Type::TYPE_SIGNLE_TEXTURE_ENV:
		push_bind(0, sizeof(vec4_t));	  // xyz array
		push_bind(1, sizeof(color4ub_t)); // color array
		// push_bind( 2, sizeof( vec2_t ) );					// st0 array
		push_bind(5, sizeof(vec4_t)); // normals
		push_attr(0, 0, vk::Format::eR32G32B32A32Sfloat);
		push_attr(1, 1, vk::Format::eR8G8B8A8Unorm);
		// push_attr( 2, 2, vk::Format::eR8G8B8A8Unorm );
		push_attr(5, 5, vk::Format::eR32G32B32A32Sfloat);
		break;

	case Vk_Shader_Type::TYPE_SIGNLE_TEXTURE_IDENTITY_ENV:
	case Vk_Shader_Type::TYPE_SIGNLE_TEXTURE_FIXED_COLOR_ENV:
	case Vk_Shader_Type::TYPE_SIGNLE_TEXTURE_ENT_COLOR_ENV:
		push_bind(0, sizeof(vec4_t)); // xyz array
		push_bind(5, sizeof(vec4_t)); // normals
		push_attr(0, 0, vk::Format::eR32G32B32A32Sfloat);
		push_attr(5, 5, vk::Format::eR32G32B32A32Sfloat);
		break;

	case Vk_Shader_Type::TYPE_SIGNLE_TEXTURE_LIGHTING:
	case Vk_Shader_Type::TYPE_SIGNLE_TEXTURE_LIGHTING_LINEAR:
		push_bind(0, sizeof(vec4_t)); // xyz array
		push_bind(1, sizeof(vec2_t)); // st0 array
		push_bind(2, sizeof(vec4_t)); // normals array
		push_attr(0, 0, vk::Format::eR32G32B32A32Sfloat);
		push_attr(1, 1, vk::Format::eR32G32Sfloat);
		push_attr(2, 2, vk::Format::eR32G32B32A32Sfloat);
		break;

	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_MUL2_IDENTITY:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD2_IDENTITY:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR:
		push_bind(0, sizeof(vec4_t)); // xyz array
		push_bind(2, sizeof(vec2_t)); // st0 array
		push_bind(3, sizeof(vec2_t)); // st1 array
		push_attr(0, 0, vk::Format::eR32G32B32A32Sfloat);
		push_attr(2, 2, vk::Format::eR32G32Sfloat);
		push_attr(3, 3, vk::Format::eR32G32Sfloat);
		break;

	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_MUL2_IDENTITY_ENV:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD2_IDENTITY_ENV:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR_ENV:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR_ENV:
		push_bind(0, sizeof(vec4_t)); // xyz array
		push_bind(3, sizeof(vec2_t)); // st1 array
		push_bind(5, sizeof(vec4_t)); // normals
		push_attr(0, 0, vk::Format::eR32G32B32A32Sfloat);
		push_attr(3, 3, vk::Format::eR32G32Sfloat);
		push_attr(5, 5, vk::Format::eR32G32B32A32Sfloat);
		break;

	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_MUL2:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD2_1_1:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD2:
		push_bind(0, sizeof(vec4_t));	  // xyz array
		push_bind(1, sizeof(color4ub_t)); // color array
		push_bind(2, sizeof(vec2_t));	  // st0 array
		push_bind(3, sizeof(vec2_t));	  // st1 array
		push_attr(0, 0, vk::Format::eR32G32B32A32Sfloat);
		push_attr(1, 1, vk::Format::eR8G8B8A8Unorm);
		push_attr(2, 2, vk::Format::eR32G32Sfloat);
		push_attr(3, 3, vk::Format::eR32G32Sfloat);
		break;

	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_MUL2_ENV:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD2_1_1_ENV:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD2_ENV:
		push_bind(0, sizeof(vec4_t));	  // xyz array
		push_bind(1, sizeof(color4ub_t)); // color array
		// push_bind( 2, sizeof( vec2_t ) );					// st0 array
		push_bind(3, sizeof(vec2_t)); // st1 array
		push_bind(5, sizeof(vec4_t)); // normals
		push_attr(0, 0, vk::Format::eR32G32B32A32Sfloat);
		push_attr(1, 1, vk::Format::eR8G8B8A8Unorm);
		// push_attr( 2, 2, vk::Format::eR32G32Sfloat );
		push_attr(3, 3, vk::Format::eR32G32Sfloat);
		push_attr(5, 5, vk::Format::eR32G32B32A32Sfloat);
		break;

	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_MUL3:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD3_1_1:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD3:
		push_bind(0, sizeof(vec4_t));	  // xyz array
		push_bind(1, sizeof(color4ub_t)); // color array
		push_bind(2, sizeof(vec2_t));	  // st0 array
		push_bind(3, sizeof(vec2_t));	  // st1 array
		push_bind(4, sizeof(vec2_t));	  // st2 array
		push_attr(0, 0, vk::Format::eR32G32B32A32Sfloat);
		push_attr(1, 1, vk::Format::eR8G8B8A8Unorm);
		push_attr(2, 2, vk::Format::eR32G32Sfloat);
		push_attr(3, 3, vk::Format::eR32G32Sfloat);
		push_attr(4, 4, vk::Format::eR32G32Sfloat);
		break;

	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_MUL3_ENV:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD3_1_1_ENV:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD3_ENV:
		push_bind(0, sizeof(vec4_t));	  // xyz array
		push_bind(1, sizeof(color4ub_t)); // color array
		// push_bind( 2, sizeof( vec2_t ) );					// st0 array
		push_bind(3, sizeof(vec2_t)); // st1 array
		push_bind(4, sizeof(vec2_t)); // st2 array
		push_bind(5, sizeof(vec4_t)); // normals
		push_attr(0, 0, vk::Format::eR32G32B32A32Sfloat);
		push_attr(1, 1, vk::Format::eR8G8B8A8Unorm);
		// push_attr( 2, 2, vk::Format::eR32G32Sfloat );
		push_attr(3, 3, vk::Format::eR32G32Sfloat);
		push_attr(4, 4, vk::Format::eR32G32Sfloat);
		push_attr(5, 5, vk::Format::eR32G32B32A32Sfloat);
		break;

	case Vk_Shader_Type::TYPE_BLEND2_ADD:
	case Vk_Shader_Type::TYPE_BLEND2_MUL:
	case Vk_Shader_Type::TYPE_BLEND2_ALPHA:
	case Vk_Shader_Type::TYPE_BLEND2_ONE_MINUS_ALPHA:
	case Vk_Shader_Type::TYPE_BLEND2_MIX_ALPHA:
	case Vk_Shader_Type::TYPE_BLEND2_MIX_ONE_MINUS_ALPHA:
	case Vk_Shader_Type::TYPE_BLEND2_DST_COLOR_SRC_ALPHA:
		push_bind(0, sizeof(vec4_t));	  // xyz array
		push_bind(1, sizeof(color4ub_t)); // color0 array
		push_bind(2, sizeof(vec2_t));	  // st0 array
		push_bind(3, sizeof(vec2_t));	  // st1 array
		push_bind(6, sizeof(color4ub_t)); // color1 array
		push_attr(0, 0, vk::Format::eR32G32B32A32Sfloat);
		push_attr(1, 1, vk::Format::eR8G8B8A8Unorm);
		push_attr(2, 2, vk::Format::eR32G32Sfloat);
		push_attr(3, 3, vk::Format::eR32G32Sfloat);
		push_attr(6, 6, vk::Format::eR8G8B8A8Unorm);
		break;

	case Vk_Shader_Type::TYPE_BLEND2_ADD_ENV:
	case Vk_Shader_Type::TYPE_BLEND2_MUL_ENV:
	case Vk_Shader_Type::TYPE_BLEND2_ALPHA_ENV:
	case Vk_Shader_Type::TYPE_BLEND2_ONE_MINUS_ALPHA_ENV:
	case Vk_Shader_Type::TYPE_BLEND2_MIX_ALPHA_ENV:
	case Vk_Shader_Type::TYPE_BLEND2_MIX_ONE_MINUS_ALPHA_ENV:
	case Vk_Shader_Type::TYPE_BLEND2_DST_COLOR_SRC_ALPHA_ENV:
		push_bind(0, sizeof(vec4_t));	  // xyz array
		push_bind(1, sizeof(color4ub_t)); // color0 array
		// push_bind( 2, sizeof( vec2_t ) );					// st0 array
		push_bind(3, sizeof(vec2_t));	  // st1 array
		push_bind(5, sizeof(vec4_t));	  // normals
		push_bind(6, sizeof(color4ub_t)); // color1 array
		push_attr(0, 0, vk::Format::eR32G32B32A32Sfloat);
		push_attr(1, 1, vk::Format::eR8G8B8A8Unorm);
		// push_attr( 2, 2, vk::Format::eR32G32Sfloat );
		push_attr(3, 3, vk::Format::eR32G32Sfloat);
		push_attr(5, 5, vk::Format::eR32G32B32A32Sfloat);
		push_attr(6, 6, vk::Format::eR8G8B8A8Unorm);
		break;

	case Vk_Shader_Type::TYPE_BLEND3_ADD:
	case Vk_Shader_Type::TYPE_BLEND3_MUL:
	case Vk_Shader_Type::TYPE_BLEND3_ALPHA:
	case Vk_Shader_Type::TYPE_BLEND3_ONE_MINUS_ALPHA:
	case Vk_Shader_Type::TYPE_BLEND3_MIX_ALPHA:
	case Vk_Shader_Type::TYPE_BLEND3_MIX_ONE_MINUS_ALPHA:
	case Vk_Shader_Type::TYPE_BLEND3_DST_COLOR_SRC_ALPHA:
		push_bind(0, sizeof(vec4_t));	  // xyz array
		push_bind(1, sizeof(color4ub_t)); // color0 array
		push_bind(2, sizeof(vec2_t));	  // st0 array
		push_bind(3, sizeof(vec2_t));	  // st1 array
		push_bind(4, sizeof(vec2_t));	  // st2 array
		push_bind(6, sizeof(color4ub_t)); // color1 array
		push_bind(7, sizeof(color4ub_t)); // color2 array
		push_attr(0, 0, vk::Format::eR32G32B32A32Sfloat);
		push_attr(1, 1, vk::Format::eR8G8B8A8Unorm);
		push_attr(2, 2, vk::Format::eR32G32Sfloat);
		push_attr(3, 3, vk::Format::eR32G32Sfloat);
		push_attr(4, 4, vk::Format::eR32G32Sfloat);
		push_attr(6, 6, vk::Format::eR8G8B8A8Unorm);
		push_attr(7, 7, vk::Format::eR8G8B8A8Unorm);
		break;

	case Vk_Shader_Type::TYPE_BLEND3_ADD_ENV:
	case Vk_Shader_Type::TYPE_BLEND3_MUL_ENV:
	case Vk_Shader_Type::TYPE_BLEND3_ALPHA_ENV:
	case Vk_Shader_Type::TYPE_BLEND3_ONE_MINUS_ALPHA_ENV:
	case Vk_Shader_Type::TYPE_BLEND3_MIX_ALPHA_ENV:
	case Vk_Shader_Type::TYPE_BLEND3_MIX_ONE_MINUS_ALPHA_ENV:
	case Vk_Shader_Type::TYPE_BLEND3_DST_COLOR_SRC_ALPHA_ENV:
		push_bind(0, sizeof(vec4_t));	  // xyz array
		push_bind(1, sizeof(color4ub_t)); // color0 array
		// push_bind( 2, sizeof( vec2_t ) );					// st0 array
		push_bind(3, sizeof(vec2_t));	  // st1 array
		push_bind(4, sizeof(vec2_t));	  // st2 array
		push_bind(5, sizeof(vec4_t));	  // normals
		push_bind(6, sizeof(color4ub_t)); // color1 array
		push_bind(7, sizeof(color4ub_t)); // color2 array
		push_attr(0, 0, vk::Format::eR32G32B32A32Sfloat);
		push_attr(1, 1, vk::Format::eR8G8B8A8Unorm);
		// push_attr( 2, 2, vk::Format::eR32G32Sfloat );
		push_attr(3, 3, vk::Format::eR32G32Sfloat);
		push_attr(4, 4, vk::Format::eR32G32Sfloat);
		push_attr(5, 5, vk::Format::eR32G32B32A32Sfloat);
		push_attr(6, 6, vk::Format::eR8G8B8A8Unorm);
		push_attr(7, 7, vk::Format::eR8G8B8A8Unorm);
		break;

	default:
		ri.Error(ERR_DROP, "%s: invalid shader type - %i", __func__, static_cast<int>(def.shader_type));
		break;
	}

	vk::PipelineVertexInputStateCreateInfo vertex_input_state{{},
															  num_binds,
															  bindingsCpp,
															  num_attrs,
															  attribsCpp,
															  nullptr};

	//
	// Primitive assembly.
	//
	vk::PipelineInputAssemblyStateCreateInfo input_assembly_state{{},
																  GetTopologyByPrimitivies(def),
																  vk::False,
																  nullptr};

	//
	// Viewport.
	//
	vk::PipelineViewportStateCreateInfo viewport_state{{},
													   1,
													   nullptr, // dynamic viewport state
													   1,
													   nullptr, // dynamic scissor state
													   nullptr};

	//
	// Rasterization.
	//
	vk::PipelineRasterizationStateCreateInfo rasterization_state{{},
																 vk::False,
																 vk::False,
																 def.shader_type == Vk_Shader_Type::TYPE_DOT ? vk::PolygonMode::ePoint : ((state_bits & GLS_POLYMODE_LINE) ? vk::PolygonMode::eLine : vk::PolygonMode::eFill),
																 {},
																 vk::FrontFace::eClockwise, // Q3 defaults to clockwise vertex order
																 def.polygon_offset ? vk::True : vk::False,
																 {},
																 0.0f,
																 {},
																 def.line_width ? (float)def.line_width : 1.0f,
																 nullptr};

	GetCullModeByFaceCulling(def, rasterization_state.cullMode);

	// depth bias state
	if (def.polygon_offset)
	{
		rasterization_state.depthBiasEnable = vk::True;
		rasterization_state.depthBiasClamp = 0.0f;
#ifdef USE_REVERSED_DEPTH
		rasterization_state.depthBiasConstantFactor = -r_offsetUnits->value;
		rasterization_state.depthBiasSlopeFactor = -r_offsetFactor->value;
#else
		rasterization_state.depthBiasConstantFactor = r_offsetUnits->value;
		rasterization_state.depthBiasSlopeFactor = r_offsetFactor->value;
#endif
	}
	else
	{
		rasterization_state.depthBiasEnable = vk::False;
		rasterization_state.depthBiasClamp = 0.0f;
		rasterization_state.depthBiasConstantFactor = 0.0f;
		rasterization_state.depthBiasSlopeFactor = 0.0f;
	}

	vk::PipelineMultisampleStateCreateInfo multisample_state{{},
															 (renderPassIndex == renderPass_t::RENDER_PASS_SCREENMAP) ? vk_inst.screenMapSamples : vkSamples,
															 vk::False,
															 1.0f,
															 nullptr,
															 alphaToCoverage,
															 vk::False,
															 nullptr};

	vk::PipelineDepthStencilStateCreateInfo depth_stencil_state{{},
																(state_bits & GLS_DEPTHTEST_DISABLE) ? vk::False : vk::True,
																(state_bits & GLS_DEPTHMASK_TRUE) ? vk::True : vk::False,
																{},
																vk::False,
																(def.shadow_phase != Vk_Shadow_Phase::SHADOW_DISABLED) ? vk::True : vk::False,
																{},
																{},
																0.0f,
																1.0f,
																nullptr};

#ifdef USE_REVERSED_DEPTH
	depth_stencil_state.depthCompareOp = (state_bits & GLS_DEPTHFUNC_EQUAL) ? vk::CompareOp::eEqual : vk::CompareOp::eGreaterOrEqual;
#else
	depth_stencil_state.depthCompareOp = (state_bits & GLS_DEPTHFUNC_EQUAL) ? vk::CompareOp::eEqual : vk::CompareOp::eLessOrEqual;
#endif

	if (def.shadow_phase == Vk_Shadow_Phase::SHADOW_EDGES)
	{
		depth_stencil_state.front.failOp = vk::StencilOp::eKeep;
		depth_stencil_state.front.passOp = (def.face_culling == cullType_t::CT_FRONT_SIDED) ? vk::StencilOp::eIncrementAndClamp : vk::StencilOp::eDecrementAndClamp;
		depth_stencil_state.front.depthFailOp = vk::StencilOp::eKeep;
		depth_stencil_state.front.compareOp = vk::CompareOp::eAlways;
		depth_stencil_state.front.compareMask = 255;
		depth_stencil_state.front.writeMask = 255;
		depth_stencil_state.front.reference = 0;

		depth_stencil_state.back = depth_stencil_state.front;
	}
	else if (def.shadow_phase == Vk_Shadow_Phase::SHADOW_FS_QUAD)
	{
		depth_stencil_state.front.failOp = vk::StencilOp::eKeep;
		depth_stencil_state.front.passOp = vk::StencilOp::eKeep;
		depth_stencil_state.front.depthFailOp = vk::StencilOp::eKeep;
		depth_stencil_state.front.compareOp = vk::CompareOp::eNotEqual;
		depth_stencil_state.front.compareMask = 255;
		depth_stencil_state.front.writeMask = 255;
		depth_stencil_state.front.reference = 0;

		depth_stencil_state.back = depth_stencil_state.front;
	}

	vk::PipelineColorBlendAttachmentState attachment_blend_state = createBlendAttachmentState(state_bits, def);

	if (attachment_blend_state.blendEnable)
	{
		if (def.allow_discard && vkSamples != vk::SampleCountFlagBits::e1)
		{
			// try to reduce pixel fillrate for transparent surfaces, this yields 1..10% fps increase when multisampling in enabled
			if (attachment_blend_state.srcColorBlendFactor == vk::BlendFactor::eSrcAlpha && attachment_blend_state.dstColorBlendFactor == vk::BlendFactor::eOneMinusSrcAlpha)
			{
				frag_spec_data[7].i = 1;
			}
			else if (attachment_blend_state.srcColorBlendFactor == vk::BlendFactor::eOne && attachment_blend_state.dstColorBlendFactor == vk::BlendFactor::eOne)
			{
				frag_spec_data[7].i = 2;
			}
		}
	}

	vk::PipelineColorBlendStateCreateInfo blend_state{{},
													  vk::False,
													  vk::LogicOp::eCopy,
													  1,
													  &attachment_blend_state,
													  {0.0f, 0.0f, 0.0f, 0.0f},
													  nullptr};

	vk::PipelineDynamicStateCreateInfo dynamic_state{{},
													 arrayLen(dynamic_state_array),
													 dynamic_state_array,
													 nullptr};

	vk::GraphicsPipelineCreateInfo create_info{{},
											   shader_stages.size(),
											   shader_stages.data(),
											   &vertex_input_state,
											   &input_assembly_state,
											   nullptr,
											   &viewport_state,
											   &rasterization_state,
											   &multisample_state,
											   &depth_stencil_state,
											   &blend_state,
											   &dynamic_state,
											   (def.shader_type == Vk_Shader_Type::TYPE_DOT )? vk_inst.pipeline_layout_storage : vk_inst.pipeline_layout,
											   (renderPassIndex == renderPass_t::RENDER_PASS_SCREENMAP) ? vk_inst.render_pass.screenmap : vk_inst.render_pass.main,
											   0,
											   nullptr,
											   -1,
											   nullptr};

	vk::Pipeline resultPipeline;

#ifdef USE_VK_VALIDATION
	// VK_CHECK_ASSIGN(res, vk_inst.device.createGraphicsPipeline(vk_inst.pipelineCache, create_info));
	try
	{
		auto createGraphicsPipelineResult = vk_inst.device.createGraphicsPipeline(vk_inst.pipelineCache, create_info);

		if (static_cast<int>(createGraphicsPipelineResult.result) < 0)
		{
			ri.Error(ERR_FATAL, "Vulkan: %s returned %s", "create_pipeline -> createGraphicsPipeline", vk::to_string(createGraphicsPipelineResult.result).data());
		}
		else
		{
			resultPipeline = createGraphicsPipelineResult.value;
			SET_OBJECT_NAME(VkPipeline(&resultPipeline), va("pipeline def#%i, pass#%i", def_index, static_cast<int>(renderPassIndex)), VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT);
			SET_OBJECT_NAME(VkPipeline(&resultPipeline), "create_pipeline -> createGraphicsPipeline", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT);
		}
	}
	catch (vk::SystemError &err)
	{
		ri.Error(ERR_FATAL, "Vulkan error in function: %s, what: %s", "create_pipeline -> createGraphicsPipeline", err.what());
	}

#else
	VK_CHECK_ASSIGN(resultPipeline, vk_inst.device.createGraphicsPipeline(vk_inst.pipelineCache, create_info));

#endif
	vk_inst.pipeline_create_count++;
	return resultPipeline;
}

vk::Pipeline vk_gen_pipeline(const uint32_t index)
{
	if (index < vk_inst.pipelines_count)
	{
		VK_Pipeline_t *pipeline = vk_inst.pipelines + index;
		const uint8_t pass = static_cast<uint8_t>(vk_inst.renderPassIndex);
		if (!pipeline->handle[pass])
			pipeline->handle[pass] = create_pipeline(pipeline->def, vk_inst.renderPassIndex, index);
		return pipeline->handle[pass];
	}
	else
	{
		ri.Error( ERR_FATAL, "%s(%i): NULL pipeline", __func__, index );
		return nullptr;
	}
}

static uint32_t vk_alloc_pipeline(const Vk_Pipeline_Def &def)
{
	VK_Pipeline_t *pipeline;
	if (vk_inst.pipelines_count >= MAX_VK_PIPELINES)
	{
		ri.Error(ERR_DROP, "alloc_pipeline: MAX_VK_PIPELINES reached");
		return 0;
	}
	else
	{
		int j;
		pipeline = &vk_inst.pipelines[vk_inst.pipelines_count];
		pipeline->def = def;
		for (j = 0; j < RENDER_PASS_COUNT; j++)
		{
			pipeline->handle[j] = nullptr;
		}
		return vk_inst.pipelines_count++;
	}
}

uint32_t vk_find_pipeline_ext(const uint32_t base, const Vk_Pipeline_Def &def, bool use)
{
	const Vk_Pipeline_Def *cur_def;
	uint32_t index;

	for (index = base; index < vk_inst.pipelines_count; index++)
	{
		cur_def = &vk_inst.pipelines[index].def;
		if (memcmp(cur_def, &def, sizeof(def)) == 0)
		{
			goto found;
		}
	}

	index = vk_alloc_pipeline(def);
found:

	if (use)
		vk_gen_pipeline(index);

	return index;
}

void vk_get_pipeline_def(const uint32_t pipeline, Vk_Pipeline_Def &def)
{
	if (pipeline >= vk_inst.pipelines_count)
	{
		def = {};
	}
	else
	{
		Com_Memcpy(&def, &vk_inst.pipelines[pipeline].def, sizeof(def));
	}
}

static void get_viewport_rect(vk::Rect2D &r)
{
	if (backEnd.projection2D)
	{
		r.offset.x = 0;
		r.offset.y = 0;
		r.extent.width = vk_inst.renderWidth;
		r.extent.height = vk_inst.renderHeight;
	}
	else
	{
		r.offset.x = backEnd.viewParms.viewportX * vk_inst.renderScaleX;

		r.offset.y = vk_inst.renderHeight -
					 (backEnd.viewParms.viewportY + backEnd.viewParms.viewportHeight) * vk_inst.renderScaleY;

		r.extent.width = (float)backEnd.viewParms.viewportWidth * vk_inst.renderScaleX;
		r.extent.height = (float)backEnd.viewParms.viewportHeight * vk_inst.renderScaleY;
	}
}

static constexpr float get_min_depth(const Vk_Depth_Range depth_range)
{
	switch (depth_range)
	{
#ifdef USE_REVERSED_DEPTH
	case Vk_Depth_Range::DEPTH_RANGE_ZERO:
		return 1.0f;
	case Vk_Depth_Range::DEPTH_RANGE_ONE:
		return 0.0f;
	case Vk_Depth_Range::DEPTH_RANGE_WEAPON:
		return 0.6f;
	default:
		return 0.0f; // DEPTH_RANGE_NORMAL
#else
	case Vk_Depth_Range::DEPTH_RANGE_ZERO:
		return 0.0f;
	case Vk_Depth_Range::DEPTH_RANGE_ONE:
		return 1.0f;
	case Vk_Depth_Range::DEPTH_RANGE_WEAPON:
		return 0.0f;
	default:
		return 0.0f; // DEPTH_RANGE_NORMAL
#endif
	}
}

static constexpr float get_max_depth(const Vk_Depth_Range depth_range)
{
	switch (depth_range)
	{
#ifdef USE_REVERSED_DEPTH
	case Vk_Depth_Range::DEPTH_RANGE_ZERO:
		return 1.0f;
	case Vk_Depth_Range::DEPTH_RANGE_ONE:
		return 0.0f;
	case Vk_Depth_Range::DEPTH_RANGE_WEAPON:
		return 1.0f;
	default:
		return 1.0f; // DEPTH_RANGE_NORMAL
#else
	case Vk_Depth_Range::DEPTH_RANGE_ZERO:
		return 0.0f;
	case Vk_Depth_Range::DEPTH_RANGE_ONE:
		return 1.0f;
	case Vk_Depth_Range::DEPTH_RANGE_WEAPON:
		return 0.3f;
	default:
		return 1.0f; // DEPTH_RANGE_NORMAL
#endif
	}
}

static void get_viewport(vk::Viewport &viewport, const Vk_Depth_Range depth_range)
{
	vk::Rect2D r;

	get_viewport_rect(r);

	viewport.x = (float)r.offset.x;
	viewport.y = (float)r.offset.y;
	viewport.width = (float)r.extent.width;
	viewport.height = (float)r.extent.height;
	viewport.minDepth = get_min_depth(depth_range);
	viewport.maxDepth = get_max_depth(depth_range);
}

static void get_scissor_rect(vk::Rect2D &r)
{
	if (backEnd.viewParms.portalView != portalView_t::PV_NONE)
	{
		r.offset.x = backEnd.viewParms.scissorX;
		r.offset.y = glConfig.vidHeight - backEnd.viewParms.scissorY - backEnd.viewParms.scissorHeight;
		r.extent.width = backEnd.viewParms.scissorWidth;
		r.extent.height = backEnd.viewParms.scissorHeight;
	}
	else
	{
		get_viewport_rect(r);

		if (r.offset.x < 0)
			r.offset.x = 0;
		if (r.offset.y < 0)
			r.offset.y = 0;

		if (r.offset.x + r.extent.width > static_cast<uint32_t>(glConfig.vidWidth))
			r.extent.width = glConfig.vidWidth - r.offset.x;
		if (r.offset.y + r.extent.height > static_cast<uint32_t>(glConfig.vidHeight))
			r.extent.height = glConfig.vidHeight - r.offset.y;
	}
}

static void get_mvp_transform(float *mvp)
{
	if (backEnd.projection2D)
	{
		float mvp0 = 2.0f / glConfig.vidWidth;
		float mvp5 = 2.0f / glConfig.vidHeight;

		mvp[0] = mvp0;
		mvp[1] = 0.0f;
		mvp[2] = 0.0f;
		mvp[3] = 0.0f;
		mvp[4] = 0.0f;
		mvp[5] = mvp5;
		mvp[6] = 0.0f;
		mvp[7] = 0.0f;
#ifdef USE_REVERSED_DEPTH
		mvp[8] = 0.0f;
		mvp[9] = 0.0f;
		mvp[10] = 0.0f;
		mvp[11] = 0.0f;
		mvp[12] = -1.0f;
		mvp[13] = -1.0f;
		mvp[14] = 1.0f;
		mvp[15] = 1.0f;
#else
		mvp[8] = 0.0f;
		mvp[9] = 0.0f;
		mvp[10] = 1.0f;
		mvp[11] = 0.0f;
		mvp[12] = -1.0f;
		mvp[13] = -1.0f;
		mvp[14] = 0.0f;
		mvp[15] = 1.0f;
#endif
	}
	else
	{
		const float *p = backEnd.viewParms.projectionMatrix;
		float proj[16];
		Com_Memcpy(proj, p, 64);

		// update q3's proj matrix (opengl) to vulkan conventions: z - [0, 1] instead of [-1, 1] and invert y direction
		proj[5] = -p[5];
		// proj[10] = ( p[10] - 1.0f ) / 2.0f;
		// proj[14] = p[14] / 2.0f;
		myGlMultMatrix_SIMD(vk_world.modelview_transform, proj, mvp);
	}
}

void vk_clear_color(const vec4_t &color)
{

	vk::ClearRect clear_rect{};

	if (!vk_inst.active)
		return;

	vk::ClearAttachment attachment{vk::ImageAspectFlagBits::eColor,
								   0,
								   vk::ClearColorValue{color[0], color[1], color[2], color[3]}};

	get_scissor_rect(clear_rect.rect);
	clear_rect.baseArrayLayer = 0;
	clear_rect.layerCount = 1;

	vk_inst.cmd->command_buffer.clearAttachments(1, &attachment, 1, &clear_rect);
}

void vk_clear_depth(const bool clear_stencil)
{
	if (!vk_inst.active || vk_world.dirty_depth_attachment == 0)
		return;

	vk::ClearRect clear_rect{};
	get_scissor_rect(clear_rect.rect);
	clear_rect.baseArrayLayer = 0;
	clear_rect.layerCount = 1;

	vk::ClearAttachment attachment{
		clear_stencil && glConfig.stencilBits ? vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil
											  : vk::ImageAspectFlagBits::eDepth,
		0,
		{vk::ClearDepthStencilValue{0, 1}}};

#ifdef USE_REVERSED_DEPTH
	attachment.clearValue.depthStencil.depth = 0.0f;
#else
	attachment.clearValue.depthStencil.depth = 1.0f;
#endif

	vk_inst.cmd->command_buffer.clearAttachments(attachment, clear_rect);
}

void vk_update_mvp(const float *m)
{
	float push_constants[16]; // mvp transform

	//
	// Specify push constants.
	//
	if (m)
		Com_Memcpy(push_constants, m, sizeof(push_constants));
	else
		get_mvp_transform(push_constants);

	vk_inst.cmd->command_buffer.pushConstants(vk_inst.pipeline_layout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(push_constants), push_constants);

	vk_inst.stats.push_size += sizeof(push_constants);
}

static vk::Buffer shade_bufs[8];
static int bind_base;
static int bind_count;

static void vk_bind_index_attr(const int index)
{
	if (bind_base == -1)
	{
		bind_base = index;
		bind_count = 1;
	}
	else
	{
		bind_count = index - bind_base + 1;
	}
}

static void vk_bind_attr(const int index, const unsigned int item_size, const void *src)
{
	const uint32_t offset = PAD(vk_inst.cmd->vertex_buffer_offset, 32);
	const uint32_t size = tess.numVertexes * item_size;

	if (offset + size > vk_inst.geometry_buffer_size)
	{
		// schedule geometry buffer resize
		vk_inst.geometry_buffer_size_new = log2pad_plus(offset + size, 1);
	}
	else
	{
		vk_inst.cmd->buf_offset[index] = offset;
		Com_Memcpy(vk_inst.cmd->vertex_buffer_ptr + offset, src, size);
		vk_inst.cmd->vertex_buffer_offset = (vk::DeviceSize)offset + size;
	}

	vk_bind_index_attr(index);
}

uint32_t vk_tess_index(const uint32_t numIndexes, const void *src)
{
	const uint32_t offset = vk_inst.cmd->vertex_buffer_offset;
	const uint32_t size = numIndexes * sizeof(tess.indexes[0]);

	if (offset + size > vk_inst.geometry_buffer_size)
	{
		// schedule geometry buffer resize
		vk_inst.geometry_buffer_size_new = log2pad_plus(offset + size, 1);
		return ~0U;
	}
	else
	{
		Com_Memcpy(vk_inst.cmd->vertex_buffer_ptr + offset, src, size);
		vk_inst.cmd->vertex_buffer_offset = (vk::DeviceSize)offset + size;
		return offset;
	}
}

void vk_bind_index_buffer(const vk::Buffer &buffer, const uint32_t offset)
{
	if (vk_inst.cmd->curr_index_buffer != buffer || vk_inst.cmd->curr_index_offset != offset)
		vk_inst.cmd->command_buffer.bindIndexBuffer(buffer, offset, vk::IndexType::eUint32);

	vk_inst.cmd->curr_index_buffer = buffer;
	vk_inst.cmd->curr_index_offset = offset;
}

#ifdef USE_VBO
void vk_draw_indexed(const uint32_t indexCount, const uint32_t firstIndex)
{
	vk_inst.cmd->command_buffer.drawIndexed(indexCount, 1, firstIndex, 0, 0);
}
#endif

void vk_bind_index(void)
{
#ifdef USE_VBO
	if (tess.vboIndex)
	{
		vk_inst.cmd->num_indexes = 0;
		// qvkCmdBindIndexBuffer( vk_inst.cmd->command_buffer, vk_inst.vbo.index_buffer, tess.shader->iboOffset, VK_INDEX_TYPE_UINT32 );
		return;
	}
#endif

	vk_bind_index_ext(tess.numIndexes, tess.indexes);
}

void vk_bind_index_ext(const int numIndexes, const uint32_t *indexes)
{
	uint32_t offset = vk_tess_index(numIndexes, indexes);
	if (offset != ~0U)
	{
		vk_bind_index_buffer(vk_inst.cmd->vertex_buffer, offset);
		vk_inst.cmd->num_indexes = numIndexes;
	}
	else
	{
		// overflowed
		vk_inst.cmd->num_indexes = 0;
	}
}

void vk_bind_geometry(const uint32_t flags)
{
	// unsigned int size;
	bind_base = -1;
	bind_count = 0;

	if ((flags & (TESS_XYZ | TESS_RGBA0 | TESS_ST0 | TESS_ST1 | TESS_ST2 | TESS_NNN | TESS_RGBA1 | TESS_RGBA2)) == 0)
		return;

#ifdef USE_VBO
	if (tess.vboIndex)
	{

		shade_bufs[0] = shade_bufs[1] = shade_bufs[2] = shade_bufs[3] = shade_bufs[4] = shade_bufs[5] = shade_bufs[6] = shade_bufs[7] = vk_inst.vbo.vertex_buffer;

		if (flags & TESS_XYZ)
		{ // 0
			vk_inst.cmd->vbo_offset[0] = tess.shader->vboOffset + 0;
			vk_bind_index_attr(0);
		}

		if (flags & TESS_RGBA0)
		{ // 1
			vk_inst.cmd->vbo_offset[1] = tess.shader->stages[tess.vboStage]->rgb_offset[0];
			vk_bind_index_attr(1);
		}

		if (flags & TESS_ST0)
		{ // 2
			vk_inst.cmd->vbo_offset[2] = tess.shader->stages[tess.vboStage]->tex_offset[0];
			vk_bind_index_attr(2);
		}

		if (flags & TESS_ST1)
		{ // 3
			vk_inst.cmd->vbo_offset[3] = tess.shader->stages[tess.vboStage]->tex_offset[1];
			vk_bind_index_attr(3);
		}

		if (flags & TESS_ST2)
		{ // 4
			vk_inst.cmd->vbo_offset[4] = tess.shader->stages[tess.vboStage]->tex_offset[2];
			vk_bind_index_attr(4);
		}

		if (flags & TESS_NNN)
		{ // 5
			vk_inst.cmd->vbo_offset[5] = tess.shader->normalOffset;
			vk_bind_index_attr(5);
		}

		if (flags & TESS_RGBA1)
		{ // 6
			vk_inst.cmd->vbo_offset[6] = tess.shader->stages[tess.vboStage]->rgb_offset[1];
			vk_bind_index_attr(6);
		}

		if (flags & TESS_RGBA2)
		{ // 7
			vk_inst.cmd->vbo_offset[7] = tess.shader->stages[tess.vboStage]->rgb_offset[2];
			vk_bind_index_attr(7);
		}

		vk_inst.cmd->command_buffer.bindVertexBuffers(bind_base, bind_count, shade_bufs, vk_inst.cmd->vbo_offset + bind_base);
	}
	else
#endif // USE_VBO
	{
		shade_bufs[0] = shade_bufs[1] = shade_bufs[2] = shade_bufs[3] = shade_bufs[4] = shade_bufs[5] = shade_bufs[6] = shade_bufs[7] = vk_inst.cmd->vertex_buffer;

		if (flags & TESS_XYZ)
		{
			vk_bind_attr(0, sizeof(tess.xyz[0]), &tess.xyz[0]);
		}

		if (flags & TESS_RGBA0)
		{
			vk_bind_attr(1, sizeof(color4ub_t), tess.svars.colors[0][0].rgba);
		}

		if (flags & TESS_ST0)
		{
			vk_bind_attr(2, sizeof(vec2_t), tess.svars.texcoordPtr[0]);
		}

		if (flags & TESS_ST1)
		{
			vk_bind_attr(3, sizeof(vec2_t), tess.svars.texcoordPtr[1]);
		}

		if (flags & TESS_ST2)
		{
			vk_bind_attr(4, sizeof(vec2_t), tess.svars.texcoordPtr[2]);
		}

		if (flags & TESS_NNN)
		{
			vk_bind_attr(5, sizeof(tess.normal[0]), tess.normal);
		}

		if (flags & TESS_RGBA1)
		{
			vk_bind_attr(6, sizeof(color4ub_t), tess.svars.colors[1][0].rgba);
		}

		if (flags & TESS_RGBA2)
		{
			vk_bind_attr(7, sizeof(color4ub_t), tess.svars.colors[2][0].rgba);
		}

		vk_inst.cmd->command_buffer.bindVertexBuffers(bind_base, bind_count, shade_bufs, vk_inst.cmd->buf_offset + bind_base);
	}
}

void vk_bind_lighting(const int stage, const int bundle)
{
	bind_base = -1;
	bind_count = 0;

#ifdef USE_VBO
	if (tess.vboIndex)
	{

		shade_bufs[0] = shade_bufs[1] = shade_bufs[2] = vk_inst.vbo.vertex_buffer;

		vk_inst.cmd->vbo_offset[0] = tess.shader->vboOffset + 0;
		vk_inst.cmd->vbo_offset[1] = tess.shader->stages[stage]->tex_offset[bundle];
		vk_inst.cmd->vbo_offset[2] = tess.shader->normalOffset;

		vk_inst.cmd->command_buffer.bindVertexBuffers(0, 3, shade_bufs, vk_inst.cmd->vbo_offset);
	}
	else
#endif // USE_VBO
	{
		shade_bufs[0] = shade_bufs[1] = shade_bufs[2] = vk_inst.cmd->vertex_buffer;

		vk_bind_attr(0, sizeof(tess.xyz[0]), &tess.xyz[0]);
		vk_bind_attr(1, sizeof(vec2_t), tess.svars.texcoordPtr[bundle]);
		vk_bind_attr(2, sizeof(tess.normal[0]), tess.normal);

		vk_inst.cmd->command_buffer.bindVertexBuffers(bind_base, bind_count, shade_bufs, vk_inst.cmd->buf_offset + bind_base);
	}
}

void vk_reset_descriptor(const int index)
{
	vk_inst.cmd->descriptor_set.current[index] = nullptr;
}

void vk_update_descriptor(const int index, const vk::DescriptorSet &descriptor)
{
	if (vk_inst.cmd->descriptor_set.current[index] != descriptor)
	{
		vk_inst.cmd->descriptor_set.start = (static_cast<uint32_t>(index) < vk_inst.cmd->descriptor_set.start) ? static_cast<uint32_t>(index) : vk_inst.cmd->descriptor_set.start;
		vk_inst.cmd->descriptor_set.end = (static_cast<uint32_t>(index) > vk_inst.cmd->descriptor_set.end) ? static_cast<uint32_t>(index) : vk_inst.cmd->descriptor_set.end;
	}
	vk_inst.cmd->descriptor_set.current[index] = descriptor;
}

void vk_update_descriptor_offset(const int index, const uint32_t offset)
{
	vk_inst.cmd->descriptor_set.offset[index] = offset;
}

void vk_bind_descriptor_sets(void)
{
	uint32_t start = vk_inst.cmd->descriptor_set.start;
	if (start == ~0U)
		return;

	uint32_t offsets[2]{}, offset_count;
	uint32_t end, count, i;

	end = vk_inst.cmd->descriptor_set.end;

	offset_count = 0;
	if (/*start == VK_DESC_STORAGE ||*/ start == VK_DESC_UNIFORM)
	{ // uniform offset or storage offset
		offsets[offset_count++] = vk_inst.cmd->descriptor_set.offset[start];
	}

	count = end - start + 1;

	// fill NULL descriptor gaps
	for ( i = start + 1; i < end; i++ ) {
		if ( vk_inst.cmd->descriptor_set.current[i] == vk::DescriptorSet() ) {
			vk_inst.cmd->descriptor_set.current[i] = tr.whiteImage->descriptor;
		}
	}

	// vk_inst.cmd->command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, vk_inst.pipeline_layout, start, vk_inst.cmd->descriptor_set.current + start, offsets);
	vk_inst.cmd->command_buffer.bindDescriptorSets(
		vk::PipelineBindPoint::eGraphics,
		vk_inst.pipeline_layout,
		start,
		count,
		vk_inst.cmd->descriptor_set.current + start,
		offset_count,
		offsets);

	vk_inst.cmd->descriptor_set.end = 0;
	vk_inst.cmd->descriptor_set.start = ~0U;
}

void vk_bind_pipeline(const uint32_t pipeline)
{
	vk::Pipeline vkpipe;

	vkpipe = vk_gen_pipeline(pipeline);

	if (vkpipe != vk_inst.cmd->last_pipeline)
	{
		vk_inst.cmd->command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, vkpipe);
		vk_inst.cmd->last_pipeline = vkpipe;
	}

	vk_world.dirty_depth_attachment |= (vk_inst.pipelines[pipeline].def.state_bits & GLS_DEPTHMASK_TRUE);
}

static void vk_update_depth_range( Vk_Depth_Range depth_range )
{
	if ( vk_inst.cmd->depth_range != depth_range ) {
		vk::Rect2D scissor_rect;
		vk::Viewport viewport;

		vk_inst.cmd->depth_range = depth_range;

		get_scissor_rect(scissor_rect);

		if (memcmp(&vk_inst.cmd->scissor_rect, &scissor_rect, sizeof(scissor_rect)) != 0)
		{
			vk_inst.cmd->command_buffer.setScissor(0, scissor_rect);
			vk_inst.cmd->scissor_rect = scissor_rect;
		}

		get_viewport(viewport, depth_range);
		vk_inst.cmd->command_buffer.setViewport(0, viewport);
	}
}

void vk_draw_geometry( Vk_Depth_Range depth_range, bool indexed ) {

	if ( vk_inst.geometry_buffer_size_new ) {
		// geometry buffer overflow happened this frame
		return;
	}

	vk_bind_descriptor_sets();

	// configure pipeline's dynamic state
	vk_update_depth_range( depth_range );

	// issue draw call(s)
#ifdef USE_VBO
	if ( tess.vboIndex )
		VBO_RenderIBOItems();
	else
#endif
	if (indexed)
	{
		vk_inst.cmd->command_buffer.drawIndexed(vk_inst.cmd->num_indexes, 1, 0, 0, 0);
	}
	else
	{
		vk_inst.cmd->command_buffer.draw(tess.numVertexes, 1, 0, 0);
	}
}

void vk_draw_dot( uint32_t storage_offset )
{
	if ( vk_inst.geometry_buffer_size_new ) {
		// geometry buffer overflow happened this frame
		return;
	}

	vk_inst.cmd->command_buffer.bindDescriptorSets(
		vk::PipelineBindPoint::eGraphics,     // pipeline bind point
		vk_inst.pipeline_layout_storage,           // pipeline layout
		VK_DESC_STORAGE,                      // first set
		vk_inst.storage.descriptor,                // descriptor set (vk::DescriptorSet, not pointer)
		storage_offset                        // dynamic offset (uint32_t)
	);

	// configure pipeline's dynamic state
	vk_update_depth_range( Vk_Depth_Range::DEPTH_RANGE_NORMAL );

	vk_inst.cmd->command_buffer.draw(tess.numVertexes, 1, 0, 0);

}


static void vk_begin_render_pass(const vk::RenderPass &renderPass, const vk::Framebuffer &frameBuffer, const bool clearValues, const uint32_t width, const uint32_t height)
{
	vk::ClearValue clear_values[3]{};

	// Begin render pass.

	vk::RenderPassBeginInfo render_pass_begin_info{renderPass,
												   frameBuffer,
												   {{0, 0}, {width, height}},
												   0,
												   nullptr,
												   nullptr};

	if (clearValues)
	{
		// attachments layout:
		// [0] - resolve/color/presentation
		// [1] - depth/stencil
		// [2] - multisampled color, optional
#ifndef USE_REVERSED_DEPTH
		clear_values[1].depthStencil.depth = 1.0;
#endif
		render_pass_begin_info.clearValueCount = vk_inst.msaaActive ? 3 : 2;
		render_pass_begin_info.pClearValues = clear_values;

		vk_world.dirty_depth_attachment = 0;
	}

	vk_inst.cmd->command_buffer.beginRenderPass(render_pass_begin_info, vk::SubpassContents::eInline);
	vk_inst.cmd->last_pipeline = VK_NULL_HANDLE;
	vk_inst.cmd->depth_range = Vk_Depth_Range::DEPTH_RANGE_COUNT;
}

void vk_begin_main_render_pass(void)
{
	vk::Framebuffer &frameBuffer = vk_inst.framebuffers.main[vk_inst.cmd->swapchain_image_index];

	vk_inst.renderPassIndex = renderPass_t::RENDER_PASS_MAIN;

	vk_inst.renderWidth = glConfig.vidWidth;
	vk_inst.renderHeight = glConfig.vidHeight;

	// vk_inst.renderScaleX = (float)vk_inst.renderWidth / (float)glConfig.vidWidth;
	// vk_inst.renderScaleY = (float)vk_inst.renderHeight / (float)glConfig.vidHeight;
	vk_inst.renderScaleX = vk_inst.renderScaleY = 1.0f;

	vk_begin_render_pass(vk_inst.render_pass.main, frameBuffer, true, vk_inst.renderWidth, vk_inst.renderHeight);
}

void vk_begin_post_bloom_render_pass(void)
{
	vk::Framebuffer &frameBuffer = vk_inst.framebuffers.main[vk_inst.cmd->swapchain_image_index];

	vk_inst.renderPassIndex = renderPass_t::RENDER_PASS_POST_BLOOM;

	vk_inst.renderWidth = glConfig.vidWidth;
	vk_inst.renderHeight = glConfig.vidHeight;

	// vk_inst.renderScaleX = (float)vk_inst.renderWidth / (float)glConfig.vidWidth;
	// vk_inst.renderScaleY = (float)vk_inst.renderHeight / (float)glConfig.vidHeight;
	vk_inst.renderScaleX = vk_inst.renderScaleY = 1.0f;

	vk_begin_render_pass(vk_inst.render_pass.post_bloom, frameBuffer, false, vk_inst.renderWidth, vk_inst.renderHeight);
}

void vk_begin_bloom_extract_render_pass(void)
{
	vk::Framebuffer &frameBuffer = vk_inst.framebuffers.bloom_extract;

	// vk_inst.renderPassIndex = RENDER_PASS_BLOOM_EXTRACT; // doesn't matter, we will use dedicated pipelines

	vk_inst.renderWidth = gls.captureWidth;
	vk_inst.renderHeight = gls.captureHeight;

	// vk_inst.renderScaleX = (float)vk_inst.renderWidth / (float)glConfig.vidWidth;
	// vk_inst.renderScaleY = (float)vk_inst.renderHeight / (float)glConfig.vidHeight;
	vk_inst.renderScaleX = vk_inst.renderScaleY = 1.0f;

	vk_begin_render_pass(vk_inst.render_pass.bloom_extract, frameBuffer, false, vk_inst.renderWidth, vk_inst.renderHeight);
}

void vk_begin_blur_render_pass(const uint32_t index)
{
	vk::Framebuffer &frameBuffer = vk_inst.framebuffers.blur[index];

	// vk_inst.renderPassIndex = RENDER_PASS_BLOOM_EXTRACT; // doesn't matter, we will use dedicated pipelines

	vk_inst.renderWidth = gls.captureWidth / (2 << (index / 2));
	vk_inst.renderHeight = gls.captureHeight / (2 << (index / 2));

	// vk_inst.renderScaleX = (float)vk_inst.renderWidth / (float)glConfig.vidWidth;
	// vk_inst.renderScaleY = (float)vk_inst.renderHeight / (float)glConfig.vidHeight;
	vk_inst.renderScaleX = vk_inst.renderScaleY = 1.0f;

	vk_begin_render_pass(vk_inst.render_pass.blur[index], frameBuffer, false, vk_inst.renderWidth, vk_inst.renderHeight);
}

static void vk_begin_screenmap_render_pass(void)
{
	vk::Framebuffer &frameBuffer = vk_inst.framebuffers.screenmap;

	vk_inst.renderPassIndex = renderPass_t::RENDER_PASS_SCREENMAP;

	vk_inst.renderWidth = vk_inst.screenMapWidth;
	vk_inst.renderHeight = vk_inst.screenMapHeight;

	vk_inst.renderScaleX = (float)vk_inst.renderWidth / (float)glConfig.vidWidth;
	vk_inst.renderScaleY = (float)vk_inst.renderHeight / (float)glConfig.vidHeight;

	vk_begin_render_pass(vk_inst.render_pass.screenmap, frameBuffer, true, vk_inst.renderWidth, vk_inst.renderHeight);
}

void vk_end_render_pass(void)
{
	vk_inst.cmd->command_buffer.endRenderPass();

	//	vk_inst.renderPassIndex = renderPass_t::RENDER_PASS_MAIN;
}

static bool vk_find_screenmap_drawsurfs(void)
{
	const void *curCmd = &backEndData->commands.cmds;
	const drawBufferCommand_t *db_cmd;
	const drawSurfsCommand_t *ds_cmd;

	for (;;)
	{
		curCmd = PADP(curCmd, sizeof(void *));
		switch (static_cast<renderCommand_t>(*(const int *)curCmd))
		{
		case renderCommand_t::RC_DRAW_BUFFER:
			db_cmd = (const drawBufferCommand_t *)curCmd;
			curCmd = (const void *)(db_cmd + 1);
			break;
		case renderCommand_t::RC_DRAW_SURFS:
			ds_cmd = (const drawSurfsCommand_t *)curCmd;
			return ds_cmd->refdef.needScreenMap;
		default:
			return false;
		}
	}
}

#ifndef UINT64_MAX
#define UINT64_MAX 0xFFFFFFFFFFFFFFFFULL
#endif

void vk_begin_frame(void)
{
	vk::Result res;

	if (vk_inst.frame_count++) // might happen during stereo rendering
		return;

#ifdef USE_UPLOAD_QUEUE
	vk_flush_staging_buffer( true );
#endif

	vk_inst.cmd = &vk_inst.tess[vk_inst.cmd_index];

	if ( vk_inst.cmd->waitForFence ) {
		vk_inst.cmd->waitForFence = false;

		res = vk_inst.device.waitForFences(vk_inst.cmd->rendering_finished_fence, vk::False, 1e10);
		if (res != vk::Result::eSuccess)
		{
			if (res == vk::Result::eErrorDeviceLost)
			{
				// silently discard previous command buffer
				ri.Printf(PRINT_WARNING, "Vulkan: %s returned %s", "vkWaitForFences", vk::to_string(res).data());
			}
			else
			{
				ri.Error(ERR_FATAL, "Vulkan: %s returned %s", "vkWaitForFences", vk::to_string(res).data());
			}
		}
		VK_CHECK(vk_inst.device.resetFences(vk_inst.cmd->rendering_finished_fence));
	}

		if (!ri.CL_IsMinimized() && !vk_inst.cmd->swapchain_image_acquired)
		{
		bool retry = false;
_retry:

			res = vk_inst.device.acquireNextImageKHR(vk_inst.swapchain,
													1 * 1000000000ULL,
													vk_inst.cmd->image_acquired,
													nullptr,
													&vk_inst.cmd->swapchain_image_index);
			// when running via RDP: "Application has already acquired the maximum number of images (0x2)"
			// probably caused by "device lost" errors
			if (res < vk::Result::eSuccess)
			{
				if (res == vk::Result::eErrorOutOfDateKHR && retry == false)
				{
					// swapchain re-creation needed
					retry = true;
					vk_restart_swapchain(__func__, res);
					goto _retry;
				}
				else
				{
					ri.Error(ERR_FATAL, "vkAcquireNextImageKHR returned %s", vk::to_string(res).data());
				}
			}
		vk_inst.cmd->swapchain_image_acquired = true;
	}

	//VK_CHECK(vk_inst.device.resetFences(vk_inst.cmd->rendering_finished_fence));

	vk::CommandBufferBeginInfo begin_info{vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
										  nullptr,
										  nullptr};

	VK_CHECK(vk_inst.cmd->command_buffer.begin(begin_info));

	// Ensure visibility of geometry buffers writes.
	// record_buffer_memory_barrier( vk_inst.cmd->command_buffer, vk_inst.cmd->vertex_buffer, vk_inst.cmd->geometry_buffer_size, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT );

#if 0
	//  add explicit layout transition dependency
	if (vk_inst.fboActive)
	{
		record_image_layout_transition(vk_inst.cmd->command_buffer, vk_inst.color_image, vk::ImageAspectFlagBits::eColor,
									   vk::ImageLayout::eUndefined, vk::ImageLayout::eShaderReadOnlyOptimal);
	}
	else
	{
		record_image_layout_transition(vk_inst.cmd->command_buffer, vk_inst.swapchain_images[vk_inst.swapchain_image_index], vk::ImageAspectFlagBits::eColor,
									   vk::ImageLayout::eUndefined, vk::ImageLayout::ePresentSrcKHR);
	}
#endif

	if (vk_inst.cmd->vertex_buffer_offset > vk_inst.stats.vertex_buffer_max)
	{
		vk_inst.stats.vertex_buffer_max = vk_inst.cmd->vertex_buffer_offset;
	}

	if (vk_inst.stats.push_size > vk_inst.stats.push_size_max)
	{
		vk_inst.stats.push_size_max = vk_inst.stats.push_size;
	}

	vk_inst.cmd->last_pipeline = nullptr;

	backEnd.screenMapDone = false;

	if (vk_find_screenmap_drawsurfs())
	{
		vk_begin_screenmap_render_pass();
	}
	else
	{
		vk_begin_main_render_pass();
	}

	// dynamic vertex buffer layout
	vk_inst.cmd->uniform_read_offset = 0;
	vk_inst.cmd->vertex_buffer_offset = 0;
	Com_Memset(vk_inst.cmd->buf_offset, 0, sizeof(vk_inst.cmd->buf_offset));
	// std::fill_n(vk_inst.cmd->buf_offset, sizeof(vk_inst.cmd->buf_offset), vk::DeviceSize{0});
	Com_Memset(vk_inst.cmd->vbo_offset, 0, sizeof(vk_inst.cmd->vbo_offset));
	// std::fill_n(vk_inst.cmd->vbo_offset, sizeof(vk_inst.cmd->vbo_offset), vk::DeviceSize{0});
	vk_inst.cmd->curr_index_buffer = nullptr;
	vk_inst.cmd->curr_index_offset = 0;
	vk_inst.cmd->num_indexes = 0;

	vk_inst.cmd->descriptor_set = decltype(vk_inst.cmd->descriptor_set){};
	vk_inst.cmd->descriptor_set.start = ~0U;
	// vk_inst.cmd->descriptor_set.end = 0;

	// Com_Memset(&vk_inst.cmd->scissor_rect, 0, sizeof(vk_inst.cmd->scissor_rect));
	vk_inst.cmd->scissor_rect = vk::Rect2D{};

	// vk_update_descriptor(VK_DESC_TEXTURE0, tr.whiteImage->descriptor);
	// vk_update_descriptor(VK_DESC_TEXTURE1, tr.whiteImage->descriptor);
	// if (vk_inst.maxBoundDescriptorSets >= VK_DESC_COUNT)
	// {
	// 	vk_update_descriptor(VK_DESC_TEXTURE2, tr.whiteImage->descriptor);
	// }

	// other stats
	vk_inst.stats.push_size = 0;
}

static void vk_resize_geometry_buffer(void)
{
	int i;

	vk_end_render_pass();

	VK_CHECK(vk_inst.cmd->command_buffer.end());

	vk_inst.cmd->command_buffer.reset();

	vk_wait_idle();

	vk_release_geometry_buffers();

	vk_create_geometry_buffers(vk_inst.geometry_buffer_size_new);
	vk_inst.geometry_buffer_size_new = 0;

	for (i = 0; i < NUM_COMMAND_BUFFERS; i++)
		vk_update_uniform_descriptor(vk_inst.tess[i].uniform_descriptor, vk_inst.tess[i].vertex_buffer);

	ri.Printf(PRINT_DEVELOPER, "...geometry buffer resized to %iK\n", (int)(vk_inst.geometry_buffer_size / 1024));
}

void vk_end_frame(void)
{
#ifdef USE_UPLOAD_QUEUE
    std::array<vk::Semaphore, 2> waits{};
    std::array<vk::Semaphore, 2> signals{};
    constexpr std::array<vk::PipelineStageFlags, 2> wait_dst_stage_mask = {
        vk::PipelineStageFlagBits::eColorAttachmentOutput,
        vk::PipelineStageFlagBits::eColorAttachmentOutput
    };
#else
    constexpr vk::PipelineStageFlags wait_dst_stage_mask =
        vk::PipelineStageFlagBits::eColorAttachmentOutput;
#endif

	if (vk_inst.frame_count == 0)
		return;

	vk_inst.frame_count = 0;

	if (vk_inst.geometry_buffer_size_new)
	{
		vk_resize_geometry_buffer();
		// issue: one frame may be lost during video recording
		// solution: re-record all commands again? (might be complicated though)
		return;
	}

	if (vk_inst.fboActive)
	{
		vk_inst.cmd->last_pipeline = nullptr; // do not restore clobbered descriptors in vk_bloom()

		if (r_bloom->integer)
		{
			vk_bloom();
		}

		if (backEnd.screenshotMask && vk_inst.capture.image)
		{
			vk_end_render_pass();

			// render to capture FBO
			vk_begin_render_pass(vk_inst.render_pass.capture, vk_inst.framebuffers.capture, false, gls.captureWidth, gls.captureHeight);
			vk_inst.cmd->command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, vk_inst.capture_pipeline);

			vk_inst.cmd->command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, vk_inst.pipeline_layout_post_process, 0, vk_inst.color_descriptor, nullptr);

			vk_inst.cmd->command_buffer.draw(4, 1, 0, 0);
		}

		if (!ri.CL_IsMinimized())
		{
			vk_end_render_pass();

			vk_inst.renderWidth = gls.windowWidth;
			vk_inst.renderHeight = gls.windowHeight;

			vk_inst.renderScaleX = 1.0;
			vk_inst.renderScaleY = 1.0;

			vk_begin_render_pass(vk_inst.render_pass.gamma, vk_inst.framebuffers.gamma[vk_inst.cmd->swapchain_image_index], false, vk_inst.renderWidth, vk_inst.renderHeight);
			vk_inst.cmd->command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, vk_inst.gamma_pipeline);

			vk_inst.cmd->command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, vk_inst.pipeline_layout_post_process, 0, vk_inst.color_descriptor, nullptr);

			vk_inst.cmd->command_buffer.draw(4, 1, 0, 0);
		}
	}

	vk_end_render_pass();

	VK_CHECK(vk_inst.cmd->command_buffer.end());

	vk::SubmitInfo submit_info{0,
							   nullptr,
							   nullptr,
							   1,
							   &vk_inst.cmd->command_buffer,
							   0,
							   nullptr};

	if (!ri.CL_IsMinimized())
	{
#ifdef USE_UPLOAD_QUEUE
		if ( vk_inst.image_uploaded != vk::Semaphore() ) {
			waits[0] = vk_inst.cmd->image_acquired;
			waits[1] = vk_inst.image_uploaded;
			submit_info.waitSemaphoreCount = 2;
			submit_info.pWaitSemaphores = &waits[0];
			submit_info.pWaitDstStageMask = &wait_dst_stage_mask[0];
			signals[0] = vk_inst.swapchain_rendering_finished[ vk_inst.cmd->swapchain_image_index ];
			signals[1] = vk_inst.cmd->rendering_finished2;
			submit_info.signalSemaphoreCount = 2;
			submit_info.pSignalSemaphores = &signals[0];

			vk_inst.rendering_finished = vk_inst.cmd->rendering_finished2;
			vk_inst.image_uploaded = vk::Semaphore();
		} else if ( vk_inst.rendering_finished != vk::Semaphore() ) {
			waits[0] = vk_inst.cmd->image_acquired;
			waits[1] = vk_inst.rendering_finished;
			submit_info.waitSemaphoreCount = 2;
			submit_info.pWaitSemaphores = &waits[0];
			submit_info.pWaitDstStageMask = &wait_dst_stage_mask[0];
			signals[0] = vk_inst.swapchain_rendering_finished[ vk_inst.cmd->swapchain_image_index ];
			signals[1] = vk_inst.cmd->rendering_finished2;
			submit_info.signalSemaphoreCount = 2;
			submit_info.pSignalSemaphores = &signals[0];

			vk_inst.rendering_finished = vk_inst.cmd->rendering_finished2;
		} else {
			submit_info.waitSemaphoreCount = 1;
			submit_info.pWaitSemaphores = &vk_inst.cmd->image_acquired;
			submit_info.pWaitDstStageMask = &wait_dst_stage_mask[0];
			submit_info.signalSemaphoreCount = 1;
			submit_info.pSignalSemaphores = &vk_inst.swapchain_rendering_finished[ vk_inst.cmd->swapchain_image_index ];
		}
#else
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = &vk_inst.cmd->image_acquired;
		submit_info.pWaitDstStageMask = &wait_dst_stage_mask;
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = &vk_inst.cmd->rendering_finished;
		submit_info.pSignalSemaphores = &vk_inst.swapchain_rendering_finished[ vk_inst.cmd->swapchain_image_index ];
#endif
	} else {
		submit_info.waitSemaphoreCount = 0;
		submit_info.pWaitSemaphores = nullptr;
		submit_info.pWaitDstStageMask = nullptr;
		submit_info.signalSemaphoreCount = 0;
		submit_info.pSignalSemaphores = nullptr;
	}

	VK_CHECK(vk_inst.queue.submit(submit_info, vk_inst.cmd->rendering_finished_fence));
	vk_inst.cmd->waitForFence = true;

	// presentation may take undefined time to complete, we can't measure it in a reliable way
	backEnd.pc.msec = ri.Milliseconds() - backEnd.pc.msec;

	vk_inst.renderPassIndex = renderPass_t::RENDER_PASS_MAIN;
	// vk_present_frame();
}

void vk_present_frame(void)
{
	if (ri.CL_IsMinimized() || !vk_inst.cmd->swapchain_image_acquired )
		return;

	if ( !vk_inst.cmd->waitForFence ) {
		// nothing has been submitted this frame due to geometry buffer overflow?
		return;
	}

	vk::PresentInfoKHR present_info{1,
									&vk_inst.swapchain_rendering_finished[ vk_inst.cmd->swapchain_image_index ],
									1,
									&vk_inst.swapchain,
									&vk_inst.cmd->swapchain_image_index,
									nullptr,
									nullptr};

	vk_inst.cmd->swapchain_image_acquired = false;

#ifdef USE_VK_VALIDATION
	try
	{
		vk::Result res = vk_inst.queue.presentKHR(present_info);
		switch (res)
		{
		case vk::Result::eSuccess:
			break;
		case vk::Result::eSuboptimalKHR:
		case vk::Result::eErrorOutOfDateKHR:
			// swapchain re-creation needed
			vk_restart_swapchain(__func__, res);
			break;
		case vk::Result::eErrorDeviceLost:
			// we can ignore that
			ri.Printf(PRINT_DEVELOPER, "vkQueuePresentKHR: device lost\n");
			break;
		default:
			// or we don't
			ri.Error(ERR_FATAL, "vkQueuePresentKHR returned %s", vk::to_string(res).data());
		}
	}
	catch (vk::OutOfDateKHRError &err)
	{
		vk_restart_swapchain(__func__, vk::Result::eErrorOutOfDateKHR);
	}
	catch (vk::SystemError &e)
	{
		ri.Error(ERR_FATAL, "vkQueuePresentKHR failed: %s", e.what());
	}
#else
	vk::Result res = vk_inst.queue.presentKHR(present_info);

	switch (res)
	{
	case vk::Result::eSuccess:
		break;
	case vk::Result::eSuboptimalKHR:
	case vk::Result::eErrorOutOfDateKHR:
		// swapchain re-creation needed
		vk_restart_swapchain(__func__, res);
		break;
	case vk::Result::eErrorDeviceLost:
		// we can ignore that
		ri.Printf(PRINT_DEVELOPER, "vkQueuePresentKHR: device lost\n");
		break;
	default:
		// or we don't
		ri.Error(ERR_FATAL, "vkQueuePresentKHR returned %s", vk::to_string(res).data());
	}
#endif

	// pickup next command buffer for rendering
	vk_inst.cmd_index++;
	vk_inst.cmd_index %= NUM_COMMAND_BUFFERS;
	vk_inst.cmd = &vk_inst.tess[vk_inst.cmd_index];
}

static constexpr bool is_bgr(const vk::Format format)
{
	switch (format)
	{
	case vk::Format::eB8G8R8A8Unorm:
	case vk::Format::eB8G8R8A8Snorm:
	case vk::Format::eB8G8R8A8Uint:
	case vk::Format::eB8G8R8A8Sint:
	case vk::Format::eB8G8R8A8Srgb:
	case vk::Format::eB4G4R4A4UnormPack16:
		return true;
	default:
		return false;
	}
}

void vk_read_pixels(byte *buffer, const uint32_t width, const uint32_t height)
{
	vk::MemoryPropertyFlags memory_flags;

	vk::Image srcImage;
	vk::ImageLayout srcImageLayout;
	byte *buffer_ptr;
	byte *data;
	uint32_t pixel_width;
	uint32_t i, n;
	bool invalidate_ptr;

	VK_CHECK(vk_inst.device.waitForFences(vk_inst.cmd->rendering_finished_fence, vk::False, 1e12));

	if (vk_inst.fboActive)
	{
		if (vk_inst.capture.image)
		{
			// dedicated capture buffer
			srcImageLayout = vk::ImageLayout::eTransferSrcOptimal;
			srcImage = vk_inst.capture.image;
		}
		else
		{
			srcImageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
			srcImage = vk_inst.color_image;
		}
	}
	else
	{
		srcImageLayout = vk::ImageLayout::ePresentSrcKHR;
		srcImage = vk_inst.swapchain_images[vk_inst.cmd->swapchain_image_index];
	}

	// Com_Memset(&desc, 0, sizeof(desc));

	// Create image in host visible memory to serve as a destination for framebuffer pixels.
	vk::ImageCreateInfo desc{{},
							 vk::ImageType::e2D,
							 vk_inst.capture_format,
							 {width, height, 1},
							 1,
							 1,
							 vk::SampleCountFlagBits::e1,
							 vk::ImageTiling::eLinear,
							 vk::ImageUsageFlagBits::eTransferDst,
							 vk::SharingMode::eExclusive,
							 0,
							 nullptr,
							 vk::ImageLayout::eUndefined,
							 nullptr};

	vk::Image dstImage;
	VK_CHECK_ASSIGN(dstImage, vk_inst.device.createImage(desc));

	vk::MemoryRequirements memory_requirements = vk_inst.device.getImageMemoryRequirements(dstImage);
	// host_cached bit is desirable for fast reads
	vk::MemoryPropertyFlags memory_reqs = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostCached;
	vk::MemoryAllocateInfo alloc_info = vk::MemoryAllocateInfo(memory_requirements.size, find_memory_type2(memory_requirements.memoryTypeBits, memory_reqs, &memory_flags));

	if (alloc_info.memoryTypeIndex == static_cast<uint32_t>(~0U))
	{
		memory_reqs = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCached;
		alloc_info.memoryTypeIndex = find_memory_type2(memory_requirements.memoryTypeBits, memory_reqs, &memory_flags);
		if (alloc_info.memoryTypeIndex == ~0U)
		{
			memory_reqs = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
			alloc_info.memoryTypeIndex = find_memory_type2(memory_requirements.memoryTypeBits, memory_reqs, &memory_flags);
			if (alloc_info.memoryTypeIndex == ~0U)
			{
				ri.Error(ERR_FATAL, "%s(): failed to find matching memory type for image capture", __func__);
			}
		}
	}

	invalidate_ptr = !(memory_flags & vk::MemoryPropertyFlagBits::eHostCoherent);

	vk::DeviceMemory memory;
	VK_CHECK_ASSIGN(memory, vk_inst.device.allocateMemory(alloc_info));
	VK_CHECK(vk_inst.device.bindImageMemory(dstImage, memory, 0));

	vk::CommandBuffer command_buffer = begin_command_buffer();

	if (srcImageLayout != vk::ImageLayout::eTransferSrcOptimal)
	{
		record_image_layout_transition(command_buffer, srcImage,
									   vk::ImageAspectFlagBits::eColor,
									   srcImageLayout,
									   vk::ImageLayout::eTransferSrcOptimal);
	}

	record_image_layout_transition(command_buffer, dstImage,
								   vk::ImageAspectFlagBits::eColor,
								   vk::ImageLayout::eUndefined,
								   vk::ImageLayout::eTransferDstOptimal);

	// end_command_buffer( command_buffer );

	// command_buffer = begin_command_buffer();

	if (vk_inst.blitEnabled)
	{
		vk::ImageBlit region{
			// srcSubresource
			{vk::ImageAspectFlagBits::eColor, 0, 0, 1},
			// srcOffsets[0]
			{vk::Offset3D{0, 0, 0}, vk::Offset3D{(int32_t)width, (int32_t)height, 1}},
			// dstSubresource (initialized to the same as srcSubresource)
			{vk::ImageAspectFlagBits::eColor, 0, 0, 1},
			// dstOffsets[0]
			{vk::Offset3D{0, 0, 0}, vk::Offset3D{(int32_t)width, (int32_t)height, 1}}};

		command_buffer.blitImage(srcImage, vk::ImageLayout::eTransferSrcOptimal, dstImage, vk::ImageLayout::eTransferDstOptimal, region, vk::Filter::eNearest);
	}
	else
	{
		vk::ImageCopy region{
			// srcSubresource
			{vk::ImageAspectFlagBits::eColor, 0, 0, 1},
			// srcOffset
			vk::Offset3D{0, 0, 0},
			// dstSubresource (initialized to the same as srcSubresource)
			{vk::ImageAspectFlagBits::eColor, 0, 0, 1},
			// dstOffset
			vk::Offset3D{0, 0, 0},
			// extent
			vk::Extent3D{width, height, 1}};

		command_buffer.copyImage(srcImage, vk::ImageLayout::eTransferSrcOptimal, dstImage, vk::ImageLayout::eTransferDstOptimal, region);
	}

	end_command_buffer(command_buffer, __func__);

	// Copy data from destination image to memory buffer.
	vk::ImageSubresource subresource{vk::ImageAspectFlagBits::eColor, 0, 0};

	vk::SubresourceLayout layout = vk_inst.device.getImageSubresourceLayout(dstImage, subresource);

	void *mappedMemory;
	VK_CHECK_ASSIGN(mappedMemory, vk_inst.device.mapMemory(memory, 0, vk::WholeSize));

	data = static_cast<uint8_t *>(mappedMemory);

	if (invalidate_ptr)
	{
		vk::MappedMemoryRange range(memory, 0, vk::WholeSize);
		VK_CHECK(vk_inst.device.invalidateMappedMemoryRanges(range));
	}

	data += layout.offset;

	switch (vk_inst.capture_format)
	{
	case vk::Format::eB4G4R4A4UnormPack16:
		pixel_width = 2;
		break;
	case vk::Format::eR16G16B16A16Unorm:
		pixel_width = 8;
		break;
	default:
		pixel_width = 4;
		break;
	}

	buffer_ptr = buffer + width * (height - 1) * 3;
	for (i = 0; i < height; i++)
	{
		switch (pixel_width)
		{
		case 2:
		{
			uint16_t *src = (uint16_t *)data;
			for (n = 0; n < width; n++)
			{
				buffer_ptr[n * 3 + 0] = ((src[n] >> 12) & 0xF) << 4;
				buffer_ptr[n * 3 + 1] = ((src[n] >> 8) & 0xF) << 4;
				buffer_ptr[n * 3 + 2] = ((src[n] >> 4) & 0xF) << 4;
			}
		}
		break;

		case 4:
		{
			for (n = 0; n < width; n++)
			{
				Com_Memcpy(&buffer_ptr[n * 3], &data[n * 4], 3);
				// buffer_ptr[n*3+0] = data[n*4+0];
				// buffer_ptr[n*3+1] = data[n*4+1];
				// buffer_ptr[n*3+2] = data[n*4+2];
			}
		}
		break;

		case 8:
		{
			const uint16_t *src = (uint16_t *)data;
			for (n = 0; n < width; n++)
			{
				buffer_ptr[n * 3 + 0] = src[n * 4 + 0] >> 8;
				buffer_ptr[n * 3 + 1] = src[n * 4 + 1] >> 8;
				buffer_ptr[n * 3 + 2] = src[n * 4 + 2] >> 8;
			}
		}
		break;
		}
		buffer_ptr -= width * 3;
		data += layout.rowPitch;
	}

	if (is_bgr(vk_inst.capture_format))
	{
		buffer_ptr = buffer;
		for (i = 0; i < width * height; i++)
		{
			byte tmp = buffer_ptr[0];
			buffer_ptr[0] = buffer_ptr[2];
			buffer_ptr[2] = tmp;
			buffer_ptr += 3;
		}
	}

	vk_inst.device.destroyImage(dstImage);
	vk_inst.device.freeMemory(memory);

	// restore previous layout
	if (srcImageLayout != vk::ImageLayout::eTransferSrcOptimal)
	{
		command_buffer = begin_command_buffer();

		record_image_layout_transition(command_buffer, srcImage,
									   vk::ImageAspectFlagBits::eColor,
									   vk::ImageLayout::eTransferSrcOptimal,
									   srcImageLayout);

		end_command_buffer(command_buffer, "restore layout");
	}
}

bool vk_bloom(void)
{
	uint32_t i;

	if (vk_inst.renderPassIndex == renderPass_t::RENDER_PASS_SCREENMAP)
	{
		return false;
	}

	if (backEnd.doneBloom || !backEnd.doneSurfaces || !vk_inst.fboActive)
	{
		return false;
	}

	vk_end_render_pass(); // end main

	// bloom extraction
	vk_begin_bloom_extract_render_pass();
	vk_inst.cmd->command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, vk_inst.bloom_extract_pipeline);
	vk_inst.cmd->command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, vk_inst.pipeline_layout_post_process, 0, vk_inst.color_descriptor, nullptr);
	vk_inst.cmd->command_buffer.draw(4, 1, 0, 0);
	vk_end_render_pass();

	for (i = 0; i < VK_NUM_BLOOM_PASSES * 2; i += 2)
	{
		// horizontal blur
		vk_begin_blur_render_pass(i + 0);
		vk_inst.cmd->command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, vk_inst.blur_pipeline[i + 0]);
		vk_inst.cmd->command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, vk_inst.pipeline_layout_post_process, 0, vk_inst.bloom_image_descriptor[i + 0], nullptr);
		vk_inst.cmd->command_buffer.draw(4, 1, 0, 0);
		vk_end_render_pass();

		// vectical blur
		vk_begin_blur_render_pass(i + 1);
		vk_inst.cmd->command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, vk_inst.blur_pipeline[i + 1]);
		vk_inst.cmd->command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, vk_inst.pipeline_layout_post_process, 0, vk_inst.bloom_image_descriptor[i + 1], nullptr);
		vk_inst.cmd->command_buffer.draw(4, 1, 0, 0);
		vk_end_render_pass();
#if 0
		// horizontal blur
		vk_begin_blur_render_pass(i + 0);
		qvkCmdBindPipeline(vk_inst.cmd->command_buffer, vk::PipelineBindPoint::eGraphics, vk_inst.blur_pipeline[i + 0]);
		qvkCmdBindDescriptorSets(vk_inst.cmd->command_buffer, vk::PipelineBindPoint::eGraphics, vk_inst.pipeline_layout_post_process, 0, 1, &vk_inst.bloom_image_descriptor[i + 2], 0, nullptr);
		qvkCmdDraw(vk_inst.cmd->command_buffer, 4, 1, 0, 0);
		vk_end_render_pass();

		// vectical blur
		vk_begin_blur_render_pass(i + 1);
		qvkCmdBindPipeline(vk_inst.cmd->command_buffer, vk::PipelineBindPoint::eGraphics, vk_inst.blur_pipeline[i + 1]);
		qvkCmdBindDescriptorSets(vk_inst.cmd->command_buffer, vk::PipelineBindPoint::eGraphics, vk_inst.pipeline_layout_post_process, 0, 1, &vk_inst.bloom_image_descriptor[i + 1], 0, nullptr);
		qvkCmdDraw(vk_inst.cmd->command_buffer, 4, 1, 0, 0);
		vk_end_render_pass();
#endif
	}

	vk_begin_post_bloom_render_pass(); // begin post-bloom
	{
		vk::DescriptorSet dset[VK_NUM_BLOOM_PASSES];

		for (i = 0; i < VK_NUM_BLOOM_PASSES; i++)
		{
			dset[i] = vk_inst.bloom_image_descriptor[(i + 1) * 2];
		}

		// blend downscaled buffers to main fbo
		vk_inst.cmd->command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, vk_inst.bloom_blend_pipeline);
		vk_inst.cmd->command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, vk_inst.pipeline_layout_blend, 0, dset, nullptr);
		vk_inst.cmd->command_buffer.draw(4, 1, 0, 0);
	}

	// invalidate pipeline state cache
	// vk_inst.cmd->last_pipeline = nullptr;

	if (vk_inst.cmd->last_pipeline)
	{
		// restore last pipeline
		vk_inst.cmd->command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, vk_inst.cmd->last_pipeline);

		vk_update_mvp(nullptr);

		// force depth range and viewport/scissor updates
		vk_inst.cmd->depth_range = Vk_Depth_Range::DEPTH_RANGE_COUNT;

		// restore clobbered descriptor sets
		for (i = 0; i < VK_NUM_BLOOM_PASSES; i++)
		{
			if (vk_inst.cmd->descriptor_set.current[i])
			{
				if (i == VK_DESC_UNIFORM /*|| i == VK_DESC_STORAGE*/)
					vk_inst.cmd->command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, vk_inst.pipeline_layout, i, vk_inst.cmd->descriptor_set.current[i], vk_inst.cmd->descriptor_set.offset[i]);
				else
					vk_inst.cmd->command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, vk_inst.pipeline_layout, i, vk_inst.cmd->descriptor_set.current[i], nullptr);
			}
		}
	}

	backEnd.doneBloom = true;

	return true;
}

#include "string_operations.hpp"
#include "vk.hpp"
#include <stdexcept>
#include <algorithm>
#include "q_math.hpp"
#include "utils.hpp"

#if defined(_DEBUG)
#if defined(_WIN32)
#define USE_VK_VALIDATION
#include <windows.h> // for win32 debug callback
#endif

#endif

#include <unordered_map>
#include <numeric>

constexpr int MIN_SWAPCHAIN_IMAGES_IMM = 3;
constexpr int MIN_SWAPCHAIN_IMAGES_FIFO = 3;
constexpr int MIN_SWAPCHAIN_IMAGES_FIFO_0 = 4;
constexpr int MIN_SWAPCHAIN_IMAGES_MAILBOX = 3;
constexpr int VERTEX_BUFFER_SIZE = (4 * 1024 * 1024);
constexpr int IMAGE_CHUNK_SIZE = (32 * 1024 * 1024);

constexpr VkSampleCountFlagBits convertToVkSampleCountFlagBits(int sampleCountInt)
{
	switch (sampleCountInt)
	{
	case 1:
		return VK_SAMPLE_COUNT_1_BIT;
	case 2:
		return VK_SAMPLE_COUNT_2_BIT;
	case 4:
		return VK_SAMPLE_COUNT_4_BIT;
	case 8:
		return VK_SAMPLE_COUNT_8_BIT;
	// Add more cases for other sample counts as needed
	default:
		// Handle unsupported sample count
		// For example, throw an exception or return a default value
		ri.Printf(PRINT_DEVELOPER, "Wrong vk_inst.simple count, use 1 bit: %d\n", sampleCountInt);
		return VK_SAMPLE_COUNT_1_BIT;
	}
}

static VkSampleCountFlagBits vkSamples = VK_SAMPLE_COUNT_1_BIT;
static int vkMaxSamples = VK_SAMPLE_COUNT_1_BIT;

static VkInstance vk_instance = VK_NULL_HANDLE;
static vk::Instance vk_instanceCpp = VK_NULL_HANDLE;
static VkSurfaceKHR vk_surface = VK_NULL_HANDLE;
static vk::SurfaceKHR vk_surfaceCpp = VK_NULL_HANDLE;

#ifndef NDEBUG
VkDebugReportCallbackEXT vk_debug_callback = VK_NULL_HANDLE;
#endif

//
// Vulkan API functions used by the renderer.
//
static PFN_vkCreateInstance qvkCreateInstance;
static PFN_vkEnumerateInstanceExtensionProperties qvkEnumerateInstanceExtensionProperties;

static PFN_vkCreateDevice qvkCreateDevice;
static PFN_vkDestroyInstance qvkDestroyInstance;
static PFN_vkEnumerateDeviceExtensionProperties qvkEnumerateDeviceExtensionProperties;
static PFN_vkEnumeratePhysicalDevices qvkEnumeratePhysicalDevices;
static PFN_vkGetDeviceProcAddr qvkGetDeviceProcAddr;
static PFN_vkGetPhysicalDeviceFeatures qvkGetPhysicalDeviceFeatures;
static PFN_vkGetPhysicalDeviceFormatProperties qvkGetPhysicalDeviceFormatProperties;
static PFN_vkGetPhysicalDeviceMemoryProperties qvkGetPhysicalDeviceMemoryProperties;
static PFN_vkGetPhysicalDeviceProperties qvkGetPhysicalDeviceProperties;
static PFN_vkGetPhysicalDeviceQueueFamilyProperties qvkGetPhysicalDeviceQueueFamilyProperties;
static PFN_vkDestroySurfaceKHR qvkDestroySurfaceKHR;
static PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR qvkGetPhysicalDeviceSurfaceCapabilitiesKHR;
static PFN_vkGetPhysicalDeviceSurfaceFormatsKHR qvkGetPhysicalDeviceSurfaceFormatsKHR;
static PFN_vkGetPhysicalDeviceSurfacePresentModesKHR qvkGetPhysicalDeviceSurfacePresentModesKHR;
static PFN_vkGetPhysicalDeviceSurfaceSupportKHR qvkGetPhysicalDeviceSurfaceSupportKHR;
#ifdef USE_VK_VALIDATION
static PFN_vkCreateDebugReportCallbackEXT qvkCreateDebugReportCallbackEXT;
static PFN_vkDestroyDebugReportCallbackEXT qvkDestroyDebugReportCallbackEXT;
#endif
static PFN_vkAllocateCommandBuffers qvkAllocateCommandBuffers;
static PFN_vkAllocateDescriptorSets qvkAllocateDescriptorSets;
static PFN_vkAllocateMemory qvkAllocateMemory;
static PFN_vkBeginCommandBuffer qvkBeginCommandBuffer;
static PFN_vkBindBufferMemory qvkBindBufferMemory;
static PFN_vkBindImageMemory qvkBindImageMemory;
static PFN_vkCmdBeginRenderPass qvkCmdBeginRenderPass;
static PFN_vkCmdBindDescriptorSets qvkCmdBindDescriptorSets;
static PFN_vkCmdBindIndexBuffer qvkCmdBindIndexBuffer;
static PFN_vkCmdBindPipeline qvkCmdBindPipeline;
static PFN_vkCmdBindVertexBuffers qvkCmdBindVertexBuffers;
static PFN_vkCmdBlitImage qvkCmdBlitImage;
static PFN_vkCmdClearAttachments qvkCmdClearAttachments;
static PFN_vkCmdCopyBuffer qvkCmdCopyBuffer;
static PFN_vkCmdCopyBufferToImage qvkCmdCopyBufferToImage;
static PFN_vkCmdCopyImage qvkCmdCopyImage;
static PFN_vkCmdDraw qvkCmdDraw;
static PFN_vkCmdDrawIndexed qvkCmdDrawIndexed;
static PFN_vkCmdEndRenderPass qvkCmdEndRenderPass;
static PFN_vkCmdNextSubpass qvkCmdNextSubpass;
static PFN_vkCmdPipelineBarrier qvkCmdPipelineBarrier;
static PFN_vkCmdPushConstants qvkCmdPushConstants;
static PFN_vkCmdSetDepthBias qvkCmdSetDepthBias;
static PFN_vkCmdSetScissor qvkCmdSetScissor;
static PFN_vkCmdSetViewport qvkCmdSetViewport;
static PFN_vkCreateBuffer qvkCreateBuffer;
static PFN_vkCreateCommandPool qvkCreateCommandPool;
static PFN_vkCreateDescriptorPool qvkCreateDescriptorPool;
static PFN_vkCreateDescriptorSetLayout qvkCreateDescriptorSetLayout;
static PFN_vkCreateFence qvkCreateFence;
static PFN_vkCreateFramebuffer qvkCreateFramebuffer;
static PFN_vkCreateGraphicsPipelines qvkCreateGraphicsPipelines;
static PFN_vkCreateImage qvkCreateImage;
static PFN_vkCreateImageView qvkCreateImageView;
static PFN_vkCreatePipelineLayout qvkCreatePipelineLayout;
static PFN_vkCreatePipelineCache qvkCreatePipelineCache;
static PFN_vkCreateRenderPass qvkCreateRenderPass;
static PFN_vkCreateSampler qvkCreateSampler;
static PFN_vkCreateSemaphore qvkCreateSemaphore;
static PFN_vkCreateShaderModule qvkCreateShaderModule;
static PFN_vkDestroyBuffer qvkDestroyBuffer;
static PFN_vkDestroyCommandPool qvkDestroyCommandPool;
static PFN_vkDestroyDescriptorPool qvkDestroyDescriptorPool;
static PFN_vkDestroyDescriptorSetLayout qvkDestroyDescriptorSetLayout;
static PFN_vkDestroyDevice qvkDestroyDevice;
static PFN_vkDestroyFence qvkDestroyFence;
static PFN_vkDestroyFramebuffer qvkDestroyFramebuffer;
static PFN_vkDestroyImage qvkDestroyImage;
static PFN_vkDestroyImageView qvkDestroyImageView;
static PFN_vkDestroyPipeline qvkDestroyPipeline;
static PFN_vkDestroyPipelineCache qvkDestroyPipelineCache;
static PFN_vkDestroyPipelineLayout qvkDestroyPipelineLayout;
static PFN_vkDestroyRenderPass qvkDestroyRenderPass;
static PFN_vkDestroySampler qvkDestroySampler;
static PFN_vkDestroySemaphore qvkDestroySemaphore;
static PFN_vkDestroyShaderModule qvkDestroyShaderModule;
static PFN_vkDeviceWaitIdle qvkDeviceWaitIdle;
static PFN_vkEndCommandBuffer qvkEndCommandBuffer;
static PFN_vkFlushMappedMemoryRanges qvkFlushMappedMemoryRanges;
static PFN_vkFreeCommandBuffers qvkFreeCommandBuffers;
static PFN_vkFreeDescriptorSets qvkFreeDescriptorSets;
static PFN_vkFreeMemory qvkFreeMemory;
static PFN_vkGetBufferMemoryRequirements qvkGetBufferMemoryRequirements;
static PFN_vkGetDeviceQueue qvkGetDeviceQueue;
static PFN_vkGetImageMemoryRequirements qvkGetImageMemoryRequirements;
static PFN_vkGetImageSubresourceLayout qvkGetImageSubresourceLayout;
static PFN_vkInvalidateMappedMemoryRanges qvkInvalidateMappedMemoryRanges;
static PFN_vkMapMemory qvkMapMemory;
static PFN_vkQueueSubmit qvkQueueSubmit;
static PFN_vkQueueWaitIdle qvkQueueWaitIdle;
static PFN_vkResetCommandBuffer qvkResetCommandBuffer;
static PFN_vkResetDescriptorPool qvkResetDescriptorPool;
static PFN_vkResetFences qvkResetFences;
static PFN_vkUnmapMemory qvkUnmapMemory;
static PFN_vkUpdateDescriptorSets qvkUpdateDescriptorSets;
static PFN_vkWaitForFences qvkWaitForFences;
static PFN_vkAcquireNextImageKHR qvkAcquireNextImageKHR;
static PFN_vkCreateSwapchainKHR qvkCreateSwapchainKHR;
static PFN_vkDestroySwapchainKHR qvkDestroySwapchainKHR;
static PFN_vkGetSwapchainImagesKHR qvkGetSwapchainImagesKHR;
static PFN_vkQueuePresentKHR qvkQueuePresentKHR;

static PFN_vkGetBufferMemoryRequirements2KHR qvkGetBufferMemoryRequirements2KHR;
static PFN_vkGetImageMemoryRequirements2KHR qvkGetImageMemoryRequirements2KHR;

static PFN_vkDebugMarkerSetObjectNameEXT qvkDebugMarkerSetObjectNameEXT;

////////////////////////////////////////////////////////////////////////////

// forward declaration
VkPipeline create_pipeline(const Vk_Pipeline_Def &def, renderPass_t renderPassIndex);

static uint32_t find_memory_type(uint32_t memory_type_bits, vk::MemoryPropertyFlags properties)
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

static uint32_t find_memory_type2(uint32_t memory_type_bits, VkMemoryPropertyFlags properties, VkMemoryPropertyFlags *outprops)
{
	VkPhysicalDeviceMemoryProperties memory_properties;
	uint32_t i;

	qvkGetPhysicalDeviceMemoryProperties(vk_inst.physical_device, &memory_properties);

	for (i = 0; i < memory_properties.memoryTypeCount; i++)
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

struct PresentModeMapping
{
	vk::PresentModeKHR mode;
	std::string_view name;
};

// Define the mapping array
constexpr std::array<PresentModeMapping, 4> pmode_map = {{{vk::PresentModeKHR::eImmediate, "IMMEDIATE"},
														  {vk::PresentModeKHR::eMailbox, "MAILBOX"},
														  {vk::PresentModeKHR::eFifo, "FIFO"},
														  {vk::PresentModeKHR::eFifoRelaxed, "FIFO_RELAXED"}}};

static std::string_view pmode_to_str(vk::PresentModeKHR mode)
{
	static char buf[32];

	for (const auto &mapping : pmode_map)
	{
		if (mapping.mode == mode)
		{
			return mapping.name;
		}
	}
	sprintf(buf, "mode#%x", mode);
	return buf;
}

#define CASE_STR(x) \
	case (x):       \
		return #x

struct FormatMapping
{
	vk::Format format;
	std::string_view name;
};

constexpr std::array<FormatMapping, 22> format_map = {
	// color formats
	{{vk::Format::eR5G5B5A1UnormPack16, "VK_FORMAT_R5G5B5A1_UNORM_PACK16"},
	 {vk::Format::eB5G5R5A1UnormPack16, "VK_FORMAT_B5G5R5A1_UNORM_PACK16"},
	 {vk::Format::eR5G6B5UnormPack16, "VK_FORMAT_R5G6B5_UNORM_PACK16"},
	 {vk::Format::eB5G6R5UnormPack16, "VK_FORMAT_B5G6R5_UNORM_PACK16"},
	 {vk::Format::eB8G8R8A8Srgb, "VK_FORMAT_B8G8R8A8_SRGB"},
	 {vk::Format::eR8G8B8A8Srgb, "VK_FORMAT_R8G8B8A8_SRGB"},
	 {vk::Format::eB8G8R8A8Snorm, "VK_FORMAT_B8G8R8A8_SNORM"},
	 {vk::Format::eR8G8B8A8Snorm, "VK_FORMAT_R8G8B8A8_SNORM"},
	 {vk::Format::eB8G8R8A8Unorm, "VK_FORMAT_B8G8R8A8_UNORM"},
	 {vk::Format::eR8G8B8A8Unorm, "VK_FORMAT_R8G8B8A8_UNORM"},
	 {vk::Format::eB4G4R4A4UnormPack16, "VK_FORMAT_B4G4R4A4_UNORM_PACK16"},
	 {vk::Format::eR4G4B4A4UnormPack16, "VK_FORMAT_R4G4B4A4_UNORM_PACK16"},
	 {vk::Format::eR16G16B16A16Unorm, "VK_FORMAT_R16G16B16A16_UNORM"},
	 {vk::Format::eA2B10G10R10UnormPack32, "VK_FORMAT_A2B10G10R10_UNORM_PACK32"},
	 {vk::Format::eA2R10G10B10UnormPack32, "VK_FORMAT_A2R10G10B10_UNORM_PACK32"},
	 {vk::Format::eB10G11R11UfloatPack32, "VK_FORMAT_B10G11R11_UFLOAT_PACK32"},
	 // depth formats
	 {vk::Format::eD16Unorm, "VK_FORMAT_D16_UNORM"},
	 {vk::Format::eD16UnormS8Uint, "VK_FORMAT_D16_UNORM_S8_UINT"},
	 {vk::Format::eX8D24UnormPack32, "VK_FORMAT_X8_D24_UNORM_PACK32"},
	 {vk::Format::eD24UnormS8Uint, "VK_FORMAT_D24_UNORM_S8_UINT"},
	 {vk::Format::eD32Sfloat, "VK_FORMAT_D32_SFLOAT"},
	 {vk::Format::eD32SfloatS8Uint, "VK_FORMAT_D32_SFLOAT_S8_UINT"}}};

std::string_view vk_format_string(VkFormat inc_format)
{
	vk::Format format = static_cast<vk::Format>(inc_format);
	static char buf[16];

	for (const auto &mapping : format_map)
	{
		if (mapping.format == format)
		{
			return mapping.name;
		}
	}
	Com_sprintf(buf, sizeof(buf), "#%i", inc_format);
	return buf;
}

struct ResultMapping
{
	vk::Result result;
	std::string_view name;
};

constexpr std::array<ResultMapping, 37> result_map = {{{vk::Result::eSuccess, "VK_SUCCESS"},
													   {vk::Result::eNotReady, "VK_NOT_READY"},
													   {vk::Result::eTimeout, "VK_TIMEOUT"},
													   {vk::Result::eEventSet, "VK_EVENT_SET"},
													   {vk::Result::eEventReset, "VK_EVENT_RESET"},
													   {vk::Result::eIncomplete, "VK_INCOMPLETE"},
													   {vk::Result::eErrorOutOfHostMemory, "VK_ERROR_OUT_OF_HOST_MEMORY"},
													   {vk::Result::eErrorOutOfDeviceMemory, "VK_ERROR_OUT_OF_DEVICE_MEMORY"},
													   {vk::Result::eErrorInitializationFailed, "VK_ERROR_INITIALIZATION_FAILED"},
													   {vk::Result::eErrorDeviceLost, "VK_ERROR_DEVICE_LOST"},
													   {vk::Result::eErrorMemoryMapFailed, "VK_ERROR_MEMORY_MAP_FAILED"},
													   {vk::Result::eErrorLayerNotPresent, "VK_ERROR_LAYER_NOT_PRESENT"},
													   {vk::Result::eErrorExtensionNotPresent, "VK_ERROR_EXTENSION_NOT_PRESENT"},
													   {vk::Result::eErrorFeatureNotPresent, "VK_ERROR_FEATURE_NOT_PRESENT"},
													   {vk::Result::eErrorIncompatibleDriver, "VK_ERROR_INCOMPATIBLE_DRIVER"},
													   {vk::Result::eErrorTooManyObjects, "VK_ERROR_TOO_MANY_OBJECTS"},
													   {vk::Result::eErrorFormatNotSupported, "VK_ERROR_FORMAT_NOT_SUPPORTED"},
													   {vk::Result::eErrorFragmentedPool, "VK_ERROR_FRAGMENTED_POOL"},
													   {vk::Result::eErrorUnknown, "VK_ERROR_UNKNOWN"},
													   {vk::Result::eErrorOutOfPoolMemory, "VK_ERROR_OUT_OF_POOL_MEMORY"},
													   {vk::Result::eErrorInvalidExternalHandle, "VK_ERROR_INVALID_EXTERNAL_HANDLE"},
													   {vk::Result::eErrorFragmentation, "VK_ERROR_FRAGMENTATION"},
													   {vk::Result::eErrorInvalidOpaqueCaptureAddress, "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS"},
													   {vk::Result::eErrorSurfaceLostKHR, "VK_ERROR_SURFACE_LOST_KHR"},
													   {vk::Result::eErrorNativeWindowInUseKHR, "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR"},
													   {vk::Result::eSuboptimalKHR, "VK_SUBOPTIMAL_KHR"},
													   {vk::Result::eErrorOutOfDateKHR, "VK_ERROR_OUT_OF_DATE_KHR"},
													   {vk::Result::eErrorIncompatibleDisplayKHR, "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR"},
													   {vk::Result::eErrorValidationFailedEXT, "VK_ERROR_VALIDATION_FAILED_EXT"},
													   {vk::Result::eErrorInvalidShaderNV, "VK_ERROR_INVALID_SHADER_NV"},
													   {vk::Result::eErrorInvalidDrmFormatModifierPlaneLayoutEXT, "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT"},
													   {vk::Result::eErrorNotPermittedEXT, "VK_ERROR_NOT_PERMITTED_EXT"},
#if defined(VK_USE_PLATFORM_WIN32_KHR)
													   {vk::Result::eErrorFullScreenExclusiveModeLostEXT, "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT"},
#endif
													   {vk::Result::eThreadIdleKHR, "VK_THREAD_IDLE_KHR"},
													   {vk::Result::eThreadDoneKHR, "VK_THREAD_DONE_KHR"},
													   {vk::Result::eOperationDeferredKHR, "VK_OPERATION_DEFERRED_KHR"},
													   {vk::Result::eOperationNotDeferredKHR, "VK_OPERATION_NOT_DEFERRED_KHR"},
													   {vk::Result::ePipelineCompileRequiredEXT, "VK_PIPELINE_COMPILE_REQUIRED_EXT"}}};

static std::string_view vk_result_string(VkResult inc_code)
{
	vk::Result code = static_cast<vk::Result>(inc_code);
	static char buffer[32];

	for (const auto &mapping : result_map)
	{
		if (mapping.result == code)
		{
			return mapping.name;
		}
	}

	sprintf(buffer, "code %i", inc_code);
	return buffer;
}

#undef CASE_STR

#ifdef VULKAN_HPP_NO_EXCEPTIONS

inline void vkCheckFunctionCall(VkResult res, const char *funcName)
{
	if (res < 0)
	{
		ri.Error(ERR_FATAL, "Vulkan: %s returned %s", funcName, vk_result_string(res));
	}
}

#define VK_CHECK(function_call) \
	{                           \
		(function_call);        \
	}

#else

// Wrapper function that simply calls the Vulkan function
#define VK_CHECK(function_call) \
	{                           \
		function_call;          \
	}

#endif

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

	for ( i = 1; i < arrayLen( compositeFlags ); i++ ) {
		if ( flags & compositeFlags[i] ) {
			return compositeFlags[i];
		}
	}

	return compositeFlags[0];
}
*/

static VkCommandBuffer begin_command_buffer(void)
{
	VkCommandBuffer command_buffer;
	vk::CommandBufferAllocateInfo alloc_info{
		vk_inst.command_pool,
		vk::CommandBufferLevel::ePrimary,
		1,
		nullptr};

	vk::Device vkDeviceHpp(vk_inst.device);

	// VK_CHECK(command_buffer = vkDeviceHpp.allocateCommandBuffers(alloc_info)[0]);

	VkCommandBufferAllocateInfo aaa = static_cast<VkCommandBufferAllocateInfo>(alloc_info);

	VK_CHECK(qvkAllocateCommandBuffers(vk_inst.device, &aaa, &command_buffer));

	vk::CommandBufferBeginInfo begin_info{
		vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
		nullptr,
		nullptr};

	// VK_CHECK(command_buffer.begin(begin_info));
	VkCommandBufferBeginInfo bbb = static_cast<VkCommandBufferBeginInfo>(begin_info);
	VK_CHECK(qvkBeginCommandBuffer(command_buffer, &bbb));

	return command_buffer;
}

static void end_command_buffer(vk::CommandBuffer command_buffer)
{
	vk::CommandBuffer cmdbuf[1];

	cmdbuf[0] = command_buffer;

	VK_CHECK(qvkEndCommandBuffer(command_buffer));

	vk::SubmitInfo submit_info{0,
							   nullptr,
							   nullptr,
							   1,
							   cmdbuf,
							   0,
							   nullptr,
							   nullptr};

	VK_CHECK(vk::Queue(vk_inst.queue).submit(1, &submit_info, VK_NULL_HANDLE));
	// VK_CHECK(qvkQueueSubmit(vk_inst.queue, 1, &submit_info, VK_NULL_HANDLE));
	vk::Queue(vk_inst.queue).waitIdle();
	// VK_CHECK(qvkQueueWaitIdle(vk_inst.queue));
	vk_inst.device.freeCommandBuffers(vk_inst.command_pool,
									  1,
									  cmdbuf);

	// qvkFreeCommandBuffers(vk_inst.device, vk_inst.command_pool, 1, cmdbuf);
}

static void record_image_layout_transition(vk::CommandBuffer command_buffer, vk::Image image, vk::ImageAspectFlags image_aspect_flags, vk::ImageLayout old_layout, vk::ImageLayout new_layout)
{
	vk::ImageMemoryBarrier barrier{{},
								   {},
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

	vk::PipelineStageFlagBits src_stage, dst_stage;

	switch (old_layout)
	{
	case vk::ImageLayout::eUndefined:
		src_stage = vk::PipelineStageFlagBits::eTopOfPipe;
		barrier.srcAccessMask = vk::AccessFlagBits::eNone;
		break;
	case vk::ImageLayout::eTransferDstOptimal:
		src_stage = vk::PipelineStageFlagBits::eTransfer;
		barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
		break;
	case vk::ImageLayout::eTransferSrcOptimal:
		src_stage = vk::PipelineStageFlagBits::eTransfer;
		barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
		break;
	case vk::ImageLayout::eShaderReadOnlyOptimal:
		src_stage = vk::PipelineStageFlagBits::eFragmentShader;
		barrier.srcAccessMask = vk::AccessFlagBits::eShaderRead;
		break;
	case vk::ImageLayout::ePresentSrcKHR:
		src_stage = vk::PipelineStageFlagBits::eTransfer;
		barrier.srcAccessMask = vk::AccessFlagBits::eNone;
		break;
	default:
		ri.Error(ERR_DROP, "unsupported old layout %i", old_layout);
		src_stage = vk::PipelineStageFlagBits::eAllCommands;
		barrier.srcAccessMask = vk::AccessFlagBits::eNone;
		break;
	}

	switch (new_layout)
	{
	case vk::ImageLayout::eColorAttachmentOptimal:
		dst_stage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
		barrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
		break;
	case vk::ImageLayout::eDepthStencilAttachmentOptimal:
		dst_stage = vk::PipelineStageFlagBits::eEarlyFragmentTests;
		barrier.dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
		break;
	case vk::ImageLayout::ePresentSrcKHR:
		dst_stage = vk::PipelineStageFlagBits::eTransfer;
		barrier.dstAccessMask = vk::AccessFlagBits::eNone;
		break;
	case vk::ImageLayout::eTransferSrcOptimal:
		dst_stage = vk::PipelineStageFlagBits::eTransfer;
		barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;
		break;
	case vk::ImageLayout::eTransferDstOptimal:
		dst_stage = vk::PipelineStageFlagBits::eTransfer;
		barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
		break;
	case vk::ImageLayout::eShaderReadOnlyOptimal:
		dst_stage = vk::PipelineStageFlagBits::eFragmentShader;
		barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eInputAttachmentRead;
		break;
	default:
		ri.Error(ERR_DROP, "unsupported new layout %i", new_layout);
		dst_stage = vk::PipelineStageFlagBits::eAllCommands;
		barrier.dstAccessMask = vk::AccessFlagBits::eNone;
		break;
	}

	command_buffer.pipelineBarrier(src_stage,
								   dst_stage,
								   {},
								   0,
								   nullptr,
								   0,
								   nullptr,
								   1,
								   &barrier);
	// qvkCmdPipelineBarrier(command_buffer, src_stage, dst_stage, 0, 0, NULL, 0, NULL, 1, &barrier);
}

// debug markers
#define SET_OBJECT_NAME(obj, objName, objType) vk_set_object_name((uint64_t)(obj), (objName), (objType))

static void vk_set_object_name(uint64_t obj, const char *objName, VkDebugReportObjectTypeEXT objType)
{
	if (qvkDebugMarkerSetObjectNameEXT && obj)
	{
		VkDebugMarkerObjectNameInfoEXT info;
		info.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
		info.pNext = nullptr;
		info.objectType = objType;
		info.object = obj;
		info.pObjectName = objName;
		qvkDebugMarkerSetObjectNameEXT(vk_inst.device, &info);
	}
}

static void vk_create_swapchain(vk::PhysicalDevice physical_device, vk::Device device, VkSurfaceKHR surface, vk::SurfaceFormatKHR surface_format, VkSwapchainKHR *swapchain)
{

	vk::SurfaceCapabilitiesKHR surface_caps;
	vk::Extent2D image_extent;
	uint32_t present_mode_count, i;
	vk::PresentModeKHR present_mode;
	// vk::PresentModeKHR *present_modes;
	uint32_t image_count;
	bool mailbox_supported = false;
	bool immediate_supported = false;
	bool fifo_relaxed_supported = false;
	int v;

	VK_CHECK(surface_caps = physical_device.getSurfaceCapabilitiesKHR(surface));

	// VK_CHECK(qvkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &surface_caps));

	image_extent = surface_caps.currentExtent;
	if (image_extent.width == 0xffffffff && image_extent.height == 0xffffffff)
	{
		image_extent.width = MIN(surface_caps.maxImageExtent.width, MAX(surface_caps.minImageExtent.width, (uint32_t)glConfig.vidWidth));
		image_extent.height = MIN(surface_caps.maxImageExtent.height, MAX(surface_caps.minImageExtent.height, (uint32_t)glConfig.vidHeight));
	}

	vk_inst.fastSky = true;

	if (!vk_inst.fboActive)
	{
		// vk::ImageUsageFlagBits::eTransferDst is required by image clear operations.
		if ((surface_caps.supportedUsageFlags & vk::ImageUsageFlagBits::eTransferDst) == vk::ImageUsageFlags{})
		{
			vk_inst.fastSky = false;
			ri.Printf(PRINT_WARNING, "vk::ImageUsageFlagBits::eTransferDst is not supported by the swapchain\n");
		}

		// vk::ImageUsageFlagBits::eTransferSrc is required in order to take screenshots.
		if ((surface_caps.supportedUsageFlags & vk::ImageUsageFlagBits::eTransferSrc) == vk::ImageUsageFlags{})
		{
			ri.Error(ERR_FATAL, "create_swapchain: vk::ImageUsageFlagBits::eTransferSrc is not supported by the swapchain");
		}
	}

	// determine present mode and swapchain image count
	auto present_modes = physical_device.getSurfacePresentModesKHR(surface);
	present_mode_count = present_modes.size();
	// VK_CHECK(qvkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, nullptr));

	// present_modes = (VkPresentModeKHR *)ri.Malloc(present_mode_count * sizeof(VkPresentModeKHR));
	// VK_CHECK(qvkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, present_modes));

	ri.Printf(PRINT_ALL, "...presentation modes:");
	for (i = 0; i < present_mode_count; i++)
	{
		ri.Printf(PRINT_ALL, " %s", pmode_to_str(present_modes[i]).data());
		if (present_modes[i] == vk::PresentModeKHR::eMailbox)
			mailbox_supported = true;
		else if (present_modes[i] == vk::PresentModeKHR::eImmediate)
			immediate_supported = true;
		else if (present_modes[i] == vk::PresentModeKHR::eFifoRelaxed)
			fifo_relaxed_supported = true;
	}
	ri.Printf(PRINT_ALL, "\n");

	// ri.Free(present_modes);

	if ((v = ri.Cvar_VariableIntegerValue("r_swapInterval")) != 0)
	{
		if (v == 2 && mailbox_supported)
			present_mode = vk::PresentModeKHR::eMailbox;
		else if (fifo_relaxed_supported)
			present_mode = vk::PresentModeKHR::eFifoRelaxed;
		else
			present_mode = vk::PresentModeKHR::eFifo;
		image_count = MAX(MIN_SWAPCHAIN_IMAGES_FIFO, surface_caps.minImageCount);
	}
	else
	{
		if (immediate_supported)
		{
			present_mode = vk::PresentModeKHR::eImmediate;
			image_count = MAX(MIN_SWAPCHAIN_IMAGES_IMM, surface_caps.minImageCount);
		}
		else if (mailbox_supported)
		{
			present_mode = vk::PresentModeKHR::eMailbox;
			image_count = MAX(MIN_SWAPCHAIN_IMAGES_MAILBOX, surface_caps.minImageCount);
		}
		else if (fifo_relaxed_supported)
		{
			present_mode = vk::PresentModeKHR::eFifoRelaxed;
			image_count = MAX(MIN_SWAPCHAIN_IMAGES_FIFO, surface_caps.minImageCount);
		}
		else
		{
			present_mode = vk::PresentModeKHR::eFifo;
			image_count = MAX(MIN_SWAPCHAIN_IMAGES_FIFO, surface_caps.minImageCount);
		}
	}

	if (image_count < 2)
	{
		image_count = 2;
	}

	if (surface_caps.maxImageCount == 0 && present_mode == vk::PresentModeKHR::eFifo)
	{
		image_count = MAX(MIN_SWAPCHAIN_IMAGES_FIFO_0, surface_caps.minImageCount);
	}
	else if (surface_caps.maxImageCount > 0)
	{
		image_count = MIN(MIN(image_count, surface_caps.maxImageCount), MAX_SWAPCHAIN_IMAGES);
	}

	ri.Printf(PRINT_ALL, "...selected presentation mode: %s, image count: %i\n", pmode_to_str(present_mode).data(), image_count);

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
									VK_TRUE,
									VK_NULL_HANDLE,
									nullptr};

	if (!vk_inst.fboActive)
	{
		desc.imageUsage |= vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc;
	}

	auto swapchainCreateInfoKHR = static_cast<VkSwapchainCreateInfoKHR>(desc);

	VK_CHECK(qvkCreateSwapchainKHR(device, &swapchainCreateInfoKHR, nullptr, swapchain));

	auto swapchainImages = vk_inst.device.getSwapchainImagesKHR(vk_inst.swapchain);
	vk_inst.swapchain_image_count = MIN(swapchainImages.size(), MAX_SWAPCHAIN_IMAGES);

	std::copy(swapchainImages.begin(), swapchainImages.end(), vk_inst.swapchain_images);

	for (i = 0; i < vk_inst.swapchain_image_count; i++)
	{
		vk::ImageViewCreateInfo view{{},
									 vk_inst.swapchain_images[i],
									 vk::ImageViewType::e2D,
									 vk::Format(vk_inst.present_format.format),
									 {},
									 {vk::ImageAspectFlagBits::eColor,
									  0,
									  1,
									  0,
									  1},
									 nullptr};

		vk_inst.swapchain_image_views[i] = vk_inst.device.createImageView(view);

		SET_OBJECT_NAME(vk_inst.swapchain_images[i], va("swapchain image %i", i), VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT);
		SET_OBJECT_NAME(vk_inst.swapchain_image_views[i], va("swapchain image %i", i), VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT);
	}

	if (vk_inst.initSwapchainLayout != VK_IMAGE_LAYOUT_UNDEFINED)
	{
		VkCommandBuffer command_buffer = begin_command_buffer();

		for (i = 0; i < vk_inst.swapchain_image_count; i++)
		{
			record_image_layout_transition(command_buffer, vk_inst.swapchain_images[i],
										   vk::ImageAspectFlagBits::eColor,
										   vk::ImageLayout::eUndefined, vk::ImageLayout(vk_inst.initSwapchainLayout));
		}

		end_command_buffer(command_buffer);
	}
}

static void vk_create_render_passes(void)
{
	vk::AttachmentDescription attachments[3]; // color | depth | msaa color
	vk::AttachmentReference colorResolveRef;
	vk::AttachmentReference colorRef0;
	vk::AttachmentReference depthRef0;
	vk::SubpassDescription subpass;
	vk::SubpassDependency deps[3];
	vk::RenderPassCreateInfo desc;
	vk::Format depth_format;
	vk::Device device;
	uint32_t i;

	depth_format = vk::Format(vk_inst.depth_format);
	device = vk_inst.device;

	if (r_fbo->integer == 0)
	{
		// presentation
		attachments[0].flags = {};
		attachments[0].format = vk::Format(vk_inst.present_format.format);
		attachments[0].samples = vk::SampleCountFlagBits::e1;
#ifdef USE_BUFFER_CLEAR
		attachments[0].loadOp = vk::AttachmentLoadOp::eClear;
#else
		attachments[0].loadOp = vk::AttachmentLoadOp::eDontCare; // Assuming this will be completely overwritten
#endif
		attachments[0].storeOp = vk::AttachmentStoreOp::eStore; // needed for presentation
		attachments[0].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
		attachments[0].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
		attachments[0].initialLayout = vk::ImageLayout(vk_inst.initSwapchainLayout);
		attachments[0].finalLayout = vk::ImageLayout::ePresentSrcKHR;
	}
	else
	{
		// resolve/color buffer
		attachments[0].flags = {};
		attachments[0].format = vk::Format(vk_inst.color_format);
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
	attachments[1].flags = {};
	attachments[1].format = depth_format;
	attachments[1].samples = vk::SampleCountFlagBits(vkSamples);
	attachments[1].loadOp = vk::AttachmentLoadOp::eClear; // Need empty depth buffer before use
	attachments[1].stencilLoadOp = r_stencilbits->integer ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eDontCare;
	if (r_bloom->integer)
	{
		attachments[1].storeOp = vk::AttachmentStoreOp::eStore; // keep it for post-bloom pass
		attachments[1].stencilStoreOp = r_stencilbits->integer ? vk::AttachmentStoreOp::eStore : vk::AttachmentStoreOp::eDontCare;
	}
	else
	{
		attachments[1].storeOp = vk::AttachmentStoreOp::eDontCare;
		attachments[1].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
	}
	attachments[1].initialLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
	attachments[1].finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

	colorRef0.attachment = 0;
	colorRef0.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

	depthRef0.attachment = 1;
	depthRef0.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

	Com_Memset(&subpass, 0, sizeof(subpass));
	subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorRef0;
	subpass.pDepthStencilAttachment = &depthRef0;

	Com_Memset(&desc, 0, sizeof(desc));
	desc.pNext = nullptr;
	desc.flags = {};
	desc.pAttachments = attachments;
	desc.pSubpasses = &subpass;

	desc.subpassCount = 1;
	desc.attachmentCount = 2;

	if (vk_inst.msaaActive)
	{
		attachments[2].flags = {};
		attachments[2].format = vk::Format(vk_inst.color_format);
		attachments[2].samples = vk::SampleCountFlagBits(vkSamples);
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
		attachments[2].initialLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
		attachments[2].finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

		desc.attachmentCount = 3;

		colorRef0.attachment = 2; // msaa image attachment
		colorRef0.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

		colorResolveRef.attachment = 0; // resolve image attachment
		colorResolveRef.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

		subpass.pResolveAttachments = &colorResolveRef;
	}

	// subpass dependencies

	Com_Memset(&deps, 0, sizeof(deps));

	deps[2].srcSubpass = vk::SubpassExternal;
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

		VK_CHECK(vk_inst.render_pass.main = device.createRenderPass(desc));
		// VK_CHECK(qvkCreateRenderPass(device, &desc, NULL, &vk_inst.render_pass.main));
		SET_OBJECT_NAME(vk_inst.render_pass.main, "render pass - main", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT);

		return;
	}

	desc.dependencyCount = 2;
	desc.pDependencies = &deps[0];

	deps[0].srcSubpass = vk::SubpassExternal;
	deps[0].dstSubpass = 0;
	deps[0].srcStageMask = vk::PipelineStageFlagBits::eFragmentShader;											  // What pipeline stage must have completed for the dependency
	deps[0].dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;									  // What pipeline stage is waiting on the dependency
	deps[0].srcAccessMask = vk::AccessFlagBits::eShaderRead;													  // What access scopes are influence the dependency
	deps[0].dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite; // What access scopes are waiting on the dependency
	deps[0].dependencyFlags = vk::DependencyFlagBits::eByRegion;												  // Only need the current fragment (or tile) synchronized, not the whole framebuffer

	deps[1].srcSubpass = 0;
	deps[1].dstSubpass = vk::SubpassExternal;
	deps[1].srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput; // Fragment data has been written
	deps[1].dstStageMask = vk::PipelineStageFlagBits::eFragmentShader;		  // Don't start shading until data is available
	deps[1].srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;		  // Waiting for color data to be written
	deps[1].dstAccessMask = vk::AccessFlagBits::eShaderRead;				  // Don't read things from the shader before ready
	deps[1].dependencyFlags = vk::DependencyFlagBits::eByRegion;			  // Only need the current fragment (or tile) synchronized, not the whole framebuffer

	// VK_CHECK(qvkCreateRenderPass(device, &desc, NULL, &vk_inst.render_pass.main));
	VK_CHECK(vk_inst.render_pass.main = device.createRenderPass(desc));
	SET_OBJECT_NAME(vk_inst.render_pass.main, "render pass - main", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT);

	if (r_bloom->integer)
	{

		// post-bloom pass
		// color buffer
		attachments[0].loadOp = vk::AttachmentLoadOp::eLoad; // load from previous pass
															 // depth buffer
		attachments[1].loadOp = vk::AttachmentLoadOp::eLoad;
		attachments[1].storeOp = vk::AttachmentStoreOp::eDontCare;
		attachments[1].stencilLoadOp = vk::AttachmentLoadOp::eLoad;
		attachments[1].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
		if (vk_inst.msaaActive)
		{
			// msaa render target
			attachments[2].loadOp = vk::AttachmentLoadOp::eLoad;
			attachments[2].storeOp = vk::AttachmentStoreOp::eDontCare;
		}
		// VK_CHECK(qvkCreateRenderPass(device, &desc, NULL, &vk_inst.render_pass.post_bloom));
		VK_CHECK(vk_inst.render_pass.post_bloom = device.createRenderPass(desc));
		SET_OBJECT_NAME(vk_inst.render_pass.post_bloom, "render pass - post_bloom", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT);

		// bloom extraction, using resolved/main fbo as a source
		desc.attachmentCount = 1;

		colorRef0.attachment = 0;
		colorRef0.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

		Com_Memset(&subpass, 0, sizeof(subpass));
		subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorRef0;

		attachments[0].flags = {};
		attachments[0].format = vk::Format(vk_inst.bloom_format);
		attachments[0].samples = vk::SampleCountFlagBits::e1;
		attachments[0].loadOp = vk::AttachmentLoadOp::eDontCare; // Assuming this will be completely overwritten
		attachments[0].storeOp = vk::AttachmentStoreOp::eStore;	 // needed for next render pass
		attachments[0].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
		attachments[0].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
		attachments[0].initialLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
		attachments[0].finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

		// VK_CHECK(qvkCreateRenderPass(device, &desc, NULL, &vk_inst.render_pass.bloom_extract));
		VK_CHECK(vk_inst.render_pass.bloom_extract = device.createRenderPass(desc));
		SET_OBJECT_NAME(vk_inst.render_pass.bloom_extract, "render pass - bloom_extract", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT);

		for (i = 0; i < arrayLen(vk_inst.render_pass.blur); i++)
		{
			// VK_CHECK(qvkCreateRenderPass(device, &desc, NULL, &vk_inst.render_pass.blur[i]));
			VK_CHECK(vk_inst.render_pass.blur[i] = device.createRenderPass(desc));
			SET_OBJECT_NAME(vk_inst.render_pass.blur[i], va("render pass - blur %i", i), VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT);
		}
	}

	// capture render pass
	if (vk_inst.capture.image)
	{
		Com_Memset(&subpass, 0, sizeof(subpass));

		attachments[0].flags = {};
		attachments[0].format = vk::Format(vk_inst.capture_format);
		attachments[0].samples = vk::SampleCountFlagBits::e1;
		attachments[0].loadOp = vk::AttachmentLoadOp::eDontCare; // this will be completely overwritten
		attachments[0].storeOp = vk::AttachmentStoreOp::eStore;	 // needed for next render pass
		attachments[0].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
		attachments[0].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
		attachments[0].initialLayout = vk::ImageLayout::eUndefined;
		attachments[0].finalLayout = vk::ImageLayout::eTransferSrcOptimal;

		colorRef0.attachment = 0;
		colorRef0.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

		subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorRef0;

		desc.pNext = nullptr;
		desc.flags = {};
		desc.pAttachments = attachments;
		desc.attachmentCount = 1;
		desc.pSubpasses = &subpass;
		desc.subpassCount = 1;

		// VK_CHECK(qvkCreateRenderPass(device, &desc, NULL, &vk_inst.render_pass.capture));
		VK_CHECK(vk_inst.render_pass.capture = device.createRenderPass(desc));
		SET_OBJECT_NAME(vk_inst.render_pass.capture, "render pass - capture", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT);
	}

	colorRef0.attachment = 0;
	colorRef0.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

	desc.attachmentCount = 1;

	Com_Memset(&subpass, 0, sizeof(subpass));
	subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorRef0;

	// gamma post-processing
	attachments[0].flags = {};
	attachments[0].format = vk::Format(vk_inst.present_format.format);
	attachments[0].samples = vk::SampleCountFlagBits::e1;
	attachments[0].loadOp = vk::AttachmentLoadOp::eDontCare;
	attachments[0].storeOp = vk::AttachmentStoreOp::eStore; // needed for presentation
	attachments[0].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
	attachments[0].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
	attachments[0].initialLayout = vk::ImageLayout(vk_inst.initSwapchainLayout);
	attachments[0].finalLayout = vk::ImageLayout::ePresentSrcKHR;

	desc.dependencyCount = 1;
	desc.pDependencies = &deps[2];

	// VK_CHECK(qvkCreateRenderPass(device, &desc, NULL, &vk_inst.render_pass.gamma));
	VK_CHECK(vk_inst.render_pass.gamma = device.createRenderPass(desc));
	SET_OBJECT_NAME(vk_inst.render_pass.gamma, "render pass - gamma", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT);

	// screenmap
	desc.dependencyCount = 2;
	desc.pDependencies = &deps[0];

	// screenmap resolve/color buffer
	attachments[0].flags = {};
	attachments[0].format = vk::Format(vk_inst.color_format);
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
	attachments[1].flags = {};
	attachments[1].format = depth_format;
	attachments[1].samples = vk_inst.screenMapSamples;
	attachments[1].loadOp = vk::AttachmentLoadOp::eClear; // Need empty depth buffer before use
	attachments[1].storeOp = vk::AttachmentStoreOp::eDontCare;
	attachments[1].stencilLoadOp = vk::AttachmentLoadOp::eClear;
	attachments[1].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
	attachments[1].initialLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
	attachments[1].finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

	colorRef0.attachment = 0;
	colorRef0.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

	depthRef0.attachment = 1;
	depthRef0.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

	Com_Memset(&subpass, 0, sizeof(subpass));
	subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorRef0;
	subpass.pDepthStencilAttachment = &depthRef0;

	Com_Memset(&desc, 0, sizeof(desc));
	desc.pNext = nullptr;
	desc.flags = {};
	desc.pAttachments = attachments;
	desc.pSubpasses = &subpass;
	desc.subpassCount = 1;
	desc.attachmentCount = 2;
	desc.dependencyCount = 2;
	desc.pDependencies = deps;

	if (vk_inst.screenMapSamples > vk::SampleCountFlagBits::e1)
	{

		attachments[2].flags = {};
		attachments[2].format = vk::Format(vk_inst.color_format);
		attachments[2].samples = vk_inst.screenMapSamples;
#ifdef USE_BUFFER_CLEAR
		attachments[2].loadOp = vk::AttachmentLoadOp::eClear;
#else
		attachments[2].loadOp = vk::AttachmentLoadOp::eDontCare;
#endif
		attachments[2].storeOp = vk::AttachmentStoreOp::eDontCare;
		attachments[2].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
		attachments[2].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
		attachments[2].initialLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
		attachments[2].finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

		desc.attachmentCount = 3;

		colorRef0.attachment = 2; // screenmap msaa image attachment
		colorRef0.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

		colorResolveRef.attachment = 0; // screenmap resolve image attachment
		colorResolveRef.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

		subpass.pResolveAttachments = &colorResolveRef;
	}

	// VK_CHECK(qvkCreateRenderPass(device, &desc, NULL, &vk_inst.render_pass.screenmap));
	VK_CHECK(vk_inst.render_pass.screenmap = device.createRenderPass(desc));

	SET_OBJECT_NAME(vk_inst.render_pass.screenmap, "render pass - screenmap", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT);
}

static void allocate_and_bind_image_memory(VkImage image)
{
	VkMemoryRequirements memory_requirements;
	VkDeviceSize alignment;
	ImageChunk *chunk;
	int i;

	qvkGetImageMemoryRequirements(vk_inst.device, image, &memory_requirements);

	if (memory_requirements.size > vk_inst.image_chunk_size)
	{
		ri.Error(ERR_FATAL, "Vulkan: could not allocate memory, image is too large (%ikbytes).",
				 (int)(memory_requirements.size / 1024));
	}

	chunk = nullptr;

	// Try to find an existing chunk of sufficient capacity.
	alignment = memory_requirements.alignment;
	for (i = 0; i < vk_world.num_image_chunks; i++)
	{
		// ensure that memory region has proper alignment
		VkDeviceSize offset = PAD(vk_world.image_chunks[i].used, alignment);

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
		VkMemoryAllocateInfo alloc_info;
		VkDeviceMemory memory;

		if (vk_world.num_image_chunks >= MAX_IMAGE_CHUNKS)
		{
			ri.Error(ERR_FATAL, "Vulkan: image chunk limit has been reached");
		}

		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.pNext = nullptr;
		alloc_info.allocationSize = vk_inst.image_chunk_size;
		alloc_info.memoryTypeIndex = find_memory_type(memory_requirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);

		VK_CHECK(qvkAllocateMemory(vk_inst.device, &alloc_info, nullptr, &memory));

		chunk = &vk_world.image_chunks[vk_world.num_image_chunks];
		chunk->memory = memory;
		chunk->used = memory_requirements.size;

		SET_OBJECT_NAME(memory, va("image memory chunk %i", vk_world.num_image_chunks), VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT);

		vk_world.num_image_chunks++;
	}

	VK_CHECK(qvkBindImageMemory(vk_inst.device, image, chunk->memory, chunk->used - memory_requirements.size));
}

static void ensure_staging_buffer_allocation(VkDeviceSize size)
{
	VkBufferCreateInfo buffer_desc;
	VkMemoryRequirements memory_requirements;
	VkMemoryAllocateInfo alloc_info;
	uint32_t memory_type;
	void *data;

	if (vk_world.staging_buffer_size >= size)
		return;

	if (vk_world.staging_buffer != VK_NULL_HANDLE)
		qvkDestroyBuffer(vk_inst.device, vk_world.staging_buffer, nullptr);

	if (vk_world.staging_buffer_memory != VK_NULL_HANDLE)
		qvkFreeMemory(vk_inst.device, vk_world.staging_buffer_memory, nullptr);

	vk_world.staging_buffer_size = MAX(size, 1024 * 1024);

	buffer_desc.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_desc.pNext = nullptr;
	buffer_desc.flags = 0;
	buffer_desc.size = vk_world.staging_buffer_size;
	buffer_desc.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	buffer_desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	buffer_desc.queueFamilyIndexCount = 0;
	buffer_desc.pQueueFamilyIndices = nullptr;
	VK_CHECK(qvkCreateBuffer(vk_inst.device, &buffer_desc, nullptr, &vk_world.staging_buffer));

	qvkGetBufferMemoryRequirements(vk_inst.device, vk_world.staging_buffer, &memory_requirements);

	memory_type = find_memory_type(memory_requirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = nullptr;
	alloc_info.allocationSize = memory_requirements.size;
	alloc_info.memoryTypeIndex = memory_type;

	VK_CHECK(qvkAllocateMemory(vk_inst.device, &alloc_info, nullptr, &vk_world.staging_buffer_memory));
	VK_CHECK(qvkBindBufferMemory(vk_inst.device, vk_world.staging_buffer, vk_world.staging_buffer_memory, 0));

	VK_CHECK(qvkMapMemory(vk_inst.device, vk_world.staging_buffer_memory, 0, VK_WHOLE_SIZE, 0, &data));
	vk_world.staging_buffer_ptr = (byte *)data;

	SET_OBJECT_NAME(vk_world.staging_buffer, "staging buffer", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT);
	SET_OBJECT_NAME(vk_world.staging_buffer_memory, "staging buffer memory", VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT);
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
	return VK_FALSE;
}
#endif

static bool used_instance_extension(std::string_view ext)
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
	if (Q_stricmp_cpp(ext, vk::EXTDebugReportExtensionName) == 0)
		return true;
#endif

	if (Q_stricmp_cpp(ext, vk::EXTDebugUtilsExtensionName) == 0)
		return true;

	if (Q_stricmp_cpp(ext, vk::KHRGetPhysicalDeviceProperties2ExtensionName) == 0)
		return true;

	if (Q_stricmp_cpp(ext, vk::KHRPortabilityEnumerationExtensionName) == 0)
		return true;

	return false;
}

static void create_instance(void)
{
#ifdef USE_VK_VALIDATION
	const char *validation_layer_name = "VK_LAYER_LUNARG_standard_validation";
	const char *validation_layer_name2 = "VK_LAYER_KHRONOS_validation";
#endif

	vk::InstanceCreateFlags flags = {};
	const char **extension_names;
	uint32_t i, n, count, extension_count;

	extension_count = 0;
	std::vector<vk::ExtensionProperties> extension_properties;
	VK_CHECK(extension_properties = vk::enumerateInstanceExtensionProperties());

	count = extension_properties.size();
	extension_names = (const char **)ri.Malloc(sizeof(char *) * count);

	for (i = 0; i < count; i++)
	{
		const char *ext = std::string_view(extension_properties[i].extensionName).data();

		if (!used_instance_extension(ext))
		{
			continue;
		}

		// search for duplicates
		for (n = 0; n < extension_count; n++)
		{
			if (Q_stricmp(ext, extension_names[n]) == 0)
			{
				break;
			}
		}
		if (n != extension_count)
		{
			continue;
		}

		extension_names[extension_count++] = ext;

		if (Q_stricmp(ext, vk::KHRPortabilityEnumerationExtensionName) == 0)
		{
			flags |= vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR;
		}

		ri.Printf(PRINT_DEVELOPER, "instance extension: %s\n", ext);
	}
	vk::ApplicationInfo appInfo{
		nullptr,
		0x0,
		nullptr,
		0x0,
		VK_API_VERSION_1_0,
		nullptr};

	// create instance
	vk::InstanceCreateInfo desc{flags,
								&appInfo,
								0,
								nullptr,
								extension_count,
								extension_names,
								nullptr};

#ifdef USE_VK_VALIDATION
	desc.enabledLayerCount = 1;
	desc.ppEnabledLayerNames = &validation_layer_name;

	auto resultInstance = createInstance(desc);
	if (resultInstance.result == vk::Result::eErrorLayerNotPresent)
	{

		desc.enabledLayerCount = 1;
		desc.ppEnabledLayerNames = &validation_layer_name2;

		resultInstance = createInstance(desc);

		if (resultInstance.result == vk::Result::eErrorLayerNotPresent)
		{

			ri.Printf(PRINT_WARNING, "...validation layer is not available\n");

			// try without validation layer
			desc.enabledLayerCount = 0;
			desc.ppEnabledLayerNames = NULL;

			resultInstance = createInstance(desc);
			VK_CHECK(resultInstance.result);
			vk_instance = VkInstance(resultInstance.value);
		}
	}
#else

	VK_CHECK(vk_instanceCpp = createInstance(desc));
	vk_instance = VkInstance(vk_instanceCpp);
#endif

	ri.Free((void *)extension_names);
}

static vk::Format get_depth_format(vk::PhysicalDevice physical_device)
{
	vk::FormatProperties props;
	std::array<vk::Format, 2> formats;

	if (r_stencilbits->integer > 0)
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
static bool vk_blit_enabled(vk::PhysicalDevice physical_device, const vk::Format srcFormat, const vk::Format dstFormat)
{
	vk::FormatProperties formatProps;

	formatProps = physical_device.getFormatProperties(srcFormat);
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

static vk::Format get_hdr_format(vk::Format base_format)
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

static void get_present_format(int present_bits, vk::Format &bgr, vk::Format &rgb)
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

static bool vk_select_surface_format(vk::PhysicalDevice physical_deviceCpp, vk::SurfaceKHR surfaceCpp)
{
	vk::Format base_bgr, base_rgb;
	vk::Format ext_bgr, ext_rgb;

	uint32_t format_count;

	std::vector<vk::SurfaceFormatKHR> candidatesCpp;

#ifdef USE_VK_VALIDATION

	auto resultSurfaceFormatsKHR = physical_deviceCpp.getSurfaceFormatsKHR(surfaceCpp);

	if (resultSurfaceFormatsKHR.result != vk::Result::eSuccess)
	{
		ri.Printf(PRINT_ERROR, "vkGetPhysicalDeviceSurfaceFormatsKHR returned %s\n", vk_result_string(resultSurfaceFormatsKHR.result).data());
		return false;
	}

	format_count = resultSurfaceFormatsKHR.value.size();
	if (format_count == 0)
	{
		ri.Printf(PRINT_ERROR, "...no surface formats found\n");
		return false;
	}

	candidatesCpp = resultSurfaceFormatsKHR.value;

#else
	candidatesCpp = physical_deviceCpp.getSurfaceFormatsKHR(surfaceCpp);
	format_count = candidatesCpp.size();

	if (format_count == 0)
	{
		ri.Printf(PRINT_ERROR, "...no surface formats found\n");
		return false;
	}
#endif

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

	if (format_count == 1 && candidatesCpp[0].format == vk::Format::eUndefined)
	{
		// special case that means we can choose any format
		vk_inst.base_format.format = static_cast<VkFormat>(base_bgr);
		vk_inst.base_format.colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
		vk_inst.present_format.format = static_cast<VkFormat>(ext_bgr);
		vk_inst.present_format.colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
	}
	else
	{
		uint32_t i;
		for (i = 0; i < format_count; i++)
		{
			if ((candidatesCpp[i].format == base_bgr || candidatesCpp[i].format == base_rgb) && candidatesCpp[i].colorSpace == vk::ColorSpaceKHR::eVkColorspaceSrgbNonlinear)
			{
				vk_inst.base_format = candidatesCpp[i];
				break;
			}
		}
		if (i == format_count)
		{
			vk_inst.base_format = candidatesCpp[0];
		}
		for (i = 0; i < format_count; i++)
		{
			if ((candidatesCpp[i].format == ext_bgr || candidatesCpp[i].format == ext_rgb) && candidatesCpp[i].colorSpace == vk::ColorSpaceKHR::eVkColorspaceSrgbNonlinear)
			{
				vk_inst.present_format = candidatesCpp[i];
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

static void setup_surface_formats(vk::PhysicalDevice physical_deviceCpp)
{
	auto physical_device = static_cast<VkPhysicalDevice>(physical_deviceCpp);
	vk_inst.depth_format = static_cast<VkFormat>(get_depth_format(physical_deviceCpp));

	vk_inst.color_format = static_cast<VkFormat>(get_hdr_format(vk::Format(vk_inst.base_format.format)));

	vk_inst.capture_format = VK_FORMAT_R8G8B8A8_UNORM;

	vk_inst.bloom_format = vk_inst.base_format.format;

	vk_inst.blitEnabled = vk_blit_enabled(physical_device, vk::Format(vk_inst.color_format), vk::Format(vk_inst.capture_format));

	if (!vk_inst.blitEnabled)
	{
		vk_inst.capture_format = vk_inst.color_format;
	}
}

static const char *renderer_name(const VkPhysicalDeviceProperties *props)
{
	static char buf[sizeof(props->deviceName) + 64];
	const char *device_type;

	switch (props->deviceType)
	{
	case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
		device_type = "Integrated";
		break;
	case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
		device_type = "Discrete";
		break;
	case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
		device_type = "Virtual";
		break;
	case VK_PHYSICAL_DEVICE_TYPE_CPU:
		device_type = "CPU";
		break;
	default:
		device_type = "OTHER";
		break;
	}

	Com_sprintf(buf, sizeof(buf), "%s %s, 0x%04x",
				device_type, props->deviceName, props->deviceID);

	return buf;
}

static const char *renderer_nameCpp(const vk::PhysicalDeviceProperties &props)
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

	Com_sprintf(buf, sizeof(buf), "%s %s, 0x%04x",
				device_type, props.deviceName, props.deviceID);

	return buf;
}

static bool vk_create_device(vk::PhysicalDevice physical_deviceCpp, int device_index)
{

	ri.Printf(PRINT_ALL, "...selected physical device: %i\n", device_index);

	// select surface format
	if (!vk_select_surface_format(physical_deviceCpp, vk_surfaceCpp))
	{
		return false;
	}

	setup_surface_formats(physical_deviceCpp);

	// select queue family
	{
		std::vector<vk::QueueFamilyProperties> queue_families;
		uint32_t queue_family_count;
		uint32_t i;

		queue_families = physical_deviceCpp.getQueueFamilyProperties();

		queue_family_count = queue_families.size();

		// select queue family with presentation and graphics support
		vk_inst.queue_family_index = ~0U;
		for (i = 0; i < queue_family_count; i++)
		{
			vk::Bool32 presentation_supported = physical_deviceCpp.getSurfaceSupportKHR(i, vk_surfaceCpp);

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
		const char *device_extension_list[4];
		uint32_t device_extension_count;
		const char *end;
		char *str;
		const float priority = 1.0;

		vk::PhysicalDeviceFeatures features;

		VkResult res;
		bool swapchainSupported = false;
		bool dedicatedAllocation = false;
		bool memoryRequirements2 = false;
		bool debugMarker = false;
		uint32_t i, len, count = 0;

		auto extension_properties = physical_deviceCpp.enumerateDeviceExtensionProperties();
		count = extension_properties.size();

		// fill glConfig.extensions_string
		str = glConfig.extensions_string;
		*str = '\0';
		end = &glConfig.extensions_string[sizeof(glConfig.extensions_string) - 1];

		for (i = 0; i < count; i++)
		{
			auto ext = std::string_view(extension_properties[i].extensionName);
			if (ext == vk::KHRSwapchainExtensionName)
			{
				swapchainSupported = true;
			}
			else if (ext == vk::KHRDedicatedAllocationExtensionName)
			{
				dedicatedAllocation = true;
			}
			else if (ext == vk::KHRGetMemoryRequirements2ExtensionName)
			{
				memoryRequirements2 = true;
			}
			else if (ext == vk::EXTDebugMarkerExtensionName)
			{
				debugMarker = true;
			}
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
		}

		// ri.Free(extension_properties);

		device_extension_count = 0;

		if (!swapchainSupported)
		{
			ri.Printf(PRINT_ERROR, "...required device extension is not available: %s\n", vk::KHRSwapchainExtensionName);
			return false;
		}

		if (!memoryRequirements2)
			dedicatedAllocation = false;
		else
			vk_inst.dedicatedAllocation = dedicatedAllocation;

#ifndef USE_DEDICATED_ALLOCATION
		vk_inst.dedicatedAllocation = false;
#endif

		device_extension_list[device_extension_count++] = vk::KHRSwapchainExtensionName;

		if (vk_inst.dedicatedAllocation)
		{
			device_extension_list[device_extension_count++] = vk::KHRDedicatedAllocationExtensionName;
			device_extension_list[device_extension_count++] = vk::KHRGetMemoryRequirements2ExtensionName;
		}

		if (debugMarker)
		{
			device_extension_list[device_extension_count++] = vk::EXTDebugMarkerExtensionName;
			vk_inst.debugMarkers = true;
		}

		auto device_features = physical_deviceCpp.getFeatures();

		if (device_features.fillModeNonSolid == VK_FALSE)
		{
			ri.Printf(PRINT_ERROR, "...fillModeNonSolid feature is not supported\n");
			return false;
		}

		vk::DeviceQueueCreateInfo queue_desc{{},
											 vk_inst.queue_family_index,
											 1,
											 &priority,
											 nullptr};

		Com_Memset(&features, 0, sizeof(features));
		features.fillModeNonSolid = VK_TRUE;

		if (device_features.wideLines)
		{ // needed for RB_SurfaceAxis
			features.wideLines = VK_TRUE;
			vk_inst.wideLines = true;
		}

		if (device_features.fragmentStoresAndAtomics)
		{
			features.fragmentStoresAndAtomics = VK_TRUE;
			vk_inst.fragmentStores = true;
		}

		if (r_ext_texture_filter_anisotropic->integer && device_features.samplerAnisotropy)
		{
			features.samplerAnisotropy = VK_TRUE;
			vk_inst.samplerAnisotropy = true;
		}

		vk::DeviceCreateInfo device_desc{{},
										 1,
										 &queue_desc,
										 0,
										 nullptr,
										 device_extension_count,
										 device_extension_list,
										 &features,
										 nullptr};

#ifdef USE_VK_VALIDATION

		auto resultCreateDevice = physical_deviceCpp.createDevice(device_desc);

		if (resultCreateDevice.result != vk::Result::eSuccess)
		{
			ri.Printf(PRINT_ERROR, "vkCreateDevice returned %s\n", vk_result_string(resultCreateDevice.result).data());
			return false;
		}

#else
		vk_inst.device = physical_deviceCpp.createDevice(device_desc);
		// vk_inst.device = static_cast<VkDevice>(vkDeviceCpp);
#endif
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
	q##func = /*(PFN_ ## func)*/ ri.VK_GetInstanceProcAddr(vk_instance, #func);

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
	if (vk_surface != VK_NULL_HANDLE)
	{
		if (qvkDestroySurfaceKHR != nullptr)
		{
			qvkDestroySurfaceKHR(vk_instance, vk_surface, nullptr);
		}
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

	if (vk_instance != VK_NULL_HANDLE)
	{
		if (qvkDestroyInstance)
		{
			qvkDestroyInstance(vk_instance, nullptr);
		}
		vk_instance = VK_NULL_HANDLE;
	}
}

static void init_vulkan_library(void)
{
	uint32_t device_count;
	int device_index;
	uint32_t i;
	VkResult res;

	Com_Memset(&vk_inst, 0, sizeof(vk_inst));

	if (vk_instance == VK_NULL_HANDLE)
	{

		// force cleanup
		vk_destroy_instance();

		// Get functions that do not depend on VkInstance (vk_instance == nullptr at this point).
		INIT_INSTANCE_FUNCTION(vkCreateInstance)
		INIT_INSTANCE_FUNCTION(vkEnumerateInstanceExtensionProperties)

		// Get instance level functions.
		create_instance();

		INIT_INSTANCE_FUNCTION(vkCreateDevice)
		INIT_INSTANCE_FUNCTION(vkDestroyInstance)
		INIT_INSTANCE_FUNCTION(vkEnumerateDeviceExtensionProperties)
		INIT_INSTANCE_FUNCTION(vkEnumeratePhysicalDevices)
		INIT_INSTANCE_FUNCTION(vkGetDeviceProcAddr)
		INIT_INSTANCE_FUNCTION(vkGetPhysicalDeviceFeatures)
		INIT_INSTANCE_FUNCTION(vkGetPhysicalDeviceFormatProperties)
		INIT_INSTANCE_FUNCTION(vkGetPhysicalDeviceMemoryProperties)
		INIT_INSTANCE_FUNCTION(vkGetPhysicalDeviceProperties)
		INIT_INSTANCE_FUNCTION(vkGetPhysicalDeviceQueueFamilyProperties)
		INIT_INSTANCE_FUNCTION(vkDestroySurfaceKHR)
		INIT_INSTANCE_FUNCTION(vkGetPhysicalDeviceSurfaceCapabilitiesKHR)
		INIT_INSTANCE_FUNCTION(vkGetPhysicalDeviceSurfaceFormatsKHR)
		INIT_INSTANCE_FUNCTION(vkGetPhysicalDeviceSurfacePresentModesKHR)
		INIT_INSTANCE_FUNCTION(vkGetPhysicalDeviceSurfaceSupportKHR)

#ifdef USE_VK_VALIDATION
		INIT_INSTANCE_FUNCTION_EXT(vkCreateDebugReportCallbackEXT)
		INIT_INSTANCE_FUNCTION_EXT(vkDestroyDebugReportCallbackEXT)

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
		if (!ri.VK_CreateSurface(vk_instance, &vk_surface))
		{
			ri.Error(ERR_FATAL, "Error creating Vulkan surface");
			return;
		}
		vk_surfaceCpp = vk::SurfaceKHR(vk_surface);
	} // vk_instance == VK_NULL_HANDLE

	std::vector<vk::PhysicalDevice> physical_devicesCpp = vk_instanceCpp.enumeratePhysicalDevices();

#ifdef USE_VK_VALIDATION
	auto resultPhysicalDevices = vk_instanceCpp.enumeratePhysicalDevices();

	if (resultPhysicalDevices.result != vk::Result::eSuccess)
	{
		ri.Error(ERR_FATAL, "vkEnumeratePhysicalDevices returned %s", vk_result_string(resultPhysicalDevices.result).data());
		return;
	}

	device_count = resultPhysicalDevices.value.size();
	if (device_count == 0)
	{
		ri.Error(ERR_FATAL, "Vulkan: no physical devices found");
		return;
	}

	physical_devicesCpp = resultPhysicalDevices.value;

#else
	physical_devicesCpp = vk_instanceCpp.enumeratePhysicalDevices();
	device_count = physical_devicesCpp.size();

	if (device_count == 0)
	{
		ri.Error(ERR_FATAL, "Vulkan: no physical devices found");
		return;
	}
#endif

	// initial physical device index
	device_index = r_device->integer;

	ri.Printf(PRINT_ALL, ".......................\nAvailable physical devices:\n");

	VkPhysicalDeviceProperties props;

	for (i = 0; i < device_count; i++)
	{
		//  auto propsCpp = vk::PhysicalDevice(physical_devices[i]).getProperties();
		//  ri.Printf(PRINT_ALL, " %i: %s\n", i, renderer_nameCpp(propsCpp));

		// if (device_index == -1 && propsCpp.deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
		// {
		// 	device_index = i;
		// }
		// else if (device_index == -2 && propsCpp.deviceType == vk::PhysicalDeviceType::eIntegratedGpu)
		// {
		// 	device_index = i;
		// }

		qvkGetPhysicalDeviceProperties(VkPhysicalDevice(physical_devicesCpp[i]), &props);
		ri.Printf(PRINT_ALL, " %i: %s\n", i, renderer_name(&props));
		if (device_index == -1 && props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
		{
			device_index = i;
		}
		else if (device_index == -2 && props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
		{
			device_index = i;
		}
	}
	ri.Printf(PRINT_ALL, ".......................\n");

	vk_inst.physical_device = VK_NULL_HANDLE;
	for (i = 0; i < device_count; i++, device_index++)
	{
		if (device_index >= static_cast<int>(device_count) || device_index < 0)
		{
			device_index = 0;
		}

		if (vk_create_device(physical_devicesCpp[i], device_index))
		{
			vk_inst.physical_device = physical_devicesCpp[device_index];
			break;
		}
	}

	// ri.Free(physical_devices);

	if (vk_inst.physical_device == VK_NULL_HANDLE)
	{
		ri.Error(ERR_FATAL, "Vulkan: unable to find any suitable physical device");
		return;
	}

	//
	// Get device level functions.
	//
	INIT_DEVICE_FUNCTION(vkAllocateCommandBuffers)
	INIT_DEVICE_FUNCTION(vkAllocateDescriptorSets)
	INIT_DEVICE_FUNCTION(vkAllocateMemory)
	INIT_DEVICE_FUNCTION(vkBeginCommandBuffer)
	INIT_DEVICE_FUNCTION(vkBindBufferMemory)
	INIT_DEVICE_FUNCTION(vkBindImageMemory)
	INIT_DEVICE_FUNCTION(vkCmdBeginRenderPass)
	INIT_DEVICE_FUNCTION(vkCmdBindDescriptorSets)
	INIT_DEVICE_FUNCTION(vkCmdBindIndexBuffer)
	INIT_DEVICE_FUNCTION(vkCmdBindPipeline)
	INIT_DEVICE_FUNCTION(vkCmdBindVertexBuffers)
	INIT_DEVICE_FUNCTION(vkCmdBlitImage)
	INIT_DEVICE_FUNCTION(vkCmdClearAttachments)
	INIT_DEVICE_FUNCTION(vkCmdCopyBuffer)
	INIT_DEVICE_FUNCTION(vkCmdCopyBufferToImage)
	INIT_DEVICE_FUNCTION(vkCmdCopyImage)
	INIT_DEVICE_FUNCTION(vkCmdDraw)
	INIT_DEVICE_FUNCTION(vkCmdDrawIndexed)
	INIT_DEVICE_FUNCTION(vkCmdEndRenderPass)
	INIT_DEVICE_FUNCTION(vkCmdNextSubpass)
	INIT_DEVICE_FUNCTION(vkCmdPipelineBarrier)
	INIT_DEVICE_FUNCTION(vkCmdPushConstants)
	INIT_DEVICE_FUNCTION(vkCmdSetDepthBias)
	INIT_DEVICE_FUNCTION(vkCmdSetScissor)
	INIT_DEVICE_FUNCTION(vkCmdSetViewport)
	INIT_DEVICE_FUNCTION(vkCreateBuffer)
	INIT_DEVICE_FUNCTION(vkCreateCommandPool)
	INIT_DEVICE_FUNCTION(vkCreateDescriptorPool)
	INIT_DEVICE_FUNCTION(vkCreateDescriptorSetLayout)
	INIT_DEVICE_FUNCTION(vkCreateFence)
	INIT_DEVICE_FUNCTION(vkCreateFramebuffer)
	INIT_DEVICE_FUNCTION(vkCreateGraphicsPipelines)
	INIT_DEVICE_FUNCTION(vkCreateImage)
	INIT_DEVICE_FUNCTION(vkCreateImageView)
	INIT_DEVICE_FUNCTION(vkCreatePipelineCache)
	INIT_DEVICE_FUNCTION(vkCreatePipelineLayout)
	INIT_DEVICE_FUNCTION(vkCreateRenderPass)
	INIT_DEVICE_FUNCTION(vkCreateSampler)
	INIT_DEVICE_FUNCTION(vkCreateSemaphore)
	INIT_DEVICE_FUNCTION(vkCreateShaderModule)
	INIT_DEVICE_FUNCTION(vkDestroyBuffer)
	INIT_DEVICE_FUNCTION(vkDestroyCommandPool)
	INIT_DEVICE_FUNCTION(vkDestroyDescriptorPool)
	INIT_DEVICE_FUNCTION(vkDestroyDescriptorSetLayout)
	INIT_DEVICE_FUNCTION(vkDestroyDevice)
	INIT_DEVICE_FUNCTION(vkDestroyFence)
	INIT_DEVICE_FUNCTION(vkDestroyFramebuffer)
	INIT_DEVICE_FUNCTION(vkDestroyImage)
	INIT_DEVICE_FUNCTION(vkDestroyImageView)
	INIT_DEVICE_FUNCTION(vkDestroyPipeline)
	INIT_DEVICE_FUNCTION(vkDestroyPipelineCache)
	INIT_DEVICE_FUNCTION(vkDestroyPipelineLayout)
	INIT_DEVICE_FUNCTION(vkDestroyRenderPass)
	INIT_DEVICE_FUNCTION(vkDestroySampler)
	INIT_DEVICE_FUNCTION(vkDestroySemaphore)
	INIT_DEVICE_FUNCTION(vkDestroyShaderModule)
	INIT_DEVICE_FUNCTION(vkDeviceWaitIdle)
	INIT_DEVICE_FUNCTION(vkEndCommandBuffer)
	INIT_DEVICE_FUNCTION(vkFlushMappedMemoryRanges)
	INIT_DEVICE_FUNCTION(vkFreeCommandBuffers)
	INIT_DEVICE_FUNCTION(vkFreeDescriptorSets)
	INIT_DEVICE_FUNCTION(vkFreeMemory)
	INIT_DEVICE_FUNCTION(vkGetBufferMemoryRequirements)
	INIT_DEVICE_FUNCTION(vkGetDeviceQueue)
	INIT_DEVICE_FUNCTION(vkGetImageMemoryRequirements)
	INIT_DEVICE_FUNCTION(vkGetImageSubresourceLayout)
	INIT_DEVICE_FUNCTION(vkInvalidateMappedMemoryRanges)
	INIT_DEVICE_FUNCTION(vkMapMemory)
	INIT_DEVICE_FUNCTION(vkQueueSubmit)
	INIT_DEVICE_FUNCTION(vkQueueWaitIdle)
	INIT_DEVICE_FUNCTION(vkResetCommandBuffer)
	INIT_DEVICE_FUNCTION(vkResetDescriptorPool)
	INIT_DEVICE_FUNCTION(vkResetFences)
	INIT_DEVICE_FUNCTION(vkUnmapMemory)
	INIT_DEVICE_FUNCTION(vkUpdateDescriptorSets)
	INIT_DEVICE_FUNCTION(vkWaitForFences)
	INIT_DEVICE_FUNCTION(vkAcquireNextImageKHR)
	INIT_DEVICE_FUNCTION(vkCreateSwapchainKHR)
	INIT_DEVICE_FUNCTION(vkDestroySwapchainKHR)
	INIT_DEVICE_FUNCTION(vkGetSwapchainImagesKHR)
	INIT_DEVICE_FUNCTION(vkQueuePresentKHR)

	if (vk_inst.dedicatedAllocation)
	{
		INIT_DEVICE_FUNCTION_EXT(vkGetBufferMemoryRequirements2KHR);
		INIT_DEVICE_FUNCTION_EXT(vkGetImageMemoryRequirements2KHR);
		if (!qvkGetBufferMemoryRequirements2KHR || !qvkGetImageMemoryRequirements2KHR)
		{
			vk_inst.dedicatedAllocation = false;
		}
	}

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
	qvkCreateInstance = nullptr;
	qvkEnumerateInstanceExtensionProperties = nullptr;

	// instance functions:
	qvkCreateDevice = nullptr;
	qvkDestroyInstance = nullptr;
	qvkEnumerateDeviceExtensionProperties = nullptr;
	qvkEnumeratePhysicalDevices = nullptr;
	qvkGetDeviceProcAddr = nullptr;
	qvkGetPhysicalDeviceFeatures = nullptr;
	qvkGetPhysicalDeviceFormatProperties = nullptr;
	qvkGetPhysicalDeviceMemoryProperties = nullptr;
	qvkGetPhysicalDeviceProperties = nullptr;
	qvkGetPhysicalDeviceQueueFamilyProperties = nullptr;
	qvkDestroySurfaceKHR = nullptr;
	qvkGetPhysicalDeviceSurfaceCapabilitiesKHR = nullptr;
	qvkGetPhysicalDeviceSurfaceFormatsKHR = nullptr;
	qvkGetPhysicalDeviceSurfacePresentModesKHR = nullptr;
	qvkGetPhysicalDeviceSurfaceSupportKHR = nullptr;
#ifdef USE_VK_VALIDATION
	qvkCreateDebugReportCallbackEXT = nullptr;
	qvkDestroyDebugReportCallbackEXT = nullptr;
#endif
}

static void deinit_device_functions(void)
{
	// device functions:
	qvkAllocateCommandBuffers = nullptr;
	qvkAllocateDescriptorSets = nullptr;
	qvkAllocateMemory = nullptr;
	qvkBeginCommandBuffer = nullptr;
	qvkBindBufferMemory = nullptr;
	qvkBindImageMemory = nullptr;
	qvkCmdBeginRenderPass = nullptr;
	qvkCmdBindDescriptorSets = nullptr;
	qvkCmdBindIndexBuffer = nullptr;
	qvkCmdBindPipeline = nullptr;
	qvkCmdBindVertexBuffers = nullptr;
	qvkCmdBlitImage = nullptr;
	qvkCmdClearAttachments = nullptr;
	qvkCmdCopyBuffer = nullptr;
	qvkCmdCopyBufferToImage = nullptr;
	qvkCmdCopyImage = nullptr;
	qvkCmdDraw = nullptr;
	qvkCmdDrawIndexed = nullptr;
	qvkCmdEndRenderPass = nullptr;
	qvkCmdNextSubpass = nullptr;
	qvkCmdPipelineBarrier = nullptr;
	qvkCmdPushConstants = nullptr;
	qvkCmdSetDepthBias = nullptr;
	qvkCmdSetScissor = nullptr;
	qvkCmdSetViewport = nullptr;
	qvkCreateBuffer = nullptr;
	qvkCreateCommandPool = nullptr;
	qvkCreateDescriptorPool = nullptr;
	qvkCreateDescriptorSetLayout = nullptr;
	qvkCreateFence = nullptr;
	qvkCreateFramebuffer = nullptr;
	qvkCreateGraphicsPipelines = nullptr;
	qvkCreateImage = nullptr;
	qvkCreateImageView = nullptr;
	qvkCreatePipelineCache = nullptr;
	qvkCreatePipelineLayout = nullptr;
	qvkCreateRenderPass = nullptr;
	qvkCreateSampler = nullptr;
	qvkCreateSemaphore = nullptr;
	qvkCreateShaderModule = nullptr;
	qvkDestroyBuffer = nullptr;
	qvkDestroyCommandPool = nullptr;
	qvkDestroyDescriptorPool = nullptr;
	qvkDestroyDescriptorSetLayout = nullptr;
	qvkDestroyDevice = nullptr;
	qvkDestroyFence = nullptr;
	qvkDestroyFramebuffer = nullptr;
	qvkDestroyImage = nullptr;
	qvkDestroyImageView = nullptr;
	qvkDestroyPipeline = nullptr;
	qvkDestroyPipelineCache = nullptr;
	qvkDestroyPipelineLayout = nullptr;
	qvkDestroyRenderPass = nullptr;
	qvkDestroySampler = nullptr;
	qvkDestroySemaphore = nullptr;
	qvkDestroyShaderModule = nullptr;
	qvkDeviceWaitIdle = nullptr;
	qvkEndCommandBuffer = nullptr;
	qvkFlushMappedMemoryRanges = nullptr;
	qvkFreeCommandBuffers = nullptr;
	qvkFreeDescriptorSets = nullptr;
	qvkFreeMemory = nullptr;
	qvkGetBufferMemoryRequirements = nullptr;
	qvkGetDeviceQueue = nullptr;
	qvkGetImageMemoryRequirements = nullptr;
	qvkGetImageSubresourceLayout = nullptr;
	qvkInvalidateMappedMemoryRanges = nullptr;
	qvkMapMemory = nullptr;
	qvkQueueSubmit = nullptr;
	qvkQueueWaitIdle = nullptr;
	qvkResetCommandBuffer = nullptr;
	qvkResetDescriptorPool = nullptr;
	qvkResetFences = nullptr;
	qvkUnmapMemory = nullptr;
	qvkUpdateDescriptorSets = nullptr;
	qvkWaitForFences = nullptr;
	qvkAcquireNextImageKHR = nullptr;
	qvkCreateSwapchainKHR = nullptr;
	qvkDestroySwapchainKHR = nullptr;
	qvkGetSwapchainImagesKHR = nullptr;
	qvkQueuePresentKHR = nullptr;

	qvkGetBufferMemoryRequirements2KHR = nullptr;
	qvkGetImageMemoryRequirements2KHR = nullptr;

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
									count,
									(const uint32_t *)bytes,
									nullptr};

	VK_CHECK(module = vk_inst.device.createShaderModule(desc));

	return module;
}

static vk::DescriptorSetLayout vk_create_layout_binding(int binding, vk::DescriptorType type, vk::ShaderStageFlags flags)
{

	vk::DescriptorSetLayoutBinding bind{binding,
										type,
										1,
										flags,
										nullptr};

	vk::DescriptorSetLayoutCreateInfo desc{{},
										   1,
										   &bind,
										   nullptr};

#ifdef USE_VK_VALIDATION

	auto resultDescSetLayout = vk_inst.device.createDescriptorSetLayout(desc);
	if (resultDescSetLayout.result != vk::Result::eSuccess)
	{
		ri.Printf(PRINT_ERROR, "createDescriptorSetLayout returned %s\n", vk_result_string(resultDescSetLayout.result).data());
	}
	return vk_inst.device.createDescriptorSetLayout(desc);
#else
	return vk_inst.device.createDescriptorSetLayout(desc);
#endif
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

static VkSampler vk_find_sampler(const Vk_Sampler_Def *def)
{
	vk::SamplerAddressMode address_mode;
	VkSampler sampler;
	vk::Filter mag_filter;
	vk::Filter min_filter;
	vk::SamplerMipmapMode mipmap_mode;
	float maxLod;
	int i;

	// Look for sampler among existing samplers.
	for (i = 0; i < vk_world.num_samplers; i++)
	{
		const Vk_Sampler_Def *cur_def = &vk_world.sampler_defs[i];
		if (memcmp(cur_def, def, sizeof(*def)) == 0)
		{
			return vk_world.samplers[i];
		}
	}

	// Create new sampler.
	if (vk_world.num_samplers >= MAX_VK_SAMPLERS)
	{
		ri.Error(ERR_DROP, "vk_find_sampler: MAX_VK_SAMPLERS hit\n");
	}

	address_mode = def->address_mode;

	if (def->gl_mag_filter == GL_NEAREST)
	{
		mag_filter = vk::Filter::eNearest;
	}
	else if (def->gl_mag_filter == GL_LINEAR)
	{
		mag_filter = vk::Filter::eLinear;
	}
	else
	{
		ri.Error(ERR_FATAL, "vk_find_sampler: invalid gl_mag_filter");
		return VK_NULL_HANDLE;
	}

	maxLod = vk_inst.maxLod;

	if (def->gl_min_filter == GL_NEAREST)
	{
		min_filter = vk::Filter::eNearest;
		mipmap_mode = vk::SamplerMipmapMode::eNearest;
		maxLod = 0.25f; // used to emulate OpenGL's GL_LINEAR/GL_NEAREST minification filter
	}
	else if (def->gl_min_filter == GL_LINEAR)
	{
		min_filter = vk::Filter::eLinear;
		mipmap_mode = vk::SamplerMipmapMode::eNearest;
		maxLod = 0.25f; // used to emulate OpenGL's GL_LINEAR/GL_NEAREST minification filter
	}
	else if (def->gl_min_filter == GL_NEAREST_MIPMAP_NEAREST)
	{
		min_filter = vk::Filter::eNearest;
		mipmap_mode = vk::SamplerMipmapMode::eNearest;
	}
	else if (def->gl_min_filter == GL_LINEAR_MIPMAP_NEAREST)
	{
		min_filter = vk::Filter::eLinear;
		mipmap_mode = vk::SamplerMipmapMode::eNearest;
	}
	else if (def->gl_min_filter == GL_NEAREST_MIPMAP_LINEAR)
	{
		min_filter = vk::Filter::eNearest;
		mipmap_mode = vk::SamplerMipmapMode::eLinear;
	}
	else if (def->gl_min_filter == GL_LINEAR_MIPMAP_LINEAR)
	{
		min_filter = vk::Filter::eLinear;
		mipmap_mode = vk::SamplerMipmapMode::eLinear;
	}
	else
	{
		ri.Error(ERR_FATAL, "vk_find_sampler: invalid gl_min_filter");
		return VK_NULL_HANDLE;
	}

	if (def->max_lod_1_0)
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
							   VK_FALSE,
							   vk::CompareOp::eAlways,
							   0.0f,
							   (maxLod == vk_inst.maxLod) ? VK_LOD_CLAMP_NONE : maxLod,
							   vk::BorderColor::eFloatTransparentBlack,
							   VK_FALSE,
							   nullptr};

	if (def->noAnisotropy || mipmap_mode == vk::SamplerMipmapMode::eNearest || mag_filter == vk::Filter::eNearest)
	{
		desc.anisotropyEnable = VK_FALSE;
		desc.maxAnisotropy = 1.0f;
	}
	else
	{
		desc.anisotropyEnable = (r_ext_texture_filter_anisotropic->integer && vk_inst.samplerAnisotropy) ? VK_TRUE : VK_FALSE;
		if (desc.anisotropyEnable)
		{
			desc.maxAnisotropy = MIN(r_ext_max_anisotropy->integer, vk_inst.maxAnisotropy);
		}
	}

	VK_CHECK(sampler = vk_inst.device.createSampler(desc));

	// VK_CHECK(qvkCreateSampler(vk_inst.device, &desc, nullptr, &sampler));

	SET_OBJECT_NAME(sampler, va("image sampler %i", vk_world.num_samplers), VK_DEBUG_REPORT_OBJECT_TYPE_SAMPLER_EXT);

	vk_world.sampler_defs[vk_world.num_samplers] = *def;
	vk_world.samplers[vk_world.num_samplers] = sampler;
	vk_world.num_samplers++;

	return sampler;
}

static void vk_update_attachment_descriptors(void)
{

	if (vk_inst.color_image_view)
	{

		Vk_Sampler_Def sd;

		Com_Memset(&sd, 0, sizeof(sd));
		sd.gl_mag_filter = sd.gl_min_filter = vk_inst.blitFilter;
		sd.address_mode = vk::SamplerAddressMode::eClampToEdge;
		sd.max_lod_1_0 = true;
		sd.noAnisotropy = true;

		vk::DescriptorImageInfo info{vk_find_sampler(&sd),
									 vk_inst.color_image_view,
									 vk::ImageLayout::eShaderReadOnlyOptimal};

		vk::WriteDescriptorSet desc{vk::DescriptorSet(vk_inst.color_descriptor),
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

		info.sampler = vk_find_sampler(&sd);

		info.imageView = vk::ImageView(vk_inst.screenMap.color_image_view);
		desc.dstSet = vk::DescriptorSet(vk_inst.screenMap.color_descriptor);

		vk_inst.device.updateDescriptorSets(desc, nullptr);

		// bloom images
		if (r_bloom->integer)
		{
			uint32_t i;
			for (i = 0; i < arrayLen(vk_inst.bloom_image_descriptor); i++)
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

	auto layout_storage = vk::DescriptorSetLayout(vk_inst.set_layout_storage);

	vk::DescriptorSetAllocateInfo alloc{vk::DescriptorPool(vk_inst.descriptor_pool),
										1,
										&layout_storage,
										nullptr};

	std::vector<vk::DescriptorSet> descriptorSets;
	VK_CHECK(descriptorSets = vk_inst.device.allocateDescriptorSets(alloc));
	vk_inst.storage.descriptor = descriptorSets[0];

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

	// qvkUpdateDescriptorSets(vk_inst.device, 1, &desc, 0, nullptr);

	// allocated and update descriptor set
	for (i = 0; i < NUM_COMMAND_BUFFERS; i++)
	{
		auto layout_uniform = vk::DescriptorSetLayout(vk_inst.set_layout_uniform);

		alloc.descriptorPool = vk_inst.descriptor_pool;
		alloc.descriptorSetCount = 1;
		alloc.pSetLayouts = &layout_uniform;

		std::vector<vk::DescriptorSet> descriptorSetsInner;
		VK_CHECK(descriptorSetsInner = vk_inst.device.allocateDescriptorSets(alloc));
		vk_inst.tess[i].uniform_descriptor = descriptorSetsInner[0];
		// VK_CHECK(qvkAllocateDescriptorSets(vk_inst.device, &alloc, &vk_inst.tess[i].uniform_descriptor));

		vk_update_uniform_descriptor(vk_inst.tess[i].uniform_descriptor, vk_inst.tess[i].vertex_buffer);

		SET_OBJECT_NAME(vk_inst.tess[i].uniform_descriptor, va("uniform descriptor %i", i), VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT);
	}

	if (vk_inst.color_image_view)
	{
		auto layout_sampler = vk::DescriptorSetLayout(vk_inst.set_layout_sampler);

		alloc.descriptorPool = vk_inst.descriptor_pool;
		alloc.descriptorSetCount = 1;
		alloc.pSetLayouts = &layout_sampler;

		std::vector<vk::DescriptorSet> descriptorSetsInner;
		VK_CHECK(descriptorSetsInner = vk_inst.device.allocateDescriptorSets(alloc));
		vk_inst.color_descriptor = descriptorSetsInner[0];
		// VK_CHECK(qvkAllocateDescriptorSets(vk_inst.device, &alloc, &vk_inst.color_descriptor));

		if (r_bloom->integer)
		{
			// Allocate all bloom descriptors in a single call
			alloc.descriptorSetCount = arrayLen(vk_inst.bloom_image_descriptor);
			std::vector<vk::DescriptorSet> bloomDescriptorSets(alloc.descriptorSetCount);
			VK_CHECK(bloomDescriptorSets = vk_inst.device.allocateDescriptorSets(alloc));

			for (i = 0; i < alloc.descriptorSetCount; i++)
			{
				vk_inst.bloom_image_descriptor[i] = bloomDescriptorSets[i];
			}
		}

		alloc.descriptorSetCount = 1;

		VK_CHECK(descriptorSetsInner = vk_inst.device.allocateDescriptorSets(alloc));
		vk_inst.screenMap.color_descriptor = descriptorSetsInner[0];

		// VK_CHECK(qvkAllocateDescriptorSets(vk_inst.device, &alloc, &vk_inst.screenMap.color_descriptor)); // screenmap

		vk_update_attachment_descriptors();
	}
}

static void vk_release_geometry_buffers(void)
{
	int i;

	for (i = 0; i < NUM_COMMAND_BUFFERS; i++)
	{
		vk_inst.device.destroyBuffer(vk_inst.tess[i].vertex_buffer);
		vk_inst.tess[i].vertex_buffer = VK_NULL_HANDLE;
	}

	vk_inst.device.freeMemory(vk_inst.geometry_buffer_memory);
	vk_inst.geometry_buffer_memory = VK_NULL_HANDLE;
}

static void vk_create_geometry_buffers(VkDeviceSize size)
{
	vk::MemoryRequirements vb_memory_requirements;
	vk::DeviceSize vertex_buffer_offset;
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

	Com_Memset(&vb_memory_requirements, 0, sizeof(vb_memory_requirements));

	for (i = 0; i < NUM_COMMAND_BUFFERS; i++)
	{
		desc.size = size;
		desc.usage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eUniformBuffer;

		VK_CHECK(vk_inst.tess[i].vertex_buffer = vk_inst.device.createBuffer(desc));
		// VK_CHECK(qvkCreateBuffer(vk_inst.device, &desc, nullptr, &vk_inst.tess[i].vertex_buffer));
		vb_memory_requirements = vk_inst.device.getBufferMemoryRequirements(vk_inst.tess[i].vertex_buffer);
		// qvkGetBufferMemoryRequirements(vk_inst.device, vk_inst.tess[i].vertex_buffer, &vb_memory_requirements);
	}

	memory_type_bits = vb_memory_requirements.memoryTypeBits;
	memory_type = find_memory_type(memory_type_bits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

	vk::MemoryAllocateInfo alloc_info{vb_memory_requirements.size * NUM_COMMAND_BUFFERS,
									  memory_type,
									  nullptr};

	// VK_CHECK(qvkAllocateMemory(vk_inst.device, &alloc_info, nullptr, &vk_inst.geometry_buffer_memory));
	VK_CHECK(vk_inst.geometry_buffer_memory = vk_inst.device.allocateMemory(alloc_info));

	// VK_CHECK(qvkMapMemory(vk_inst.device, vk_inst.geometry_buffer_memory, 0, VK_WHOLE_SIZE, 0, &data));
	VK_CHECK(vk_inst.device.mapMemory(vk_inst.geometry_buffer_memory, 0, vk::WholeSize, {}, &data));

	vertex_buffer_offset = 0;

	for (i = 0; i < NUM_COMMAND_BUFFERS; i++)
	{
		vk_inst.device.bindBufferMemory(vk_inst.tess[i].vertex_buffer, vk_inst.geometry_buffer_memory, vertex_buffer_offset);
		// qvkBindBufferMemory(vk_inst.device, vk_inst.tess[i].vertex_buffer, vk_inst.geometry_buffer_memory, vertex_buffer_offset);
		vk_inst.tess[i].vertex_buffer_ptr = (byte *)data + vertex_buffer_offset;
		vk_inst.tess[i].vertex_buffer_offset = 0;
		vertex_buffer_offset += vb_memory_requirements.size;

		SET_OBJECT_NAME(vk_inst.tess[i].vertex_buffer, va("geometry buffer %i", i), VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT);
	}

	SET_OBJECT_NAME(vk_inst.geometry_buffer_memory, "geometry buffer memory", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT);

	vk_inst.geometry_buffer_size = vb_memory_requirements.size;

	Com_Memset(&vk_inst.stats, 0, sizeof(vk_inst.stats));
}

static void vk_create_storage_buffer(uint32_t size)
{
	vk::MemoryRequirements memory_requirements;
	uint32_t memory_type_bits;
	uint32_t memory_type;

	vk::BufferCreateInfo desc{{},
							  size,
							  vk::BufferUsageFlagBits::eStorageBuffer,
							  vk::SharingMode::eExclusive,
							  0,
							  nullptr,
							  nullptr};

	Com_Memset(&memory_requirements, 0, sizeof(memory_requirements));

	vk_inst.storage.buffer = vk_inst.device.createBuffer(desc);
	// VK_CHECK(qvkCreateBuffer(vk_inst.device, &desc, NULL, &vk_inst.storage.buffer));

	memory_requirements = vk_inst.device.getBufferMemoryRequirements(vk_inst.storage.buffer);
	// qvkGetBufferMemoryRequirements(vk_inst.device, vk_inst.storage.buffer, &memory_requirements);

	memory_type_bits = memory_requirements.memoryTypeBits;
	memory_type = find_memory_type(memory_type_bits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

	vk::MemoryAllocateInfo alloc_info{memory_requirements.size,
									  memory_type,
									  nullptr};

	vk_inst.storage.memory = vk_inst.device.allocateMemory(alloc_info);
	// VK_CHECK(qvkAllocateMemory(vk_inst.device, &alloc_info, NULL, &vk_inst.storage.memory));

	vk_inst.device.mapMemory(vk_inst.storage.memory,
							 0,
							 vk::WholeSize,
							 {},
							 (void **)&vk_inst.storage.buffer_ptr);
	// VK_CHECK(qvkMapMemory(vk_inst.device, vk_inst.storage.memory, 0, vk::WholeSize, 0, (void **)&vk_inst.storage.buffer_ptr));

	Com_Memset(vk_inst.storage.buffer_ptr, 0, memory_requirements.size);

	vk_inst.device.bindBufferMemory(vk_inst.storage.buffer, vk_inst.storage.memory, 0);
	// qvkBindBufferMemory(vk_inst.device, vk_inst.storage.buffer, vk_inst.storage.memory, 0);

	SET_OBJECT_NAME(vk_inst.storage.buffer, "storage buffer", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT);
	SET_OBJECT_NAME(vk_inst.storage.descriptor, "storage buffer", VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT);
	SET_OBJECT_NAME(vk_inst.storage.memory, "storage buffer memory", VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT);
}

#ifdef USE_VBO
void vk_release_vbo(void)
{
	if (vk_inst.vbo.vertex_buffer)
		vk_inst.device.destroyBuffer(vk_inst.vbo.vertex_buffer);
	// qvkDestroyBuffer(vk_inst.device, vk_inst.vbo.vertex_buffer, nullptr);
	vk_inst.vbo.vertex_buffer = VK_NULL_HANDLE;

	if (vk_inst.vbo.buffer_memory)
		vk_inst.device.freeMemory(vk_inst.vbo.buffer_memory);
	// qvkFreeMemory(vk_inst.device, vk_inst.vbo.buffer_memory, nullptr);
	vk_inst.vbo.buffer_memory = VK_NULL_HANDLE;
}

bool vk_alloc_vbo(const byte *vbo_data, uint32_t vbo_size)
{
	vk::MemoryRequirements vb_mem_reqs;
	vk::DeviceSize vertex_buffer_offset;
	vk::DeviceSize allocationSize;
	uint32_t memory_type_bits;
	vk::Buffer staging_vertex_buffer;
	vk::DeviceMemory staging_buffer_memory;
	vk::CommandBuffer command_buffer;
	vk::BufferCopy copyRegion[1];
	void *data;

	vk_release_vbo();

	vk::BufferCreateInfo desc{
		{},
		vbo_size,
		vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndexBuffer,

		vk::SharingMode::eExclusive,
		0,
		nullptr,
		nullptr};

	// VK_CHECK(qvkCreateBuffer(vk_inst.device, &desc, NULL, &vk_inst.vbo.vertex_buffer));
	vk_inst.vbo.vertex_buffer = vk_inst.device.createBuffer(desc);

	// staging buffer
	desc.size = vbo_size;
	desc.usage = vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndexBuffer;
	// VK_CHECK(qvkCreateBuffer(vk_inst.device, &desc, NULL, &staging_vertex_buffer));
	staging_vertex_buffer = vk_inst.device.createBuffer(desc);

	// memory requirements
	// qvkGetBufferMemoryRequirements(vk_inst.device, vk_inst.vbo.vertex_buffer, &vb_mem_reqs);
	vb_mem_reqs = vk_inst.device.getBufferMemoryRequirements(vk_inst.vbo.vertex_buffer);

	vertex_buffer_offset = 0;
	allocationSize = vertex_buffer_offset + vb_mem_reqs.size;
	memory_type_bits = vb_mem_reqs.memoryTypeBits;

	vk::MemoryAllocateInfo alloc_info{allocationSize,
									  find_memory_type(memory_type_bits, vk::MemoryPropertyFlagBits::eDeviceLocal),
									  nullptr};

	// VK_CHECK(qvkAllocateMemory(vk_inst.device, &alloc_info, NULL, &vk_inst.vbo.buffer_memory));
	vk_inst.vbo.buffer_memory = vk_inst.device.allocateMemory(alloc_info);
	// qvkBindBufferMemory(vk_inst.device, vk_inst.vbo.vertex_buffer, vk_inst.vbo.buffer_memory, vertex_buffer_offset);
	vk_inst.device.bindBufferMemory(vk_inst.vbo.vertex_buffer, vk_inst.vbo.buffer_memory, vertex_buffer_offset);

	// staging buffers

	// memory requirements
	// qvkGetBufferMemoryRequirements(vk_inst.device, staging_vertex_buffer, &vb_mem_reqs);
	vb_mem_reqs = vk_inst.device.getBufferMemoryRequirements(staging_vertex_buffer);
	vertex_buffer_offset = 0;
	allocationSize = vertex_buffer_offset + vb_mem_reqs.size;
	memory_type_bits = vb_mem_reqs.memoryTypeBits;

	alloc_info.allocationSize = allocationSize;
	alloc_info.memoryTypeIndex = find_memory_type(memory_type_bits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

	// VK_CHECK(qvkAllocateMemory(vk_inst.device, &alloc_info, NULL, &staging_buffer_memory));
	staging_buffer_memory = vk_inst.device.allocateMemory(alloc_info);
	// 	qvkBindBufferMemory(vk_inst.device, staging_vertex_buffer, staging_buffer_memory, vertex_buffer_offset);
	vk_inst.device.bindBufferMemory(staging_vertex_buffer, staging_buffer_memory, vertex_buffer_offset);

	// VK_CHECK(qvkMapMemory(vk_inst.device, staging_buffer_memory, 0, vk::WholeSize, 0, &data));
	vk_inst.device.mapMemory(staging_buffer_memory, 0, vk::WholeSize, {}, &data);
	memcpy((byte *)data + vertex_buffer_offset, vbo_data, vbo_size);
	// qvkUnmapMemory(vk_inst.device, staging_buffer_memory);
	vk_inst.device.unmapMemory(staging_buffer_memory);

	command_buffer = begin_command_buffer();
	copyRegion[0].srcOffset = 0;
	copyRegion[0].dstOffset = 0;
	copyRegion[0].size = vbo_size;
	// qvkCmdCopyBuffer(command_buffer, staging_vertex_buffer, vk_inst.vbo.vertex_buffer, 1, &copyRegion[0]);
	command_buffer.copyBuffer(staging_vertex_buffer, vk_inst.vbo.vertex_buffer, copyRegion[0]);

	end_command_buffer(command_buffer);

	// qvkDestroyBuffer(vk_inst.device, staging_vertex_buffer, NULL);
	vk_inst.device.destroyBuffer(staging_vertex_buffer);
	// qvkFreeMemory(vk_inst.device, staging_buffer_memory, NULL);
	vk_inst.device.freeMemory(staging_buffer_memory);

	SET_OBJECT_NAME(vk_inst.vbo.vertex_buffer, "static VBO", VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT);
	SET_OBJECT_NAME(vk_inst.vbo.buffer_memory, "static VBO memory", VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT);

	return true;
}
#endif

#include "../renderervk/shaders/spirv/shader_data.c"
#include "tr_main.hpp"
#define SHADER_MODULE(name) SHADER_MODULE(name, sizeof(name))

static void vk_create_shader_modules(void)
{
	int i, j, k, l;

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
					SET_OBJECT_NAME(vk_inst.modules.vert.gen[i][j][k][l], s, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT);
				}
			}
		}
	}

	// specialized depth-fragment shader
	vk_inst.modules.frag.gen0_df = SHADER_MODULE(frag_tx0_df);
	SET_OBJECT_NAME(vk_inst.modules.frag.gen0_df, "single-texture df fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT);

	// fixed-color (1.0) shader modules
	vk_inst.modules.vert.ident1[0][0][0] = SHADER_MODULE(vert_tx0_ident1);
	vk_inst.modules.vert.ident1[0][0][1] = SHADER_MODULE(vert_tx0_ident1_fog);
	vk_inst.modules.vert.ident1[0][1][0] = SHADER_MODULE(vert_tx0_ident1_env);
	vk_inst.modules.vert.ident1[0][1][1] = SHADER_MODULE(vert_tx0_ident1_env_fog);
	vk_inst.modules.vert.ident1[1][0][0] = SHADER_MODULE(vert_tx1_ident1);
	vk_inst.modules.vert.ident1[1][0][1] = SHADER_MODULE(vert_tx1_ident1_fog);
	vk_inst.modules.vert.ident1[1][1][0] = SHADER_MODULE(vert_tx1_ident1_env);
	vk_inst.modules.vert.ident1[1][1][1] = SHADER_MODULE(vert_tx1_ident1_env_fog);
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
				SET_OBJECT_NAME(vk_inst.modules.vert.ident1[i][j][k], s, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT);
			}
		}
	}

	vk_inst.modules.frag.ident1[0][0] = SHADER_MODULE(frag_tx0_ident1);
	vk_inst.modules.frag.ident1[0][1] = SHADER_MODULE(frag_tx0_ident1_fog);
	vk_inst.modules.frag.ident1[1][0] = SHADER_MODULE(frag_tx1_ident1);
	vk_inst.modules.frag.ident1[1][1] = SHADER_MODULE(frag_tx1_ident1_fog);
	for (i = 0; i < 2; i++)
	{
		const char *tx[] = {"single", "double"};
		const char *fog[] = {"", "+fog"};
		for (j = 0; j < 2; j++)
		{
			const char *s = va("%s-texture identity%s fragment module", tx[i], fog[j]);
			SET_OBJECT_NAME(vk_inst.modules.frag.ident1[i][j], s, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT);
		}
	}

	vk_inst.modules.vert.fixed[0][0][0] = SHADER_MODULE(vert_tx0_fixed);
	vk_inst.modules.vert.fixed[0][0][1] = SHADER_MODULE(vert_tx0_fixed_fog);
	vk_inst.modules.vert.fixed[0][1][0] = SHADER_MODULE(vert_tx0_fixed_env);
	vk_inst.modules.vert.fixed[0][1][1] = SHADER_MODULE(vert_tx0_fixed_env_fog);
	vk_inst.modules.vert.fixed[1][0][0] = SHADER_MODULE(vert_tx1_fixed);
	vk_inst.modules.vert.fixed[1][0][1] = SHADER_MODULE(vert_tx1_fixed_fog);
	vk_inst.modules.vert.fixed[1][1][0] = SHADER_MODULE(vert_tx1_fixed_env);
	vk_inst.modules.vert.fixed[1][1][1] = SHADER_MODULE(vert_tx1_fixed_env_fog);
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
				SET_OBJECT_NAME(vk_inst.modules.vert.fixed[i][j][k], s, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT);
			}
		}
	}

	vk_inst.modules.frag.fixed[0][0] = SHADER_MODULE(frag_tx0_fixed);
	vk_inst.modules.frag.fixed[0][1] = SHADER_MODULE(frag_tx0_fixed_fog);
	vk_inst.modules.frag.fixed[1][0] = SHADER_MODULE(frag_tx1_fixed);
	vk_inst.modules.frag.fixed[1][1] = SHADER_MODULE(frag_tx1_fixed_fog);
	for (i = 0; i < 2; i++)
	{
		const char *tx[] = {"single", "double"};
		const char *fog[] = {"", "+fog"};
		for (j = 0; j < 2; j++)
		{
			const char *s = va("%s-texture fixed-color%s fragment module", tx[i], fog[j]);
			SET_OBJECT_NAME(vk_inst.modules.frag.fixed[i][j], s, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT);
		}
	}

	vk_inst.modules.frag.ent[0][0] = SHADER_MODULE(frag_tx0_ent);
	vk_inst.modules.frag.ent[0][1] = SHADER_MODULE(frag_tx0_ent_fog);
	// vk_inst.modules.frag.ent[1][0] = SHADER_MODULE( frag_tx1_ent );
	// vk_inst.modules.frag.ent[1][1] = SHADER_MODULE( frag_tx1_ent_fog );
	for (i = 0; i < 1; i++)
	{
		const char *tx[] = {"single" /*, "double" */};
		const char *fog[] = {"", "+fog"};
		for (j = 0; j < 2; j++)
		{
			const char *s = va("%s-texture entity-color%s fragment module", tx[i], fog[j]);
			SET_OBJECT_NAME(vk_inst.modules.frag.ent[i][j], s, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT);
		}
	}

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
				SET_OBJECT_NAME(vk_inst.modules.frag.gen[i][j][k], s, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT);
			}
		}
	}

	vk_inst.modules.vert.light[0] = SHADER_MODULE(vert_light);
	vk_inst.modules.vert.light[1] = SHADER_MODULE(vert_light_fog);
	SET_OBJECT_NAME(vk_inst.modules.vert.light[0], "light vertex module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT);
	SET_OBJECT_NAME(vk_inst.modules.vert.light[1], "light fog vertex module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT);

	vk_inst.modules.frag.light[0][0] = SHADER_MODULE(frag_light);
	vk_inst.modules.frag.light[0][1] = SHADER_MODULE(frag_light_fog);
	vk_inst.modules.frag.light[1][0] = SHADER_MODULE(frag_light_line);
	vk_inst.modules.frag.light[1][1] = SHADER_MODULE(frag_light_line_fog);
	SET_OBJECT_NAME(vk_inst.modules.frag.light[0][0], "light fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT);
	SET_OBJECT_NAME(vk_inst.modules.frag.light[0][1], "light fog fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT);
	SET_OBJECT_NAME(vk_inst.modules.frag.light[1][0], "linear light fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT);
	SET_OBJECT_NAME(vk_inst.modules.frag.light[1][1], "linear light fog fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT);

	vk_inst.modules.color_fs = SHADER_MODULE(color_frag_spv);
	vk_inst.modules.color_vs = SHADER_MODULE(color_vert_spv);

	SET_OBJECT_NAME(vk_inst.modules.color_vs, "single-color vertex module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT);
	SET_OBJECT_NAME(vk_inst.modules.color_fs, "single-color fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT);

	vk_inst.modules.fog_vs = SHADER_MODULE(fog_vert_spv);
	vk_inst.modules.fog_fs = SHADER_MODULE(fog_frag_spv);

	SET_OBJECT_NAME(vk_inst.modules.fog_vs, "fog-only vertex module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT);
	SET_OBJECT_NAME(vk_inst.modules.fog_fs, "fog-only fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT);

	vk_inst.modules.dot_vs = SHADER_MODULE(dot_vert_spv);
	vk_inst.modules.dot_fs = SHADER_MODULE(dot_frag_spv);

	SET_OBJECT_NAME(vk_inst.modules.dot_vs, "dot vertex module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT);
	SET_OBJECT_NAME(vk_inst.modules.dot_fs, "dot fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT);

	vk_inst.modules.bloom_fs = SHADER_MODULE(bloom_frag_spv);
	vk_inst.modules.blur_fs = SHADER_MODULE(blur_frag_spv);
	vk_inst.modules.blend_fs = SHADER_MODULE(blend_frag_spv);

	SET_OBJECT_NAME(vk_inst.modules.bloom_fs, "bloom extraction fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT);
	SET_OBJECT_NAME(vk_inst.modules.blur_fs, "gaussian blur fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT);
	SET_OBJECT_NAME(vk_inst.modules.blend_fs, "final bloom blend fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT);

	vk_inst.modules.gamma_fs = SHADER_MODULE(gamma_frag_spv);
	vk_inst.modules.gamma_vs = SHADER_MODULE(gamma_vert_spv);

	SET_OBJECT_NAME(vk_inst.modules.gamma_fs, "gamma post-processing fragment module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT);
	SET_OBJECT_NAME(vk_inst.modules.gamma_vs, "gamma post-processing vertex module", VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT);
}

static void vk_alloc_persistent_pipelines(void)
{
	unsigned int state_bits;
	Vk_Pipeline_Def def;

	// skybox
	{
		Com_Memset(&def, 0, sizeof(def));
		def.shader_type = TYPE_SIGNLE_TEXTURE_FIXED_COLOR;
		def.color.rgb = tr.identityLightByte;
		def.color.alpha = tr.identityLightByte;
		def.face_culling = CT_FRONT_SIDED;
		def.polygon_offset = false;
		def.mirror = false;
		vk_inst.skybox_pipeline = vk_find_pipeline_ext(0, def, true);
	}

	// stencil shadows
	{
		cullType_t cull_types[2] = {CT_FRONT_SIDED, CT_BACK_SIDED};
		bool mirror_flags[2] = {false, true};
		int i, j;

		Com_Memset(&def, 0, sizeof(def));
		def.polygon_offset = false;
		def.state_bits = 0;
		def.shader_type = TYPE_SIGNLE_TEXTURE;
		def.shadow_phase = SHADOW_EDGES;

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
		Com_Memset(&def, 0, sizeof(def));
		def.face_culling = CT_FRONT_SIDED;
		def.polygon_offset = false;
		def.state_bits = GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO;
		def.shader_type = TYPE_SIGNLE_TEXTURE;
		def.mirror = false;
		def.shadow_phase = SHADOW_FS_QUAD;
		def.primitives = TRIANGLE_STRIP;
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
		int i, j, k, l;

		Com_Memset(&def, 0, sizeof(def));
		def.shader_type = TYPE_SIGNLE_TEXTURE;
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
					def.shader_type = TYPE_FOG_ONLY;
#else
					def.shader_type = TYPE_SIGNLE_TEXTURE;
#endif
					def.state_bits = fog_state;
					vk_inst.fog_pipelines[i][j][k] = vk_find_pipeline_ext(0, def, true);

					def.shader_type = TYPE_SIGNLE_TEXTURE;
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
		// def.shader_type = TYPE_SIGNLE_TEXTURE_LIGHTING;
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
						def.shader_type = TYPE_SIGNLE_TEXTURE_LIGHTING;
						vk_inst.dlight_pipelines_x[i][j][k][l] = vk_find_pipeline_ext(0, def, false);
						def.shader_type = TYPE_SIGNLE_TEXTURE_LIGHTING_LINEAR;
						vk_inst.dlight1_pipelines_x[i][j][k][l] = vk_find_pipeline_ext(0, def, false);
					}
				}
			}
		}
#endif // USE_PMLIGHT
	}

	// RT_BEAM surface
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE;
		def.face_culling = CT_FRONT_SIDED;
		def.primitives = TRIANGLE_STRIP;
		vk_inst.surface_beam_pipeline = vk_find_pipeline_ext(0, def, false);
	}

	// axis for missing models
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = GLS_DEFAULT;
		def.shader_type = TYPE_SIGNLE_TEXTURE;
		def.face_culling = CT_TWO_SIDED;
		def.primitives = LINE_LIST;
		if (vk_inst.wideLines)
			def.line_width = 3;
		vk_inst.surface_axis_pipeline = vk_find_pipeline_ext(0, def, false);
	}

	// flare visibility test dot
	{
		Com_Memset(&def, 0, sizeof(def));
		// def.state_bits = GLS_DEFAULT;
		def.face_culling = CT_TWO_SIDED;
		def.shader_type = TYPE_DOT;
		def.primitives = POINT_LIST;
		vk_inst.dot_pipeline = vk_find_pipeline_ext(0, def, true);
	}

	// DrawTris()
	state_bits = GLS_POLYMODE_LINE | GLS_DEPTHMASK_TRUE;
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = state_bits;
		def.shader_type = TYPE_COLOR_WHITE;
		def.face_culling = CT_FRONT_SIDED;
		vk_inst.tris_debug_pipeline = vk_find_pipeline_ext(0, def, false);
	}
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = state_bits;
		def.shader_type = TYPE_COLOR_WHITE;
		def.face_culling = CT_BACK_SIDED;
		vk_inst.tris_mirror_debug_pipeline = vk_find_pipeline_ext(0, def, false);
	}
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = state_bits;
		def.shader_type = TYPE_COLOR_GREEN;
		def.face_culling = CT_FRONT_SIDED;
		vk_inst.tris_debug_green_pipeline = vk_find_pipeline_ext(0, def, false);
	}
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = state_bits;
		def.shader_type = TYPE_COLOR_GREEN;
		def.face_culling = CT_BACK_SIDED;
		vk_inst.tris_mirror_debug_green_pipeline = vk_find_pipeline_ext(0, def, false);
	}
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = state_bits;
		def.shader_type = TYPE_COLOR_RED;
		def.face_culling = CT_FRONT_SIDED;
		vk_inst.tris_debug_red_pipeline = vk_find_pipeline_ext(0, def, false);
	}
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = state_bits;
		def.shader_type = TYPE_COLOR_RED;
		def.face_culling = CT_BACK_SIDED;
		vk_inst.tris_mirror_debug_red_pipeline = vk_find_pipeline_ext(0, def, false);
	}

	// DrawNormals()
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = GLS_DEPTHMASK_TRUE;
		def.shader_type = TYPE_SIGNLE_TEXTURE;
		def.primitives = LINE_LIST;
		vk_inst.normals_debug_pipeline = vk_find_pipeline_ext(0, def, false);
	}

	// RB_DebugPolygon()
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE;
		def.shader_type = TYPE_SIGNLE_TEXTURE;
		vk_inst.surface_debug_pipeline_solid = vk_find_pipeline_ext(0, def, false);
	}
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = GLS_POLYMODE_LINE | GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE;
		def.shader_type = TYPE_SIGNLE_TEXTURE;
		def.primitives = LINE_LIST;
		vk_inst.surface_debug_pipeline_outline = vk_find_pipeline_ext(0, def, false);
	}

	// RB_ShowImages
	{
		Com_Memset(&def, 0, sizeof(def));
		def.state_bits = GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
		def.shader_type = TYPE_SIGNLE_TEXTURE;
		def.primitives = TRIANGLE_STRIP;
		vk_inst.images_debug_pipeline = vk_find_pipeline_ext(0, def, false);
	}
}

void vk_create_blur_pipeline(uint32_t index, uint32_t width, uint32_t height, bool horizontal_pass);

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
	VkImage descriptor;
	VkImageView *image_view;
	VkImageUsageFlags usage;
	VkMemoryRequirements reqs;
	uint32_t memoryTypeIndex;
	VkDeviceSize memory_offset;
	// for layout transition:
	VkImageAspectFlags aspect_flags;
	VkImageLayout image_layout;
	VkFormat image_format;
} vk_attach_desc_t;

static vk_attach_desc_t attachments[MAX_ATTACHMENTS_IN_POOL];
static uint32_t num_attachments = 0;

static void vk_clear_attachment_pool(void)
{
	num_attachments = 0;
}

static void vk_alloc_attachments(void)
{
	VkImageViewCreateInfo view_desc;
	VkMemoryDedicatedAllocateInfoKHR alloc_info2;
	VkMemoryAllocateInfo alloc_info;
	VkCommandBuffer command_buffer;
	VkDeviceMemory memory;
	VkDeviceSize offset;
	uint32_t memoryTypeBits;
	uint32_t memoryTypeIndex;
	uint32_t i;

	if (num_attachments == 0)
	{
		return;
	}

	if (vk_inst.image_memory_count >= arrayLen(vk_inst.image_memory))
	{
		ri.Error(ERR_DROP, "vk_inst.image_memory_count == %i", (int)arrayLen(vk_inst.image_memory));
	}

	memoryTypeBits = ~0U;
	offset = 0;

	for (i = 0; i < num_attachments; i++)
	{
#ifdef MIN_IMAGE_ALIGN
		VkDeviceSize alignment = MAX(attachments[i].reqs.alignment, MIN_IMAGE_ALIGN);
#else
		VkDeviceSize alignment = attachments[i].reqs.alignment;
#endif
		memoryTypeBits &= attachments[i].reqs.memoryTypeBits;
		offset = PAD(offset, alignment);
		attachments[i].memory_offset = offset;
		offset += attachments[i].reqs.size;
#ifdef _DEBUG
		ri.Printf(PRINT_ALL, S_COLOR_CYAN "[%i] type %i, size %i, align %i\n", i,
				  attachments[i].reqs.memoryTypeBits,
				  (int)attachments[i].reqs.size,
				  (int)attachments[i].reqs.alignment);
#endif
	}

	if (num_attachments == 1 && attachments[0].usage & VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT)
	{
		// try lazy memory
		memoryTypeIndex = find_memory_type2(memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT, nullptr);
		if (memoryTypeIndex == ~0U)
		{
			memoryTypeIndex = find_memory_type(memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);
		}
	}
	else
	{
		memoryTypeIndex = find_memory_type(memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);
	}

#ifdef _DEBUG
	ri.Printf(PRINT_ALL, "memory type bits: %04x\n", memoryTypeBits);
	ri.Printf(PRINT_ALL, "memory type index: %04x\n", memoryTypeIndex);
	ri.Printf(PRINT_ALL, "total size: %i\n", (int)offset);
#endif

	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = nullptr;
	alloc_info.allocationSize = offset;
	alloc_info.memoryTypeIndex = memoryTypeIndex;

	if (num_attachments == 1)
	{
		if (vk_inst.dedicatedAllocation)
		{
			Com_Memset(&alloc_info2, 0, sizeof(alloc_info2));
			alloc_info2.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO_KHR;
			alloc_info2.image = attachments[0].descriptor;
			alloc_info.pNext = &alloc_info2;
		}
	}

	// allocate and bind memory
	VK_CHECK(qvkAllocateMemory(vk_inst.device, &alloc_info, nullptr, &memory));

	vk_inst.image_memory[vk_inst.image_memory_count++] = memory;

	for (i = 0; i < num_attachments; i++)
	{

		VK_CHECK(qvkBindImageMemory(vk_inst.device, attachments[i].descriptor, memory, attachments[i].memory_offset));

		// create color image view
		view_desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view_desc.pNext = nullptr;
		view_desc.flags = 0;
		view_desc.image = attachments[i].descriptor;
		view_desc.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view_desc.format = attachments[i].image_format;
		view_desc.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		view_desc.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		view_desc.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		view_desc.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		view_desc.subresourceRange.aspectMask = attachments[i].aspect_flags;
		view_desc.subresourceRange.baseMipLevel = 0;
		view_desc.subresourceRange.levelCount = 1;
		view_desc.subresourceRange.baseArrayLayer = 0;
		view_desc.subresourceRange.layerCount = 1;

		VK_CHECK(qvkCreateImageView(vk_inst.device, &view_desc, nullptr, attachments[i].image_view));
	}

	// perform layout transition
	command_buffer = begin_command_buffer();
	for (i = 0; i < num_attachments; i++)
	{
		record_image_layout_transition(command_buffer,
									   attachments[i].descriptor,
									   vk::ImageAspectFlagBits(attachments[i].aspect_flags),
									   vk::ImageLayout::eUndefined, // old_layout
									   vk::ImageLayout(attachments[i].image_layout));
	}
	end_command_buffer(command_buffer);

	num_attachments = 0;
}

static void vk_add_attachment_desc(VkImage desc, VkImageView *image_view, VkImageUsageFlags usage, VkMemoryRequirements *reqs, VkFormat image_format, VkImageAspectFlags aspect_flags, VkImageLayout image_layout)
{
	if (num_attachments >= arrayLen(attachments))
	{
		ri.Error(ERR_FATAL, "Attachments array overflow");
	}
	else
	{
		attachments[num_attachments].descriptor = desc;
		attachments[num_attachments].image_view = image_view;
		attachments[num_attachments].usage = usage;
		attachments[num_attachments].reqs = *reqs;
		attachments[num_attachments].aspect_flags = aspect_flags;
		attachments[num_attachments].image_layout = image_layout;
		attachments[num_attachments].image_format = image_format;
		attachments[num_attachments].memory_offset = 0;
		num_attachments++;
	}
}

static void vk_get_image_memory_erquirements(VkImage image, VkMemoryRequirements *memory_requirements)
{
	if (vk_inst.dedicatedAllocation)
	{
		VkMemoryRequirements2KHR memory_requirements2;
		VkImageMemoryRequirementsInfo2KHR image_requirements2;
		VkMemoryDedicatedRequirementsKHR mem_req2;

		Com_Memset(&mem_req2, 0, sizeof(mem_req2));
		mem_req2.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS_KHR;

		image_requirements2.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2_KHR;
		image_requirements2.image = image;
		image_requirements2.pNext = nullptr;

		Com_Memset(&memory_requirements2, 0, sizeof(memory_requirements2));
		memory_requirements2.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2_KHR;
		memory_requirements2.pNext = &mem_req2;

		qvkGetImageMemoryRequirements2KHR(vk_inst.device, &image_requirements2, &memory_requirements2);

		*memory_requirements = memory_requirements2.memoryRequirements;
	}
	else
	{
		qvkGetImageMemoryRequirements(vk_inst.device, image, memory_requirements);
	}
}

static void create_color_attachment(uint32_t width, uint32_t height, VkSampleCountFlagBits samples, VkFormat format,
									VkImageUsageFlags usage, VkImage *image, VkImageView *image_view, VkImageLayout image_layout, bool multisample)
{
	VkImageCreateInfo create_desc;
	VkMemoryRequirements memory_requirements;

	if (multisample && !(usage & VK_IMAGE_USAGE_SAMPLED_BIT))
		usage |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;

	// create color image
	create_desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	create_desc.pNext = nullptr;
	create_desc.flags = 0;
	create_desc.imageType = VK_IMAGE_TYPE_2D;
	create_desc.format = format;
	create_desc.extent.width = width;
	create_desc.extent.height = height;
	create_desc.extent.depth = 1;
	create_desc.mipLevels = 1;
	create_desc.arrayLayers = 1;
	create_desc.samples = samples;
	create_desc.tiling = VK_IMAGE_TILING_OPTIMAL;
	create_desc.usage = usage;
	create_desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	create_desc.queueFamilyIndexCount = 0;
	create_desc.pQueueFamilyIndices = nullptr;
	create_desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	VK_CHECK(qvkCreateImage(vk_inst.device, &create_desc, nullptr, image));

	vk_get_image_memory_erquirements(*image, &memory_requirements);

	vk_add_attachment_desc(*image, image_view, usage, &memory_requirements, format, VK_IMAGE_ASPECT_COLOR_BIT, image_layout);
}

static void create_depth_attachment(uint32_t width, uint32_t height, VkSampleCountFlagBits samples, VkImage *image, VkImageView *image_view, bool allowTransient)
{
	VkImageCreateInfo create_desc;
	VkMemoryRequirements memory_requirements;
	VkImageAspectFlags image_aspect_flags;

	// create depth image
	create_desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	create_desc.pNext = nullptr;
	create_desc.flags = 0;
	create_desc.imageType = VK_IMAGE_TYPE_2D;
	create_desc.format = vk_inst.depth_format;
	create_desc.extent.width = width;
	create_desc.extent.height = height;
	create_desc.extent.depth = 1;
	create_desc.mipLevels = 1;
	create_desc.arrayLayers = 1;
	create_desc.samples = samples;
	create_desc.tiling = VK_IMAGE_TILING_OPTIMAL;
	create_desc.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	if (allowTransient)
	{
		create_desc.usage |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
	}
	create_desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	create_desc.queueFamilyIndexCount = 0;
	create_desc.pQueueFamilyIndices = nullptr;
	create_desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	image_aspect_flags = VK_IMAGE_ASPECT_DEPTH_BIT;
	if (r_stencilbits->integer)
		image_aspect_flags |= VK_IMAGE_ASPECT_STENCIL_BIT;

	VK_CHECK(qvkCreateImage(vk_inst.device, &create_desc, nullptr, image));

	vk_get_image_memory_erquirements(*image, &memory_requirements);

	vk_add_attachment_desc(*image, image_view, create_desc.usage, &memory_requirements, vk_inst.depth_format, image_aspect_flags, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
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

		VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

		// bloom
		if (r_bloom->integer)
		{
			uint32_t width = gls.captureWidth;
			uint32_t height = gls.captureHeight;

			create_color_attachment(width, height, VK_SAMPLE_COUNT_1_BIT, vk_inst.bloom_format,
									usage, &vk_inst.bloom_image[0], &vk_inst.bloom_image_view[0], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, false);

			for (i = 1; i < arrayLen(vk_inst.bloom_image); i += 2)
			{
				width /= 2;
				height /= 2;
				create_color_attachment(width, height, VK_SAMPLE_COUNT_1_BIT, vk_inst.bloom_format,
										usage, &vk_inst.bloom_image[i + 0], &vk_inst.bloom_image_view[i + 0], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, false);

				create_color_attachment(width, height, VK_SAMPLE_COUNT_1_BIT, vk_inst.bloom_format,
										usage, &vk_inst.bloom_image[i + 1], &vk_inst.bloom_image_view[i + 1], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, false);
			}
		}

		// post-processing/msaa-resolve
		create_color_attachment(glConfig.vidWidth, glConfig.vidHeight, VK_SAMPLE_COUNT_1_BIT, vk_inst.color_format,
								usage | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, &vk_inst.color_image, &vk_inst.color_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, false);

		// screenmap-msaa
		if (vk_inst.screenMapSamples > vk::SampleCountFlagBits::e1)
		{
			create_color_attachment(vk_inst.screenMapWidth, vk_inst.screenMapHeight, VkSampleCountFlagBits(vk_inst.screenMapSamples), vk_inst.color_format,
									VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &vk_inst.screenMap.color_image_msaa, &vk_inst.screenMap.color_image_view_msaa, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, true);
		}

		// screenmap/msaa-resolve
		create_color_attachment(vk_inst.screenMapWidth, vk_inst.screenMapHeight, VK_SAMPLE_COUNT_1_BIT, vk_inst.color_format,
								usage, &vk_inst.screenMap.color_image, &vk_inst.screenMap.color_image_view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, false);

		// screenmap depth
		create_depth_attachment(vk_inst.screenMapWidth, vk_inst.screenMapHeight, VkSampleCountFlagBits(vk_inst.screenMapSamples), &vk_inst.screenMap.depth_image, &vk_inst.screenMap.depth_image_view, true);

		if (vk_inst.msaaActive)
		{
			create_color_attachment(glConfig.vidWidth, glConfig.vidHeight, vkSamples, vk_inst.color_format,
									VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &vk_inst.msaa_image, &vk_inst.msaa_image_view, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, true);
		}

		if (r_ext_supersample->integer)
		{
			// capture buffer
			usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
			create_color_attachment(gls.captureWidth, gls.captureHeight, VK_SAMPLE_COUNT_1_BIT, vk_inst.capture_format,
									usage, &vk_inst.capture.image, &vk_inst.capture.image_view, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, false);
		}
	} // if ( vk_inst.fboActive )

	// vk_alloc_attachments();

	create_depth_attachment(glConfig.vidWidth, glConfig.vidHeight, vkSamples, &vk_inst.depth_image, &vk_inst.depth_image_view,
							(vk_inst.fboActive && r_bloom->integer) ? false : true);

	vk_alloc_attachments();

	for (i = 0; i < vk_inst.image_memory_count; i++)
	{
		SET_OBJECT_NAME(vk_inst.image_memory[i], va("framebuffer memory chunk %i", i), VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_MEMORY_EXT);
	}

	SET_OBJECT_NAME(vk_inst.depth_image, "depth attachment", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT);
	SET_OBJECT_NAME(vk_inst.depth_image_view, "depth attachment", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT);

	SET_OBJECT_NAME(vk_inst.color_image, "color attachment", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT);
	SET_OBJECT_NAME(vk_inst.color_image_view, "color attachment", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT);

	SET_OBJECT_NAME(vk_inst.capture.image, "capture image", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT);
	SET_OBJECT_NAME(vk_inst.capture.image_view, "capture image view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT);

	for (i = 0; i < arrayLen(vk_inst.bloom_image); i++)
	{
		SET_OBJECT_NAME(vk_inst.bloom_image[i], va("bloom attachment %i", i), VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT);
		SET_OBJECT_NAME(vk_inst.bloom_image_view[i], va("bloom attachment %i", i), VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT);
	}
}

static void vk_create_framebuffers(void)
{
	VkImageView attachments[3];
	VkFramebufferCreateInfo desc;
	uint32_t n;

	desc.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	desc.pNext = nullptr;
	desc.flags = 0;
	desc.pAttachments = attachments;
	desc.layers = 1;

	for (n = 0; n < vk_inst.swapchain_image_count; n++)
	{
		desc.renderPass = vk_inst.render_pass.main;
		desc.attachmentCount = 2;
		if (r_fbo->integer == 0)
		{
			desc.width = gls.windowWidth;
			desc.height = gls.windowHeight;
			attachments[0] = vk_inst.swapchain_image_views[n];
			attachments[1] = vk_inst.depth_image_view;
			VK_CHECK(qvkCreateFramebuffer(vk_inst.device, &desc, nullptr, &vk_inst.framebuffers.main[n]));

			SET_OBJECT_NAME(vk_inst.framebuffers.main[n], va("framebuffer - main %i", n), VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT);
		}
		else
		{
			// same framebuffer configuration for main and post-bloom render passes
			if (n == 0)
			{
				desc.width = glConfig.vidWidth;
				desc.height = glConfig.vidHeight;
				attachments[0] = vk_inst.color_image_view;
				attachments[1] = vk_inst.depth_image_view;
				if (vk_inst.msaaActive)
				{
					desc.attachmentCount = 3;
					attachments[2] = vk_inst.msaa_image_view;
				}
				VK_CHECK(qvkCreateFramebuffer(vk_inst.device, &desc, nullptr, &vk_inst.framebuffers.main[n]));
				SET_OBJECT_NAME(vk_inst.framebuffers.main[n], "framebuffer - main", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT);
			}
			else
			{
				vk_inst.framebuffers.main[n] = vk_inst.framebuffers.main[0];
			}

			// gamma correction
			desc.renderPass = vk_inst.render_pass.gamma;
			desc.attachmentCount = 1;
			desc.width = gls.windowWidth;
			desc.height = gls.windowHeight;
			attachments[0] = vk_inst.swapchain_image_views[n];
			VK_CHECK(qvkCreateFramebuffer(vk_inst.device, &desc, nullptr, &vk_inst.framebuffers.gamma[n]));

			SET_OBJECT_NAME(vk_inst.framebuffers.gamma[n], "framebuffer - gamma-correction", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT);
		}
	}

	if (vk_inst.fboActive)
	{
		// screenmap
		desc.renderPass = vk_inst.render_pass.screenmap;
		desc.attachmentCount = 2;
		desc.width = vk_inst.screenMapWidth;
		desc.height = vk_inst.screenMapHeight;
		attachments[0] = vk_inst.screenMap.color_image_view;
		attachments[1] = vk_inst.screenMap.depth_image_view;
		if (vk_inst.screenMapSamples > vk::SampleCountFlagBits::e1)
		{
			desc.attachmentCount = 3;
			attachments[2] = vk_inst.screenMap.color_image_view_msaa;
		}
		VK_CHECK(qvkCreateFramebuffer(vk_inst.device, &desc, nullptr, &vk_inst.framebuffers.screenmap));
		SET_OBJECT_NAME(vk_inst.framebuffers.screenmap, "framebuffer - screenmap", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT);

		if (vk_inst.capture.image != VK_NULL_HANDLE)
		{
			attachments[0] = vk_inst.capture.image_view;

			desc.renderPass = vk_inst.render_pass.capture;
			desc.pAttachments = attachments;
			desc.attachmentCount = 1;
			desc.width = gls.captureWidth;
			desc.height = gls.captureHeight;

			VK_CHECK(qvkCreateFramebuffer(vk_inst.device, &desc, nullptr, &vk_inst.framebuffers.capture));
			SET_OBJECT_NAME(vk_inst.framebuffers.capture, "framebuffer - capture", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT);
		}

		if (r_bloom->integer)
		{
			uint32_t width = gls.captureWidth;
			uint32_t height = gls.captureHeight;

			// bloom color extraction
			desc.renderPass = vk_inst.render_pass.bloom_extract;
			desc.width = width;
			desc.height = height;

			desc.attachmentCount = 1;
			attachments[0] = vk_inst.bloom_image_view[0];

			VK_CHECK(qvkCreateFramebuffer(vk_inst.device, &desc, nullptr, &vk_inst.framebuffers.bloom_extract));

			SET_OBJECT_NAME(vk_inst.framebuffers.bloom_extract, "framebuffer - bloom extraction", VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT);

			for (n = 0; n < arrayLen(vk_inst.framebuffers.blur); n += 2)
			{
				width /= 2;
				height /= 2;

				desc.renderPass = vk_inst.render_pass.blur[n];
				desc.width = width;
				desc.height = height;

				desc.attachmentCount = 1;

				attachments[0] = vk_inst.bloom_image_view[n + 0 + 1];
				VK_CHECK(qvkCreateFramebuffer(vk_inst.device, &desc, nullptr, &vk_inst.framebuffers.blur[n + 0]));

				attachments[0] = vk_inst.bloom_image_view[n + 1 + 1];
				VK_CHECK(qvkCreateFramebuffer(vk_inst.device, &desc, nullptr, &vk_inst.framebuffers.blur[n + 1]));

				SET_OBJECT_NAME(vk_inst.framebuffers.blur[n + 0], va("framebuffer - blur %i", n + 0), VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT);
				SET_OBJECT_NAME(vk_inst.framebuffers.blur[n + 1], va("framebuffer - blur %i", n + 1), VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT);
			}
		}
	}
}

static void vk_create_sync_primitives(void)
{

	uint32_t i;

	// all commands submitted
	for (i = 0; i < NUM_COMMAND_BUFFERS; i++)
	{
		vk::SemaphoreCreateInfo desc{{}, nullptr};

		// swapchain image acquired
		// VK_CHECK(qvkCreateSemaphore(vk_inst.device, &desc, nullptr, &vk_inst.tess[i].image_acquired));
		vk_inst.tess[i].image_acquired = vk_inst.device.createSemaphore(desc);

		// VK_CHECK(qvkCreateSemaphore(vk_inst.device, &desc, nullptr, &vk_inst.tess[i].rendering_finished));
		vk_inst.tess[i].rendering_finished = vk_inst.device.createSemaphore(desc);

		vk::FenceCreateInfo fence_desc{vk::FenceCreateFlagBits::eSignaled};

		// VK_CHECK(qvkCreateFence(vk_inst.device, &fence_desc, nullptr, &vk_inst.tess[i].rendering_finished_fence));
		vk_inst.tess[i].rendering_finished_fence = vk_inst.device.createFence(fence_desc);
		vk_inst.tess[i].waitForFence = true;

		SET_OBJECT_NAME(vk_inst.tess[i].image_acquired, va("image_acquired semaphore %i", i), VK_DEBUG_REPORT_OBJECT_TYPE_SEMAPHORE_EXT);
		SET_OBJECT_NAME(vk_inst.tess[i].rendering_finished, va("rendering_finished semaphore %i", i), VK_DEBUG_REPORT_OBJECT_TYPE_SEMAPHORE_EXT);
		SET_OBJECT_NAME(vk_inst.tess[i].rendering_finished_fence, va("rendering_finished fence %i", i), VK_DEBUG_REPORT_OBJECT_TYPE_FENCE_EXT);
	}
}

static void vk_destroy_sync_primitives(void)
{
	uint32_t i;

	for (i = 0; i < NUM_COMMAND_BUFFERS; i++)
	{
		vk_inst.device.destroySemaphore(vk_inst.tess[i].image_acquired);
		vk_inst.device.destroySemaphore(vk_inst.tess[i].rendering_finished);
		vk_inst.device.destroyFence(vk_inst.tess[i].rendering_finished_fence);
		vk_inst.tess[i].waitForFence = false;
	}
}

static void vk_destroy_framebuffers(void)
{
	uint32_t n;

	for (n = 0; n < vk_inst.swapchain_image_count; n++)
	{
		if (vk_inst.framebuffers.main[n] != VK_NULL_HANDLE)
		{
			if (!vk_inst.fboActive || n == 0)
			{
				vk_inst.device.destroyFramebuffer(vk_inst.framebuffers.main[n]);
			}
			vk_inst.framebuffers.main[n] = VK_NULL_HANDLE;
		}
		if (vk_inst.framebuffers.gamma[n] != VK_NULL_HANDLE)
		{
			vk_inst.device.destroyFramebuffer(vk_inst.framebuffers.gamma[n]);
			vk_inst.framebuffers.gamma[n] = VK_NULL_HANDLE;
		}
	}

	if (vk_inst.framebuffers.bloom_extract != VK_NULL_HANDLE)
	{
		vk_inst.device.destroyFramebuffer(vk_inst.framebuffers.bloom_extract);
		vk_inst.framebuffers.bloom_extract = VK_NULL_HANDLE;
	}

	if (vk_inst.framebuffers.screenmap != VK_NULL_HANDLE)
	{
		vk_inst.device.destroyFramebuffer(vk_inst.framebuffers.screenmap);
		vk_inst.framebuffers.screenmap = VK_NULL_HANDLE;
	}

	if (vk_inst.framebuffers.capture != VK_NULL_HANDLE)
	{
		vk_inst.device.destroyFramebuffer(vk_inst.framebuffers.capture);
		vk_inst.framebuffers.capture = VK_NULL_HANDLE;
	}

	for (n = 0; n < arrayLen(vk_inst.framebuffers.blur); n++)
	{
		if (vk_inst.framebuffers.blur[n] != VK_NULL_HANDLE)
		{
			vk_inst.device.destroyFramebuffer(vk_inst.framebuffers.blur[n]);
			vk_inst.framebuffers.blur[n] = VK_NULL_HANDLE;
		}
	}
}

static void vk_destroy_swapchain(void)
{
	uint32_t i;

	for (i = 0; i < vk_inst.swapchain_image_count; i++)
	{
		if (vk_inst.swapchain_image_views[i] != VK_NULL_HANDLE)
		{
			vk_inst.device.destroyImageView(vk_inst.swapchain_image_views[i]);
			vk_inst.swapchain_image_views[i] = VK_NULL_HANDLE;
		}
	}

	vk_inst.device.destroySwapchainKHR(vk_inst.swapchain);
}

static void vk_destroy_attachments(void);
static void vk_destroy_render_passes(void);
static void vk_destroy_pipelines(bool resetCount);

static void vk_restart_swapchain(const char *funcname)
{
	uint32_t i;
	ri.Printf(PRINT_WARNING, "%s(): restarting swapchain...\n", funcname);

	vk_wait_idle();

	for (i = 0; i < NUM_COMMAND_BUFFERS; i++)
	{
		qvkResetCommandBuffer(vk_inst.tess[i].command_buffer, 0);
	}

	vk_destroy_pipelines(false);
	vk_destroy_framebuffers();
	vk_destroy_render_passes();
	vk_destroy_attachments();
	vk_destroy_swapchain();
	vk_destroy_sync_primitives();

	vk_select_surface_format(vk_inst.physical_device, vk_surface);
	setup_surface_formats(vk_inst.physical_device);

	vk_create_sync_primitives();

	vk_create_swapchain(vk_inst.physical_device, vk_inst.device, vk_surface, vk_inst.present_format, &vk_inst.swapchain);
	vk_create_attachments();
	vk_create_render_passes();
	vk_create_framebuffers();

	vk_update_attachment_descriptors();

	vk_update_post_process_pipelines();
}

static void vk_set_render_scale(void)
{
	if (gls.windowWidth != glConfig.vidWidth || gls.windowHeight != glConfig.vidHeight)
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

	// qvkGetDeviceQueue(vk_inst.device, vk_inst.queue_family_index, 0, &vk_inst.queue);
	vk_inst.queue = vk_inst.device.getQueue(vk_inst.queue_family_index, 0);

	// qvkGetPhysicalDeviceProperties(vk_inst.physical_device, &props);
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

	vkMaxSamples = MIN(static_cast<VkSampleCountFlags>(props.limits.sampledImageColorSampleCounts), static_cast<VkSampleCountFlags>(props.limits.sampledImageDepthSampleCounts));

	if (/*vk_inst.fboActive &&*/ vk_inst.msaaActive)
	{
		VkSampleCountFlags mask = vkMaxSamples;
		vkSamples = convertToVkSampleCountFlagBits(std::max(log2pad_plus(r_ext_multisample->integer, 1), static_cast<unsigned int>(VK_SAMPLE_COUNT_2_BIT)));
		while (vkSamples > mask)
		{
			int shiftAmount = 1;
			vkSamples = static_cast<VkSampleCountFlagBits>(vkSamples >> shiftAmount);
		}
		ri.Printf(PRINT_ALL, "...using %ix MSAA\n", vkSamples);
	}
	else
	{
		vkSamples = VK_SAMPLE_COUNT_1_BIT;
	}

	vk_inst.screenMapSamples = vk::SampleCountFlagBits(MIN(vkMaxSamples, (int)vk::SampleCountFlagBits::e4));

	vk_inst.screenMapWidth = (float)glConfig.vidWidth / 16.0;
	if (vk_inst.screenMapWidth < 4)
		vk_inst.screenMapWidth = 4;

	vk_inst.screenMapHeight = (float)glConfig.vidHeight / 16.0;
	if (vk_inst.screenMapHeight < 4)
		vk_inst.screenMapHeight = 4;

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

	glConfig.textureEnvAddAvailable = true;
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
	VkPhysicalDeviceProperties a = static_cast<VkPhysicalDeviceProperties>(props);
	Q_strncpyz(glConfig.renderer_string, renderer_name(&a), sizeof(glConfig.renderer_string));

	SET_OBJECT_NAME((intptr_t) static_cast<VkDevice>(vk_inst.device), glConfig.renderer_string, VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_EXT);

	//
	// Sync primitives.
	//
	vk_create_sync_primitives();

	//
	// Command pool.
	//
	{
		vk::CommandPoolCreateInfo desc{vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer, vk_inst.queue_family_index};
		vk_inst.command_pool = vk_inst.device.createCommandPool(desc);
		// VK_CHECK(qvkCreateCommandPool(vk_inst.device, &desc, nullptr, &vk_inst.command_pool));

		SET_OBJECT_NAME(vk_inst.command_pool, "command pool", VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_POOL_EXT);
	}

	//
	// Command buffers and color attachments.
	//
	vk::CommandBufferAllocateInfo alloc_info{
		vk_inst.command_pool,			  // Command pool to allocate from
		vk::CommandBufferLevel::ePrimary, // Level of the command buffers
		NUM_COMMAND_BUFFERS				  // Number of command buffers to allocate
	};

	// Allocate all command buffers at once
	std::vector<vk::CommandBuffer> command_buffers = vk_inst.device.allocateCommandBuffers(alloc_info);

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
		vk_inst.descriptor_pool = vk_inst.device.createDescriptorPool(desc);

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
			vk_inst.set_layout_storage, // storage for testing flare visibility
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
			vk::PipelineLayoutCreateFlags{},									   // flags
			(vk_inst.maxBoundDescriptorSets >= VK_DESC_COUNT) ? VK_DESC_COUNT : 4, // setLayoutCount
			set_layouts.data(),													   // pSetLayouts
			1,																	   // pushConstantRangeCount
			&push_range															   // pPushConstantRanges
		};

		// Create the pipeline layout
		vk_inst.pipeline_layout = vk_inst.device.createPipelineLayout(desc);

		// VK_CHECK(qvkCreatePipelineLayout(vk_inst.device, &desc, nullptr, &vk_inst.pipeline_layout));

		// flare test pipeline
#if 0
		set_layouts[0] = vk_inst.set_layout_storage; // dynamic storage buffer

		desc.setLayoutCount = 1;
		vk_inst.pipeline_layout_storage = vk_inst.device.createPipelineLayout(desc);


	//	VK_CHECK( qvkCreatePipelineLayout( vk_inst.device, &desc, nullptr, &vk_inst.pipeline_layout_storage ) );
#endif

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
		vk_inst.pipeline_layout_post_process = vk_inst.device.createPipelineLayout(desc);

		// VK_CHECK(qvkCreatePipelineLayout(vk_inst.device, &desc, nullptr, &vk_inst.pipeline_layout_post_process));

		desc.setLayoutCount = VK_NUM_BLOOM_PASSES;

		vk_inst.pipeline_layout_blend = vk_inst.device.createPipelineLayout(desc);
		// VK_CHECK(qvkCreatePipelineLayout(vk_inst.device, &desc, nullptr, &vk_inst.pipeline_layout_blend));

		SET_OBJECT_NAME(vk_inst.pipeline_layout, "pipeline layout - main", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT);
		SET_OBJECT_NAME(vk_inst.pipeline_layout_post_process, "pipeline layout - post-processing", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT);
		SET_OBJECT_NAME(vk_inst.pipeline_layout_blend, "pipeline layout - blend", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT);
	}

	vk_inst.geometry_buffer_size_new = VERTEX_BUFFER_SIZE;
	vk_create_geometry_buffers(vk_inst.geometry_buffer_size_new);
	vk_inst.geometry_buffer_size_new = 0;

	vk_create_storage_buffer(MAX_FLARES * vk_inst.storage_alignment);

	vk_create_shader_modules();

	{
		vk::PipelineCacheCreateInfo ci{};
		Com_Memset(&ci, 0, sizeof(ci));
		// ci.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
		vk_inst.pipelineCache = vk_inst.device.createPipelineCache(ci);
		// VK_CHECK(qvkCreatePipelineCache(vk_inst.device, &ci, nullptr, &vk_inst.pipelineCache));
	}

	vk_inst.renderPassIndex = RENDER_PASS_MAIN; // default render pass

	// swapchain
	vk_inst.initSwapchainLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	// vk_inst.initSwapchainLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	vk_create_swapchain(vk_inst.physical_device, vk_inst.device, vk_surface, vk_inst.present_format, &vk_inst.swapchain);

	// color/depth attachments
	vk_create_attachments();

	// renderpasses
	vk_create_render_passes();

	// framebuffers for each swapchain image
	vk_create_framebuffers();

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
			// qvkDestroyImage(vk_inst.device, vk_inst.bloom_image[i], nullptr);
			vk_inst.device.destroyImage(vk_inst.bloom_image[i]);
			// qvkDestroyImageView(vk_inst.device, vk_inst.bloom_image_view[i], nullptr);
			vk_inst.device.destroyImageView(vk_inst.bloom_image_view[i]);
			vk_inst.bloom_image[i] = VK_NULL_HANDLE;
			vk_inst.bloom_image_view[i] = VK_NULL_HANDLE;
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
		// qvkFreeMemory(vk_inst.device, vk_inst.image_memory[i], NULL);
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

	for (int i = 0; i < arrayLen(vk_inst.render_pass.blur); i++)
	{
		if (vk_inst.render_pass.blur[i] != VK_NULL_HANDLE)
		{
			vk_inst.device.destroyRenderPass(vk_inst.render_pass.blur[i]);
			vk_inst.render_pass.blur[i] = VK_NULL_HANDLE;
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
		std::memset(&vk_inst.pipelines, 0, sizeof(vk_inst.pipelines));
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

void vk_shutdown(refShutdownCode_t code)
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
	vk_inst.device.destroyPipelineLayout(vk_inst.pipeline_layout_post_process);
	vk_inst.device.destroyPipelineLayout(vk_inst.pipeline_layout_blend);

#ifdef USE_VBO
	vk_release_vbo();
#endif

	vk_release_geometry_buffers();
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

	std::memset(&vk_inst, 0, sizeof(vk_inst));
	std::memset(&vk_world, 0, sizeof(vk_world));

	if (code != REF_KEEP_CONTEXT)
	{
		vk_destroy_instance();
		deinit_instance_functions();
	}
}

void vk_wait_idle(void)
{
	// VK_CHECK(qvkDeviceWaitIdle(vk_inst.device));
	vk_inst.device.waitIdle();
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

	// Destroy staging buffer if it exists
	if (vk_world.staging_buffer)
	{
		vk_inst.device.destroyBuffer(vk_world.staging_buffer);
		vk_world.staging_buffer = nullptr;
	}

	// Free staging buffer memory if it exists
	if (vk_world.staging_buffer_memory)
	{
		vk_inst.device.freeMemory(vk_world.staging_buffer_memory);
		vk_world.staging_buffer_memory = nullptr;
	}

	// Destroy samplers
	for (i = 0; i < static_cast<uint32_t>(vk_world.num_samplers); ++i)
	{
		vk_inst.device.destroySampler(vk_world.samplers[i]);
		vk_world.samplers[i] = nullptr;
	}

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
		std::memset(&vk_inst.pipelines[i], 0, sizeof(vk_inst.pipelines[0]));
	}
	vk_inst.pipelines_count = vk_inst.pipelines_world_base;

	// Reset descriptor pool
	VK_CHECK(vk_inst.device.resetDescriptorPool(vk_inst.descriptor_pool));

	// Adjust image chunk size based on usage
	if (vk_world.num_image_chunks > 1)
	{
		vk_inst.image_chunk_size = (IMAGE_CHUNK_SIZE * 2);
	}
	else if (vk_world.num_image_chunks == 1)
	{
		if (vk_world.image_chunks[0].used < (IMAGE_CHUNK_SIZE - (IMAGE_CHUNK_SIZE / 10)))
		{
			vk_inst.image_chunk_size = IMAGE_CHUNK_SIZE;
		}
	}

	std::memset(&vk_world, 0, sizeof(vk_world));

	// Reset geometry buffers offsets
	for (i = 0; i < NUM_COMMAND_BUFFERS; ++i)
	{
		vk_inst.tess[i].uniform_read_offset = 0;
		vk_inst.tess[i].vertex_buffer_offset = 0;
	}

	std::memset(vk_inst.cmd->buf_offset, 0, sizeof(vk_inst.cmd->buf_offset));
	std::memset(vk_inst.cmd->vbo_offset, 0, sizeof(vk_inst.cmd->vbo_offset));

	std::memset(&vk_inst.stats, 0, sizeof(vk_inst.stats));
}

static void record_buffer_memory_barrier(vk::CommandBuffer cb, vk::Buffer buffer,
										 vk::DeviceSize size, vk::PipelineStageFlags src_stages,
										 vk::PipelineStageFlags dst_stages, vk::AccessFlags src_access,
										 vk::AccessFlags dst_access)
{
	vk::BufferMemoryBarrier barrier = {src_access,
									   dst_access,
									   vk::QueueFamilyIgnored,
									   vk::QueueFamilyIgnored,
									   buffer,
									   0,
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

void vk_create_image(image_t &image, int width, int height, int mip_levels)
{

	VkFormat format = static_cast<VkFormat>(image.internalFormat);

	if (image.handle)
	{
		qvkDestroyImage(vk_inst.device, image.handle, nullptr);
		image.handle = VK_NULL_HANDLE;
	}

	if (image.view)
	{
		qvkDestroyImageView(vk_inst.device, image.view, nullptr);
		image.view = VK_NULL_HANDLE;
	}

	// create image
	{
		VkImageCreateInfo desc;

		desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		desc.pNext = nullptr;
		desc.flags = 0;
		desc.imageType = VK_IMAGE_TYPE_2D;
		desc.format = format;
		desc.extent.width = width;
		desc.extent.height = height;
		desc.extent.depth = 1;
		desc.mipLevels = mip_levels;
		desc.arrayLayers = 1;
		desc.samples = VK_SAMPLE_COUNT_1_BIT;
		desc.tiling = VK_IMAGE_TILING_OPTIMAL;
		desc.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		desc.queueFamilyIndexCount = 0;
		desc.pQueueFamilyIndices = nullptr;
		desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VK_CHECK(qvkCreateImage(vk_inst.device, &desc, nullptr, &image.handle));

		allocate_and_bind_image_memory(image.handle);
	}

	// create image view
	{
		VkImageViewCreateInfo desc;

		desc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		desc.pNext = nullptr;
		desc.flags = 0;
		desc.image = image.handle;
		desc.viewType = VK_IMAGE_VIEW_TYPE_2D;
		desc.format = format;
		desc.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		desc.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		desc.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		desc.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		desc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		desc.subresourceRange.baseMipLevel = 0;
		desc.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
		desc.subresourceRange.baseArrayLayer = 0;
		desc.subresourceRange.layerCount = 1;

		VK_CHECK(qvkCreateImageView(vk_inst.device, &desc, nullptr, &image.view));
	}

	// create associated descriptor set
	if (image.descriptor == VK_NULL_HANDLE)
	{
		VkDescriptorSetAllocateInfo desc;

		desc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		desc.pNext = nullptr;
		desc.descriptorPool = vk_inst.descriptor_pool;
		desc.descriptorSetCount = 1;
		desc.pSetLayouts = &vk_inst.set_layout_sampler;

		VK_CHECK(qvkAllocateDescriptorSets(vk_inst.device, &desc, &image.descriptor));
	}

	vk_update_descriptor_set(image, mip_levels > 1 ? true : false);

	SET_OBJECT_NAME(image.handle, image.imgName, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT);
	SET_OBJECT_NAME(image.view, image.imgName, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT);
	SET_OBJECT_NAME(image.descriptor, image.imgName, VK_DEBUG_REPORT_OBJECT_TYPE_DESCRIPTOR_SET_EXT);
}

static byte *resample_image_data(const int target_format, byte *data, const int data_size, int *bytes_per_pixel)
{
	byte *buffer;
	uint16_t *p;
	int i, n;

	switch (target_format)
	{
	case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
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

	case VK_FORMAT_A1R5G5B5_UNORM_PACK16:
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

	case VK_FORMAT_B8G8R8A8_UNORM:
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

	case VK_FORMAT_R8G8B8_UNORM:
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
	int bpp;
	int buffer_size = 0;
	std::size_t num_regions = 0;

	buf = resample_image_data(image.internalFormat, pixels, size, &bpp);

	while (true)
	{
		if (num_regions >= max_regions)
		{
			break; // Avoid overflow
		}

		vk::BufferImageCopy region = {buffer_size,
									  0,
									  0,
									  {vk::ImageAspectFlagBits::eColor,
									   num_regions,
									   0,
									   1},
									  {x, y, 0},
									  {width, height, 1}};

		regions[num_regions] = region;
		num_regions++;

		buffer_size += width * height * bpp;

		if (num_regions >= mipmaps || (width == 1 && height == 1) || num_regions >= max_regions)
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

	ensure_staging_buffer_allocation(buffer_size);
	std::memcpy(vk_world.staging_buffer_ptr, buf, buffer_size);

	command_buffer = begin_command_buffer();

	record_buffer_memory_barrier(command_buffer, vk_world.staging_buffer, vk::WholeSize, vk::PipelineStageFlagBits::eHost, vk::PipelineStageFlagBits::eTransfer, vk::AccessFlagBits::eHostWrite, vk::AccessFlagBits::eTransferRead);

	if (update)
	{
		record_image_layout_transition(command_buffer, image.handle, vk::ImageAspectFlagBits::eColor, vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageLayout::eTransferDstOptimal);
	}
	else
	{
		record_image_layout_transition(command_buffer, image.handle, vk::ImageAspectFlagBits::eColor, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
	}
	// qvkCmdCopyBufferToImage(command_buffer, vk_world.staging_buffer, image.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, num_regions, regions);
	command_buffer.copyBufferToImage(
		vk_world.staging_buffer,
		image.handle,
		vk::ImageLayout::eTransferDstOptimal,
		std::vector<vk::BufferImageCopy>(regions.begin(), regions.begin() + num_regions));

	record_image_layout_transition(command_buffer, image.handle, vk::ImageAspectFlagBits::eColor, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
	end_command_buffer(command_buffer);

	if (buf != pixels)
	{
		ri.Hunk_FreeTempMemory(buf);
	}
}

void vk_update_descriptor_set(image_t &image, bool mipmap)
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

	vk::DescriptorImageInfo image_info{vk_find_sampler(&sampler_def),
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
	// qvkUpdateDescriptorSets(vk_inst.device, 1, &descriptor_write, 0, nullptr);
	vk_inst.device.updateDescriptorSets(descriptor_write, nullptr);
}

void vk_destroy_image_resources(VkImage &image, VkImageView &imageView)
{

	if (image != VK_NULL_HANDLE)
	{
		qvkDestroyImage(vk_inst.device, image, nullptr);
		image = VK_NULL_HANDLE;
	}

	if (imageView != VK_NULL_HANDLE)
	{
		qvkDestroyImageView(vk_inst.device, imageView, nullptr);
		imageView = VK_NULL_HANDLE;
	}
}

static void set_shader_stage_desc(vk::PipelineShaderStageCreateInfo &desc, vk::ShaderStageFlagBits stage, vk::ShaderModule shader_module, const char *entry)
{
	desc.pNext = nullptr;
	desc.flags = {};
	desc.stage = stage;
	desc.module = shader_module;
	desc.pName = entry;
	desc.pSpecializationInfo = nullptr;
}

#define FORMAT_DEPTH(format, r_bits, g_bits, b_bits) \
	case (VK_FORMAT_##format):                       \
		*r = r_bits;                                 \
		*b = b_bits;                                 \
		*g = g_bits;                                 \
		return true;
static bool vk_surface_format_color_depth(VkFormat format, int *r, int *g, int *b)
{
	switch (format)
	{
		// Common formats from https://vulkan.gpuinfo.org/listsurfaceformats.php
		FORMAT_DEPTH(B8G8R8A8_UNORM, 255, 255, 255)
		FORMAT_DEPTH(B8G8R8A8_SRGB, 255, 255, 255)
		FORMAT_DEPTH(A2B10G10R10_UNORM_PACK32, 1023, 1023, 1023)
		FORMAT_DEPTH(R8G8B8A8_UNORM, 255, 255, 255)
		FORMAT_DEPTH(R8G8B8A8_SRGB, 255, 255, 255)
		FORMAT_DEPTH(A2R10G10B10_UNORM_PACK32, 1023, 1023, 1023)
		FORMAT_DEPTH(R5G6B5_UNORM_PACK16, 31, 63, 31)
		FORMAT_DEPTH(R8G8B8A8_SNORM, 255, 255, 255)
		FORMAT_DEPTH(A8B8G8R8_UNORM_PACK32, 255, 255, 255)
		FORMAT_DEPTH(A8B8G8R8_SNORM_PACK32, 255, 255, 255)
		FORMAT_DEPTH(A8B8G8R8_SRGB_PACK32, 255, 255, 255)
		FORMAT_DEPTH(R16G16B16A16_UNORM, 65535, 65535, 65535)
		FORMAT_DEPTH(R16G16B16A16_SNORM, 65535, 65535, 65535)
		FORMAT_DEPTH(B5G6R5_UNORM_PACK16, 31, 63, 31)
		FORMAT_DEPTH(B8G8R8A8_SNORM, 255, 255, 255)
		FORMAT_DEPTH(R4G4B4A4_UNORM_PACK16, 15, 15, 15)
		FORMAT_DEPTH(B4G4R4A4_UNORM_PACK16, 15, 15, 15)
		FORMAT_DEPTH(A1R5G5B5_UNORM_PACK16, 31, 31, 31)
		FORMAT_DEPTH(R5G5B5A1_UNORM_PACK16, 31, 31, 31)
		FORMAT_DEPTH(B5G5R5A1_UNORM_PACK16, 31, 31, 31)
	default:
		*r = 255;
		*g = 255;
		*b = 255;
		return false;
	}
}

void vk_create_post_process_pipeline(int program_index, uint32_t width, uint32_t height)
{
	vk::Pipeline *pipeline;
	vk::ShaderModule fsmodule;
	vk::RenderPass renderpass;
	vk::PipelineLayout layout;
	vk::SampleCountFlagBits samples;
	const char *pipeline_name;
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
	} frag_spec_data;

	switch (program_index)
	{
	case 1: // bloom extraction
		pipeline = &vk_inst.bloom_extract_pipeline;
		fsmodule = vk_inst.modules.bloom_fs;
		renderpass = vk_inst.render_pass.bloom_extract;
		layout = vk_inst.pipeline_layout_post_process;
		samples = vk::SampleCountFlagBits::e1;
		pipeline_name = "bloom extraction pipeline";
		blend = false;
		break;
	case 2: // final bloom blend
		pipeline = &vk_inst.bloom_blend_pipeline;
		fsmodule = vk_inst.modules.blend_fs;
		renderpass = vk_inst.render_pass.post_bloom;
		layout = vk_inst.pipeline_layout_blend;
		samples = vk::SampleCountFlagBits(vkSamples);
		pipeline_name = "bloom blend pipeline";
		blend = true;
		break;
	case 3: // capture buffer extraction
		pipeline = &vk_inst.capture_pipeline;
		fsmodule = vk_inst.modules.gamma_fs;
		renderpass = vk_inst.render_pass.capture;
		layout = vk_inst.pipeline_layout_post_process;
		samples = vk::SampleCountFlagBits::e1;
		pipeline_name = "capture buffer pipeline";
		blend = false;
		break;
	default: // gamma correction
		pipeline = &vk_inst.gamma_pipeline;
		fsmodule = vk_inst.modules.gamma_fs;
		renderpass = vk_inst.render_pass.gamma;
		layout = vk_inst.pipeline_layout_post_process;
		samples = vk::SampleCountFlagBits::e1;
		pipeline_name = "gamma-correction pipeline";
		blend = false;
		break;
	}

	if (*pipeline != VK_NULL_HANDLE)
	{

		vk_inst.device.waitIdle();
		vk_inst.device.destroyPipeline(*pipeline);
		*pipeline = VK_NULL_HANDLE;
	}

	vk::PipelineVertexInputStateCreateInfo vertex_input_state{};

	// shaders
	std::array<vk::PipelineShaderStageCreateInfo, 2> shader_stages;
	set_shader_stage_desc(shader_stages[0], vk::ShaderStageFlagBits::eVertex, vk_inst.modules.gamma_vs, "main");
	set_shader_stage_desc(shader_stages[1], vk::ShaderStageFlagBits::eFragment, fsmodule, "main");

	frag_spec_data.gamma = 1.0 / (r_gamma->value);
	frag_spec_data.overbright = (float)(1 << tr.overbrightBits);
	frag_spec_data.greyscale = r_greyscale->value;
	frag_spec_data.bloom_threshold = r_bloom_threshold->value;
	frag_spec_data.bloom_intensity = r_bloom_intensity->value;
	frag_spec_data.bloom_threshold_mode = r_bloom_threshold_mode->integer;
	frag_spec_data.bloom_modulate = r_bloom_modulate->integer;
	frag_spec_data.dither = r_dither->integer;

	if (!vk_surface_format_color_depth(vk_inst.present_format.format, &frag_spec_data.depth_r, &frag_spec_data.depth_g, &frag_spec_data.depth_b))
		ri.Printf(PRINT_ALL, "Format %s not recognized, dither to assume 8bpc\n", vk_format_string(vk_inst.base_format.format).data());

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
	vk::PipelineInputAssemblyStateCreateInfo input_assembly_state{{}, vk::PrimitiveTopology::eTriangleStrip, VK_FALSE};

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
																 VK_FALSE,
																 VK_FALSE,
																 vk::PolygonMode::eFill,
																 vk::CullModeFlagBits::eNone,
																 vk::FrontFace::eClockwise,
																 VK_FALSE,
																 0.0f,
																 0.0f,
																 0.0f,
																 1.0f,
																 nullptr};

	vk::PipelineMultisampleStateCreateInfo multisample_state{{},
															 samples,
															 VK_FALSE,
															 1.0f,
															 nullptr,
															 VK_FALSE,
															 VK_FALSE,
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

	std::array<float, 4> blendConstants = {{0.0f, 0.0f, 0.0f, 0.0f}};

	vk::PipelineColorBlendStateCreateInfo blend_state{{},
													  VK_FALSE,
													  vk::LogicOp::eCopy,
													  1,
													  &attachment_blend_state,
													  blendConstants};

	vk::PipelineDepthStencilStateCreateInfo depth_stencil_state{{},
																VK_FALSE,
																VK_FALSE,
																vk::CompareOp::eNever,
																VK_FALSE,
																VK_FALSE,
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
		VK_NULL_HANDLE,
		-1};

	*pipeline = vk_inst.device.createGraphicsPipeline(nullptr, pipeline_create_info).value;

	// VK_CHECK(qvkCreateGraphicsPipelines(vk_inst.device, VK_NULL_HANDLE, 1, &create_info, nullptr, pipeline));

	// SET_OBJECT_NAME(*pipeline, pipeline_name, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT);
}

void vk_create_blur_pipeline(uint32_t index, uint32_t width, uint32_t height, bool horizontal_pass)
{
	vk::Pipeline *pipeline = &vk_inst.blur_pipeline[index];

	if (*pipeline != VK_NULL_HANDLE)
	{
		vk_inst.device.waitIdle();
		vk_inst.device.destroyPipeline(*pipeline);
		*pipeline = VK_NULL_HANDLE;
	}

	vk::PipelineVertexInputStateCreateInfo vertex_input_state{};

	// shaders
	std::array<vk::PipelineShaderStageCreateInfo, 2> shader_stages;
	set_shader_stage_desc(shader_stages[0], vk::ShaderStageFlagBits::eVertex, vk_inst.modules.gamma_vs, "main");
	set_shader_stage_desc(shader_stages[1], vk::ShaderStageFlagBits::eFragment, vk_inst.modules.blur_fs, "main");

	struct frag_spec_data_t
	{
		float is_vertical;
		float width;
		float height;
	} frag_spec_data{horizontal_pass ? 0.0f : 1.0f, static_cast<float>(width), static_cast<float>(height)};

	std::array<vk::SpecializationMapEntry, 3> spec_entries = {{
		{0, offsetof(frag_spec_data_t, is_vertical), sizeof(frag_spec_data.is_vertical)},
		{1, offsetof(frag_spec_data_t, width), sizeof(frag_spec_data.width)},
		{2, offsetof(frag_spec_data_t, height), sizeof(frag_spec_data.height)},
	}};

	vk::SpecializationInfo frag_spec_info{
		static_cast<uint32_t>(spec_entries.size()), spec_entries.data(), sizeof(frag_spec_data),
		&frag_spec_data.is_vertical};

	shader_stages[1].pSpecializationInfo = &frag_spec_info;

	//
	// Primitive assembly.
	//
	vk::PipelineInputAssemblyStateCreateInfo input_assembly_state{{},
																  vk::PrimitiveTopology::eTriangleStrip,
																  VK_FALSE,
																  nullptr};

	//
	// Viewport.
	//
	vk::Viewport viewport{0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f};
	vk::Rect2D scissor{{viewport.x, viewport.y}, {viewport.width, viewport.height}};

	vk::PipelineViewportStateCreateInfo viewport_state{{}, 1, &viewport, 1, &scissor};

	//
	// Rasterization.
	//
	vk::PipelineRasterizationStateCreateInfo rasterization_state{
		{},
		VK_FALSE,
		VK_FALSE,
		vk::PolygonMode::eFill,
		vk::CullModeFlagBits::eNone,
		vk::FrontFace::eClockwise,
		VK_FALSE,
		0.0f,
		0.0f,
		0.0f,
		1.0f,
		nullptr};

	vk::PipelineMultisampleStateCreateInfo multisample_state{{},
															 vk::SampleCountFlagBits::e1,
															 VK_FALSE,
															 1.0f,
															 nullptr,
															 VK_FALSE,
															 VK_FALSE,
															 nullptr};

	vk::PipelineColorBlendAttachmentState attachment_blend_state{
		VK_FALSE,
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
		VK_FALSE,
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
		VK_NULL_HANDLE,
		-1,
		nullptr};

	*pipeline = vk_inst.device.createGraphicsPipeline(nullptr, create_info).value;

	// SET_OBJECT_NAME(*pipeline, va("%s blur pipeline %i", horizontal_pass ? "horizontal" : "vertical", index / 2 + 1), VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT);
}

static VkVertexInputBindingDescription bindings[8];
static VkVertexInputAttributeDescription attribs[8];
static uint32_t num_binds;
static uint32_t num_attrs;

static void push_bind(uint32_t binding, uint32_t stride)
{
	bindings[num_binds].binding = binding;
	bindings[num_binds].stride = stride;
	bindings[num_binds].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	num_binds++;
}

static void push_attr(uint32_t location, uint32_t binding, VkFormat format)
{
	attribs[num_attrs].location = location;
	attribs[num_attrs].binding = binding;
	attribs[num_attrs].format = format;
	attribs[num_attrs].offset = 0;
	num_attrs++;
}

VkPipeline create_pipeline(const Vk_Pipeline_Def &def, renderPass_t renderPassIndex)
{
	VkShaderModule *vs_module = nullptr;
	VkShaderModule *fs_module = nullptr;
	// int32_t vert_spec_data[1]; // clippping
	floatint_t frag_spec_data[11]; // 0:alpha-test-func, 1:alpha-test-value, 2:depth-fragment, 3:alpha-to-coverage, 4:color_mode, 5:abs_light, 6:multitexture mode, 7:discard mode, 8: ident.color, 9 - ident.alpha, 10 - acff
	VkSpecializationMapEntry spec_entries[12];
	// VkSpecializationInfo vert_spec_info;
	VkSpecializationInfo frag_spec_info;
	VkPipelineVertexInputStateCreateInfo vertex_input_state;
	VkPipelineInputAssemblyStateCreateInfo input_assembly_state;
	VkPipelineRasterizationStateCreateInfo rasterization_state;
	VkPipelineViewportStateCreateInfo viewport_state;
	VkPipelineMultisampleStateCreateInfo multisample_state;
	VkPipelineDepthStencilStateCreateInfo depth_stencil_state;
	VkPipelineColorBlendStateCreateInfo blend_state;
	VkPipelineColorBlendAttachmentState attachment_blend_state;
	VkPipelineDynamicStateCreateInfo dynamic_state;
	VkDynamicState dynamic_state_array[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
	VkGraphicsPipelineCreateInfo create_info;
	VkPipeline pipeline;
	VkBool32 alphaToCoverage = VK_FALSE;
	unsigned int atest_bits;
	unsigned int state_bits = def.state_bits;

	switch (def.shader_type)
	{

	case TYPE_SIGNLE_TEXTURE_LIGHTING:
		vs_module = &vk_inst.modules.vert.light[0];
		fs_module = &vk_inst.modules.frag.light[0][0];
		break;

	case TYPE_SIGNLE_TEXTURE_LIGHTING_LINEAR:
		vs_module = &vk_inst.modules.vert.light[0];
		fs_module = &vk_inst.modules.frag.light[1][0];
		break;

	case TYPE_SIGNLE_TEXTURE_DF:
		state_bits |= GLS_DEPTHMASK_TRUE;
		vs_module = &vk_inst.modules.vert.ident1[0][0][0];
		fs_module = &vk_inst.modules.frag.gen0_df;
		break;

	case TYPE_SIGNLE_TEXTURE_FIXED_COLOR:
		vs_module = &vk_inst.modules.vert.fixed[0][0][0];
		fs_module = &vk_inst.modules.frag.fixed[0][0];
		break;

	case TYPE_SIGNLE_TEXTURE_FIXED_COLOR_ENV:
		vs_module = &vk_inst.modules.vert.fixed[0][1][0];
		fs_module = &vk_inst.modules.frag.fixed[0][0];
		break;

	case TYPE_SIGNLE_TEXTURE_ENT_COLOR:
		vs_module = &vk_inst.modules.vert.fixed[0][0][0];
		fs_module = &vk_inst.modules.frag.ent[0][0];
		break;

	case TYPE_SIGNLE_TEXTURE_ENT_COLOR_ENV:
		vs_module = &vk_inst.modules.vert.fixed[0][1][0];
		fs_module = &vk_inst.modules.frag.ent[0][0];
		break;

	case TYPE_SIGNLE_TEXTURE:
		vs_module = &vk_inst.modules.vert.gen[0][0][0][0];
		fs_module = &vk_inst.modules.frag.gen[0][0][0];
		break;

	case TYPE_SIGNLE_TEXTURE_ENV:
		vs_module = &vk_inst.modules.vert.gen[0][0][1][0];
		fs_module = &vk_inst.modules.frag.gen[0][0][0];
		break;

	case TYPE_SIGNLE_TEXTURE_IDENTITY:
		vs_module = &vk_inst.modules.vert.ident1[0][0][0];
		fs_module = &vk_inst.modules.frag.ident1[0][0];
		break;

	case TYPE_SIGNLE_TEXTURE_IDENTITY_ENV:
		vs_module = &vk_inst.modules.vert.ident1[0][1][0];
		fs_module = &vk_inst.modules.frag.ident1[0][0];
		break;

	case TYPE_MULTI_TEXTURE_ADD2_IDENTITY:
	case TYPE_MULTI_TEXTURE_MUL2_IDENTITY:
		vs_module = &vk_inst.modules.vert.ident1[1][0][0];
		fs_module = &vk_inst.modules.frag.ident1[1][0];
		break;

	case TYPE_MULTI_TEXTURE_ADD2_IDENTITY_ENV:
	case TYPE_MULTI_TEXTURE_MUL2_IDENTITY_ENV:
		vs_module = &vk_inst.modules.vert.ident1[1][1][0];
		fs_module = &vk_inst.modules.frag.ident1[1][0];
		break;

	case TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR:
	case TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR:
		vs_module = &vk_inst.modules.vert.fixed[1][0][0];
		fs_module = &vk_inst.modules.frag.fixed[1][0];
		break;

	case TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR_ENV:
	case TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR_ENV:
		vs_module = &vk_inst.modules.vert.fixed[1][1][0];
		fs_module = &vk_inst.modules.frag.fixed[1][0];
		break;

	case TYPE_MULTI_TEXTURE_MUL2:
	case TYPE_MULTI_TEXTURE_ADD2_1_1:
	case TYPE_MULTI_TEXTURE_ADD2:
		vs_module = &vk_inst.modules.vert.gen[1][0][0][0];
		fs_module = &vk_inst.modules.frag.gen[1][0][0];
		break;

	case TYPE_MULTI_TEXTURE_MUL2_ENV:
	case TYPE_MULTI_TEXTURE_ADD2_1_1_ENV:
	case TYPE_MULTI_TEXTURE_ADD2_ENV:
		vs_module = &vk_inst.modules.vert.gen[1][0][1][0];
		fs_module = &vk_inst.modules.frag.gen[1][0][0];
		break;

	case TYPE_MULTI_TEXTURE_MUL3:
	case TYPE_MULTI_TEXTURE_ADD3_1_1:
	case TYPE_MULTI_TEXTURE_ADD3:
		vs_module = &vk_inst.modules.vert.gen[2][0][0][0];
		fs_module = &vk_inst.modules.frag.gen[2][0][0];
		break;

	case TYPE_MULTI_TEXTURE_MUL3_ENV:
	case TYPE_MULTI_TEXTURE_ADD3_1_1_ENV:
	case TYPE_MULTI_TEXTURE_ADD3_ENV:
		vs_module = &vk_inst.modules.vert.gen[2][0][1][0];
		fs_module = &vk_inst.modules.frag.gen[2][0][0];
		break;

	case TYPE_BLEND2_ADD:
	case TYPE_BLEND2_MUL:
	case TYPE_BLEND2_ALPHA:
	case TYPE_BLEND2_ONE_MINUS_ALPHA:
	case TYPE_BLEND2_MIX_ALPHA:
	case TYPE_BLEND2_MIX_ONE_MINUS_ALPHA:
	case TYPE_BLEND2_DST_COLOR_SRC_ALPHA:
		vs_module = &vk_inst.modules.vert.gen[1][1][0][0];
		fs_module = &vk_inst.modules.frag.gen[1][1][0];
		break;

	case TYPE_BLEND2_ADD_ENV:
	case TYPE_BLEND2_MUL_ENV:
	case TYPE_BLEND2_ALPHA_ENV:
	case TYPE_BLEND2_ONE_MINUS_ALPHA_ENV:
	case TYPE_BLEND2_MIX_ALPHA_ENV:
	case TYPE_BLEND2_MIX_ONE_MINUS_ALPHA_ENV:
	case TYPE_BLEND2_DST_COLOR_SRC_ALPHA_ENV:
		vs_module = &vk_inst.modules.vert.gen[1][1][1][0];
		fs_module = &vk_inst.modules.frag.gen[1][1][0];
		break;

	case TYPE_BLEND3_ADD:
	case TYPE_BLEND3_MUL:
	case TYPE_BLEND3_ALPHA:
	case TYPE_BLEND3_ONE_MINUS_ALPHA:
	case TYPE_BLEND3_MIX_ALPHA:
	case TYPE_BLEND3_MIX_ONE_MINUS_ALPHA:
	case TYPE_BLEND3_DST_COLOR_SRC_ALPHA:
		vs_module = &vk_inst.modules.vert.gen[2][1][0][0];
		fs_module = &vk_inst.modules.frag.gen[2][1][0];
		break;

	case TYPE_BLEND3_ADD_ENV:
	case TYPE_BLEND3_MUL_ENV:
	case TYPE_BLEND3_ALPHA_ENV:
	case TYPE_BLEND3_ONE_MINUS_ALPHA_ENV:
	case TYPE_BLEND3_MIX_ALPHA_ENV:
	case TYPE_BLEND3_MIX_ONE_MINUS_ALPHA_ENV:
	case TYPE_BLEND3_DST_COLOR_SRC_ALPHA_ENV:
		vs_module = &vk_inst.modules.vert.gen[2][1][1][0];
		fs_module = &vk_inst.modules.frag.gen[2][1][0];
		break;

	case TYPE_COLOR_BLACK:
	case TYPE_COLOR_WHITE:
	case TYPE_COLOR_GREEN:
	case TYPE_COLOR_RED:
		vs_module = &vk_inst.modules.color_vs;
		fs_module = &vk_inst.modules.color_fs;
		break;

	case TYPE_FOG_ONLY:
		vs_module = &vk_inst.modules.fog_vs;
		fs_module = &vk_inst.modules.fog_fs;
		break;

	case TYPE_DOT:
		vs_module = &vk_inst.modules.dot_vs;
		fs_module = &vk_inst.modules.dot_fs;
		break;

	default:
		ri.Error(ERR_DROP, "create_pipeline_plus: unknown shader type %i\n", def.shader_type);
		return 0;
	}

	if (def.fog_stage)
	{
		switch (def.shader_type)
		{
		case TYPE_FOG_ONLY:
		case TYPE_DOT:
		case TYPE_SIGNLE_TEXTURE_DF:
		case TYPE_COLOR_BLACK:
		case TYPE_COLOR_WHITE:
		case TYPE_COLOR_GREEN:
		case TYPE_COLOR_RED:
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
	Com_Memset(frag_spec_data, 0, sizeof(frag_spec_data));

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
	if ( r_ext_alpha_to_coverage->integer && vkSamples != VK_SAMPLE_COUNT_1_BIT && frag_spec_data[0].i ) {
		frag_spec_data[3].i = 1;
		alphaToCoverage = VK_TRUE;
	}
#endif

	// constant color
	switch (def.shader_type)
	{
	default:
		frag_spec_data[4].i = 0;
		break;
	case TYPE_COLOR_WHITE:
		frag_spec_data[4].i = 1;
		break;
	case TYPE_COLOR_GREEN:
		frag_spec_data[4].i = 2;
		break;
	case TYPE_COLOR_RED:
		frag_spec_data[4].i = 3;
		break;
	}

	// abs lighting
	switch (def.shader_type)
	{
	case TYPE_SIGNLE_TEXTURE_LIGHTING:
	case TYPE_SIGNLE_TEXTURE_LIGHTING_LINEAR:
		frag_spec_data[5].i = def.abs_light ? 1 : 0;
	default:
		break;
	}

	// multutexture mode
	switch (def.shader_type)
	{
	case TYPE_MULTI_TEXTURE_MUL2_IDENTITY:
	case TYPE_MULTI_TEXTURE_MUL2_IDENTITY_ENV:
	case TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR:
	case TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR_ENV:
	case TYPE_MULTI_TEXTURE_MUL2:
	case TYPE_MULTI_TEXTURE_MUL2_ENV:
	case TYPE_MULTI_TEXTURE_MUL3:
	case TYPE_MULTI_TEXTURE_MUL3_ENV:
	case TYPE_BLEND2_MUL:
	case TYPE_BLEND2_MUL_ENV:
	case TYPE_BLEND3_MUL:
	case TYPE_BLEND3_MUL_ENV:
		frag_spec_data[6].i = 0;
		break;

	case TYPE_MULTI_TEXTURE_ADD2_IDENTITY:
	case TYPE_MULTI_TEXTURE_ADD2_IDENTITY_ENV:
	case TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR:
	case TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR_ENV:
	case TYPE_MULTI_TEXTURE_ADD2_1_1:
	case TYPE_MULTI_TEXTURE_ADD2_1_1_ENV:
	case TYPE_MULTI_TEXTURE_ADD3_1_1:
	case TYPE_MULTI_TEXTURE_ADD3_1_1_ENV:
		frag_spec_data[6].i = 1;
		break;

	case TYPE_MULTI_TEXTURE_ADD2:
	case TYPE_MULTI_TEXTURE_ADD2_ENV:
	case TYPE_MULTI_TEXTURE_ADD3:
	case TYPE_MULTI_TEXTURE_ADD3_ENV:
	case TYPE_BLEND2_ADD:
	case TYPE_BLEND2_ADD_ENV:
	case TYPE_BLEND3_ADD:
	case TYPE_BLEND3_ADD_ENV:
		frag_spec_data[6].i = 2;
		break;

	case TYPE_BLEND2_ALPHA:
	case TYPE_BLEND2_ALPHA_ENV:
	case TYPE_BLEND3_ALPHA:
	case TYPE_BLEND3_ALPHA_ENV:
		frag_spec_data[6].i = 3;
		break;

	case TYPE_BLEND2_ONE_MINUS_ALPHA:
	case TYPE_BLEND2_ONE_MINUS_ALPHA_ENV:
	case TYPE_BLEND3_ONE_MINUS_ALPHA:
	case TYPE_BLEND3_ONE_MINUS_ALPHA_ENV:
		frag_spec_data[6].i = 4;
		break;

	case TYPE_BLEND2_MIX_ALPHA:
	case TYPE_BLEND2_MIX_ALPHA_ENV:
	case TYPE_BLEND3_MIX_ALPHA:
	case TYPE_BLEND3_MIX_ALPHA_ENV:
		frag_spec_data[6].i = 5;
		break;

	case TYPE_BLEND2_MIX_ONE_MINUS_ALPHA:
	case TYPE_BLEND2_MIX_ONE_MINUS_ALPHA_ENV:
	case TYPE_BLEND3_MIX_ONE_MINUS_ALPHA:
	case TYPE_BLEND3_MIX_ONE_MINUS_ALPHA_ENV:
		frag_spec_data[6].i = 6;
		break;

	case TYPE_BLEND2_DST_COLOR_SRC_ALPHA:
	case TYPE_BLEND2_DST_COLOR_SRC_ALPHA_ENV:
	case TYPE_BLEND3_DST_COLOR_SRC_ALPHA:
	case TYPE_BLEND3_DST_COLOR_SRC_ALPHA_ENV:
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
	spec_entries[0].offset = 0 * sizeof( int32_t );
	spec_entries[0].size = sizeof( int32_t );

	vert_spec_info.mapEntryCount = 1;
	vert_spec_info.pMapEntries = spec_entries + 0;
	vert_spec_info.dataSize = 1 * sizeof( int32_t );
	vert_spec_info.pData = &vert_spec_data[0];
	shader_stages[0].pSpecializationInfo = &vert_spec_info;
#endif
	shader_stages[0].pSpecializationInfo = nullptr;

	//
	// fragment module specialization data
	//

	spec_entries[1].constantID = 0; // alpha-test-function
	spec_entries[1].offset = 0 * sizeof(int32_t);
	spec_entries[1].size = sizeof(int32_t);

	spec_entries[2].constantID = 1; // alpha-test-value
	spec_entries[2].offset = 1 * sizeof(int32_t);
	spec_entries[2].size = sizeof(float);

	spec_entries[3].constantID = 2; // depth-fragment
	spec_entries[3].offset = 2 * sizeof(int32_t);
	spec_entries[3].size = sizeof(float);

	spec_entries[4].constantID = 3; // alpha-to-coverage
	spec_entries[4].offset = 3 * sizeof(int32_t);
	spec_entries[4].size = sizeof(int32_t);

	spec_entries[5].constantID = 4; // color_mode
	spec_entries[5].offset = 4 * sizeof(int32_t);
	spec_entries[5].size = sizeof(int32_t);

	spec_entries[6].constantID = 5; // abs_light
	spec_entries[6].offset = 5 * sizeof(int32_t);
	spec_entries[6].size = sizeof(int32_t);

	spec_entries[7].constantID = 6; // multitexture mode
	spec_entries[7].offset = 6 * sizeof(int32_t);
	spec_entries[7].size = sizeof(int32_t);

	spec_entries[8].constantID = 7; // discard mode
	spec_entries[8].offset = 7 * sizeof(int32_t);
	spec_entries[8].size = sizeof(int32_t);

	spec_entries[9].constantID = 8; // fixed color
	spec_entries[9].offset = 8 * sizeof(int32_t);
	spec_entries[9].size = sizeof(float);

	spec_entries[10].constantID = 9; // fixed alpha
	spec_entries[10].offset = 9 * sizeof(int32_t);
	spec_entries[10].size = sizeof(float);

	spec_entries[11].constantID = 10; // acff
	spec_entries[11].offset = 10 * sizeof(int32_t);
	spec_entries[11].size = sizeof(int32_t);

	frag_spec_info.mapEntryCount = 11;
	frag_spec_info.pMapEntries = spec_entries + 1;
	frag_spec_info.dataSize = sizeof(int32_t) * 11;
	frag_spec_info.pData = &frag_spec_data[0];

	auto frag_spec_infoCpp = vk::SpecializationInfo(frag_spec_info);
	shader_stages[1].pSpecializationInfo = &frag_spec_infoCpp;

	//
	// Vertex input
	//
	num_binds = num_attrs = 0;
	switch (def.shader_type)
	{

	case TYPE_FOG_ONLY:
	case TYPE_DOT:
		push_bind(0, sizeof(vec4_t)); // xyz array
		push_attr(0, 0, VK_FORMAT_R32G32B32A32_SFLOAT);
		break;

	case TYPE_COLOR_BLACK:
	case TYPE_COLOR_WHITE:
	case TYPE_COLOR_GREEN:
	case TYPE_COLOR_RED:
		push_bind(0, sizeof(vec4_t)); // xyz array
		push_attr(0, 0, VK_FORMAT_R32G32B32A32_SFLOAT);
		break;

	case TYPE_SIGNLE_TEXTURE_DF:
	case TYPE_SIGNLE_TEXTURE_IDENTITY:
	case TYPE_SIGNLE_TEXTURE_FIXED_COLOR:
	case TYPE_SIGNLE_TEXTURE_ENT_COLOR:
		push_bind(0, sizeof(vec4_t)); // xyz array
		push_bind(2, sizeof(vec2_t)); // st0 array
		push_attr(0, 0, VK_FORMAT_R32G32B32A32_SFLOAT);
		push_attr(2, 2, VK_FORMAT_R32G32_SFLOAT);
		break;

	case TYPE_SIGNLE_TEXTURE:
		push_bind(0, sizeof(vec4_t));	  // xyz array
		push_bind(1, sizeof(color4ub_t)); // color array
		push_bind(2, sizeof(vec2_t));	  // st0 array
		push_attr(0, 0, VK_FORMAT_R32G32B32A32_SFLOAT);
		push_attr(1, 1, VK_FORMAT_R8G8B8A8_UNORM);
		push_attr(2, 2, VK_FORMAT_R32G32_SFLOAT);
		break;

	case TYPE_SIGNLE_TEXTURE_ENV:
		push_bind(0, sizeof(vec4_t));	  // xyz array
		push_bind(1, sizeof(color4ub_t)); // color array
		// push_bind( 2, sizeof( vec2_t ) );					// st0 array
		push_bind(5, sizeof(vec4_t)); // normals
		push_attr(0, 0, VK_FORMAT_R32G32B32A32_SFLOAT);
		push_attr(1, 1, VK_FORMAT_R8G8B8A8_UNORM);
		// push_attr( 2, 2, VK_FORMAT_R8G8B8A8_UNORM );
		push_attr(5, 5, VK_FORMAT_R32G32B32A32_SFLOAT);
		break;

	case TYPE_SIGNLE_TEXTURE_IDENTITY_ENV:
	case TYPE_SIGNLE_TEXTURE_FIXED_COLOR_ENV:
	case TYPE_SIGNLE_TEXTURE_ENT_COLOR_ENV:
		push_bind(0, sizeof(vec4_t)); // xyz array
		push_bind(5, sizeof(vec4_t)); // normals
		push_attr(0, 0, VK_FORMAT_R32G32B32A32_SFLOAT);
		push_attr(5, 5, VK_FORMAT_R32G32B32A32_SFLOAT);
		break;

	case TYPE_SIGNLE_TEXTURE_LIGHTING:
	case TYPE_SIGNLE_TEXTURE_LIGHTING_LINEAR:
		push_bind(0, sizeof(vec4_t)); // xyz array
		push_bind(1, sizeof(vec2_t)); // st0 array
		push_bind(2, sizeof(vec4_t)); // normals array
		push_attr(0, 0, VK_FORMAT_R32G32B32A32_SFLOAT);
		push_attr(1, 1, VK_FORMAT_R32G32_SFLOAT);
		push_attr(2, 2, VK_FORMAT_R32G32B32A32_SFLOAT);
		break;

	case TYPE_MULTI_TEXTURE_MUL2_IDENTITY:
	case TYPE_MULTI_TEXTURE_ADD2_IDENTITY:
	case TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR:
	case TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR:
		push_bind(0, sizeof(vec4_t)); // xyz array
		push_bind(2, sizeof(vec2_t)); // st0 array
		push_bind(3, sizeof(vec2_t)); // st1 array
		push_attr(0, 0, VK_FORMAT_R32G32B32A32_SFLOAT);
		push_attr(2, 2, VK_FORMAT_R32G32_SFLOAT);
		push_attr(3, 3, VK_FORMAT_R32G32_SFLOAT);
		break;

	case TYPE_MULTI_TEXTURE_MUL2_IDENTITY_ENV:
	case TYPE_MULTI_TEXTURE_ADD2_IDENTITY_ENV:
	case TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR_ENV:
	case TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR_ENV:
		push_bind(0, sizeof(vec4_t)); // xyz array
		push_bind(3, sizeof(vec2_t)); // st1 array
		push_bind(5, sizeof(vec4_t)); // normals
		push_attr(0, 0, VK_FORMAT_R32G32B32A32_SFLOAT);
		push_attr(3, 3, VK_FORMAT_R32G32_SFLOAT);
		push_attr(5, 5, VK_FORMAT_R32G32B32A32_SFLOAT);
		break;

	case TYPE_MULTI_TEXTURE_MUL2:
	case TYPE_MULTI_TEXTURE_ADD2_1_1:
	case TYPE_MULTI_TEXTURE_ADD2:
		push_bind(0, sizeof(vec4_t));	  // xyz array
		push_bind(1, sizeof(color4ub_t)); // color array
		push_bind(2, sizeof(vec2_t));	  // st0 array
		push_bind(3, sizeof(vec2_t));	  // st1 array
		push_attr(0, 0, VK_FORMAT_R32G32B32A32_SFLOAT);
		push_attr(1, 1, VK_FORMAT_R8G8B8A8_UNORM);
		push_attr(2, 2, VK_FORMAT_R32G32_SFLOAT);
		push_attr(3, 3, VK_FORMAT_R32G32_SFLOAT);
		break;

	case TYPE_MULTI_TEXTURE_MUL2_ENV:
	case TYPE_MULTI_TEXTURE_ADD2_1_1_ENV:
	case TYPE_MULTI_TEXTURE_ADD2_ENV:
		push_bind(0, sizeof(vec4_t));	  // xyz array
		push_bind(1, sizeof(color4ub_t)); // color array
		// push_bind( 2, sizeof( vec2_t ) );					// st0 array
		push_bind(3, sizeof(vec2_t)); // st1 array
		push_bind(5, sizeof(vec4_t)); // normals
		push_attr(0, 0, VK_FORMAT_R32G32B32A32_SFLOAT);
		push_attr(1, 1, VK_FORMAT_R8G8B8A8_UNORM);
		// push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
		push_attr(3, 3, VK_FORMAT_R32G32_SFLOAT);
		push_attr(5, 5, VK_FORMAT_R32G32B32A32_SFLOAT);
		break;

	case TYPE_MULTI_TEXTURE_MUL3:
	case TYPE_MULTI_TEXTURE_ADD3_1_1:
	case TYPE_MULTI_TEXTURE_ADD3:
		push_bind(0, sizeof(vec4_t));	  // xyz array
		push_bind(1, sizeof(color4ub_t)); // color array
		push_bind(2, sizeof(vec2_t));	  // st0 array
		push_bind(3, sizeof(vec2_t));	  // st1 array
		push_bind(4, sizeof(vec2_t));	  // st2 array
		push_attr(0, 0, VK_FORMAT_R32G32B32A32_SFLOAT);
		push_attr(1, 1, VK_FORMAT_R8G8B8A8_UNORM);
		push_attr(2, 2, VK_FORMAT_R32G32_SFLOAT);
		push_attr(3, 3, VK_FORMAT_R32G32_SFLOAT);
		push_attr(4, 4, VK_FORMAT_R32G32_SFLOAT);
		break;

	case TYPE_MULTI_TEXTURE_MUL3_ENV:
	case TYPE_MULTI_TEXTURE_ADD3_1_1_ENV:
	case TYPE_MULTI_TEXTURE_ADD3_ENV:
		push_bind(0, sizeof(vec4_t));	  // xyz array
		push_bind(1, sizeof(color4ub_t)); // color array
		// push_bind( 2, sizeof( vec2_t ) );					// st0 array
		push_bind(3, sizeof(vec2_t)); // st1 array
		push_bind(4, sizeof(vec2_t)); // st2 array
		push_bind(5, sizeof(vec4_t)); // normals
		push_attr(0, 0, VK_FORMAT_R32G32B32A32_SFLOAT);
		push_attr(1, 1, VK_FORMAT_R8G8B8A8_UNORM);
		// push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
		push_attr(3, 3, VK_FORMAT_R32G32_SFLOAT);
		push_attr(4, 4, VK_FORMAT_R32G32_SFLOAT);
		push_attr(5, 5, VK_FORMAT_R32G32B32A32_SFLOAT);
		break;

	case TYPE_BLEND2_ADD:
	case TYPE_BLEND2_MUL:
	case TYPE_BLEND2_ALPHA:
	case TYPE_BLEND2_ONE_MINUS_ALPHA:
	case TYPE_BLEND2_MIX_ALPHA:
	case TYPE_BLEND2_MIX_ONE_MINUS_ALPHA:
	case TYPE_BLEND2_DST_COLOR_SRC_ALPHA:
		push_bind(0, sizeof(vec4_t));	  // xyz array
		push_bind(1, sizeof(color4ub_t)); // color0 array
		push_bind(2, sizeof(vec2_t));	  // st0 array
		push_bind(3, sizeof(vec2_t));	  // st1 array
		push_bind(6, sizeof(color4ub_t)); // color1 array
		push_attr(0, 0, VK_FORMAT_R32G32B32A32_SFLOAT);
		push_attr(1, 1, VK_FORMAT_R8G8B8A8_UNORM);
		push_attr(2, 2, VK_FORMAT_R32G32_SFLOAT);
		push_attr(3, 3, VK_FORMAT_R32G32_SFLOAT);
		push_attr(6, 6, VK_FORMAT_R8G8B8A8_UNORM);
		break;

	case TYPE_BLEND2_ADD_ENV:
	case TYPE_BLEND2_MUL_ENV:
	case TYPE_BLEND2_ALPHA_ENV:
	case TYPE_BLEND2_ONE_MINUS_ALPHA_ENV:
	case TYPE_BLEND2_MIX_ALPHA_ENV:
	case TYPE_BLEND2_MIX_ONE_MINUS_ALPHA_ENV:
	case TYPE_BLEND2_DST_COLOR_SRC_ALPHA_ENV:
		push_bind(0, sizeof(vec4_t));	  // xyz array
		push_bind(1, sizeof(color4ub_t)); // color0 array
		// push_bind( 2, sizeof( vec2_t ) );					// st0 array
		push_bind(3, sizeof(vec2_t));	  // st1 array
		push_bind(5, sizeof(vec4_t));	  // normals
		push_bind(6, sizeof(color4ub_t)); // color1 array
		push_attr(0, 0, VK_FORMAT_R32G32B32A32_SFLOAT);
		push_attr(1, 1, VK_FORMAT_R8G8B8A8_UNORM);
		// push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
		push_attr(3, 3, VK_FORMAT_R32G32_SFLOAT);
		push_attr(5, 5, VK_FORMAT_R32G32B32A32_SFLOAT);
		push_attr(6, 6, VK_FORMAT_R8G8B8A8_UNORM);
		break;

	case TYPE_BLEND3_ADD:
	case TYPE_BLEND3_MUL:
	case TYPE_BLEND3_ALPHA:
	case TYPE_BLEND3_ONE_MINUS_ALPHA:
	case TYPE_BLEND3_MIX_ALPHA:
	case TYPE_BLEND3_MIX_ONE_MINUS_ALPHA:
	case TYPE_BLEND3_DST_COLOR_SRC_ALPHA:
		push_bind(0, sizeof(vec4_t));	  // xyz array
		push_bind(1, sizeof(color4ub_t)); // color0 array
		push_bind(2, sizeof(vec2_t));	  // st0 array
		push_bind(3, sizeof(vec2_t));	  // st1 array
		push_bind(4, sizeof(vec2_t));	  // st2 array
		push_bind(6, sizeof(color4ub_t)); // color1 array
		push_bind(7, sizeof(color4ub_t)); // color2 array
		push_attr(0, 0, VK_FORMAT_R32G32B32A32_SFLOAT);
		push_attr(1, 1, VK_FORMAT_R8G8B8A8_UNORM);
		push_attr(2, 2, VK_FORMAT_R32G32_SFLOAT);
		push_attr(3, 3, VK_FORMAT_R32G32_SFLOAT);
		push_attr(4, 4, VK_FORMAT_R32G32_SFLOAT);
		push_attr(6, 6, VK_FORMAT_R8G8B8A8_UNORM);
		push_attr(7, 7, VK_FORMAT_R8G8B8A8_UNORM);
		break;

	case TYPE_BLEND3_ADD_ENV:
	case TYPE_BLEND3_MUL_ENV:
	case TYPE_BLEND3_ALPHA_ENV:
	case TYPE_BLEND3_ONE_MINUS_ALPHA_ENV:
	case TYPE_BLEND3_MIX_ALPHA_ENV:
	case TYPE_BLEND3_MIX_ONE_MINUS_ALPHA_ENV:
	case TYPE_BLEND3_DST_COLOR_SRC_ALPHA_ENV:
		push_bind(0, sizeof(vec4_t));	  // xyz array
		push_bind(1, sizeof(color4ub_t)); // color0 array
		// push_bind( 2, sizeof( vec2_t ) );					// st0 array
		push_bind(3, sizeof(vec2_t));	  // st1 array
		push_bind(4, sizeof(vec2_t));	  // st2 array
		push_bind(5, sizeof(vec4_t));	  // normals
		push_bind(6, sizeof(color4ub_t)); // color1 array
		push_bind(7, sizeof(color4ub_t)); // color2 array
		push_attr(0, 0, VK_FORMAT_R32G32B32A32_SFLOAT);
		push_attr(1, 1, VK_FORMAT_R8G8B8A8_UNORM);
		// push_attr( 2, 2, VK_FORMAT_R32G32_SFLOAT );
		push_attr(3, 3, VK_FORMAT_R32G32_SFLOAT);
		push_attr(4, 4, VK_FORMAT_R32G32_SFLOAT);
		push_attr(5, 5, VK_FORMAT_R32G32B32A32_SFLOAT);
		push_attr(6, 6, VK_FORMAT_R8G8B8A8_UNORM);
		push_attr(7, 7, VK_FORMAT_R8G8B8A8_UNORM);
		break;

	default:
		ri.Error(ERR_DROP, "%s: invalid shader type - %i", __func__, def.shader_type);
		break;
	}

	vertex_input_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input_state.pNext = nullptr;
	vertex_input_state.flags = 0;
	vertex_input_state.pVertexBindingDescriptions = bindings;
	vertex_input_state.pVertexAttributeDescriptions = attribs;
	vertex_input_state.vertexBindingDescriptionCount = num_binds;
	vertex_input_state.vertexAttributeDescriptionCount = num_attrs;

	//
	// Primitive assembly.
	//
	input_assembly_state.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly_state.pNext = nullptr;
	input_assembly_state.flags = 0;
	input_assembly_state.primitiveRestartEnable = VK_FALSE;

	switch (def.primitives)
	{
	case LINE_LIST:
		input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
		break;
	case POINT_LIST:
		input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
		break;
	case TRIANGLE_STRIP:
		input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
		break;
	default:
		input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		break;
	}

	//
	// Viewport.
	//
	viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.pNext = nullptr;
	viewport_state.flags = 0;
	viewport_state.viewportCount = 1;
	viewport_state.pViewports = nullptr; // dynamic viewport state
	viewport_state.scissorCount = 1;
	viewport_state.pScissors = nullptr; // dynamic scissor state

	//
	// Rasterization.
	//
	rasterization_state.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterization_state.pNext = nullptr;
	rasterization_state.flags = 0;
	rasterization_state.depthClampEnable = VK_FALSE;
	rasterization_state.rasterizerDiscardEnable = VK_FALSE;
	if (def.shader_type == TYPE_DOT)
	{
		rasterization_state.polygonMode = VK_POLYGON_MODE_POINT;
	}
	else
	{
		rasterization_state.polygonMode = (state_bits & GLS_POLYMODE_LINE) ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
	}

	switch (def.face_culling)
	{
	case CT_TWO_SIDED:
		rasterization_state.cullMode = VK_CULL_MODE_NONE;
		break;
	case CT_FRONT_SIDED:
		rasterization_state.cullMode = (def.mirror ? VK_CULL_MODE_FRONT_BIT : VK_CULL_MODE_BACK_BIT);
		break;
	case CT_BACK_SIDED:
		rasterization_state.cullMode = (def.mirror ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_FRONT_BIT);
		break;
	default:
		ri.Error(ERR_DROP, "create_pipeline: invalid face culling mode %i\n", def.face_culling);
		break;
	}

	rasterization_state.frontFace = VK_FRONT_FACE_CLOCKWISE; // Q3 defaults to clockwise vertex order

	// depth bias state
	if (def.polygon_offset)
	{
		rasterization_state.depthBiasEnable = VK_TRUE;
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
		rasterization_state.depthBiasEnable = VK_FALSE;
		rasterization_state.depthBiasClamp = 0.0f;
		rasterization_state.depthBiasConstantFactor = 0.0f;
		rasterization_state.depthBiasSlopeFactor = 0.0f;
	}

	if (def.line_width)
		rasterization_state.lineWidth = (float)def.line_width;
	else
		rasterization_state.lineWidth = 1.0f;

	multisample_state.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample_state.pNext = nullptr;
	multisample_state.flags = 0;

	multisample_state.rasterizationSamples = VkSampleCountFlagBits((vk_inst.renderPassIndex == RENDER_PASS_SCREENMAP) ? vk_inst.screenMapSamples : vk::SampleCountFlagBits(vkSamples));

	multisample_state.sampleShadingEnable = VK_FALSE;
	multisample_state.minSampleShading = 1.0f;
	multisample_state.pSampleMask = nullptr;
	multisample_state.alphaToCoverageEnable = alphaToCoverage;
	multisample_state.alphaToOneEnable = VK_FALSE;

	Com_Memset(&depth_stencil_state, 0, sizeof(depth_stencil_state));

	depth_stencil_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth_stencil_state.pNext = nullptr;
	depth_stencil_state.flags = 0;
	depth_stencil_state.depthTestEnable = (state_bits & GLS_DEPTHTEST_DISABLE) ? VK_FALSE : VK_TRUE;
	depth_stencil_state.depthWriteEnable = (state_bits & GLS_DEPTHMASK_TRUE) ? VK_TRUE : VK_FALSE;
#ifdef USE_REVERSED_DEPTH
	depth_stencil_state.depthCompareOp = (state_bits & GLS_DEPTHFUNC_EQUAL) ? VK_COMPARE_OP_EQUAL : VK_COMPARE_OP_GREATER_OR_EQUAL;
#else
	depth_stencil_state.depthCompareOp = (state_bits & GLS_DEPTHFUNC_EQUAL) ? VK_COMPARE_OP_EQUAL : VK_COMPARE_OP_LESS_OR_EQUAL;
#endif
	depth_stencil_state.depthBoundsTestEnable = VK_FALSE;
	depth_stencil_state.stencilTestEnable = (def.shadow_phase != SHADOW_DISABLED) ? VK_TRUE : VK_FALSE;

	if (def.shadow_phase == SHADOW_EDGES)
	{
		depth_stencil_state.front.failOp = VK_STENCIL_OP_KEEP;
		depth_stencil_state.front.passOp = (def.face_culling == CT_FRONT_SIDED) ? VK_STENCIL_OP_INCREMENT_AND_CLAMP : VK_STENCIL_OP_DECREMENT_AND_CLAMP;
		depth_stencil_state.front.depthFailOp = VK_STENCIL_OP_KEEP;
		depth_stencil_state.front.compareOp = VK_COMPARE_OP_ALWAYS;
		depth_stencil_state.front.compareMask = 255;
		depth_stencil_state.front.writeMask = 255;
		depth_stencil_state.front.reference = 0;

		depth_stencil_state.back = depth_stencil_state.front;
	}
	else if (def.shadow_phase == SHADOW_FS_QUAD)
	{
		depth_stencil_state.front.failOp = VK_STENCIL_OP_KEEP;
		depth_stencil_state.front.passOp = VK_STENCIL_OP_KEEP;
		depth_stencil_state.front.depthFailOp = VK_STENCIL_OP_KEEP;
		depth_stencil_state.front.compareOp = VK_COMPARE_OP_NOT_EQUAL;
		depth_stencil_state.front.compareMask = 255;
		depth_stencil_state.front.writeMask = 255;
		depth_stencil_state.front.reference = 0;

		depth_stencil_state.back = depth_stencil_state.front;
	}

	depth_stencil_state.minDepthBounds = 0.0f;
	depth_stencil_state.maxDepthBounds = 1.0f;

	Com_Memset(&attachment_blend_state, 0, sizeof(attachment_blend_state));
	attachment_blend_state.blendEnable = (state_bits & (GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS)) ? VK_TRUE : VK_FALSE;

	if (def.shadow_phase == SHADOW_EDGES || def.shader_type == TYPE_SIGNLE_TEXTURE_DF)
		attachment_blend_state.colorWriteMask = 0;
	else
		attachment_blend_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	if (attachment_blend_state.blendEnable)
	{
		switch (state_bits & GLS_SRCBLEND_BITS)
		{
		case GLS_SRCBLEND_ZERO:
			attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
			break;
		case GLS_SRCBLEND_ONE:
			attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
			break;
		case GLS_SRCBLEND_DST_COLOR:
			attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_DST_COLOR;
			break;
		case GLS_SRCBLEND_ONE_MINUS_DST_COLOR:
			attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
			break;
		case GLS_SRCBLEND_SRC_ALPHA:
			attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
			break;
		case GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA:
			attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
			break;
		case GLS_SRCBLEND_DST_ALPHA:
			attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
			break;
		case GLS_SRCBLEND_ONE_MINUS_DST_ALPHA:
			attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
			break;
		case GLS_SRCBLEND_ALPHA_SATURATE:
			attachment_blend_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
			break;
		default:
			ri.Error(ERR_DROP, "create_pipeline: invalid src blend state bits\n");
			break;
		}
		switch (state_bits & GLS_DSTBLEND_BITS)
		{
		case GLS_DSTBLEND_ZERO:
			attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
			break;
		case GLS_DSTBLEND_ONE:
			attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
			break;
		case GLS_DSTBLEND_SRC_COLOR:
			attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_SRC_COLOR;
			break;
		case GLS_DSTBLEND_ONE_MINUS_SRC_COLOR:
			attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
			break;
		case GLS_DSTBLEND_SRC_ALPHA:
			attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
			break;
		case GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA:
			attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
			break;
		case GLS_DSTBLEND_DST_ALPHA:
			attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
			break;
		case GLS_DSTBLEND_ONE_MINUS_DST_ALPHA:
			attachment_blend_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
			break;
		default:
			ri.Error(ERR_DROP, "create_pipeline: invalid dst blend state bits\n");
			break;
		}

		attachment_blend_state.srcAlphaBlendFactor = attachment_blend_state.srcColorBlendFactor;
		attachment_blend_state.dstAlphaBlendFactor = attachment_blend_state.dstColorBlendFactor;
		attachment_blend_state.colorBlendOp = VK_BLEND_OP_ADD;
		attachment_blend_state.alphaBlendOp = VK_BLEND_OP_ADD;

		if (def.allow_discard && vkSamples != VK_SAMPLE_COUNT_1_BIT)
		{
			// try to reduce pixel fillrate for transparent surfaces, this yields 1..10% fps increase when multisampling in enabled
			if (attachment_blend_state.srcColorBlendFactor == VK_BLEND_FACTOR_SRC_ALPHA && attachment_blend_state.dstColorBlendFactor == VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA)
			{
				frag_spec_data[7].i = 1;
			}
			else if (attachment_blend_state.srcColorBlendFactor == VK_BLEND_FACTOR_ONE && attachment_blend_state.dstColorBlendFactor == VK_BLEND_FACTOR_ONE)
			{
				frag_spec_data[7].i = 2;
			}
		}
	}

	blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend_state.pNext = nullptr;
	blend_state.flags = 0;
	blend_state.logicOpEnable = VK_FALSE;
	blend_state.logicOp = VK_LOGIC_OP_COPY;
	blend_state.attachmentCount = 1;
	blend_state.pAttachments = &attachment_blend_state;
	blend_state.blendConstants[0] = 0.0f;
	blend_state.blendConstants[1] = 0.0f;
	blend_state.blendConstants[2] = 0.0f;
	blend_state.blendConstants[3] = 0.0f;

	dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic_state.pNext = nullptr;
	dynamic_state.flags = 0;
	dynamic_state.dynamicStateCount = arrayLen(dynamic_state_array);
	dynamic_state.pDynamicStates = dynamic_state_array;

	create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	create_info.pNext = nullptr;
	create_info.flags = 0;
	create_info.stageCount = shader_stages.size();

	// Convert to VkPipelineShaderStageCreateInfo array
	std::array<VkPipelineShaderStageCreateInfo, 2> vk_shader_stages;
	vk_shader_stages[0] = static_cast<VkPipelineShaderStageCreateInfo>(shader_stages[0]);
	vk_shader_stages[1] = static_cast<VkPipelineShaderStageCreateInfo>(shader_stages[1]);

	create_info.pStages = vk_shader_stages.data();

	create_info.pVertexInputState = &vertex_input_state;
	create_info.pInputAssemblyState = &input_assembly_state;
	create_info.pTessellationState = nullptr;
	create_info.pViewportState = &viewport_state;
	create_info.pRasterizationState = &rasterization_state;
	create_info.pMultisampleState = &multisample_state;
	create_info.pDepthStencilState = &depth_stencil_state;
	create_info.pColorBlendState = &blend_state;
	create_info.pDynamicState = &dynamic_state;

	// if ( def.shader_type == TYPE_DOT )
	//	create_info.layout = vk_inst.pipeline_layout_storage;
	// else
	create_info.layout = vk_inst.pipeline_layout;

	if (renderPassIndex == RENDER_PASS_SCREENMAP)
		create_info.renderPass = vk_inst.render_pass.screenmap;
	else
		create_info.renderPass = vk_inst.render_pass.main;

	create_info.subpass = 0;
	create_info.basePipelineHandle = VK_NULL_HANDLE;
	create_info.basePipelineIndex = -1;

	VK_CHECK(qvkCreateGraphicsPipelines(vk_inst.device, vk_inst.pipelineCache, 1, &create_info, nullptr, &pipeline));

	vk_inst.pipeline_create_count++;

	return pipeline;
}

uint32_t vk_alloc_pipeline(const Vk_Pipeline_Def &def)
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
			pipeline->handle[j] = VK_NULL_HANDLE;
		}
		return vk_inst.pipelines_count++;
	}
}

VkPipeline vk_gen_pipeline(uint32_t index)
{
	if (index < vk_inst.pipelines_count)
	{
		VK_Pipeline_t *pipeline = vk_inst.pipelines + index;
		if (pipeline->handle[vk_inst.renderPassIndex] == VK_NULL_HANDLE)
			pipeline->handle[vk_inst.renderPassIndex] = create_pipeline(pipeline->def, vk_inst.renderPassIndex);
		return pipeline->handle[vk_inst.renderPassIndex];
	}
	else
	{
		return VK_NULL_HANDLE;
	}
}

uint32_t vk_find_pipeline_ext(uint32_t base, const Vk_Pipeline_Def &def, bool use)
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

void vk_get_pipeline_def(uint32_t pipeline, Vk_Pipeline_Def &def)
{
	if (pipeline >= vk_inst.pipelines_count)
	{
		Com_Memset(&def, 0, sizeof(def));
	}
	else
	{
		Com_Memcpy(&def, &vk_inst.pipelines[pipeline].def, sizeof(def));
	}
}

static void get_viewport_rect(VkRect2D *r)
{
	if (backEnd.projection2D)
	{
		r->offset.x = 0;
		r->offset.y = 0;
		r->extent.width = vk_inst.renderWidth;
		r->extent.height = vk_inst.renderHeight;
	}
	else
	{
		r->offset.x = backEnd.viewParms.viewportX * vk_inst.renderScaleX;
		r->offset.y = vk_inst.renderHeight - (backEnd.viewParms.viewportY + backEnd.viewParms.viewportHeight) * vk_inst.renderScaleY;
		r->extent.width = (float)backEnd.viewParms.viewportWidth * vk_inst.renderScaleX;
		r->extent.height = (float)backEnd.viewParms.viewportHeight * vk_inst.renderScaleY;
	}
}

static void get_viewport(VkViewport *viewport, Vk_Depth_Range depth_range)
{
	VkRect2D r;

	get_viewport_rect(&r);

	viewport->x = (float)r.offset.x;
	viewport->y = (float)r.offset.y;
	viewport->width = (float)r.extent.width;
	viewport->height = (float)r.extent.height;

	switch (depth_range)
	{
	default:
#ifdef USE_REVERSED_DEPTH
		// case DEPTH_RANGE_NORMAL:
		viewport->minDepth = 0.0f;
		viewport->maxDepth = 1.0f;
		break;
	case DEPTH_RANGE_ZERO:
		viewport->minDepth = 1.0f;
		viewport->maxDepth = 1.0f;
		break;
	case DEPTH_RANGE_ONE:
		viewport->minDepth = 0.0f;
		viewport->maxDepth = 0.0f;
		break;
	case DEPTH_RANGE_WEAPON:
		viewport->minDepth = 0.6f;
		viewport->maxDepth = 1.0f;
		break;
#else
		// case DEPTH_RANGE_NORMAL:
		viewport->minDepth = 0.0f;
		viewport->maxDepth = 1.0f;
		break;
	case DEPTH_RANGE_ZERO:
		viewport->minDepth = 0.0f;
		viewport->maxDepth = 0.0f;
		break;
	case DEPTH_RANGE_ONE:
		viewport->minDepth = 1.0f;
		viewport->maxDepth = 1.0f;
		break;
	case DEPTH_RANGE_WEAPON:
		viewport->minDepth = 0.0f;
		viewport->maxDepth = 0.3f;
		break;
#endif
	}
}

static void get_scissor_rect(VkRect2D *r)
{

	if (backEnd.viewParms.portalView != PV_NONE)
	{
		r->offset.x = backEnd.viewParms.scissorX;
		r->offset.y = glConfig.vidHeight - backEnd.viewParms.scissorY - backEnd.viewParms.scissorHeight;
		r->extent.width = backEnd.viewParms.scissorWidth;
		r->extent.height = backEnd.viewParms.scissorHeight;
	}
	else
	{
		get_viewport_rect(r);

		if (r->offset.x < 0)
			r->offset.x = 0;
		if (r->offset.y < 0)
			r->offset.y = 0;

		if (r->offset.x + r->extent.width > glConfig.vidWidth)
			r->extent.width = glConfig.vidWidth - r->offset.x;
		if (r->offset.y + r->extent.height > glConfig.vidHeight)
			r->extent.height = glConfig.vidHeight - r->offset.y;
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
		myGlMultMatrix(vk_world.modelview_transform, proj, mvp);
	}
}

void vk_clear_color(const vec4_t color)
{

	VkClearAttachment attachment;
	VkClearRect clear_rect[2];
	uint32_t rect_count;

	if (!vk_inst.active)
		return;

	attachment.colorAttachment = 0;
	attachment.clearValue.color.float32[0] = color[0];
	attachment.clearValue.color.float32[1] = color[1];
	attachment.clearValue.color.float32[2] = color[2];
	attachment.clearValue.color.float32[3] = color[3];
	attachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

	get_scissor_rect(&clear_rect[0].rect);
	clear_rect[0].baseArrayLayer = 0;
	clear_rect[0].layerCount = 1;
	rect_count = 1;

#ifdef _DEBUG
	// Split viewport rectangle into two non-overlapping rectangles.
	// It's a HACK to prevent Vulkan validation layer's performance warning:
	//		"vkCmdClearAttachments() issued on command buffer object XXX prior to any Draw Cmds.
	//		 It is recommended you use RenderPass LOAD_OP_CLEAR on Attachments prior to any Draw."
	//
	// NOTE: we don't use LOAD_OP_CLEAR for color attachment when we begin renderpass
	// since at that point we don't know whether we need collor buffer clear (usually we don't).
	{
		uint32_t h = clear_rect[0].rect.extent.height / 2;
		clear_rect[0].rect.extent.height = h;
		clear_rect[1] = clear_rect[0];
		clear_rect[1].rect.offset.y = h;
		rect_count = 2;
	}
#endif

	qvkCmdClearAttachments(vk_inst.cmd->command_buffer, 1, &attachment, rect_count, clear_rect);
}

void vk_clear_depth(bool clear_stencil)
{

	VkClearAttachment attachment;
	VkClearRect clear_rect[1];

	if (!vk_inst.active)
		return;

	if (vk_world.dirty_depth_attachment == 0)
		return;

	attachment.colorAttachment = 0;
#ifdef USE_REVERSED_DEPTH
	attachment.clearValue.depthStencil.depth = 0.0f;
#else
	attachment.clearValue.depthStencil.depth = 1.0f;
#endif
	attachment.clearValue.depthStencil.stencil = 0;
	if (clear_stencil && r_stencilbits->integer)
	{
		attachment.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	}
	else
	{
		attachment.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	}

	get_scissor_rect(&clear_rect[0].rect);
	clear_rect[0].baseArrayLayer = 0;
	clear_rect[0].layerCount = 1;

	qvkCmdClearAttachments(vk_inst.cmd->command_buffer, 1, &attachment, 1, clear_rect);
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

	qvkCmdPushConstants(vk_inst.cmd->command_buffer, vk_inst.pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push_constants), push_constants);

	vk_inst.stats.push_size += sizeof(push_constants);
}

static VkBuffer shade_bufs[8];
static int bind_base;
static int bind_count;

static void vk_bind_index_attr(int index)
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

static void vk_bind_attr(int index, unsigned int item_size, const void *src)
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
		vk_inst.cmd->vertex_buffer_offset = (VkDeviceSize)offset + size;
	}

	vk_bind_index_attr(index);
}

uint32_t vk_tess_index(uint32_t numIndexes, const void *src)
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
		vk_inst.cmd->vertex_buffer_offset = (VkDeviceSize)offset + size;
		return offset;
	}
}

void vk_bind_index_buffer(VkBuffer buffer, uint32_t offset)
{
	if (vk_inst.cmd->curr_index_buffer != buffer || vk_inst.cmd->curr_index_offset != offset)
		qvkCmdBindIndexBuffer(vk_inst.cmd->command_buffer, buffer, offset, VK_INDEX_TYPE_UINT32);

	vk_inst.cmd->curr_index_buffer = buffer;
	vk_inst.cmd->curr_index_offset = offset;
}

void vk_draw_indexed(uint32_t indexCount, uint32_t firstIndex)
{
	qvkCmdDrawIndexed(vk_inst.cmd->command_buffer, indexCount, 1, firstIndex, 0, 0);
}

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

void vk_bind_geometry(uint32_t flags)
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

		qvkCmdBindVertexBuffers(vk_inst.cmd->command_buffer, bind_base, bind_count, shade_bufs, vk_inst.cmd->vbo_offset + bind_base);
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

		qvkCmdBindVertexBuffers(vk_inst.cmd->command_buffer, bind_base, bind_count, shade_bufs, vk_inst.cmd->buf_offset + bind_base);
	}
}

void vk_bind_lighting(int stage, int bundle)
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

		qvkCmdBindVertexBuffers(vk_inst.cmd->command_buffer, 0, 3, shade_bufs, vk_inst.cmd->vbo_offset + 0);
	}
	else
#endif // USE_VBO
	{
		shade_bufs[0] = shade_bufs[1] = shade_bufs[2] = vk_inst.cmd->vertex_buffer;

		vk_bind_attr(0, sizeof(tess.xyz[0]), &tess.xyz[0]);
		vk_bind_attr(1, sizeof(vec2_t), tess.svars.texcoordPtr[bundle]);
		vk_bind_attr(2, sizeof(tess.normal[0]), tess.normal);

		qvkCmdBindVertexBuffers(vk_inst.cmd->command_buffer, bind_base, bind_count, shade_bufs, vk_inst.cmd->buf_offset + bind_base);
	}
}

void vk_reset_descriptor(int index)
{
	vk_inst.cmd->descriptor_set.current[index] = VK_NULL_HANDLE;
}

void vk_update_descriptor(int index, VkDescriptorSet descriptor)
{
	if (vk_inst.cmd->descriptor_set.current[index] != descriptor)
	{
		vk_inst.cmd->descriptor_set.start = (static_cast<uint32_t>(index) < vk_inst.cmd->descriptor_set.start) ? static_cast<uint32_t>(index) : vk_inst.cmd->descriptor_set.start;
		vk_inst.cmd->descriptor_set.end = (static_cast<uint32_t>(index) > vk_inst.cmd->descriptor_set.end) ? static_cast<uint32_t>(index) : vk_inst.cmd->descriptor_set.end;
	}
	vk_inst.cmd->descriptor_set.current[index] = descriptor;
}

void vk_update_descriptor_offset(int index, uint32_t offset)
{
	vk_inst.cmd->descriptor_set.offset[index] = offset;
}

void vk_bind_descriptor_sets(void)
{
	uint32_t offsets[2], offset_count;
	uint32_t start, end, count;

	start = vk_inst.cmd->descriptor_set.start;
	if (start == ~0U)
		return;

	end = vk_inst.cmd->descriptor_set.end;

	offset_count = 0;
	if (start == VK_DESC_STORAGE || start == VK_DESC_UNIFORM)
	{ // uniform offset or storage offset
		offsets[offset_count++] = vk_inst.cmd->descriptor_set.offset[start];
	}

	count = end - start + 1;

	qvkCmdBindDescriptorSets(vk_inst.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_inst.pipeline_layout, start, count, vk_inst.cmd->descriptor_set.current + start, offset_count, offsets);

	vk_inst.cmd->descriptor_set.end = 0;
	vk_inst.cmd->descriptor_set.start = ~0U;
}

void vk_bind_pipeline(uint32_t pipeline)
{
	VkPipeline vkpipe;

	vkpipe = vk_gen_pipeline(pipeline);

	if (vkpipe != vk_inst.cmd->last_pipeline)
	{
		qvkCmdBindPipeline(vk_inst.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vkpipe);
		vk_inst.cmd->last_pipeline = vkpipe;
	}

	vk_world.dirty_depth_attachment |= (vk_inst.pipelines[pipeline].def.state_bits & GLS_DEPTHMASK_TRUE);
}

void vk_draw_geometry(Vk_Depth_Range depth_range, bool indexed)
{
	VkRect2D scissor_rect;
	VkViewport viewport;

	if (vk_inst.geometry_buffer_size_new)
	{
		// geometry buffer overflow happened this frame
		return;
	}

	vk_bind_descriptor_sets();

	// configure pipeline's dynamic state
	if (vk_inst.cmd->depth_range != depth_range)
	{
		vk_inst.cmd->depth_range = depth_range;

		get_scissor_rect(&scissor_rect);

		if (memcmp(&vk_inst.cmd->scissor_rect, &scissor_rect, sizeof(scissor_rect)) != 0)
		{
			qvkCmdSetScissor(vk_inst.cmd->command_buffer, 0, 1, &scissor_rect);
			vk_inst.cmd->scissor_rect = scissor_rect;
		}

		get_viewport(&viewport, depth_range);
		qvkCmdSetViewport(vk_inst.cmd->command_buffer, 0, 1, &viewport);
	}

	// issue draw call(s)
#ifdef USE_VBO
	if (tess.vboIndex)
		VBO_RenderIBOItems();
	else
#endif
		if (indexed)
	{
		qvkCmdDrawIndexed(vk_inst.cmd->command_buffer, vk_inst.cmd->num_indexes, 1, 0, 0, 0);
	}
	else
	{
		qvkCmdDraw(vk_inst.cmd->command_buffer, tess.numVertexes, 1, 0, 0);
	}
}

static void vk_begin_render_pass(VkRenderPass renderPass, VkFramebuffer frameBuffer, bool clearValues, uint32_t width, uint32_t height)
{
	VkRenderPassBeginInfo render_pass_begin_info;
	VkClearValue clear_values[3];

	// Begin render pass.

	render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	render_pass_begin_info.pNext = nullptr;
	render_pass_begin_info.renderPass = renderPass;
	render_pass_begin_info.framebuffer = frameBuffer;
	render_pass_begin_info.renderArea.offset.x = 0;
	render_pass_begin_info.renderArea.offset.y = 0;
	render_pass_begin_info.renderArea.extent.width = width;
	render_pass_begin_info.renderArea.extent.height = height;

	if (clearValues)
	{
		// attachments layout:
		// [0] - resolve/color/presentation
		// [1] - depth/stencil
		// [2] - multisampled color, optional
		Com_Memset(clear_values, 0, sizeof(clear_values));
#ifndef USE_REVERSED_DEPTH
		clear_values[1].depthStencil.depth = 1.0;
#endif
		render_pass_begin_info.clearValueCount = vk_inst.msaaActive ? 3 : 2;
		render_pass_begin_info.pClearValues = clear_values;

		vk_world.dirty_depth_attachment = 0;
	}
	else
	{
		render_pass_begin_info.clearValueCount = 0;
		render_pass_begin_info.pClearValues = nullptr;
	}

	qvkCmdBeginRenderPass(vk_inst.cmd->command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
}

void vk_begin_main_render_pass(void)
{
	VkFramebuffer frameBuffer = vk_inst.framebuffers.main[vk_inst.swapchain_image_index];

	vk_inst.renderPassIndex = RENDER_PASS_MAIN;

	vk_inst.renderWidth = glConfig.vidWidth;
	vk_inst.renderHeight = glConfig.vidHeight;

	// vk_inst.renderScaleX = (float)vk_inst.renderWidth / (float)glConfig.vidWidth;
	// vk_inst.renderScaleY = (float)vk_inst.renderHeight / (float)glConfig.vidHeight;
	vk_inst.renderScaleX = vk_inst.renderScaleY = 1.0f;

	vk_begin_render_pass(vk_inst.render_pass.main, frameBuffer, true, vk_inst.renderWidth, vk_inst.renderHeight);
}

void vk_begin_post_bloom_render_pass(void)
{
	VkFramebuffer frameBuffer = vk_inst.framebuffers.main[vk_inst.swapchain_image_index];

	vk_inst.renderPassIndex = RENDER_PASS_POST_BLOOM;

	vk_inst.renderWidth = glConfig.vidWidth;
	vk_inst.renderHeight = glConfig.vidHeight;

	// vk_inst.renderScaleX = (float)vk_inst.renderWidth / (float)glConfig.vidWidth;
	// vk_inst.renderScaleY = (float)vk_inst.renderHeight / (float)glConfig.vidHeight;
	vk_inst.renderScaleX = vk_inst.renderScaleY = 1.0f;

	vk_begin_render_pass(vk_inst.render_pass.post_bloom, frameBuffer, false, vk_inst.renderWidth, vk_inst.renderHeight);
}

void vk_begin_bloom_extract_render_pass(void)
{
	VkFramebuffer frameBuffer = vk_inst.framebuffers.bloom_extract;

	// vk_inst.renderPassIndex = RENDER_PASS_BLOOM_EXTRACT; // doesn't matter, we will use dedicated pipelines

	vk_inst.renderWidth = gls.captureWidth;
	vk_inst.renderHeight = gls.captureHeight;

	// vk_inst.renderScaleX = (float)vk_inst.renderWidth / (float)glConfig.vidWidth;
	// vk_inst.renderScaleY = (float)vk_inst.renderHeight / (float)glConfig.vidHeight;
	vk_inst.renderScaleX = vk_inst.renderScaleY = 1.0f;

	vk_begin_render_pass(vk_inst.render_pass.bloom_extract, frameBuffer, false, vk_inst.renderWidth, vk_inst.renderHeight);
}

void vk_begin_blur_render_pass(uint32_t index)
{
	VkFramebuffer frameBuffer = vk_inst.framebuffers.blur[index];

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
	VkFramebuffer frameBuffer = vk_inst.framebuffers.screenmap;

	vk_inst.renderPassIndex = RENDER_PASS_SCREENMAP;

	vk_inst.renderWidth = vk_inst.screenMapWidth;
	vk_inst.renderHeight = vk_inst.screenMapHeight;

	vk_inst.renderScaleX = (float)vk_inst.renderWidth / (float)glConfig.vidWidth;
	vk_inst.renderScaleY = (float)vk_inst.renderHeight / (float)glConfig.vidHeight;

	vk_begin_render_pass(vk_inst.render_pass.screenmap, frameBuffer, true, vk_inst.renderWidth, vk_inst.renderHeight);
}

void vk_end_render_pass(void)
{
	qvkCmdEndRenderPass(vk_inst.cmd->command_buffer);

	//	vk_inst.renderPassIndex = RENDER_PASS_MAIN;
}

static bool vk_find_screenmap_drawsurfs(void)
{
	const void *curCmd = &backEndData->commands.cmds;
	const drawBufferCommand_t *db_cmd;
	const drawSurfsCommand_t *ds_cmd;

	for (;;)
	{
		curCmd = PADP(curCmd, sizeof(void *));
		switch (*(const int *)curCmd)
		{
		case RC_DRAW_BUFFER:
			db_cmd = (const drawBufferCommand_t *)curCmd;
			curCmd = (const void *)(db_cmd + 1);
			break;
		case RC_DRAW_SURFS:
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
	VkCommandBufferBeginInfo begin_info;
	// VkFramebuffer frameBuffer;
	VkResult res;

	if (vk_inst.frame_count++) // might happen during stereo rendering
		return;

	if (vk_inst.cmd->waitForFence)
	{

		vk_inst.cmd = &vk_inst.tess[vk_inst.cmd_index++];
		vk_inst.cmd_index %= NUM_COMMAND_BUFFERS;

		vk_inst.cmd->waitForFence = false;
		res = qvkWaitForFences(vk_inst.device, 1, &vk_inst.cmd->rendering_finished_fence, VK_FALSE, 1e10);
		if (res != VK_SUCCESS)
		{
			if (res == VK_ERROR_DEVICE_LOST)
			{
				// silently discard previous command buffer
				ri.Printf(PRINT_WARNING, "Vulkan: %s returned %s", "vkWaitForfences", vk_result_string(res).data());
			}
			else
			{
				ri.Error(ERR_FATAL, "Vulkan: %s returned %s", "vkWaitForfences", vk_result_string(res).data());
			}
		}

		if (!ri.CL_IsMinimized())
		{
			res = qvkAcquireNextImageKHR(vk_inst.device, vk_inst.swapchain, 5 * 1000000000ULL, vk_inst.cmd->image_acquired, VK_NULL_HANDLE, &vk_inst.swapchain_image_index);
			// when running via RDP: "Application has already acquired the maximum number of images (0x2)"
			// probably caused by "device lost" errors
			if (res < 0)
			{
				if (res == VK_ERROR_OUT_OF_DATE_KHR)
				{
					// swapchain re-creation needed
					vk_restart_swapchain(__func__);
				}
				else
				{
					ri.Error(ERR_FATAL, "vkAcquireNextImageKHR returned %s", vk_result_string(res).data());
				}
			}
		}
		else
		{
			vk_inst.swapchain_image_index++;
			vk_inst.swapchain_image_index %= vk_inst.swapchain_image_count;
		}
	}
	else
	{
		// current command buffer has been reset due to geometry buffer overflow/update
		// so we will reuse it with current swapchain image as well
	}

	VK_CHECK(qvkResetFences(vk_inst.device, 1, &vk_inst.cmd->rendering_finished_fence));

	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.pNext = nullptr;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	begin_info.pInheritanceInfo = nullptr;

	VK_CHECK(qvkBeginCommandBuffer(vk_inst.cmd->command_buffer, &begin_info));

	// Ensure visibility of geometry buffers writes.
	// record_buffer_memory_barrier( vk_inst.cmd->command_buffer, vk_inst.cmd->vertex_buffer, vk_inst.cmd->vertex_buffer_offset, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT );

#if 0
	// add explicit layout transition dependency
	if ( vk_inst.fboActive ) {
		record_image_layout_transition( vk_inst.cmd->command_buffer,
			vk_inst.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
			VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );

	} else {
		record_image_layout_transition( vk_inst.cmd->command_buffer,
			vk_inst.swapchain_images[ vk_inst.swapchain_image_index ], VK_IMAGE_ASPECT_COLOR_BIT,
			VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
			VK_ACCESS_MEMORY_READ_BIT, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR );
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

	vk_inst.cmd->last_pipeline = VK_NULL_HANDLE;

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
	Com_Memset(vk_inst.cmd->vbo_offset, 0, sizeof(vk_inst.cmd->vbo_offset));
	vk_inst.cmd->curr_index_buffer = VK_NULL_HANDLE;
	vk_inst.cmd->curr_index_offset = 0;

	Com_Memset(&vk_inst.cmd->descriptor_set, 0, sizeof(vk_inst.cmd->descriptor_set));
	vk_inst.cmd->descriptor_set.start = ~0U;
	// vk_inst.cmd->descriptor_set.end = 0;

	Com_Memset(&vk_inst.cmd->scissor_rect, 0, sizeof(vk_inst.cmd->scissor_rect));

	vk_update_descriptor(VK_DESC_TEXTURE0, tr.whiteImage->descriptor);
	vk_update_descriptor(VK_DESC_TEXTURE1, tr.whiteImage->descriptor);
	if (vk_inst.maxBoundDescriptorSets >= VK_DESC_COUNT)
	{
		vk_update_descriptor(VK_DESC_TEXTURE2, tr.whiteImage->descriptor);
	}

	// other stats
	vk_inst.stats.push_size = 0;
}

static void vk_resize_geometry_buffer(void)
{
	int i;

	vk_end_render_pass();

	VK_CHECK(qvkEndCommandBuffer(vk_inst.cmd->command_buffer));

	qvkResetCommandBuffer(vk_inst.cmd->command_buffer, 0);

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
	const VkPipelineStageFlags wait_dst_stage_mask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	VkSubmitInfo submit_info;

	if (vk_inst.frame_count == 0)
		return;

	vk_inst.frame_count = 0;

	if (vk_inst.geometry_buffer_size_new)
	{
		vk_resize_geometry_buffer();
		return;
	}

	if (vk_inst.fboActive)
	{
		vk_inst.cmd->last_pipeline = VK_NULL_HANDLE; // do not restore clobbered descriptors in vk_bloom()

		if (r_bloom->integer)
		{
			vk_bloom();
		}

		if (backEnd.screenshotMask && vk_inst.capture.image)
		{
			vk_end_render_pass();

			// render to capture FBO
			vk_begin_render_pass(vk_inst.render_pass.capture, vk_inst.framebuffers.capture, false, gls.captureWidth, gls.captureHeight);
			qvkCmdBindPipeline(vk_inst.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_inst.capture_pipeline);
			qvkCmdBindDescriptorSets(vk_inst.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_inst.pipeline_layout_post_process, 0, 1, &vk_inst.color_descriptor, 0, nullptr);

			qvkCmdDraw(vk_inst.cmd->command_buffer, 4, 1, 0, 0);
		}

		if (!ri.CL_IsMinimized())
		{
			vk_end_render_pass();

			vk_inst.renderWidth = gls.windowWidth;
			vk_inst.renderHeight = gls.windowHeight;

			vk_inst.renderScaleX = 1.0;
			vk_inst.renderScaleY = 1.0;

			vk_begin_render_pass(vk_inst.render_pass.gamma, vk_inst.framebuffers.gamma[vk_inst.swapchain_image_index], false, vk_inst.renderWidth, vk_inst.renderHeight);
			qvkCmdBindPipeline(vk_inst.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_inst.gamma_pipeline);
			qvkCmdBindDescriptorSets(vk_inst.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_inst.pipeline_layout_post_process, 0, 1, &vk_inst.color_descriptor, 0, nullptr);

			qvkCmdDraw(vk_inst.cmd->command_buffer, 4, 1, 0, 0);
		}
	}

	vk_end_render_pass();

	VK_CHECK(qvkEndCommandBuffer(vk_inst.cmd->command_buffer));

	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.pNext = nullptr;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &vk_inst.cmd->command_buffer;
	if (!ri.CL_IsMinimized())
	{
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = &vk_inst.cmd->image_acquired;
		submit_info.pWaitDstStageMask = &wait_dst_stage_mask;
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = &vk_inst.cmd->rendering_finished;
	}
	else
	{
		submit_info.waitSemaphoreCount = 0;
		submit_info.pWaitSemaphores = nullptr;
		submit_info.pWaitDstStageMask = nullptr;
		submit_info.signalSemaphoreCount = 0;
		submit_info.pSignalSemaphores = nullptr;
	}

	VK_CHECK(qvkQueueSubmit(vk_inst.queue, 1, &submit_info, vk_inst.cmd->rendering_finished_fence));
	vk_inst.cmd->waitForFence = true;

	// presentation may take undefined time to complete, we can't measure it in a reliable way
	backEnd.pc.msec = ri.Milliseconds() - backEnd.pc.msec;

	// vk_present_frame();
}

void vk_present_frame(void)
{
	VkPresentInfoKHR present_info;
	VkResult res;

	if (ri.CL_IsMinimized())
		return;

	if (!vk_inst.cmd->waitForFence)
		return;

	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.pNext = nullptr;
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = &vk_inst.cmd->rendering_finished;
	present_info.swapchainCount = 1;
	present_info.pSwapchains = &vk_inst.swapchain;
	present_info.pImageIndices = &vk_inst.swapchain_image_index;
	present_info.pResults = nullptr;

	res = qvkQueuePresentKHR(vk_inst.queue, &present_info);
	switch (res)
	{
	case VK_SUCCESS:
		break;
	case VK_SUBOPTIMAL_KHR:
	case VK_ERROR_OUT_OF_DATE_KHR:
		// swapchain re-creation needed
		vk_restart_swapchain(__func__);
		break;
	case VK_ERROR_DEVICE_LOST:
		// we can ignore that
		ri.Printf(PRINT_DEVELOPER, "vkQueuePresentKHR: device lost\n");
		break;
	default:
		// or we don't
		ri.Error(ERR_FATAL, "vkQueuePresentKHR returned %s", vk_result_string(res).data());
	}
}

static bool is_bgr(VkFormat format)
{
	switch (format)
	{
	case VK_FORMAT_B8G8R8A8_UNORM:
	case VK_FORMAT_B8G8R8A8_SNORM:
	case VK_FORMAT_B8G8R8A8_UINT:
	case VK_FORMAT_B8G8R8A8_SINT:
	case VK_FORMAT_B8G8R8A8_SRGB:
	case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
		return true;
	default:
		return false;
	}
}

void vk_read_pixels(byte *buffer, uint32_t width, uint32_t height)
{
	VkCommandBuffer command_buffer;
	VkDeviceMemory memory;
	VkMemoryRequirements memory_requirements;
	VkMemoryPropertyFlags memory_reqs;
	VkMemoryPropertyFlags memory_flags;
	VkMemoryAllocateInfo alloc_info;
	VkImageSubresource subresource;
	VkSubresourceLayout layout;
	VkImageCreateInfo desc;
	VkImage srcImage;
	vk::ImageLayout srcImageLayout;
	VkImage dstImage;
	byte *buffer_ptr;
	byte *data;
	uint32_t pixel_width;
	uint32_t i, n;
	bool invalidate_ptr;

	VK_CHECK(qvkWaitForFences(vk_inst.device, 1, &vk_inst.cmd->rendering_finished_fence, VK_FALSE, 1e12));

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
		srcImage = vk_inst.swapchain_images[vk_inst.swapchain_image_index];
	}

	Com_Memset(&desc, 0, sizeof(desc));

	// Create image in host visible memory to serve as a destination for framebuffer pixels.
	desc.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	desc.pNext = nullptr;
	desc.flags = 0;
	desc.imageType = VK_IMAGE_TYPE_2D;
	desc.format = vk_inst.capture_format;
	desc.extent.width = width;
	desc.extent.height = height;
	desc.extent.depth = 1;
	desc.mipLevels = 1;
	desc.arrayLayers = 1;
	desc.samples = VK_SAMPLE_COUNT_1_BIT;
	desc.tiling = VK_IMAGE_TILING_LINEAR;
	desc.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	desc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	desc.queueFamilyIndexCount = 0;
	desc.pQueueFamilyIndices = nullptr;
	desc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VK_CHECK(qvkCreateImage(vk_inst.device, &desc, nullptr, &dstImage));

	qvkGetImageMemoryRequirements(vk_inst.device, dstImage, &memory_requirements);

	alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc_info.pNext = nullptr;
	alloc_info.allocationSize = memory_requirements.size;

	// host_cached bit is desirable for fast reads
	memory_reqs = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
	alloc_info.memoryTypeIndex = find_memory_type2(memory_requirements.memoryTypeBits, memory_reqs, &memory_flags);
	if (alloc_info.memoryTypeIndex == static_cast<uint32_t>(~0U))
	{
		// try less explicit flags, without host_coherent
		memory_reqs = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
		alloc_info.memoryTypeIndex = find_memory_type2(memory_requirements.memoryTypeBits, memory_reqs, &memory_flags);
		if (alloc_info.memoryTypeIndex == ~0U)
		{
			// slowest case
			memory_reqs = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
			alloc_info.memoryTypeIndex = find_memory_type2(memory_requirements.memoryTypeBits, memory_reqs, &memory_flags);
			if (alloc_info.memoryTypeIndex == ~0U)
			{
				ri.Error(ERR_FATAL, "%s(): failed to find matching memory type for image capture", __func__);
			}
		}
	}

	if (memory_flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
	{
		invalidate_ptr = false;
	}
	else
	{
		// according to specification - must be performed if host_coherent is not set
		invalidate_ptr = true;
	}

	VK_CHECK(qvkAllocateMemory(vk_inst.device, &alloc_info, nullptr, &memory));
	VK_CHECK(qvkBindImageMemory(vk_inst.device, dstImage, memory, 0));

	command_buffer = begin_command_buffer();

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
		VkImageBlit region;

		region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.srcSubresource.mipLevel = 0;
		region.srcSubresource.baseArrayLayer = 0;
		region.srcSubresource.layerCount = 1;
		region.srcOffsets[0].x = 0;
		region.srcOffsets[0].y = 0;
		region.srcOffsets[0].z = 0;
		region.srcOffsets[1].x = width;
		region.srcOffsets[1].y = height;
		region.srcOffsets[1].z = 1;
		region.dstSubresource = region.srcSubresource;
		region.dstOffsets[0] = region.srcOffsets[0];
		region.dstOffsets[1] = region.srcOffsets[1];

		qvkCmdBlitImage(command_buffer, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region, VK_FILTER_NEAREST);
	}
	else
	{
		VkImageCopy region;

		region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.srcSubresource.mipLevel = 0;
		region.srcSubresource.baseArrayLayer = 0;
		region.srcSubresource.layerCount = 1;
		region.srcOffset.x = 0;
		region.srcOffset.y = 0;
		region.srcOffset.z = 0;
		region.dstSubresource = region.srcSubresource;
		region.dstOffset = region.srcOffset;
		region.extent.width = width;
		region.extent.height = height;
		region.extent.depth = 1;

		qvkCmdCopyImage(command_buffer, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
	}

	end_command_buffer(command_buffer);

	// Copy data from destination image to memory buffer.
	subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresource.mipLevel = 0;
	subresource.arrayLayer = 0;

	qvkGetImageSubresourceLayout(vk_inst.device, dstImage, &subresource, &layout);

	VK_CHECK(qvkMapMemory(vk_inst.device, memory, 0, VK_WHOLE_SIZE, 0, (void **)&data));

	if (invalidate_ptr)
	{
		VkMappedMemoryRange range;
		range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		range.pNext = nullptr;
		range.memory = memory;
		range.size = VK_WHOLE_SIZE;
		range.offset = 0;
		qvkInvalidateMappedMemoryRanges(vk_inst.device, 1, &range);
	}

	data += layout.offset;

	switch (vk_inst.capture_format)
	{
	case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
		pixel_width = 2;
		break;
	case VK_FORMAT_R16G16B16A16_UNORM:
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

	qvkDestroyImage(vk_inst.device, dstImage, nullptr);
	qvkFreeMemory(vk_inst.device, memory, nullptr);

	// restore previous layout
	if (srcImageLayout != vk::ImageLayout::eTransferSrcOptimal)
	{
		command_buffer = begin_command_buffer();

		record_image_layout_transition(command_buffer, srcImage,
									   vk::ImageAspectFlagBits::eColor,
									   vk::ImageLayout::eTransferSrcOptimal,
									   srcImageLayout);

		end_command_buffer(command_buffer);
	}
}

bool vk_bloom(void)
{
	uint32_t i;

	if (vk_inst.renderPassIndex == RENDER_PASS_SCREENMAP)
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
	qvkCmdBindPipeline(vk_inst.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_inst.bloom_extract_pipeline);
	qvkCmdBindDescriptorSets(vk_inst.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_inst.pipeline_layout_post_process, 0, 1, &vk_inst.color_descriptor, 0, nullptr);
	qvkCmdDraw(vk_inst.cmd->command_buffer, 4, 1, 0, 0);
	vk_end_render_pass();

	for (i = 0; i < VK_NUM_BLOOM_PASSES * 2; i += 2)
	{
		// horizontal blur
		vk_begin_blur_render_pass(i + 0);
		qvkCmdBindPipeline(vk_inst.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_inst.blur_pipeline[i + 0]);
		qvkCmdBindDescriptorSets(vk_inst.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_inst.pipeline_layout_post_process, 0, 1, &vk_inst.bloom_image_descriptor[i + 0], 0, nullptr);
		qvkCmdDraw(vk_inst.cmd->command_buffer, 4, 1, 0, 0);
		vk_end_render_pass();

		// vectical blur
		vk_begin_blur_render_pass(i + 1);
		qvkCmdBindPipeline(vk_inst.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_inst.blur_pipeline[i + 1]);
		qvkCmdBindDescriptorSets(vk_inst.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_inst.pipeline_layout_post_process, 0, 1, &vk_inst.bloom_image_descriptor[i + 1], 0, nullptr);
		qvkCmdDraw(vk_inst.cmd->command_buffer, 4, 1, 0, 0);
		vk_end_render_pass();
#if 0
		// horizontal blur
		vk_begin_blur_render_pass( i+0 );
		qvkCmdBindPipeline( vk_inst.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_inst.blur_pipeline[i+0] );
		qvkCmdBindDescriptorSets( vk_inst.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_inst.pipeline_layout_post_process, 0, 1, &vk_inst.bloom_image_descriptor[i+2], 0, nullptr );
		qvkCmdDraw( vk_inst.cmd->command_buffer, 4, 1, 0, 0 );
		vk_end_render_pass();

		// vectical blur
		vk_begin_blur_render_pass( i+1 );
		qvkCmdBindPipeline( vk_inst.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_inst.blur_pipeline[i+1] );
		qvkCmdBindDescriptorSets( vk_inst.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_inst.pipeline_layout_post_process, 0, 1, &vk_inst.bloom_image_descriptor[i+1], 0, nullptr );
		qvkCmdDraw( vk_inst.cmd->command_buffer, 4, 1, 0, 0 );
		vk_end_render_pass();
#endif
	}

	vk_begin_post_bloom_render_pass(); // begin post-bloom
	{
		VkDescriptorSet dset[VK_NUM_BLOOM_PASSES];

		for (i = 0; i < VK_NUM_BLOOM_PASSES; i++)
		{
			dset[i] = vk_inst.bloom_image_descriptor[(i + 1) * 2];
		}

		// blend downscaled buffers to main fbo
		qvkCmdBindPipeline(vk_inst.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_inst.bloom_blend_pipeline);
		qvkCmdBindDescriptorSets(vk_inst.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_inst.pipeline_layout_blend, 0, arrayLen(dset), dset, 0, nullptr);
		qvkCmdDraw(vk_inst.cmd->command_buffer, 4, 1, 0, 0);
	}

	// invalidate pipeline state cache
	// vk_inst.cmd->last_pipeline = VK_NULL_HANDLE;

	if (vk_inst.cmd->last_pipeline != VK_NULL_HANDLE)
	{
		// restore last pipeline
		qvkCmdBindPipeline(vk_inst.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_inst.cmd->last_pipeline);

		vk_update_mvp(nullptr);

		// force depth range and viewport/scissor updates
		vk_inst.cmd->depth_range = DEPTH_RANGE_COUNT;

		// restore clobbered descriptor sets
		for (i = 0; i < VK_NUM_BLOOM_PASSES; i++)
		{
			if (vk_inst.cmd->descriptor_set.current[i] != VK_NULL_HANDLE)
			{
				if (i == VK_DESC_UNIFORM || i == VK_DESC_STORAGE)
					qvkCmdBindDescriptorSets(vk_inst.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_inst.pipeline_layout, i, 1, &vk_inst.cmd->descriptor_set.current[i], 1, &vk_inst.cmd->descriptor_set.offset[i]);
				else
					qvkCmdBindDescriptorSets(vk_inst.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_inst.pipeline_layout, i, 1, &vk_inst.cmd->descriptor_set.current[i], 0, nullptr);
			}
		}
	}

	backEnd.doneBloom = true;

	return true;
}

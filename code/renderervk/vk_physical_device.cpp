#include "vk_physical_device.hpp"

#include <cstring>
#include <algorithm>
#include <numeric>
#include <array>
#include <vector>

#include "tr_local.hpp"     // Vk_Instance, glConfig, cvars, ri, etc.
#include "vk_utils.hpp"     // find_memory_type, VK_CHECK_ASSIGN, etc.
#include "string_operations.hpp"

constexpr int defaultVulkanApiVersion = VK_API_VERSION_1_0;
static int vulkanApiVersion = defaultVulkanApiVersion;


// -------------------------
// Local helpers (private)
// -------------------------

static vk::Format get_hdr_format(const vk::Format base_format)
{
	if (r_fbo->integer == 0)
		return base_format;

	switch (r_hdr->integer)
	{
	case -1: return vk::Format::eB4G4R4A4UnormPack16;
	case  1: return vk::Format::eR16G16B16A16Unorm;
	default: return base_format;
	}
}

typedef struct
{
	int       bits;
	vk::Format rgb;
	vk::Format bgr;
} present_format_t;

static constexpr present_format_t present_formats[] = {
	{16, vk::Format::eB5G6R5UnormPack16,  vk::Format::eR5G6B5UnormPack16},
	{24, vk::Format::eB8G8R8A8Unorm,      vk::Format::eR8G8B8A8Unorm},
	{30, vk::Format::eA2B10G10R10UnormPack32, vk::Format::eA2R10G10B10UnormPack32},
};

static void get_present_format(const int present_bits, vk::Format& bgr, vk::Format& rgb)
{
	const present_format_t* sel = nullptr;
	for (const auto& pf : present_formats)
		if (pf.bits <= present_bits) sel = &pf;

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

static vk::Format get_depth_format(const vk::PhysicalDevice& physical_device)
{
	vk::FormatProperties props{};
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

	for (const auto& format : formats)
	{
		props = physical_device.getFormatProperties(format);
		if ((props.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment) != vk::FormatFeatureFlags{})
			return format;
	}

	ri.Error(ERR_FATAL, "get_depth_format: failed to find depth attachment format");
	return vk::Format::eUndefined;
}

static bool vk_blit_enabled(const vk::PhysicalDevice& physical_device, vk::Format srcFormat, vk::Format dstFormat)
{
	vk::FormatProperties srcProps = physical_device.getFormatProperties(srcFormat);
	if ((srcProps.optimalTilingFeatures & vk::FormatFeatureFlagBits::eBlitSrc) == vk::FormatFeatureFlags{})
		return false;

	vk::FormatProperties dstProps = physical_device.getFormatProperties(dstFormat);
	if ((dstProps.linearTilingFeatures & vk::FormatFeatureFlagBits::eBlitDst) == vk::FormatFeatureFlags{})
		return false;

	return true;
}

static bool select_surface_format(
	Vk_Instance& vk_inst,
	const vk::PhysicalDevice& physical_device,
	const vk::SurfaceKHR& surface)
{
	vk::Format base_bgr{}, base_rgb{};
	vk::Format ext_bgr{}, ext_rgb{};

	std::vector<vk::SurfaceFormatKHR> candidates;
	VK_CHECK_ASSIGN(candidates, physical_device.getSurfaceFormatsKHR(surface));

	if (candidates.empty())
	{
		ri.Printf(PRINT_ERROR, "...no surface formats found\n");
		return false;
	}

	get_present_format(24, base_bgr, base_rgb);

	if (r_fbo->integer)
		get_present_format(r_presentBits->integer, ext_bgr, ext_rgb);
	else
	{
		ext_bgr = base_bgr;
		ext_rgb = base_rgb;
	}

	if (candidates.size() == 1 && candidates[0].format == vk::Format::eUndefined)
	{
		vk_inst.base_format.format = base_bgr;
		vk_inst.base_format.colorSpace = vk::ColorSpaceKHR::eVkColorspaceSrgbNonlinear;
		vk_inst.present_format.format = ext_bgr;
		vk_inst.present_format.colorSpace = vk::ColorSpaceKHR::eVkColorspaceSrgbNonlinear;
	}
	else
	{
		// base format
		{
			bool found = false;
			for (const auto& c : candidates)
			{
				if ((c.format == base_bgr || c.format == base_rgb) &&
					c.colorSpace == vk::ColorSpaceKHR::eVkColorspaceSrgbNonlinear)
				{
					vk_inst.base_format = c;
					found = true;
					break;
				}
			}
			if (!found) vk_inst.base_format = candidates[0];
		}

		// present format (HDR / presentBits)
		{
			bool found = false;
			for (const auto& c : candidates)
			{
				if ((c.format == ext_bgr || c.format == ext_rgb) &&
					c.colorSpace == vk::ColorSpaceKHR::eVkColorspaceSrgbNonlinear)
				{
					vk_inst.present_format = c;
					found = true;
					break;
				}
			}
			if (!found) vk_inst.present_format = vk_inst.base_format;
		}
	}

	if (!r_fbo->integer)
		vk_inst.present_format = vk_inst.base_format;

	return true;
}

static void setup_surface_formats(Vk_Instance& vk_inst, const vk::PhysicalDevice& physical_device)
{
	vk_inst.depth_format = get_depth_format(physical_device);
	vk_inst.color_format = get_hdr_format(vk_inst.base_format.format);
	vk_inst.capture_format = vk::Format::eR8G8B8A8Unorm;
	vk_inst.bloom_format = vk_inst.base_format.format;

	vk_inst.blitEnabled = vk_blit_enabled(physical_device, vk_inst.color_format, vk_inst.capture_format);
	if (!vk_inst.blitEnabled)
		vk_inst.capture_format = vk_inst.color_format;
}

static bool find_graphics_present_queue(
	Vk_Instance& vk_inst,
	const vk::PhysicalDevice& physical_device,
	const vk::SurfaceKHR& surface)
{
	const auto queue_families = physical_device.getQueueFamilyProperties();

	vk_inst.queue_family_index = ~0u;

	for (uint32_t i = 0; i < queue_families.size(); ++i)
	{
		vk::Bool32 presentation_supported{};
		VK_CHECK_ASSIGN(presentation_supported, physical_device.getSurfaceSupportKHR(i, surface));

		const bool graphics = (queue_families[i].queueFlags & vk::QueueFlagBits::eGraphics) != vk::QueueFlags{};
		if (presentation_supported && graphics)
		{
			vk_inst.queue_family_index = i;
			return true;
		}
	}

	ri.Printf(PRINT_ERROR, "...failed to find graphics queue family\n");
	return false;
}

static bool create_logical_device(
	Vk_Instance& vk_inst,
	const vk::PhysicalDevice& physical_device)
{
	// Enumerate device extensions and build list.
	std::vector<const char*> device_extension_list;
	bool swapchainSupported = false;

	bool dedicatedAllocation = false;
	bool memoryRequirements2 = false;

#ifdef USE_VK_VALIDATION
	bool debugMarker = false;
	bool timelineSemaphore = false;
	bool memoryModel = false;
	bool devAddrFeat = false;
	bool storage8bit = false;

	vk::PhysicalDeviceTimelineSemaphoreFeatures     timeline_semaphore{};
	vk::PhysicalDeviceVulkanMemoryModelFeatures     memory_model{};
	vk::PhysicalDeviceBufferDeviceAddressFeatures   devaddr_features{};
	vk::PhysicalDevice8BitStorageFeatures           storage_8bit_features{};
#endif

	std::vector<vk::ExtensionProperties> exts;
	VK_CHECK_ASSIGN(exts, physical_device.enumerateDeviceExtensionProperties());

	// Fill glConfig.extensions_string like your original does.
	char* str = glConfig.extensions_string;
	*str = '\0';
	const char* end = &glConfig.extensions_string[sizeof(glConfig.extensions_string) - 1];

	for (size_t i = 0; i < exts.size(); ++i)
	{
		const auto ext = std::string_view(exts[i].extensionName.data());

		// append to glConfig.extensions_string
		if (i != 0)
		{
			if (str + 1 < end) str = Q_stradd(str, " ");
		}
		if (str + ext.size() < end) str = Q_stradd(str, ext.data());

		// Vulkan 1.0 compatibility: (your legacy path)
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

	if (!memoryRequirements2) dedicatedAllocation = false;

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
	if (timelineSemaphore) device_extension_list.push_back(vk::KHRTimelineSemaphoreExtensionName);
	if (memoryModel)       device_extension_list.push_back(vk::KHRVulkanMemoryModelExtensionName);
	if (devAddrFeat)       device_extension_list.push_back(vk::KHRBufferDeviceAddressExtensionName);
	if (storage8bit)       device_extension_list.push_back(vk::KHR8BitStorageExtensionName);
#endif

	// Required core features.
	vk::PhysicalDeviceFeatures device_features = physical_device.getFeatures();
	if (device_features.fillModeNonSolid == vk::False)
	{
		ri.Printf(PRINT_ERROR, "...fillModeNonSolid feature is not supported\n");
		return false;
	}

	const float priority = 1.0f;
	vk::DeviceQueueCreateInfo queue_desc{ {}, vk_inst.queue_family_index, 1, &priority };

	vk::PhysicalDeviceFeatures features{};
	features.fillModeNonSolid = vk::True;

#ifdef USE_VK_VALIDATION
	if (device_features.shaderInt64)
		features.shaderInt64 = vk::True;
#endif

	if (device_features.wideLines)
	{
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

	vk::DeviceCreateInfo device_desc{
		{},
		1, &queue_desc,
		0, nullptr,
		static_cast<uint32_t>(device_extension_list.size()),
		device_extension_list.data(),
		&features
	};

#ifdef USE_VK_VALIDATION
	// pNext chain (as you had)
	const void** pNextPtr = (const void**)&device_desc.pNext;

	if (timelineSemaphore)
	{
		timeline_semaphore.sType = vk::StructureType::ePhysicalDeviceTimelineSemaphoreFeatures;
		timeline_semaphore.timelineSemaphore = vk::True;
		timeline_semaphore.pNext = nullptr;
		*pNextPtr = &timeline_semaphore;
		pNextPtr = (const void**)&timeline_semaphore.pNext;
	}
	if (memoryModel)
	{
		memory_model.sType = vk::StructureType::ePhysicalDeviceVulkanMemoryModelFeatures;
		memory_model.vulkanMemoryModel = VK_TRUE;
		memory_model.vulkanMemoryModelAvailabilityVisibilityChains = VK_FALSE;
		memory_model.vulkanMemoryModelDeviceScope = VK_TRUE;
		memory_model.pNext = nullptr;
		*pNextPtr = &memory_model;
		pNextPtr = (const void**)&memory_model.pNext;
	}
	if (devAddrFeat)
	{
		devaddr_features.sType = vk::StructureType::ePhysicalDeviceBufferDeviceAddressFeatures;
		devaddr_features.bufferDeviceAddress = VK_TRUE;
		devaddr_features.bufferDeviceAddressCaptureReplay = VK_FALSE;
		devaddr_features.bufferDeviceAddressMultiDevice = VK_FALSE;
		devaddr_features.pNext = nullptr;
		*pNextPtr = &devaddr_features;
		pNextPtr = (const void**)&devaddr_features.pNext;
	}
	if (storage8bit)
	{
		storage_8bit_features.sType = vk::StructureType::ePhysicalDevice8BitStorageFeatures;
		storage_8bit_features.storageBuffer8BitAccess = VK_TRUE;
		storage_8bit_features.uniformAndStorageBuffer8BitAccess = VK_TRUE;
		storage_8bit_features.storagePushConstant8 = VK_FALSE;
		storage_8bit_features.pNext = nullptr;
		*pNextPtr = &storage_8bit_features;
		pNextPtr = (const void**)&storage_8bit_features.pNext;
	}
#endif

	VK_CHECK_ASSIGN(vk_inst.device, physical_device.createDevice(device_desc));
	return true;
}

// -------------------------
// Public API
// -------------------------


const char* renderer_name(const vk::PhysicalDeviceProperties& props)
{
	static char buf[sizeof(props.deviceName) + 64];
	const char* device_type = "OTHER";

	switch (props.deviceType)
	{
	case vk::PhysicalDeviceType::eIntegratedGpu: device_type = "Integrated"; break;
	case vk::PhysicalDeviceType::eDiscreteGpu:   device_type = "Discrete";   break;
	case vk::PhysicalDeviceType::eVirtualGpu:    device_type = "Virtual";    break;
	case vk::PhysicalDeviceType::eCpu:           device_type = "CPU";        break;
	default: break;
	}

	char name[256];
	std::memcpy(name, props.deviceName.data(), 256);
	Com_sprintf(buf, sizeof(buf), "%s %s, 0x%04x", device_type, name, props.deviceID);
	return buf;
}

std::vector<vk::PhysicalDevice> enumerate_devices(vk::Instance instance, bool verbose)
{
	std::vector<vk::PhysicalDevice> gpus;
	VK_CHECK_ASSIGN(gpus, instance.enumeratePhysicalDevices());

	if (verbose)
	{
		ri.Printf(PRINT_ALL, ".......................\nAvailable physical devices:\n");
		for (uint32_t i = 0; i < gpus.size(); ++i)
		{
			const auto props = gpus[i].getProperties();
			ri.Printf(PRINT_ALL, " %i: %s\n", i, renderer_name(props));
		}
		ri.Printf(PRINT_ALL, ".......................\n");
	}

	return gpus;
}

int choose_initial_device_index(const std::vector<vk::PhysicalDevice>& gpus, int r_device_value)
{
	if (gpus.empty()) return -1;

	// explicit index
	if (r_device_value >= 0)
		return std::min(r_device_value, static_cast<int>(gpus.size() - 1));

	// -1 => discrete, -2 => integrated
	if (r_device_value == -1 || r_device_value == -2)
	{
		const auto want = (r_device_value == -1)
			? vk::PhysicalDeviceType::eDiscreteGpu
			: vk::PhysicalDeviceType::eIntegratedGpu;

		for (int i = 0; i < static_cast<int>(gpus.size()); ++i)
		{
			if (gpus[i].getProperties().deviceType == want)
				return i;
		}
	}

	// fallback
	return 0;
}

bool pick_and_create_device(
	Vk_Instance& vk_inst,
	vk::Instance   instance,
	vk::SurfaceKHR surface,
	int            r_device_value,
	bool           verbose)
{
	auto gpus = enumerate_devices(instance, verbose);
	if (gpus.empty())
	{
		ri.Error(ERR_FATAL, "Vulkan: no physical devices found");
		return false;
	}

	const int startIndex = choose_initial_device_index(gpus, r_device_value);

	// Try starting from startIndex, then wrap around.
	for (int attempt = 0; attempt < static_cast<int>(gpus.size()); ++attempt)
	{
		int idx = startIndex + attempt;
		if (idx >= static_cast<int>(gpus.size())) idx %= static_cast<int>(gpus.size());
		if (idx < 0) idx = 0;

		const auto& gpu = gpus[idx];
		ri.Printf(PRINT_ALL, "...selected physical device candidate: %i\n", idx);

		// Surface format selection
		if (!select_surface_format(vk_inst, gpu, surface))
			continue;

		setup_surface_formats(vk_inst, gpu);

		// Queue family selection
		if (!find_graphics_present_queue(vk_inst, gpu, surface))
			continue;

		// Logical device creation
		if (!create_logical_device(vk_inst, gpu))
			continue;

		vk_inst.physical_device = gpu;
		ri.Printf(PRINT_ALL, "...selected physical device: %i (%s)\n", idx, renderer_name(gpu.getProperties()));
		return true;
	}

	ri.Error(ERR_FATAL, "Vulkan: unable to find any suitable physical device");
	return false;
}


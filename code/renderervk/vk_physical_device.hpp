#pragma once

#include <vector>
#include <string_view>
#include "definitions.hpp"

// Centralized "physical device + logical device creation" module.

struct PickResult
{
	vk::PhysicalDevice physical = {};
	int                deviceIndex = -1;      // index in enumeratePhysicalDevices()
	uint32_t           queueFamilyIndex = ~0u; // graphics+present queue family
};

// Returns a stable printable name.
const char* renderer_name(const vk::PhysicalDeviceProperties& props);

// Enumerate GPUs and print them (uses renderer_name()).
// Returns list of physical devices.
std::vector<vk::PhysicalDevice> enumerate_devices(vk::Instance instance, bool verbose);

// Chooses a device index from r_device semantics:
//  -1 => first discrete, -2 => first integrated, else => explicit index (clamped).
int choose_initial_device_index(const std::vector<vk::PhysicalDevice>& gpus, int r_device_value);

// Picks a GPU (based on initial index + fallback tries) AND creates vk_inst.device.
// On success: fills vk_inst.physical_device, vk_inst.device, vk_inst.queue_family_index, formats, etc.
bool pick_and_create_device(
	Vk_Instance& vk_inst,
	vk::Instance       instance,
	vk::SurfaceKHR     surface,
	int                r_device_value,
	bool               verbose);


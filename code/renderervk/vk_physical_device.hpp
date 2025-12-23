#pragma once
#include <vulkan/vulkan.hpp>
#include <array>
#include <vector>
#include <string_view>
#include <cstdint>
#include "tr_local.hpp"

// ---- MSAA helpers (constexpr, header-only, no runtime cost) -----------------
constexpr uint32_t vk_flag_to_count(vk::SampleCountFlagBits f) {
    switch (f) {
        case vk::SampleCountFlagBits::e1: return 1u;
        case vk::SampleCountFlagBits::e2: return 2u;
        case vk::SampleCountFlagBits::e4: return 4u;
        case vk::SampleCountFlagBits::e8: return 8u;
        case vk::SampleCountFlagBits::e16: return 16u;
        case vk::SampleCountFlagBits::e32: return 32u;
        case vk::SampleCountFlagBits::e64: return 64u;
        default: return 1u;
    }
}
constexpr uint32_t vk_flags_to_max_count(vk::SampleCountFlags f) {
    uint32_t m = 0u;
    if (f & vk::SampleCountFlagBits::e64) m = 64u;
    else if (f & vk::SampleCountFlagBits::e32) m = 32u;
    else if (f & vk::SampleCountFlagBits::e16) m = 16u;
    else if (f & vk::SampleCountFlagBits::e8)  m = 8u;
    else if (f & vk::SampleCountFlagBits::e4)  m = 4u;
    else if (f & vk::SampleCountFlagBits::e2)  m = 2u;
    else                                       m = 1u;
    return m;
}
constexpr vk::SampleCountFlagBits vk_clamp_samples(vk::SampleCountFlags supported, uint32_t requested) {
    if (requested >= 64u && (supported & vk::SampleCountFlagBits::e64)) return vk::SampleCountFlagBits::e64;
    if (requested >= 32u && (supported & vk::SampleCountFlagBits::e32)) return vk::SampleCountFlagBits::e32;
    if (requested >= 16u && (supported & vk::SampleCountFlagBits::e16)) return vk::SampleCountFlagBits::e16;
    if (requested >= 8u  && (supported & vk::SampleCountFlagBits::e8 )) return vk::SampleCountFlagBits::e8;
    if (requested >= 4u  && (supported & vk::SampleCountFlagBits::e4 )) return vk::SampleCountFlagBits::e4;
    if (requested >= 2u  && (supported & vk::SampleCountFlagBits::e2 )) return vk::SampleCountFlagBits::e2;
    return vk::SampleCountFlagBits::e1;
}

// ---- DeviceCaps: frozen, POD-ish capability snapshot ------------------------
struct DeviceCaps {
    // Chosen physical device + some derived info
    vk::PhysicalDevice              phys{};
    vk::PhysicalDeviceProperties    props{};
    vk::PhysicalDeviceFeatures      features{};
    vk::PhysicalDeviceMemoryProperties mem{};

    // Queue families
    uint32_t graphicsQF  = VK_QUEUE_FAMILY_IGNORED;
    uint32_t presentQF   = VK_QUEUE_FAMILY_IGNORED;

    // Formats (base/present and render targets)
    vk::SurfaceFormatKHR baseFormat{};    // “best” surface format engine wants
    vk::SurfaceFormatKHR presentFormat{}; // actual chosen format for swapchain
    vk::Format           colorFormat{};   // internal color (HDR/SDR mapping)
    vk::Format           depthFormat{};   // chosen depth-stencil

    // MSAA
    vk::SampleCountFlags supportedSamples{};
    vk::SampleCountFlagBits msaaSamples{vk::SampleCountFlagBits::e1};

    // Misc caps / toggles used in engine
    bool   blitEnabled = false;
    bool   samplerAnisotropy = false;
    bool   wideLines = false;
    bool   fragmentStores = false;
    bool   dedicatedAllocation = false;
    bool   debugMarkers = false;

    // Convenience
    char   renderer[256]{};
};

// Encapsulates selection preferences (mapped from your CVARs one-to-one)
struct DevicePickArgs {
    // r_device semantics in your engine:
    //  >=0 -> explicit index
    //  -1  -> prefer discrete
    //  -2  -> prefer integrated
    //  else-> first suitable
    int requestedIndex = -1;
};

// ---- API --------------------------------------------------------------------
bool PickPhysicalDevice(vk::Instance instance,
                        vk::SurfaceKHR surface,
                        const DevicePickArgs& args,
                        DeviceCaps& out);

vk::Device CreateLogicalDevice(const DeviceCaps& caps,
                               std::span<const char* const> extraExtensions,
                               std::span<const void* const>  pNextFeatureChain,
                               vk::Queue* outGraphicsQueue,
                               vk::Queue* outPresentQueue);

// Helper queries exposed for reuse (swapchain, attachments)
vk::Format     ChooseDepthFormat(const vk::PhysicalDevice& phys);
bool           CanBlitFormats  (const vk::PhysicalDevice& phys,
                                vk::Format src, vk::Format dst);
bool           ChooseSurfaceFormat(const vk::PhysicalDevice& phys,
                                   vk::SurfaceKHR surface,
                                   uint32_t desiredColorBits,
                                   vk::SurfaceFormatKHR& outBase,
                                   vk::SurfaceFormatKHR& outPresent);

// Fill “pretty” device string (e.g. “Discrete NVIDIA GeForce RTX 3080, 0xXXXX”)
void           BuildRendererName(const vk::PhysicalDeviceProperties& props,
                                 std::string_view typeName,
                                 char (&dst)[256]) noexcept;

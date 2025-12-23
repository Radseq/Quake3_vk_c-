#include "vk_physical_device.hpp"
#include <algorithm>
#include <cstring>

// --- local utilities ---------------------------------------------------------
static const char* device_type_name(vk::PhysicalDeviceType t) {
    switch (t) {
        case vk::PhysicalDeviceType::eDiscreteGpu:   return "Discrete";
        case vk::PhysicalDeviceType::eIntegratedGpu: return "Integrated";
        case vk::PhysicalDeviceType::eVirtualGpu:    return "Virtual";
        case vk::PhysicalDeviceType::eCpu:           return "CPU";
        default:                                     return "Other";
    }
}

void BuildRendererName(const vk::PhysicalDeviceProperties& props,
                       std::string_view typeName,
                       char (&dst)[256]) noexcept {
    // props.deviceName is char[VK_MAX_PHYSICAL_DEVICE_NAME_SIZE]
    char local[256]{};
    std::memcpy(local, props.deviceName.data(), std::min<size_t>(props.deviceName.size(), 255));
    std::snprintf(dst, sizeof(dst), "%.*s %s, 0x%04x",
                  int(typeName.size()), typeName.data(),
                  local, props.deviceID);
}

// Prefer B8G8R8A8 for WSI if available (Windows/Linux common), fallback to R8G8B8A8.
static bool select_surface_format_impl(const vk::PhysicalDevice& phys,
                                       vk::SurfaceKHR surface,
                                       uint32_t desiredBits,
                                       vk::SurfaceFormatKHR& outBase,
                                       vk::SurfaceFormatKHR& outPresent)
{
    auto formats = phys.getSurfaceFormatsKHR(surface);
    if (formats.empty()) return false;

    const auto prefer = [&](vk::Format f)->int {
        // Rank formats: exact match first, then UNORM of similar bitness.
        int score = 0;
        if (f == vk::Format::eB8G8R8A8Unorm) score += 100;
        if (f == vk::Format::eR8G8B8A8Unorm) score += 90;
        // Favor exact bit match when desiredBits (24/30/36 etc.) is set
        // Note: for simplicity treat 24/32 as near. You can expand this mapping.
        if (desiredBits >= 30) {
            if (f == vk::Format::eA2R10G10B10UnormPack32 ||
                f == vk::Format::eA2B10G10R10UnormPack32) score += 15;
        }
        return score;
    };

    // Choose “base” (engine friendly) and “present” (exact surface) formats.
    // If SRGB variants are required, you can extend this; this keeps parity with your current code.
    std::sort(formats.begin(), formats.end(), [&](const auto& a, const auto& b){
        return prefer(a.format) > prefer(b.format);
    });

    outBase    = formats.front();   // engine-preferred
    outPresent = outBase;           // default same; can diverge later if needed

    // If FBO path is disabled, force present==base (matches your current logic).
    if (!r_fbo->integer) outPresent = outBase;
    return true;
}

vk::Format ChooseDepthFormat(const vk::PhysicalDevice& phys)
{
    // Preserve your policy: prefer D24S8 when stencilBits>0 otherwise D32 or D16.
    const bool wantStencil = (glConfig.stencilBits > 0);
    const std::array<vk::Format, 2> candidatesStencil{
        vk::Format::eD24UnormS8Uint,
        vk::Format::eD32SfloatS8Uint
    };
    const std::array<vk::Format, 3> candidatesNoStencil{
        vk::Format::eD32Sfloat,
        vk::Format::eX8D24UnormPack32,
        vk::Format::eD16Unorm
    };

    auto supports = [&](vk::Format f)->bool{
        const auto props = phys.getFormatProperties(f);
        const auto req = vk::FormatFeatureFlagBits::eDepthStencilAttachment;
        return bool(props.optimalTilingFeatures & req);
    };

    if (wantStencil) {
        for (auto f : candidatesStencil) if (supports(f)) return f;
    } else {
        for (auto f : candidatesNoStencil) if (supports(f)) return f;
    }
    throw std::runtime_error("Vulkan: failed to find a suitable depth format");
}

bool CanBlitFormats(const vk::PhysicalDevice& phys, vk::Format src, vk::Format dst)
{
    const auto s = phys.getFormatProperties(src);
    if (!(s.optimalTilingFeatures & vk::FormatFeatureFlagBits::eBlitSrc)) return false;
    const auto d = phys.getFormatProperties(dst);
    if (!(d.optimalTilingFeatures & vk::FormatFeatureFlagBits::eBlitDst)) return false;
    return true;
}

bool ChooseSurfaceFormat(const vk::PhysicalDevice& phys,
                         vk::SurfaceKHR surface,
                         uint32_t desiredColorBits,
                         vk::SurfaceFormatKHR& outBase,
                         vk::SurfaceFormatKHR& outPresent)
{
    return select_surface_format_impl(phys, surface, desiredColorBits, outBase, outPresent);
}

// Find queue family indices (graphics + present).
static bool pick_queue_families(const vk::PhysicalDevice& phys,
                                vk::SurfaceKHR surface,
                                uint32_t& outGraphicsQF,
                                uint32_t& outPresentQF)
{
    const auto families = phys.getQueueFamilyProperties();
    uint32_t bestGraphics = VK_QUEUE_FAMILY_IGNORED;
    uint32_t bestPresent  = VK_QUEUE_FAMILY_IGNORED;

    for (uint32_t i = 0; i < families.size(); ++i) {
        const bool graphics = (families[i].queueFlags & vk::QueueFlagBits::eGraphics) &&
                              families[i].queueCount > 0;
        const bool present  = phys.getSurfaceSupportKHR(i, surface);

        if (graphics && bestGraphics == VK_QUEUE_FAMILY_IGNORED) bestGraphics = i;
        if (present  && bestPresent  == VK_QUEUE_FAMILY_IGNORED) bestPresent  = i;
        if (bestGraphics != VK_QUEUE_FAMILY_IGNORED &&
            bestPresent  != VK_QUEUE_FAMILY_IGNORED) break;
    }

    if (bestGraphics == VK_QUEUE_FAMILY_IGNORED ||
        bestPresent  == VK_QUEUE_FAMILY_IGNORED) return false;

    outGraphicsQF = bestGraphics;
    outPresentQF  = bestPresent;
    return true;
}

// Rank devices similar to your current r_device policy.
static int rank_device(const vk::PhysicalDevice& d, int requestedIndex, uint32_t actualIndex)
{
    if (requestedIndex >= 0) {
        // explicit index: return high score only if matches
        return (int(actualIndex) == requestedIndex) ? 1'000'000 : -1;
    }

    const auto props = d.getProperties();
    switch (requestedIndex) {
        case -1: // prefer discrete
            return props.deviceType == vk::PhysicalDeviceType::eDiscreteGpu ? 2 : 1;
        case -2: // prefer integrated
            return props.deviceType == vk::PhysicalDeviceType::eIntegratedGpu ? 2 : 1;
        default:
            return 1;
    }
}

bool PickPhysicalDevice(vk::Instance instance,
                        vk::SurfaceKHR surface,
                        const DevicePickArgs& args,
                        DeviceCaps& out)
{
    const auto devices = instance.enumeratePhysicalDevices();
    if (devices.empty()) return false;

    int bestScore = -1;
    vk::PhysicalDevice best{};
    uint32_t bestG = 0, bestP = 0;

    for (uint32_t i = 0; i < devices.size(); ++i) {
        uint32_t g = 0, p = 0;
        if (!pick_queue_families(devices[i], surface, g, p)) continue;

        const int score = rank_device(devices[i], args.requestedIndex, i);
        if (score > bestScore) {
            bestScore = score; best = devices[i]; bestG = g; bestP = p;
        }
    }
    if (!best) return false;

    out.phys = best;
    out.graphicsQF = bestG;
    out.presentQF  = bestP;
    out.props      = best.getProperties();
    out.features   = best.getFeatures();
    out.mem        = best.getMemoryProperties();

    // Formats
    if (!ChooseSurfaceFormat(best, surface, uint32_t(r_presentBits->integer),
                             out.baseFormat, out.presentFormat))
        return false;

    out.depthFormat = ChooseDepthFormat(best);

    // MSAA support caps
    const auto limits = out.props.limits;
    out.supportedSamples = limits.framebufferColorSampleCounts & limits.framebufferDepthSampleCounts;

    // Your logic: clamp to user’s desired (from CVARs) or just highest supported when enabled.
    const uint32_t wantSamples = r_ext_multisample->integer ? std::max(1, r_ext_multisample->integer) : 1;
    out.msaaSamples = vk_clamp_samples(out.supportedSamples, wantSamples);

    // Blit
    out.blitEnabled = CanBlitFormats(best, out.baseFormat.format, out.baseFormat.format);

    // Feature flags mirrored from your code
    out.samplerAnisotropy = out.features.samplerAnisotropy;
    out.wideLines         = out.features.wideLines;
    out.fragmentStores    = out.features.fragmentStoresAndAtomics;

    // Vendor quirks / dedicated allocation / debug markers toggles you maintain elsewhere
    out.dedicatedAllocation = true; // you gate via KHR_get_memory_requirements2 + dedicated_allocation
    out.debugMarkers        = vk_debug_markers_supported;

    BuildRendererName(out.props, device_type_name(out.props.deviceType), out.renderer);
    return true;
}

vk::Device CreateLogicalDevice(const DeviceCaps& caps,
                               std::span<const char* const> extraExtensions,
                               std::span<const void* const>  pNextFeatureChain,
                               vk::Queue* outGraphicsQueue,
                               vk::Queue* outPresentQueue)
{
    // Required device extensions for WSI
    std::vector<const char*> exts{
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };
    exts.insert(exts.end(), extraExtensions.begin(), extraExtensions.end());

#ifdef USE_VK_VALIDATION
    // If you enable debug marker/tag extensions only when available
    if (caps.debugMarkers) {
        exts.push_back(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
    }
#endif

    // Queue setup (dedicated or shared)
    const bool sameQ = (caps.graphicsQF == caps.presentQF);
    const float prio = 1.0f;

    std::vector<vk::DeviceQueueCreateInfo> qcis;
    qcis.reserve(sameQ ? 1 : 2);
    qcis.emplace_back(vk::DeviceQueueCreateInfo{}
        .setQueueFamilyIndex(caps.graphicsQF)
        .setQueueCount(1)
        .setPQueuePriorities(&prio));
    if (!sameQ) {
        qcis.emplace_back(vk::DeviceQueueCreateInfo{}
            .setQueueFamilyIndex(caps.presentQF)
            .setQueueCount(1)
            .setPQueuePriorities(&prio));
    }

    // Base features: we reuse probed features but you can mask to safe subset here.
    vk::PhysicalDeviceFeatures enabledFeatures = {};
    enabledFeatures.samplerAnisotropy = caps.samplerAnisotropy ? VK_TRUE : VK_FALSE;
    enabledFeatures.wideLines         = caps.wideLines ? VK_TRUE : VK_FALSE;
    enabledFeatures.fragmentStoresAndAtomics = caps.fragmentStores ? VK_TRUE : VK_FALSE;

    // Build pNext chain from provided feature structs (timeline semaphores, memory model, etc.)
    const void* pNextHead = nullptr;
    if (!pNextFeatureChain.empty()) {
        // Chain is pre-linked on caller side; just pass first head
        pNextHead = pNextFeatureChain.front();
    }

    vk::DeviceCreateInfo dci{};
    dci.setQueueCreateInfos(qcis)
       .setPEnabledFeatures(&enabledFeatures)
       .setPEnabledExtensionNames(exts)
       .setPNext(pNextHead);

#ifndef USE_VK_VALIDATION
    // Vulkan-Hpp without exceptions prefers error codes; but we built with exceptions as well.
#endif

    vk::Device device = caps.phys.createDevice(dci);

    if (outGraphicsQueue) *outGraphicsQueue = device.getQueue(caps.graphicsQF, 0);
    if (outPresentQueue)  *outPresentQueue  = device.getQueue(caps.presentQF,  0);

    return device;
}

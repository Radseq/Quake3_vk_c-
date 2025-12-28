#include "vk_attachments.hpp"
#include "utils.hpp"
#include "vk_utils.hpp"
#include "vk_descriptors.hpp"

typedef struct vk_attach_desc_s
{
	vk::Image descriptor = {};
	vk::ImageView* image_view = nullptr;
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

static void vk_add_attachment_desc(const vk::Image& desc, vk::ImageView& image_view, vk::ImageUsageFlags usage,
	vk::MemoryRequirements* reqs, vk::Format image_format,
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

static void vk_get_image_memory_requirements(const vk::Image& image, vk::MemoryRequirements* memory_requirements)
{
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
	vk::Format format, vk::ImageUsageFlags usage, vk::Image& image,
	vk::ImageView& image_view, vk::ImageLayout image_layout, bool multisample)
{
	vk::MemoryRequirements memory_requirements;

	if (multisample && !(usage & vk::ImageUsageFlagBits::eSampled))
		usage |= vk::ImageUsageFlagBits::eTransientAttachment;

	// create color image
	vk::ImageCreateInfo create_desc{ {},
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
									nullptr };

	// VK_CHECK(qvkCreateImage(vk_inst.device, &create_desc, nullptr, image));
	VK_CHECK_ASSIGN(image, vk_inst.device.createImage(create_desc));

	vk_get_image_memory_requirements(image, &memory_requirements);

	vk_add_attachment_desc(image, image_view, usage, &memory_requirements, format, vk::ImageAspectFlagBits::eColor, image_layout);
}

static void create_depth_attachment(uint32_t width, uint32_t height, vk::SampleCountFlagBits samples,
	vk::Image& image, vk::ImageView& image_view, bool allowTransient)
{
	vk::MemoryRequirements memory_requirements;
	vk::ImageAspectFlags image_aspect_flags;

	vk::ImageUsageFlags usage{ vk::ImageUsageFlagBits::eDepthStencilAttachment };
	if (allowTransient)
	{
		usage |= vk::ImageUsageFlagBits::eTransientAttachment;
	}

	// create depth image
	vk::ImageCreateInfo create_desc{ {},
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
									nullptr };

	image_aspect_flags = vk::ImageAspectFlagBits::eDepth;
	if (glConfig.stencilBits)
		image_aspect_flags |= vk::ImageAspectFlagBits::eStencil;

	// VK_CHECK(qvkCreateImage(vk_inst.device, &create_desc, nullptr, image));
	VK_CHECK_ASSIGN(image, vk_inst.device.createImage(create_desc));

	vk_get_image_memory_requirements(image, &memory_requirements);

	vk_add_attachment_desc(image, image_view, create_desc.usage, &memory_requirements, vk_inst.depth_format,
		image_aspect_flags, vk::ImageLayout::eDepthStencilAttachmentOptimal);
}

static void vk_alloc_attachments()
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
		offset = pad_up(offset, alignment);
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
		alloc_info2 = { attachments[0].descriptor, {} };
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

		vk::ImageViewCreateInfo view_desc{ {},
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
										  nullptr };

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

void vk_create_attachments()
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

	SET_OBJECT_NAME(VkImage(vk_inst.capture.image), "capture image", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT);
	SET_OBJECT_NAME(VkImageView(vk_inst.capture.image_view), "capture image view", VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT);

	for (i = 0; i < arrayLen(vk_inst.bloom_image); i++)
	{
		SET_OBJECT_NAME(VkImage(vk_inst.bloom_image[i]), va("bloom attachment %i", i), VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT);
		SET_OBJECT_NAME(VkImageView(vk_inst.bloom_image_view[i]), va("bloom attachment %i", i), VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT);
	}
#endif
}

void vk_destroy_attachments()
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
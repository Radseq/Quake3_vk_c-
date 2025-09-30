#include "vk_utils.hpp"

vk::CommandBuffer begin_command_buffer()
{
	vk::CommandBuffer command_buffer;

	vk::CommandBufferAllocateInfo alloc_info{
		vk_inst.command_pool,
		vk::CommandBufferLevel::ePrimary,
		1,
		nullptr };

	VK_CHECK(vk_inst.device.allocateCommandBuffers(&alloc_info, &command_buffer));

	vk::CommandBufferBeginInfo begin_info{
		vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
		nullptr,
		nullptr };

	VK_CHECK(command_buffer.begin(begin_info));

	return command_buffer;
}

void vk_queue_wait_idle(void)
{
	VK_CHECK(vk_inst.queue.waitIdle());
}

void end_command_buffer(const vk::CommandBuffer& command_buffer, const char* location)
{
#ifdef USE_UPLOAD_QUEUE
	const vk::PipelineStageFlags wait_dst_stage_mask = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
	vk::Semaphore waits{};
#endif

	VK_CHECK(command_buffer.end());

	vk::SubmitInfo submit_info{ 0,
							   nullptr,
							   nullptr,
							   1,
							   &command_buffer,
							   0,
							   nullptr };
#ifdef USE_UPLOAD_QUEUE
	if (vk_inst.rendering_finished != vk::Semaphore{}) {
		waits = vk_inst.rendering_finished;
		vk_inst.rendering_finished = vk::Semaphore{};   // clear to "null"

		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = &waits;
		submit_info.pWaitDstStageMask = &wait_dst_stage_mask;
	}
	else
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


void record_image_layout_transition(const vk::CommandBuffer& command_buffer, const vk::Image& image,
	const vk::ImageAspectFlags image_aspect_flags,
	const vk::ImageLayout old_layout,
	const vk::ImageLayout new_layout,
	vk::PipelineStageFlags src_stage_override,  // empty => no override
	vk::PipelineStageFlags dst_stage_override   // empty => no override
)
{
	vk::ImageMemoryBarrier barrier{ get_src_access_mask(old_layout),
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
								   nullptr };

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

	if (dst_stage_override)
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

uint32_t find_memory_type2(const uint32_t memory_type_bits, const vk::MemoryPropertyFlags properties, vk::MemoryPropertyFlags* outprops)
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

uint32_t find_memory_type(const uint32_t memory_type_bits, const vk::MemoryPropertyFlags properties)
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
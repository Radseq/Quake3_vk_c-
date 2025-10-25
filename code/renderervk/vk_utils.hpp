#ifndef VK_UTILS_HPP
#define VK_UTILS_HPP

#include "tr_local.hpp"

vk::CommandBuffer begin_command_buffer();
void end_command_buffer(const vk::CommandBuffer& command_buffer, const char* location);

void vk_queue_wait_idle(void);

void record_image_layout_transition(const vk::CommandBuffer& command_buffer, const vk::Image& image,
	const vk::ImageAspectFlags image_aspect_flags,
	const vk::ImageLayout old_layout,
	const vk::ImageLayout new_layout,
	vk::PipelineStageFlags src_stage_override = {},  // empty => no override
	vk::PipelineStageFlags dst_stage_override = {}   // empty => no override
);

uint32_t find_memory_type2(const uint32_t memory_type_bits, const vk::MemoryPropertyFlags properties, vk::MemoryPropertyFlags* outprops);
uint32_t find_memory_type(const uint32_t memory_type_bits, const vk::MemoryPropertyFlags properties);

#endif // VK_UTILS_HPP

#ifndef VK_RENDER_PASS_HPP
#define VK_RENDER_PASS_HPP

#include "tr_local.hpp"

void vk_begin_post_bloom_render_pass(void);
void vk_begin_bloom_extract_render_pass(void);

void vk_begin_blur_render_pass(const uint32_t index);
void vk_end_render_pass(void);
void vk_begin_main_render_pass(void);

void vk_create_render_passes(void);
void vk_destroy_render_passes();
void vk_begin_screenmap_render_pass(void);
void vk_begin_render_pass(const vk::RenderPass& renderPass, const vk::Framebuffer& frameBuffer, const bool clearValues, const uint32_t width, const uint32_t height);

#endif // VK_RENDER_PASS_HPP

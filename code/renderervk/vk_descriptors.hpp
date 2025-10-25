#ifndef VK_DESCRIPTORS_HPP
#define VK_DESCRIPTORS_HPP

#include "tr_local.hpp"

constexpr int VK_DESC_STORAGE = 0;
constexpr int VK_DESC_UNIFORM = 0;
constexpr int VK_DESC_TEXTURE0 = 1;
constexpr int VK_DESC_TEXTURE1 = 2;
constexpr int VK_DESC_TEXTURE2 = 3;
constexpr int VK_DESC_FOG_COLLAPSE = 4;
constexpr int VK_DESC_COUNT = 5;

constexpr int VK_DESC_TEXTURE_BASE = VK_DESC_TEXTURE0;
constexpr int VK_DESC_FOG_ONLY = VK_DESC_TEXTURE1;
constexpr int VK_DESC_FOG_DLIGHT = VK_DESC_TEXTURE1;

//void vk_descriptors_init();
//void vk_descriptors_shutdown();
void vk_reset_descriptor(int idx);
void vk_update_descriptor(int idx, const vk::DescriptorSet&);
void vk_update_descriptor_offset(int idx, uint32_t);
void vk_bind_descriptor_sets();
void vk_update_descriptor_set(const image_t&, bool mipmap);
void vk_update_attachment_descriptors(void);

void vk_update_uniform_descriptor(const vk::DescriptorSet& descriptor, const vk::Buffer& buffer);

#endif // VK_DESCRIPTORS_HPP

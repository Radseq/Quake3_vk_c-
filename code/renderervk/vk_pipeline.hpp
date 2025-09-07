#ifndef VK_PIPELINE_HPP
#define VK_PIPELINE_HPP

#include "tr_local.hpp"

void vk_alloc_persistent_pipelines();
uint32_t vk_find_pipeline_ext(const uint32_t base, const Vk_Pipeline_Def& def, bool use);
vk::Pipeline vk_gen_pipeline(const uint32_t index);
vk::Pipeline create_pipeline(const Vk_Pipeline_Def& def, const renderPass_t renderPassIndex, uint32_t def_index);
void vk_bind_pipeline(const uint32_t pipeline);

void vk_create_blur_pipeline(const uint32_t index, const uint32_t width, const uint32_t height, const bool horizontal_pass);
void vk_create_post_process_pipeline(const int program_index, const uint32_t width, const uint32_t height);

void vk_create_pipelines();

void vk_get_pipeline_def(const uint32_t pipeline, Vk_Pipeline_Def& def);

void vk_destroy_pipelines(bool resetCounter);

void vk_update_post_process_pipelines(void);


#endif // VK_PIPELINE_HPP

#include "vk_descriptors.hpp"

static vk::Sampler vk_find_sampler(const Vk_Sampler_Def& def)
{
	int i;

	// Look for sampler among existing samplers.
	for (i = 0; i < vk_inst.samplers.count; i++) {
		const Vk_Sampler_Def* cur_def = &vk_inst.samplers.def[i];
		if (memcmp(cur_def, &def, sizeof(def)) == 0) {
			return vk_inst.samplers.handle[i];
		}
	}

	vk::SamplerAddressMode address_mode;
	vk::Sampler sampler;
	vk::Filter mag_filter;
	vk::Filter min_filter;
	vk::SamplerMipmapMode mipmap_mode;
	float maxLod;

	// Create new sampler.
	if (vk_inst.samplers.count >= MAX_VK_SAMPLERS) {
		ri.Error(ERR_DROP, "vk_find_sampler: MAX_VK_SAMPLERS hit\n");
		// return VK_NULL_HANDLE;
	}

	address_mode = def.address_mode;

	if (def.gl_mag_filter == std::to_underlying(glCompat::GL_NEAREST))
	{
		mag_filter = vk::Filter::eNearest;
	}
	else if (def.gl_mag_filter == std::to_underlying(glCompat::GL_LINEAR))
	{
		mag_filter = vk::Filter::eLinear;
	}
	else
	{
		ri.Error(ERR_FATAL, "vk_find_sampler: invalid gl_mag_filter");
		return nullptr;
	}

	maxLod = vk_inst.maxLod;

	if (def.gl_min_filter == std::to_underlying(glCompat::GL_NEAREST))
	{
		min_filter = vk::Filter::eNearest;
		mipmap_mode = vk::SamplerMipmapMode::eNearest;
		maxLod = 0.25f; // used to emulate OpenGL's GL_LINEAR/GL_NEAREST minification filter
	}
	else if (def.gl_min_filter == std::to_underlying(glCompat::GL_LINEAR))
	{
		min_filter = vk::Filter::eLinear;
		mipmap_mode = vk::SamplerMipmapMode::eNearest;
		maxLod = 0.25f; // used to emulate OpenGL's GL_LINEAR/GL_NEAREST minification filter
	}
	else if (def.gl_min_filter == std::to_underlying(glCompat::GL_NEAREST_MIPMAP_NEAREST))
	{
		min_filter = vk::Filter::eNearest;
		mipmap_mode = vk::SamplerMipmapMode::eNearest;
	}
	else if (def.gl_min_filter == std::to_underlying(glCompat::GL_LINEAR_MIPMAP_NEAREST))
	{
		min_filter = vk::Filter::eLinear;
		mipmap_mode = vk::SamplerMipmapMode::eNearest;
	}
	else if (def.gl_min_filter == std::to_underlying(glCompat::GL_NEAREST_MIPMAP_LINEAR))
	{
		min_filter = vk::Filter::eNearest;
		mipmap_mode = vk::SamplerMipmapMode::eLinear;
	}
	else if (def.gl_min_filter == std::to_underlying(glCompat::GL_LINEAR_MIPMAP_LINEAR))
	{
		min_filter = vk::Filter::eLinear;
		mipmap_mode = vk::SamplerMipmapMode::eLinear;
	}
	else
	{
		ri.Error(ERR_FATAL, "vk_find_sampler: invalid gl_min_filter");
		return nullptr;
	}

	if (def.max_lod_1_0)
	{
		maxLod = 1.0f;
	}

	vk::SamplerCreateInfo desc{ {},
							   mag_filter,
							   min_filter,
							   mipmap_mode,
							   address_mode,
							   address_mode,
							   address_mode,
							   0.0f,
							   {},
							   {},
							   vk::False,
							   vk::CompareOp::eAlways,
							   0.0f,
							   (maxLod == vk_inst.maxLod) ? VK_LOD_CLAMP_NONE : maxLod,
							   vk::BorderColor::eFloatTransparentBlack,
							   vk::False,
							   nullptr };

	if (def.noAnisotropy || mipmap_mode == vk::SamplerMipmapMode::eNearest || mag_filter == vk::Filter::eNearest)
	{
		desc.anisotropyEnable = vk::False;
		desc.maxAnisotropy = 1.0f;
	}
	else
	{
		desc.anisotropyEnable = (r_ext_texture_filter_anisotropic->integer && vk_inst.samplerAnisotropy) ? vk::True : vk::False;
		if (desc.anisotropyEnable)
		{
			desc.maxAnisotropy = MIN(r_ext_max_anisotropy->integer, vk_inst.maxAnisotropy);
		}
	}

	VK_CHECK_ASSIGN(sampler, vk_inst.device.createSampler(desc));
#ifdef USE_VK_VALIDATION
	SET_OBJECT_NAME(VkSampler(sampler), va("image sampler %i", vk_inst.samplers.count), VK_DEBUG_REPORT_OBJECT_TYPE_SAMPLER_EXT);
#endif
	vk_inst.samplers.def[vk_inst.samplers.count] = def;
	vk_inst.samplers.handle[vk_inst.samplers.count] = sampler;
	vk_inst.samplers.count++;

	return sampler;
}

void vk_reset_descriptor(const int index)
{
	vk_inst.cmd->descriptor_set.current[index] = nullptr;
}

void vk_update_descriptor(const int index, const vk::DescriptorSet& descriptor)
{
	if (vk_inst.cmd->descriptor_set.current[index] != descriptor)
	{
		vk_inst.cmd->descriptor_set.start = (static_cast<uint32_t>(index) < vk_inst.cmd->descriptor_set.start) ? static_cast<uint32_t>(index) : vk_inst.cmd->descriptor_set.start;
		vk_inst.cmd->descriptor_set.end = (static_cast<uint32_t>(index) > vk_inst.cmd->descriptor_set.end) ? static_cast<uint32_t>(index) : vk_inst.cmd->descriptor_set.end;
	}
	vk_inst.cmd->descriptor_set.current[index] = descriptor;
}

void vk_update_descriptor_offset(const int index, const uint32_t offset)
{
	vk_inst.cmd->descriptor_set.offset[index] = offset;
}

void vk_bind_descriptor_sets(void)
{
	uint32_t start = vk_inst.cmd->descriptor_set.start;
	if (start == ~0U)
		return;

	uint32_t offsets[2]{}, offset_count;
	uint32_t end, count, i;

	end = vk_inst.cmd->descriptor_set.end;

	offset_count = 0;
	if (/*start == VK_DESC_STORAGE ||*/ start == VK_DESC_UNIFORM)
	{ // uniform offset or storage offset
		offsets[offset_count++] = vk_inst.cmd->descriptor_set.offset[start];
	}

	count = end - start + 1;

	// fill NULL descriptor gaps
	for (i = start + 1; i < end; i++) {
		if (vk_inst.cmd->descriptor_set.current[i] == vk::DescriptorSet()) {
			vk_inst.cmd->descriptor_set.current[i] = tr.whiteImage->descriptor;
		}
	}

	// vk_inst.cmd->command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, vk_inst.pipeline_layout, start, vk_inst.cmd->descriptor_set.current + start, offsets);
	vk_inst.cmd->command_buffer.bindDescriptorSets(
		vk::PipelineBindPoint::eGraphics,
		vk_inst.pipeline_layout,
		start,
		count,
		vk_inst.cmd->descriptor_set.current + start,
		offset_count,
		offsets);

	vk_inst.cmd->descriptor_set.end = 0;
	vk_inst.cmd->descriptor_set.start = ~0U;
}

void vk_update_descriptor_set(const image_t& image, const bool mipmap)
{
	Vk_Sampler_Def sampler_def = {};

	sampler_def.address_mode = image.wrapClampMode;

	if (mipmap)
	{
		sampler_def.gl_mag_filter = gl_filter_max;
		sampler_def.gl_min_filter = gl_filter_min;
	}
	else
	{
		sampler_def.gl_mag_filter = std::to_underlying(glCompat::GL_LINEAR);
		sampler_def.gl_min_filter = std::to_underlying(glCompat::GL_LINEAR);
		// no anisotropy without mipmaps
		sampler_def.noAnisotropy = true;
	}

	vk::DescriptorImageInfo image_info{ vk_find_sampler(sampler_def),
									   image.view,
									   vk::ImageLayout::eShaderReadOnlyOptimal };

	vk::WriteDescriptorSet descriptor_write{ image.descriptor,
											0,
											0,
											1,
											vk::DescriptorType::eCombinedImageSampler,
											&image_info,
											nullptr,
											nullptr,
											nullptr };
	vk_inst.device.updateDescriptorSets(descriptor_write, nullptr);
}

void vk_update_attachment_descriptors(void)
{

	if (vk_inst.color_image_view)
	{

		Vk_Sampler_Def sd{ vk::SamplerAddressMode::eClampToEdge, vk_inst.blitFilter, vk_inst.blitFilter, true, true };

		vk::DescriptorImageInfo info{ vk_find_sampler(sd),
									 vk_inst.color_image_view,
									 vk::ImageLayout::eShaderReadOnlyOptimal };

		vk::WriteDescriptorSet desc{ vk_inst.color_descriptor,
									0,
									0,
									1,
									vk::DescriptorType::eCombinedImageSampler,
									&info,
									nullptr,
									nullptr,
									nullptr };

		vk_inst.device.updateDescriptorSets(desc, nullptr);

		// screenmap
		sd.gl_mag_filter = sd.gl_min_filter = std::to_underlying(glCompat::GL_LINEAR);
		sd.max_lod_1_0 = false;
		sd.noAnisotropy = true;

		info.sampler = vk_find_sampler(sd);

		info.imageView = vk_inst.screenMap.color_image_view;
		desc.dstSet = vk_inst.screenMap.color_descriptor;

		vk_inst.device.updateDescriptorSets(desc, nullptr);

		// bloom images
		if (r_bloom->integer)
		{
			uint32_t i;
			for (i = 0; i < vk_inst.bloom_image_descriptor.size(); i++)
			{
				info.imageView = vk_inst.bloom_image_view[i];
				desc.dstSet = vk_inst.bloom_image_descriptor[i];

				vk_inst.device.updateDescriptorSets(desc, nullptr);
			}
		}
	}
}

void vk_update_uniform_descriptor(const vk::DescriptorSet& descriptor, const vk::Buffer& buffer)
{
	vk::DescriptorBufferInfo info{ buffer, 0, sizeof(vkUniform_t) };

	vk::WriteDescriptorSet desc{ descriptor,
								0,
								0,
								1,
								vk::DescriptorType::eUniformBufferDynamic,
								nullptr,
								&info,
								nullptr,
								nullptr };

	vk_inst.device.updateDescriptorSets(desc, nullptr);
}
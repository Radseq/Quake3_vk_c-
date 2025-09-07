#include "vk_render_pass.hpp"
#include "utils.hpp"

void vk_begin_render_pass(const vk::RenderPass& renderPass, const vk::Framebuffer& frameBuffer, const bool clearValues, const uint32_t width, const uint32_t height)
{
	vk::ClearValue clear_values[3]{};

	// Begin render pass.

	vk::RenderPassBeginInfo render_pass_begin_info{ renderPass,
												   frameBuffer,
												   {{0, 0}, {width, height}},
												   0,
												   nullptr,
												   nullptr };

	if (clearValues)
	{
		// attachments layout:
		// [0] - resolve/color/presentation
		// [1] - depth/stencil
		// [2] - multisampled color, optional
#ifndef USE_REVERSED_DEPTH
		clear_values[1].depthStencil.depth = 1.0;
#endif
		render_pass_begin_info.clearValueCount = vk_inst.msaaActive ? 3 : 2;
		render_pass_begin_info.pClearValues = clear_values;

		vk_world.dirty_depth_attachment = 0;
	}

	vk_inst.cmd->command_buffer.beginRenderPass(render_pass_begin_info, vk::SubpassContents::eInline);
	vk_inst.cmd->last_pipeline = VK_NULL_HANDLE;
	vk_inst.cmd->depth_range = Vk_Depth_Range::DEPTH_RANGE_COUNT;
}

void vk_end_render_pass(void)
{
	vk_inst.cmd->command_buffer.endRenderPass();

	//	vk_inst.renderPassIndex = renderPass_t::RENDER_PASS_MAIN;
}

void vk_begin_main_render_pass(void)
{
	vk::Framebuffer& frameBuffer = vk_inst.framebuffers.main[vk_inst.cmd->swapchain_image_index];

	vk_inst.renderPassIndex = renderPass_t::RENDER_PASS_MAIN;

	vk_inst.renderWidth = glConfig.vidWidth;
	vk_inst.renderHeight = glConfig.vidHeight;

	// vk_inst.renderScaleX = (float)vk_inst.renderWidth / (float)glConfig.vidWidth;
	// vk_inst.renderScaleY = (float)vk_inst.renderHeight / (float)glConfig.vidHeight;
	vk_inst.renderScaleX = vk_inst.renderScaleY = 1.0f;

	vk_begin_render_pass(vk_inst.render_pass.main, frameBuffer, true, vk_inst.renderWidth, vk_inst.renderHeight);
}

void vk_begin_post_bloom_render_pass(void)
{
	vk::Framebuffer& frameBuffer = vk_inst.framebuffers.main[vk_inst.cmd->swapchain_image_index];

	vk_inst.renderPassIndex = renderPass_t::RENDER_PASS_POST_BLOOM;

	vk_inst.renderWidth = glConfig.vidWidth;
	vk_inst.renderHeight = glConfig.vidHeight;

	// vk_inst.renderScaleX = (float)vk_inst.renderWidth / (float)glConfig.vidWidth;
	// vk_inst.renderScaleY = (float)vk_inst.renderHeight / (float)glConfig.vidHeight;
	vk_inst.renderScaleX = vk_inst.renderScaleY = 1.0f;

	vk_begin_render_pass(vk_inst.render_pass.post_bloom, frameBuffer, false, vk_inst.renderWidth, vk_inst.renderHeight);
}

void vk_begin_bloom_extract_render_pass(void)
{
	vk::Framebuffer& frameBuffer = vk_inst.framebuffers.bloom_extract;

	// vk_inst.renderPassIndex = RENDER_PASS_BLOOM_EXTRACT; // doesn't matter, we will use dedicated pipelines

	vk_inst.renderWidth = gls.captureWidth;
	vk_inst.renderHeight = gls.captureHeight;

	// vk_inst.renderScaleX = (float)vk_inst.renderWidth / (float)glConfig.vidWidth;
	// vk_inst.renderScaleY = (float)vk_inst.renderHeight / (float)glConfig.vidHeight;
	vk_inst.renderScaleX = vk_inst.renderScaleY = 1.0f;

	vk_begin_render_pass(vk_inst.render_pass.bloom_extract, frameBuffer, false, vk_inst.renderWidth, vk_inst.renderHeight);
}

void vk_begin_blur_render_pass(const uint32_t index)
{
	vk::Framebuffer& frameBuffer = vk_inst.framebuffers.blur[index];

	// vk_inst.renderPassIndex = RENDER_PASS_BLOOM_EXTRACT; // doesn't matter, we will use dedicated pipelines

	vk_inst.renderWidth = gls.captureWidth / (2 << (index / 2));
	vk_inst.renderHeight = gls.captureHeight / (2 << (index / 2));

	// vk_inst.renderScaleX = (float)vk_inst.renderWidth / (float)glConfig.vidWidth;
	// vk_inst.renderScaleY = (float)vk_inst.renderHeight / (float)glConfig.vidHeight;
	vk_inst.renderScaleX = vk_inst.renderScaleY = 1.0f;

	vk_begin_render_pass(vk_inst.render_pass.blur[index], frameBuffer, false, vk_inst.renderWidth, vk_inst.renderHeight);
}

void vk_create_render_passes(void)
{
	vk::Format depth_format = vk_inst.depth_format;
	vk::Device device = vk_inst.device;
	vk::AttachmentDescription attachments[3]; // color | depth | msaa color
	vk::AttachmentReference colorRef0, depthRef0;
	vk::SubpassDependency deps[3]{};
	vk::RenderPassCreateInfo desc{};

	vk::AttachmentReference colorResolveRef = { 0, vk::ImageLayout::eColorAttachmentOptimal }; // Not UNDEFINED

	if (r_fbo->integer == 0)
	{
		// presentation
		attachments[0].format = vk_inst.present_format.format;
		attachments[0].samples = vk::SampleCountFlagBits::e1;
#ifdef USE_BUFFER_CLEAR
		attachments[0].loadOp = vk::AttachmentLoadOp::eClear;
#else
		attachments[0].loadOp = vk::AttachmentLoadOp::eDontCare; // Assuming this will be completely overwritten
#endif
		attachments[0].storeOp = vk::AttachmentStoreOp::eStore; // needed for presentation
		attachments[0].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
		attachments[0].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
		attachments[0].initialLayout = vk_inst.initSwapchainLayout;
		attachments[0].finalLayout = vk::ImageLayout::ePresentSrcKHR;
	}
	else
	{
		// resolve/color buffer
		attachments[0].format = vk_inst.color_format;
		attachments[0].samples = vk::SampleCountFlagBits::e1;

#ifdef USE_BUFFER_CLEAR
		if (vk_inst.msaaActive)
			attachments[0].loadOp = vk::AttachmentLoadOp::eDontCare; // Assuming this will be completely overwritten
		else
			attachments[0].loadOp = vk::AttachmentLoadOp::eClear;
#else
		attachments[0].loadOp = vk::AttachmentLoadOp::eDontCare; // Assuming this will be completely overwritten
#endif

		attachments[0].storeOp = vk::AttachmentStoreOp::eStore; // needed for next render pass
		attachments[0].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
		attachments[0].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
		attachments[0].initialLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
		attachments[0].finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
	}

	// depth buffer
	attachments[1].format = depth_format;
	attachments[1].samples = vkSamples;
	attachments[1].loadOp = vk::AttachmentLoadOp::eClear; // Need empty depth buffer before use
	attachments[1].stencilLoadOp = glConfig.stencilBits ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eDontCare;
	if (r_bloom->integer)
	{
		attachments[1].storeOp = vk::AttachmentStoreOp::eStore; // keep it for post-bloom pass
		attachments[1].stencilStoreOp = glConfig.stencilBits ? vk::AttachmentStoreOp::eStore : vk::AttachmentStoreOp::eDontCare;
	}
	else
	{
		attachments[1].storeOp = vk::AttachmentStoreOp::eDontCare;
		attachments[1].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
	}
	attachments[1].initialLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
	attachments[1].finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

	colorRef0 = { 0, vk::ImageLayout::eColorAttachmentOptimal };
	depthRef0 = { 1, vk::ImageLayout::eDepthStencilAttachmentOptimal };

	vk::SubpassDescription subpass{ {},
								   vk::PipelineBindPoint::eGraphics,
								   0,
								   nullptr,
								   1,
								   &colorRef0,
								   &colorResolveRef,
								   &depthRef0,
								   0,
								   nullptr };

	desc = { {}, 2, attachments, 1, &subpass, 0, nullptr };

	if (vk_inst.msaaActive)
	{
		attachments[2].format = vk_inst.color_format;
		attachments[2].samples = vkSamples;
#ifdef USE_BUFFER_CLEAR
		attachments[2].loadOp = vk::AttachmentLoadOp::eClear;
#else
		attachments[2].loadOp = vk::AttachmentLoadOp::eDontCare;
#endif
		if (r_bloom->integer)
		{
			attachments[2].storeOp = vk::AttachmentStoreOp::eStore; // keep it for post-bloom pass
		}
		else
		{
			attachments[2].storeOp = vk::AttachmentStoreOp::eDontCare; // Intermediate storage (not written)
		}
		attachments[2].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
		attachments[2].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
		attachments[2].initialLayout = vk::ImageLayout::eColorAttachmentOptimal;
		attachments[2].finalLayout = vk::ImageLayout::eColorAttachmentOptimal;

		desc.attachmentCount = 3;

		colorRef0.attachment = 2; // msaa image attachment
		colorRef0.layout = vk::ImageLayout::eColorAttachmentOptimal;

		colorResolveRef.attachment = 0; // resolve image attachment
		colorResolveRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

		subpass.pResolveAttachments = &colorResolveRef;
	}
	else
	{
		// Don't use resolve attachments when MSAA is disabled
		subpass.pResolveAttachments = nullptr;
	}

	Com_Memset(&deps, 0, sizeof(deps));

	deps[2].srcSubpass = VK_SUBPASS_EXTERNAL;
	deps[2].dstSubpass = 0;
	deps[2].srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput; // What pipeline stage is waiting on the dependency
	deps[2].dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput; // What pipeline stage is waiting on the dependency
	deps[2].srcAccessMask = vk::AccessFlagBits::eColorAttachmentRead;		  // What access scopes are influence the dependency
	deps[2].dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;		  // What access scopes are waiting on the dependency
	deps[2].dependencyFlags = {};

	if (r_fbo->integer == 0)
	{
		desc.dependencyCount = 1;
		desc.pDependencies = &deps[2];
		VK_CHECK_ASSIGN(vk_inst.render_pass.main, device.createRenderPass(desc));
#ifdef USE_VK_VALIDATION
		SET_OBJECT_NAME(VkRenderPass(vk_inst.render_pass.main), "render pass - main", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT);
#endif
		return;
	}

	deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	deps[0].dstSubpass = 0;
	deps[0].srcStageMask = vk::PipelineStageFlagBits::eFragmentShader;											  // What pipeline stage must have completed for the dependency
	deps[0].dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;									  // What pipeline stage is waiting on the dependency
	deps[0].srcAccessMask = vk::AccessFlagBits::eShaderRead;													  // What access scopes are influence the dependency
	deps[0].dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite; // What access scopes are waiting on the dependency
	deps[0].dependencyFlags = vk::DependencyFlagBits::eByRegion;												  // Only need the current fragment (or tile) synchronized, not the whole framebuffer

	deps[1].srcSubpass = 0;
	deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	deps[1].srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput; // Fragment data has been written
	deps[1].dstStageMask = vk::PipelineStageFlagBits::eFragmentShader;		  // Don't start shading until data is available
	deps[1].srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;		  // Waiting for color data to be written
	deps[1].dstAccessMask = vk::AccessFlagBits::eShaderRead;				  // Don't read things from the shader before ready
	deps[1].dependencyFlags = vk::DependencyFlagBits::eByRegion;			  // Only need the current fragment (or tile) synchronized, not the whole framebuffer

	desc.dependencyCount = 2;
	desc.pDependencies = &deps[0];

	VK_CHECK_ASSIGN(vk_inst.render_pass.main, device.createRenderPass(desc));
#ifdef USE_VK_VALIDATION
	SET_OBJECT_NAME(VkRenderPass(vk_inst.render_pass.main), "render pass - main", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT);
#endif
	if (r_bloom->integer)
	{
		// post-bloom pass
		// color buffer
		attachments[0].loadOp = vk::AttachmentLoadOp::eLoad;
		// depth buffer
		attachments[1].loadOp = vk::AttachmentLoadOp::eLoad;
		attachments[1].storeOp = vk::AttachmentStoreOp::eDontCare;
		attachments[1].stencilLoadOp = glConfig.stencilBits ? vk::AttachmentLoadOp::eLoad : vk::AttachmentLoadOp::eDontCare;

		if (vk_inst.msaaActive)
		{
			// msaa render target
			attachments[2].loadOp = vk::AttachmentLoadOp::eLoad;
			attachments[2].storeOp = vk::AttachmentStoreOp::eDontCare;
		}

		VK_CHECK_ASSIGN(vk_inst.render_pass.post_bloom, device.createRenderPass(desc));
#ifdef USE_VK_VALIDATION
		SET_OBJECT_NAME(VkRenderPass(vk_inst.render_pass.post_bloom), "render pass - post_bloom", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT);
#endif

		// bloom extraction, using resolved/main fbo as a source
		desc.attachmentCount = 1;

		colorRef0.attachment = 0;
		colorRef0.layout = vk::ImageLayout::eColorAttachmentOptimal;

		subpass = { {},
				   vk::PipelineBindPoint::eGraphics,
				   0,
				   nullptr,
				   1,
				   &colorRef0,
				   nullptr,
				   nullptr,
				   0,
				   nullptr };

		attachments[0].format = vk_inst.bloom_format;
		attachments[0].samples = vk::SampleCountFlagBits::e1;
		attachments[0].loadOp = vk::AttachmentLoadOp::eDontCare; // Assuming this will be completely overwritten
		attachments[0].storeOp = vk::AttachmentStoreOp::eStore;	 // needed for next render pass
		attachments[0].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
		attachments[0].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
		attachments[0].initialLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
		attachments[0].finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

		VK_CHECK_ASSIGN(vk_inst.render_pass.bloom_extract, device.createRenderPass(desc));
#ifdef USE_VK_VALIDATION
		SET_OBJECT_NAME(VkRenderPass(vk_inst.render_pass.bloom_extract), "render pass - bloom_extract", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT);
#endif

		for (auto& blur : vk_inst.render_pass.blur)
		{
			VK_CHECK_ASSIGN(blur, device.createRenderPass(desc));
#ifdef USE_VK_VALIDATION
			SET_OBJECT_NAME(VkRenderPass(blur), "render pass - blur", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT);
#endif
		}
	}

	if (vk_inst.capture.image)
	{

		subpass = { {},
				   vk::PipelineBindPoint::eGraphics,
				   0,
				   nullptr,
				   1,
				   &colorRef0,
				   nullptr,
				   nullptr,
				   0,
				   nullptr };

		attachments[0].format = vk_inst.capture_format;
		attachments[0].samples = vk::SampleCountFlagBits::e1;
		attachments[0].loadOp = vk::AttachmentLoadOp::eDontCare; // this will be completely overwritten
		attachments[0].storeOp = vk::AttachmentStoreOp::eStore;	 // needed for next render pass
		attachments[0].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
		attachments[0].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
		attachments[0].initialLayout = vk::ImageLayout::eUndefined;
		attachments[0].finalLayout = vk::ImageLayout::eTransferSrcOptimal;

		colorRef0.attachment = 0;
		colorRef0.layout = vk::ImageLayout::eColorAttachmentOptimal;

		desc.sType = vk::StructureType::eRenderPassCreateInfo;
		desc.pNext = nullptr;
		desc.pAttachments = attachments;
		desc.attachmentCount = 1;
		desc.pSubpasses = &subpass;
		desc.subpassCount = 1;

		VK_CHECK_ASSIGN(vk_inst.render_pass.capture, device.createRenderPass(desc));
#ifdef USE_VK_VALIDATION
		SET_OBJECT_NAME(VkRenderPass(vk_inst.render_pass.capture), "render pass - capture", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT);
#endif
	}

	colorRef0.attachment = 0;
	colorRef0.layout = vk::ImageLayout::eColorAttachmentOptimal;

	desc.attachmentCount = 1;

	subpass = { {},
			   vk::PipelineBindPoint::eGraphics,
			   0,
			   nullptr,
			   1,
			   &colorRef0,
			   nullptr,
			   nullptr,
			   0,
			   nullptr };

	// gamma post-processing
	attachments[0].format = vk_inst.present_format.format;
	attachments[0].samples = vk::SampleCountFlagBits::e1;
	attachments[0].loadOp = vk::AttachmentLoadOp::eDontCare;
	attachments[0].storeOp = vk::AttachmentStoreOp::eStore; // needed for presentation
	attachments[0].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
	attachments[0].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
	attachments[0].initialLayout = vk_inst.initSwapchainLayout;
	attachments[0].finalLayout = vk::ImageLayout::ePresentSrcKHR;

	desc.dependencyCount = 1;
	desc.pDependencies = &deps[2];

	VK_CHECK_ASSIGN(vk_inst.render_pass.gamma, device.createRenderPass(desc));
#ifdef USE_VK_VALIDATION
	SET_OBJECT_NAME(VkRenderPass(vk_inst.render_pass.gamma), "render pass - gamma", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT);
#endif
	desc.dependencyCount = 2;
	desc.pDependencies = &deps[0];

	// screenmap resolve/color buffer
	attachments[0].format = vk_inst.color_format;
	attachments[0].samples = vk::SampleCountFlagBits::e1;
#ifdef USE_BUFFER_CLEAR
	if (vk_inst.screenMapSamples > vk::SampleCountFlagBits::e1)
		attachments[0].loadOp = vk::AttachmentLoadOp::eDontCare;
	else
		attachments[0].loadOp = vk::AttachmentLoadOp::eClear;
#else
	attachments[0].loadOp = vk::AttachmentLoadOp::eDontCare; // Assuming this will be completely overwritten
#endif
	attachments[0].storeOp = vk::AttachmentStoreOp::eStore; // needed for next render pass
	attachments[0].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
	attachments[0].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
	attachments[0].initialLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
	attachments[0].finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

	// screenmap depth buffer
	attachments[1].format = depth_format;
	attachments[1].samples = vk_inst.screenMapSamples;
	attachments[1].loadOp = vk::AttachmentLoadOp::eClear; // Need empty depth buffer before use
	attachments[1].storeOp = vk::AttachmentStoreOp::eDontCare;
	attachments[1].stencilLoadOp = vk::AttachmentLoadOp::eClear;
	attachments[1].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
	attachments[1].initialLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
	attachments[1].finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

	colorRef0.attachment = 0;
	colorRef0.layout = vk::ImageLayout::eColorAttachmentOptimal;

	depthRef0.attachment = 1;
	depthRef0.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

	subpass = { {},
			   vk::PipelineBindPoint::eGraphics,
			   0,
			   nullptr,
			   1,
			   &colorRef0,
			   nullptr,
			   &depthRef0,
			   0,
			   nullptr };

	desc = { {}, 1, attachments, 1, &subpass, 2, deps };

	if (vk_inst.screenMapSamples > vk::SampleCountFlagBits::e1)
	{
		attachments[2].format = vk_inst.color_format;
		attachments[2].samples = vk_inst.screenMapSamples;
#ifdef USE_BUFFER_CLEAR
		attachments[2].loadOp = vk::AttachmentLoadOp::eClear;
#else
		attachments[2].loadOp = vk::AttachmentLoadOp::eDontCare;
#endif
		attachments[2].storeOp = vk::AttachmentStoreOp::eDontCare;
		attachments[2].stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
		attachments[2].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
		attachments[2].initialLayout = vk::ImageLayout::eColorAttachmentOptimal;
		attachments[2].finalLayout = vk::ImageLayout::eColorAttachmentOptimal;

		desc.attachmentCount = 3;

		colorRef0.attachment = 2; // screenmap msaa image attachment
		colorRef0.layout = vk::ImageLayout::eColorAttachmentOptimal;

		colorResolveRef.attachment = 0; // screenmap resolve image attachment
		colorResolveRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

		subpass.pResolveAttachments = &colorResolveRef;
	}

	VK_CHECK_ASSIGN(vk_inst.render_pass.screenmap, device.createRenderPass(desc));
#ifdef USE_VK_VALIDATION
	SET_OBJECT_NAME(VkRenderPass(vk_inst.render_pass.screenmap), "render pass - screenmap", VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT);
#endif
}

void vk_destroy_render_passes()
{
	// Destroy the main render pass
	if (vk_inst.render_pass.main)
	{
		vk_inst.device.destroyRenderPass(vk_inst.render_pass.main);
		vk_inst.render_pass.main = nullptr;
	}

	// Destroy the bloom extract render pass
	if (vk_inst.render_pass.bloom_extract)
	{
		vk_inst.device.destroyRenderPass(vk_inst.render_pass.bloom_extract);
		vk_inst.render_pass.bloom_extract = nullptr;
	}

	for (std::size_t i = 0; i < arrayLen(vk_inst.render_pass.blur); i++)
	{
		if (vk_inst.render_pass.blur[i])
		{
			vk_inst.device.destroyRenderPass(vk_inst.render_pass.blur[i]);
			vk_inst.render_pass.blur[i] = nullptr;
		}
	}

	// Destroy the post-bloom render pass
	if (vk_inst.render_pass.post_bloom)
	{
		vk_inst.device.destroyRenderPass(vk_inst.render_pass.post_bloom);
		vk_inst.render_pass.post_bloom = nullptr;
	}

	// Destroy the screenmap render pass
	if (vk_inst.render_pass.screenmap)
	{
		vk_inst.device.destroyRenderPass(vk_inst.render_pass.screenmap);
		vk_inst.render_pass.screenmap = nullptr;
	}

	// Destroy the gamma render pass
	if (vk_inst.render_pass.gamma)
	{
		vk_inst.device.destroyRenderPass(vk_inst.render_pass.gamma);
		vk_inst.render_pass.gamma = nullptr;
	}

	// Destroy the capture render pass
	if (vk_inst.render_pass.capture)
	{
		vk_inst.device.destroyRenderPass(vk_inst.render_pass.capture);
		vk_inst.render_pass.capture = nullptr;
	}
}

void vk_begin_screenmap_render_pass(void)
{
	vk::Framebuffer& frameBuffer = vk_inst.framebuffers.screenmap;

	vk_inst.renderPassIndex = renderPass_t::RENDER_PASS_SCREENMAP;

	vk_inst.renderWidth = vk_inst.screenMapWidth;
	vk_inst.renderHeight = vk_inst.screenMapHeight;

	vk_inst.renderScaleX = (float)vk_inst.renderWidth / (float)glConfig.vidWidth;
	vk_inst.renderScaleY = (float)vk_inst.renderHeight / (float)glConfig.vidHeight;

	vk_begin_render_pass(vk_inst.render_pass.screenmap, frameBuffer, true, vk_inst.renderWidth, vk_inst.renderHeight);
}
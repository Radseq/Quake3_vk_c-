#include "vk_pipeline.hpp"
#include "utils.hpp"

void vk_alloc_persistent_pipelines()
{
	unsigned int state_bits;
	Vk_Pipeline_Def def{};

	// skybox
	{
		def.shader_type = Vk_Shader_Type::TYPE_SIGNLE_TEXTURE_FIXED_COLOR;
		def.color.rgb = tr.identityLightByte;
		def.color.alpha = tr.identityLightByte;
		def.face_culling = cullType_t::CT_FRONT_SIDED;
		def.polygon_offset = false;
		def.mirror = false;
		vk_inst.skybox_pipeline = vk_find_pipeline_ext(0, def, true);
	}

	// stencil shadows
	{
		cullType_t cull_types[2] = { cullType_t::CT_FRONT_SIDED, cullType_t::CT_BACK_SIDED };
		bool mirror_flags[2] = { false, true };
		int i, j;

		def = {};
		def.polygon_offset = false;
		def.state_bits = 0;
		def.shader_type = Vk_Shader_Type::TYPE_SIGNLE_TEXTURE;
		def.shadow_phase = Vk_Shadow_Phase::SHADOW_EDGES;

		for (i = 0; i < 2; i++)
		{
			def.face_culling = cull_types[i];
			for (j = 0; j < 2; j++)
			{
				def.mirror = mirror_flags[j];
				vk_inst.shadow_volume_pipelines[i][j] = vk_find_pipeline_ext(0, def, r_shadows->integer ? true : false);
			}
		}
	}
	{
		def = {};
		def.face_culling = cullType_t::CT_FRONT_SIDED;
		def.polygon_offset = false;
		def.state_bits = GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO;
		def.shader_type = Vk_Shader_Type::TYPE_SIGNLE_TEXTURE;
		def.mirror = false;
		def.shadow_phase = Vk_Shadow_Phase::SHADOW_FS_QUAD;
		def.primitives = Vk_Primitive_Topology::TRIANGLE_STRIP;
		vk_inst.shadow_finish_pipeline = vk_find_pipeline_ext(0, def, r_shadows->integer ? true : false);
	}

	// fog and dlights
	{
		unsigned int fog_state_bits[2] = {
			GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_DEPTHFUNC_EQUAL, // fogPass == FP_EQUAL
			GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA						 // fogPass == FP_LE
		};
		unsigned int dlight_state_bits[2] = {
			GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL, // modulated
			GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL		 // additive
		};
		bool polygon_offset[2] = { false, true };
		int i, j, k;

#ifdef USE_PMLIGHT
		int l;
#endif

		def = {};
		def.shader_type = Vk_Shader_Type::TYPE_SIGNLE_TEXTURE;
		def.mirror = false;

		for (i = 0; i < 2; i++)
		{
			unsigned fog_state = fog_state_bits[i];
			unsigned dlight_state = dlight_state_bits[i];

			for (j = 0; j < 3; j++)
			{
				def.face_culling = static_cast<cullType_t>(j); // cullType_t value

				for (k = 0; k < 2; k++)
				{
					def.polygon_offset = polygon_offset[k];
#ifdef USE_FOG_ONLY
					def.shader_type = Vk_Shader_Type::TYPE_FOG_ONLY;
#else
					def.shader_type = Vk_Shader_Type::TYPE_SIGNLE_TEXTURE;
#endif
					def.state_bits = fog_state;
					vk_inst.fog_pipelines[i][j][k] = vk_find_pipeline_ext(0, def, true);

					def.shader_type = Vk_Shader_Type::TYPE_SIGNLE_TEXTURE;
					def.state_bits = dlight_state;
#ifdef USE_LEGACY_DLIGHTS
#ifdef USE_PMLIGHT
					vk_inst.dlight_pipelines[i][j][k] = vk_find_pipeline_ext(0, def, r_dlightMode->integer == 0 ? true : false);
#else
					vk_inst.dlight_pipelines[i][j][k] = vk_find_pipeline_ext(0, def, true);
#endif
#endif
				}
			}
		}

#ifdef USE_PMLIGHT
		def.state_bits = GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL;
		// def.shader_type = Vk_Shader_Type::TYPE_SIGNLE_TEXTURE_LIGHTING;
		for (i = 0; i < 3; i++)
		{ // cullType
			def.face_culling = static_cast<cullType_t>(i);
			for (j = 0; j < 2; j++)
			{ // polygonOffset
				def.polygon_offset = polygon_offset[j];
				for (k = 0; k < 2; k++)
				{
					def.fog_stage = k; // fogStage
					for (l = 0; l < 2; l++)
					{
						def.abs_light = l;
						def.shader_type = Vk_Shader_Type::TYPE_SIGNLE_TEXTURE_LIGHTING;
						vk_inst.dlight_pipelines_x[i][j][k][l] = vk_find_pipeline_ext(0, def, false);
						def.shader_type = Vk_Shader_Type::TYPE_SIGNLE_TEXTURE_LIGHTING_LINEAR;
						vk_inst.dlight1_pipelines_x[i][j][k][l] = vk_find_pipeline_ext(0, def, false);
					}
				}
			}
		}
#endif // USE_PMLIGHT
	}

	// RT_BEAM surface
	{
		def = {};
		def.state_bits = GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE;
		def.face_culling = cullType_t::CT_FRONT_SIDED;
		def.primitives = Vk_Primitive_Topology::TRIANGLE_STRIP;
		vk_inst.surface_beam_pipeline = vk_find_pipeline_ext(0, def, false);
	}

	// axis for missing models
	{
		def = {};
		def.state_bits = GLS_DEFAULT;
		def.shader_type = Vk_Shader_Type::TYPE_SIGNLE_TEXTURE;
		def.face_culling = cullType_t::CT_TWO_SIDED;
		def.primitives = Vk_Primitive_Topology::LINE_LIST;
		if (vk_inst.wideLines)
			def.line_width = 3;
		vk_inst.surface_axis_pipeline = vk_find_pipeline_ext(0, def, false);
	}

	// flare visibility test dot
	if (vk_inst.fragmentStores)
	{
		def = {};
		// def.state_bits = GLS_DEFAULT;
		def.face_culling = cullType_t::CT_TWO_SIDED;
		def.shader_type = Vk_Shader_Type::TYPE_DOT;
		def.primitives = Vk_Primitive_Topology::POINT_LIST;
		vk_inst.dot_pipeline = vk_find_pipeline_ext(0, def, true);
	}

	// DrawTris()
	state_bits = GLS_POLYMODE_LINE | GLS_DEPTHMASK_TRUE;
	{
		def = {};
		def.state_bits = state_bits;
		def.shader_type = Vk_Shader_Type::TYPE_COLOR_WHITE;
		def.face_culling = cullType_t::CT_FRONT_SIDED;
		vk_inst.tris_debug_pipeline = vk_find_pipeline_ext(0, def, false);
	}
	{
		def = {};
		def.state_bits = state_bits;
		def.shader_type = Vk_Shader_Type::TYPE_COLOR_WHITE;
		def.face_culling = cullType_t::CT_BACK_SIDED;
		vk_inst.tris_mirror_debug_pipeline = vk_find_pipeline_ext(0, def, false);
	}
	{
		def = {};
		def.state_bits = state_bits;
		def.shader_type = Vk_Shader_Type::TYPE_COLOR_GREEN;
		def.face_culling = cullType_t::CT_FRONT_SIDED;
		vk_inst.tris_debug_green_pipeline = vk_find_pipeline_ext(0, def, false);
	}
	{
		def = {};
		def.state_bits = state_bits;
		def.shader_type = Vk_Shader_Type::TYPE_COLOR_GREEN;
		def.face_culling = cullType_t::CT_BACK_SIDED;
		vk_inst.tris_mirror_debug_green_pipeline = vk_find_pipeline_ext(0, def, false);
	}
	{
		def = {};
		def.state_bits = state_bits;
		def.shader_type = Vk_Shader_Type::TYPE_COLOR_RED;
		def.face_culling = cullType_t::CT_FRONT_SIDED;
		vk_inst.tris_debug_red_pipeline = vk_find_pipeline_ext(0, def, false);
	}
	{
		def = {};
		def.state_bits = state_bits;
		def.shader_type = Vk_Shader_Type::TYPE_COLOR_RED;
		def.face_culling = cullType_t::CT_BACK_SIDED;
		vk_inst.tris_mirror_debug_red_pipeline = vk_find_pipeline_ext(0, def, false);
	}

	// DrawNormals()
	{
		def = {};
		def.state_bits = GLS_DEPTHMASK_TRUE;
		def.shader_type = Vk_Shader_Type::TYPE_SIGNLE_TEXTURE;
		def.primitives = Vk_Primitive_Topology::LINE_LIST;
		vk_inst.normals_debug_pipeline = vk_find_pipeline_ext(0, def, false);
	}

	// RB_DebugPolygon()
	{
		def = {};
		def.state_bits = GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE;
		def.shader_type = Vk_Shader_Type::TYPE_SIGNLE_TEXTURE;
		vk_inst.surface_debug_pipeline_solid = vk_find_pipeline_ext(0, def, false);
	}
	{
		def = {};
		def.state_bits = GLS_POLYMODE_LINE | GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE;
		def.shader_type = Vk_Shader_Type::TYPE_SIGNLE_TEXTURE;
		def.primitives = Vk_Primitive_Topology::LINE_LIST;
		vk_inst.surface_debug_pipeline_outline = vk_find_pipeline_ext(0, def, false);
	}

	// RB_ShowImages
	{
		def = {};
		def.state_bits = GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
		def.shader_type = Vk_Shader_Type::TYPE_SIGNLE_TEXTURE;
		def.primitives = Vk_Primitive_Topology::TRIANGLE_STRIP;
		vk_inst.images_debug_pipeline = vk_find_pipeline_ext(0, def, false);
		def.state_bits = GLS_DEPTHTEST_DISABLE;
		def.shader_type = Vk_Shader_Type::TYPE_COLOR_BLACK;
		def.primitives = Vk_Primitive_Topology::TRIANGLE_STRIP;
		vk_inst.images_debug_pipeline2 = vk_find_pipeline_ext(0, def, false);
	}
}

static uint32_t vk_alloc_pipeline(const Vk_Pipeline_Def& def)
{
	VK_Pipeline_t* pipeline;
	if (vk_inst.pipelines_count >= MAX_VK_PIPELINES)
	{
		ri.Error(ERR_DROP, "alloc_pipeline: MAX_VK_PIPELINES reached");
		return 0;
	}
	else
	{
		int j;
		pipeline = &vk_inst.pipelines[vk_inst.pipelines_count];
		pipeline->def = def;
		for (j = 0; j < RENDER_PASS_COUNT; j++)
		{
			pipeline->handle[j] = nullptr;
		}
		return vk_inst.pipelines_count++;
	}
}

uint32_t vk_find_pipeline_ext(const uint32_t base, const Vk_Pipeline_Def& def, bool use)
{
	const Vk_Pipeline_Def* cur_def;
	uint32_t index;

	for (index = base; index < vk_inst.pipelines_count; index++)
	{
		cur_def = &vk_inst.pipelines[index].def;
		if (memcmp(cur_def, &def, sizeof(def)) == 0)
		{
			goto found;
		}
	}

	index = vk_alloc_pipeline(def);
found:

	if (use)
		vk_gen_pipeline(index);

	return index;
}

static void set_shader_stage_desc(vk::PipelineShaderStageCreateInfo& desc, const vk::ShaderStageFlagBits stage, const vk::ShaderModule& shader_module, const char* entry)
{
	desc.pNext = nullptr;
	desc.flags = {};
	desc.stage = stage;
	desc.module = shader_module;
	desc.pName = entry;
	desc.pSpecializationInfo = nullptr;
}

inline constexpr std::array<vk::SpecializationMapEntry, 11> kFragSpecEntries = { {
	{ 0, offsetof(FragSpec, alphaFunc),        sizeof(int)   },
	{ 1, offsetof(FragSpec, alphaRef),         sizeof(float) },
	{ 2, offsetof(FragSpec, depthFrag),        sizeof(float) },
	{ 3, offsetof(FragSpec, alphaToCoverage),  sizeof(int)   },
	{ 4, offsetof(FragSpec, colorMode),        sizeof(int)   },
	{ 5, offsetof(FragSpec, absLight),         sizeof(int)   },
	{ 6, offsetof(FragSpec, multiTexMode),     sizeof(int)   },
	{ 7, offsetof(FragSpec, discardMode),      sizeof(int)   },
	{ 8, offsetof(FragSpec, fixedColor),       sizeof(float) },
	{ 9, offsetof(FragSpec, fixedAlpha),       sizeof(float) },
	{10, offsetof(FragSpec, acff),             sizeof(int)   },
} };

static void GetCullModeByFaceCulling(const Vk_Pipeline_Def& def, vk::CullModeFlags& cullMode)
{
	switch (def.face_culling)
	{
	case cullType_t::CT_TWO_SIDED:
		cullMode = vk::CullModeFlagBits::eNone;
		break;
	case cullType_t::CT_FRONT_SIDED:
		cullMode = (def.mirror ? vk::CullModeFlagBits::eFront : vk::CullModeFlagBits::eBack);
		break;
	case cullType_t::CT_BACK_SIDED:
		cullMode = (def.mirror ? vk::CullModeFlagBits::eBack : vk::CullModeFlagBits::eFront);
		break;
	default:
		ri.Error(ERR_DROP, "create_pipeline: invalid face culling mode %i\n", static_cast<int>(def.face_culling));
		break;
	}
}

static vk::VertexInputBindingDescription bindingsCpp[8];
static vk::VertexInputAttributeDescription attribsCpp[8];
static uint32_t num_binds;
static uint32_t num_attrs;

static void push_bind(const uint32_t binding, const uint32_t stride)
{
	bindingsCpp[num_binds].binding = binding;
	bindingsCpp[num_binds].stride = stride;
	bindingsCpp[num_binds].inputRate = vk::VertexInputRate::eVertex;
	num_binds++;
}

static void push_attr(const uint32_t location, const uint32_t binding, const vk::Format format)
{
	attribsCpp[num_attrs].location = location;
	attribsCpp[num_attrs].binding = binding;
	attribsCpp[num_attrs].format = format;
	attribsCpp[num_attrs].offset = 0;
	num_attrs++;
}

constexpr vk::PrimitiveTopology kTopo[] = {
	vk::PrimitiveTopology::eTriangleList,   // default
	vk::PrimitiveTopology::eLineList,       // LINE_LIST
	vk::PrimitiveTopology::ePointList,      // POINT_LIST
	vk::PrimitiveTopology::eTriangleStrip,  // TRIANGLE_STRIP
};

inline static vk::PrimitiveTopology GetTopologyByPrimitivies(Vk_Primitive_Topology p) {
	switch (p) {
	case Vk_Primitive_Topology::LINE_LIST: return kTopo[1];
	case Vk_Primitive_Topology::POINT_LIST: return kTopo[2];
	case Vk_Primitive_Topology::TRIANGLE_STRIP: return kTopo[3];
	default: return kTopo[0];
	}
}

static vk::PipelineColorBlendAttachmentState createBlendAttachmentState(const uint32_t state_bits, const Vk_Pipeline_Def& def)
{
	// Determine whether blending is enabled
	bool blendEnable = (state_bits & (GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS)) ? vk::True : vk::False;

	// Initialize colorWriteMask based on shader phase/type
	vk::ColorComponentFlags colorWriteMask =
		(def.shadow_phase == Vk_Shadow_Phase::SHADOW_EDGES || def.shader_type == Vk_Shader_Type::TYPE_SIGNLE_TEXTURE_DF) ? vk::ColorComponentFlags(0) : vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;

	vk::BlendFactor srcColorBlendFactor = {};
	vk::BlendFactor dstColorBlendFactor = {};

	if (blendEnable)
	{
		switch (state_bits & GLS_SRCBLEND_BITS)
		{
		case GLS_SRCBLEND_ZERO:
			srcColorBlendFactor = vk::BlendFactor::eZero;
			break;
		case GLS_SRCBLEND_ONE:
			srcColorBlendFactor = vk::BlendFactor::eOne;
			break;
		case GLS_SRCBLEND_DST_COLOR:
			srcColorBlendFactor = vk::BlendFactor::eDstColor;
			break;
		case GLS_SRCBLEND_ONE_MINUS_DST_COLOR:
			srcColorBlendFactor = vk::BlendFactor::eOneMinusDstColor;
			break;
		case GLS_SRCBLEND_SRC_ALPHA:
			srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
			break;
		case GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA:
			srcColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
			break;
		case GLS_SRCBLEND_DST_ALPHA:
			srcColorBlendFactor = vk::BlendFactor::eDstAlpha;
			break;
		case GLS_SRCBLEND_ONE_MINUS_DST_ALPHA:
			srcColorBlendFactor = vk::BlendFactor::eOneMinusDstAlpha;
			break;
		case GLS_SRCBLEND_ALPHA_SATURATE:
			srcColorBlendFactor = vk::BlendFactor::eSrcAlphaSaturate;
			break;
		default:
			ri.Error(ERR_DROP, "create_pipeline: invalid src blend state bits\n");
			break;
		}

		switch (state_bits & GLS_DSTBLEND_BITS)
		{
		case GLS_DSTBLEND_ZERO:
			dstColorBlendFactor = vk::BlendFactor::eZero;
			break;
		case GLS_DSTBLEND_ONE:
			dstColorBlendFactor = vk::BlendFactor::eOne;
			break;
		case GLS_DSTBLEND_SRC_COLOR:
			dstColorBlendFactor = vk::BlendFactor::eSrcColor;
			break;
		case GLS_DSTBLEND_ONE_MINUS_SRC_COLOR:
			dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcColor;
			break;
		case GLS_DSTBLEND_SRC_ALPHA:
			dstColorBlendFactor = vk::BlendFactor::eSrcAlpha;
			break;
		case GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA:
			dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
			break;
		case GLS_DSTBLEND_DST_ALPHA:
			dstColorBlendFactor = vk::BlendFactor::eDstAlpha;
			break;
		case GLS_DSTBLEND_ONE_MINUS_DST_ALPHA:
			dstColorBlendFactor = vk::BlendFactor::eOneMinusDstAlpha;
			break;
		default:
			ri.Error(ERR_DROP, "create_pipeline: invalid dst blend state bits\n");
			break;
		}
	}

	// Create the blend attachment state
	vk::PipelineColorBlendAttachmentState attachmentBlendState{
		blendEnable,
		srcColorBlendFactor,
		dstColorBlendFactor,
		vk::BlendOp::eAdd,
		srcColorBlendFactor,
		dstColorBlendFactor,
		vk::BlendOp::eAdd,
		colorWriteMask,
	};

	return attachmentBlendState;
}

vk::Pipeline create_pipeline(const Vk_Pipeline_Def& def, const renderPass_t renderPassIndex, uint32_t def_index)
{
	vk::ShaderModule* vs_module = nullptr;
	vk::ShaderModule* fs_module = nullptr;
	// int32_t vert_spec_data[1]; // clippping

	FragSpec fragSpec = {};

	vk::DynamicState dynamic_state_array[] = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };

	vk::Pipeline pipeline;
	vk::Bool32 alphaToCoverage = vk::False;
	unsigned int atest_bits;
	unsigned int state_bits = def.state_bits;

	switch (def.shader_type)
	{

	case Vk_Shader_Type::TYPE_SIGNLE_TEXTURE_LIGHTING:
		vs_module = &vk_inst.modules.vert.light[0];
		fs_module = &vk_inst.modules.frag.light[0][0];
		break;

	case Vk_Shader_Type::TYPE_SIGNLE_TEXTURE_LIGHTING_LINEAR:
		vs_module = &vk_inst.modules.vert.light[0];
		fs_module = &vk_inst.modules.frag.light[1][0];
		break;

	case Vk_Shader_Type::TYPE_SIGNLE_TEXTURE_DF:
		state_bits |= GLS_DEPTHMASK_TRUE;
		vs_module = &vk_inst.modules.vert.ident1[0][0][0];
		fs_module = &vk_inst.modules.frag.gen0_df;
		break;

	case Vk_Shader_Type::TYPE_SIGNLE_TEXTURE_FIXED_COLOR:
		vs_module = &vk_inst.modules.vert.fixed[0][0][0];
		fs_module = &vk_inst.modules.frag.fixed[0][0];
		break;

	case Vk_Shader_Type::TYPE_SIGNLE_TEXTURE_FIXED_COLOR_ENV:
		vs_module = &vk_inst.modules.vert.fixed[0][1][0];
		fs_module = &vk_inst.modules.frag.fixed[0][0];
		break;

	case Vk_Shader_Type::TYPE_SIGNLE_TEXTURE_ENT_COLOR:
		vs_module = &vk_inst.modules.vert.fixed[0][0][0];
		fs_module = &vk_inst.modules.frag.ent[0][0];
		break;

	case Vk_Shader_Type::TYPE_SIGNLE_TEXTURE_ENT_COLOR_ENV:
		vs_module = &vk_inst.modules.vert.fixed[0][1][0];
		fs_module = &vk_inst.modules.frag.ent[0][0];
		break;

	case Vk_Shader_Type::TYPE_SIGNLE_TEXTURE:
		vs_module = &vk_inst.modules.vert.gen[0][0][0][0];
		fs_module = &vk_inst.modules.frag.gen[0][0][0];
		break;

	case Vk_Shader_Type::TYPE_SIGNLE_TEXTURE_ENV:
		vs_module = &vk_inst.modules.vert.gen[0][0][1][0];
		fs_module = &vk_inst.modules.frag.gen[0][0][0];
		break;

	case Vk_Shader_Type::TYPE_SIGNLE_TEXTURE_IDENTITY:
		vs_module = &vk_inst.modules.vert.ident1[0][0][0];
		fs_module = &vk_inst.modules.frag.ident1[0][0];
		break;

	case Vk_Shader_Type::TYPE_SIGNLE_TEXTURE_IDENTITY_ENV:
		vs_module = &vk_inst.modules.vert.ident1[0][1][0];
		fs_module = &vk_inst.modules.frag.ident1[0][0];
		break;

	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD2_IDENTITY:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_MUL2_IDENTITY:
		vs_module = &vk_inst.modules.vert.ident1[1][0][0];
		fs_module = &vk_inst.modules.frag.ident1[1][0];
		break;

	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD2_IDENTITY_ENV:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_MUL2_IDENTITY_ENV:
		vs_module = &vk_inst.modules.vert.ident1[1][1][0];
		fs_module = &vk_inst.modules.frag.ident1[1][0];
		break;

	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR:
		vs_module = &vk_inst.modules.vert.fixed[1][0][0];
		fs_module = &vk_inst.modules.frag.fixed[1][0];
		break;

	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR_ENV:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR_ENV:
		vs_module = &vk_inst.modules.vert.fixed[1][1][0];
		fs_module = &vk_inst.modules.frag.fixed[1][0];
		break;

	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_MUL2:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD2_1_1:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD2:
		vs_module = &vk_inst.modules.vert.gen[1][0][0][0];
		fs_module = &vk_inst.modules.frag.gen[1][0][0];
		break;

	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_MUL2_ENV:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD2_1_1_ENV:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD2_ENV:
		vs_module = &vk_inst.modules.vert.gen[1][0][1][0];
		fs_module = &vk_inst.modules.frag.gen[1][0][0];
		break;

	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_MUL3:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD3_1_1:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD3:
		vs_module = &vk_inst.modules.vert.gen[2][0][0][0];
		fs_module = &vk_inst.modules.frag.gen[2][0][0];
		break;

	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_MUL3_ENV:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD3_1_1_ENV:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD3_ENV:
		vs_module = &vk_inst.modules.vert.gen[2][0][1][0];
		fs_module = &vk_inst.modules.frag.gen[2][0][0];
		break;

	case Vk_Shader_Type::TYPE_BLEND2_ADD:
	case Vk_Shader_Type::TYPE_BLEND2_MUL:
	case Vk_Shader_Type::TYPE_BLEND2_ALPHA:
	case Vk_Shader_Type::TYPE_BLEND2_ONE_MINUS_ALPHA:
	case Vk_Shader_Type::TYPE_BLEND2_MIX_ALPHA:
	case Vk_Shader_Type::TYPE_BLEND2_MIX_ONE_MINUS_ALPHA:
	case Vk_Shader_Type::TYPE_BLEND2_DST_COLOR_SRC_ALPHA:
		vs_module = &vk_inst.modules.vert.gen[1][1][0][0];
		fs_module = &vk_inst.modules.frag.gen[1][1][0];
		break;

	case Vk_Shader_Type::TYPE_BLEND2_ADD_ENV:
	case Vk_Shader_Type::TYPE_BLEND2_MUL_ENV:
	case Vk_Shader_Type::TYPE_BLEND2_ALPHA_ENV:
	case Vk_Shader_Type::TYPE_BLEND2_ONE_MINUS_ALPHA_ENV:
	case Vk_Shader_Type::TYPE_BLEND2_MIX_ALPHA_ENV:
	case Vk_Shader_Type::TYPE_BLEND2_MIX_ONE_MINUS_ALPHA_ENV:
	case Vk_Shader_Type::TYPE_BLEND2_DST_COLOR_SRC_ALPHA_ENV:
		vs_module = &vk_inst.modules.vert.gen[1][1][1][0];
		fs_module = &vk_inst.modules.frag.gen[1][1][0];
		break;

	case Vk_Shader_Type::TYPE_BLEND3_ADD:
	case Vk_Shader_Type::TYPE_BLEND3_MUL:
	case Vk_Shader_Type::TYPE_BLEND3_ALPHA:
	case Vk_Shader_Type::TYPE_BLEND3_ONE_MINUS_ALPHA:
	case Vk_Shader_Type::TYPE_BLEND3_MIX_ALPHA:
	case Vk_Shader_Type::TYPE_BLEND3_MIX_ONE_MINUS_ALPHA:
	case Vk_Shader_Type::TYPE_BLEND3_DST_COLOR_SRC_ALPHA:
		vs_module = &vk_inst.modules.vert.gen[2][1][0][0];
		fs_module = &vk_inst.modules.frag.gen[2][1][0];
		break;

	case Vk_Shader_Type::TYPE_BLEND3_ADD_ENV:
	case Vk_Shader_Type::TYPE_BLEND3_MUL_ENV:
	case Vk_Shader_Type::TYPE_BLEND3_ALPHA_ENV:
	case Vk_Shader_Type::TYPE_BLEND3_ONE_MINUS_ALPHA_ENV:
	case Vk_Shader_Type::TYPE_BLEND3_MIX_ALPHA_ENV:
	case Vk_Shader_Type::TYPE_BLEND3_MIX_ONE_MINUS_ALPHA_ENV:
	case Vk_Shader_Type::TYPE_BLEND3_DST_COLOR_SRC_ALPHA_ENV:
		vs_module = &vk_inst.modules.vert.gen[2][1][1][0];
		fs_module = &vk_inst.modules.frag.gen[2][1][0];
		break;

	case Vk_Shader_Type::TYPE_COLOR_BLACK:
	case Vk_Shader_Type::TYPE_COLOR_WHITE:
	case Vk_Shader_Type::TYPE_COLOR_GREEN:
	case Vk_Shader_Type::TYPE_COLOR_RED:
		vs_module = &vk_inst.modules.color_vs;
		fs_module = &vk_inst.modules.color_fs;
		break;

	case Vk_Shader_Type::TYPE_FOG_ONLY:
		vs_module = &vk_inst.modules.fog_vs;
		fs_module = &vk_inst.modules.fog_fs;
		break;

	case Vk_Shader_Type::TYPE_DOT:
		vs_module = &vk_inst.modules.dot_vs;
		fs_module = &vk_inst.modules.dot_fs;
		break;

	default:
		ri.Error(ERR_DROP, "create_pipeline_plus: unknown shader type %i\n", static_cast<int>(def.shader_type));
		return nullptr;
	}

	if (def.fog_stage)
	{
		switch (def.shader_type)
		{
		case Vk_Shader_Type::TYPE_FOG_ONLY:
		case Vk_Shader_Type::TYPE_DOT:
		case Vk_Shader_Type::TYPE_SIGNLE_TEXTURE_DF:
		case Vk_Shader_Type::TYPE_COLOR_BLACK:
		case Vk_Shader_Type::TYPE_COLOR_WHITE:
		case Vk_Shader_Type::TYPE_COLOR_GREEN:
		case Vk_Shader_Type::TYPE_COLOR_RED:
			break;
		default:
			// switch to fogged modules
			vs_module++;
			fs_module++;
			break;
		}
	}

	std::array<vk::PipelineShaderStageCreateInfo, 2> shader_stages;
	set_shader_stage_desc(shader_stages[0], vk::ShaderStageFlagBits::eVertex, *vs_module, "main");
	set_shader_stage_desc(shader_stages[1], vk::ShaderStageFlagBits::eFragment, *fs_module, "main");

	// Com_Memset( vert_spec_data, 0, sizeof( vert_spec_data ) );

	// vert_spec_data[0] = def.clipping_plane ? 1 : 0;

	// fragment shader specialization data
	atest_bits = state_bits & GLS_ATEST_BITS;
	switch (atest_bits)
	{
	case GLS_ATEST_GT_0:
		fragSpec.alphaFunc = 1; // not equal
		fragSpec.alphaRef = 0.0f;
		break;
	case GLS_ATEST_LT_80:
		fragSpec.alphaFunc = 2; // less than
		fragSpec.alphaRef = 0.5f;
		break;
	case GLS_ATEST_GE_80:
		fragSpec.alphaFunc = 3; // greater or equal
		fragSpec.alphaRef = 0.5f;
		break;
	default:
		fragSpec.alphaFunc = 0;
		fragSpec.alphaRef = 0.0f;
		break;
	};

	// depth fragment threshold
	fragSpec.depthFrag = 0.85f;

#if 0
	if (r_ext_alpha_to_coverage->integer && vkSamples != vk::SampleCountFlagBits::e1 && frag_spec_data[0].i) {
		frag_spec_data[3].i = 1;
		alphaToCoverage = vk::True;
	}
#endif

	// constant color
	switch (def.shader_type)
	{
	default:
		fragSpec.colorMode = 0;
		break;
	case Vk_Shader_Type::TYPE_COLOR_WHITE:
		fragSpec.colorMode = 1;
		break;
	case Vk_Shader_Type::TYPE_COLOR_GREEN:
		fragSpec.colorMode = 2;
		break;
	case Vk_Shader_Type::TYPE_COLOR_RED:
		fragSpec.colorMode = 3;
		break;
	}

	// abs lighting
	switch (def.shader_type)
	{
	case Vk_Shader_Type::TYPE_SIGNLE_TEXTURE_LIGHTING:
	case Vk_Shader_Type::TYPE_SIGNLE_TEXTURE_LIGHTING_LINEAR:
		fragSpec.absLight = def.abs_light ? 1 : 0;
	default:
		break;
	}

	// multutexture mode
	switch (def.shader_type)
	{
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_MUL2_IDENTITY:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_MUL2_IDENTITY_ENV:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR_ENV:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_MUL2:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_MUL2_ENV:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_MUL3:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_MUL3_ENV:
	case Vk_Shader_Type::TYPE_BLEND2_MUL:
	case Vk_Shader_Type::TYPE_BLEND2_MUL_ENV:
	case Vk_Shader_Type::TYPE_BLEND3_MUL:
	case Vk_Shader_Type::TYPE_BLEND3_MUL_ENV:
		fragSpec.multiTexMode = 0;
		break;

	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD2_IDENTITY:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD2_IDENTITY_ENV:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR_ENV:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD2_1_1:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD2_1_1_ENV:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD3_1_1:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD3_1_1_ENV:
		fragSpec.multiTexMode = 1;
		break;

	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD2:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD2_ENV:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD3:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD3_ENV:
	case Vk_Shader_Type::TYPE_BLEND2_ADD:
	case Vk_Shader_Type::TYPE_BLEND2_ADD_ENV:
	case Vk_Shader_Type::TYPE_BLEND3_ADD:
	case Vk_Shader_Type::TYPE_BLEND3_ADD_ENV:
		fragSpec.multiTexMode = 2;
		break;

	case Vk_Shader_Type::TYPE_BLEND2_ALPHA:
	case Vk_Shader_Type::TYPE_BLEND2_ALPHA_ENV:
	case Vk_Shader_Type::TYPE_BLEND3_ALPHA:
	case Vk_Shader_Type::TYPE_BLEND3_ALPHA_ENV:
		fragSpec.multiTexMode = 3;
		break;

	case Vk_Shader_Type::TYPE_BLEND2_ONE_MINUS_ALPHA:
	case Vk_Shader_Type::TYPE_BLEND2_ONE_MINUS_ALPHA_ENV:
	case Vk_Shader_Type::TYPE_BLEND3_ONE_MINUS_ALPHA:
	case Vk_Shader_Type::TYPE_BLEND3_ONE_MINUS_ALPHA_ENV:
		fragSpec.multiTexMode = 4;
		break;

	case Vk_Shader_Type::TYPE_BLEND2_MIX_ALPHA:
	case Vk_Shader_Type::TYPE_BLEND2_MIX_ALPHA_ENV:
	case Vk_Shader_Type::TYPE_BLEND3_MIX_ALPHA:
	case Vk_Shader_Type::TYPE_BLEND3_MIX_ALPHA_ENV:
		fragSpec.multiTexMode = 5;
		break;

	case Vk_Shader_Type::TYPE_BLEND2_MIX_ONE_MINUS_ALPHA:
	case Vk_Shader_Type::TYPE_BLEND2_MIX_ONE_MINUS_ALPHA_ENV:
	case Vk_Shader_Type::TYPE_BLEND3_MIX_ONE_MINUS_ALPHA:
	case Vk_Shader_Type::TYPE_BLEND3_MIX_ONE_MINUS_ALPHA_ENV:
		fragSpec.multiTexMode = 6;
		break;

	case Vk_Shader_Type::TYPE_BLEND2_DST_COLOR_SRC_ALPHA:
	case Vk_Shader_Type::TYPE_BLEND2_DST_COLOR_SRC_ALPHA_ENV:
	case Vk_Shader_Type::TYPE_BLEND3_DST_COLOR_SRC_ALPHA:
	case Vk_Shader_Type::TYPE_BLEND3_DST_COLOR_SRC_ALPHA_ENV:
		fragSpec.multiTexMode = 7;
		break;

	default:
		break;
	}

	fragSpec.fixedColor = ((float)def.color.rgb) / 255.0;
	fragSpec.fixedAlpha = ((float)def.color.alpha) / 255.0;

	if (def.fog_stage)
	{
		fragSpec.acff = def.acff;
	}
	else
	{
		fragSpec.acff = 0;
	}

	//
	// vertex module specialization data
	//
#if 0
	spec_entries[0].constantID = 0; // clip_plane
	spec_entries[0].offset = 0 * sizeof(int32_t);
	spec_entries[0].size = sizeof(int32_t);

	vert_spec_info.mapEntryCount = 1;
	vert_spec_info.pMapEntries = spec_entries + 0;
	vert_spec_info.dataSize = 1 * sizeof(int32_t);
	vert_spec_info.pData = &vert_spec_data[0];
	shader_stages[0].pSpecializationInfo = &vert_spec_info;
#endif
	shader_stages[0].pSpecializationInfo = nullptr;

	//
	// fragment module specialization data
	//

	vk::SpecializationInfo frag_spec_info{ static_cast<uint32_t>(kFragSpecEntries.size()),
										  kFragSpecEntries.data(),
										  sizeof(FragSpec),
										  &fragSpec };

	shader_stages[1].pSpecializationInfo = &frag_spec_info;

	//
	// Vertex input
	//
	num_binds = num_attrs = 0;
	switch (def.shader_type)
	{

	case Vk_Shader_Type::TYPE_FOG_ONLY:
	case Vk_Shader_Type::TYPE_DOT:
		push_bind(0, sizeof(vec4_t)); // xyz array
		push_attr(0, 0, vk::Format::eR32G32B32A32Sfloat);
		break;

	case Vk_Shader_Type::TYPE_COLOR_BLACK:
	case Vk_Shader_Type::TYPE_COLOR_WHITE:
	case Vk_Shader_Type::TYPE_COLOR_GREEN:
	case Vk_Shader_Type::TYPE_COLOR_RED:
		push_bind(0, sizeof(vec4_t)); // xyz array
		push_attr(0, 0, vk::Format::eR32G32B32A32Sfloat);
		break;

	case Vk_Shader_Type::TYPE_SIGNLE_TEXTURE_DF:
	case Vk_Shader_Type::TYPE_SIGNLE_TEXTURE_IDENTITY:
	case Vk_Shader_Type::TYPE_SIGNLE_TEXTURE_FIXED_COLOR:
	case Vk_Shader_Type::TYPE_SIGNLE_TEXTURE_ENT_COLOR:
		push_bind(0, sizeof(vec4_t)); // xyz array
		push_bind(2, sizeof(vec2_t)); // st0 array
		push_attr(0, 0, vk::Format::eR32G32B32A32Sfloat);
		push_attr(2, 2, vk::Format::eR32G32Sfloat);
		break;

	case Vk_Shader_Type::TYPE_SIGNLE_TEXTURE:
		push_bind(0, sizeof(vec4_t));	  // xyz array
		push_bind(1, sizeof(color4ub_t)); // color array
		push_bind(2, sizeof(vec2_t));	  // st0 array
		push_attr(0, 0, vk::Format::eR32G32B32A32Sfloat);
		push_attr(1, 1, vk::Format::eR8G8B8A8Unorm);
		push_attr(2, 2, vk::Format::eR32G32Sfloat);
		break;

	case Vk_Shader_Type::TYPE_SIGNLE_TEXTURE_ENV:
		push_bind(0, sizeof(vec4_t));	  // xyz array
		push_bind(1, sizeof(color4ub_t)); // color array
		// push_bind( 2, sizeof( vec2_t ) );					// st0 array
		push_bind(5, sizeof(vec4_t)); // normals
		push_attr(0, 0, vk::Format::eR32G32B32A32Sfloat);
		push_attr(1, 1, vk::Format::eR8G8B8A8Unorm);
		// push_attr( 2, 2, vk::Format::eR8G8B8A8Unorm );
		push_attr(5, 5, vk::Format::eR32G32B32A32Sfloat);
		break;

	case Vk_Shader_Type::TYPE_SIGNLE_TEXTURE_IDENTITY_ENV:
	case Vk_Shader_Type::TYPE_SIGNLE_TEXTURE_FIXED_COLOR_ENV:
	case Vk_Shader_Type::TYPE_SIGNLE_TEXTURE_ENT_COLOR_ENV:
		push_bind(0, sizeof(vec4_t)); // xyz array
		push_bind(5, sizeof(vec4_t)); // normals
		push_attr(0, 0, vk::Format::eR32G32B32A32Sfloat);
		push_attr(5, 5, vk::Format::eR32G32B32A32Sfloat);
		break;

	case Vk_Shader_Type::TYPE_SIGNLE_TEXTURE_LIGHTING:
	case Vk_Shader_Type::TYPE_SIGNLE_TEXTURE_LIGHTING_LINEAR:
		push_bind(0, sizeof(vec4_t)); // xyz array
		push_bind(1, sizeof(vec2_t)); // st0 array
		push_bind(2, sizeof(vec4_t)); // normals array
		push_attr(0, 0, vk::Format::eR32G32B32A32Sfloat);
		push_attr(1, 1, vk::Format::eR32G32Sfloat);
		push_attr(2, 2, vk::Format::eR32G32B32A32Sfloat);
		break;

	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_MUL2_IDENTITY:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD2_IDENTITY:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR:
		push_bind(0, sizeof(vec4_t)); // xyz array
		push_bind(2, sizeof(vec2_t)); // st0 array
		push_bind(3, sizeof(vec2_t)); // st1 array
		push_attr(0, 0, vk::Format::eR32G32B32A32Sfloat);
		push_attr(2, 2, vk::Format::eR32G32Sfloat);
		push_attr(3, 3, vk::Format::eR32G32Sfloat);
		break;

	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_MUL2_IDENTITY_ENV:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD2_IDENTITY_ENV:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR_ENV:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR_ENV:
		push_bind(0, sizeof(vec4_t)); // xyz array
		push_bind(3, sizeof(vec2_t)); // st1 array
		push_bind(5, sizeof(vec4_t)); // normals
		push_attr(0, 0, vk::Format::eR32G32B32A32Sfloat);
		push_attr(3, 3, vk::Format::eR32G32Sfloat);
		push_attr(5, 5, vk::Format::eR32G32B32A32Sfloat);
		break;

	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_MUL2:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD2_1_1:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD2:
		push_bind(0, sizeof(vec4_t));	  // xyz array
		push_bind(1, sizeof(color4ub_t)); // color array
		push_bind(2, sizeof(vec2_t));	  // st0 array
		push_bind(3, sizeof(vec2_t));	  // st1 array
		push_attr(0, 0, vk::Format::eR32G32B32A32Sfloat);
		push_attr(1, 1, vk::Format::eR8G8B8A8Unorm);
		push_attr(2, 2, vk::Format::eR32G32Sfloat);
		push_attr(3, 3, vk::Format::eR32G32Sfloat);
		break;

	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_MUL2_ENV:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD2_1_1_ENV:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD2_ENV:
		push_bind(0, sizeof(vec4_t));	  // xyz array
		push_bind(1, sizeof(color4ub_t)); // color array
		// push_bind( 2, sizeof( vec2_t ) );					// st0 array
		push_bind(3, sizeof(vec2_t)); // st1 array
		push_bind(5, sizeof(vec4_t)); // normals
		push_attr(0, 0, vk::Format::eR32G32B32A32Sfloat);
		push_attr(1, 1, vk::Format::eR8G8B8A8Unorm);
		// push_attr( 2, 2, vk::Format::eR32G32Sfloat );
		push_attr(3, 3, vk::Format::eR32G32Sfloat);
		push_attr(5, 5, vk::Format::eR32G32B32A32Sfloat);
		break;

	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_MUL3:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD3_1_1:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD3:
		push_bind(0, sizeof(vec4_t));	  // xyz array
		push_bind(1, sizeof(color4ub_t)); // color array
		push_bind(2, sizeof(vec2_t));	  // st0 array
		push_bind(3, sizeof(vec2_t));	  // st1 array
		push_bind(4, sizeof(vec2_t));	  // st2 array
		push_attr(0, 0, vk::Format::eR32G32B32A32Sfloat);
		push_attr(1, 1, vk::Format::eR8G8B8A8Unorm);
		push_attr(2, 2, vk::Format::eR32G32Sfloat);
		push_attr(3, 3, vk::Format::eR32G32Sfloat);
		push_attr(4, 4, vk::Format::eR32G32Sfloat);
		break;

	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_MUL3_ENV:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD3_1_1_ENV:
	case Vk_Shader_Type::TYPE_MULTI_TEXTURE_ADD3_ENV:
		push_bind(0, sizeof(vec4_t));	  // xyz array
		push_bind(1, sizeof(color4ub_t)); // color array
		// push_bind( 2, sizeof( vec2_t ) );					// st0 array
		push_bind(3, sizeof(vec2_t)); // st1 array
		push_bind(4, sizeof(vec2_t)); // st2 array
		push_bind(5, sizeof(vec4_t)); // normals
		push_attr(0, 0, vk::Format::eR32G32B32A32Sfloat);
		push_attr(1, 1, vk::Format::eR8G8B8A8Unorm);
		// push_attr( 2, 2, vk::Format::eR32G32Sfloat );
		push_attr(3, 3, vk::Format::eR32G32Sfloat);
		push_attr(4, 4, vk::Format::eR32G32Sfloat);
		push_attr(5, 5, vk::Format::eR32G32B32A32Sfloat);
		break;

	case Vk_Shader_Type::TYPE_BLEND2_ADD:
	case Vk_Shader_Type::TYPE_BLEND2_MUL:
	case Vk_Shader_Type::TYPE_BLEND2_ALPHA:
	case Vk_Shader_Type::TYPE_BLEND2_ONE_MINUS_ALPHA:
	case Vk_Shader_Type::TYPE_BLEND2_MIX_ALPHA:
	case Vk_Shader_Type::TYPE_BLEND2_MIX_ONE_MINUS_ALPHA:
	case Vk_Shader_Type::TYPE_BLEND2_DST_COLOR_SRC_ALPHA:
		push_bind(0, sizeof(vec4_t));	  // xyz array
		push_bind(1, sizeof(color4ub_t)); // color0 array
		push_bind(2, sizeof(vec2_t));	  // st0 array
		push_bind(3, sizeof(vec2_t));	  // st1 array
		push_bind(6, sizeof(color4ub_t)); // color1 array
		push_attr(0, 0, vk::Format::eR32G32B32A32Sfloat);
		push_attr(1, 1, vk::Format::eR8G8B8A8Unorm);
		push_attr(2, 2, vk::Format::eR32G32Sfloat);
		push_attr(3, 3, vk::Format::eR32G32Sfloat);
		push_attr(6, 6, vk::Format::eR8G8B8A8Unorm);
		break;

	case Vk_Shader_Type::TYPE_BLEND2_ADD_ENV:
	case Vk_Shader_Type::TYPE_BLEND2_MUL_ENV:
	case Vk_Shader_Type::TYPE_BLEND2_ALPHA_ENV:
	case Vk_Shader_Type::TYPE_BLEND2_ONE_MINUS_ALPHA_ENV:
	case Vk_Shader_Type::TYPE_BLEND2_MIX_ALPHA_ENV:
	case Vk_Shader_Type::TYPE_BLEND2_MIX_ONE_MINUS_ALPHA_ENV:
	case Vk_Shader_Type::TYPE_BLEND2_DST_COLOR_SRC_ALPHA_ENV:
		push_bind(0, sizeof(vec4_t));	  // xyz array
		push_bind(1, sizeof(color4ub_t)); // color0 array
		// push_bind( 2, sizeof( vec2_t ) );					// st0 array
		push_bind(3, sizeof(vec2_t));	  // st1 array
		push_bind(5, sizeof(vec4_t));	  // normals
		push_bind(6, sizeof(color4ub_t)); // color1 array
		push_attr(0, 0, vk::Format::eR32G32B32A32Sfloat);
		push_attr(1, 1, vk::Format::eR8G8B8A8Unorm);
		// push_attr( 2, 2, vk::Format::eR32G32Sfloat );
		push_attr(3, 3, vk::Format::eR32G32Sfloat);
		push_attr(5, 5, vk::Format::eR32G32B32A32Sfloat);
		push_attr(6, 6, vk::Format::eR8G8B8A8Unorm);
		break;

	case Vk_Shader_Type::TYPE_BLEND3_ADD:
	case Vk_Shader_Type::TYPE_BLEND3_MUL:
	case Vk_Shader_Type::TYPE_BLEND3_ALPHA:
	case Vk_Shader_Type::TYPE_BLEND3_ONE_MINUS_ALPHA:
	case Vk_Shader_Type::TYPE_BLEND3_MIX_ALPHA:
	case Vk_Shader_Type::TYPE_BLEND3_MIX_ONE_MINUS_ALPHA:
	case Vk_Shader_Type::TYPE_BLEND3_DST_COLOR_SRC_ALPHA:
		push_bind(0, sizeof(vec4_t));	  // xyz array
		push_bind(1, sizeof(color4ub_t)); // color0 array
		push_bind(2, sizeof(vec2_t));	  // st0 array
		push_bind(3, sizeof(vec2_t));	  // st1 array
		push_bind(4, sizeof(vec2_t));	  // st2 array
		push_bind(6, sizeof(color4ub_t)); // color1 array
		push_bind(7, sizeof(color4ub_t)); // color2 array
		push_attr(0, 0, vk::Format::eR32G32B32A32Sfloat);
		push_attr(1, 1, vk::Format::eR8G8B8A8Unorm);
		push_attr(2, 2, vk::Format::eR32G32Sfloat);
		push_attr(3, 3, vk::Format::eR32G32Sfloat);
		push_attr(4, 4, vk::Format::eR32G32Sfloat);
		push_attr(6, 6, vk::Format::eR8G8B8A8Unorm);
		push_attr(7, 7, vk::Format::eR8G8B8A8Unorm);
		break;

	case Vk_Shader_Type::TYPE_BLEND3_ADD_ENV:
	case Vk_Shader_Type::TYPE_BLEND3_MUL_ENV:
	case Vk_Shader_Type::TYPE_BLEND3_ALPHA_ENV:
	case Vk_Shader_Type::TYPE_BLEND3_ONE_MINUS_ALPHA_ENV:
	case Vk_Shader_Type::TYPE_BLEND3_MIX_ALPHA_ENV:
	case Vk_Shader_Type::TYPE_BLEND3_MIX_ONE_MINUS_ALPHA_ENV:
	case Vk_Shader_Type::TYPE_BLEND3_DST_COLOR_SRC_ALPHA_ENV:
		push_bind(0, sizeof(vec4_t));	  // xyz array
		push_bind(1, sizeof(color4ub_t)); // color0 array
		// push_bind( 2, sizeof( vec2_t ) );					// st0 array
		push_bind(3, sizeof(vec2_t));	  // st1 array
		push_bind(4, sizeof(vec2_t));	  // st2 array
		push_bind(5, sizeof(vec4_t));	  // normals
		push_bind(6, sizeof(color4ub_t)); // color1 array
		push_bind(7, sizeof(color4ub_t)); // color2 array
		push_attr(0, 0, vk::Format::eR32G32B32A32Sfloat);
		push_attr(1, 1, vk::Format::eR8G8B8A8Unorm);
		// push_attr( 2, 2, vk::Format::eR32G32Sfloat );
		push_attr(3, 3, vk::Format::eR32G32Sfloat);
		push_attr(4, 4, vk::Format::eR32G32Sfloat);
		push_attr(5, 5, vk::Format::eR32G32B32A32Sfloat);
		push_attr(6, 6, vk::Format::eR8G8B8A8Unorm);
		push_attr(7, 7, vk::Format::eR8G8B8A8Unorm);
		break;

	default:
		ri.Error(ERR_DROP, "%s: invalid shader type - %i", __func__, static_cast<int>(def.shader_type));
		break;
	}

	vk::PipelineVertexInputStateCreateInfo vertex_input_state{ {},
															  num_binds,
															  bindingsCpp,
															  num_attrs,
															  attribsCpp,
															  nullptr };

	//
	// Primitive assembly.
	//
	vk::PipelineInputAssemblyStateCreateInfo input_assembly_state{ {},
																  GetTopologyByPrimitivies(def.primitives),
																  vk::False,
																  nullptr };

	//
	// Viewport.
	//
	vk::PipelineViewportStateCreateInfo viewport_state{ {},
													   1,
													   nullptr, // dynamic viewport state
													   1,
													   nullptr, // dynamic scissor state
													   nullptr };

	//
	// Rasterization.
	//
	vk::PipelineRasterizationStateCreateInfo rasterization_state{ {},
																 vk::False,
																 vk::False,
																 def.shader_type == Vk_Shader_Type::TYPE_DOT ? vk::PolygonMode::ePoint : ((state_bits & GLS_POLYMODE_LINE) ? vk::PolygonMode::eLine : vk::PolygonMode::eFill),
																 {},
																 vk::FrontFace::eClockwise, // Q3 defaults to clockwise vertex order
																 def.polygon_offset ? vk::True : vk::False,
																 {},
																 0.0f,
																 {},
																 def.line_width ? (float)def.line_width : 1.0f,
																 nullptr };

	GetCullModeByFaceCulling(def, rasterization_state.cullMode);

	// depth bias state
	if (def.polygon_offset)
	{
		rasterization_state.depthBiasEnable = vk::True;
		rasterization_state.depthBiasClamp = 0.0f;
#ifdef USE_REVERSED_DEPTH
		rasterization_state.depthBiasConstantFactor = -r_offsetUnits->value;
		rasterization_state.depthBiasSlopeFactor = -r_offsetFactor->value;
#else
		rasterization_state.depthBiasConstantFactor = r_offsetUnits->value;
		rasterization_state.depthBiasSlopeFactor = r_offsetFactor->value;
#endif
	}
	else
	{
		rasterization_state.depthBiasEnable = vk::False;
		rasterization_state.depthBiasClamp = 0.0f;
		rasterization_state.depthBiasConstantFactor = 0.0f;
		rasterization_state.depthBiasSlopeFactor = 0.0f;
	}

	vk::PipelineMultisampleStateCreateInfo multisample_state{ {},
															 (renderPassIndex == renderPass_t::RENDER_PASS_SCREENMAP) ? vk_inst.screenMapSamples : vkSamples,
															 vk::False,
															 1.0f,
															 nullptr,
															 alphaToCoverage,
															 vk::False,
															 nullptr };

	vk::PipelineDepthStencilStateCreateInfo depth_stencil_state{ {},
																(state_bits & GLS_DEPTHTEST_DISABLE) ? vk::False : vk::True,
																(state_bits & GLS_DEPTHMASK_TRUE) ? vk::True : vk::False,
																{},
																vk::False,
																(def.shadow_phase != Vk_Shadow_Phase::SHADOW_DISABLED) ? vk::True : vk::False,
																{},
																{},
																0.0f,
																1.0f,
																nullptr };

#ifdef USE_REVERSED_DEPTH
	depth_stencil_state.depthCompareOp = (state_bits & GLS_DEPTHFUNC_EQUAL) ? vk::CompareOp::eEqual : vk::CompareOp::eGreaterOrEqual;
#else
	depth_stencil_state.depthCompareOp = (state_bits & GLS_DEPTHFUNC_EQUAL) ? vk::CompareOp::eEqual : vk::CompareOp::eLessOrEqual;
#endif

	if (def.shadow_phase == Vk_Shadow_Phase::SHADOW_EDGES)
	{
		depth_stencil_state.front.failOp = vk::StencilOp::eKeep;
		depth_stencil_state.front.passOp = (def.face_culling == cullType_t::CT_FRONT_SIDED) ? vk::StencilOp::eIncrementAndClamp : vk::StencilOp::eDecrementAndClamp;
		depth_stencil_state.front.depthFailOp = vk::StencilOp::eKeep;
		depth_stencil_state.front.compareOp = vk::CompareOp::eAlways;
		depth_stencil_state.front.compareMask = 255;
		depth_stencil_state.front.writeMask = 255;
		depth_stencil_state.front.reference = 0;

		depth_stencil_state.back = depth_stencil_state.front;
	}
	else if (def.shadow_phase == Vk_Shadow_Phase::SHADOW_FS_QUAD)
	{
		depth_stencil_state.front.failOp = vk::StencilOp::eKeep;
		depth_stencil_state.front.passOp = vk::StencilOp::eKeep;
		depth_stencil_state.front.depthFailOp = vk::StencilOp::eKeep;
		depth_stencil_state.front.compareOp = vk::CompareOp::eNotEqual;
		depth_stencil_state.front.compareMask = 255;
		depth_stencil_state.front.writeMask = 255;
		depth_stencil_state.front.reference = 0;

		depth_stencil_state.back = depth_stencil_state.front;
	}

	vk::PipelineColorBlendAttachmentState attachment_blend_state = createBlendAttachmentState(state_bits, def);

	if (attachment_blend_state.blendEnable)
	{
		if (def.allow_discard && vkSamples != vk::SampleCountFlagBits::e1)
		{
			// try to reduce pixel fillrate for transparent surfaces, this yields 1..10% fps increase when multisampling in enabled
			if (attachment_blend_state.srcColorBlendFactor == vk::BlendFactor::eSrcAlpha && attachment_blend_state.dstColorBlendFactor == vk::BlendFactor::eOneMinusSrcAlpha)
			{
				fragSpec.discardMode = 1;
			}
			else if (attachment_blend_state.srcColorBlendFactor == vk::BlendFactor::eOne && attachment_blend_state.dstColorBlendFactor == vk::BlendFactor::eOne)
			{
				fragSpec.discardMode = 2;
			}
		}
	}

	vk::PipelineColorBlendStateCreateInfo blend_state{ {},
													  vk::False,
													  vk::LogicOp::eCopy,
													  1,
													  &attachment_blend_state,
													  {0.0f, 0.0f, 0.0f, 0.0f},
													  nullptr };

	vk::PipelineDynamicStateCreateInfo dynamic_state{ {},
													 arrayLen(dynamic_state_array),
													 dynamic_state_array,
													 nullptr };

	vk::GraphicsPipelineCreateInfo create_info{ {},
											   shader_stages.size(),
											   shader_stages.data(),
											   &vertex_input_state,
											   &input_assembly_state,
											   nullptr,
											   &viewport_state,
											   &rasterization_state,
											   &multisample_state,
											   &depth_stencil_state,
											   &blend_state,
											   &dynamic_state,
											   (def.shader_type == Vk_Shader_Type::TYPE_DOT) ? vk_inst.pipeline_layout_storage : vk_inst.pipeline_layout,
											   (renderPassIndex == renderPass_t::RENDER_PASS_SCREENMAP) ? vk_inst.render_pass.screenmap : vk_inst.render_pass.main,
											   0,
											   nullptr,
											   -1,
											   nullptr };

	vk::Pipeline resultPipeline;

#ifdef USE_VK_VALIDATION
	// VK_CHECK_ASSIGN(res, vk_inst.device.createGraphicsPipeline(vk_inst.pipelineCache, create_info));
	try
	{
		auto createGraphicsPipelineResult = vk_inst.device.createGraphicsPipeline(vk_inst.pipelineCache, create_info);

		if (static_cast<int>(createGraphicsPipelineResult.result) < 0)
		{
			ri.Error(ERR_FATAL, "Vulkan: %s returned %s", "create_pipeline -> createGraphicsPipeline", vk::to_string(createGraphicsPipelineResult.result).data());
		}
		else
		{
			resultPipeline = createGraphicsPipelineResult.value;
			SET_OBJECT_NAME(VkPipeline(resultPipeline), va("pipeline def#%i, pass#%i", def_index, static_cast<int>(renderPassIndex)), VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT);
			SET_OBJECT_NAME(VkPipeline(resultPipeline), "create_pipeline -> createGraphicsPipeline", VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT);
		}
	}
	catch (vk::SystemError& err)
	{
		ri.Error(ERR_FATAL, "Vulkan error in function: %s, what: %s", "create_pipeline -> createGraphicsPipeline", err.what());
	}

#else
	VK_CHECK_ASSIGN(resultPipeline, vk_inst.device.createGraphicsPipeline(vk_inst.pipelineCache, create_info));

#endif
	vk_inst.pipeline_create_count++;
	return resultPipeline;
}

vk::Pipeline vk_gen_pipeline(const uint32_t index)
{
	if (index < vk_inst.pipelines_count)
	{
		VK_Pipeline_t* pipeline = vk_inst.pipelines + index;
		const uint8_t pass = static_cast<uint8_t>(vk_inst.renderPassIndex);
		if (!pipeline->handle[pass])
			pipeline->handle[pass] = create_pipeline(pipeline->def, vk_inst.renderPassIndex, index);
		return pipeline->handle[pass];
	}
	else
	{
		ri.Error(ERR_FATAL, "%s(%i): NULL pipeline", __func__, index);
		return nullptr;
	}
}

void vk_bind_pipeline(const uint32_t pipeline)
{
	vk::Pipeline vkpipe;

	vkpipe = vk_gen_pipeline(pipeline);

	if (vkpipe != vk_inst.cmd->last_pipeline)
	{
		vk_inst.cmd->command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, vkpipe);
		vk_inst.cmd->last_pipeline = vkpipe;
	}

	vk_world.dirty_depth_attachment |= (vk_inst.pipelines[pipeline].def.state_bits & GLS_DEPTHMASK_TRUE);
}

// Define a struct to hold the RGB values
struct ColorDepth
{
	int r;
	int g;
	int b;
};

static constexpr ColorDepth GetColorDepthForFormat(vk::Format format)
{
	switch (format)
	{
	case vk::Format::eB8G8R8A8Unorm:
	case vk::Format::eB8G8R8A8Srgb:
	case vk::Format::eR8G8B8A8Unorm:
	case vk::Format::eR8G8B8A8Srgb:
	case vk::Format::eR8G8B8A8Snorm:
	case vk::Format::eA8B8G8R8UnormPack32:
	case vk::Format::eA8B8G8R8SnormPack32:
	case vk::Format::eA8B8G8R8SrgbPack32:
	case vk::Format::eB8G8R8A8Snorm:
		return { 255, 255, 255 };

	case vk::Format::eA2B10G10R10UnormPack32:
	case vk::Format::eA2R10G10B10UnormPack32:
		return { 1023, 1023, 1023 };

	case vk::Format::eR16G16B16A16Unorm:
	case vk::Format::eR16G16B16A16Snorm:
		return { 65535, 65535, 65535 };

	case vk::Format::eR5G6B5UnormPack16:
	case vk::Format::eB5G6R5UnormPack16:
		return { 31, 63, 31 };

	case vk::Format::eR4G4B4A4UnormPack16:
	case vk::Format::eB4G4R4A4UnormPack16:
		return { 15, 15, 15 };

	case vk::Format::eA1R5G5B5UnormPack16:
	case vk::Format::eR5G5B5A1UnormPack16:
	case vk::Format::eB5G5R5A1UnormPack16:
		return { 31, 31, 31 };

	default:
		return { -1, -1, -1 };
	}
}

static bool vk_surface_format_color_depth(const vk::Format format, int* r, int* g, int* b)
{
	ColorDepth depth = GetColorDepthForFormat(format);

	if (depth.r == -1 && depth.g == -1 && depth.b == -1) // Default case
	{
		*r = 255;
		*g = 255;
		*b = 255;
		return false;
	}

	*r = depth.r;
	*g = depth.g;
	*b = depth.b;
	return true;
}

void vk_create_post_process_pipeline(const int program_index, const uint32_t width, const uint32_t height)
{
	vk::Pipeline* pipeline;
	vk::ShaderModule fsmodule;
	vk::RenderPass renderpass;
	vk::PipelineLayout layout;
	vk::SampleCountFlagBits samples;
#ifdef USE_VK_VALIDATION
	const char* pipeline_name;
#endif
	bool blend = false;

	struct FragSpecData
	{
		float gamma;
		float overbright;
		float greyscale;
		float bloom_threshold;
		float bloom_intensity;
		int bloom_threshold_mode;
		int bloom_modulate;
		int dither;
		int depth_r;
		int depth_g;
		int depth_b;
	};

	switch (program_index)
	{
	case 1: // bloom extraction
		pipeline = &vk_inst.bloom_extract_pipeline;
		fsmodule = vk_inst.modules.bloom_fs;
		renderpass = vk_inst.render_pass.bloom_extract;
		layout = vk_inst.pipeline_layout_post_process;
		samples = vk::SampleCountFlagBits::e1;
#ifdef USE_VK_VALIDATION
		pipeline_name = "bloom extraction pipeline";
#endif
		blend = false;
		break;
	case 2: // final bloom blend
		pipeline = &vk_inst.bloom_blend_pipeline;
		fsmodule = vk_inst.modules.blend_fs;
		renderpass = vk_inst.render_pass.post_bloom;
		layout = vk_inst.pipeline_layout_blend;
		samples = vkSamples;
#ifdef USE_VK_VALIDATION
		pipeline_name = "bloom blend pipeline";
#endif
		blend = true;
		break;
	case 3: // capture buffer extraction
		pipeline = &vk_inst.capture_pipeline;
		fsmodule = vk_inst.modules.gamma_fs;
		renderpass = vk_inst.render_pass.capture;
		layout = vk_inst.pipeline_layout_post_process;
		samples = vk::SampleCountFlagBits::e1;
#ifdef USE_VK_VALIDATION
		pipeline_name = "capture buffer pipeline";
#endif
		blend = false;
		break;
	default: // gamma correction
		pipeline = &vk_inst.gamma_pipeline;
		fsmodule = vk_inst.modules.gamma_fs;
		renderpass = vk_inst.render_pass.gamma;
		layout = vk_inst.pipeline_layout_post_process;
		samples = vk::SampleCountFlagBits::e1;
#ifdef USE_VK_VALIDATION
		pipeline_name = "gamma-correction pipeline";
#endif
		blend = false;
		break;
	}

	if (*pipeline)
	{
		VK_CHECK(vk_inst.device.waitIdle());
		vk_inst.device.destroyPipeline(*pipeline);
		*pipeline = nullptr;
	}

	vk::PipelineVertexInputStateCreateInfo vertex_input_state{};

	// shaders
	std::array<vk::PipelineShaderStageCreateInfo, 2> shader_stages;
	set_shader_stage_desc(shader_stages[0], vk::ShaderStageFlagBits::eVertex, vk_inst.modules.gamma_vs, "main");
	set_shader_stage_desc(shader_stages[1], vk::ShaderStageFlagBits::eFragment, fsmodule, "main");

	FragSpecData frag_spec_data{
		1.0f / (r_gamma->value),
		(float)(1 << tr.overbrightBits),
		r_greyscale->value,
		r_bloom_threshold->value,
		r_bloom_intensity->value,
		r_bloom_threshold_mode->integer,
		r_bloom_modulate->integer,
		r_dither->integer,
		0,
		0,
		0 };

	if (!vk_surface_format_color_depth(vk_inst.present_format.format, &frag_spec_data.depth_r, &frag_spec_data.depth_g, &frag_spec_data.depth_b))
		ri.Printf(PRINT_ALL, "Format %s not recognized, dither to assume 8bpc\n", vk::to_string(vk_inst.base_format.format).data());

	std::array<vk::SpecializationMapEntry, 11> spec_entries = { {
		{0, offsetof(FragSpecData, gamma), sizeof(frag_spec_data.gamma)},
		{1, offsetof(FragSpecData, overbright), sizeof(frag_spec_data.overbright)},
		{2, offsetof(FragSpecData, greyscale), sizeof(frag_spec_data.greyscale)},
		{3, offsetof(FragSpecData, bloom_threshold), sizeof(frag_spec_data.bloom_threshold)},
		{4, offsetof(FragSpecData, bloom_intensity), sizeof(frag_spec_data.bloom_intensity)},
		{5, offsetof(FragSpecData, bloom_threshold_mode), sizeof(frag_spec_data.bloom_threshold_mode)},
		{6, offsetof(FragSpecData, bloom_modulate), sizeof(frag_spec_data.bloom_modulate)},
		{7, offsetof(FragSpecData, dither), sizeof(frag_spec_data.dither)},
		{8, offsetof(FragSpecData, depth_r), sizeof(frag_spec_data.depth_r)},
		{9, offsetof(FragSpecData, depth_g), sizeof(frag_spec_data.depth_g)},
		{10, offsetof(FragSpecData, depth_b), sizeof(frag_spec_data.depth_b)},
	} };

	vk::SpecializationInfo frag_spec_info{
		static_cast<uint32_t>(spec_entries.size()),
		spec_entries.data(),
		sizeof(frag_spec_data),
		&frag_spec_data };
	shader_stages[1].pSpecializationInfo = &frag_spec_info;

	//
	// Primitive assembly.
	//
	vk::PipelineInputAssemblyStateCreateInfo input_assembly_state{ {}, vk::PrimitiveTopology::eTriangleStrip, vk::False };

	//
	// Viewport.
	//
	vk::Viewport viewport{
		program_index == 0 ? 0.0f + vk_inst.blitX0 : 0.0f,
		program_index == 0 ? 0.0f + vk_inst.blitY0 : 0.0f,
		program_index == 0 ? gls.windowWidth - vk_inst.blitX0 * 2 : static_cast<float>(width),
		program_index == 0 ? gls.windowHeight - vk_inst.blitY0 * 2 : static_cast<float>(height),
		0.0f,
		1.0f };

	vk::Rect2D scissor{
		{static_cast<int32_t>(viewport.x), static_cast<int32_t>(viewport.y)},
		{static_cast<uint32_t>(viewport.width), static_cast<uint32_t>(viewport.height)} };

	vk::PipelineViewportStateCreateInfo viewport_state{ {}, 1, &viewport, 1, &scissor };

	//
	// Rasterization.
	//
	vk::PipelineRasterizationStateCreateInfo rasterization_state{ {},
																 vk::False,
																 vk::False,
																 vk::PolygonMode::eFill,
																 vk::CullModeFlagBits::eNone,
																 vk::FrontFace::eClockwise,
																 vk::False,
																 0.0f,
																 0.0f,
																 0.0f,
																 1.0f,
																 nullptr };

	vk::PipelineMultisampleStateCreateInfo multisample_state{ {},
															 samples,
															 vk::False,
															 1.0f,
															 nullptr,
															 vk::False,
															 vk::False,
															 nullptr };

	vk::PipelineColorBlendAttachmentState attachment_blend_state{
		blend,
		blend == true ? vk::BlendFactor::eOne : vk::BlendFactor::eZero,
		blend == true ? vk::BlendFactor::eOne : vk::BlendFactor::eZero,
		vk::BlendOp::eAdd,
		vk::BlendFactor::eZero,
		vk::BlendFactor::eZero,
		vk::BlendOp::eAdd,
		vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
			vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA };

	std::array<float, 4> blendConstants{};

	vk::PipelineColorBlendStateCreateInfo blend_state{ {},
													  vk::False,
													  vk::LogicOp::eCopy,
													  1,
													  &attachment_blend_state,
													  blendConstants };

	vk::PipelineDepthStencilStateCreateInfo depth_stencil_state{ {},
																vk::False,
																vk::False,
																vk::CompareOp::eNever,
																vk::False,
																vk::False,
																{},
																{},
																0.0f,
																1.0f,
																nullptr };

	vk::GraphicsPipelineCreateInfo pipeline_create_info{
		{},
		static_cast<uint32_t>(shader_stages.size()),
		shader_stages.data(),
		&vertex_input_state,
		&input_assembly_state,
		nullptr,
		&viewport_state,
		&rasterization_state,
		&multisample_state,
		&depth_stencil_state,
		&blend_state,
		nullptr,
		layout,
		renderpass,
		0,
		nullptr,
		-1 };

#ifdef USE_VK_VALIDATION
	try
	{
		auto createGraphicsPipelineResult = vk_inst.device.createGraphicsPipeline(nullptr, pipeline_create_info);
		if (static_cast<int>(createGraphicsPipelineResult.result) < 0)
		{
			ri.Error(ERR_FATAL, "Vulkan: %s returned %s", "vk_create_post_process_pipeline -> createGraphicsPipeline", vk::to_string(createGraphicsPipelineResult.result).data());
		}
		else
		{
			*pipeline = createGraphicsPipelineResult.value;
			SET_OBJECT_NAME(VkPipeline(*pipeline), pipeline_name, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT);
		}
	}
	catch (vk::SystemError& err)
	{
		ri.Error(ERR_FATAL, "Vulkan error in function: %s, what: %s", "vk_create_post_process_pipeline -> createGraphicsPipeline", err.what());
	}

#else
	VK_CHECK_ASSIGN(*pipeline, vk_inst.device.createGraphicsPipeline(nullptr, pipeline_create_info));
#endif
}

void vk_create_blur_pipeline(const uint32_t index, const uint32_t width, const uint32_t height, const bool horizontal_pass)
{
	vk::Pipeline* pipeline = &vk_inst.blur_pipeline[index];

	if (*pipeline)
	{
		VK_CHECK(vk_inst.device.waitIdle());
		vk_inst.device.destroyPipeline(*pipeline);
		*pipeline = nullptr;
	}

	vk::PipelineVertexInputStateCreateInfo vertex_input_state{};

	// shaders
	std::array<vk::PipelineShaderStageCreateInfo, 2> shader_stages;
	set_shader_stage_desc(shader_stages[0], vk::ShaderStageFlagBits::eVertex, vk_inst.modules.gamma_vs, "main");
	set_shader_stage_desc(shader_stages[1], vk::ShaderStageFlagBits::eFragment, vk_inst.modules.blur_fs, "main");

	std::array<float, 3> frag_spec_data{ {1.2f / (float)width, 1.2f / (float)height, 1.0} }; // x-offset, y-offset, correction

	if (horizontal_pass)
	{
		frag_spec_data[1] = 0.0;
	}
	else
	{
		frag_spec_data[0] = 0.0;
	}

	std::array<vk::SpecializationMapEntry, 3> spec_entries = { {
		{0, 0 * sizeof(frag_spec_data[0]), sizeof(frag_spec_data[0])},
		{1, 1 * sizeof(frag_spec_data[1]), sizeof(frag_spec_data[1])},
		{2, 2 * sizeof(frag_spec_data[2]), sizeof(frag_spec_data[2])},
	} };

	vk::SpecializationInfo frag_spec_info{
		static_cast<uint32_t>(spec_entries.size()), spec_entries.data(), sizeof(frag_spec_data),
		&frag_spec_data[0] };

	shader_stages[1].pSpecializationInfo = &frag_spec_info;

	//
	// Primitive assembly.
	//
	vk::PipelineInputAssemblyStateCreateInfo input_assembly_state{ {},
																  vk::PrimitiveTopology::eTriangleStrip,
																  vk::False,
																  nullptr };

	//
	// Viewport.
	//
	vk::Viewport viewport{ 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f };
	vk::Rect2D scissor{ {(int32_t)viewport.x, (int32_t)viewport.y}, {(uint32_t)viewport.width, (uint32_t)viewport.height} };

	vk::PipelineViewportStateCreateInfo viewport_state{ {}, 1, &viewport, 1, &scissor };

	//
	// Rasterization.
	//
	vk::PipelineRasterizationStateCreateInfo rasterization_state{
		{},
		vk::False,
		vk::False,
		vk::PolygonMode::eFill,
		vk::CullModeFlagBits::eNone,
		vk::FrontFace::eClockwise,
		vk::False,
		0.0f,
		0.0f,
		0.0f,
		1.0f,
		nullptr };

	vk::PipelineMultisampleStateCreateInfo multisample_state{ {},
															 vk::SampleCountFlagBits::e1,
															 vk::False,
															 1.0f,
															 nullptr,
															 vk::False,
															 vk::False,
															 nullptr };

	vk::PipelineColorBlendAttachmentState attachment_blend_state{
		vk::False,
		{},
		{},
		{},
		{},
		{},
		{},
		vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
			vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA };

	vk::PipelineColorBlendStateCreateInfo blend_state{
		{},
		vk::False,
		vk::LogicOp::eCopy,
		1,
		&attachment_blend_state,
		{0.0f, 0.0f, 0.0f, 0.0f},
		nullptr };

	vk::GraphicsPipelineCreateInfo create_info{
		{},
		static_cast<uint32_t>(shader_stages.size()),
		shader_stages.data(),
		&vertex_input_state,
		&input_assembly_state,
		nullptr,
		&viewport_state,
		&rasterization_state,
		&multisample_state,
		nullptr,
		&blend_state,
		nullptr,
		vk_inst.pipeline_layout_post_process,
		vk_inst.render_pass.blur[index],
		0,
		nullptr,
		-1,
		nullptr };

#ifdef USE_VK_VALIDATION
	try
	{
		auto createGraphicsPipelineResult = vk_inst.device.createGraphicsPipeline(nullptr, create_info);
		if (static_cast<int>(createGraphicsPipelineResult.result) < 0)
		{
			ri.Error(ERR_FATAL, "Vulkan: %s returned %s", "vk_create_blur_pipeline -> createGraphicsPipeline", vk::to_string(createGraphicsPipelineResult.result).data());
		}
		else
		{
			*pipeline = createGraphicsPipelineResult.value;
			SET_OBJECT_NAME(VkPipeline(*pipeline), va("%s blur pipeline %i", horizontal_pass ? "horizontal" : "vertical", index / 2 + 1), VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT);
		}
	}
	catch (vk::SystemError& err)
	{
		ri.Error(ERR_FATAL, "Vulkan error in function: %s, what: %s", "vk_create_blur_pipeline -> createGraphicsPipeline", err.what());
	}

#else
	VK_CHECK_ASSIGN(*pipeline, vk_inst.device.createGraphicsPipeline(vk_inst.pipelineCache, create_info));
#endif
}
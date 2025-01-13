#ifndef DEFINITIONS_HPP
#define DEFINITIONS_HPP

#if defined(_WIN32) && defined(_DEBUG)
#define USE_VK_VALIDATION
#endif

#ifdef DEBUG
#define USE_VK_VALIDATION
#endif

#ifndef USE_VK_VALIDATION
#define VULKAN_HPP_NO_EXCEPTIONS
// #define VULKAN_HPP_HAS_NOEXCEPT
#endif

#include "vulkan/vulkan.hpp"

constexpr int VK_NUM_BLOOM_PASSES = 4;
constexpr int MAX_VK_SAMPLERS = 32;
constexpr int MAX_IMAGE_CHUNKS = 56;
constexpr int MAX_SWAPCHAIN_IMAGES = 8;
constexpr int MAX_ATTACHMENTS_IN_POOL(8 + VK_NUM_BLOOM_PASSES * 2); // depth + msaa + msaa-resolve + depth-resolve + screenmap.msaa + screenmap.resolve + screenmap.depth + bloom_extract + blur pairs
constexpr int NUM_COMMAND_BUFFERS = 2;                              // number of command buffers / render semaphores / framebuffer sets
constexpr int MAX_VK_PIPELINES = (1024 + 128);

typedef unsigned char byte;

typedef enum
{
    DEPTH_RANGE_NORMAL, // [0..1]
    DEPTH_RANGE_ZERO,   // [0..0]
    DEPTH_RANGE_ONE,    // [1..1]
    DEPTH_RANGE_WEAPON, // [0..0.3]
    DEPTH_RANGE_COUNT
} Vk_Depth_Range;

typedef struct
{
    vk::SamplerAddressMode address_mode; // clamp/repeat texture addressing mode
    int gl_mag_filter;                   // GL_XXX mag filter
    int gl_min_filter;                   // GL_XXX min filter
    bool max_lod_1_0;                    // fixed 1.0 lod
    bool noAnisotropy;
} Vk_Sampler_Def;

typedef struct
{
    vk::DeviceMemory memory;
    vk::DeviceSize used;
} ImageChunk;

typedef struct
{
    //
    // Resources.
    //
    int num_samplers;
    Vk_Sampler_Def sampler_defs[MAX_VK_SAMPLERS];
    vk::Sampler samplers[MAX_VK_SAMPLERS];

    //
    // Memory allocations.
    //
    int num_image_chunks;
    ImageChunk image_chunks[MAX_IMAGE_CHUNKS];

    // Host visible memory used to copy image data to device local memory.
    vk::Buffer staging_buffer;
    vk::DeviceMemory staging_buffer_memory;
    vk::DeviceSize staging_buffer_size;
    byte *staging_buffer_ptr; // pointer to mapped staging buffer

    //
    // State.
    //

    // Descriptor sets corresponding to bound texture images.
    // VkDescriptorSet current_descriptor_sets[ MAX_TEXTURE_UNITS ];

    // This flag is used to decide whether framebuffer's depth attachment should be cleared
    // with vmCmdClearAttachment (dirty_depth_attachment != 0), or it have just been
    // cleared by render pass instance clear op (dirty_depth_attachment == 0).
    int dirty_depth_attachment;

    float modelview_transform[16];
} Vk_World;

typedef enum
{
    RENDER_PASS_SCREENMAP = 0,
    RENDER_PASS_MAIN,
    RENDER_PASS_POST_BLOOM,
    RENDER_PASS_COUNT
} renderPass_t;

typedef enum
{
    TYPE_COLOR_BLACK,
    TYPE_COLOR_WHITE,
    TYPE_COLOR_GREEN,
    TYPE_COLOR_RED,
    TYPE_FOG_ONLY,
    TYPE_DOT,

    TYPE_SIGNLE_TEXTURE_LIGHTING,
    TYPE_SIGNLE_TEXTURE_LIGHTING_LINEAR,

    TYPE_SIGNLE_TEXTURE_DF,

    TYPE_GENERIC_BEGIN, // start of non-env/env shader pairs
    TYPE_SIGNLE_TEXTURE = TYPE_GENERIC_BEGIN,
    TYPE_SIGNLE_TEXTURE_ENV,

    TYPE_SIGNLE_TEXTURE_IDENTITY,
    TYPE_SIGNLE_TEXTURE_IDENTITY_ENV,

    TYPE_SIGNLE_TEXTURE_FIXED_COLOR,
    TYPE_SIGNLE_TEXTURE_FIXED_COLOR_ENV,

    TYPE_SIGNLE_TEXTURE_ENT_COLOR,
    TYPE_SIGNLE_TEXTURE_ENT_COLOR_ENV,

    TYPE_MULTI_TEXTURE_ADD2_IDENTITY,
    TYPE_MULTI_TEXTURE_ADD2_IDENTITY_ENV,
    TYPE_MULTI_TEXTURE_MUL2_IDENTITY,
    TYPE_MULTI_TEXTURE_MUL2_IDENTITY_ENV,

    TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR,
    TYPE_MULTI_TEXTURE_ADD2_FIXED_COLOR_ENV,
    TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR,
    TYPE_MULTI_TEXTURE_MUL2_FIXED_COLOR_ENV,

    TYPE_MULTI_TEXTURE_MUL2,
    TYPE_MULTI_TEXTURE_MUL2_ENV,
    TYPE_MULTI_TEXTURE_ADD2_1_1,
    TYPE_MULTI_TEXTURE_ADD2_1_1_ENV,
    TYPE_MULTI_TEXTURE_ADD2,
    TYPE_MULTI_TEXTURE_ADD2_ENV,

    TYPE_MULTI_TEXTURE_MUL3,
    TYPE_MULTI_TEXTURE_MUL3_ENV,
    TYPE_MULTI_TEXTURE_ADD3_1_1,
    TYPE_MULTI_TEXTURE_ADD3_1_1_ENV,
    TYPE_MULTI_TEXTURE_ADD3,
    TYPE_MULTI_TEXTURE_ADD3_ENV,

    TYPE_BLEND2_ADD,
    TYPE_BLEND2_ADD_ENV,
    TYPE_BLEND2_MUL,
    TYPE_BLEND2_MUL_ENV,
    TYPE_BLEND2_ALPHA,
    TYPE_BLEND2_ALPHA_ENV,
    TYPE_BLEND2_ONE_MINUS_ALPHA,
    TYPE_BLEND2_ONE_MINUS_ALPHA_ENV,
    TYPE_BLEND2_MIX_ALPHA,
    TYPE_BLEND2_MIX_ALPHA_ENV,

    TYPE_BLEND2_MIX_ONE_MINUS_ALPHA,
    TYPE_BLEND2_MIX_ONE_MINUS_ALPHA_ENV,

    TYPE_BLEND2_DST_COLOR_SRC_ALPHA,
    TYPE_BLEND2_DST_COLOR_SRC_ALPHA_ENV,

    TYPE_BLEND3_ADD,
    TYPE_BLEND3_ADD_ENV,
    TYPE_BLEND3_MUL,
    TYPE_BLEND3_MUL_ENV,
    TYPE_BLEND3_ALPHA,
    TYPE_BLEND3_ALPHA_ENV,
    TYPE_BLEND3_ONE_MINUS_ALPHA,
    TYPE_BLEND3_ONE_MINUS_ALPHA_ENV,
    TYPE_BLEND3_MIX_ALPHA,
    TYPE_BLEND3_MIX_ALPHA_ENV,
    TYPE_BLEND3_MIX_ONE_MINUS_ALPHA,
    TYPE_BLEND3_MIX_ONE_MINUS_ALPHA_ENV,

    TYPE_BLEND3_DST_COLOR_SRC_ALPHA,
    TYPE_BLEND3_DST_COLOR_SRC_ALPHA_ENV,

    TYPE_GENERIC_END = TYPE_BLEND3_MIX_ONE_MINUS_ALPHA_ENV

} Vk_Shader_Type;

// used with cg_shadows == 2
typedef enum
{
    SHADOW_DISABLED,
    SHADOW_EDGES,
    SHADOW_FS_QUAD,
} Vk_Shadow_Phase;

typedef enum
{
    TRIANGLE_LIST = 0,
    TRIANGLE_STRIP,
    LINE_LIST,
    POINT_LIST
} Vk_Primitive_Topology;

typedef struct
{
    Vk_Shader_Type shader_type;
    unsigned int state_bits; // GLS_XXX flags
    cullType_t face_culling;
    bool polygon_offset;
    bool mirror;
    Vk_Shadow_Phase shadow_phase;
    Vk_Primitive_Topology primitives;
    int line_width;
    int fog_stage; // off, fog-in / fog-out
    int abs_light;
    int allow_discard;
    int acff; // none, rgb, rgba, alpha
    struct
    {
        byte rgb;
        byte alpha;
    } color;
} Vk_Pipeline_Def;

typedef struct VK_Pipeline
{
    Vk_Pipeline_Def def;
    vk::Pipeline handle[RENDER_PASS_COUNT];
} VK_Pipeline_t;

typedef struct vk_tess_s
{
    vk::CommandBuffer command_buffer;

    vk::Semaphore image_acquired;
    vk::Semaphore rendering_finished;
    vk::Fence rendering_finished_fence;
    bool waitForFence;

    vk::Buffer vertex_buffer;
    byte *vertex_buffer_ptr; // pointer to mapped vertex buffer
    vk::DeviceSize vertex_buffer_offset;

    vk::DescriptorSet uniform_descriptor;
    uint32_t uniform_read_offset;
    vk::DeviceSize buf_offset[8];
    vk::DeviceSize vbo_offset[8];

    vk::Buffer curr_index_buffer;
    uint32_t curr_index_offset;

    struct
    {
        uint32_t start, end;
        vk::DescriptorSet current[6]; // 0:storage, 1:uniform, 2:color0, 3:color1, 4:color2, 5:fog
        uint32_t offset[2];           // 0 (uniform) and 5 (storage)
    } descriptor_set;

    Vk_Depth_Range depth_range;
    vk::Pipeline last_pipeline;

    uint32_t num_indexes; // value from most recent vk_bind_index() call

    vk::Rect2D scissor_rect;
} vk_tess_t;

// Vk_Instance contains engine-specific vulkan resources that persist entire renderer lifetime.
// This structure is initialized/deinitialized by vk_initialize/vk_shutdown functions correspondingly.
typedef struct
{
    vk::PhysicalDevice physical_device;
    vk::PhysicalDevice physical_deviceHpp;
    vk::SurfaceFormatKHR base_format;
    vk::SurfaceFormatKHR present_format;

    uint32_t queue_family_index;
    vk::Device device;
    vk::Queue queue;

    vk::SwapchainKHR swapchain;
    uint32_t swapchain_image_count;
    vk::Image swapchain_images[MAX_SWAPCHAIN_IMAGES];
    vk::ImageView swapchain_image_views[MAX_SWAPCHAIN_IMAGES];
    uint32_t swapchain_image_index;

    vk::CommandPool command_pool;

    vk::DeviceMemory image_memory[MAX_ATTACHMENTS_IN_POOL];
    uint32_t image_memory_count;

    struct
    {
        vk::RenderPass main;
        vk::RenderPass screenmap;
        vk::RenderPass gamma;
        vk::RenderPass capture;
        vk::RenderPass bloom_extract;
        vk::RenderPass blur[VK_NUM_BLOOM_PASSES * 2]; // horizontal-vertical pairs
        vk::RenderPass post_bloom;
    } render_pass;

    vk::DescriptorPool descriptor_pool;
    vk::DescriptorSetLayout set_layout_sampler; // combined image sampler
    vk::DescriptorSetLayout set_layout_uniform; // dynamic uniform buffer
    vk::DescriptorSetLayout set_layout_storage; // feedback bufvk::r

    vk::PipelineLayout pipeline_layout; // default shaders
    // VkPipelineLayout pipeline_layout_storage;	// flare test shader layout
    vk::PipelineLayout pipeline_layout_post_process; // post-processing
    vk::PipelineLayout pipeline_layout_blend;        // post-processing

    vk::DescriptorSet color_descriptor;

    vk::Image color_image;
    vk::ImageView color_image_view;

    vk::Image bloom_image[1 + VK_NUM_BLOOM_PASSES * 2];
    vk::ImageView bloom_image_view[1 + VK_NUM_BLOOM_PASSES * 2];

    std::array<vk::DescriptorSet, 1 + VK_NUM_BLOOM_PASSES * 2> bloom_image_descriptor;

    vk::Image depth_image;
    vk::ImageView depth_image_view;

    vk::Image msaa_image;
    vk::ImageView msaa_image_view;

    // screenMap
    struct
    {
        vk::DescriptorSet color_descriptor;
        vk::Image color_image;
        vk::ImageView color_image_view;

        vk::Image color_image_msaa;
        vk::ImageView color_image_view_msaa;

        vk::Image depth_image;
        vk::ImageView depth_image_view;

    } screenMap;

    struct
    {
        vk::Image image;
        vk::ImageView image_view;
    } capture;

    struct
    {
        vk::Framebuffer blur[VK_NUM_BLOOM_PASSES * 2];
        vk::Framebuffer bloom_extract;
        vk::Framebuffer main[MAX_SWAPCHAIN_IMAGES];
        vk::Framebuffer gamma[MAX_SWAPCHAIN_IMAGES];
        vk::Framebuffer screenmap;
        vk::Framebuffer capture;
    } framebuffers;

    vk_tess_t tess[NUM_COMMAND_BUFFERS], *cmd;
    int cmd_index;

    struct
    {
        vk::Buffer buffer;
        byte *buffer_ptr;
        vk::DeviceMemory memory;
        vk::DescriptorSet descriptor;
    } storage;

    uint32_t uniform_item_size;
    uint32_t uniform_alignment;
    uint32_t storage_alignment;

    struct
    {
        vk::Buffer vertex_buffer;
        vk::DeviceMemory buffer_memory;
    } vbo;

    // host visible memory that holds vertex, index and uniform data
    vk::DeviceMemory geometry_buffer_memory;
    vk::DeviceSize geometry_buffer_size;
    vk::DeviceSize geometry_buffer_size_new;

    // statistics
    struct
    {
        vk::DeviceSize vertex_buffer_max;
        uint32_t push_size;
        uint32_t push_size_max;
    } stats;

    //
    // Shader modules.
    //
    struct
    {
        struct
        {
            vk::ShaderModule gen[3][2][2][2]; // tx[0,1,2], cl[0,1] env0[0,1] fog[0,1]
            vk::ShaderModule ident1[2][2][2]; // tx[0,1], env0[0,1] fog[0,1]
            vk::ShaderModule fixed[2][2][2];  // tx[0,1], env0[0,1] fog[0,1]
            vk::ShaderModule light[2];        // fog[0,1]
        } vert;
        struct
        {
            vk::ShaderModule gen0_df;
            vk::ShaderModule gen[3][2][2]; // tx[0,1,2] cl[0,1] fog[0,1]
            vk::ShaderModule ident1[2][2]; // tx[0,1], fog[0,1]
            vk::ShaderModule fixed[2][2];  // tx[0,1], fog[0,1]
            vk::ShaderModule ent[1][2];    // tx[0], fog[0,1]
            vk::ShaderModule light[2][2];  // linear[0,1] fog[0,1]
        } frag;

        vk::ShaderModule color_fs;
        vk::ShaderModule color_vs;

        vk::ShaderModule bloom_fs;
        vk::ShaderModule blur_fs;
        vk::ShaderModule blend_fs;

        vk::ShaderModule gamma_fs;
        vk::ShaderModule gamma_vs;

        vk::ShaderModule fog_fs;
        vk::ShaderModule fog_vs;

        vk::ShaderModule dot_fs;
        vk::ShaderModule dot_vs;
    } modules;

    vk::PipelineCache pipelineCache;

    VK_Pipeline_t pipelines[MAX_VK_PIPELINES];
    uint32_t pipelines_count;
    uint32_t pipelines_world_base;

    // pipeline statistics
    int32_t pipeline_create_count;

    //
    // Standard pipelines.
    //
    uint32_t skybox_pipeline;

    // dim 0: 0 - front side, 1 - back size
    // dim 1: 0 - normal view, 1 - mirror view
    uint32_t shadow_volume_pipelines[2][2];
    uint32_t shadow_finish_pipeline;

    // dim 0 is based on fogPass_t: 0 - corresponds to FP_EQUAL, 1 - corresponds to FP_LE.
    // dim 1 is directly a cullType_t enum value.
    // dim 2 is a polygon offset value (0 - off, 1 - on).
    uint32_t fog_pipelines[2][3][2];

    // dim 0 is based on dlight additive flag: 0 - not additive, 1 - additive
    // dim 1 is directly a cullType_t enum value.
    // dim 2 is a polygon offset value (0 - off, 1 - on).
#ifdef USE_LEGACY_DLIGHTS
    uint32_t dlight_pipelines[2][3][2];
#endif

    // cullType[3], polygonOffset[2], fogStage[2], absLight[2]
#ifdef USE_PMLIGHT
    uint32_t dlight_pipelines_x[3][2][2][2];
    uint32_t dlight1_pipelines_x[3][2][2][2];
#endif

    // debug visualization pipelines
    uint32_t tris_debug_pipeline;
    uint32_t tris_mirror_debug_pipeline;
    uint32_t tris_debug_green_pipeline;
    uint32_t tris_mirror_debug_green_pipeline;
    uint32_t tris_debug_red_pipeline;
    uint32_t tris_mirror_debug_red_pipeline;

    uint32_t normals_debug_pipeline;
    uint32_t surface_debug_pipeline_solid;
    uint32_t surface_debug_pipeline_outline;
    uint32_t images_debug_pipeline;
    uint32_t surface_beam_pipeline;
    uint32_t surface_axis_pipeline;
    uint32_t dot_pipeline;

    vk::Pipeline gamma_pipeline;
    vk::Pipeline capture_pipeline;
    vk::Pipeline bloom_extract_pipeline;
    std::array<vk::Pipeline, VK_NUM_BLOOM_PASSES * 2> blur_pipeline; // horizontal & vertical pairs
    vk::Pipeline bloom_blend_pipeline;

    uint32_t frame_count;
    bool active;
    bool wideLines;
    bool samplerAnisotropy;
    bool fragmentStores;
    bool dedicatedAllocation;
    bool debugMarkers;

    float maxAnisotropy;
    float maxLod;

    vk::Format color_format;
    vk::Format capture_format;
    vk::Format depth_format;
    vk::Format bloom_format;

    vk::ImageLayout initSwapchainLayout;

    bool fastSky; // requires VK_IMAGE_USAGE_TRANSFER_DST_BIT
    bool fboActive;
    bool blitEnabled;
    bool msaaActive;

    bool offscreenRender;

    bool windowAdjusted;
    int blitX0;
    int blitY0;
    int blitFilter;

    uint32_t renderWidth;
    uint32_t renderHeight;

    float renderScaleX;
    float renderScaleY;

    renderPass_t renderPassIndex;

    uint32_t screenMapWidth;
    uint32_t screenMapHeight;
    vk::SampleCountFlagBits screenMapSamples;

    uint32_t image_chunk_size;

    uint32_t maxBoundDescriptorSets;

} Vk_Instance;

#endif // DEFINITIONS_HPP

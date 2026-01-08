/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#ifndef TR_LOCAL_HPP
#define TR_LOCAL_HPP

// #define USE_TESS_NEEDS_NORMAL
// #define USE_TESS_NEEDS_ST2

// #include "q_shared.hpp"

extern "C"
{
#include "../qcommon/q_shared.h"
#include "../qcommon/qfiles.h"
#include "../qcommon/qcommon.h"
#include "../renderercommon/tr_public.h"
}

#include <cstdint>
using byte = std::uint8_t;

#include "tr_common.hpp"
#include "iqm.hpp"
#include "definitions.hpp"
#include <array>

extern Vk_Instance vk_inst; // shouldn't be cleared during ref re-init
extern Vk_World vk_world;	// this data is cleared during ref re-init
extern vk::SampleCountFlagBits vkSamples;
extern vk::detail::DispatchLoaderDynamic dldi;

#ifdef USE_VK_VALIDATION
extern PFN_vkDebugMarkerSetObjectNameEXT qvkDebugMarkerSetObjectNameEXT;

#define VK_CHECK(function_call)                                                                        \
	{                                                                                                  \
		try                                                                                            \
		{                                                                                              \
			function_call;                                                                             \
		}                                                                                              \
		catch (vk::SystemError & err)                                                                  \
		{                                                                                              \
			ri.Error(ERR_FATAL, "Vulkan error in function: %s, what: %s", #function_call, err.what()); \
		}                                                                                              \
	}

#define VK_CHECK_ASSIGN(var, function_call)                                                            \
	{                                                                                                  \
		try                                                                                            \
		{                                                                                              \
			var = function_call;                                                                       \
		}                                                                                              \
		catch (vk::SystemError & err)                                                                  \
		{                                                                                              \
			ri.Error(ERR_FATAL, "Vulkan error in function: %s, what: %s", #function_call, err.what()); \
		}                                                                                              \
	}

void vk_set_object_name(uint64_t obj, const char* objName, VkDebugReportObjectTypeEXT objType);
#define SET_OBJECT_NAME(obj, objName, objType) vk_set_object_name((uint64_t)(obj), (objName), (objType))

#else

template <typename T>
inline void vkCheckFunctionCall(const vk::ResultValue<T> res, const char* funcName)
{
	if (static_cast<int>(res.result) < 0)
	{
		ri.Error(ERR_FATAL, "Vulkan: %s returned %s", funcName, vk::to_string(res.result).data());
	}
}

static inline void vkCheckFunctionCall(const vk::Result res, const char* funcName)
{
	if (static_cast<int>(res) < 0)
	{
		ri.Error(ERR_FATAL, "Vulkan: %s returned %s", funcName, vk::to_string(res).data());
	}
}

#define VK_CHECK_ASSIGN(var, function_call)                 \
	{                                                       \
		auto result = function_call;                        \
		vkCheckFunctionCall(result.result, #function_call); \
		var = result.value;                                 \
	}

#define VK_CHECK(function_call)                             \
	{                                                       \
		vkCheckFunctionCall(function_call, #function_call); \
	}

#endif

constexpr int MAX_DRAWSURFS = 0x20000;
constexpr int MAX_LITSURFS = (MAX_DRAWSURFS);
constexpr int MAX_FLARES = 256;

constexpr int MAX_TEXTURE_SIZE = 2048; // must be less or equal to 32768

// GL constants substitutions
enum class glCompat : uint8_t
{
	GL_NEAREST,
	GL_LINEAR,
	GL_NEAREST_MIPMAP_NEAREST,
	GL_LINEAR_MIPMAP_NEAREST,
	GL_NEAREST_MIPMAP_LINEAR,
	GL_LINEAR_MIPMAP_LINEAR,
	GL_MODULATE,
	GL_ADD,
	GL_ADD_NONIDENTITY,

	GL_BLEND_MODULATE,
	GL_BLEND_ADD,
	GL_BLEND_ALPHA,
	GL_BLEND_ONE_MINUS_ALPHA,
	GL_BLEND_MIX_ALPHA,			  // SRC_ALPHA + ONE_MINUS_SRC_ALPHA
	GL_BLEND_MIX_ONE_MINUS_ALPHA, // ONE_MINUS_SRC_ALPHA + SRC_ALPHA

	GL_BLEND_DST_COLOR_SRC_ALPHA, // GLS_SRCBLEND_DST_COLOR + GLS_DSTBLEND_SRC_ALPHA

	GL_DECAL,
	GL_BACK_LEFT,
	GL_BACK_RIGHT
};


#define GL_INDEX_TYPE uint32_t
#define GLint int
#define GLuint unsigned int
#define GLboolean VkBool32

#define USE_BUFFER_CLEAR	/* clear attachments on render pass begin */
#define USE_REVERSED_DEPTH

typedef uint32_t glIndex_t;

constexpr int REFENTITYNUM_BITS = 12; // as we actually using only 1 bit for dlight mask in opengl1 renderer
constexpr int REFENTITYNUM_MASK = ((1 << REFENTITYNUM_BITS) - 1);
// the last N-bit number (2^REFENTITYNUM_BITS - 1) is reserved for the special world refentity,
//  and this is reflected by the value of MAX_REFENTITIES (which therefore is not a power-of-2)
constexpr int MAX_REFENTITIES = ((1 << REFENTITYNUM_BITS) - 1);
constexpr int REFENTITYNUM_WORLD = ((1 << REFENTITYNUM_BITS) - 1);
// 14 bits
// can't be increased without changing bit packing for drawsurfs
// see QSORT_SHADERNUM_SHIFT
constexpr int SHADERNUM_BITS = 14;
constexpr int MAX_SHADERS = (1 << SHADERNUM_BITS);
constexpr int SHADERNUM_MASK = (MAX_SHADERS - 1);

// this structure must be in sync with shader uniforms!
typedef struct vkUniform_s
{
	// light/env parameters:
	vec4_t eyePos; // vertex
	union
	{
		struct
		{
			vec4_t pos;    // vertex: light origin
			vec4_t color;  // fragment: rgb + 1/(r*r)
			vec4_t vector; // fragment: linear dynamic light
		} light;
		struct
		{
			vec4_t color[3]; // ent.color[3]
		} ent;
	};
	// fog parameters:
	vec4_t fogDistanceVector; // vertex
	vec4_t fogDepthVector;    // vertex
	vec4_t fogEyeT;           // vertex
	vec4_t fogColor;          // fragment
} vkUniform_t;

typedef struct dlight_s
{
	vec3_t origin;
	vec3_t origin2;
	vec3_t dir; // origin2 - origin

	vec3_t color; // range from 0.0 to 1.0, should be color normalized
	float radius;

	vec3_t transformed;	 // origin in local coordinate system
	vec3_t transformed2; // origin2 in local coordinate system
	int additive;		 // texture detail is lost tho when the lightmap is dark
	bool linear;
#ifdef USE_PMLIGHT
	struct litSurf_s *head;
	struct litSurf_s *tail;
#endif
} dlight_t;

// a trRefEntity_t has all the information passed in by
// the client game, as well as some locally derived info
typedef struct
{
	refEntity_t e;

	float axisLength; // compensate for non-normalized axis
#ifdef USE_LEGACY_DLIGHTS
	int needDlights; // 1 for bmodels that touch a dlight
#endif
	bool lightingCalculated;
	vec3_t lightDir;	 // normalized direction towards light
	vec3_t ambientLight; // color normalized to 0-255
	std::uint32_t ambientLightInt; // 32 bit rgba packed
	vec3_t directedLight;
#ifdef USE_PMLIGHT
	vec3_t shadowLightDir; // normalized direction towards light
#endif
	bool intShaderTime;
} trRefEntity_t;

typedef struct
{
	vec3_t origin;	   // in world coordinates
	vec3_t axis[3];	   // orientation in world
	vec3_t viewOrigin; // viewParms->or.origin in local coordinates
	float modelMatrix[16];
} orientationr_t;

//===============================================================================

enum class shaderSort_t : uint8_t
{
	SS_BAD,
	SS_PORTAL,		// mirrors, portals, viewscreens
	SS_ENVIRONMENT, // sky box
	SS_OPAQUE,		// opaque

	SS_DECAL,		// scorch marks, etc.
	SS_SEE_THROUGH, // ladders, grates, grills that may have small blended edges
					// in addition to alpha test
	SS_BANNER,

	SS_FOG,

	SS_UNDERWATER, // for items that should be drawn in front of the water plane

	SS_BLEND0, // regular transparency and filters
	SS_BLEND1, // generally only used for additive type effects
	SS_BLEND2,
	SS_BLEND3,

	SS_BLEND6,
	SS_STENCIL_SHADOW,
	SS_ALMOST_NEAREST, // gun smoke puffs

	SS_NEAREST // blood blobs
};

constexpr int MAX_SHADER_STAGES = 8;

enum class genFunc_t : uint8_t
{
	GF_NONE,

	GF_SIN,
	GF_SQUARE,
	GF_TRIANGLE,
	GF_SAWTOOTH,
	GF_INVERSE_SAWTOOTH,

	GF_NOISE

};

enum class deform_t : uint8_t
{
	DEFORM_NONE,
	DEFORM_WAVE,
	DEFORM_NORMALS,
	DEFORM_BULGE,
	DEFORM_MOVE,
	DEFORM_PROJECTION_SHADOW,
	DEFORM_AUTOSPRITE,
	DEFORM_AUTOSPRITE2,
	DEFORM_TEXT0,
	DEFORM_TEXT1,
	DEFORM_TEXT2,
	DEFORM_TEXT3,
	DEFORM_TEXT4,
	DEFORM_TEXT5,
	DEFORM_TEXT6,
	DEFORM_TEXT7
};

enum class alphaGen_t : uint8_t
{
	AGEN_IDENTITY,
	AGEN_SKIP,
	AGEN_ENTITY,
	AGEN_ONE_MINUS_ENTITY,
	AGEN_VERTEX,
	AGEN_ONE_MINUS_VERTEX,
	AGEN_LIGHTING_SPECULAR,
	AGEN_WAVEFORM,
	AGEN_PORTAL,
	AGEN_CONST
};

enum class colorGen_t : uint8_t
{
	CGEN_BAD,
	CGEN_IDENTITY_LIGHTING, // tr.identityLight
	CGEN_IDENTITY,			// always (1,1,1,1)
	CGEN_ENTITY,			// grabbed from entity's modulate field
	CGEN_ONE_MINUS_ENTITY,	// grabbed from 1 - entity.modulate
	CGEN_EXACT_VERTEX,		// tess.vertexColors
	CGEN_VERTEX,			// tess.vertexColors * tr.identityLight
	CGEN_ONE_MINUS_VERTEX,
	CGEN_WAVEFORM, // programmatically generated
	CGEN_LIGHTING_DIFFUSE,
	CGEN_FOG,  // standard fog
	CGEN_CONST // fixed color
};

enum class texCoordGen_t : uint8_t
{
	TCGEN_BAD,
	TCGEN_IDENTITY, // clear to 0,0
	TCGEN_LIGHTMAP,
	TCGEN_TEXTURE,
	TCGEN_ENVIRONMENT_MAPPED,
	TCGEN_ENVIRONMENT_MAPPED_FP, // with correct first-person mapping
	TCGEN_FOG,
	TCGEN_VECTOR // S and T from world coordinates
};

enum class acff_t : uint8_t
{
	ACFF_NONE,
	ACFF_MODULATE_RGB,
	ACFF_MODULATE_RGBA,
	ACFF_MODULATE_ALPHA
};

typedef struct
{
	float base;
	float amplitude;
	float phase;
	float frequency;

	genFunc_t func;
} waveForm_t;

constexpr int TR_MAX_TEXMODS = 4;

enum class texMod_t : uint8_t
{
	TMOD_NONE,
	TMOD_TRANSFORM,
	TMOD_TURBULENT,
	TMOD_SCROLL,
	TMOD_SCALE,
	TMOD_STRETCH,
	TMOD_ROTATE,
	TMOD_ENTITY_TRANSLATE,
	TMOD_OFFSET,
	TMOD_SCALE_OFFSET,
	TMOD_OFFSET_SCALE,
};

constexpr int MAX_SHADER_DEFORMS = 3;
typedef struct
{
	deform_t deformation; // vertex coordinate modification type

	vec3_t moveVector;
	waveForm_t deformationWave;
	float deformationSpread;

	float bulgeWidth;
	float bulgeHeight;
	float bulgeSpeed;
} deformStage_t;

typedef struct
{
	texMod_t type;

	union
	{

		// used for texMod_t::TMOD_TURBULENT and texMod_t::TMOD_STRETCH
		waveForm_t wave;

		// used for texMod_t::TMOD_TRANSFORM
		struct
		{
			float matrix[2][2]; // s' = s * m[0][0] + t * m[1][0] + trans[0]
			float translate[2]; // t' = s * m[0][1] + t * m[0][1] + trans[1]
		};

		// used for texMod_t::TMOD_SCALE, texMod_t::TMOD_OFFSET, texMod_t::TMOD_SCALE_OFFSET
		struct
		{
			float scale[2];	 // s' = s * scale[0] + offset[0]
			float offset[2]; // t' = t * scale[1] + offset[1]
		};

		// used for texMod_t::TMOD_SCROLL
		float scroll[2]; // s' = s + scroll[0] * time
						 // t' = t + scroll[1] * time
		// used for texMod_t::TMOD_ROTATE
		// + = clockwise
		// - = counterclockwise
		float rotateSpeed;
	};

} texModInfo_t;

constexpr int MAX_IMAGE_ANIMATIONS = 24;
constexpr int MAX_IMAGE_ANIMATIONS_VQ3 = 8;

constexpr int LIGHTMAP_INDEX_NONE = 0;
constexpr int LIGHTMAP_INDEX_SHADER = 1;
constexpr int LIGHTMAP_INDEX_OFFSET = 2;

typedef struct
{
	std::array<image_t *, MAX_IMAGE_ANIMATIONS> image;
	int numImageAnimations;
	double imageAnimationSpeed; // -EC- set to double

	texCoordGen_t tcGen;
	vec3_t tcGenVectors[2];

	int numTexMods;
	texModInfo_t *texMods;

	waveForm_t rgbWave;
	colorGen_t rgbGen;

	waveForm_t alphaWave;
	alphaGen_t alphaGen;

	color4ub_t constantColor; // for colorGen_t::CGEN_CONST and alphaGen_t::AGEN_CONST

	acff_t adjustColorsForFog;

	int videoMapHandle;
	int lightmap; // LIGHTMAP_INDEX_NONE, LIGHTMAP_INDEX_SHADER, LIGHTMAP_INDEX_OFFSET
	bool isVideoMap;
	unsigned int 	isScreenMap : 1;
	unsigned int 	dlight : 1;
} textureBundle_t;

#ifdef USE_VULKAN
constexpr int NUM_TEXTURE_BUNDLES = 3;
#else
constexpr int NUM_TEXTURE_BUNDLES = 2;
#endif

typedef struct
{
	bool active;

	textureBundle_t bundle[NUM_TEXTURE_BUNDLES];

	unsigned stateBits; // GLS_xxxx mask
	GLint mtEnv;		// 0, GL_MODULATE, GL_ADD, GL_DECAL
	GLint mtEnv3;		// 0, GL_MODULATE, GL_ADD, GL_DECAL

	bool isDetail;
	bool depthFragment;

#ifdef USE_VULKAN
	uint32_t tessFlags;
	uint32_t numTexBundles;

	uint32_t vk_pipeline[2]; // normal,fogged
	uint32_t vk_mirror_pipeline[2];

	uint32_t vk_pipeline_df; // depthFragment
	uint32_t vk_mirror_pipeline_df;
#endif

#ifdef USE_VBO
	uint32_t rgb_offset[NUM_TEXTURE_BUNDLES]; // within current shader
	uint32_t tex_offset[NUM_TEXTURE_BUNDLES]; // within current shader
#endif

} shaderStage_t;

struct shaderCommands_s;

enum class fogPass_t : uint8_t
{
	FP_NONE,  // surface is translucent and will just be adjusted properly
	FP_EQUAL, // surface is opaque but possibly alpha tested
	FP_LE	  // surface is translucent, but still needs a fog pass (fog surface)
};

typedef struct
{
	float cloudHeight;
	image_t *outerbox[6], *innerbox[6];
} skyParms_t;

typedef struct
{
	vec3_t color;
	float depthForOpaque;
} fogParms_t;

typedef struct shader_s
{
	char name[MAX_QPATH];	 // game path, including extension
	int lightmapSearchIndex; // for a shader to match, both name and lightmapIndex must match
	int lightmapIndex;		 // for rendering

	int index;		 // this shader == tr.shaders[index]
	int sortedIndex; // this shader == tr.sortedShaders[sortedIndex]

	float sort; // lower numbered shaders draw before higher numbered

	bool defaultShader; // we want to return index 0 if the shader failed to
						// load for some reason, but R_FindShader should
						// still keep a name allocated for it, so if
						// something calls RE_RegisterShader again with
						// the same name, we don't try looking for it again

	bool explicitlyDefined; // found in a .shader file

	int surfaceFlags; // if explicitlyDefined, this will have SURF_* flags
	int contentFlags;

	bool entityMergable; // merge across entites optimizable (smoke, blood)

	bool isSky;
	skyParms_t sky;
	fogParms_t fogParms;

	float portalRange; // distance to fog out at
	float portalRangeR;

	bool multitextureEnv; // if shader has multitexture stage(s)

	cullType_t cullType; // cullType_t::CT_FRONT_SIDED, cullType_t::CT_BACK_SIDED, or cullType_t::CT_TWO_SIDED
	bool polygonOffset;	 // set for decals and other items that must be offset

	unsigned noMipMaps : 1; // for console fonts, 2D elements, etc.
	unsigned noPicMip : 1;	// for images that must always be full resolution
	unsigned noLightScale : 1;
	unsigned noVLcollapse : 1; // ignore vertexlight mode

	fogPass_t fogPass; // draw a blended pass, possibly with depth test equals

	bool needsNormal; // not all shaders will need all data to be gathered
	// bool	needsST1;
	bool needsST2;
	// bool	needsColor;

	int numDeforms;
	deformStage_t deforms[MAX_SHADER_DEFORMS];

	int numUnfoggedPasses;
	shaderStage_t *stages[MAX_SHADER_STAGES];

#ifdef USE_PMLIGHT
	int lightingStage;
	int lightingBundle;
#endif
	bool fogCollapse;
	int tessFlags;

#ifdef USE_VBO
	// VBO structures
	bool isStaticShader;
	int svarsSize;
	int iboOffset;
	int vboOffset;
	int normalOffset;
	int numIndexes;
	int numVertexes;
	int curVertexes;
	int curIndexes;
#endif

	int hasScreenMap;

	void (*optimalStageIteratorFunc)(void);

	double clampTime;  // time this shader is clamped to - set to double for frameloss fix -EC-
	double timeOffset; // current time offset for this shader - set to double for frameloss fix -EC-

	struct shader_s *remappedShader; // current shader this one is remapped too

	struct shader_s *next;
} shader_t;

// trRefdef_t holds everything that comes in refdef_t,
// as well as the locally generated scene information
typedef struct
{
	int x, y, width, height;
	float fov_x, fov_y;
	vec3_t vieworg;
	vec3_t viewaxis[3]; // transformation matrix

	stereoFrame_t stereoFrame;

	int time;	 // time in milliseconds for shader effects and other time dependent rendering issues
	int rdflags; // RDF_NOWORLDMODEL, etc

	// 1 bits will prevent the associated area from rendering at all
	byte areamask[MAX_MAP_AREA_BYTES];
	bool areamaskModified; // qtrue if areamask changed since last scene

	double floatTime; // tr.refdef.time / 1000.0 -EC- set to double

	// text messages for deform text shaders
	char text[MAX_RENDER_STRINGS][MAX_RENDER_STRING_LENGTH];

	int num_entities;
	trRefEntity_t *entities;

	unsigned int num_dlights;
	struct dlight_s *dlights;

	int numPolys;
	struct srfPoly_s *polys;

	int numDrawSurfs;
	struct drawSurf_s *drawSurfs;
#ifdef USE_PMLIGHT
	int numLitSurfs;
	struct litSurf_s *litSurfs;
#endif
#ifdef USE_VULKAN
	bool switchRenderPass;
	bool needScreenMap;
#endif
} trRefdef_t;

typedef struct image_s
{
	char *imgName;		  // image path, including extension
	char *imgName2;		  // image path with real file extension
	struct image_s *next; // for hash search
	int width, height;	  // source image
	int uploadWidth;	  // after power of two and picmip but not including clamp to MAX_TEXTURE_SIZE
	int uploadHeight;
	imgFlags_t flags;
	int frameUsed; // for texture usage in frame statistics

#ifdef USE_VULKAN
	vk::Format internalFormat;

	vk::SamplerAddressMode wrapClampMode;
	vk::Image handle;
	vk::ImageView view;
	// Descriptor set that contains single descriptor used to access the given image.
	// It is updated only once during image initialization.
	vk::DescriptorSet descriptor;
#else
	GLuint texnum; // gl texture binding
	GLint internalFormat;
	int TMU; // only needed for voodoo2
#endif

} image_t;

//=================================================================================

// max surfaces per-skin
// This is an arbitrary limit. Vanilla Q3 only supported 32 surfaces in skins but failed to
// enforce the maximum limit when reading skin files. It was possile to use more than 32
// surfaces which accessed out of bounds memory past end of skin->surfaces hunk block.
constexpr int MAX_SKIN_SURFACES = 256;

// skins allow models to be retextured without modifying the model file
typedef struct
{
	char name[MAX_QPATH];
	shader_t *shader;
} skinSurface_t;

typedef struct skin_s
{
	char name[MAX_QPATH]; // game path, including extension
	int numSurfaces;
	skinSurface_t *surfaces; // dynamically allocated array of surfaces
} skin_t;

typedef struct
{
	int originalBrushNumber;
	vec3_t bounds[2];

	color4ub_t colorInt; // in packed byte format
	vec4_t color;
	float tcScale; // texture coordinate vector scales
	fogParms_t parms;

	// for clipping distance in fog when outside
	bool hasSurface;
	float surface[4];
} fog_t;

typedef struct
{
	float eyeT;
	bool eyeOutside;
	vec4_t fogDistanceVector;
	vec4_t fogDepthVector;
	const float *fogColor; // vec4_t
} fogProgramParms_t;

enum class portalView_t : uint8_t
{
	PV_NONE = 0,
	PV_PORTAL, // this view is through a portal
	PV_MIRROR, // portal + inverted face culling
	PV_COUNT
};

typedef struct
{
	orientationr_t ort;
	orientationr_t world;
	vec3_t pvsOrigin; // may be different than or.origin for portals
	portalView_t portalView;
	int frameSceneNum;	  // copied from tr.frameSceneNum
	int frameCount;		  // copied from tr.frameCount
	cplane_t portalPlane; // clip anything behind this if mirroring
	int viewportX, viewportY, viewportWidth, viewportHeight;
	int scissorX, scissorY, scissorWidth, scissorHeight;
	float fovX, fovY;
	float projectionMatrix[16];
	cplane_t frustum[5];
	vec3_t visBounds[2];
	float zFar;
	stereoFrame_t stereoFrame;
#ifdef USE_PMLIGHT
	// each view will have its own dlight set
	unsigned int num_dlights;
	struct dlight_s *dlights;
#endif
} viewParms_t;

/*
==============================================================================

SURFACES

==============================================================================
*/

// any changes in surfaceType must be mirrored in rb_surfaceTable[]
enum class surfaceType_t : uint32_t
{
	SF_BAD,
	SF_SKIP, // ignore
	SF_FACE,
	SF_GRID,
	SF_TRIANGLES,
	SF_POLY,
	SF_MD3,
	SF_MDR,
	SF_IQM,
	SF_FLARE,
	SF_ENTITY, // beams, rails, lightning, etc that can be determined by entity

	SF_NUM_SURFACE_TYPES,
	SF_MAX = 0x7fffffff // ensures that sizeof( surfaceType_t ) == sizeof( int )
};

typedef struct drawSurf_s
{
	unsigned int sort;		// bit combination for fast compares
	surfaceType_t *surface; // any of surface*_t
} drawSurf_t;

#ifdef USE_PMLIGHT
typedef struct litSurf_s
{
	unsigned int sort;		// bit combination for fast compares
	surfaceType_t *surface; // any of surface*_t
	struct litSurf_s *next;
} litSurf_t;
#endif

constexpr int MAX_FACE_POINTS = 64;

constexpr int MAX_PATCH_SIZE = 32; // max dimensions of a patch mesh in map file
constexpr int MAX_GRID_SIZE = 65;  // max dimensions of a grid mesh in memory

// when cgame directly specifies a polygon, it becomes a srfPoly_t
// as soon as it is called
typedef struct srfPoly_s
{
	surfaceType_t surfaceType;
	qhandle_t hShader;
	int fogIndex;
	int numVerts;
	polyVert_t *verts;
} srfPoly_t;

typedef struct srfFlare_s
{
	surfaceType_t surfaceType;
	vec3_t origin;
	vec3_t normal;
	vec3_t color;
} srfFlare_t;

typedef struct srfGridMesh_s
{
	surfaceType_t surfaceType;

	// dynamic lighting information
	int dlightBits;

	// culling information
	vec3_t meshBounds[2];
	vec3_t localOrigin;
	float meshRadius;

	// lod information, which may be different
	// than the culling information to allow for
	// groups of curves that LOD as a unit
	vec3_t lodOrigin;
	float lodRadius;
	int lodFixed;
	int lodStitched;
#ifdef USE_VBO
	int vboItemIndex;
	int vboExpectIndices;
	int vboExpectVertices;
#endif
	// vertexes
	int width, height;
	float *widthLodError;
	float *heightLodError;
	drawVert_t verts[1]; // variable sized
} srfGridMesh_t;

constexpr int VERTEXSIZE = 8;
typedef struct
{
	surfaceType_t surfaceType;
	cplane_t plane;

	// dynamic lighting information
#ifdef USE_LEGACY_DLIGHTS
	int dlightBits;
#endif
#ifdef USE_VBO
	int vboItemIndex;
#endif
	float *normals;

	// triangle definitions (no normals at points)
	int numPoints;
	int numIndices;
	int ofsIndices;
	float points[1][VERTEXSIZE]; // variable sized
								 // there is a variable length list of indices here also
} srfSurfaceFace_t;

// misc_models in maps are turned into direct geometry by q3map
typedef struct
{
	surfaceType_t surfaceType;

	// dynamic lighting information
#ifdef USE_LEGACY_DLIGHTS
	int dlightBits;
#endif
#ifdef USE_VBO
	int vboItemIndex;
#endif

	// culling information (FIXME: use this!)
	vec3_t bounds[2];
	vec3_t localOrigin;
	float radius;

	// triangle definitions
	int numIndexes;
	int *indexes;

	int numVerts;
	drawVert_t *verts;
} srfTriangles_t;

typedef struct
{
	vec3_t translate;
	quat_t rotate;
	vec3_t scale;
} iqmTransform_t;

// inter-quake-model
typedef struct
{
	int num_vertexes;
	int num_triangles;
	int num_frames;
	int num_surfaces;
	int num_joints;
	int num_poses;
	struct srfIQModel_s *surfaces;

	int *triangles;

	// vertex arrays
	float *positions;
	float *texcoords;
	float *normals;
	float *tangents;
	byte *colors;
	int *influences; // [num_vertexes] indexes into influenceBlendVertexes

	// unique list of vertex blend indexes/weights for faster CPU vertex skinning
	byte *influenceBlendIndexes; // [num_influences]
	union
	{
		float *f;
		byte *b;
	} influenceBlendWeights; // [num_influences]

	// depending upon the exporter, blend indices and weights might be int/float
	// as opposed to the recommended byte/byte, for example Noesis exports
	// int/float whereas the official IQM tool exports byte/byte
	int blendWeightsType; // IQM_UBYTE or IQM_FLOAT

	char *jointNames;
	int *jointParents;
	float *bindJoints;	   // [num_joints * 12]
	float *invBindJoints;  // [num_joints * 12]
	iqmTransform_t *poses; // [num_frames * num_poses]
	float *bounds;
} iqmData_t;

// inter-quake-model surface
typedef struct srfIQModel_s
{
	surfaceType_t surfaceType;
	char name[MAX_QPATH];
	shader_t *shader;
	iqmData_t *data;
	int first_vertex, num_vertexes;
	int first_triangle, num_triangles;
	int first_influence, num_influences;
} srfIQModel_t;

extern void (*rb_surfaceTable[static_cast<uint32_t>(surfaceType_t::SF_NUM_SURFACE_TYPES)])(void *);

/*
==============================================================================

BRUSH MODELS

==============================================================================
*/

//
// in memory representation
//

constexpr int SIDE_FRONT = 0;
constexpr int SIDE_BACK = 1;
constexpr int SIDE_ON = 2;

typedef struct msurface_s
{
	int viewCount; // if == tr.viewCount, already added
	struct shader_s *shader;
	int fogIndex;
#ifdef USE_PMLIGHT
	int vcVisible;		 // if == tr.viewCount, is actually VISIBLE in this frame, i.e. passed facecull and has been added to the drawsurf list
	int lightCount;		 // if == tr.lightCount, already added to the litsurf list for the current light
#endif					 // USE_PMLIGHT
	surfaceType_t *data; // any of srf*_t
} msurface_t;

typedef struct mnode_s
{
	// common with leaf and node
	int contents;	   // -1 for nodes, to differentiate from leafs
	int visframe;	   // node needs to be traversed if current
	vec3_t mins, maxs; // for bounding box culling
	struct mnode_s *parent;

	// node specific
	cplane_t *plane;
	struct mnode_s *children[2];

	// leaf specific
	int cluster;
	int area;

	msurface_t **firstmarksurface;
	int nummarksurfaces;
} mnode_t;

typedef struct
{
	vec3_t bounds[2]; // for culling
	msurface_t *firstSurface;
	int numSurfaces;
} bmodel_t;

typedef struct
{
	std::array<char, MAX_QPATH> name;	  // ie: maps/tim_dm2.bsp
	std::array<char, MAX_QPATH> baseName; // ie: tim_dm2

	int dataSize;

	int numShaders;
	dshader_t *shaders;

	bmodel_t *bmodels;

	int numplanes;
	cplane_t *planes;

	int numnodes; // includes leafs
	int numDecisionNodes;
	mnode_t *nodes;

	int numsurfaces;
	msurface_t *surfaces;

	int nummarksurfaces;
	msurface_t **marksurfaces;

	int numfogs;
	fog_t *fogs;

	vec3_t lightGridOrigin;
	vec3_t lightGridSize;
	vec3_t lightGridInverseSize;
	int lightGridBounds[3];
	byte *lightGridData;

	int numClusters;
	int clusterBytes;
	const byte *vis; // may be passed in by CM_LoadMap to save space

	byte *novis; // clusterBytes of 0xff

	char *entityString;
	const char *entityParsePoint;
} world_t;

//======================================================================

enum class modtype_t : uint8_t
{
	MOD_BAD,
	MOD_BRUSH,
	MOD_MESH,
	MOD_MDR,
	MOD_IQM
};

typedef struct model_s
{
	std::array<char, MAX_QPATH> name;
	modtype_t type;
	int index; // model = tr.models[model->index]

	int dataSize;					// just for listing purposes
	bmodel_t *bmodel;				// only if type == modtype_t::MOD_BRUSH
	md3Header_t *md3[MD3_MAX_LODS]; // only if type == modtype_t::MOD_MESH
	void *modelData;				// only if type == (modtype_t::MOD_MDR | modtype_t::MOD_IQM)

	int numLods;
} model_t;

constexpr int MAX_MOD_KNOWN = 1024;

//====================================================

constexpr int MAX_DRAWIMAGES = 2048;
constexpr int MAX_SKINS = 1024;

constexpr int DRAWSURF_MASK(MAX_DRAWSURFS - 1);

/*

the drawsurf sort data is packed into a single 32 bit value so it can be
compared quickly during the qsorting process

the bits are allocated as follows:

0 - 1	: dlightmap index
//2		: used to be clipped flag REMOVED - 03.21.00 rad
2 - 6	: fog index
11 - 20	: entity index
21 - 31	: sorted shader index

	TTimo - 1.32
0-1   : dlightmap index
2-6   : fog index
7-16  : entity index
17-30 : sorted shader index
*/
constexpr int DLIGHT_BITS = 1; // bool in opengl1 renderer
constexpr int DLIGHT_MASK = ((1 << DLIGHT_BITS) - 1);
constexpr int FOGNUM_BITS = 5;
constexpr int FOGNUM_MASK = ((1 << FOGNUM_BITS) - 1);
constexpr int QSORT_FOGNUM_SHIFT = DLIGHT_BITS;
constexpr int QSORT_REFENTITYNUM_SHIFT = (QSORT_FOGNUM_SHIFT + FOGNUM_BITS);
constexpr int QSORT_SHADERNUM_SHIFT = (QSORT_REFENTITYNUM_SHIFT + REFENTITYNUM_BITS);
#if (QSORT_SHADERNUM_SHIFT + SHADERNUM_BITS) > 32
#error "Need to update sorting, too many bits."
#endif
constexpr int QSORT_REFENTITYNUM_MASK = (REFENTITYNUM_MASK << QSORT_REFENTITYNUM_SHIFT);

extern int gl_filter_min, gl_filter_max;

/*
** performanceCounters_t
*/
typedef struct
{
	int c_sphere_cull_patch_in, c_sphere_cull_patch_clip, c_sphere_cull_patch_out;
	int c_box_cull_patch_in, c_box_cull_patch_clip, c_box_cull_patch_out;
	int c_sphere_cull_md3_in, c_sphere_cull_md3_clip, c_sphere_cull_md3_out;
	int c_box_cull_md3_in, c_box_cull_md3_clip, c_box_cull_md3_out;

	int c_leafs;
	int c_dlightSurfaces;
	int c_dlightSurfacesCulled;
#ifdef USE_PMLIGHT
	int c_light_cull_out;
	int c_light_cull_in;
	int c_lit_leafs;
	int c_lit_surfs;
	int c_lit_culls;
	int c_lit_masks;
#endif
} frontEndCounters_t;

constexpr int FOG_TABLE_SIZE = 256;
constexpr int FUNCTABLE_SIZE = 1024;
constexpr int FUNCTABLE_SIZE2 = 10;
constexpr int FUNCTABLE_MASK = (FUNCTABLE_SIZE - 1);

// the renderer front end should never modify glstate_t
typedef struct
{
	GLuint currenttextures[MAX_TEXTURE_UNITS];
	int currenttmu;
	bool finishCalled;
	GLint texEnv[2];
	cullType_t faceCulling;
	unsigned glStateBits;
	unsigned glClientStateBits[MAX_TEXTURE_UNITS];
	int currentArray;
} glstate_t;

typedef struct glstatic_s
{
	// unmodified width/height according to actual \r_mode*
	uint32_t windowWidth;
	uint32_t windowHeight;
	int captureWidth;
	int captureHeight;
	int initTime;
	bool deviceSupportsGamma;
} glstatic_t;

typedef struct
{
	int c_surfaces, c_shaders, c_vertexes, c_indexes, c_totalIndexes;
	float c_overDraw;

	int c_dlightVertexes;
	int c_dlightIndexes;

	int c_flareAdds;
	int c_flareTests;
	int c_flareRenders;

	int msec; // total msec for backend run
#ifdef USE_PMLIGHT
	int c_lit_batches;
	int c_lit_vertices;
	int c_lit_indices;
	int c_lit_indices_latecull_in;
	int c_lit_indices_latecull_out;
	int c_lit_vertices_lateculltest;
#endif
} backEndCounters_t;

typedef struct videoFrameCommand_s
{
	int commandId;
	int width;
	int height;
	byte *captureBuffer;
	byte *encodeBuffer;
	bool motionJpeg;
} videoFrameCommand_t;

enum
{
	SCREENSHOT_TGA = 1 << 0,
	SCREENSHOT_JPG = 1 << 1,
	SCREENSHOT_BMP = 1 << 2,
	SCREENSHOT_BMP_CLIPBOARD = 1 << 3,
	SCREENSHOT_AVI = 1 << 4 // take video frame
};

// all state modified by the back end is separated
// from the front end state
typedef struct
{
	trRefdef_t refdef;
	viewParms_t viewParms;
	orientationr_t ort;
	backEndCounters_t pc;
	bool isHyperspace;
	const trRefEntity_t *currentEntity;
	bool skyRenderedThisView; // flag for drawing sun

	bool projection2D; // if qtrue, drawstretchpic doesn't need to change modes
	color4ub_t color2D;
	bool doneSurfaces;		// done any 3d surfaces already
	trRefEntity_t entity2D; // currentEntity will point at this when doing 2D rendering

	int screenshotMask; // tga | jpg | bmp
	char screenshotTGA[MAX_OSPATH];
	char screenshotJPG[MAX_OSPATH];
	char screenshotBMP[MAX_OSPATH];
	bool screenShotTGAsilent;
	bool screenShotJPGsilent;
	bool screenShotBMPsilent;
	videoFrameCommand_t vcmd; // avi capture

	bool throttle;
	bool drawConsole;
	bool doneShadows;

	bool screenMapDone;
	bool doneBloom;

} backEndState_t;

typedef struct drawSurfsCommand_s drawSurfsCommand_t;

enum class renderCommand_t : int8_t
{
	RC_END_OF_LIST,
	RC_SET_COLOR,
	RC_STRETCH_PIC,
	RC_DRAW_SURFS,
	RC_DRAW_BUFFER,
	RC_SWAP_BUFFERS,
	RC_FINISHBLOOM,
	RC_COLORMASK,
	RC_CLEARDEPTH,
	RC_CLEARCOLOR
};

/*
** trGlobals_t
**
** Most renderer globals are defined here.
** backend functions should never modify any of these fields,
** but may read fields that aren't dynamically modified
** by the frontend.
*/
struct trGlobals_t
{
	bool registered; // cleared at shutdown, set at beginRegistration
	bool inited;	 // cleared at shutdown, set at InitOpenGL

	int visCount;	// incremented every time a new vis cluster is entered
	int frameCount; // incremented every frame
	int sceneCount; // incremented every scene
	int viewCount;	// incremented every view (twice a scene if portaled)
					// and every R_MarkFragments call
#ifdef USE_PMLIGHT
	int lightCount; // incremented for each dlight in the view
#endif

	int frameSceneNum; // zeroed at RE_BeginFrame

	bool worldMapLoaded;
	world_t *world;

	const byte *externalVisData; // from RE_SetWorldVisData, shared with CM_Load

	image_t *defaultImage;
	image_t *scratchImage[MAX_VIDEO_HANDLES];
	image_t *fogImage;
	image_t *dlightImage; // inverse-quare highlight for projective adding
	image_t *flareImage;
	image_t *blackImage;
	image_t *whiteImage;		 // full of 0xff
	image_t *identityLightImage; // full of tr.identityLightByte

	shader_t *defaultShader;
	shader_t *whiteShader;
	shader_t *cinematicShader;
	shader_t *shadowShader;
	shader_t *projectionShadowShader;

	shader_t *flareShader;
	shader_t *sunShader;

	int numLightmaps;
	image_t **lightmaps;

	bool mergeLightmaps;
	float lightmapOffset[2]; // current shader lightmap offset
	float lightmapScale[2];	 // for lightmap atlases
	int lightmapMod;		 // for lightmap atlases

	trRefEntity_t *currentEntity;
	trRefEntity_t worldEntity; // point currentEntity at this when rendering world
	int currentEntityNum;
	int shiftedEntityNum; // currentEntityNum << QSORT_REFENTITYNUM_SHIFT
	model_t *currentModel;

	viewParms_t viewParms;

	float identityLight;   // 1.0 / ( 1 << overbrightBits )
	int identityLightByte; // identityLight * 255
	int overbrightBits;	   // r_overbrightBits->integer, but set to 0 if no hw gamma

	orientationr_t ort; // for current entity

	trRefdef_t refdef;

	int viewCluster;
#ifdef USE_PMLIGHT
	dlight_t *light; // current light during R_RecursiveLightNode
#endif
	vec3_t sunLight; // from the sky shader for this level
	vec3_t sunDirection;

	frontEndCounters_t pc;
	int frontEndMsec; // not in pc due to clearing issue

	//
	// put large tables at the end, so most elements will be
	// within the +/32K indexed range on risc processors
	//
	model_t *models[MAX_MOD_KNOWN];
	int numModels;

	int numImages;
	image_t *images[MAX_DRAWIMAGES];

	// shader indexes from other modules will be looked up in tr.shaders[]
	// shader indexes from drawsurfs will be looked up in sortedShaders[]
	// lower indexed sortedShaders must be rendered first (opaque surfaces before translucent)
	int numShaders;
	shader_t *shaders[MAX_SHADERS];
	shader_t *sortedShaders[MAX_SHADERS];

	int numSkins;
	skin_t *skins[MAX_SKINS];

	std::array<float, FOG_TABLE_SIZE> fogTable{};

	alignas(64) std::array<float, FUNCTABLE_SIZE> sinTable{};
	alignas(64) std::array<float, FUNCTABLE_SIZE> squareTable{};
	alignas(64) std::array<float, FUNCTABLE_SIZE> triangleTable{};
	alignas(64) std::array<float, FUNCTABLE_SIZE> sawToothTable{};
	alignas(64) std::array<float, FUNCTABLE_SIZE> inverseSawToothTable{};
	//alignas(64) std::array<float, FOG_TABLE_SIZE>  fogTable{};

	bool mapLoading;

	int needScreenMap;
#ifdef USE_VULKAN
	drawSurfsCommand_t *drawSurfCmd;
	int numDrawSurfCmds;
	renderCommand_t lastRenderCommand;
	int numFogs; // read before parsing shaders
#endif

	bool vertexLightingAllowed;
};

extern backEndState_t backEnd;
extern trGlobals_t tr;

extern int gl_clamp_mode;

extern glstate_t glState; // outside of TR since it shouldn't be cleared during ref re-init

extern glstatic_t gls;

// extern void myGlMultMatrix(const float *a, const float *b, float *out);

//
// cvars
//
extern cvar_t *r_flareSize;
extern cvar_t *r_flareFade;
extern cvar_t *r_flareCoeff; // coefficient for the flare intensity falloff function.

extern cvar_t *r_railWidth;
extern cvar_t *r_railCoreWidth;
extern cvar_t *r_railSegmentLength;

extern cvar_t *r_znear;			   // near Z clip plane
extern cvar_t *r_zproj;			   // z distance of projection plane
extern cvar_t *r_stereoSeparation; // separation of cameras for stereo rendering

extern cvar_t *r_lodbias; // push/pull LOD transitions
extern cvar_t *r_lodscale;

extern cvar_t *r_teleporterFlash; // teleport hyperspace visual

extern cvar_t *r_fastsky;	   // controls whether sky should be cleared or drawn
extern cvar_t *r_neatsky;	   // nomip and nopicmip for skyboxes, cnq3 like look
extern cvar_t *r_drawSun;	   // controls drawing of sun quad
extern cvar_t *r_dynamiclight; // dynamic lights enabled/disabled
extern cvar_t *r_mergeLightmaps;
#ifdef USE_PMLIGHT
extern cvar_t *r_dlightMode; // 0 - vq3, 1 - pmlight
// extern cvar_t	*r_dlightSpecPower;		// 1 - 32
// extern cvar_t	*r_dlightSpecColor;		// -1.0 - 1.0
extern cvar_t *r_dlightScale;	  // 0.1 - 1.0
extern cvar_t *r_dlightIntensity; // 0.1 - 1.0
#endif
extern cvar_t *r_dlightSaturation; // 0.0 - 1.0
#ifdef USE_VULKAN
extern cvar_t *r_device;
#ifdef USE_VBO
extern cvar_t *r_vbo;
#endif
extern cvar_t *r_fbo;
extern cvar_t *r_hdr;
extern cvar_t *r_bloom;
extern cvar_t *r_bloom_threshold;
extern cvar_t *r_bloom_intensity;
extern cvar_t *r_bloom_threshold_mode;
extern cvar_t *r_bloom_modulate;
extern cvar_t *r_ext_multisample;
extern cvar_t *r_ext_supersample;
// extern cvar_t	*r_ext_alpha_to_coverage;
extern cvar_t *r_renderWidth;
extern cvar_t *r_renderHeight;
extern cvar_t *r_renderScale;
#endif

extern cvar_t *r_dlightBacks; // dlight non-facing surfaces for continuity

extern cvar_t *r_norefresh;		 // bypasses the ref rendering
extern cvar_t *r_drawentities;	 // disable/enable entity rendering
extern cvar_t *r_drawworld;		 // disable/enable world rendering
extern cvar_t *r_speeds;		 // various levels of information display
extern cvar_t *r_detailTextures; // enables/disables detail texturing stages
extern cvar_t *r_novis;			 // disable/enable usage of PVS
extern cvar_t *r_nocull;
extern cvar_t *r_facePlaneCull; // enables culling of planar surfaces with back side test
extern cvar_t *r_nocurves;
extern cvar_t *r_showcluster;

extern cvar_t *r_gamma;

extern cvar_t *r_nobind;	   // turns off binding to appropriate textures
extern cvar_t *r_singleShader; // make most world faces use default shader
extern cvar_t *r_roundImagesDown;
extern cvar_t *r_colorMipLevels; // development aid to see texture mip usage
extern cvar_t *r_picmip;		 // controls picmip values
extern cvar_t *r_nomip;			 // apply picmip only on worldspawn textures
extern cvar_t *r_finish;
extern cvar_t *r_textureMode;
extern cvar_t *r_offsetFactor;
extern cvar_t *r_offsetUnits;

extern cvar_t *r_fullbright;  // avoid lightmap pass
extern cvar_t *r_lightmap;	  // render lightmaps only
extern cvar_t *r_vertexLight; // vertex lighting mode for better performance

extern cvar_t *r_showtris;	  // enables wireframe rendering of the world
extern cvar_t *r_showsky;	  // forces sky in front of all surfaces
extern cvar_t *r_shownormals; // draws wireframe normals
extern cvar_t *r_clear;		  // force screen clear every frame

extern cvar_t *r_shadows; // controls shadows: 0 = none, 1 = blur, 2 = stencil, 3 = black planar projection
extern cvar_t *r_flares;  // light flares

extern cvar_t *r_intensity;

extern cvar_t *r_lockpvs;
extern cvar_t *r_noportals;
extern cvar_t *r_portalOnly;

extern cvar_t *r_subdivisions;
extern cvar_t *r_lodCurveError;
extern cvar_t *r_skipBackEnd;

extern cvar_t *r_greyscale;
extern cvar_t *r_dither;
extern cvar_t *r_presentBits;

extern cvar_t *r_ignoreGLErrors;

extern cvar_t *r_overBrightBits;
extern cvar_t *r_mapOverBrightBits;
extern cvar_t *r_mapGreyScale;

extern cvar_t *r_debugSurface;
extern cvar_t *r_simpleMipMaps;

extern cvar_t *r_showImages;
extern cvar_t *r_defaultImage;
extern cvar_t *r_debugSort;

extern cvar_t *r_printShaders;

extern cvar_t *r_marksOnTriangleMeshes;

//====================================================================

void R_SwapBuffers(int);

void R_AddNullModelSurfaces(trRefEntity_t *e);
void R_AddBeamSurfaces(trRefEntity_t *e);
void R_AddRailSurfaces(trRefEntity_t *e, bool isUnderwater);
void R_AddLightningBoltSurfaces(trRefEntity_t *e);

constexpr int CULL_IN = 0;	 // completely unclipped
constexpr int CULL_CLIP = 1; // clipped by one or more planes
constexpr int CULL_OUT = 2;	 // completely outside the clipping planes
/*
** GL wrapper/helper functions
*/

void CheckErrors(void);

constexpr int GLS_SRCBLEND_ZERO = 0x00000001;
constexpr int GLS_SRCBLEND_ONE = 0x00000002;
constexpr int GLS_SRCBLEND_DST_COLOR = 0x00000003;
constexpr int GLS_SRCBLEND_ONE_MINUS_DST_COLOR = 0x00000004;
constexpr int GLS_SRCBLEND_SRC_ALPHA = 0x00000005;
constexpr int GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA = 0x00000006;
constexpr int GLS_SRCBLEND_DST_ALPHA = 0x00000007;
constexpr int GLS_SRCBLEND_ONE_MINUS_DST_ALPHA = 0x00000008;
constexpr int GLS_SRCBLEND_ALPHA_SATURATE = 0x00000009;
constexpr int GLS_SRCBLEND_BITS = 0x0000000f;

constexpr int GLS_DSTBLEND_ZERO = 0x00000010;
constexpr int GLS_DSTBLEND_ONE = 0x00000020;
constexpr int GLS_DSTBLEND_SRC_COLOR = 0x00000030;
constexpr int GLS_DSTBLEND_ONE_MINUS_SRC_COLOR = 0x00000040;
constexpr int GLS_DSTBLEND_SRC_ALPHA = 0x00000050;
constexpr int GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA = 0x00000060;
constexpr int GLS_DSTBLEND_DST_ALPHA = 0x00000070;
constexpr int GLS_DSTBLEND_ONE_MINUS_DST_ALPHA = 0x00000080;
constexpr int GLS_DSTBLEND_BITS = 0x000000f0;

constexpr int GLS_BLEND_BITS = (GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS);

constexpr int GLS_DEPTHMASK_TRUE = 0x00000100;

constexpr int GLS_POLYMODE_LINE = 0x00000200;

constexpr int GLS_DEPTHTEST_DISABLE = 0x00000400;
constexpr int GLS_DEPTHFUNC_EQUAL = 0x00000800;

constexpr int GLS_ATEST_GT_0 = 0x00001000;
constexpr int GLS_ATEST_LT_80 = 0x00002000;
constexpr int GLS_ATEST_GE_80 = 0x00003000;
constexpr int GLS_ATEST_BITS = 0x00003000;

constexpr int GLS_DEFAULT = GLS_DEPTHMASK_TRUE;

// vertex array states

constexpr int CLS_NONE = 0x00000000;
constexpr int CLS_COLOR_ARRAY = 0x00000001;
constexpr int CLS_TEXCOORD_ARRAY = 0x00000002;
constexpr int CLS_NORMAL_ARRAY = 0x00000004;

constexpr vec4_t colorBlackCxpr = {0, 0, 0, 1};

void R_Init(void);

const void *RB_TakeVideoFrameCmd(const void *data);

//
// tr_shader.c
//

/*
====================================================================

TESSELATOR/SHADER DECLARATIONS

====================================================================
*/

typedef struct stageVars
{
	color4ub_t colors[NUM_TEXTURE_BUNDLES][SHADER_MAX_VERTEXES]; // we need at least 2xSHADER_MAX_VERTEXES for shadows and normals
	vec2_t texcoords[NUM_TEXTURE_BUNDLES][SHADER_MAX_VERTEXES];
	vec2_t *texcoordPtr[NUM_TEXTURE_BUNDLES];
} stageVars_t;

typedef struct shaderCommands_s
{
#pragma pack(push, 16)
	glIndex_t indexes[SHADER_MAX_INDEXES] QALIGN(16);
	vec4_t xyz[SHADER_MAX_VERTEXES * 2] QALIGN(16); // 2x needed for shadows
	vec4_t normal[SHADER_MAX_VERTEXES] QALIGN(16);
	vec2_t texCoords[2][SHADER_MAX_VERTEXES] QALIGN(16);
	vec2_t texCoords00[SHADER_MAX_VERTEXES] QALIGN(16);
	color4ub_t vertexColors[SHADER_MAX_VERTEXES] QALIGN(16);
#ifdef USE_LEGACY_DLIGHTS
	int vertexDlightBits[SHADER_MAX_VERTEXES] QALIGN(16);
#endif
	stageVars_t svars QALIGN(16);

	color4ub_t constantColor255[SHADER_MAX_VERTEXES] QALIGN(16);
#pragma pack(pop)

#ifdef USE_VBO
	surfaceType_t surfType;
	int vboIndex;
	int vboStage;
	bool allowVBO;
#endif

	shader_t *shader;
	double shaderTime; // -EC- set to double for frameloss fix
	int fogNum;
#ifdef USE_LEGACY_DLIGHTS
	int dlightBits; // or together of all vertexDlightBits
#endif
	int numIndexes;
	int numVertexes;

#ifdef USE_PMLIGHT
	const dlight_t *light;
	bool dlightPass;
	bool dlightUpdateParams;
#endif

#ifdef USE_VULKAN
	Vk_Depth_Range depthRange;
#endif

	// info extracted from current shader
#ifdef USE_TESS_NEEDS_NORMAL
	int needsNormal;
#endif
#ifdef USE_TESS_NEEDS_ST2
	int needsST2;
#endif

	int numPasses;
	shaderStage_t **xstages;

} shaderCommands_t;

extern shaderCommands_t tess;

void RB_ShowImages(void);

/*
============================================================

WORLD MAP

============================================================
*/

/*
============================================================

LIGHTS

============================================================
*/

void R_DrawElements(int numIndexes, const glIndex_t *indexes);

/*
============================================================

CURVE TESSELATION

============================================================
*/

#define PATCH_STITCHING

/*
============================================================

MARKERS, POLYGON PROJECTION ON WORLD POLYGONS

============================================================
*/

/*
============================================================

SCENE GENERATION

============================================================
*/

/*
=============================================================

UNCOMPRESSING BONES

=============================================================
*/

constexpr int MC_BITS_X = 16;
constexpr int MC_BITS_Y = 16;
constexpr int MC_BITS_Z = 16;
constexpr int MC_BITS_VECT = 16;

constexpr int MC_SCALE_X = (1.0f / 64);
constexpr int MC_SCALE_Y = (1.0f / 64);
constexpr int MC_SCALE_Z = (1.0f / 64);

/*
=============================================================
=============================================================
*/

/*
=============================================================

RENDERER BACK END FUNCTIONS

=============================================================
*/

/*
=============================================================

RENDERER BACK END COMMAND QUEUE

=============================================================
*/

constexpr int MAX_RENDER_COMMANDS = 0x80000;

typedef struct
{
	byte cmds[MAX_RENDER_COMMANDS];
	int used;
} renderCommandList_t;

typedef struct
{
	renderCommand_t commandId;
	float color[4];
} setColorCommand_t;

typedef struct
{
	renderCommand_t commandId;
	int buffer;
} drawBufferCommand_t;

typedef struct
{
	int commandId;
	image_t *image;
	int width;
	int height;
	void *data;
} subImageCommand_t;

typedef struct
{
	renderCommand_t commandId;
} swapBuffersCommand_t;

typedef struct
{
	renderCommand_t commandId;
} finishBloomCommand_t;

typedef struct
{
	renderCommand_t commandId;
	shader_t *shader;
	float x, y;
	float w, h;
	float s1, t1;
	float s2, t2;
} stretchPicCommand_t;

typedef struct drawSurfsCommand_s
{
	renderCommand_t commandId;
	trRefdef_t refdef;
	viewParms_t viewParms;
	drawSurf_t *drawSurfs;
	int numDrawSurfs;
} drawSurfsCommand_t;

typedef struct
{
	int commandId;

	GLboolean rgba[4];
} colorMaskCommand_t;

typedef struct
{
	int commandId;
} clearDepthCommand_t;

typedef struct
{
	renderCommand_t commandId;
} clearColorCommand_t;

// these are sort of arbitrary limits.
// the limits apply to the sum of all scenes in a frame --
// the main view, all the 3D icons, etc
constexpr int MAX_POLYS = 8192;
constexpr int MAX_POLYVERTS = 32768;

// all of the information needed by the back end must be
// contained in a backEndData_t
typedef struct
{
	drawSurf_t drawSurfs[MAX_DRAWSURFS];
#ifdef USE_PMLIGHT
	litSurf_t litSurfs[MAX_LITSURFS];
	dlight_t dlights[MAX_REAL_DLIGHTS];
#else
	dlight_t dlights[MAX_DLIGHTS];
#endif

	trRefEntity_t entities[MAX_REFENTITIES];
	srfPoly_t *polys;	   //[MAX_POLYS];
	polyVert_t *polyVerts; //[MAX_POLYVERTS];
	renderCommandList_t commands;
} backEndData_t;

extern int max_polys;
extern int max_polyverts;

extern backEndData_t *backEndData;

void RB_TakeScreenshot(const int x, const int y, const int width, const int height, const char *fileName);
void RB_TakeScreenshotJPEG(const int x, const int y, const int width, const int height, const char *fileName);
void RB_TakeScreenshotBMP(const int x, const int y, const int width, const int height, const char *fileName, const int clipboardOnly);

#ifndef USE_VULKAN
#define GLE(ret, name, ...) extern ret(APIENTRY *q##name)(__VA_ARGS__);
QGL_Core_PROCS;
QGL_Ext_PROCS;
#undef GLE
#endif

#endif // TR_LOCAL_HPP

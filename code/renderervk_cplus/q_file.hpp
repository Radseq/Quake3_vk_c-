#ifndef Q_FILE_HPP
#define Q_FILE_HPP

#include "q_shared.hpp"

/*
==============================================================================

MDR file format

==============================================================================
*/

/*
 * Here are the definitions for Ravensoft's model format of md4. Raven stores their
 * playermodels in .mdr files, in some games, which are pretty much like the md4
 * format implemented by ID soft. It seems like ID's original md4 stuff is not used at all.
 * MDR is being used in EliteForce, JediKnight2 and Soldiers of Fortune2 (I think).
 * So this comes in handy for anyone who wants to make it possible to load player
 * models from these games.
 * This format has bone tags, which is similar to the thing you have in md3 I suppose.
 * Raven has released their version of md3view under GPL enabling me to add support
 * to this codebase. Thanks to Steven Howes aka Skinner for helping with example
 * source code.
 *
 * - Thilo Schulz (arny@ats.s.bawue.de)
 */

#define MDR_IDENT	(('5'<<24)+('M'<<16)+('D'<<8)+'R')
#define MDR_VERSION	2
#define	MDR_MAX_BONES	128

// surface geometry should not exceed these limits
#define SHADER_MAX_VERTEXES 1000
#define SHADER_MAX_INDEXES (6 * SHADER_MAX_VERTEXES)

// limits
#define MD3_MAX_LODS 3

// the maximum size of game relative pathnames
#define MAX_QPATH 64

typedef struct
{
    int32_t ident;
    int32_t version;

    char name[MAX_QPATH]; // model name

    uint32_t flags;

    int32_t numFrames;
    int32_t numTags;
    int32_t numSurfaces;

    int32_t numSkins;

    uint32_t ofsFrames;   // offset for first frame
    uint32_t ofsTags;     // numFrames * numTags
    uint32_t ofsSurfaces; // first surface, others follow

    uint32_t ofsEnd; // end of file
} md3Header_t;

typedef struct
{
    vec3_t xyz;
    float st[2];
    float lightmap[2];
    vec3_t normal;
    color4ub_t color;
} drawVert_t;

typedef struct
{
    char shader[MAX_QPATH];
    int surfaceFlags;
    int contentFlags;
} dshader_t;

typedef struct
{
    int boneIndex;    // these are indexes into the boneReferences,
    float boneWeight; // not the global per-frame bone list
    vec3_t offset;
} mdrWeight_t;

typedef struct
{
    vec3_t normal;
    vec2_t texCoords;
    int numWeights;
    mdrWeight_t weights[1]; // variable sized
} mdrVertex_t;

typedef struct
{
    int ident;
    int version;

    char name[MAX_QPATH]; // model name

    // frames and bones are shared by all levels of detail
    int numFrames;
    int numBones;
    int ofsFrames; // mdrFrame_t[numFrames]

    // each level of detail has completely separate sets of surfaces
    int numLODs;
    int ofsLODs;

    int numTags;
    int ofsTags;

    int ofsEnd; // end of file
} mdrHeader_t;

typedef struct md3Frame_s
{
    vec3_t bounds[2];
    vec3_t localOrigin;
    float radius;
    char name[16];
} md3Frame_t;

typedef struct
{
    float matrix[3][4];
} mdrBone_t;

typedef struct
{
    vec3_t bounds[2];   // bounds of all surfaces of all LOD's for this frame
    vec3_t localOrigin; // midpoint of bounds, used for sphere cull
    float radius;       // dist from localOrigin to corner
    char name[16];
    mdrBone_t bones[1]; // [numBones]
} mdrFrame_t;

typedef struct md3Tag_s
{
    char name[MAX_QPATH]; // tag name
    vec3_t origin;
    vec3_t axis[3];
} md3Tag_t;

typedef struct
{
    int boneIndex;
    char name[32];
} mdrTag_t;

typedef struct {
	int			indexes[3];
} mdrTriangle_t;

typedef struct {
	int			numSurfaces;
	int			ofsSurfaces;		// first surface, others follow
	int			ofsEnd;				// next lod follows
} mdrLOD_t;

typedef struct {
	short		xyz[3];
	short		normal;
} md3XyzNormal_t;

typedef struct {
	int			ident;

	char		name[MAX_QPATH];	// polyset name
	char		shader[MAX_QPATH];
	int			shaderIndex;	// for in-game use

	int			ofsHeader;	// this will be a negative number

	int			numVerts;
	int			ofsVerts;

	int			numTriangles;
	int			ofsTriangles;

	// Bone references are a set of ints representing all the bones
	// present in any vertex weights for this surface.  This is
	// needed because a model may have surfaces that need to be
	// drawn at different sort times, and we don't want to have
	// to re-interpolate all the bones for each surface.
	int			numBoneReferences;
	int			ofsBoneReferences;

	int			ofsEnd;		// next surface follows
} mdrSurface_t;

typedef struct {
        unsigned char Comp[24]; // MC_COMP_BYTES is in MatComp.h, but don't want to couple
} mdrCompBone_t;

typedef struct {
        vec3_t          bounds[2];		// bounds of all surfaces of all LOD's for this frame
        vec3_t          localOrigin;		// midpoint of bounds, used for sphere cull
        float           radius;			// dist from localOrigin to corner
        mdrCompBone_t   bones[1];		// [numBones]
} mdrCompFrame_t;

typedef struct {
	char			name[MAX_QPATH];
	int				shaderIndex;	// for in-game use
} md3Shader_t;

/*
** md3Surface_t
**
** CHUNK			SIZE
** header			sizeof( md3Surface_t )
** shaders			sizeof( md3Shader_t ) * numShaders
** triangles[0]		sizeof( md3Triangle_t ) * numTriangles
** st				sizeof( md3St_t ) * numVerts
** XyzNormals		sizeof( md3XyzNormal_t ) * numVerts * numFrames
*/
typedef struct {
	int32_t ident;				//

	char	name[MAX_QPATH];	// polyset name

	int32_t flags;
	int32_t numFrames;			// all surfaces in a model should have the same

	int32_t numShaders;			// all surfaces in a model should have the same
	int32_t numVerts;

	int32_t numTriangles;
	uint32_t ofsTriangles;

	uint32_t ofsShaders;			// offset from start of md3Surface_t
	uint32_t ofsSt;				// texture coords are common for all frames
	uint32_t ofsXyzNormals;		// numVerts * numFrames

	uint32_t ofsEnd;				// next surface follows
} md3Surface_t;

typedef struct {
	int			indexes[3];
} md3Triangle_t;

#endif // Q_FILE_HPP

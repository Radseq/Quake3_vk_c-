/*
===========================================================================
Copyright (C) 2011 Thilo Schulz <thilo@tjps.eu>
Copyright (C) 2011 Matthias Bentrup <matthias.bentrup@googlemail.com>
Copyright (C) 2011-2019 Zack Middleton <zturtleman@gmail.com>

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

#include "tr_model_iqm.hpp"

static int R_CullIQM(const iqmData_t *data, const trRefEntity_t *ent)
{
	vec3_t bounds[2];
	vec_t *oldBounds, *newBounds;
	int i;

	if (!data->bounds)
	{
		tr.pc.c_box_cull_md3_clip++;
		return CULL_CLIP;
	}

	// compute bounds pointers
	oldBounds = data->bounds + 6 * ent->e.oldframe;
	newBounds = data->bounds + 6 * ent->e.frame;

	// calculate a bounding box in the current coordinate system
	for (i = 0; i < 3; i++)
	{
		bounds[0][i] = oldBounds[i] < newBounds[i] ? oldBounds[i] : newBounds[i];
		bounds[1][i] = oldBounds[i + 3] > newBounds[i + 3] ? oldBounds[i + 3] : newBounds[i + 3];
	}

	switch (R_CullLocalBox_plus(bounds))
	{
	case CULL_IN:
		tr.pc.c_box_cull_md3_in++;
		return CULL_IN;
	case CULL_CLIP:
		tr.pc.c_box_cull_md3_clip++;
		return CULL_CLIP;
	case CULL_OUT:
	default:
		tr.pc.c_box_cull_md3_out++;
		return CULL_OUT;
	}
}

static int R_ComputeIQMFogNum(const iqmData_t *data, const trRefEntity_t *ent)
{
	int i, j;
	const fog_t *fog;
	const vec_t *bounds;
	const vec_t defaultBounds[6] = {-8, -8, -8, 8, 8, 8};
	vec3_t diag, center;
	vec3_t localOrigin;
	vec_t radius;

	if (tr.refdef.rdflags & RDF_NOWORLDMODEL)
	{
		return 0;
	}

	// FIXME: non-normalized axis issues
	if (data->bounds)
	{
		bounds = data->bounds + 6 * ent->e.frame;
	}
	else
	{
		bounds = defaultBounds;
	}
	VectorSubtract(bounds + 3, bounds, diag);
	VectorMA(bounds, 0.5f, diag, center);
	VectorAdd(ent->e.origin, center, localOrigin);
	radius = 0.5f * VectorLength(diag);

	for (i = 1; i < tr.world->numfogs; i++)
	{
		fog = &tr.world->fogs[i];
		for (j = 0; j < 3; j++)
		{
			if (localOrigin[j] - radius >= fog->bounds[1][j])
			{
				break;
			}
			if (localOrigin[j] + radius <= fog->bounds[0][j])
			{
				break;
			}
		}
		if (j == 3)
		{
			return i;
		}
	}

	return 0;
}

/*
=================
R_AddIQMSurfaces

Add all surfaces of this model
=================
*/
void R_AddIQMSurfaces_plus(trRefEntity_t *ent)
{
	iqmData_t *data;
	srfIQModel_t *surface;
	int i, j;
	bool personalModel;
	int cull;
	int fogNum;
	shader_t *shader;
	const skin_t *skin;

	data = static_cast<iqmData_t *>(tr.currentModel->modelData);
	surface = data->surfaces;

	// don't add third_person objects if not in a portal
	personalModel = (ent->e.renderfx & RF_THIRD_PERSON) && (tr.viewParms.portalView == PV_NONE);

	if (ent->e.renderfx & RF_WRAP_FRAMES)
	{
		ent->e.frame %= data->num_frames;
		ent->e.oldframe %= data->num_frames;
	}

	//
	// Validate the frames so there is no chance of a crash.
	// This will write directly into the entity structure, so
	// when the surfaces are rendered, they don't need to be
	// range checked again.
	//
	if ((ent->e.frame >= data->num_frames) || (ent->e.frame < 0) || (ent->e.oldframe >= data->num_frames) || (ent->e.oldframe < 0))
	{
		ri.Printf(PRINT_DEVELOPER, "R_AddIQMSurfaces: no such frame %d to %d for '%s'\n",
				  ent->e.oldframe, ent->e.frame,
				  tr.currentModel->name);
		ent->e.frame = 0;
		ent->e.oldframe = 0;
	}

	//
	// cull the entire model if merged bounding box of both frames
	// is outside the view frustum.
	//
	cull = R_CullIQM(data, ent);
	if (cull == CULL_OUT)
	{
		return;
	}

	//
	// set up lighting now that we know we aren't culled
	//
	if (!personalModel || r_shadows->integer > 1)
	{
		R_SetupEntityLighting_plus(&tr.refdef, ent);
	}

	//
	// see if we are in a fog volume
	//
	fogNum = R_ComputeIQMFogNum(data, ent);

	for (i = 0; i < data->num_surfaces; i++)
	{
		if (ent->e.customShader)
			shader = R_GetShaderByHandle_plus(ent->e.customShader);
		else if (ent->e.customSkin > 0 && ent->e.customSkin < tr.numSkins)
		{
			skin = R_GetSkinByHandle_plus(ent->e.customSkin);
			shader = tr.defaultShader;

			for (j = 0; j < skin->numSurfaces; j++)
			{
				if (!strcmp(skin->surfaces[j].name, surface->name))
				{
					shader = skin->surfaces[j].shader;
					break;
				}
			}
		}
		else
		{
			shader = surface->shader;
		}

		// we will add shadows even if the main object isn't visible in the view

		// stencil shadows can't do personal models unless I polyhedron clip
		if (!personalModel && r_shadows->integer == 2 && fogNum == 0 && !(ent->e.renderfx & (RF_NOSHADOW | RF_DEPTHHACK)) && shader->sort == SS_OPAQUE)
		{
			R_AddDrawSurf_plus(reinterpret_cast<surfaceType_t *>(surface), tr.shadowShader, 0, 0);
		}

		// projection shadows work fine with personal models
		if (r_shadows->integer == 3 && fogNum == 0 && (ent->e.renderfx & RF_SHADOW_PLANE) && shader->sort == SS_OPAQUE)
		{
			R_AddDrawSurf_plus(reinterpret_cast<surfaceType_t *>(surface), tr.projectionShadowShader, 0, 0);
		}

		if (!personalModel)
		{
			R_AddDrawSurf_plus(reinterpret_cast<surfaceType_t *>(surface), shader, fogNum, 0);
			tr.needScreenMap |= shader->hasScreenMap;
		}

		surface++;
	}
}

// "multiply" 3x4 matrices, these are assumed to be the top 3 rows
// of a 4x4 matrix with the last row = (0 0 0 1)
static void Matrix34Multiply( const float *a, const float *b, float *out ) {
	out[ 0] = a[0] * b[0] + a[1] * b[4] + a[ 2] * b[ 8];
	out[ 1] = a[0] * b[1] + a[1] * b[5] + a[ 2] * b[ 9];
	out[ 2] = a[0] * b[2] + a[1] * b[6] + a[ 2] * b[10];
	out[ 3] = a[0] * b[3] + a[1] * b[7] + a[ 2] * b[11] + a[ 3];
	out[ 4] = a[4] * b[0] + a[5] * b[4] + a[ 6] * b[ 8];
	out[ 5] = a[4] * b[1] + a[5] * b[5] + a[ 6] * b[ 9];
	out[ 6] = a[4] * b[2] + a[5] * b[6] + a[ 6] * b[10];
	out[ 7] = a[4] * b[3] + a[5] * b[7] + a[ 6] * b[11] + a[ 7];
	out[ 8] = a[8] * b[0] + a[9] * b[4] + a[10] * b[ 8];
	out[ 9] = a[8] * b[1] + a[9] * b[5] + a[10] * b[ 9];
	out[10] = a[8] * b[2] + a[9] * b[6] + a[10] * b[10];
	out[11] = a[8] * b[3] + a[9] * b[7] + a[10] * b[11] + a[11];
}

static void QuatSlerp(const quat_t from, const quat_t _to, float fraction, quat_t out) {
	float angle, cosAngle, sinAngle, backlerp, lerp;
	quat_t to;

	// cos() of angle
	cosAngle = from[0] * _to[0] + from[1] * _to[1] + from[2] * _to[2] + from[3] * _to[3];

	// negative handling is needed for taking shortest path (required for model joints)
	if ( cosAngle < 0.0f ) {
		cosAngle = -cosAngle;
		to[0] = - _to[0];
		to[1] = - _to[1];
		to[2] = - _to[2];
		to[3] = - _to[3];
	} else {
		QuatCopy( _to, to );
	}

	if ( cosAngle < 0.999999f ) {
		// spherical lerp (slerp)
		angle = acosf( cosAngle );
		sinAngle = sinf( angle );
		backlerp = sinf( ( 1.0f - fraction ) * angle ) / sinAngle;
		lerp = sinf( fraction * angle ) / sinAngle;
	} else {
		// linear lerp
		backlerp = 1.0f - fraction;
		lerp = fraction;
	}

	out[0] = from[0] * backlerp + to[0] * lerp;
	out[1] = from[1] * backlerp + to[1] * lerp;
	out[2] = from[2] * backlerp + to[2] * lerp;
	out[3] = from[3] * backlerp + to[3] * lerp;
}

static void JointToMatrix( const quat_t rot, const vec3_t scale, const vec3_t trans,
			   float *mat ) {
	float xx = 2.0f * rot[0] * rot[0];
	float yy = 2.0f * rot[1] * rot[1];
	float zz = 2.0f * rot[2] * rot[2];
	float xy = 2.0f * rot[0] * rot[1];
	float xz = 2.0f * rot[0] * rot[2];
	float yz = 2.0f * rot[1] * rot[2];
	float wx = 2.0f * rot[3] * rot[0];
	float wy = 2.0f * rot[3] * rot[1];
	float wz = 2.0f * rot[3] * rot[2];

	mat[ 0] = scale[0] * (1.0f - (yy + zz));
	mat[ 1] = scale[0] * (xy - wz);
	mat[ 2] = scale[0] * (xz + wy);
	mat[ 3] = trans[0];
	mat[ 4] = scale[1] * (xy + wz);
	mat[ 5] = scale[1] * (1.0f - (xx + zz));
	mat[ 6] = scale[1] * (yz - wx);
	mat[ 7] = trans[1];
	mat[ 8] = scale[2] * (xz - wy);
	mat[ 9] = scale[2] * (yz + wx);
	mat[10] = scale[2] * (1.0f - (xx + yy));
	mat[11] = trans[2];
}

static void ComputePoseMats( iqmData_t *data, int frame, int oldframe,
			      float backlerp, float *poseMats ) {
	iqmTransform_t relativeJoints[IQM_MAX_JOINTS];
	iqmTransform_t *relativeJoint;
	const iqmTransform_t *pose;
	const iqmTransform_t *oldpose;
	const int *jointParent;
	const float *invBindMat;
	float *poseMat, lerp;
	int i;

	relativeJoint = relativeJoints;

	// copy or lerp animation frame pose
	if ( oldframe == frame ) {
		pose = &data->poses[frame * data->num_poses];
		for ( i = 0; i < data->num_poses; i++, pose++, relativeJoint++ ) {
			VectorCopy( pose->translate, relativeJoint->translate );
			QuatCopy( pose->rotate, relativeJoint->rotate );
			VectorCopy( pose->scale, relativeJoint->scale );
		}
	} else {
		lerp = 1.0f - backlerp;
		pose = &data->poses[frame * data->num_poses];
		oldpose = &data->poses[oldframe * data->num_poses];
		for ( i = 0; i < data->num_poses; i++, oldpose++, pose++, relativeJoint++ ) {
			relativeJoint->translate[0] = oldpose->translate[0] * backlerp + pose->translate[0] * lerp;
			relativeJoint->translate[1] = oldpose->translate[1] * backlerp + pose->translate[1] * lerp;
			relativeJoint->translate[2] = oldpose->translate[2] * backlerp + pose->translate[2] * lerp;

			relativeJoint->scale[0] = oldpose->scale[0] * backlerp + pose->scale[0] * lerp;
			relativeJoint->scale[1] = oldpose->scale[1] * backlerp + pose->scale[1] * lerp;
			relativeJoint->scale[2] = oldpose->scale[2] * backlerp + pose->scale[2] * lerp;

			QuatSlerp( oldpose->rotate, pose->rotate, lerp, relativeJoint->rotate );
		}
	}

	// multiply by inverse of bind pose and parent 'pose mat' (bind pose transform matrix)
	relativeJoint = relativeJoints;
	jointParent = data->jointParents;
	invBindMat = data->invBindJoints;
	poseMat = poseMats;
	for ( i = 0; i < data->num_poses; i++, relativeJoint++, jointParent++, invBindMat += 12, poseMat += 12 ) {
		float mat1[12], mat2[12];

		JointToMatrix( relativeJoint->rotate, relativeJoint->scale, relativeJoint->translate, mat1 );

		if ( *jointParent >= 0 ) {
			Matrix34Multiply( &data->bindJoints[(*jointParent)*12], mat1, mat2 );
			Matrix34Multiply( mat2, invBindMat, mat1 );
			Matrix34Multiply( &poseMats[(*jointParent)*12], mat1, poseMat );
		} else {
			Matrix34Multiply( mat1, invBindMat, poseMat );
		}
	}
}

static void ComputeJointMats( iqmData_t *data, int frame, int oldframe,
			      float backlerp, float *mat ) {
	float	*mat1;
	int	i;

	if ( data->num_poses == 0 ) {
		Com_Memcpy( mat, data->bindJoints, data->num_joints * 12 * sizeof(float) );
		return;
	}

	ComputePoseMats( data, frame, oldframe, backlerp, mat );

	for( i = 0; i < data->num_joints; i++ ) {
		float outmat[12];
		mat1 = mat + 12 * i;

		Com_Memcpy(outmat, mat1, sizeof(outmat));

		Matrix34Multiply( outmat, data->bindJoints + 12*i, mat1 );
	}
}

int R_IQMLerpTag_plus( orientation_t *tag, iqmData_t *data,
		  int startFrame, int endFrame, 
		  float frac, const char *tagName ) {
	float	jointMats[IQM_MAX_JOINTS * 12];
	int	joint;
	char	*names = data->jointNames;

	// get joint number by reading the joint names
	for( joint = 0; joint < data->num_joints; joint++ ) {
		if( !strcmp( tagName, names ) )
			break;
		names += strlen( names ) + 1;
	}
	if( joint >= data->num_joints ) {
		AxisClear_plus( tag->axis );
		VectorClear( tag->origin );
		return false;
	}

	ComputeJointMats( data, startFrame, endFrame, frac, jointMats );

	tag->axis[0][0] = jointMats[12 * joint + 0];
	tag->axis[1][0] = jointMats[12 * joint + 1];
	tag->axis[2][0] = jointMats[12 * joint + 2];
	tag->origin[0] = jointMats[12 * joint + 3];
	tag->axis[0][1] = jointMats[12 * joint + 4];
	tag->axis[1][1] = jointMats[12 * joint + 5];
	tag->axis[2][1] = jointMats[12 * joint + 6];
	tag->origin[1] = jointMats[12 * joint + 7];
	tag->axis[0][2] = jointMats[12 * joint + 8];
	tag->axis[1][2] = jointMats[12 * joint + 9];
	tag->axis[2][2] = jointMats[12 * joint + 10];
	tag->origin[2] = jointMats[12 * joint + 11];

	return true;
}

// /*
// =================
// RB_AddIQMSurfaces

// Compute vertices for this model surface
// =================
// */
// void RB_IQMSurfaceAnim( const surfaceType_t *surface ) {
// 	srfIQModel_t	*surf = (srfIQModel_t *)surface;
// 	iqmData_t	*data = surf->data;
// 	float		poseMats[IQM_MAX_JOINTS * 12];
// 	float		influenceVtxMat[SHADER_MAX_VERTEXES * 12];
// 	float		influenceNrmMat[SHADER_MAX_VERTEXES * 9];
// 	int		i;

// 	float		*xyz;
// 	float		*normal;
// 	float		*texCoords;
// 	byte		*color;
// 	vec4_t		*outXYZ;
// 	vec4_t		*outNormal;
// 	float		*outTexCoord;
// 	color4ub_t	*outColor;

// 	int	frame = data->num_frames ? backEnd.currentEntity->e.frame % data->num_frames : 0;
// 	int	oldframe = data->num_frames ? backEnd.currentEntity->e.oldframe % data->num_frames : 0;
// 	float	backlerp = backEnd.currentEntity->e.backlerp;

// 	int		*tri;
// 	glIndex_t	*ptr;
// 	glIndex_t	base;

// 	RB_CHECKOVERFLOW( surf->num_vertexes, surf->num_triangles * 3 );

// 	xyz = &data->positions[surf->first_vertex * 3];
// 	normal = &data->normals[surf->first_vertex * 3];
// 	texCoords = &data->texcoords[surf->first_vertex * 2];

// 	if ( data->colors ) {
// 		color = &data->colors[surf->first_vertex * 4];
// 	} else {
// 		color = NULL;
// 	}

// 	outXYZ = &tess.xyz[tess.numVertexes];
// 	outNormal = &tess.normal[tess.numVertexes];
// 	outTexCoord = &tess.texCoords[0][tess.numVertexes][0];
// 	outColor = &tess.vertexColors[tess.numVertexes];

// 	if ( data->num_poses > 0 ) {
// 		// compute interpolated joint matrices
// 		ComputePoseMats( data, frame, oldframe, backlerp, poseMats );

// 		// compute vertex blend influence matricies
// 		for( i = 0; i < surf->num_influences; i++ ) {
// 			int influence = surf->first_influence + i;
// 			float *vtxMat = &influenceVtxMat[12*i];
// 			float *nrmMat = &influenceNrmMat[9*i];
// 			int	j;
// 			float	blendWeights[4];

// 			if ( data->blendWeightsType == IQM_FLOAT ) {
// 				blendWeights[0] = data->influenceBlendWeights.f[4*influence + 0];
// 				blendWeights[1] = data->influenceBlendWeights.f[4*influence + 1];
// 				blendWeights[2] = data->influenceBlendWeights.f[4*influence + 2];
// 				blendWeights[3] = data->influenceBlendWeights.f[4*influence + 3];
// 			} else {
// 				blendWeights[0] = (float)data->influenceBlendWeights.b[4*influence + 0] / 255.0f;
// 				blendWeights[1] = (float)data->influenceBlendWeights.b[4*influence + 1] / 255.0f;
// 				blendWeights[2] = (float)data->influenceBlendWeights.b[4*influence + 2] / 255.0f;
// 				blendWeights[3] = (float)data->influenceBlendWeights.b[4*influence + 3] / 255.0f;
// 			}

// 			if ( blendWeights[0] <= 0.0f ) {
// 				// no blend joint, use identity matrix.
// 				vtxMat[0] = identityMatrix[0];
// 				vtxMat[1] = identityMatrix[1];
// 				vtxMat[2] = identityMatrix[2];
// 				vtxMat[3] = identityMatrix[3];
// 				vtxMat[4] = identityMatrix[4];
// 				vtxMat[5] = identityMatrix[5];
// 				vtxMat[6] = identityMatrix[6];
// 				vtxMat[7] = identityMatrix[7];
// 				vtxMat[8] = identityMatrix[8];
// 				vtxMat[9] = identityMatrix[9];
// 				vtxMat[10] = identityMatrix[10];
// 				vtxMat[11] = identityMatrix[11];
// 			} else {
// 				// compute the vertex matrix by blending the up to
// 				// four blend weights
// 				vtxMat[0] = blendWeights[0] * poseMats[12 * data->influenceBlendIndexes[4*influence + 0] + 0];
// 				vtxMat[1] = blendWeights[0] * poseMats[12 * data->influenceBlendIndexes[4*influence + 0] + 1];
// 				vtxMat[2] = blendWeights[0] * poseMats[12 * data->influenceBlendIndexes[4*influence + 0] + 2];
// 				vtxMat[3] = blendWeights[0] * poseMats[12 * data->influenceBlendIndexes[4*influence + 0] + 3];
// 				vtxMat[4] = blendWeights[0] * poseMats[12 * data->influenceBlendIndexes[4*influence + 0] + 4];
// 				vtxMat[5] = blendWeights[0] * poseMats[12 * data->influenceBlendIndexes[4*influence + 0] + 5];
// 				vtxMat[6] = blendWeights[0] * poseMats[12 * data->influenceBlendIndexes[4*influence + 0] + 6];
// 				vtxMat[7] = blendWeights[0] * poseMats[12 * data->influenceBlendIndexes[4*influence + 0] + 7];
// 				vtxMat[8] = blendWeights[0] * poseMats[12 * data->influenceBlendIndexes[4*influence + 0] + 8];
// 				vtxMat[9] = blendWeights[0] * poseMats[12 * data->influenceBlendIndexes[4*influence + 0] + 9];
// 				vtxMat[10] = blendWeights[0] * poseMats[12 * data->influenceBlendIndexes[4*influence + 0] + 10];
// 				vtxMat[11] = blendWeights[0] * poseMats[12 * data->influenceBlendIndexes[4*influence + 0] + 11];

// 				for ( j = 1; j < ARRAY_LEN( blendWeights ); j++ ) {
// 					if ( blendWeights[j] <= 0.0f ) {
// 						break;
// 					}

// 					vtxMat[0] += blendWeights[j] * poseMats[12 * data->influenceBlendIndexes[4*influence + j] + 0];
// 					vtxMat[1] += blendWeights[j] * poseMats[12 * data->influenceBlendIndexes[4*influence + j] + 1];
// 					vtxMat[2] += blendWeights[j] * poseMats[12 * data->influenceBlendIndexes[4*influence + j] + 2];
// 					vtxMat[3] += blendWeights[j] * poseMats[12 * data->influenceBlendIndexes[4*influence + j] + 3];
// 					vtxMat[4] += blendWeights[j] * poseMats[12 * data->influenceBlendIndexes[4*influence + j] + 4];
// 					vtxMat[5] += blendWeights[j] * poseMats[12 * data->influenceBlendIndexes[4*influence + j] + 5];
// 					vtxMat[6] += blendWeights[j] * poseMats[12 * data->influenceBlendIndexes[4*influence + j] + 6];
// 					vtxMat[7] += blendWeights[j] * poseMats[12 * data->influenceBlendIndexes[4*influence + j] + 7];
// 					vtxMat[8] += blendWeights[j] * poseMats[12 * data->influenceBlendIndexes[4*influence + j] + 8];
// 					vtxMat[9] += blendWeights[j] * poseMats[12 * data->influenceBlendIndexes[4*influence + j] + 9];
// 					vtxMat[10] += blendWeights[j] * poseMats[12 * data->influenceBlendIndexes[4*influence + j] + 10];
// 					vtxMat[11] += blendWeights[j] * poseMats[12 * data->influenceBlendIndexes[4*influence + j] + 11];
// 				}
// 			}

// 			// compute the normal matrix as transpose of the adjoint
// 			// of the vertex matrix
// 			nrmMat[ 0] = vtxMat[ 5]*vtxMat[10] - vtxMat[ 6]*vtxMat[ 9];
// 			nrmMat[ 1] = vtxMat[ 6]*vtxMat[ 8] - vtxMat[ 4]*vtxMat[10];
// 			nrmMat[ 2] = vtxMat[ 4]*vtxMat[ 9] - vtxMat[ 5]*vtxMat[ 8];
// 			nrmMat[ 3] = vtxMat[ 2]*vtxMat[ 9] - vtxMat[ 1]*vtxMat[10];
// 			nrmMat[ 4] = vtxMat[ 0]*vtxMat[10] - vtxMat[ 2]*vtxMat[ 8];
// 			nrmMat[ 5] = vtxMat[ 1]*vtxMat[ 8] - vtxMat[ 0]*vtxMat[ 9];
// 			nrmMat[ 6] = vtxMat[ 1]*vtxMat[ 6] - vtxMat[ 2]*vtxMat[ 5];
// 			nrmMat[ 7] = vtxMat[ 2]*vtxMat[ 4] - vtxMat[ 0]*vtxMat[ 6];
// 			nrmMat[ 8] = vtxMat[ 0]*vtxMat[ 5] - vtxMat[ 1]*vtxMat[ 4];
// 		}

// 		// transform vertexes and fill other data
// 		for( i = 0; i < surf->num_vertexes;
// 		     i++, xyz+=3, normal+=3, texCoords+=2,
// 		     outXYZ++, outNormal++, outTexCoord+=2 ) {
// 			int influence = data->influences[surf->first_vertex + i] - surf->first_influence;
// 			float *vtxMat = &influenceVtxMat[12*influence];
// 			float *nrmMat = &influenceNrmMat[9*influence];

// 			outTexCoord[0] = texCoords[0];
// 			outTexCoord[1] = texCoords[1];

// 			(*outXYZ)[0] =
// 				vtxMat[ 0] * xyz[0] +
// 				vtxMat[ 1] * xyz[1] +
// 				vtxMat[ 2] * xyz[2] +
// 				vtxMat[ 3];
// 			(*outXYZ)[1] =
// 				vtxMat[ 4] * xyz[0] +
// 				vtxMat[ 5] * xyz[1] +
// 				vtxMat[ 6] * xyz[2] +
// 				vtxMat[ 7];
// 			(*outXYZ)[2] =
// 				vtxMat[ 8] * xyz[0] +
// 				vtxMat[ 9] * xyz[1] +
// 				vtxMat[10] * xyz[2] +
// 				vtxMat[11];

// 			(*outNormal)[0] =
// 				nrmMat[ 0] * normal[0] +
// 				nrmMat[ 1] * normal[1] +
// 				nrmMat[ 2] * normal[2];
// 			(*outNormal)[1] =
// 				nrmMat[ 3] * normal[0] +
// 				nrmMat[ 4] * normal[1] +
// 				nrmMat[ 5] * normal[2];
// 			(*outNormal)[2] =
// 				nrmMat[ 6] * normal[0] +
// 				nrmMat[ 7] * normal[1] +
// 				nrmMat[ 8] * normal[2];
// 		}
// 	} else {
// 		// copy vertexes and fill other data
// 		for( i = 0; i < surf->num_vertexes;
// 			i++, xyz+=3, normal+=3, texCoords+=2,
// 			outXYZ++, outNormal++, outTexCoord+=2 ) {
// 			outTexCoord[0] = texCoords[0];
// 			outTexCoord[1] = texCoords[1];

// 			(*outXYZ)[0] = xyz[0];
// 			(*outXYZ)[1] = xyz[1];
// 			(*outXYZ)[2] = xyz[2];

// 			(*outNormal)[0] = normal[0];
// 			(*outNormal)[1] = normal[1];
// 			(*outNormal)[2] = normal[2];
// 		}
// 	}

// 	if ( color ) {
// 		Com_Memcpy( outColor, color, surf->num_vertexes * sizeof( outColor[0] ) );
// 	} else {
// 		Com_Memset( outColor, 0, surf->num_vertexes * sizeof( outColor[0] ) );
// 	}

// 	tri = data->triangles + 3 * surf->first_triangle;
// 	ptr = &tess.indexes[tess.numIndexes];
// 	base = tess.numVertexes;

// 	for( i = 0; i < surf->num_triangles; i++ ) {
// 		*ptr++ = base + (*tri++ - surf->first_vertex);
// 		*ptr++ = base + (*tri++ - surf->first_vertex);
// 		*ptr++ = base + (*tri++ - surf->first_vertex);
// 	}

// 	tess.numIndexes += 3 * surf->num_triangles;
// 	tess.numVertexes += surf->num_vertexes;
// }
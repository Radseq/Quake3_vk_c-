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
// tr_marks.c -- polygon projection on the world polygons

#include "tr_local.h"

#define MAX_VERTS_ON_POLY		64

#define MARKER_OFFSET			0	// 1

/*
=============
R_ChopPolyBehindPlane

Out must have space for two more vertexes than in
=============
*/
#define	SIDE_FRONT	0
#define	SIDE_BACK	1
#define	SIDE_ON		2
static void R_ChopPolyBehindPlane( int numInPoints, vec3_t inPoints[MAX_VERTS_ON_POLY],
								int *numOutPoints, vec3_t outPoints[MAX_VERTS_ON_POLY],
							vec3_t normal, vec_t dist, vec_t epsilon) {
	float		dists[MAX_VERTS_ON_POLY+4];
	int			sides[MAX_VERTS_ON_POLY+4];
	int			counts[3];
	float		dot;
	int			i, j;
	float		*p1, *p2, *clip;
	float		d;

	// don't clip if it might overflow
	if ( numInPoints >= MAX_VERTS_ON_POLY - 2 ) {
		*numOutPoints = 0;
		return;
	}

	counts[0] = counts[1] = counts[2] = 0;
	dists[0] = 0.0;
	sides[0] = 0;

	// determine sides for each point
	for ( i = 0 ; i < numInPoints ; i++ ) {
		dot = DotProduct( inPoints[i], normal );
		dot -= dist;
		dists[i] = dot;
		if ( dot > epsilon ) {
			sides[i] = SIDE_FRONT;
		} else if ( dot < -epsilon ) {
			sides[i] = SIDE_BACK;
		} else {
			sides[i] = SIDE_ON;
		}
		counts[sides[i]]++;
	}
	sides[i] = sides[0];
	dists[i] = dists[0];

	*numOutPoints = 0;

	if ( !counts[0] ) {
		return;
	}
	if ( !counts[1] ) {
		*numOutPoints = numInPoints;
		Com_Memcpy( outPoints, inPoints, numInPoints * sizeof(vec3_t) );
		return;
	}

	for ( i = 0 ; i < numInPoints ; i++ ) {
		p1 = inPoints[i];
		clip = outPoints[ *numOutPoints ];
		
		if ( sides[i] == SIDE_ON ) {
			VectorCopy( p1, clip );
			(*numOutPoints)++;
			continue;
		}
	
		if ( sides[i] == SIDE_FRONT ) {
			VectorCopy( p1, clip );
			(*numOutPoints)++;
			clip = outPoints[ *numOutPoints ];
		}

		if ( sides[i+1] == SIDE_ON || sides[i+1] == sides[i] ) {
			continue;
		}
			
		// generate a split point
		p2 = inPoints[ (i+1) % numInPoints ];

		d = dists[i] - dists[i+1];
		if ( d == 0 ) {
			dot = 0;
		} else {
			dot = dists[i] / d;
		}

		// clip xyz

		for (j=0 ; j<3 ; j++) {
			clip[j] = p1[j] + dot * ( p2[j] - p1[j] );
		}

		(*numOutPoints)++;
	}
}

/*
=================
R_BoxSurfaces_r

=================
*/
static void R_BoxSurfaces_r(mnode_t *node, vec3_t mins, vec3_t maxs, surfaceType_t **list, int listsize, int *listlength, vec3_t dir) {

	int			s, c;
	msurface_t	*surf, **mark;

	// do the tail recursion in a loop
	while ( node->contents == CONTENTS_NODE ) {
		s = BoxOnPlaneSide( mins, maxs, node->plane );
		if (s == 1) {
			node = node->children[0];
		} else if (s == 2) {
			node = node->children[1];
		} else {
			R_BoxSurfaces_r(node->children[0], mins, maxs, list, listsize, listlength, dir);
			node = node->children[1];
		}
	}

	// add the individual surfaces
	mark = node->firstmarksurface;
	c = node->nummarksurfaces;
	while (c--) {
		//
		if (*listlength >= listsize) break;
		//
		surf = *mark;
		// check if the surface has NOIMPACT or NOMARKS set
		if ( ( surf->shader->surfaceFlags & ( SURF_NOIMPACT | SURF_NOMARKS ) )
			|| ( surf->shader->contentFlags & CONTENTS_FOG ) ) {
			surf->viewCount = tr.viewCount;
		}
		// extra check for surfaces to avoid list overflows
		else if (*(surf->data) == SF_FACE) {
			// the face plane should go through the box
			s = BoxOnPlaneSide( mins, maxs, &(( srfSurfaceFace_t * ) surf->data)->plane );
			if (s == 1 || s == 2) {
				surf->viewCount = tr.viewCount;
			} else if (DotProduct((( srfSurfaceFace_t * ) surf->data)->plane.normal, dir) > -0.5) {
			// don't add faces that make sharp angles with the projection direction
				surf->viewCount = tr.viewCount;
			}
		}
		else if (*(surfaceType_t *) (surf->data) != SF_GRID &&
			 *(surfaceType_t *) (surf->data) != SF_TRIANGLES)
			surf->viewCount = tr.viewCount;
		// check the viewCount because the surface may have
		// already been added if it spans multiple leafs
		if (surf->viewCount != tr.viewCount) {
			surf->viewCount = tr.viewCount;
			list[*listlength] = (surfaceType_t *) surf->data;
			(*listlength)++;
		}
		mark++;
	}
}

/*
=================
R_AddMarkFragments

=================
*/
static void R_AddMarkFragments(int numClipPoints, vec3_t clipPoints[2][MAX_VERTS_ON_POLY],
				   int numPlanes, vec3_t *normals, float *dists,
				   int maxPoints, vec3_t pointBuffer,
				   int maxFragments, markFragment_t *fragmentBuffer,
				   int *returnedPoints, int *returnedFragments,
				   vec3_t mins, vec3_t maxs) {
	int pingPong, i;
	markFragment_t	*mf;

	// chop the surface by all the bounding planes of the to be projected polygon
	pingPong = 0;

	for ( i = 0 ; i < numPlanes ; i++ ) {

		R_ChopPolyBehindPlane( numClipPoints, clipPoints[pingPong],
						   &numClipPoints, clipPoints[!pingPong],
							normals[i], dists[i], 0.5 );
		pingPong ^= 1;
		if ( numClipPoints == 0 ) {
			break;
		}
	}
	// completely clipped away?
	if ( numClipPoints == 0 ) {
		return;
	}

	// add this fragment to the returned list
	if ( numClipPoints + (*returnedPoints) > maxPoints ) {
		return;	// not enough space for this polygon
	}
	/*
	// all the clip points should be within the bounding box
	for ( i = 0 ; i < numClipPoints ; i++ ) {
		int j;
		for ( j = 0 ; j < 3 ; j++ ) {
			if (clipPoints[pingPong][i][j] < mins[j] - 0.5) break;
			if (clipPoints[pingPong][i][j] > maxs[j] + 0.5) break;
		}
		if (j < 3) break;
	}
	if (i < numClipPoints) return;
	*/

	mf = fragmentBuffer + (*returnedFragments);
	mf->firstPoint = (*returnedPoints);
	mf->numPoints = numClipPoints;
	Com_Memcpy( pointBuffer + (*returnedPoints) * 3, clipPoints[pingPong], numClipPoints * sizeof(vec3_t) );

	(*returnedPoints) += numClipPoints;
	(*returnedFragments)++;
}

/*
=================
R_MarkFragments

=================
*/
int R_MarkFragments( int numPoints, const vec3_t *points, const vec3_t projection,
				   int maxPoints, vec3_t pointBuffer, int maxFragments, markFragment_t *fragmentBuffer ){
	R_MarkFragments_plus(numPoints, points, projection, maxPoints, pointBuffer, maxFragments,fragmentBuffer );
}
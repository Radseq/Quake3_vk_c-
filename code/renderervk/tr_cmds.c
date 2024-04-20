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
#include "tr_local.h"
#include "../renderervk_cplus/tr_cmds.hpp"



/*
=============
R_GetCommandBuffer
returns NULL if there is not enough space for important commands
=============
*/
void *R_GetCommandBuffer( int bytes ) {
	return R_GetCommandBuffer_plus(bytes);
}


/*
=============
R_AddDrawSurfCmd
=============
*/
void R_AddDrawSurfCmd( drawSurf_t *drawSurfs, int numDrawSurfs ) {
	R_AddDrawSurfCmd_plus(drawSurfs, numDrawSurfs);
}


/*
=============
RE_SetColor

Passing NULL will set the color to white
=============
*/
void RE_SetColor( const float *rgba ) {
	RE_SetColor_plus(rgba);
}

/*
=============
RE_StretchPic
=============
*/
void RE_StretchPic( float x, float y, float w, float h,
					float s1, float t1, float s2, float t2, qhandle_t hShader )
					{
						RE_StretchPic_plus( x,  y,  w,  h, s1,  t1,  s2,  t2,  hShader);
					}
#define MODE_RED_CYAN	1
#define MODE_RED_BLUE	2
#define MODE_RED_GREEN	3
#define MODE_GREEN_MAGENTA 4
#define MODE_MAX	MODE_GREEN_MAGENTA

/*
====================
RE_BeginFrame

If running in stereo, RE_BeginFrame will be called twice
for each RE_EndFrame
====================
*/
void RE_BeginFrame( stereoFrame_t stereoFrame ) {
	RE_BeginFrame_plus(stereoFrame);
}

/*
=============
RE_EndFrame

Returns the number of msec spent in the back end
=============
*/
void RE_EndFrame( int *frontEndMsec, int *backEndMsec ){
	RE_EndFrame_plus(frontEndMsec, backEndMsec);
}

/*
=============
RE_TakeVideoFrame
=============
*/
void RE_TakeVideoFrame( int width, int height,
		byte *captureBuffer, byte *encodeBuffer, bool motionJpeg )
{
RE_TakeVideoFrame_plus(width, height,captureBuffer,  encodeBuffer,motionJpeg );
}


void RE_ThrottleBackend( void )
{
	RE_ThrottleBackend_plus();
}


void RE_FinishBloom( void ){
	RE_FinishBloom_plus();
}

bool RE_CanMinimize( void )
{
	return RE_CanMinimize_plus();
}


const glconfig_t *RE_GetConfig( void )
{
	return RE_GetConfig_plus();
}


void RE_VertexLighting( bool allowed )
{
	RE_VertexLighting_plus(allowed);
}

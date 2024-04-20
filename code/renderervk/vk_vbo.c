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


#ifdef USE_VBO

/*

General concept of this VBO implementation is to store all possible static data
(vertexes,colors,tex.coords[0..1],normals) in device-local memory
and accessing it via indexes ONLY.

Static data in current meaning is a world surfaces whose shader data
can be evaluated at map load time.

Every static surface gets unique item index which will be added to queue
instead of tesselation like for regular surfaces. Using items queue also
eleminates run-time tesselation limits.

When it is time to render - we sort queued items to get longest possible
index sequence run to check if it is long enough i.e. worth issuing a draw call.
So long device-local index runs are rendered via multiple draw calls,
all remaining short index sequences are grouped together into single
host-visible index buffer which is finally rendered via single draw call.

*/


#endif // USE_VBO

/*
===========================================================================
ioquake3 png decoder
Copyright (C) 2007,2008 Joerg Dietrich

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
===========================================================================
*/

#include "../qcommon/q_shared.h"
#include "../renderervk/tr_public.h"
#include "../qcommon/puff.h"

void R_LoadPNG(const char *name, byte **pic, int *width, int *height){
	R_LoadPNG_plus(name, pic, width, height);
}
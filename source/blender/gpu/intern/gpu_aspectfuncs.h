/*
* ***** BEGIN GPL LICENSE BLOCK *****
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software Foundation,
* Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*
* The Original Code is Copyright (C) 2012 Blender Foundation.
* All rights reserved.
*
* The Original Code is: all of this file.
*
* Contributor(s): Jason Wilkins.
*
* ***** END GPL LICENSE BLOCK *****
*/

/** \file blender/gpu/intern/gpu_aspectfuncs.c
*  \ingroup gpu
*/

#ifndef __GPU_ASPECTFUNCS__
#define __GPU_ASPECTFUNCS__

#include "intern/gpu_aspect.h"



#ifndef GPU_ASPECT_INTERN
#define GPU_ASPECT_EXTERN(x) extern GLuint x;
#else
#define GPU_ASPECT_EXTERN(x) GLuint x = 0;
#endif



GPU_ASPECT_EXTERN(GPU_ASPECT_FONT);
GPU_ASPECT_EXTERN(GPU_ASPECT_TEXTURE);
GPU_ASPECT_EXTERN(GPU_ASPECT_PIXELS);



extern GPUaspectfuncs GPU_ASPECTFUNCS_FONT;
extern GPUaspectfuncs GPU_ASPECTFUNCS_TEXTURE;
extern GPUaspectfuncs GPU_ASPECTFUNCS_PIXELS;



void gpuInitializeAspectFuncs(void);
void gpuShutdownAspectFuncs(void);



#endif

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
 * Contributor(s):
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/gpu/intern/gpu_object_gl11.h
 *  \ingroup gpu
 */
#if 0
#if defined(WITH_GL_PROFILE_COMPAT)

#include "gpu_object_gl11.h"

#include "intern/gpu_glew.h"



struct GPU_object_gl11_data
{
	char norma;
	char cola;
	char texta;
} static od;



void gpuVertexPointer_gl11(int size, int type, int stride, const void *pointer)
{
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(size, type, stride, pointer);
}

void gpuNormalPointer_gl11(int type, int stride, const void *pointer)
{
	glEnableClientState(GL_NORMAL_ARRAY); od.norma = 1;
	glNormalPointer(type, stride, pointer);
}

void gpuColorPointer_gl11 (int size, int type, int stride, const void *pointer)
{
	glEnableClientState(GL_COLOR_ARRAY); od.cola = 1;
	glColorPointer(size, type, stride, pointer);
}

void gpuTexCoordPointer_gl11(int size, int type, int stride, const void *pointer)
{
	if(od.texta == 0) {
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		od.texta = 1;
	}

	glTexCoordPointer(size, type, stride, pointer);
}

void gpuClientActiveTexture_gl11(int texture)
{
	if (GLEW_VERSION_1_3 || GLEW_ARB_multitexture) {
		glClientActiveTexture(texture);
	}
}

void gpuCleanupAfterDraw_gl11(void)
{
	glDisableClientState(GL_VERTEX_ARRAY);

	if(od.norma)
		glDisableClientState(GL_NORMAL_ARRAY);
	if(od.cola)
		glDisableClientState(GL_COLOR_ARRAY);
	if(od.texta)
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	if (GLEW_VERSION_1_3 || GLEW_ARB_multitexture) {
		gpuClientActiveTexture_gl11(0);
	}

	od.norma = 0;
	od.cola  = 0;
	od.texta = 0;
}

#endif
#endif

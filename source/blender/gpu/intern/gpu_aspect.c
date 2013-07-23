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

/** \file blender/gpu/intern/gpu_aspect.c
*  \ingroup gpu
*/

#include "intern/gpu_aspect.h"

#include "gpu_safety.h"

#include "MEM_guardedalloc.h"


static GPUaspectfuncs ** GPU_ASPECT_FUNCS = NULL;

static size_t aspect_max  = 0;
static size_t aspect_free = 0;
static size_t aspect_fill = 0;



void gpuInitializeAspects(void)
{
	const size_t count = 100;

	GPU_ASPECT_FUNCS = (GPUaspectfuncs**)MEM_callocN(count * sizeof(GPUaspectfuncs*), "aspect functions");

	aspect_max  = count;
	aspect_free = count;
	aspect_fill = 0;
}



void gpuShutdownAspects(void)
{
	MEM_freeN(GPU_ASPECT_FUNCS);
	GPU_ASPECT_FUNCS = NULL;

	aspect_max   = 0;
	aspect_fill  = 0;
	aspect_free  = 0;
}



void gpuGenAspects(GLsizei count, GLuint* aspects)
{
	GLuint src, dst;

	if (count == 0) {
		return;
	}

	if (count > aspect_free) {
		aspect_max   = aspect_max + count - aspect_free;
		GPU_ASPECT_FUNCS = (GPUaspectfuncs**)MEM_reallocN(GPU_ASPECT_FUNCS, aspect_max * sizeof(GPUaspectfuncs*));
		aspect_free  = count;
	}

	src = aspect_fill;
	dst = 0;

	while (dst < count) {
		if (!GPU_ASPECT_FUNCS[src]) {
			aspects[dst] = src;
			dst++;
			aspect_fill = dst;
			aspect_free--;
		}

		src++;
	}
}



void gpuDeleteAspects(GLsizei count, const GLuint* aspects)
{
	GLuint i;

	for (i = 0; i < count; i++) {
		if (aspects[i] < aspect_fill) {
			aspect_fill = aspects[i];
		}

		GPU_ASPECT_FUNCS[aspects[i]] = NULL;
	}
}



void gpuAspectFuncs(GLuint aspect, GPUaspectfuncs* aspectFuncs)
{
	GPU_ASPECT_FUNCS[aspect] = aspectFuncs;
}


static GLboolean aspect_begin = GL_FALSE;

GLboolean gpuAspectBegin(GLuint aspect, const GLvoid* object)
{
	GPUaspectfuncs* aspectFuncs;

	GPU_ASSERT(!aspect_begin);

	aspect_begin = GL_TRUE;
	aspectFuncs = GPU_ASPECT_FUNCS[aspect];
	return aspectFuncs ? aspectFuncs->begin(aspectFuncs->param, object) : GL_TRUE;
}



GLboolean gpuAspectEnd(GLuint aspect, const GLvoid* object)
{
	GPUaspectfuncs* aspectFuncs;
	GPU_ASSERT(aspect_begin);

	aspect_begin = GL_FALSE;
	aspectFuncs = GPU_ASPECT_FUNCS[aspect];
	return aspectFuncs ? aspectFuncs->end(aspectFuncs->param, object) : GL_TRUE;
}

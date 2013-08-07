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

#include "intern/gpu_safety.h"

#include "MEM_guardedalloc.h"



static GPUaspectfuncs ** GPU_ASPECT_FUNCS = NULL;

static size_t aspect_max  = 0;
static size_t aspect_free = 0;
static size_t aspect_fill = 0;

static GPUaspectfuncs dummy = { NULL };


void gpu_initialize_aspects(void)
{
	const size_t count = 100;

	GPU_ASPECT_FUNCS = (GPUaspectfuncs**)MEM_callocN(count * sizeof(GPUaspectfuncs*), "gpu aspect functions");

	aspect_max  = count;
	aspect_free = count;
	aspect_fill = 0;
}



void gpu_shutdown_aspects(void)
{
	MEM_freeN(GPU_ASPECT_FUNCS);
	GPU_ASPECT_FUNCS = NULL;

	aspect_max   = 0;
	aspect_fill  = 0;
	aspect_free  = 0;
}



void GPU_gen_aspects(size_t count, uint32_t* aspects)
{
	uint32_t src, dst;

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
			GPU_ASPECT_FUNCS[src] = &dummy;
			aspects[dst] = src;
			dst++;
			aspect_fill = dst;
			aspect_free--;
		}

		src++;
	}
}



void GPU_delete_aspects(size_t count, const uint32_t* aspects)
{
	uint32_t i;

	for (i = 0; i < count; i++) {
		if (aspects[i] < aspect_fill) {
			aspect_fill = aspects[i];
		}

		GPU_ASPECT_FUNCS[aspects[i]] = NULL;
	}
}



void GPU_aspect_funcs(uint32_t aspect, GPUaspectfuncs* aspectFuncs)
{
	GPU_ASPECT_FUNCS[aspect] = aspectFuncs;
}



static uint32_t    current_aspect = -1;
static const void* current_object = NULL;


bool GPU_aspect_begin(uint32_t aspect, const void* object)
{
	GPUaspectfuncs* aspectFuncs;

	GPU_ASSERT(current_aspect == -1);

	current_aspect = aspect;
	current_object = object;

	aspectFuncs = GPU_ASPECT_FUNCS[aspect];
	return (aspectFuncs != NULL && aspectFuncs->begin != NULL) ? aspectFuncs->begin(aspectFuncs->param, object) : true;
}



bool GPU_aspect_end(void)
{
	GPUaspectfuncs* aspectFuncs = GPU_ASPECT_FUNCS[current_aspect];
	const void*     object      = current_object;

	GPU_ASSERT(current_aspect != -1);

	current_aspect = -1;
	current_object = NULL;

	return (aspectFuncs  != NULL && aspectFuncs->end != NULL) ? aspectFuncs->end(aspectFuncs->param, object) : true;
}



void GPU_aspect_enable (uint32_t aspect, uint32_t options)
{
	GPUaspectfuncs* aspectFuncs = GPU_ASPECT_FUNCS[aspect];

	if (aspectFuncs != NULL && aspectFuncs->enable != NULL)
		aspectFuncs->enable(aspectFuncs->param, options);
}



void GPU_aspect_disable(uint32_t aspect, uint32_t options)
{
	GPUaspectfuncs* aspectFuncs = GPU_ASPECT_FUNCS[aspect];

	if (aspectFuncs != NULL && aspectFuncs->disable != NULL )
		aspectFuncs->disable(aspectFuncs->param, options);
}



void gpu_commit_aspect(void)
{
	GPUaspectfuncs* aspectFuncs = GPU_ASPECT_FUNCS[current_aspect];

	GPU_ASSERT(current_aspect != -1);

	if (aspectFuncs != NULL && aspectFuncs->commit != NULL )
		aspectFuncs->commit(aspectFuncs->param);
}

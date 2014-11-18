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

/** \file source/blender/gpu/intern/gpu_aspect.c
 *  \ingroup gpu
 */

/* external */
#include "BLI_utildefines.h"
#include "BLI_sys_types.h"
#include "MEM_guardedalloc.h"
#include "GPU_aspect.h"


/* internal */
#include "gpu_private.h"


static GPUaspectimpl** GPU_ASPECT_FUNCS = NULL;

static size_t aspect_max  = 0;
static size_t aspect_free = 0;
static size_t aspect_fill = 0;

static GPUaspectimpl dummy = { NULL };

static uint32_t current_aspect = -1;

static bool in_select_mode = false;

bool gpu_aspect_active(void)
{
	return current_aspect != -1;
}

void gpu_aspect_init(void)
{
	const size_t count = 100;

	GPU_ASPECT_FUNCS = (GPUaspectimpl**)MEM_callocN(count * sizeof(GPUaspectimpl*), "GPU aspect function array");

	aspect_max  = count;
	aspect_free = count;
	aspect_fill = 0;
}



void gpu_aspect_exit(void)
{
	MEM_freeN(GPU_ASPECT_FUNCS);
	GPU_ASPECT_FUNCS = NULL;

	aspect_max   = 0;
	aspect_fill  = 0;
	aspect_free  = 0;

	current_aspect = -1;
}



bool GPU_commit_aspect(void)
{
	GPUaspectimpl* aspectImpl;

	BLI_assert(current_aspect != -1);
	BLI_assert(in_select_mode == gpu_is_select_mode()); /* not allowed to change select/render mode while an aspect is active */

	aspectImpl = GPU_ASPECT_FUNCS[current_aspect];

	if (aspectImpl != NULL) {
		if (in_select_mode)
			return aspectImpl->select_commit != NULL ? aspectImpl->select_commit(aspectImpl->object) : false;
		else
			return aspectImpl->render_commit != NULL ? aspectImpl->render_commit(aspectImpl->object) : false;
	}

	return false;
}



void GPU_gen_aspects(size_t count, uint32_t* aspects)
{
	uint32_t src, dst;

	if (count == 0) {
		return;
	}

	if (count > aspect_free) {
		aspect_max       = aspect_max + count - aspect_free;
		GPU_ASPECT_FUNCS = (GPUaspectimpl**)MEM_reallocN(GPU_ASPECT_FUNCS, aspect_max * sizeof(GPUaspectimpl*));
		aspect_free      = count;
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



void GPU_aspect_impl(uint32_t aspect, GPUaspectimpl* aspectImpl)
{
	if (aspectImpl != NULL)
		GPU_ASPECT_FUNCS[aspect] = aspectImpl;
	else
		GPU_ASPECT_FUNCS[aspect] = &dummy;
}



bool GPU_aspect_begin(uint32_t aspect, void* param)
{
	GPUaspectimpl* aspectImpl;

	BLI_assert(!gpu_aspect_active());

	current_aspect = aspect;

	in_select_mode = gpu_is_select_mode();

	aspectImpl = GPU_ASPECT_FUNCS[aspect];

	if (aspectImpl != NULL) {
		aspectImpl->current_param = param;

		if (in_select_mode)
			return aspectImpl->select_begin != NULL ? aspectImpl->select_begin(aspectImpl->object, param) : true;
		else
			return aspectImpl->render_begin != NULL ? aspectImpl->render_begin(aspectImpl->object, param) : true;
	}

	return true;
}



bool GPU_aspect_end(void)
{
	GPUaspectimpl* aspectImpl;
	void*          param;

	BLI_assert(gpu_aspect_active());
	BLI_assert(in_select_mode == gpu_is_select_mode()); /* not allowed to change select/render mode while an aspect is active */

	aspectImpl = GPU_ASPECT_FUNCS[current_aspect];

	current_aspect = -1;

	if (aspectImpl != NULL) {
		param = aspectImpl->current_param;
		aspectImpl->current_param  = NULL;

		if (in_select_mode)
			return aspectImpl->select_end != NULL ? aspectImpl->select_end(aspectImpl->object, param) : true;
		else
			return aspectImpl->render_end != NULL ? aspectImpl->render_end(aspectImpl->object, param) : true;
	}

	return true;
}



void GPU_aspect_enable(uint32_t aspect, uint32_t options)
{
	GPUaspectimpl* aspectImpl;

	BLI_assert(aspect < aspect_max);

	aspectImpl = GPU_ASPECT_FUNCS[aspect];

	if (aspectImpl != NULL && aspectImpl->enable != NULL)
		aspectImpl->enable(aspectImpl->object, options);
}



void GPU_aspect_disable(uint32_t aspect, uint32_t options)
{
	GPUaspectimpl* aspectImpl;

	BLI_assert(aspect < aspect_max);

	aspectImpl = GPU_ASPECT_FUNCS[aspect];

	if (aspectImpl != NULL && aspectImpl->disable != NULL )
		aspectImpl->disable(aspectImpl->object, options);
}

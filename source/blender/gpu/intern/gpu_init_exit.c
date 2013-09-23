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
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Jason Wilkins
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file source/blender/gpu/intern/gpu_init_exit.c
 *  \ingroup gpu
 */

/* my interface */
#include "GPU_init_exit.h"

/* my library */
#include "GPU_safety.h"

/* internal */
#include "intern/gpu_aspect_intern.h"
#include "intern/gpu_basic_intern.h"
#include "intern/gpu_blender_aspect_intern.h"
#include "intern/gpu_codegen.h"
#include "intern/gpu_common_intern.h"
#include "intern/gpu_extensions_intern.h"
#include "intern/gpu_font_intern.h"
#include "intern/gpu_immediate_intern.h"
#include "intern/gpu_lighting_intern.h"
#include "intern/gpu_matrix_intern.h"
#include "intern/gpu_pixels_intern.h"
#include "intern/gpu_raster_intern.h"
#include "intern/gpu_sprite_intern.h"
#include "intern/gpu_state_latch_intern.h"

/*

although the order of initialization and shutdown should not matter
(except for the extensions), I chose alphabetical and reverse alphabetical order

*/

static GPUimmediate* immediate;
static GPUindex*     index;

static bool initialized = false;

void GPU_init(void)
{
	/* can't avoid calling this multiple times, see wm_window_add_ghostwindow */
	if (initialized)
		return;

	initialized = true;

	gpu_extensions_init(); /* must come first */
	
	gpu_aspect_init();
	gpu_basic_init();
	gpu_blender_aspect_init();
	gpu_codegen_init();
	gpu_common_init();
	gpu_font_init();
	gpu_immediate_init();
	gpu_lighting_init();
	gpu_matrix_init();
	gpu_pixels_init();
	gpu_raster_init();
	gpu_sprite_init();
	gpu_state_latch_init();

	immediate = gpuNewImmediate();
	gpuImmediateMakeCurrent(immediate);
	gpuImmediateMaxVertexCount(500000); // XXX jwilkins: temporary!

	index = gpuNewIndex();
	gpuImmediateIndex(index);
	gpuImmediateMaxIndexCount(50000, GL_UNSIGNED_SHORT); // XXX jwilkins: temporary!

	GPU_aspect_begin(GPU_ASPECT_BASIC, NULL);
}



void GPU_exit(void)
{
	GPU_ASSERT(initialized);

	GPU_aspect_end();

	gpuDeleteIndex(index);
	gpuImmediateIndex(NULL);

	gpuImmediateMakeCurrent(NULL);
	gpuDeleteImmediate(immediate);

	gpu_state_latch_exit();
	gpu_sprite_exit();
	gpu_raster_exit();
	gpu_pixels_exit();
	gpu_matrix_init();
	gpu_lighting_exit();
	gpu_immediate_exit();
	gpu_font_exit();
	gpu_common_exit();
	gpu_codegen_exit();
	gpu_blender_aspect_exit();
	gpu_basic_exit();
	gpu_aspect_exit();

	gpu_extensions_exit(); /* must come last */
	
	initialized = false;
}

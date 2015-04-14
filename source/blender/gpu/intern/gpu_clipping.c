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

/** \file source/blender/gpu/intern/gpu_clipping.c
 *  \ingroup gpu
 */

#if WITH_GL_PROFILE_COMPAT
#define GPU_MANGLE_DEPRECATED 0 /* Allow use of deprecated OpenGL functions in this file */
#endif

#include "BLI_sys_types.h"

#include "GPU_extensions.h"
#include "GPU_matrix.h"
#include "GPU_debug.h"
#include "GPU_common.h"
#include "GPU_clipping.h"

/* internal */
#include "intern/gpu_private.h"

/* external */
#include "BLI_math_vector.h"



static struct CLIPPING {
	double   clip_plane[GPU_MAX_COMMON_CLIP_PLANES][4];
	uint32_t clip_plane_count;
} CLIPPING;



void gpu_clipping_init(void)
{
	memset(&CLIPPING, 0, sizeof(CLIPPING));
}



void gpu_clipping_exit(void)
{
}



void gpu_commit_clipping(void)
{
	const struct GPUcommon *common = gpu_get_common();

	int i;

	for (i = 0; i < CLIPPING.clip_plane_count; i++) {
		if (common) {
			glUniform4dv(common->clip_plane[i], 1, CLIPPING.clip_plane[i]);
		}

#if defined(WITH_GL_PROFILE_COMPAT)
		if (i < 6) {
			glClipPlane(GL_CLIP_PLANE0 + i, CLIPPING.clip_plane[i]); /* deprecated */
		}
#endif
	}

	if (common)
		glUniform1i(common->clip_plane_count, CLIPPING.clip_plane_count);
}



void GPU_restore_clip_planes(int clip_plane_count, const GPUplane clip_planes[])
{
	BLI_assert(clip_plane_count >= 0);
	BLI_assert(clip_plane_count < GPU_MAX_COMMON_CLIP_PLANES);

	memcpy(CLIPPING.clip_plane, clip_planes, clip_plane_count*sizeof(GPUplane));

	CLIPPING.clip_plane_count = clip_plane_count;
}



static void feedback_clip_plane_position(double position[4] /* in-out */)
{
	GPU_feedback_vertex_4dv(GPU_MODELVIEW_MATRIX, position[0], position[1], position[2], position[3], position);
}



void GPU_set_clip_planes(int clip_plane_count, const GPUplane clip_planes[])
{
	int i;

	GPU_restore_clip_planes(clip_plane_count, clip_planes);

	for (i = 0; i < clip_plane_count; i++) {
		feedback_clip_plane_position(CLIPPING.clip_plane[i]);
	}
}



int GPU_get_clip_planes(GPUplane clip_planes_out[])
{
	memcpy(clip_planes_out, CLIPPING.clip_plane, CLIPPING.clip_plane_count * sizeof(GPUplane));

	return CLIPPING.clip_plane_count;
}



#if defined(WITH_GL_PROFILE_COMPAT)
void gpu_toggle_clipping(bool enable)
{
	unsigned int i;

	for (i = 0; i < CLIPPING.clip_plane_count; i++) {
		if (enable)
			glEnable(GL_CLIP_PLANE0 + i); /* deprecated */
		else
			glDisable(GL_CLIP_PLANE0 + i); /* deprecated */
	}
}
#endif

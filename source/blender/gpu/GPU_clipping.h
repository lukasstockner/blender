#ifndef _GPU_CLIPPING_H_
#define _GPU_CLIPPING_H_

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
 * The Original Code is Copyright (C) 2014 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Jason Wilkins
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/gpu/GPU_clipping.h
 *  \ingroup gpu
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct GPUplane {
	double equation[4];
} GPUplane;

/* Set clipping planes and also applies appropriate transformations */
void GPU_set_clip_planes(int clip_plane_count, const GPUplane clip_planes[]);

int GPU_get_clip_planes(GPUplane clip_planes_out[]);

/* Set clip planes without transforming them.
 * Suitable for restoring a backup copy of previous clip plane state.
 * Keeps clip planes from getting transformed twice. */
void GPU_restore_clip_planes(int clip_plane_count, const GPUplane clip_planes[]);

#ifdef __cplusplus
}
#endif

#endif /* _GPU_CLIPPING_H_ */

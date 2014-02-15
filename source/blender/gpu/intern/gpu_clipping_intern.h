#ifndef _GPU_CLIPPING_INTERN_H_
#define _GPU_CLIPPING_INTERN_H_

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

/** \file source/blender/gpu/intern/gpu_clipping_intern.h
 *  \ingroup gpu
 */

#include "GPU_clipping.h"

#include "BLI_sys_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void gpu_clipping_init(void);
void gpu_clipping_exit(void);

void gpu_commit_clipping(void);

#if defined(WITH_GL_PROFILE_COMPAT)
void gpu_toggle_clipping(bool enable);
#endif

#ifdef __cplusplus
}
#endif

#endif /* _GPU_CLIP_PLANES_H_ */

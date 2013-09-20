#ifndef _GPU_RASTER_INTERN_H_
#define _GPU_RASTER_INTERN_H_

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
 * Contributor(s): Jason Wilkins.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file source/blender/gpu/intern/gpu_raster_intern.h
 *  \ingroup gpu
 */

#include "GPU_raster.h"

#include "BLI_sys_types.h" /* for uint32_t */

#ifdef __cplusplus
extern "C" {
#endif

void gpu_raster_init(void);
void gpu_raster_exit(void);

void gpu_raster_enable (uint32_t options);
void gpu_raster_disable(uint32_t options);

void gpu_raster_bind  (void);
void gpu_raster_unbind(void);

void gpu_raster_reset_stipple(void);

#ifdef __cplusplus
}
#endif

#endif /* _GPU_RASTER_INTERN_H_ */

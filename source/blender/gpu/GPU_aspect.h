#ifndef _GPU_ASPECT_H_
#define _GPU_ASPECT_H_

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

/** \file blender/gpu/GPU_aspect.h
  *  \ingroup gpu
  */

#include "BLI_sys_types.h"

#include <string.h> /* for size_t */

#ifdef __cplusplus
extern "C" {
#endif

void GPU_gen_aspects   (size_t count,       uint32_t* aspects);
void GPU_delete_aspects(size_t count, const uint32_t* aspects);

typedef struct GPUaspectimpl {
	bool  (*render_begin )(void* param, const void* object);
	bool  (*render_end   )(void* param, const void* object);
	bool  (*render_commit)(void* param);
	bool  (*select_begin )(void* param, const void* object);
	bool  (*select_end   )(void* param, const void* object);
	bool  (*select_commit)(void* param);
	void  (*enable       )(void* param, uint32_t options);
	void  (*disable      )(void* param, uint32_t options);
	void* param;
} GPUaspectimpl;

void GPU_aspect_impl(uint32_t aspect, GPUaspectimpl* aspectImpl);

bool GPU_aspect_begin(uint32_t aspect, const void* object);
bool GPU_aspect_end  (void);

void GPU_aspect_enable (uint32_t aspect, uint32_t options);
void GPU_aspect_disable(uint32_t aspect, uint32_t options);

bool GPU_commit_aspect(void);

#ifdef __cplusplus
}
#endif

#endif /* _GPU_ASPECT_H_ */

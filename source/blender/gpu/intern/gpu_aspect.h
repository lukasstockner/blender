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

/** \file blender/gpu/intern/gpu_aspect.h
 *  \ingroup gpu
 */

#ifndef GPU_ASPECT_H
#define GPU_ASPECT_H

#include "stddef.h"
#include "BLI_sys_types.h"

#ifdef __cplusplus
extern "C" {
#endif



void GPU_gen_aspects   (size_t count,       uint32_t* aspects);
void GPU_delete_aspects(size_t count, const uint32_t* aspects);

typedef struct GPUaspectfuncs {
	bool  (*begin  )(void* param, const void* object);
	bool  (*end    )(void* param, const void* object);
	void  (*commit )(void* param);
	void  (*enable )(void* param, uint32_t options);
	void  (*disable)(void* param, uint32_t options);
	void* param;
} GPUaspectfuncs;

void GPU_aspect_funcs(uint32_t aspect, GPUaspectfuncs* aspectFuncs);

bool GPU_aspect_begin(uint32_t aspect, const void* object);
bool GPU_aspect_end  (void);

void GPU_aspect_enable (uint32_t aspect, uint32_t options);
void GPU_aspect_disable(uint32_t aspect, uint32_t options);



void gpu_initialize_aspects(void);
void gpu_shutdown_aspects  (void);

void gpu_commit_aspect(void);



#ifdef __cplusplus
}
#endif

#endif /* GPU_ASPECT_H */

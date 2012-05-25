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
* The Original Code is Copyright (C) 2005 Blender Foundation.
* All rights reserved.
*
* The Original Code is: all of this file.
*
* Contributor(s): Jason Wilkins.
*
* ***** END GPL LICENSE BLOCK *****
*/

/** \file blender/gpu/intern/gpu_internal.h
*  \ingroup gpu
*/

#ifndef _GPU_IMMEDIATE_INTERNAL_H_
#define _GPU_IMMEDIATE_INTERNAL_H_

#include "gpu_immediate_inline.h"

#ifdef __cplusplus
extern "C" {
#endif



#ifdef NDEBUG
#define gpu_clear_errors() ((void)0)
#else
BLI_INLINE void gpu_clear_errors()
{
	while (glGetError() != GL_NO_ERROR) {
	}
}
#endif



GLsizei gpu_calc_stride(void);



void gpu_lock_buffer_gl11(void);
void gpu_unlock_buffer_gl11(void);
void gpu_begin_buffer_gl11(void);
void gpu_end_buffer_gl11(void);
void gpu_shutdown_buffer_gl11(GPUimmediate *restrict immediate);



void gpu_lock_buffer_vbo(void);
void gpu_unlock_buffer_vbo(void);
void gpu_begin_buffer_vbo(void);
void gpu_end_buffer_vbo(void);
void gpu_shutdown_buffer_vbo(GPUimmediate *restrict immediate);



#ifdef __cplusplus
}
#endif

#endif /* _GPU_INTERNAL_H_ */

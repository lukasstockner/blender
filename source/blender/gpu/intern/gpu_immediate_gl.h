#ifndef GPU_INTERN_IMMEDIATE_GL_H
#define GPU_INTERN_IMMEDIATE_GL_H

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

/** \file blender/gpu/intern/gpu_internal.h
*  \ingroup gpu
*/

/*

This module contains the backend of the immediate mode replacement.

These are the parts of the code that depend directly on OpenGL.

*/

#include "BLI_utildefines.h" /* for restrict */

#ifdef __cplusplus
extern "C" {
#endif



void gpu_lock_buffer_gl(void);
void gpu_unlock_buffer_gl(void);
void gpu_begin_buffer_gl(void);
void gpu_end_buffer_gl(void);
void gpu_shutdown_buffer_gl(struct GPUimmediate *restrict immediate);
void gpu_current_normal_gl(void);
void gpu_index_begin_buffer_gl(void);
void gpu_index_end_buffer_gl(void);
void gpu_index_shutdown_buffer_gl(struct GPUindex *restrict index);
void gpu_draw_elements_gl(void);
void gpu_draw_range_elements_gl(void);


void gpu_quad_elements_init(void);
void gpu_quad_elements_exit(void);

#ifdef __cplusplus
}
#endif

#endif /* GPU_INTERN_IMMEDIATE_GL */

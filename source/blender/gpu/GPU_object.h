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
 * Contributor(s):
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/gpu/intern/gpu_object.h
 *  \ingroup gpu
 */

typedef struct GPU_object_func
{
	void (*gpuVertexPointer)  (int size, int type, int stride, const void *pointer);
	void (*gpuNormalPointer)  (          int type, int stride, const void *pointer);
	void (*gpuColorPointer )  (int size, int type, int stride, const void *pointer);
	void (*gpuTexCoordPointer)(int size, int type, int stride, const void *pointer);
	void (*gpuColorSet)  (const float *value);


	void (*gpuClientActiveTexture)(int texture);
	void (*gpuCleanupAfterDraw)(void);


} GPU_object_func;



#ifdef __cplusplus
extern "C" {
#endif

extern GPU_object_func gpugameobj;

void GPU_init_object_func(void);

#ifdef __cplusplus
}
#endif

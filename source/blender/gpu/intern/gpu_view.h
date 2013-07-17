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
 * Contributor(s): Alexandr Kuznetsov
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#if !defined(GPU_VIEW_H) || defined(GPU_VIEW_INTERN)
#define GPU_VIEW_H

#ifndef GPU_VIEW_INTERN
#define GPU_VIEW_FUNC extern
#else
#define GPU_VIEW_FUNC
#endif


#ifdef __cplusplus
extern "C" {
#endif

void gpuInitializeViewFuncs(void);

GPU_VIEW_FUNC void (* gpuColorAndClear)(float r, float g, float b, float a);
GPU_VIEW_FUNC void (* gpuClearColor)(float r, float g, float b, float a);

GPU_VIEW_FUNC void (* gpuColorAndClearvf)(float c[3], float a);
GPU_VIEW_FUNC void (* gpuClearColorfv)(float c[3], float a);
GPU_VIEW_FUNC void (* gpuGetClearColor)(float r[4]);

GPU_VIEW_FUNC void (* gpuClear)(int mask);

GPU_VIEW_FUNC void (* gpuViewport)(int x, int y, unsigned int width, unsigned int height);
GPU_VIEW_FUNC void (* gpuScissor)(int x, int y, unsigned int width, unsigned int height);
GPU_VIEW_FUNC void (* gpuViewportScissor)(int x, int y, unsigned int width, unsigned int height);

GPU_VIEW_FUNC void (*gpuGetSizeBox)(int type, int *box);

#ifdef __cplusplus
}
#endif


#endif
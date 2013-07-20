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

#define GPU_VIEW_INTERN
#include "gpu_view.h"
#include "gpu_view_gl.h"


void gpuInitializeViewFuncs(void)
{

gpuColorAndClear = gpuColorAndClear_gl;
gpuClearColor = gpuClearColor_gl;

gpuColorAndClearvf = gpuColorAndClearvf_gl;
gpuClearColorfv = gpuClearColorvf_gl;


gpuViewport = gpuViewport_gl;
gpuScissor = gpuScissor_gl;
gpuViewportScissor = gpuViewportScissor_gl;
gpuGetSizeBox = gpuGetSizeBox_gl;

gpuClear = gpuClear_gl;

}

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


void gpuColorAndClear_gl(float r, float g, float b, float a);

void gpuClearColor_gl(float r, float g, float b, float a);

void gpuColorAndClearvf_gl(float c[3], float a);

void gpuClearColorvf_gl(float c[3], float a);

void gpuGetClearColor_gl(float r[4]);

void gpuViewport_gl(int x, int y, unsigned int width, unsigned int height);

void gpuScissor_gl(int x, int y, unsigned int width, unsigned int height);

void gpuViewportScissor_gl(int x, int y, unsigned int width, unsigned int height);

void gpuGetSizeBox_gl(int type, int *box);

void gpuClear_gl(int mask);

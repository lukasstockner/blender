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

/** \file blender/gpu/intern/gpu_immediate_vbo.c
*  \ingroup gpu
*/

#include "gpu_immediate_internal.h"



typedef struct bufferDataVBO {
	GLint bufferObject;
} bufferDataVBO;



void gpu_begin_buffer_vbo(void)
{
}



void gpu_end_buffer_vbo(void)
{
}



void gpu_lock_buffer_vbo(void)
{
}



void unlockBufferVBO(void)
{
}



void gpu_shutdown_buffer_vbo(GPUimmediate *restrict immediate)
{
	(void)immediate;
}

#ifndef _GPU_BLENDER_ASPECT_H_
#define _GPU_BLENDER_ASPECT_H_

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

/** \file blender/gpu/GPU_blender_aspect.h
 *  \ingroup gpu
 */

#include "GPU_aspect.h"



extern uint32_t GPU_ASPECT_BASIC;
extern uint32_t GPU_ASPECT_CODEGEN;
extern uint32_t GPU_ASPECT_FONT;
extern uint32_t GPU_ASPECT_PIXELS;
extern uint32_t GPU_ASPECT_RASTER;
extern uint32_t GPU_ASPECT_SPRITE;

extern GPUaspectimpl GPU_ASPECTIMPL_BASIC;
extern GPUaspectimpl GPU_ASPECTIMPL_CODEGEN;
extern GPUaspectimpl GPU_ASPECTIMPL_FONT;
extern GPUaspectimpl GPU_ASPECTIMPL_PIXELS;
extern GPUaspectimpl GPU_ASPECTIMPL_RASTER;
extern GPUaspectimpl GPU_ASPECTIMPL_SPRITE;



#endif /* _GPU_BLENDER_ASPECT_H_ */

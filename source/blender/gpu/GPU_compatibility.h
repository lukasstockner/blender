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
 * Contributor(s): Jason Wilkins, Alexandr Kuznetsov
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file GPU_compatibility.h
 *  \ingroup gpu
 */
 
#ifndef __GPU_COMPATIBILITY_H__
#define __GPU_COMPATIBILITY_H__

#include "intern/gpu_immediate_inline.h"
#include "GPU_lighting.h"
#include "GPU_primitives.h" // XXX: temporary, these do not belong here
#include "GPU_colors.h" // XXX: temporary, these do not belong here

#ifndef GPU_MANGLE_DEPRECATED
#define GPU_MANGLE_DEPRECATED 1
#endif

#if GPU_MANGLE_DEPRECATED
#include "intern/gpu_deprecated.h"
#endif



#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#endif /* __GPU_COMPATIBILITY_H_ */

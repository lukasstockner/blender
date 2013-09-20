#ifndef _GPU_BASIC_H_
#define _GPU_BASIC_H_

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
 * Contributor(s): Brecht Van Lommel, Jason Wilkins.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file source/blender/gpu/GPU_basic.h
 *  \ingroup gpu
 */



#ifdef __cplusplus
extern "C" {
#endif



typedef enum GPUBasicOption {
	GPU_BASIC_LIGHTING       = (1<<0), /* do lighting computations                */
	GPU_BASIC_TWO_SIDE       = (1<<1), /* flip back-facing normals towards viewer */
	GPU_BASIC_TEXTURE_2D     = (1<<2), /* use 2D texture to replace diffuse color */
	GPU_BASIC_LOCAL_VIEWER   = (1<<3), /* use for orthographic projection         */
	GPU_BASIC_SMOOTH         = (1<<4), /* use smooth shading                      */
	GPU_BASIC_ALPHATEST      = (1<<5), /* use alpha test                          */

	GPU_BASIC_FAST_LIGHTING  = (1<<6), /* use faster lighting (set automatically) */

	GPU_BASIC_OPTIONS_NUM         = 7,
	GPU_BASIC_OPTION_COMBINATIONS = (1<<GPU_BASIC_OPTIONS_NUM)
} GPUBasicOption;



bool GPU_basic_needs_normals(void);



#ifdef __cplusplus
}
#endif

#endif /* _GPU_BASIC_H_ */

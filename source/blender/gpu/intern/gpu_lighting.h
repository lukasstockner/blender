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
 * Contributor(s): Jason Wilkins
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file gpu_lighting.h
 *  \ingroup gpu
 */

#ifndef GPU_LIGHTING_H
#define GPU_LIGHTING_H

#include "BLI_sys_types.h" // for bool

#ifdef __cplusplus
extern "C" {
#endif



typedef struct GPUbasiclight {
	float position[4];
	float diffuse [4];
	float specular[4];

	float constant_attenuation;
	float linear_attenuation;
	float quadratic_attenuation;

	float spot_direction[3];
	float spot_cutoff;
	float spot_exponent;
} GPUbasiclight;

typedef struct GPUbasicmaterial {
	float diffuse [4];
	float specular[4];
	int   shininess;
} GPUbasicmaterial;

void gpu_commit_light   (void);
void gpu_commit_material(void);
bool gpu_fast_lighting  (void);

void GPU_basic_material(
	const float diffuse[3],
	float       alpha,
	const float specular[3],
	int         shininess);

void GPU_init_basic_lights(int count, GPUbasiclight lights[]);
void GPU_set_basic_light(int light_num, GPUbasiclight *light);


#ifdef __cplusplus
}
#endif

#endif /* GPU_LIGHTING_H */

#ifndef _GPU_LIGHTING_H_
#define _GPU_LIGHTING_H_

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

/** \file source/blender/gpu/GPU_lighting.h
 *  \ingroup gpu
 */

#include "BLI_sys_types.h" /* for bool */

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

void GPU_set_basic_material_shininess(int shininess);
void GPU_set_basic_material_specular(const float specular[4]);

/* Set lights and also applies appropriate transformations on
   the positions and spot directions */
void GPU_set_basic_lights(int light_count, const GPUbasiclight lights[]);

int GPU_get_basic_lights(GPUbasiclight lights_out[]); /* Lights out! Get it? :-) */

/* Set lights without transforming position or spot_direction.
   Suitable for restoring a backup copy of previous light state.
   Keeps position and spot position from getting transformed twice. */
void GPU_restore_basic_lights(int light_count, const GPUbasiclight lights[]);

/* A white directional light shining straight down with no attenuation or spot effects.
   Same as the default legacy OpenGL light #0. */
extern const GPUbasiclight GPU_DEFAULT_LIGHT;

#ifdef __cplusplus
}
#endif

#endif /* _GPU_LIGHTING_H_ */
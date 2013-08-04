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
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Jason Wilkins
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file gpu_lighting.c
 *  \ingroup gpu
 */

/* my interface */
#include "intern/gpu_lighting.h"

/* internal */
#include "intern/gpu_common.h"
#include "intern/gpu_safety.h"

/* my library */
#include "GPU_extensions.h"
#include "GPU_matrix.h"

/* external */
#include "BLI_math_vector.h"



static struct LIGHTING {
	GPUbasiclight    light[GPU_MAX_COMMON_LIGHTS];
	GPUbasicmaterial material;

	uint32_t lights_enabled;
} LIGHTING;



bool gpu_fast_lighting(void)
{
	int i;

	for (i = 0; i < GPU_MAX_COMMON_LIGHTS; i++)
		if (LIGHTING.lights_enabled & (1 << i) && LIGHTING.light[i].position[3] != 0)
				return false; 

	return true;
}



void gpu_commit_light(void)
{
	const GPUcommon*      common  = gpu_get_common();
	const uint32_t lights_enabled = LIGHTING.lights_enabled;
	const GPUbasiclight*  light   = LIGHTING.light;

	int light_count = 0;

	int i;

	for (i = 0; i < GPU_MAX_COMMON_LIGHTS; i++) {
		uint32_t light_bit = (1 << i);

		if (lights_enabled & light_bit) {
			if (common) {
				glUniform4fv(common->light_position             [light_count], 1, light->position);
				glUniform4fv(common->light_diffuse              [light_count], 1, light->diffuse);
				glUniform4fv(common->light_specular             [light_count], 1, light->specular);

				glUniform1f (common->light_constant_attenuation [light_count],    light->constant_attenuation);
				glUniform1f (common->light_linear_attenuation   [light_count],    light->linear_attenuation);
				glUniform1f (common->light_quadratic_attenuation[light_count],    light->quadratic_attenuation);

				glUniform3fv(common->light_spot_direction       [light_count], 1, light->spot_direction);
				glUniform1f (common->light_spot_cutoff          [light_count],    light->spot_cutoff);
				glUniform1f (common->light_spot_exponent        [light_count],    light->spot_exponent);
			}

#if defined(WITH_GL_PROFILE_COMPAT)
			glEnable (GL_LIGHT0+light_count);

			glLightfv(GL_LIGHT0+light_count, GL_POSITION,              light->position);
			glLightfv(GL_LIGHT0+light_count, GL_DIFFUSE,               light->diffuse);
			glLightfv(GL_LIGHT0+light_count, GL_SPECULAR,              light->specular);

			glLightf (GL_LIGHT0+light_count, GL_CONSTANT_ATTENUATION,  light->constant_attenuation);
			glLightf (GL_LIGHT0+light_count, GL_LINEAR_ATTENUATION,    light->linear_attenuation);
			glLightf (GL_LIGHT0+light_count, GL_QUADRATIC_ATTENUATION, light->quadratic_attenuation);

			glLightfv(GL_LIGHT0+light_count, GL_SPOT_DIRECTION,        light->spot_direction);
			glLightf (GL_LIGHT0+light_count, GL_SPOT_CUTOFF,           light->spot_cutoff);
			glLightf (GL_LIGHT0+light_count, GL_SPOT_EXPONENT,         light->spot_exponent);
#endif

			light_count++;
		}
		else {
#if defined(WITH_GL_PROFILE_COMPAT)
			glDisable(GL_LIGHT0+light_count);
#endif
		}
	}

	if (common)
		glUniform1i(common->light_count, light_count);
}



void gpu_commit_material(void)
{
	const GPUcommon*         common   = gpu_get_common();
	const GPUbasicmaterial* material = &(LIGHTING.material);

	if (common) {
		glUniform4fv(common->material_diffuse,   1, material->diffuse);
		glUniform4fv(common->material_specular,  1, material->specular);
		glUniform1i (common->material_shininess,    material->shininess);
	}

#if defined(WITH_GL_PROFILE_COMPAT)
	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE,   material->diffuse);
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR,  material->specular);
	glMateriali (GL_FRONT_AND_BACK, GL_SHININESS, material->shininess);
#endif
}



/* Material Properties */

void GPU_basic_material(
	const float diffuse[3],
	float       alpha,
	const float specular[3],
	int         shininess)
{
	copy_v3_v3(LIGHTING.material.diffuse, diffuse);
	LIGHTING.material.diffuse[3] = alpha;

	copy_v3_v3(LIGHTING.material.specular, specular);
	LIGHTING.material.specular[3] = 1.0f;

	LIGHTING.material.shininess = CLAMPIS(shininess, 1, 128);
}



/* Light Properties */



static void feedback_light_position(float position[4] /* in-out */)
{
	gpuFeedbackVertex4fv(GL_MODELVIEW_MATRIX,  position[0], position[1], position[2], position[3], position);
	gpuFeedbackVertex4fv(GL_PROJECTION_MATRIX, position[0], position[1], position[2], position[4], position);
}



void GPU_set_basic_light(int light_num, GPUbasiclight *light)
{
	const int light_bit = (1 << light_num);

	GPU_ASSERT(light_num < GPU_MAX_COMMON_LIGHTS);

	if (light != NULL) {
		LIGHTING.lights_enabled |= light_bit;

		memcpy(LIGHTING.light + light_num, light, sizeof(GPUbasiclight));

		feedback_light_position(LIGHTING.light->position);
	}
	else {
		LIGHTING.lights_enabled &= ~light_bit;
	}
}



void GPU_init_basic_lights(int count, GPUbasiclight lights[])
{
	int i;

	GPU_ASSERT(count < GPU_MAX_COMMON_LIGHTS);

	memcpy(LIGHTING.light, lights, count*sizeof(GPUbasiclight));

	LIGHTING.lights_enabled = (1<<count)-1; /* Make a bitmask with 'count' # of bits set. */

	for (i = 0; i < GPU_MAX_COMMON_LIGHTS; i++)
		feedback_light_position(LIGHTING.light[i].position);
}

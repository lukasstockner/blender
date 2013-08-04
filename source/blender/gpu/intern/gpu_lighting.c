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

	uint32_t light_count;
} LIGHTING;




const GPUbasiclight GPU_DEFAULT_LIGHT =
{
	{ 0, 0, 1, 0 }, /* position: directional light that is straight above (in eye coordinates) */
	{ 1, 1, 1, 1 }, /* diffuse : white                                                         */
	{ 1, 1, 1, 1 }, /* specular: white                                                         */
	1, 0, 0,        /* attenuation polynomal coefficients: no attenuation                      */
	{ 0, 0, 1 },    /* spotlight direction: straight ahead (in eye coordinates)                */
	180, 0          /* spotlight parameters: no spotlight                                      */
};


bool gpu_fast_lighting(void)
{
	int i;

	for (i = 0; i < LIGHTING.light_count; i++)
		if (LIGHTING.light[i].position[3] != 0)
				return false; 

	return true;
}



void gpu_commit_light(void)
{
	const GPUcommon*     common = gpu_get_common();
	const GPUbasiclight* light  = LIGHTING.light;

	int i;

	for (i = 0; i < LIGHTING.light_count; i++) {
		if (common) {
			glUniform4fv(common->light_position             [i], 1, light->position);
			glUniform4fv(common->light_diffuse              [i], 1, light->diffuse);
			glUniform4fv(common->light_specular             [i], 1, light->specular);

			glUniform1f (common->light_constant_attenuation [i],    light->constant_attenuation);
			glUniform1f (common->light_linear_attenuation   [i],    light->linear_attenuation);
			glUniform1f (common->light_quadratic_attenuation[i],    light->quadratic_attenuation);

			glUniform3fv(common->light_spot_direction       [i], 1, light->spot_direction);
			glUniform1f (common->light_spot_cutoff          [i],    light->spot_cutoff);
			glUniform1f (common->light_spot_exponent        [i],    light->spot_exponent);
		}

#if defined(WITH_GL_PROFILE_COMPAT)
		if (i < 8) {
			glEnable (GL_LIGHT0+i);

			glLightfv(GL_LIGHT0+i, GL_POSITION,              light->position);
			glLightfv(GL_LIGHT0+i, GL_DIFFUSE,               light->diffuse);
			glLightfv(GL_LIGHT0+i, GL_SPECULAR,              light->specular);

			glLightf (GL_LIGHT0+i, GL_CONSTANT_ATTENUATION,  light->constant_attenuation);
			glLightf (GL_LIGHT0+i, GL_LINEAR_ATTENUATION,    light->linear_attenuation);
			glLightf (GL_LIGHT0+i, GL_QUADRATIC_ATTENUATION, light->quadratic_attenuation);

			glLightfv(GL_LIGHT0+i, GL_SPOT_DIRECTION,        light->spot_direction);
			glLightf (GL_LIGHT0+i, GL_SPOT_CUTOFF,           light->spot_cutoff);
			glLightf (GL_LIGHT0+i, GL_SPOT_EXPONENT,         light->spot_exponent);
		}
#endif

		light++;
	}

#if defined(WITH_GL_PROFILE_COMPAT)
	for (; i < 8; i++) {
		glDisable(GL_LIGHT0+i);
	}
#endif

	if (common)
		glUniform1i(common->light_count, LIGHTING.light_count);
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

void GPU_set_basic_material(
	const float diffuse[3],
	float       alpha,
	const float specular[3],
	int         shininess)
{
	copy_v3_v3(LIGHTING.material.diffuse, diffuse);
	LIGHTING.material.diffuse[3] = alpha;

	GPU_set_basic_material_specular(specular);

	LIGHTING.material.shininess = CLAMPIS(shininess, 1, 128);
}



void GPU_set_basic_material_specular(const float specular[3])
{
	copy_v3_v3(LIGHTING.material.specular, specular);
	LIGHTING.material.specular[3] = 1.0f;
}



/* Light Properties */



static void feedback_light_position(float position[4] /* in-out */)
{
	gpuFeedbackVertex4fv(GL_MODELVIEW_MATRIX,  position[0], position[1], position[2], position[3], position);
	gpuFeedbackVertex4fv(GL_PROJECTION_MATRIX, position[0], position[1], position[2], position[4], position);
}



static void feedback_spot_direction(float spot_direction[4] /* in-out */)
{
	// XXX jwilkins: ToDo!!
}



void GPU_restore_basic_lights(int light_count, GPUbasiclight lights[])
{
	GPU_ASSERT(light_count < GPU_MAX_COMMON_LIGHTS);

	memcpy(LIGHTING.light, lights, light_count*sizeof(GPUbasiclight));

	LIGHTING.light_count = light_count;
}



void GPU_set_basic_lights(int light_count, GPUbasiclight lights[])
{
	int i;

	GPU_restore_basic_lights(light_count, lights);

	for (i = 0; i < light_count; i++) {
		feedback_light_position(LIGHTING.light[i].position);
		feedback_spot_direction(LIGHTING.light[i].spot_direction);
	}
}



int GPU_get_basic_lights(GPUbasiclight lights_out[])
{
	memcpy(lights_out, LIGHTING.light, LIGHTING.light_count*sizeof(GPUbasiclight));

	return LIGHTING.light_count;
}

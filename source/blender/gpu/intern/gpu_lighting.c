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

/** \file source/blender/gpu/intern/gpu_lighting.c
 *  \ingroup gpu
 */

#if WITH_GL_PROFILE_COMPAT
#define GPU_MANGLE_DEPRECATED 0 /* Allow use of deprecated OpenGL functions in this file */
#endif

#include "BLI_sys_types.h"

#include "intern/gpu_private.h"

#include "GPU_extensions.h"
#include "GPU_matrix.h"
#include "GPU_debug.h"
#include "GPU_common.h"
#include "GPU_lighting.h"

/* external */
#include "BLI_math_vector.h"



typedef struct GPUbasicmaterial {
	float specular[4];
	int shininess;
} GPUbasicmaterial;

static struct LIGHTING {
	GPUbasiclight light[GPU_MAX_COMMON_LIGHTS];
	GPUbasicmaterial material;

	uint32_t light_count;
} LIGHTING;




const GPUbasiclight GPU_DEFAULT_LIGHT =
{
	{ 0, 0, 1, 0 }, /* position: directional light that is straight above (in eye coordinates) */
	{ 1, 1, 1, 1 }, /* diffuse: white                                                          */
	{ 1, 1, 1, 1 }, /* specular: white                                                         */
	1, 0, 0,        /* attenuation polynomal coefficients: no attenuation                      */
	{ 0, 0, 1 },    /* spotlight direction: straight ahead (in eye coordinates)                */
	180, 0          /* spotlight parameters: no spotlight                                      */
};



void gpu_lighting_init(void)
{
	GPU_restore_basic_lights(1, &GPU_DEFAULT_LIGHT);
}



void gpu_lighting_exit(void)
{
}



bool gpu_lighting_is_fast(void)
{
	int i;

	for (i = 0; i < LIGHTING.light_count; i++)
		if (LIGHTING.light[i].position[3] != 0)
				return false;

	return true;
}



void gpu_commit_lighting(void)
{
	const struct GPUcommon *common = gpu_get_common();
	const struct GPUbasiclight *light = LIGHTING.light;

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
			glUniform1f (common->light_spot_cos_cutoff      [i],    DEG2RAD(light->spot_cutoff));
			glUniform1f (common->light_spot_exponent        [i],    light->spot_exponent);
		}

#if defined(WITH_GL_PROFILE_COMPAT)
		/* use deprecated lighting functions */
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
	const struct GPUcommon *common = gpu_get_common();
	const struct GPUbasicmaterial *material = &LIGHTING.material;

GPU_ASSERT_NO_GL_ERRORS("");
	if (common) {
		glUniform4fv(common->material_specular, 1, material->specular);
GPU_ASSERT_NO_GL_ERRORS("");
		glUniform1f(common->material_shininess, (float)material->shininess);
GPU_ASSERT_NO_GL_ERRORS("");
	}

#if defined(WITH_GL_PROFILE_COMPAT)
	/* use deprecated material functions */
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, material->specular);
	glMateriali(GL_FRONT_AND_BACK, GL_SHININESS, material->shininess);
#endif
GPU_ASSERT_NO_GL_ERRORS("");
}



/* Material Properties */

void GPU_set_basic_material_shininess(int shininess)
{
	LIGHTING.material.shininess = CLAMPIS(shininess, 1, 128);
}



void GPU_set_basic_material_specular(const float specular[4])
{
	copy_v4_v4(LIGHTING.material.specular, specular);
}



/* Light Properties */



void GPU_restore_basic_lights(int light_count, const GPUbasiclight lights[])
{
	BLI_assert(light_count >= 0);
	BLI_assert(light_count < GPU_MAX_COMMON_LIGHTS);

	memcpy(LIGHTING.light, lights, light_count * sizeof(GPUbasiclight));

	LIGHTING.light_count = light_count;
}



static void feedback_light_position(float position[4] /* in-out */)
{
	GPU_feedback_vertex_4fv(GL_MODELVIEW_MATRIX,  position[0], position[1], position[2], position[3], position);
}



static void feedback_spot_direction(float spot_direction[3] /* in-out */)
{
	float n[3][3];

	copy_m3_m4(n, (float (*)[4])gpuGetMatrix(GL_MODELVIEW_MATRIX, NULL));
	mul_m3_v3(n, spot_direction);
}



void GPU_set_basic_lights(int light_count, const GPUbasiclight lights[])
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
	memcpy(lights_out, LIGHTING.light, LIGHTING.light_count * sizeof(GPUbasiclight));

	return LIGHTING.light_count;
}

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

/** \file gpu_lighting.c
 *  \ingroup gpu
 */

#include "gpu_lighting_internal.h"
#include "GPU_extensions.h"

#include "gpu_safety.h"
#include "gpu_known.h"

#include "MEM_guardedalloc.h"

#if 0
GPUlighting *restrict GPU_LIGHTING = NULL;



void gpuInitializeLighting(void)
{
	GPU_LIGHTING =
		(GPUlighting*)MEM_callocN(sizeof(GPUlighting), "GPU_LIGHTING");

#if defined(WITH_GL_PROFILE_ES20) || defined(WITH_GL_PROFILE_CORE)
	GPU_LIGHTING->material_fv            = gpu_material_fv_glsl;
	GPU_LIGHTING->material_i             = gpu_material_i_glsl;
	GPU_LIGHTING->get_material_fv        = gpu_get_material_fv_glsl;
	GPU_LIGHTING->color_material         = gpu_color_material_glsl;
	GPU_LIGHTING->enable_color_material  = gpu_enable_color_material_glsl;
	GPU_LIGHTING->disable_color_material = gpu_disable_color_material_glsl;
	GPU_LIGHTING->light_f                = gpu_light_f_glsl;
	GPU_LIGHTING->light_fv               = gpu_light_fv_glsl;
	GPU_LIGHTING->enable_light           = gpu_enable_light_glsl;
	GPU_LIGHTING->disable_light          = gpu_disable_light_glsl;
	GPU_LIGHTING->is_light_enabled       = gpu_is_light_enabled_glsl;
	GPU_LIGHTING->light_model_i          = gpu_light_model_i_glsl;
	GPU_LIGHTING->light_model_fv         = gpu_light_model_fv_glsl;
	GPU_LIGHTING->enable_lighting        = gpu_enable_lighting_glsl;
	GPU_LIGHTING->disable_lighting       = gpu_disable_lighting_glsl;
	GPU_LIGHTING->is_lighting_enabled    = gpu_is_lighting_enabled_glsl;
#else
	GPU_LIGHTING->material_fv            = gpu_material_fv_gl11;
	GPU_LIGHTING->material_i             = gpu_material_i_gl11;
	GPU_LIGHTING->get_material_fv        = gpu_get_material_fv_gl11;
	GPU_LIGHTING->color_material         = gpu_color_material_gl11;
	GPU_LIGHTING->enable_color_material  = gpu_enable_color_material_gl11;
	GPU_LIGHTING->disable_color_material = gpu_disable_color_material_gl11;
	GPU_LIGHTING->light_f                = gpu_light_f_gl11;
	GPU_LIGHTING->light_fv               = gpu_light_fv_gl11;
	GPU_LIGHTING->enable_light           = gpu_enable_light_gl11;
	GPU_LIGHTING->disable_light          = gpu_disable_light_gl11;
	GPU_LIGHTING->is_light_enabled       = gpu_is_light_enabled_gl11;
	GPU_LIGHTING->light_model_i          = gpu_light_model_i_gl11;
	GPU_LIGHTING->light_model_fv         = gpu_light_model_fv_gl11;
	GPU_LIGHTING->enable_lighting        = gpu_enable_lighting_gl11;
	GPU_LIGHTING->disable_lighting       = gpu_disable_lighting_gl11;
	GPU_LIGHTING->is_lighting_enabled    = gpu_is_lighting_enabled_gl11;
#endif
}



void gpuShutdownLighting(void)
{
	MEM_freeN(GPU_LIGHTING);
	GPU_LIGHTING = NULL;
}


// XXX jwilkins: these should probably be a part of the GPU_LIGHTING driver, but not sure atm

void gpuShadeModel(GLenum model)
{
	GPU_CHECK_NO_ERROR();

#if defined(WITH_GL_PROFILE_COMPAT)
	glShadeModel(model);
#endif
}
#endif



void GPU_commit_light(uint32_t lights_enabled, const GPUsimplelight *light)
{
	int i;
	int light_count = 0;
	const GPUknownlocs* location = GPU_get_known_locations();

	for (i = 0; i < GPU_MAX_KNOWN_LIGHTS; i++) {
		uint32_t light_bit = (1 << i);

		if (lights_enabled & light_bit) {
			if (GPU_glsl_support()) {
				glUniform4fv(location->light_position             [light_count], 1, light->position);
				glUniform4fv(location->light_diffuse              [light_count], 1, light->diffuse);
				glUniform4fv(location->light_specular             [light_count], 1, light->specular);

				glUniform1f (location->light_constant_attenuation [light_count],    light->constant_attenuation);
				glUniform1f (location->light_linear_attenuation   [light_count],    light->linear_attenuation);
				glUniform1f (location->light_quadratic_attenuation[light_count],    light->quadratic_attenuation);

				glUniform3fv(location->light_spot_direction       [light_count], 1, light->spot_direction);
				glUniform1f (location->light_spot_cutoff          [light_count],    light->spot_cutoff);
				glUniform1f (location->light_spot_exponent        [light_count],    light->spot_exponent);
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

	glUniform1i(location->light_count, light_count);
}



void GPU_commit_material(const GPUsimplematerial* material)
{
	const GPUknownlocs* location = GPU_get_known_locations();

	if (GPU_glsl_support()) {
		glUniform4fv(location->material_diffuse,   1, material->diffuse);
		glUniform4fv(location->material_specular,  1, material->specular);
		glUniform1i (location->material_shininess,    material->shininess);
	}

#if defined(WITH_GL_PROFILE_COMPAT)
	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE,   SIMPLE_SHADER.material_diffuse);
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR,  SIMPLE_SHADER.material_specular);
	glMateriali (GL_FRONT_AND_BACK, GL_SHININESS, SIMPLE_SHADER.material_shininess);
#endif
}

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
 * Contributor(s): Brecht Van Lommel.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/gpu/intern/gpu_simple_shader.c
 *  \ingroup gpu
 */

/* GLSL shaders to replace fixed function OpenGL materials and lighting. These
 * are deprecated in newer OpenGL versions and missing in OpenGL ES 2.0. Also,
 * two sided lighting is no longer natively supported on NVidia cards which
 * results in slow software fallback.
 *
 * Todo:
 * x Replace glLight and glMaterial functions entirely with GLSL uniforms, to make OpenGL ES 2.0 work.
 * x Replace glTexCoord and glColor with generic attributes.
 * x Optimize for case where fewer than 3 or 8 lights are used.
 * - Optimize for case where specular is not used.
 * - Optimize for case where no texture matrix is used.
 */

#include "gpu_glew.h"
#include "gpu_safety.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "BLI_dynstr.h"

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"

#include "GPU_extensions.h"
#include "GPU_simple_shader.h"
#include "gpu_profile.h"

#include "GPU_matrix.h"
#include "intern/gpu_lighting.h"
#include "intern/gpu_known.h"
#include "intern/gpu_immediate.h"



/* State */

static struct SIMPLE_SHADER {
	GPUShader*   gpushader[GPU_SHADER_OPTION_COMBINATIONS];
	bool         failed   [GPU_SHADER_OPTION_COMBINATIONS];
	GPUknownlocs locations[GPU_SHADER_OPTION_COMBINATIONS];

	GPUsimplelight light[GPU_MAX_KNOWN_LIGHTS];

	bool need_normals;

	uint32_t lights_enabled;

	GPUsimplematerial material;
} SIMPLE_SHADER;



/* Init / exit */

void GPU_simple_shaders_init(void)
{
	memset(&SIMPLE_SHADER, 0, sizeof(SIMPLE_SHADER));
}



void GPU_simple_shaders_exit(void)
{
	int i;
	
	for (i = 0; i < GPU_SHADER_OPTION_COMBINATIONS; i++)
		if (SIMPLE_SHADER.gpushader[i] != NULL)
			GPU_shader_free(SIMPLE_SHADER.gpushader[i]);
}



/* Shader lookup / create */

static bool fast_lighting(void)
{
	int i;

	for (i = 0; i < GPU_MAX_KNOWN_LIGHTS; i++) {
		if (SIMPLE_SHADER.lights_enabled & (1 << i)) {
			if (SIMPLE_SHADER.light[i].constant_attenuation  != 1 ||
				SIMPLE_SHADER.light[i].linear_attenuation    != 0 ||
				SIMPLE_SHADER.light[i].quadratic_attenuation != 0 ||
				SIMPLE_SHADER.light[i].position[3]           != 0)
			{
				return false; 
			}
		}
	}

	return true;
}



static GPUShader *gpu_simple_shader(uint32_t options)
{
	/* glsl code */
	extern const char datatoc_gpu_shader_simple_vert_glsl[];
	extern const char datatoc_gpu_shader_simple_frag_glsl[];

	GPU_ASSERT(!(options & GPU_SHADER_FAST_LIGHTING));

	/* detect if we can do faster lighting for solid draw mode */
	if (options & GPU_SHADER_LIGHTING && fast_lighting())
		options |= GPU_SHADER_FAST_LIGHTING;

	/* create shader if it doesn't exist yet */
	if ((SIMPLE_SHADER.gpushader[options] != NULL) && !(SIMPLE_SHADER.failed[options])) {
		DynStr* vert = BLI_dynstr_new();
		DynStr* frag = BLI_dynstr_new();
		DynStr* defs = BLI_dynstr_new();

		BLI_dynstr_append(vert, datatoc_gpu_shader_known_constants_glsl);
		BLI_dynstr_append(vert, datatoc_gpu_shader_known_uniforms_glsl);
		BLI_dynstr_append(vert, datatoc_gpu_shader_known_attribs_glsl);
		BLI_dynstr_append(vert, datatoc_gpu_shader_simple_vert_glsl);

		BLI_dynstr_append(frag, datatoc_gpu_shader_known_constants_glsl);
		BLI_dynstr_append(frag, datatoc_gpu_shader_known_uniforms_glsl);
		BLI_dynstr_append(frag, datatoc_gpu_shader_simple_frag_glsl);

		BLI_dynstr_append(defs, "#define GPU_MAX_KNOWN_TEXCOORDS " STRINGIFY(GPU_MAX_KNOWN_TEXCOORDS) "\n");
		BLI_dynstr_append(defs, "#define GPU_MAX_KNOWN_SAMPLERS  " STRINGIFY(GPU_MAX_KNOWN_SAMPLERS ) "\n");

		if (options & GPU_SHADER_MATERIAL_ONLY)
			BLI_dynstr_append(defs, "#define USE_MATERAL_ONLY\n");

		if (options & GPU_SHADER_TWO_SIDED)
			BLI_dynstr_append(defs, "#define USE_TWO_SIDED\n");

		if (options & GPU_SHADER_TEXTURE_2D)
			BLI_dynstr_append(defs, "#define USE_TEXTURE\n");

		if (options & GPU_SHADER_LOCAL_VIEWER)
			BLI_dynstr_append(defs, "#define USE_LOCAL_VIEWER\n");

		if (options & GPU_SHADER_FLAT_SHADED)
			BLI_dynstr_append(defs, "#define USE_FLAT_SHADING\n");

		if (options & GPU_SHADER_LIGHTING) {
			BLI_dynstr_append(defs, "#define USE_LIGHTING\n");

			if (options & GPU_SHADER_FAST_LIGHTING)
				BLI_dynstr_append(defs, "#define USE_FAST_LIGHTING\n");
			else if (options & GPU_SHADER_LIGHTING)
				BLI_dynstr_append(defs, "#define USE_SCENE_LIGHTING\n");
		}

		SIMPLE_SHADER.gpushader[options] =
			GPU_shader_create(
				BLI_dynstr_get_cstring(vert),
				BLI_dynstr_get_cstring(frag),
				NULL,
				BLI_dynstr_get_cstring(defs));

		if (SIMPLE_SHADER.gpushader[options] != NULL) {
			int i;

			GPU_set_known_locations(SIMPLE_SHADER.locations + options, SIMPLE_SHADER.gpushader[options]);

			/* the mapping between samplers and texture units is static, so it can committed here once */
			for (i = 0; i < GPU_MAX_KNOWN_SAMPLERS; i++)
				glUniform1i(SIMPLE_SHADER.locations[options].sampler2D[i], i);
		}
		else {
			SIMPLE_SHADER.failed[options] = true;
		}
	}

	return SIMPLE_SHADER.gpushader[options];
}



/* Bind / unbind */



void GPU_simple_shader_bind(uint32_t options)
{
	if (GPU_glsl_support()) {
		GPUShader *gpushader = gpu_simple_shader(options);

		if (gpushader)
			GPU_shader_bind(gpushader);
	}

#if defined(WITH_GL_PROFILE_COMPAT)
	// only change state if it is different than the Blender default

	if (option & GPU_SHADER_LIGHTING)
		glEnable(GL_LIGHTING);

	if (option & GPU_SHADER_MATERIAL_ONLY)
		glDisable(GL_COLOR_MATERIAL);

	if (option & GPU_SHADER_TEXTURE_2D)
		glEnable(GL_TEXTURE_2D);

	if (options & GPU_SHADER_TWO_SIDED)
		glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);

	if (options & GPU_SHADER_LOCAL_VIEWER)
		glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_TRUE);

	if (options & GPU_SHADER_FLAT_SHADED)
		glShadeModel(GL_FLAT);
#endif

	GPU_commit_light   (SIMPLE_SHADER.lights_enabled, SIMPLE_SHADER.light);
	GPU_commit_material(&(SIMPLE_SHADER.material));

	/* temporary hack, should be solved outside of this file */
	SIMPLE_SHADER.need_normals = (options & GPU_SHADER_LIGHTING);
}



void GPU_simple_shader_unbind(void)
{
	if (GPU_glsl_support())
		GPU_shader_unbind();

#if defined(WITH_GL_PROFILE_COMPAT)
	// if state was switched from the Blender default, reset it

	if (option & GPU_SHADER_LIGHTING)
		gDisable(GL_LIGHTING);

	if (option & GPU_SHADER_IGNORE_COLOR_ATTRIB)
		glEnable(GL_COLOR_MATERIAL);

	if (option & GPU_SHADER_TEXTURE_2D)
		glDisable(GL_TEXTURE_2D);

	if (options & GPU_SHADER_TWO_SIDED)
		glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_FALSE);

	if (options & GPU_SHADER_LOCAL_VIEWER)
		glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_FALSE);

	if (options & GPU_SHADER_FLAT_SHADED)
		glShadeModel(GL_SMOOTH);
#endif

	/* temporary hack, should be solved outside of this file */
	SIMPLE_SHADER.need_normals = false;
}



/* Material Properties */

void GPU_simple_shader_material(
	const float diffuse[3],
	float       alpha,
	const float specular[3],
	int         shininess)
{
	copy_v3_v3(SIMPLE_SHADER.material.diffuse, diffuse);
	SIMPLE_SHADER.material.diffuse[3] = alpha;

	copy_v3_v3(SIMPLE_SHADER.material.specular, specular);
	SIMPLE_SHADER.material.specular[3] = 1.0f;

	SIMPLE_SHADER.material.shininess = CLAMPIS(shininess, 1, 128);
}



bool GPU_simple_shader_need_normals(void)
{
	return SIMPLE_SHADER.need_normals;
}



void GPU_simple_shader_light(int light_num, GPUsimplelight *light)
{
	const int light_bit = (1 << light_num);

	GPU_ASSERT(light_num < GPU_MAX_KNOWN_LIGHTS);

	if (light != NULL) {
		SIMPLE_SHADER.lights_enabled |= light_bit;

		memcpy(SIMPLE_SHADER.light + light_num, light, sizeof(GPUsimplelight));
	}
	else {
		SIMPLE_SHADER.lights_enabled &= ~light_bit;
	}
}



void GPU_simple_shader_multiple_lights(int count, GPUsimplelight lights[])
{
	GPU_ASSERT(count < GPU_MAX_KNOWN_LIGHTS);

	memcpy(SIMPLE_SHADER.light, lights, count*sizeof(GPUsimplelight));
}



static GPUShader* FONT_SHADER = NULL;
static GPUknownlocs FONT_LOCS;
static bool FONT_FAILED = FALSE;



void GPU_font_shader_init(void)
{
	FONT_SHADER = NULL;
}



void GPU_font_shader_exit(void)
{
	GPU_shader_free(FONT_SHADER);
}



static GPUShader* gpu_font_shader(void)
{
	/* glsl code */
	extern const char datatoc_gpu_shader_font_vert_glsl[];
	extern const char datatoc_gpu_shader_font_frag_glsl[];

	/* create shader if it doesn't exist yet */
	if (FONT_SHADER == NULL && !FONT_FAILED) {
		DynStr* vert = BLI_dynstr_new();
		DynStr* frag = BLI_dynstr_new();
		DynStr* defs = BLI_dynstr_new();

		BLI_dynstr_append(vert, datatoc_gpu_shader_known_constants_glsl);
		BLI_dynstr_append(vert, datatoc_gpu_shader_known_uniforms_glsl);
		BLI_dynstr_append(vert, datatoc_gpu_shader_known_attribs_glsl);
		BLI_dynstr_append(vert, datatoc_gpu_shader_font_vert_glsl);

		BLI_dynstr_append(frag, datatoc_gpu_shader_known_constants_glsl);
		BLI_dynstr_append(frag, datatoc_gpu_shader_known_uniforms_glsl);
		BLI_dynstr_append(frag, datatoc_gpu_shader_font_frag_glsl);

		BLI_dynstr_append(defs, "#define GPU_MAX_KNOWN_TEXCOORDS " STRINGIFY(GPU_MAX_KNOWN_TEXCOORDS) "\n");
		BLI_dynstr_append(defs, "#define GPU_MAX_KNOWN_SAMPLERS  " STRINGIFY(GPU_MAX_KNOWN_SAMPLERS ) "\n");

		BLI_dynstr_append(defs, "#define USE_TEXTURE\n");

		FONT_SHADER =
			GPU_shader_create(
				BLI_dynstr_get_cstring(vert),
				BLI_dynstr_get_cstring(frag),
				NULL,
				BLI_dynstr_get_cstring(defs));

		if (FONT_SHADER != NULL) {
			int i;

			GPU_set_known_locations(&FONT_LOCS, FONT_SHADER);

			/* the mapping between samplers and texture units is static, so it can committed here once */
			for (i = 0; i < GPU_MAX_KNOWN_SAMPLERS; i++)
				glUniform1i(FONT_LOCS.sampler2D[i], i);
		}
		else {
			FONT_FAILED = true;
		}
	}

	return FONT_SHADER;
}



void GPU_font_shader_bind(void)
{
	if (GPU_glsl_support()) {
		GPUShader *gpushader = gpu_font_shader();

		if (gpushader)
			GPU_shader_bind(gpushader);
	}

#if defined(WITH_GL_PROFILE_COMPAT)
	glEnable(GL_TEXTURE_2D);
#endif
}



void GPU_font_shader_unbind(void)
{
	GPU_shader_unbind();

#if defined(WITH_GL_PROFILE_COMPAT)
	glDisable(GL_TEXTURE_2D);
#endif
}

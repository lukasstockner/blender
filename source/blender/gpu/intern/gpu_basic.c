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
 * Contributor(s): Brecht Van Lommel, Jason Wilkins.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file source/blender/gpu/intern/gpu_basic.c
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

#if WITH_GL_PROFILE_COMPAT
#define GPU_MANGLE_DEPRECATED 0 /* Allow use of deprecated OpenGL functions in this file */
#endif

#include "BLI_sys_types.h"

#include "GPU_aspect.h"
#include "GPU_extensions.h"
#include "GPU_state_latch.h"
#include "GPU_debug.h"

/* internal */
#include "intern/gpu_private.h"

/* external */
#include "BLI_math.h"
#include "BLI_dynstr.h"

#include "MEM_guardedalloc.h"

/* standard */
#include <string.h>



/* State */

static struct BASIC_SHADER {
	uint32_t options;

	GPUShader*   gpushader[GPU_BASIC_OPTION_COMBINATIONS];
	bool         failed   [GPU_BASIC_OPTION_COMBINATIONS];
	GPUcommon    common   [GPU_BASIC_OPTION_COMBINATIONS];

} BASIC_SHADER;



/* Init / exit */

void gpu_basic_init(void)
{
	memset(&BASIC_SHADER, 0, sizeof(BASIC_SHADER));

#if 0
	{ 	/* for testing purposes, mark all shaders as failed*/
		int i;
		for (i = 0; i < GPU_BASIC_OPTION_COMBINATIONS; i++)
			BASIC_SHADER.failed[i] = true;
	}
#endif
}



void gpu_basic_exit(void)
{
	int i;

	for (i = 0; i < GPU_BASIC_OPTION_COMBINATIONS; i++)
		if (BASIC_SHADER.gpushader[i] != NULL)
			GPU_shader_free(BASIC_SHADER.gpushader[i]);
}



/* Shader feature enable/disable */

void gpu_basic_enable(uint32_t options)
{
	BLI_assert(!(options & GPU_BASIC_FAST_LIGHTING));

	BASIC_SHADER.options |= options;
}



void gpu_basic_disable(uint32_t options)
{
	BASIC_SHADER.options &= ~options;
}



/* Shader lookup / create */

static uint32_t tweak_options(void)
{
	uint32_t options;

	options = BASIC_SHADER.options;

	/* detect if we can do faster lighting for solid draw mode */
	if (  BASIC_SHADER.options & GPU_BASIC_LIGHTING      &&
		!(BASIC_SHADER.options & GPU_BASIC_LOCAL_VIEWER) &&
		gpu_lighting_is_fast())
	{
		options |= GPU_BASIC_FAST_LIGHTING;
	}

	if (gpuGetTextureBinding2D() == 0)
		options &= ~GPU_BASIC_TEXTURE_2D;

	return options;
}



static void basic_shader_bind(void)
{
	/* glsl code */
	extern const char datatoc_gpu_shader_basic_vert_glsl[];
	extern const char datatoc_gpu_shader_basic_frag_glsl[];

	const uint32_t tweaked_options = tweak_options();

	/* create shader if it doesn't exist yet */
	if (BASIC_SHADER.gpushader[tweaked_options] != NULL) {
		GPU_shader_bind(BASIC_SHADER.gpushader[tweaked_options]);
		gpu_set_common(BASIC_SHADER.common + tweaked_options);
	}
	else if (!BASIC_SHADER.failed[tweaked_options]) {
		DynStr* vert = BLI_dynstr_new();
		DynStr* frag = BLI_dynstr_new();
		DynStr* defs = BLI_dynstr_new();

		char* vert_cstring;
		char* frag_cstring;
		char* defs_cstring;

		gpu_include_common_vert(vert);
		BLI_dynstr_append(vert, datatoc_gpu_shader_basic_vert_glsl);

		gpu_include_common_frag(frag);
		BLI_dynstr_append(frag, datatoc_gpu_shader_basic_frag_glsl);

		gpu_include_common_defs(defs);

		if (tweaked_options & GPU_BASIC_TWO_SIDE)
			BLI_dynstr_append(defs, "#define USE_TWO_SIDE\n");

		if (tweaked_options & GPU_BASIC_TEXTURE_2D)
			BLI_dynstr_append(defs, "#define USE_TEXTURE_2D\n");

		if (tweaked_options & GPU_BASIC_LOCAL_VIEWER)
			BLI_dynstr_append(defs, "#define USE_LOCAL_VIEWER\n");

		if (tweaked_options & GPU_BASIC_SMOOTH)
			BLI_dynstr_append(defs, "#define USE_SMOOTH\n");

		if (tweaked_options & GPU_BASIC_LIGHTING) {
			BLI_dynstr_append(defs, "#define USE_LIGHTING\n");
			BLI_dynstr_append(defs, "#define USE_SPECULAR\n");

			if (tweaked_options & GPU_BASIC_FAST_LIGHTING)
				BLI_dynstr_append(defs, "#define USE_FAST_LIGHTING\n");
		}

		vert_cstring = BLI_dynstr_get_cstring(vert);
		frag_cstring = BLI_dynstr_get_cstring(frag);
		defs_cstring = BLI_dynstr_get_cstring(defs);

		BASIC_SHADER.gpushader[tweaked_options] =
			GPU_shader_create(vert_cstring, frag_cstring, NULL, defs_cstring);

		MEM_freeN(vert_cstring);
		MEM_freeN(frag_cstring);
		MEM_freeN(defs_cstring);

		BLI_dynstr_free(vert);
		BLI_dynstr_free(frag);
		BLI_dynstr_free(defs);

		if (BASIC_SHADER.gpushader[tweaked_options] != NULL) {
			gpu_common_get_symbols(BASIC_SHADER.common + tweaked_options, BASIC_SHADER.gpushader[tweaked_options]);
			gpu_set_common(BASIC_SHADER.common + tweaked_options);

			GPU_shader_bind(BASIC_SHADER.gpushader[tweaked_options]);
		}
		else {
			BASIC_SHADER.failed[tweaked_options] = true;
			gpu_set_common(NULL);
		}
	}
	else {
		gpu_set_common(NULL);
	}
}



/* Bind / Unbind */

void gpu_basic_bind(void)
{
	bool glsl_support = GPU_glsl_support();

	if (glsl_support) {
		basic_shader_bind();
	}

#if defined(WITH_GL_PROFILE_COMPAT)
	GPU_ASSERT_NO_GL_ERRORS("gpu_basic_bind start");

	if (!glsl_support) {
		if (BASIC_SHADER.options & GPU_BASIC_LIGHTING)
			glEnable(GL_LIGHTING);
		else
			glDisable(GL_LIGHTING);

		if (BASIC_SHADER.options & GPU_BASIC_TEXTURE_2D)
			glEnable(GL_TEXTURE_2D);
		else
			glDisable(GL_TEXTURE_2D);

		if (BASIC_SHADER.options & GPU_BASIC_TWO_SIDE)
			glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);
		else
			glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_FALSE);

		if (BASIC_SHADER.options & GPU_BASIC_LOCAL_VIEWER)
			glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_TRUE);
		else
			glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_FALSE);

		if (BASIC_SHADER.options & GPU_BASIC_SMOOTH)
			glShadeModel(GL_SMOOTH);
		else
			glShadeModel(GL_FLAT);

		gpu_toggle_clipping(BASIC_SHADER.options & GPU_BASIC_CLIPPING);
	}

	GPU_ASSERT_NO_GL_ERRORS("gpu_basic_bind end");
#endif

	if (BASIC_SHADER.options & GPU_BASIC_LIGHTING) {
		gpu_commit_lighting();
		gpu_commit_material();
	}

	if (BASIC_SHADER.options & GPU_BASIC_CLIPPING) {
		gpu_commit_clipping();
	}

	gpu_commit_matrix();
}



void gpu_basic_unbind(void)
{
	if (GPU_glsl_support())
		GPU_shader_unbind();

#if defined(WITH_GL_PROFILE_COMPAT)
	GPU_ASSERT_NO_GL_ERRORS("gpu_basic_unbind start");

	glDisable(GL_LIGHTING);
	glDisable(GL_TEXTURE_2D);
	glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_FALSE);
	glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_FALSE);
	glShadeModel(GL_FLAT);
	gpu_toggle_clipping(false);

	GPU_ASSERT_NO_GL_ERRORS("gpu_basic_unbind end");
#endif
}



bool GPU_basic_needs_normals(void)
{
	return BASIC_SHADER.options & GPU_BASIC_LIGHTING; // Temporary hack. Should be solved outside of this file.
}
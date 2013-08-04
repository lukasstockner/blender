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

/** \file blender/gpu/intern/gpu_font_shader.c
 *  \ingroup gpu
 */

/* my interface */
#include "GPU_font_shader.h"

/* internal */
#include "intern/gpu_common.h"

/* my library */
#include "GPU_extensions.h"

/* external */
#include "BLI_dynstr.h"
#include "BLI_utildefines.h"



static GPUShader*  FONT_SHADER = NULL;
static GPUcommon   FONT_COMMON = {0};
static bool        FONT_FAILED = FALSE;



void GPU_font_shader_init(void)
{
	FONT_SHADER = NULL;
}



void GPU_font_shader_exit(void)
{
	GPU_shader_free(FONT_SHADER);
}



static void gpu_font_shader(void)
{
	/* GLSL code */
	extern const char datatoc_gpu_shader_font_vert_glsl[];
	extern const char datatoc_gpu_shader_font_frag_glsl[];

	/* Create shader if it doesn't exist yet. */
	if (FONT_SHADER != NULL) {
		GPU_shader_bind(FONT_SHADER);
	}
	else {
		DynStr* vert = BLI_dynstr_new();
		DynStr* frag = BLI_dynstr_new();
		DynStr* defs = BLI_dynstr_new();

		gpu_include_common_vert(vert);
		BLI_dynstr_append(vert, datatoc_gpu_shader_font_vert_glsl);

		gpu_include_common_frag(frag);
		BLI_dynstr_append(frag, datatoc_gpu_shader_font_frag_glsl);

		gpu_include_common_defs(defs);
		BLI_dynstr_append(defs, "#define USE_TEXTURE\n");

		FONT_SHADER =
			GPU_shader_create(
				BLI_dynstr_get_cstring(vert),
				BLI_dynstr_get_cstring(frag),
				NULL,
				BLI_dynstr_get_cstring(defs));

		if (FONT_SHADER != NULL) {
			gpu_init_common(&FONT_COMMON, FONT_SHADER);
			gpu_set_common(&FONT_COMMON);

			GPU_shader_bind(FONT_SHADER);

			/* the mapping between samplers and texture units is static, so it can committed here once */
			glUniform1i(FONT_COMMON.sampler[0], 0);
		}
		else {
			FONT_FAILED = true;
		}
	}
}



void GPU_font_shader_bind(void)
{
	if (GPU_glsl_support())
		gpu_font_shader();

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

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

/** \file source/blender/gpu/intern/gpu_font.c
 *  \ingroup gpu
 */

/* my interface */
#include "intern/gpu_private.h"

/* my library */
#include "GPU_blender_aspect.h"
#include "GPU_extensions.h"
#include "GPU_safety.h"

/* internal */
#include "intern/gpu_common_intern.h"
#include "intern/gpu_matrix_intern.h"

/* external */

#include "BLI_dynstr.h"
#include "BLI_utildefines.h"

#include "MEM_guardedalloc.h"



static struct GPUShader*  FONT_SHADER = NULL;
static struct GPUcommon   FONT_COMMON = {0};
static bool               FONT_FAILED = false;

#if GPU_SAFETY
static bool FONT_BEGUN = false;
#endif



void gpu_font_init(void)
{
	FONT_SHADER = NULL;
}



void gpu_font_exit(void)
{
	GPU_shader_free(FONT_SHADER);

#if GPU_SAFETY
	FONT_BEGUN = false;
#endif
}



static void gpu_font_shader(void)
{
	/* GLSL code */
	extern const char datatoc_gpu_shader_font_vert_glsl[];
	extern const char datatoc_gpu_shader_font_frag_glsl[];

	/* Create shader if it doesn't exist yet. */
	if (FONT_SHADER != NULL) {
		GPU_shader_bind(FONT_SHADER);
		gpu_set_common(&FONT_COMMON);
	}
	else if (!FONT_FAILED) {
		DynStr* vert = BLI_dynstr_new();
		DynStr* frag = BLI_dynstr_new();
		DynStr* defs = BLI_dynstr_new();

		char* vert_cstring;
		char* frag_cstring;
		char* defs_cstring;

		gpu_include_common_vert(vert);
		BLI_dynstr_append(vert, datatoc_gpu_shader_font_vert_glsl);

		gpu_include_common_frag(frag);
		BLI_dynstr_append(frag, datatoc_gpu_shader_font_frag_glsl);

		gpu_include_common_defs(defs);
		BLI_dynstr_append(defs, "#define USE_TEXTURE_2D\n");

		vert_cstring = BLI_dynstr_get_cstring(vert);
		frag_cstring = BLI_dynstr_get_cstring(frag);
		defs_cstring = BLI_dynstr_get_cstring(defs);

		FONT_SHADER =
			GPU_shader_create("Font", vert_cstring, frag_cstring, NULL, defs_cstring);

		MEM_freeN(vert_cstring);
		MEM_freeN(frag_cstring);
		MEM_freeN(defs_cstring);

		BLI_dynstr_free(vert);
		BLI_dynstr_free(frag);
		BLI_dynstr_free(defs);

		if (FONT_SHADER != NULL) {
			gpu_common_get_symbols(&FONT_COMMON, FONT_SHADER);
			gpu_set_common(&FONT_COMMON);

			GPU_shader_bind(FONT_SHADER);
		}
		else {
			FONT_FAILED = true;
			gpu_set_common(NULL);
		}
	}
	else {
		gpu_set_common(NULL);
	}
}



/* Bind / Unbind */



void gpu_font_bind(void)
{
	bool glsl_support = GPU_glsl_support();

	GPU_ASSERT(FONT_BEGUN);

	if (glsl_support)
		gpu_font_shader();

#if defined(WITH_GL_PROFILE_COMPAT)
	if (!glsl_support)
		GPU_CHECK(glEnable(GL_TEXTURE_2D));
#endif

	gpu_commit_matrix();
}



void gpu_font_unbind(void)
{
	bool glsl_support = GPU_glsl_support();

	GPU_ASSERT(FONT_BEGUN);

	if (glsl_support)
		GPU_shader_unbind();

#if defined(WITH_GL_PROFILE_COMPAT)
	if (!glsl_support)
		GPU_CHECK(glDisable(GL_TEXTURE_2D));
#endif
}



void GPU_font_begin(void)
{
#if GPU_SAFETY
	GPU_ASSERT(!FONT_BEGUN);
	FONT_BEGUN = true;
#endif

	GPU_aspect_end(); /* assuming was GPU_ASPECT_BASIC */

	GPU_aspect_begin(GPU_ASPECT_FONT, 0);
}



void GPU_font_end(void)
{
#if GPU_SAFETY
	GPU_ASSERT(FONT_BEGUN);
#endif

	GPU_aspect_end();

#if GPU_SAFETY
	FONT_BEGUN = false;
#endif

	GPU_aspect_begin(GPU_ASPECT_BASIC, 0);
}

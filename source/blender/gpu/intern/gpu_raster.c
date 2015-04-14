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
 * Contributor(s): Jason Wilkins.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/gpu/intern/gpu_raster.c
 *  \ingroup gpu
 */

#define GPU_MANGLE_DEPRECATED 0

#include "GPU_aspect.h"
#include "GPU_extensions.h"
#include "GPU_debug.h"
#include "GPU_raster.h"
#include "GPU_immediate.h"
#include "GPU_matrix.h"

/* internal */
#include "intern/gpu_private.h"

/* external */

#include "BLI_dynstr.h"

#include "DNA_userdef_types.h"

#include "MEM_guardedalloc.h"

/* standard */
#include <string.h>



/* patterns */

const GLubyte GPU_stipple_halftone[128] = {
	0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
	0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
	0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
	0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
	0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
	0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
	0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
	0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
	0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
	0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
	0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
	0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
	0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
	0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
	0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55,
	0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55};


/*  repeat this pattern
 *
 *     X000X000
 *     00000000
 *     00X000X0
 *     00000000 */


const GLubyte GPU_stipple_quarttone[128] = {
	136, 136, 136, 136, 0, 0, 0, 0, 34, 34, 34, 34, 0, 0, 0, 0,
	136, 136, 136, 136, 0, 0, 0, 0, 34, 34, 34, 34, 0, 0, 0, 0,
	136, 136, 136, 136, 0, 0, 0, 0, 34, 34, 34, 34, 0, 0, 0, 0,
	136, 136, 136, 136, 0, 0, 0, 0, 34, 34, 34, 34, 0, 0, 0, 0,
	136, 136, 136, 136, 0, 0, 0, 0, 34, 34, 34, 34, 0, 0, 0, 0,
	136, 136, 136, 136, 0, 0, 0, 0, 34, 34, 34, 34, 0, 0, 0, 0,
	136, 136, 136, 136, 0, 0, 0, 0, 34, 34, 34, 34, 0, 0, 0, 0,
	136, 136, 136, 136, 0, 0, 0, 0, 34, 34, 34, 34, 0, 0, 0, 0
};


const GLubyte GPU_stipple_diag_stripes_pos[128] = {
	0x00, 0xff, 0x00, 0xff, 0x01, 0xfe, 0x01, 0xfe,
	0x03, 0xfc, 0x03, 0xfc, 0x07, 0xf8, 0x07, 0xf8,
	0x0f, 0xf0, 0x0f, 0xf0, 0x1f, 0xe0, 0x1f, 0xe0,
	0x3f, 0xc0, 0x3f, 0xc0, 0x7f, 0x80, 0x7f, 0x80,
	0xff, 0x00, 0xff, 0x00, 0xfe, 0x01, 0xfe, 0x01,
	0xfc, 0x03, 0xfc, 0x03, 0xf8, 0x07, 0xf8, 0x07,
	0xf0, 0x0f, 0xf0, 0x0f, 0xe0, 0x1f, 0xe0, 0x1f,
	0xc0, 0x3f, 0xc0, 0x3f, 0x80, 0x7f, 0x80, 0x7f,
	0x00, 0xff, 0x00, 0xff, 0x01, 0xfe, 0x01, 0xfe,
	0x03, 0xfc, 0x03, 0xfc, 0x07, 0xf8, 0x07, 0xf8,
	0x0f, 0xf0, 0x0f, 0xf0, 0x1f, 0xe0, 0x1f, 0xe0,
	0x3f, 0xc0, 0x3f, 0xc0, 0x7f, 0x80, 0x7f, 0x80,
	0xff, 0x00, 0xff, 0x00, 0xfe, 0x01, 0xfe, 0x01,
	0xfc, 0x03, 0xfc, 0x03, 0xf8, 0x07, 0xf8, 0x07,
	0xf0, 0x0f, 0xf0, 0x0f, 0xe0, 0x1f, 0xe0, 0x1f,
	0xc0, 0x3f, 0xc0, 0x3f, 0x80, 0x7f, 0x80, 0x7f
};


const GLubyte GPU_stipple_diag_stripes_neg[128] = {
	0xff, 0x00, 0xff, 0x00, 0xfe, 0x01, 0xfe, 0x01,
	0xfc, 0x03, 0xfc, 0x03, 0xf8, 0x07, 0xf8, 0x07,
	0xf0, 0x0f, 0xf0, 0x0f, 0xe0, 0x1f, 0xe0, 0x1f,
	0xc0, 0x3f, 0xc0, 0x3f, 0x80, 0x7f, 0x80, 0x7f,
	0x00, 0xff, 0x00, 0xff, 0x01, 0xfe, 0x01, 0xfe,
	0x03, 0xfc, 0x03, 0xfc, 0x07, 0xf8, 0x07, 0xf8,
	0x0f, 0xf0, 0x0f, 0xf0, 0x1f, 0xe0, 0x1f, 0xe0,
	0x3f, 0xc0, 0x3f, 0xc0, 0x7f, 0x80, 0x7f, 0x80,
	0xff, 0x00, 0xff, 0x00, 0xfe, 0x01, 0xfe, 0x01,
	0xfc, 0x03, 0xfc, 0x03, 0xf8, 0x07, 0xf8, 0x07,
	0xf0, 0x0f, 0xf0, 0x0f, 0xe0, 0x1f, 0xe0, 0x1f,
	0xc0, 0x3f, 0xc0, 0x3f, 0x80, 0x7f, 0x80, 0x7f,
	0x00, 0xff, 0x00, 0xff, 0x01, 0xfe, 0x01, 0xfe,
	0x03, 0xfc, 0x03, 0xfc, 0x07, 0xf8, 0x07, 0xf8,
	0x0f, 0xf0, 0x0f, 0xf0, 0x1f, 0xe0, 0x1f, 0xe0,
	0x3f, 0xc0, 0x3f, 0xc0, 0x7f, 0x80, 0x7f, 0x80
};



const GLubyte stipple_checker_8px[128] = {
	255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0,
	255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0,
	0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255,
	0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255,
	255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0,
	255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0,
	0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255,
	0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255
};



/* State */

static struct RASTER {
	uint32_t options;

	struct GPUShader *gpushader[GPU_RASTER_OPTION_COMBINATIONS];
	bool              failed   [GPU_RASTER_OPTION_COMBINATIONS];
	struct GPUcommon  common   [GPU_RASTER_OPTION_COMBINATIONS];

	GLubyte  polygon_stipple[32 * 32];

	GLint    line_stipple_factor;
	GLushort line_stipple_pattern;

	GLfloat  line_width;

	GLenum   polygon_mode;

} RASTER;

static bool RASTER_BEGUN = false;

/* Init / exit */

void gpu_raster_init(void)
{
	memset(&RASTER, 0, sizeof(RASTER));

	gpu_raster_reset_stipple();

	RASTER.line_width = 1;

	RASTER.line_stipple_factor  = 1;
	RASTER.line_stipple_pattern = 0xFFFF;

	RASTER.polygon_mode = GL_FILL;

	GPU_ASSERT_NO_GL_ERRORS("gpu_raster_init");
}



void gpu_raster_exit(void)
{
	int i;

	for (i = 0; i < GPU_RASTER_OPTION_COMBINATIONS; i++)
		if (RASTER.gpushader[i] != NULL)
			GPU_shader_free(RASTER.gpushader[i]);
}



void gpu_raster_reset_stipple(void)
{
	int a, x, y;
	GLubyte mask[32 * 32];

	a = 0;
	for (x = 0; x < 32; x++) {
		for (y = 0; y < 4; y++) {
			if (x & 1)
				mask[a++] = 0x88;
			else
				mask[a++] = 0x22;
		}
	}

	gpuPolygonStipple((GLubyte*)mask);
}



/* Shader feature enable/disable */

void gpu_raster_enable(uint32_t options)
{
	RASTER.options |= options;
}



void gpu_raster_disable(uint32_t options)
{
	RASTER.options &= ~options;
}



static void raster_shader_bind(void)
{
	/* glsl code */
	extern const char datatoc_gpu_shader_raster_frag_glsl[];
	extern const char datatoc_gpu_shader_raster_vert_glsl[];
	extern const char datatoc_gpu_shader_raster_uniforms_glsl[];

	const uint32_t tweaked_options = RASTER.options; /* tweak_options(); */

	/* create shader if it doesn't exist yet */
	if (RASTER.gpushader[tweaked_options] != NULL) {
		GPU_shader_bind(RASTER.gpushader[tweaked_options]);
		gpu_set_common(RASTER.common + tweaked_options);
	}
	else if (!RASTER.failed[tweaked_options]) {
		DynStr *vert = BLI_dynstr_new();
		DynStr *frag = BLI_dynstr_new();
		DynStr *defs = BLI_dynstr_new();

		char *vert_cstring;
		char *frag_cstring;
		char *defs_cstring;

		gpu_include_common_vert(vert);
		BLI_dynstr_append(vert, datatoc_gpu_shader_raster_uniforms_glsl);
		BLI_dynstr_append(vert, datatoc_gpu_shader_raster_vert_glsl);

		gpu_include_common_frag(frag);
		BLI_dynstr_append(vert, datatoc_gpu_shader_raster_uniforms_glsl);
		BLI_dynstr_append(frag, datatoc_gpu_shader_raster_frag_glsl);

		gpu_include_common_defs(defs);

		if (tweaked_options & GPU_RASTER_STIPPLE)
			BLI_dynstr_append(defs, "#define USE_STIPPLE\n");

		if (tweaked_options & GPU_RASTER_AA)
			BLI_dynstr_append(defs, "#define USE_AA\n");

		if (tweaked_options & GPU_RASTER_POLYGON)
			BLI_dynstr_append(defs, "#define USE_POLYGON\n");

		vert_cstring = BLI_dynstr_get_cstring(vert);
		frag_cstring = BLI_dynstr_get_cstring(frag);
		defs_cstring = BLI_dynstr_get_cstring(defs);

		RASTER.gpushader[tweaked_options] =
			GPU_shader_create(vert_cstring, frag_cstring, NULL, NULL, defs_cstring, 0, 0, 0);

		MEM_freeN(vert_cstring);
		MEM_freeN(frag_cstring);
		MEM_freeN(defs_cstring);

		BLI_dynstr_free(vert);
		BLI_dynstr_free(frag);
		BLI_dynstr_free(defs);

		if (RASTER.gpushader[tweaked_options] != NULL) {
			gpu_common_get_symbols(RASTER.common + tweaked_options, RASTER.gpushader[tweaked_options]);
			gpu_set_common (RASTER.common + tweaked_options);

			GPU_shader_bind(RASTER.gpushader[tweaked_options]);
		}
		else {
			RASTER.failed[tweaked_options] = true;
			gpu_set_common(NULL);
		}
	}
	else {
		gpu_set_common(NULL);
	}
}



void gpu_raster_bind(void)
{
	bool glsl_support = GPU_glsl_support();

	BLI_assert(RASTER_BEGUN);

	if (glsl_support)
		raster_shader_bind();

#if defined(WITH_GL_PROFILE_COMPAT)
	if (!glsl_support) {
		GPU_ASSERT_NO_GL_ERRORS("gpu_raster_bind start");

		if (RASTER.options & GPU_RASTER_STIPPLE) {
			glEnable(GL_LINE_STIPPLE);
			glEnable(GL_POLYGON_STIPPLE);
		}
		else {
			glDisable(GL_LINE_STIPPLE);
			glDisable(GL_POLYGON_STIPPLE);
		}

		if (RASTER.options & GPU_RASTER_AA) {
			glEnable(GL_LINE_SMOOTH);
			glEnable(GL_POLYGON_SMOOTH);
		}
		else {
			glDisable(GL_LINE_SMOOTH);
			glDisable(GL_POLYGON_SMOOTH);
		}

		glLineWidth(RASTER.line_width);
		glLineStipple(RASTER.line_stipple_factor, RASTER.line_stipple_pattern);

		glPolygonMode(GL_FRONT_AND_BACK, RASTER.polygon_mode);
		glPolygonStipple(RASTER.polygon_stipple);

		GPU_ASSERT_NO_GL_ERRORS("gpu_raster_bind end");
	}
#endif

	gpu_commit_matrix();
}



void gpu_raster_unbind(void)
{
	bool glsl_support = GPU_glsl_support();

	BLI_assert(RASTER_BEGUN);

	if (glsl_support)
		GPU_shader_unbind();

#if defined(WITH_GL_PROFILE_COMPAT)
	if (!glsl_support) {
		GPU_ASSERT_NO_GL_ERRORS("gpu_raster_unbind start");

		glDisable(GL_LINE_STIPPLE);
		glDisable(GL_POLYGON_STIPPLE);

		glDisable(GL_LINE_SMOOTH);
		glDisable(GL_POLYGON_SMOOTH);

		glLineWidth(1);

		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

		GPU_ASSERT_NO_GL_ERRORS("gpu_raster_unbind end");
	}
#endif
}



void gpuPolygonStipple(const GLubyte* mask)
{
	memcpy(RASTER.polygon_stipple, mask, sizeof(RASTER.polygon_stipple));
}



void gpuLineStipple(GLint factor, GLushort pattern)
{
	RASTER.line_stipple_factor = factor;
	RASTER.line_stipple_pattern = pattern;
}



void gpuLineWidth(GLfloat width)
{
	RASTER.line_width = width;
}



GLfloat gpuGetLineWidth(void)
{
	return RASTER.line_width;
}



void gpuPolygonMode(GLenum mode)
{
	RASTER.polygon_mode = mode;
}



GLenum gpuGetPolygonMode(void)
{
	return RASTER.polygon_mode;
}



void GPU_raster_set_line_style(int factor)
{
	if (factor == 0) {
		GPU_aspect_disable(GPU_ASPECT_RASTER, GPU_RASTER_STIPPLE);
	}
	else {
		GPU_aspect_enable(GPU_ASPECT_RASTER, GPU_RASTER_STIPPLE);

		if (U.pixelsize > 1.0f)
			gpuLineStipple(factor, 0xCCCC);
		else
			gpuLineStipple(factor, 0xAAAA);
	}
}



void GPU_raster_begin()
{
	BLI_assert(!RASTER_BEGUN);
	RASTER_BEGUN = true;

	/* SSS End (Assuming the basic aspect is ending) */
	GPU_aspect_end();

	/* SSS Begin Raster */
	GPU_aspect_begin(GPU_ASPECT_RASTER, NULL);
}



void GPU_raster_end()
{
	BLI_assert(RASTER_BEGUN);

	/* SSS End Raster */
	GPU_aspect_end();

	RASTER_BEGUN = false;

	/* SSS Begin Basic */
	GPU_aspect_begin(GPU_ASPECT_BASIC, NULL);
}




static GLboolean end_begin(void)
{
	GPU_IMMEDIATE->hasOverflowed = true;

	if (!ELEM(
			GPU_IMMEDIATE->mode,
			GL_NOOP,
			GL_LINE_LOOP,
			GL_POLYGON,
			GL_QUAD_STRIP,
			GL_LINE_STRIP,
			GL_TRIANGLE_STRIP))
/* XXX jwilkins: can restart some of these, but need to put in the logic (could be problematic with mapped VBOs?) */
	{
		gpu_end_buffer_gl();

		GPU_IMMEDIATE->offset = 0;
		GPU_IMMEDIATE->count = 1; /* count the vertex that triggered this */

		gpu_begin_buffer_gl();

		return GL_TRUE;
	}
	else {
		return GL_FALSE;
	}
}



static void gpu_copy_vertex_thick_line(void)
{
	size_t i;
	size_t size;
	size_t offset;
	GLubyte *mappedBuffer;

	if (GPU_IMMEDIATE->maxVertexCount == 0)
		return;

	if (GPU_IMMEDIATE->count == GPU_IMMEDIATE->lastPrimVertex) {
		GLboolean restarted;

		restarted = end_begin(); /* draw and clear buffer */

		BLI_assert(restarted);

		if (!restarted)
			return;
	}
	else {
		GPU_IMMEDIATE->count++;
	}

	mappedBuffer = GPU_IMMEDIATE->mappedBuffer;
	offset = GPU_IMMEDIATE->offset;

	/* vertex */

	size = (size_t)(GPU_IMMEDIATE->format.vertexSize) * sizeof(GLfloat);
	memcpy(mappedBuffer + offset, GPU_IMMEDIATE->vertex, size);
	offset += size;

	/* normal */

	if (GPU_IMMEDIATE->format.normalSize != 0) {
		/* normals always have 3 components */
		memcpy(mappedBuffer + offset, GPU_IMMEDIATE->normal, 3 * sizeof(GLfloat));
		offset += 3 * sizeof(GLfloat);
	}

	/* color */

	if (GPU_IMMEDIATE->format.colorSize != 0) {
		/* 4 bytes are always reserved for color, for efficient memory alignment */
		memcpy(mappedBuffer + offset, GPU_IMMEDIATE->color, 4 * sizeof(GLubyte));
		offset += 4 * sizeof(GLubyte);
	}

	/* texture coordinate(s) */

	for (i = 0; i < GPU_IMMEDIATE->format.texCoordCount; i++) {
		size = (size_t)(GPU_IMMEDIATE->format.texCoordSize[i]) * sizeof(GLfloat);
		memcpy(mappedBuffer + offset, GPU_IMMEDIATE->texCoord[i], size);
		offset += size;
	}

	/* float vertex attribute(s) */

	for (i = 0; i < GPU_IMMEDIATE->format.attribCount_f; i++) {
		size = (size_t)(GPU_IMMEDIATE->format.attribSize_f[i]) * sizeof(GLfloat);
		memcpy(mappedBuffer + offset, GPU_IMMEDIATE->attrib_f[i], size);
		offset += size;
	}

	/* unsigned byte vertex attribute(s) */

	for (i = 0; i < GPU_IMMEDIATE->format.attribCount_ub; i++) {
		/* 4 bytes are always reserved for byte attributes, for efficient memory alignment */
		memcpy(mappedBuffer + offset, GPU_IMMEDIATE->attrib_ub[i], 4 * sizeof(GLubyte));
		offset += 4 * sizeof(GLubyte);
	}

	GPU_IMMEDIATE->offset = offset;
}



static void gpu_copy_vertex_wire_polygon(void)
{
	size_t i;
	size_t size;
	size_t offset;
	GLubyte *mappedBuffer;

	if (GPU_IMMEDIATE->maxVertexCount == 0) {
		return;
	}

	if (GPU_IMMEDIATE->count == GPU_IMMEDIATE->lastPrimVertex) {
		GLboolean restarted;

		restarted = end_begin(); /* draw and clear buffer */

		BLI_assert(restarted);

		if (!restarted)
			return;
	}
	else {
		GPU_IMMEDIATE->count++;
	}

	mappedBuffer = GPU_IMMEDIATE->mappedBuffer;
	offset = GPU_IMMEDIATE->offset;

	/* vertex */

	size = (size_t)(GPU_IMMEDIATE->format.vertexSize) * sizeof(GLfloat);
	memcpy(mappedBuffer + offset, GPU_IMMEDIATE->vertex, size);
	offset += size;

	/* normal */

	if (GPU_IMMEDIATE->format.normalSize != 0) {
		/* normals always have 3 components */
		memcpy(mappedBuffer + offset, GPU_IMMEDIATE->normal, 3 * sizeof(GLfloat));
		offset += 3 * sizeof(GLfloat);
	}

	/* color */

	if (GPU_IMMEDIATE->format.colorSize != 0) {
		/* 4 bytes are always reserved for color, for efficient memory alignment */
		memcpy(mappedBuffer + offset, GPU_IMMEDIATE->color, 4 * sizeof(GLubyte));
		offset += 4 * sizeof(GLubyte);
	}

	/* texture coordinate(s) */

	for (i = 0; i < GPU_IMMEDIATE->format.texCoordCount; i++) {
		size = (size_t)(GPU_IMMEDIATE->format.texCoordSize[i]) * sizeof(GLfloat);
		memcpy(mappedBuffer + offset, GPU_IMMEDIATE->texCoord[i], size);
		offset += size;
	}

	/* float vertex attribute(s) */

	for (i = 0; i < GPU_IMMEDIATE->format.attribCount_f; i++) {
		size = (size_t)(GPU_IMMEDIATE->format.attribSize_f[i]) * sizeof(GLfloat);
		memcpy(mappedBuffer + offset, GPU_IMMEDIATE->attrib_f[i], size);
		offset += size;
	}

	/* unsigned byte vertex attribute(s) */

	for (i = 0; i < GPU_IMMEDIATE->format.attribCount_ub; i++) {
		/* 4 bytes are always reserved for byte attributes, for efficient memory alignment */
		memcpy(mappedBuffer + offset, GPU_IMMEDIATE->attrib_ub[i], 4 * sizeof(GLubyte));
		offset += 4 * sizeof(GLubyte);
	}

	GPU_IMMEDIATE->offset = offset;
}

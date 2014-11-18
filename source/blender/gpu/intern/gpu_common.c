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

/** \file blender/gpu/intern/gpu_common.c
 *  \ingroup gpu
 */

#ifdef WITH_GL_PROFILE_COMPAT
#define GPU_MANGLE_DEPRECATED 0 /* Allow use of deprecated OpenGL functions in this file */
#endif

#include "BLI_sys_types.h"
#include "BLI_dynstr.h"
#include "BLI_utildefines.h"
#include "GPU_extensions.h"
#include "GPU_debug.h"
#include "GPU_common.h"
#include "GPU_immediate.h"

#include <stdio.h>
#include <string.h>

#include "intern/gpu_private.h"


extern const char datatoc_gpu_shader_common_constants_glsl[];
extern const char datatoc_gpu_shader_common_uniforms_glsl [];
extern const char datatoc_gpu_shader_common_attribs_glsl  [];



static GPUcommon* current_common = NULL;
static GLint      active_texture_num = 0;



void gpu_include_common_vert(DynStr* vert)
{
	BLI_dynstr_append(vert, datatoc_gpu_shader_common_constants_glsl);
	BLI_dynstr_append(vert, datatoc_gpu_shader_common_uniforms_glsl);
	BLI_dynstr_append(vert, datatoc_gpu_shader_common_attribs_glsl);
}



void gpu_include_common_frag(DynStr* frag)
{
	BLI_dynstr_append(frag, datatoc_gpu_shader_common_constants_glsl);
	BLI_dynstr_append(frag, datatoc_gpu_shader_common_uniforms_glsl);
}



void gpu_include_common_defs(DynStr* defs)
{
	BLI_dynstr_append(defs, "#define GPU_MAX_COMMON_TEXCOORDS   " STRINGIFY(GPU_MAX_COMMON_TEXCOORDS  ) "\n");
	BLI_dynstr_append(defs, "#define GPU_MAX_COMMON_SAMPLERS    " STRINGIFY(GPU_MAX_COMMON_SAMPLERS   ) "\n");
	BLI_dynstr_append(defs, "#define GPU_MAX_COMMON_LIGHTS      " STRINGIFY(GPU_MAX_COMMON_LIGHTS     ) "\n");
	BLI_dynstr_append(defs, "#define GPU_MAX_COMMON_CLIP_PLANES " STRINGIFY(GPU_MAX_COMMON_CLIP_PLANES) "\n");

	if (GPU_PROFILE_COMPAT)
		BLI_dynstr_append(defs, "#define GPU_PROFILE_COMPAT\n");

	if (GPU_PROFILE_CORE)
		BLI_dynstr_append(defs, "#define GPU_PROFILE_CORE\n");

	if (GPU_PROFILE_ES20)
		BLI_dynstr_append(defs, "#define GPU_PROFILE_ES20\n");
}



static void get_struct_uniform(GLint* out, GPUShader* gpushader, char symbol[], size_t len, const char* field)
{
	symbol[len] = '\0';
	strcat(symbol, field);
	*out = GPU_shader_get_uniform(gpushader, symbol);
}



void gpu_common_get_symbols(GPUcommon* common, GPUShader* gpushader)
{
	int i;

	/* Attributes */
	common->vertex = GPU_shader_get_attrib(gpushader, "b_Vertex");
	common->color  = GPU_shader_get_attrib(gpushader, "b_Color");
	common->normal = GPU_shader_get_attrib(gpushader, "b_Normal");

	/* Transformation */
	common->modelview_matrix            = GPU_shader_get_uniform(gpushader, "b_ModelViewMatrix");
	common->modelview_projection_matrix = GPU_shader_get_uniform(gpushader, "b_ModelViewProjectionMatrix");
	common->modelview_matrix_inverse    = GPU_shader_get_uniform(gpushader, "b_ModelViewMatrixInverse");
	common->projection_matrix           = GPU_shader_get_uniform(gpushader, "b_ProjectionMatrix");

	/* Texture Mapping */

	for (i = 0; i < GPU_MAX_COMMON_TEXCOORDS; i++) {
		char symbol[100];

		sprintf(symbol, "b_MultiTexCoord%d", i);
		common->multi_texcoord[i] = GPU_shader_get_attrib(gpushader, symbol);

		sprintf(symbol, "b_TextureMatrix[%d]", i);
		common->texture_matrix[i] = GPU_shader_get_uniform(gpushader, symbol);
	}

	for (i = 0; i < GPU_MAX_COMMON_SAMPLERS; i++) {
		char symbol[100];

		sprintf(symbol, "b_Sampler2D[%d]", i);
		common->sampler[i] = GPU_shader_get_uniform(gpushader, symbol);
	}

	/* Lighting */

	/* Lights */
	for (i = 0; i < GPU_MAX_COMMON_LIGHTS; i++) {
		char symbol[100];
		int  len;

		len = sprintf(symbol, "b_LightSource[%d]", i);

		get_struct_uniform(common->light_position              + i, gpushader, symbol, len, ".position");
		get_struct_uniform(common->light_diffuse               + i, gpushader, symbol, len, ".diffuse");
		get_struct_uniform(common->light_specular              + i, gpushader, symbol, len, ".specular");

		get_struct_uniform(common->light_constant_attenuation  + i, gpushader, symbol, len, ".constantAttenuation");
		get_struct_uniform(common->light_linear_attenuation    + i, gpushader, symbol, len, ".linearAttenuation");
		get_struct_uniform(common->light_quadratic_attenuation + i, gpushader, symbol, len, ".quadraticAttenuation");

		get_struct_uniform(common->light_spot_direction        + i, gpushader, symbol, len, ".spotDirection");
		get_struct_uniform(common->light_spot_cutoff           + i, gpushader, symbol, len, ".spotCutoff");
		get_struct_uniform(common->light_spot_cos_cutoff       + i, gpushader, symbol, len, ".spotCosCutoff");
		get_struct_uniform(common->light_spot_exponent         + i, gpushader, symbol, len, ".spotExponent");
	}

	common->normal_matrix = GPU_shader_get_uniform(gpushader, "b_NormalMatrix");
	common->light_count   = GPU_shader_get_uniform(gpushader, "b_LightCount");

	/* Material */
	common->material_specular  = GPU_shader_get_uniform(gpushader, "b_FrontMaterial.specular");
	common->material_shininess = GPU_shader_get_uniform(gpushader, "b_FrontMaterial.shininess");

	/* Clip Planes */

	for (i = 0; i < GPU_MAX_COMMON_CLIP_PLANES; i++) {
		char symbol[100];

		sprintf(symbol, "b_ClipPlane[%d]", i);
		common->clip_plane[i] = GPU_shader_get_uniform(gpushader, symbol);
	}

	common->clip_plane_count = GPU_shader_get_uniform(gpushader, "b_ClipPlaneCount");
}



void gpu_common_init(void)
{
	current_common = NULL;
}



void gpu_common_exit(void)
{
}



void gpu_set_common(GPUcommon* common)
{
	current_common = common;
}



GPUcommon* gpu_get_common(void)
{
	return current_common;
}



void GPU_common_enable_vertex_array(void)
{
	if (current_common != NULL) {
		if (current_common->vertex != -1)
			GPU_CHECK_ERRORS_AROUND(glEnableVertexAttribArray(current_common->vertex));

		return;
	}

#if defined(WITH_GL_PROFILE_COMPAT)
	GPU_CHECK_ERRORS_AROUND(glEnableClientState(GL_VERTEX_ARRAY));
#endif
}



void GPU_common_enable_normal_array(void)
{
	if (current_common != NULL) {
		if (current_common->normal != -1)
			GPU_CHECK_ERRORS_AROUND(glEnableVertexAttribArray(current_common->normal));

		return;
	}

#if defined(WITH_GL_PROFILE_COMPAT)
	GPU_CHECK_ERRORS_AROUND(glEnableClientState(GL_NORMAL_ARRAY));
#endif
}



void GPU_common_enable_color_array    (void)
{
	if (current_common != NULL) {
		if (current_common->color != -1)
			GPU_CHECK_ERRORS_AROUND(glEnableVertexAttribArray(current_common->color));

		return;
	}

#if defined(WITH_GL_PROFILE_COMPAT)
	GPU_CHECK_ERRORS_AROUND(glEnableClientState(GL_COLOR_ARRAY));
#endif
}



void GPU_common_enable_texcoord_array (void)
{
	BLI_assert(active_texture_num >= 0);
	BLI_assert(active_texture_num < GPU_MAX_COMMON_TEXCOORDS);

	if (current_common != NULL) {
		if (current_common->multi_texcoord[active_texture_num] != -1)
			GPU_CHECK_ERRORS_AROUND(glEnableVertexAttribArray(current_common->multi_texcoord[active_texture_num]));

		return;
	}

#if defined(WITH_GL_PROFILE_COMPAT)
	if (active_texture_num < GPU_max_textures())
		GPU_CHECK_ERRORS_AROUND(glEnableClientState(GL_TEXTURE_COORD_ARRAY));
#endif
}



void GPU_common_disable_vertex_array  (void)
{
	if (current_common != NULL) {
		if (current_common->vertex != -1)
			GPU_CHECK_ERRORS_AROUND(glDisableVertexAttribArray(current_common->vertex));

		return;
	}

#if defined(WITH_GL_PROFILE_COMPAT)
	GPU_CHECK_ERRORS_AROUND(glDisableClientState(GL_VERTEX_ARRAY));
#endif
}



void GPU_common_disable_normal_array  (void)
{
	if (current_common != NULL) {
		if (current_common->normal != -1)
			GPU_CHECK_ERRORS_AROUND(glDisableVertexAttribArray(current_common->normal));

		return;
	}

#if defined(WITH_GL_PROFILE_COMPAT)
	GPU_CHECK_ERRORS_AROUND(glDisableClientState(GL_NORMAL_ARRAY));
#endif
}



void GPU_common_disable_color_array   (void)
{
	if (current_common != NULL) {
		if (current_common->color != -1)
			GPU_CHECK_ERRORS_AROUND(glDisableVertexAttribArray(current_common->color));

		return;
	}

#if defined(WITH_GL_PROFILE_COMPAT)
	GPU_CHECK_ERRORS_AROUND(glDisableClientState(GL_COLOR_ARRAY));
#endif
}



void GPU_common_disable_texcoord_array(void)
{
	BLI_assert(active_texture_num >= 0);
	BLI_assert(active_texture_num < GPU_MAX_COMMON_TEXCOORDS);

	if (current_common != NULL) {
		if (current_common->multi_texcoord[active_texture_num] != -1)
			GPU_CHECK_ERRORS_AROUND(glDisableVertexAttribArray(current_common->multi_texcoord[active_texture_num]));

		return;
	}

#if defined(WITH_GL_PROFILE_COMPAT)
	GPU_CHECK_ERRORS_AROUND(glDisableClientState(GL_TEXTURE_COORD_ARRAY));
#endif
}



void GPU_common_vertex_pointer(GLint size, GLenum type, GLsizei stride, const GLvoid* pointer)
{
	if (current_common != NULL) {
		if (current_common->vertex != -1)
			GPU_CHECK_ERRORS_AROUND(glVertexAttribPointer(current_common->vertex, size, type, GL_FALSE, stride, pointer));

		return;
	}

#if defined(WITH_GL_PROFILE_COMPAT)
	GPU_CHECK_ERRORS_AROUND(glVertexPointer(size, type, stride, pointer));
#endif
}



void GPU_common_normal_pointer(GLenum type, GLsizei stride, GLboolean normalized, const GLvoid* pointer)
{
	if (current_common != NULL) {
		if (current_common->normal != -1)
			GPU_CHECK_ERRORS_AROUND(glVertexAttribPointer(current_common->normal, 3, type, normalized, stride, pointer));

		return;
	}

#if defined(WITH_GL_PROFILE_COMPAT)
	GPU_CHECK_ERRORS_AROUND(glNormalPointer(type, stride, pointer));
#endif
}



void GPU_common_color_pointer(GLint size, GLenum type, GLsizei stride, const GLvoid* pointer)
{
	BLI_assert(type == GL_UNSIGNED_BYTE); // making assumptions about the type being a fixed point type

	if (current_common != NULL) {
		if (current_common->color != -1)
			GPU_CHECK_ERRORS_AROUND(glVertexAttribPointer(current_common->color, size, type, GL_TRUE, stride, pointer));

		return;
	}

#if defined(WITH_GL_PROFILE_COMPAT)
	GPU_CHECK_ERRORS_AROUND(glColorPointer(size, type, stride, pointer));
#endif
}



void GPU_common_texcoord_pointer(GLint size, GLenum type, GLsizei stride, const GLvoid* pointer)
{
	BLI_assert(active_texture_num >= 0);
	BLI_assert(active_texture_num < GPU_MAX_COMMON_TEXCOORDS);

	if (current_common != NULL) {
		if (current_common->multi_texcoord[active_texture_num] != -1)
			GPU_CHECK_ERRORS_AROUND(glVertexAttribPointer(current_common->multi_texcoord[active_texture_num], size, type, GL_FALSE, stride, pointer));

		return;
	}

#if defined(WITH_GL_PROFILE_COMPAT)
	if (active_texture_num < GPU_max_textures())
		GPU_CHECK_ERRORS_AROUND(glTexCoordPointer(size, type, stride, pointer));
#endif
}



void GPU_set_common_active_texture(GLint texture)
{
	GLint texture_num = texture;

	BLI_assert(texture_num >= 0);
	BLI_assert(texture_num < GPU_MAX_COMMON_TEXCOORDS);

	if (texture_num >= 0 && texture_num < GPU_MAX_COMMON_TEXCOORDS)
		active_texture_num = texture_num;

#if defined(WITH_GL_PROFILE_COMPAT)
	if (active_texture_num < GPU_max_textures())
		GPU_CHECK_ERRORS_AROUND(glClientActiveTexture(GL_TEXTURE0+texture));
#endif
}



GLint GPU_get_common_active_texture(void)
{
	return active_texture_num;
}



void GPU_common_normal_3fv(GLfloat n[3])
{
	if (current_common != NULL) {
		if (current_common->normal != -1)
			GPU_CHECK_ERRORS_AROUND(glVertexAttrib3fv(current_common->normal, n));

		return;
	}

#if defined(WITH_GL_PROFILE_COMPAT)
	GPU_CHECK_ERRORS_AROUND(glNormal3fv(GPU_IMMEDIATE->normal)); // deprecated
#endif
}



void GPU_common_color_4ubv(GLubyte c[4])
{
	if (current_common != NULL) {
		if (current_common->color != -1) {
			GPU_CHECK_ERRORS_AROUND(
				glVertexAttrib4f(
					current_common->color,
					((float)(c[0]))/255.0f,
					((float)(c[1]))/255.0f,
					((float)(c[2]))/255.0f,
					((float)(c[3]))/255.0f));
		}

		return;
	}

#if defined(WITH_GL_PROFILE_COMPAT)
	GPU_CHECK_ERRORS_AROUND(glColor4ubv(c)); // deprecated
#endif
}



void GPU_common_color_4fv(GLfloat c[4])
{
	if (current_common != NULL) {
		if (current_common->color != -1)
			GPU_CHECK_ERRORS_AROUND(glVertexAttrib4fv(current_common->color, c));

		return;
	}

#if defined(WITH_GL_PROFILE_COMPAT)
	GPU_CHECK_ERRORS_AROUND(glColor4fv(c)); // deprecated
#endif
}

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

/* my interface */
#include "intern/gpu_common.h"

/* internal */
#include "intern/gpu_extension_wrapper.h"
#include "intern/gpu_profile.h"

/* my library */
#include "GPU_extensions.h"

/* external */
#include "BLI_dynstr.h"
#include "BLI_utildefines.h"

/* standard */
#include <stdio.h>
#include <string.h>



extern const char datatoc_gpu_shader_common_constants_glsl[];
extern const char datatoc_gpu_shader_common_uniforms_glsl [];
extern const char datatoc_gpu_shader_common_attribs_glsl  [];



static GLint active_texture_num = 0;



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
	BLI_dynstr_append(defs, "#define GPU_MAX_COMMON_TEXCOORDS " STRINGIFY(GPU_MAX_COMMON_TEXCOORDS) "\n");
	BLI_dynstr_append(defs, "#define GPU_MAX_COMMON_SAMPLERS  " STRINGIFY(GPU_MAX_COMMON_SAMPLERS ) "\n");
	BLI_dynstr_append(defs, "#define GPU_MAX_COMMON_LIGHTS    " STRINGIFY(GPU_MAX_COMMON_LIGHTS   ) "\n");

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



void gpu_init_common(GPUcommon* common, GPUShader* gpushader)
{
	int i;

	common->vertex = GPU_shader_get_attrib(gpushader, "b_Vertex");
	common->color  = GPU_shader_get_attrib(gpushader, "b_Color");
	common->normal = GPU_shader_get_attrib(gpushader, "b_Normal");

	common->modelview_matrix            = GPU_shader_get_uniform(gpushader, "b_ModelViewMatrix");
	common->modelview_projection_matrix = GPU_shader_get_uniform(gpushader, "b_ModelViewProjectionMatrix");
	common->modelview_matrix_inverse    = GPU_shader_get_uniform(gpushader, "b_ModelViewMatrixInverse");
	common->projection_matrix           = GPU_shader_get_uniform(gpushader, "b_ProjectionMatrix");

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

	common->normal_matrix      = GPU_shader_get_uniform(gpushader, "b_NormalMatrix");

	common->light_count        = GPU_shader_get_uniform(gpushader, "b_LightCount");

	common->material_specular  = GPU_shader_get_uniform(gpushader, "b_FrontMaterial.specular");
	common->material_shininess = GPU_shader_get_uniform(gpushader, "b_FrontMaterial.shininess");
}




static GPUcommon* current_common = NULL;



void gpu_set_common(GPUcommon* common)
{
	current_common = common;
}



GPUcommon* gpu_get_common(void)
{
	return current_common;
}



void gpu_enable_vertex_array   (void)
{
	if (current_common != NULL) {
		if (current_common->vertex != -1)
			gpu_glEnableVertexAttribArray(current_common->vertex);

		return;
	}

#if defined(WITH_GL_PROFILE_COMPAT)
	glEnableClientState(GL_VERTEX_ARRAY);
#endif
}



void gpu_enable_normal_array   (void)
{
	if (current_common != NULL) {
		if (current_common->normal != -1)
			gpu_glEnableVertexAttribArray(current_common->normal);

		return;
	}

#if defined(WITH_GL_PROFILE_COMPAT)
	glEnableClientState(GL_NORMAL_ARRAY);
#endif
}



void gpu_enable_color_array    (void)
{
	if (current_common != NULL) {
		if (current_common->color != -1)
			gpu_glEnableVertexAttribArray(current_common->color);

		return;
	}

#if defined(WITH_GL_PROFILE_COMPAT)
	glEnableClientState(GL_COLOR_ARRAY);
#endif
}



void gpu_enable_texcoord_array (void)
{
	GPU_ASSERT(active_texture_num >= 0);
	GPU_ASSERT(active_texture_num < GPU_MAX_COMMON_TEXCOORDS);

	if (current_common != NULL) {
		if (current_common->multi_texcoord[active_texture_num] != -1)
			gpu_glEnableVertexAttribArray(current_common->multi_texcoord[active_texture_num]);

		return;
	}

#if defined(WITH_GL_PROFILE_COMPAT)
	if (active_texture_num < GPU_max_textures())
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
#endif
}



void gpu_disable_vertex_array  (void)
{
	if (current_common != NULL) {
		if (current_common->vertex != -1)
			gpu_glDisableVertexAttribArray(current_common->vertex);

		return;
	}

#if defined(WITH_GL_PROFILE_COMPAT)
	glDisableClientState(GL_VERTEX_ARRAY);
#endif
}



void gpu_disable_normal_array  (void)
{
	if (current_common != NULL) {
		if (current_common->normal != -1)
			gpu_glDisableVertexAttribArray(current_common->normal);

		return;
	}

#if defined(WITH_GL_PROFILE_COMPAT)
	glDisableClientState(GL_NORMAL_ARRAY);
#endif
}



void gpu_disable_color_array   (void)
{
	if (current_common != NULL) {
		if (current_common->color != -1)
			gpu_glDisableVertexAttribArray(current_common->color);

		return;
	}

#if defined(WITH_GL_PROFILE_COMPAT)
	glDisableClientState(GL_COLOR_ARRAY);
#endif
}



void gpu_disable_texcoord_array(void)
{
	GPU_ASSERT(active_texture_num >= 0);
	GPU_ASSERT(active_texture_num < GPU_MAX_COMMON_TEXCOORDS);

	if (current_common != NULL) {
		if (current_common->multi_texcoord[active_texture_num] != -1)
			gpu_glDisableVertexAttribArray(current_common->multi_texcoord[active_texture_num]);

		return;
	}

#if defined(WITH_GL_PROFILE_COMPAT)
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
#endif
}



void gpu_vertex_pointer(GLint size, GLenum type, GLsizei stride, const GLvoid* pointer)
{
	if (current_common != NULL) {
		if (current_common->vertex != -1)
			gpu_glVertexAttribPointer(current_common->vertex, size, type, GL_FALSE, stride, pointer);

		return;
	}

#if defined(WITH_GL_PROFILE_COMPAT)
	glVertexPointer(size, type, stride, pointer);
#endif
}



void gpu_normal_pointer(GLenum type, GLsizei stride, const GLvoid* pointer)
{
	if (current_common != NULL) {
		if (current_common->normal != -1)
			gpu_glVertexAttribPointer(current_common->normal, 3, type, GL_FALSE, stride, pointer);

		return;
	}

#if defined(WITH_GL_PROFILE_COMPAT)
	glNormalPointer(type, stride, pointer);
#endif
}



void gpu_color_pointer(GLint size, GLenum type, GLsizei stride, const GLvoid* pointer)
{
	GPU_ASSERT(type == GL_UNSIGNED_BYTE); // making assumptions about the type being a fixed point type

	if (current_common != NULL) {
		if (current_common->color != -1)
			gpu_glVertexAttribPointer(current_common->color, size, type, GL_TRUE, stride, pointer);

		return;
	}

#if defined(WITH_GL_PROFILE_COMPAT)
	glColorPointer(size, type, stride, pointer);
#endif
}



void gpu_texcoord_pointer(GLint size, GLenum type, GLsizei stride, const GLvoid* pointer)
{
	GPU_ASSERT(active_texture_num >= 0);
	GPU_ASSERT(active_texture_num < GPU_MAX_COMMON_TEXCOORDS);

	if (current_common != NULL) {
		if (current_common->multi_texcoord[active_texture_num] != -1)
			gpu_glVertexAttribPointer(current_common->multi_texcoord[active_texture_num], size, type, GL_FALSE, stride, pointer);

		return;
	}

#if defined(WITH_GL_PROFILE_COMPAT)
	if (active_texture_num < GPU_max_textures())
		glTexCoordPointer(size, type, stride, pointer);
#endif
}



void gpu_set_common_active_texture(GLint texture)
{
	GLint texture_num = texture;

	GPU_ASSERT(texture_num >= 0);
	GPU_ASSERT(texture_num < GPU_MAX_COMMON_TEXCOORDS);

	if (texture_num >= 0 && texture_num < GPU_MAX_COMMON_TEXCOORDS)
		active_texture_num = texture_num;

#if defined(WITH_GL_PROFILE_COMPAT)
	if (active_texture_num < GPU_max_textures())
		glClientActiveTexture(GL_TEXTURE0+texture);
#endif
}



GLint gpu_get_common_active_texture(void)
{
	return active_texture_num;
}



void gpu_enable_vertex_attrib_array (GLuint index)
{
	if (current_common != NULL)
		gpu_glEnableVertexAttribArray(index);
}



void gpu_disable_vertex_attrib_array(GLuint index)
{
	if (current_common != NULL)
		gpu_glDisableVertexAttribArray(index);
}



void gpu_vertex_attrib_pointer(
	GLuint        index,
	GLint         size,
	GLenum        type,
	GLboolean     normalize,
	GLsizei       stride,
	const GLvoid* pointer)
{
	if (current_common != NULL)
		gpu_glVertexAttribPointer(index, size, type, normalize, stride, pointer);
}



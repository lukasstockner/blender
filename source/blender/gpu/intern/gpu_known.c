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

/** \file blender/gpu/intern/gpu_known.c
 *  \ingroup gpu
 */

#include "intern/gpu_known.h"

#include "GPU_extensions.h"

#include <stdio.h>
#include <string.h>



static void get_struct_uniform(GLint* out, GPUShader* gpushader, char symbol[], size_t len, const char* field)
{
	symbol[len] = '\0';
	strcat(symbol, field);
	*out = GPU_shader_get_uniform(gpushader, symbol);
}


static GPUknownlocs* current_locations = NULL;



void GPU_set_known_locations(GPUknownlocs* locations, GPUShader* gpushader)
{
	int i;

	locations->vertex = GPU_shader_get_attrib(gpushader, "b_Vertex");
	locations->color  = GPU_shader_get_attrib(gpushader, "b_Color");
	locations->normal = GPU_shader_get_attrib(gpushader, "b_Normal");

	locations->modelview_matrix            = GPU_shader_get_uniform(gpushader, "b_ModelViewMatrix");
	locations->modelview_projection_matrix = GPU_shader_get_uniform(gpushader, "b_ModelViewProjectionMatrix");
	locations->modelview_matrix_inverse    = GPU_shader_get_uniform(gpushader, "b_ModelViewMatrixInverse");
	locations->projection_matrix           = GPU_shader_get_uniform(gpushader, "b_ProjectionMatrix");

	for (i = 0; i < GPU_MAX_KNOWN_TEXCOORDS; i++) {
		char symbol[64];

		sprintf(symbol, "b_TexCoord[%d]", i);
		locations->tex_coord[i] = GPU_shader_get_attrib(gpushader, symbol);

		sprintf(symbol, "b_TextureMatrix[%d]", i);
		locations->texture_matrix[i] = GPU_shader_get_uniform(gpushader, symbol);
	}

	for (i = 0; i < GPU_MAX_KNOWN_SAMPLERS; i++) {
		char symbol[64];

		sprintf(symbol, "b_Sampler2D[%d]", i);
		locations->sampler2D[i] = GPU_shader_get_uniform(gpushader, symbol);
	}

	for (i = 0; i < GPU_MAX_KNOWN_LIGHTS; i++) {
		char symbol[64];
		int  len;

		len = sprintf(symbol, "b_LightSource[%d]", i);

		get_struct_uniform(locations->light_position              + i, gpushader, symbol, len, ".position");
		get_struct_uniform(locations->light_diffuse               + i, gpushader, symbol, len, ".diffuse");
		get_struct_uniform(locations->light_specular              + i, gpushader, symbol, len, ".specular");

		get_struct_uniform(locations->light_constant_attenuation  + i, gpushader, symbol, len, ".constantAttenuation");
		get_struct_uniform(locations->light_linear_attenuation    + i, gpushader, symbol, len, ".linearAttenuation");
		get_struct_uniform(locations->light_quadratic_attenuation + i, gpushader, symbol, len, ".quadraticAttenuation");

		get_struct_uniform(locations->light_spot_direction        + i, gpushader, symbol, len, ".spotDirection");
		get_struct_uniform(locations->light_spot_cutoff           + i, gpushader, symbol, len, ".spotCutoff");
		get_struct_uniform(locations->light_spot_exponent         + i, gpushader, symbol, len, ".spotExponent");
	}

	locations->normal_matrix            = GPU_shader_get_uniform(gpushader, "b_NormalMatrix");

	locations->light_count              = GPU_shader_get_uniform(gpushader, "b_LightCount");

	locations->material_diffuse         = GPU_shader_get_uniform(gpushader, "b_FrontMaterial.diffuse");
	locations->material_specular        = GPU_shader_get_uniform(gpushader, "b_FrontMaterial.specular");
	locations->material_shininess       = GPU_shader_get_uniform(gpushader, "b_FrontMaterial.shininess");

	current_locations = locations;
}



GPUknownlocs* GPU_get_known_locations(void)
{
	return current_locations;
}

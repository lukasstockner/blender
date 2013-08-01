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
* Contributor(s): Jason Wilkins.
*
* ***** END GPL LICENSE BLOCK *****
*/

/** \file blender/gpu/intern/gpu_known.h
*  \ingroup gpu
*/

#ifndef GPU_KNOWN_H
#define GPU_KNOWN_H


#include "intern/gpu_glew.h"



#ifdef __cplusplus
extern "C" {
#endif



extern const char datatoc_gpu_shader_known_constants_glsl[];
extern const char datatoc_gpu_shader_known_uniforms_glsl[];
extern const char datatoc_gpu_shader_known_attribs_glsl[];



#define GPU_MAX_KNOWN_TEXCOORDS 1
#define GPU_MAX_KNOWN_SAMPLERS  1
#define GPU_MAX_KNOWN_LIGHTS    8



typedef struct GPUknownlocs {
	GLint vertex;
	GLint color;
	GLint normal;

	GLint modelview_matrix;
	GLint modelview_matrix_inverse;
	GLint modelview_projection_matrix;
	GLint projection_matrix;
	GLint normal_matrix;

	GLint tex_coord      [GPU_MAX_KNOWN_TEXCOORDS];
	GLint texture_matrix [GPU_MAX_KNOWN_TEXCOORDS];

	GLint sampler2D      [GPU_MAX_KNOWN_SAMPLERS];

	GLint light_position             [GPU_MAX_KNOWN_LIGHTS];
	GLint light_diffuse              [GPU_MAX_KNOWN_LIGHTS];
	GLint light_specular             [GPU_MAX_KNOWN_LIGHTS];

	GLint light_constant_attenuation [GPU_MAX_KNOWN_LIGHTS];
	GLint light_linear_attenuation   [GPU_MAX_KNOWN_LIGHTS];
	GLint light_quadratic_attenuation[GPU_MAX_KNOWN_LIGHTS];

	GLint light_spot_direction       [GPU_MAX_KNOWN_LIGHTS];
	GLint light_spot_cutoff          [GPU_MAX_KNOWN_LIGHTS];
	GLint light_spot_exponent        [GPU_MAX_KNOWN_LIGHTS];

	GLint light_count;

	GLint material_diffuse;
	GLint material_specular;
	GLint material_shininess;

} GPUknownlocs;



void GPU_set_known_locations(GPUknownlocs* location, struct GPUShader* gpushader);
GPUknownlocs* GPU_get_known_locations(void);



#ifdef __cplusplus
}
#endif

#endif /* GPU_KNOWN_H */

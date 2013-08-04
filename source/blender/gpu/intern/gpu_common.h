#ifndef GPU_INTERN_COMMON_H
#define GPU_INTERN_COMMON_H

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

/** \file blender/gpu/intern/gpu_common.h
*  \ingroup gpu
*/

/* 

The 'common' module defines a set of
well-known or "common" GLSL variable names.

These common names serve as an interface for
writing shaders that work with Blender's draw routines.

The 'basic', 'font', and 'codegen' shaders are
written using these variable names.

All of Blender's drawing routines generate uniforms and
attributes that are mapped to these names.

Any rarely used or uncommon attributes and uniforms needed for
advanced drawing routines do not belong here!

*/

#include "intern/gpu_glew.h"

#ifdef __cplusplus
extern "C" {
#endif



#define GPU_MAX_COMMON_TEXCOORDS 1
#define GPU_MAX_COMMON_SAMPLERS  1
#define GPU_MAX_COMMON_LIGHTS    8



typedef struct GPUcommon {
	GLint vertex;                                             /* b_Vertex                             */
	GLint color;                                              /* b_Color                              */
	GLint normal;                                             /* b_Normal                             */

	GLint modelview_matrix;                                   /* b_ModelViewMatrix                    */
	GLint modelview_matrix_inverse;                           /* b_ModelViewMatrixInverse             */
	GLint modelview_projection_matrix;                        /* b_ModelViewProjectionMatrix          */
	GLint projection_matrix;                                  /* b_ProjectionMatrix                   */

	GLint multi_texcoord[GPU_MAX_COMMON_TEXCOORDS];           /* b_MultiTexCoord[]                    */
	GLint texture_matrix[GPU_MAX_COMMON_TEXCOORDS];           /* b_TextureMatrix[]                    */

	GLint sampler[GPU_MAX_COMMON_SAMPLERS];                   /* b_Sampler[]                          */

	GLint light_position             [GPU_MAX_COMMON_LIGHTS]; /* b_LightSource[].position             */
	GLint light_diffuse              [GPU_MAX_COMMON_LIGHTS]; /* b_LightSource[].diffuse              */
	GLint light_specular             [GPU_MAX_COMMON_LIGHTS]; /* b_LightSource[].specular             */

	GLint light_constant_attenuation [GPU_MAX_COMMON_LIGHTS]; /* b_LightSource[].constantAttenuation  */
	GLint light_linear_attenuation   [GPU_MAX_COMMON_LIGHTS]; /* b_LightSource[].linearAttenuation    */
	GLint light_quadratic_attenuation[GPU_MAX_COMMON_LIGHTS]; /* b_LightSource[].quadraticAttenuation */

	GLint light_spot_direction       [GPU_MAX_COMMON_LIGHTS]; /* b_LightSource[].spotDirection        */
	GLint light_spot_cutoff          [GPU_MAX_COMMON_LIGHTS]; /* b_LightSource[].spotCutoff           */
	GLint light_spot_exponent        [GPU_MAX_COMMON_LIGHTS]; /* b_LightSource[].spotExponent         */

	GLint normal_matrix;                                      /* b_NormalMatrix                       */

	GLint light_count;                                        /* b_LightCount                         */

	GLint material_diffuse;                                   /* b_FrontMaterial.diffuse              */
	GLint material_specular;                                  /* b_FrontMaterial.specular             */
	GLint material_shininess;                                 /* b_FrontMaterial.shininess            */

} GPUcommon;



/* given a GPUShader, initialize a GPUcommon */
void gpu_init_common(GPUcommon* common, struct GPUShader* gpushader);

/* set/get the global GPUcommon currently in use */
void       gpu_set_common(GPUcommon* common);
GPUcommon* gpu_get_common(void);

/* for appending GLSL code that defines the common interface */
void gpu_include_common_vert(struct DynStr* vert);
void gpu_include_common_frag(struct DynStr* frag);
void gpu_include_common_defs(struct DynStr* defs);


/* for setting up the common vertex attributes */

void gpu_enable_vertex_array  (void);
void gpu_enable_normal_array  (void);
void gpu_enable_color_array   (void);
void gpu_enable_texcoord_array(void);

void gpu_disable_vertex_array  (void);
void gpu_disable_normal_array  (void);
void gpu_disable_color_array   (void);
void gpu_disable_texcoord_array(void);

void gpu_vertex_pointer  (GLint size, GLenum type, GLsizei stride, const GLvoid* pointer);
void gpu_normal_pointer  (            GLenum type, GLsizei stride, const GLvoid* pointer);
void gpu_color_pointer   (GLint size, GLenum type, GLsizei stride, const GLvoid* pointer);
void gpu_texcoord_pointer(GLint size, GLenum type, GLsizei stride, const GLvoid* pointer);

void  gpu_set_common_active_texture(GLint texture);
GLint gpu_get_common_active_texture(void);

void gpu_enable_vertex_attrib_array (GLuint index);
void gpu_disable_vertex_attrib_array(GLuint index);

void gpu_vertex_attrib_pointer(
	GLuint        index,
	GLint         size,
	GLenum        type,
	GLboolean     normalize,
	GLsizei       stride,
	const GLvoid* pointer);



#ifdef __cplusplus
}
#endif

#endif /* GPU_INTERN_COMMON_H */

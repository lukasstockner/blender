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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file gpu_private.h
 *  \ingroup gpu
 */

#ifndef __GPU_PRIVATE_H__
#define __GPU_PRIVATE_H__

#include "GPU_glew.h"
#include "GPU_common.h"


#if defined(WITH_GL_PROFILE_ES20)
#  define GPU_PROFILE_ES20 1
#else
#  define GPU_PROFILE_ES20 0
#endif

#if defined(WITH_GL_PROFILE_CORE)
#  define GPU_PROFILE_CORE 1
#else
#  define GPU_PROFILE_CORE 0
#endif

#if defined(WITH_GL_PROFILE_COMPAT)
#  define GPU_PROFILE_COMPAT 1
#else
#  define GPU_PROFILE_COMPAT 0
#endif

struct GPUShader;
struct DynStr;

/* call this before running any of the functions below */
void gpu_extensions_init(void);
void gpu_extensions_exit(void);

void gpu_aspect_init(void);
void gpu_aspect_exit(void);

void gpu_basic_init(void);
void gpu_basic_exit(void);

void gpu_basic_enable (uint32_t options);
void gpu_basic_disable(uint32_t options);

void gpu_basic_bind(void);
void gpu_basic_unbind(void);

void gpu_blender_aspect_init(void);
void gpu_blender_aspect_exit(void);

void gpu_clipping_init(void);
void gpu_clipping_exit(void);

void gpu_commit_clipping(void);

void gpu_select_init(void);
void gpu_select_exit(void);

bool gpu_default_select_begin (const void *object, void *param);
bool gpu_default_select_end   (const void *object, void *param);
bool gpu_default_select_commit(const void *object);

bool gpu_is_select_mode(void);

void gpu_matrix_init(void);
void gpu_matrix_exit(void);

void gpu_font_init(void);
void gpu_font_exit(void);

void gpu_font_bind(void);
void gpu_font_unbind(void);

void gpu_lighting_init(void);
void gpu_lighting_exit(void);

void gpu_commit_lighting(void);
void gpu_commit_material(void);

bool gpu_lighting_is_fast(void);

void gpu_pixels_init(void);
void gpu_pixels_exit(void);

void gpu_pixels_bind(void);
void gpu_pixels_unbind(void);

#if defined(WITH_GL_PROFILE_COMPAT)
void gpu_toggle_clipping(bool enable);
#endif

void gpu_raster_init(void);
void gpu_raster_exit(void);

void gpu_raster_enable (uint32_t options);
void gpu_raster_disable(uint32_t options);

void gpu_raster_bind(void);
void gpu_raster_unbind(void);

void gpu_raster_reset_stipple(void);

void gpu_sprite_init(void);
void gpu_sprite_exit(void);

void gpu_sprite_enable (uint32_t options);
void gpu_sprite_disable(uint32_t options);

void gpu_sprite_bind(void);
void gpu_sprite_unbind(void);

void gpu_immediate_init(void);
void gpu_immediate_exit(void);

void gpu_state_latch_init(void);
void gpu_state_latch_exit(void);

typedef enum GPUBasicOption {
	GPU_BASIC_LIGHTING       = (1<<0), /* do lighting computations                */
	GPU_BASIC_TWO_SIDE       = (1<<1), /* flip back-facing normals towards viewer */
	GPU_BASIC_TEXTURE_2D     = (1<<2), /* use 2D texture to replace diffuse color */
	GPU_BASIC_LOCAL_VIEWER   = (1<<3), /* use for orthographic projection         */
	GPU_BASIC_SMOOTH         = (1<<4), /* use smooth shading                      */
	GPU_BASIC_ALPHATEST      = (1<<5), /* use alpha test                          */
	GPU_BASIC_CLIPPING       = (1<<6), /* use clipping                            */

	GPU_BASIC_FAST_LIGHTING  = (1<<7), /* use faster lighting (set automatically) */

	GPU_BASIC_OPTIONS_NUM         = 8,
	GPU_BASIC_OPTION_COMBINATIONS = (1<<GPU_BASIC_OPTIONS_NUM)
} GPUBasicOption;

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
	GLint light_spot_cos_cutoff      [GPU_MAX_COMMON_LIGHTS]; /* b_LightSource[].spotCusCutoff        */
	GLint light_spot_exponent        [GPU_MAX_COMMON_LIGHTS]; /* b_LightSource[].spotExponent         */

	GLint normal_matrix;                                      /* b_NormalMatrix                       */

	GLint light_count;                                        /* b_LightCount                         */

	GLint material_specular;                                  /* b_FrontMaterial.specular             */
	GLint material_shininess;                                 /* b_FrontMaterial.shininess            */

	GLint clip_plane[6];                                      /* b_ClipPlane[]                        */
	GLint clip_plane_count;                                   /* b_ClipPlaneCount                     */
} GPUcommon;

void gpu_common_init(void);
void gpu_common_exit(void);

/* given a GPUShader, initialize a GPUcommon */
void gpu_common_get_symbols(GPUcommon *common, struct GPUShader *gpushader);

/* set/get the global GPUcommon currently in use */
void       gpu_set_common(GPUcommon *common);
GPUcommon *gpu_get_common(void);

/* for appending GLSL code that defines the common interface */
void gpu_include_common_vert(struct DynStr *vert);
void gpu_include_common_frag(struct DynStr *frag);
void gpu_include_common_defs(struct DynStr *defs);

bool gpu_aspect_active(void);

/* gpu_debug.c */
#ifdef WITH_GPU_DEBUG

void gpu_debug_init(void);
void gpu_debug_exit(void);

#  define GPU_DEBUG_INIT() gpu_debug_init()
#  define GPU_DEBUG_EXIT() gpu_debug_exit()

#else

#  define GPU_DEBUG_INIT() ((void)0)
#  define GPU_DEBUG_EXIT() ((void)0)

#endif

#endif  /* __GPU_PRIVATE_H__ */

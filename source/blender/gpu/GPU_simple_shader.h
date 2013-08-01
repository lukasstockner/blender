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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Brecht Van Lommel.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file GPU_simple_shader.h
 *  \ingroup gpu
 */

#ifndef __GPU_SIMPLE_SHADER_H__
#define __GPU_SIMPLE_SHADER_H__

#include "intern/gpu_lighting.h"



#ifdef __cplusplus
extern "C" {
#endif



/* Simple Shading */

typedef enum GPUSimpleShaderOption {
	GPU_SHADER_MATERIAL_ONLY       = (1<<0), /* use material diffuse, not the color attrib    */
	GPU_SHADER_LIGHTING            = (1<<1), /* do lighting computations                      */
	GPU_SHADER_TWO_SIDED           = (1<<2), /* flip backfacing normals towards viewer        */
	GPU_SHADER_TEXTURE_2D          = (1<<3), /* use 2D texture to replace diffuse color       */
	GPU_SHADER_LOCAL_VIEWER        = (1<<4), /* use for orthographic projection               */
	GPU_SHADER_FLAT_SHADED         = (1<<5), /* use flat shading                              */

	GPU_SHADER_FAST_LIGHTING       = (1<<6), /* use faster lighting (set automatically) */

	GPU_SHADER_OPTIONS_NUM         = 7,
	GPU_SHADER_OPTION_COMBINATIONS = (1<<GPU_SHADER_OPTIONS_NUM)
} GPUSimpleShaderOption;

void GPU_simple_shaders_init(void);
void GPU_simple_shaders_exit(void);

void GPU_simple_shader_bind(uint32_t options);
void GPU_simple_shader_unbind(void);

void GPU_simple_shader_light(int light_num, GPUsimplelight *light);
void GPU_simple_shader_multiple_lights(int count, GPUsimplelight lights[]);
void GPU_simple_shader_material(const float diffuse[3], float alpha, const float specular[3], int shininess);
bool GPU_simple_shader_needs_normals(void);




void GPU_font_shader_init(void);
void GPU_font_shader_exit(void);
void GPU_font_shader_bind(void);
void GPU_font_shader_unbind(void);



#ifdef __cplusplus
}
#endif

#endif

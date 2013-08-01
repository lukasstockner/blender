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
 * Contributor(s): Jason Wilkins
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file gpu_lighting_inline.h
 *  \ingroup gpu
 */

#ifndef GPU_LIGHTING_INLINE_H
#define GPU_LIGHTING_INLINE_H

#if 0

#include "gpu_lighting.h"


// XXX jwilkins: need rudimentary gpu safety


BLI_INLINE void gpuMaterialfv(GLenum face, GLenum pname, const GLfloat *params)
{
	GPU_CHECK_NO_ERROR();

	GPU_LIGHTING->material_fv(face, pname, params);

	GPU_CHECK_NO_ERROR();
}



BLI_INLINE void gpuMateriali(GLenum face, GLenum pname, GLint param)
{
	GPU_CHECK_NO_ERROR();

	GPU_LIGHTING->material_i(face, pname, param);

	GPU_CHECK_NO_ERROR();
}



BLI_INLINE void gpuGetMaterialfv(GLenum face, GLenum pname, GLfloat *params)
{
	GPU_CHECK_NO_ERROR();

	GPU_LIGHTING->get_material_fv(face, pname, params);

	GPU_CHECK_NO_ERROR();
}



BLI_INLINE void gpuColorMaterial(GLenum face, GLenum mode)
{
	GPU_CHECK_NO_ERROR();

	GPU_LIGHTING->color_material(face, mode);

	GPU_CHECK_NO_ERROR();
}



BLI_INLINE void gpuEnableColorMaterial(void)
{
	GPU_CHECK_NO_ERROR();

	GPU_LIGHTING->enable_color_material();

	GPU_CHECK_NO_ERROR();
}



BLI_INLINE void gpuDisableColorMaterial(void)
{
	GPU_CHECK_NO_ERROR();

	GPU_LIGHTING->disable_color_material();

	GPU_CHECK_NO_ERROR();
}



BLI_INLINE void gpuLightf(GLint light, GLenum pname, GLfloat param)
{
	GPU_CHECK_NO_ERROR();

	GPU_LIGHTING->light_f(light, pname, param);

	GPU_CHECK_NO_ERROR();
}



BLI_INLINE void gpuLightfv(GLint light, GLenum pname, const GLfloat* params)
{
	GPU_CHECK_NO_ERROR();

	GPU_LIGHTING->light_fv(light, pname, params);

	GPU_CHECK_NO_ERROR();
}



BLI_INLINE void gpuEnableLight(GLint light)
{
	GPU_CHECK_NO_ERROR();

	GPU_LIGHTING->enable_light(light);

	GPU_CHECK_NO_ERROR();
}



BLI_INLINE void gpuDisableLight(GLint light)
{
	GPU_CHECK_NO_ERROR();

	GPU_LIGHTING->disable_light(light);

	GPU_CHECK_NO_ERROR();
}



BLI_INLINE GLboolean gpuIsLightEnabled(GLint light)
{
	GPU_CHECK_NO_ERROR();

	return GPU_LIGHTING->is_light_enabled(light);

	GPU_CHECK_NO_ERROR();
}



BLI_INLINE void gpuLightModeli(GLenum pname, GLint param)
{
	GPU_CHECK_NO_ERROR();

	GPU_LIGHTING->light_model_i(pname, param);

	GPU_CHECK_NO_ERROR();
}



BLI_INLINE void gpuLightModelfv(GLenum pname, const GLfloat* params)
{
	GPU_CHECK_NO_ERROR();

	GPU_LIGHTING->light_model_fv(pname, params);

	GPU_CHECK_NO_ERROR();
}



BLI_INLINE void gpuEnableLighting(void)
{
	GPU_CHECK_NO_ERROR();

	GPU_LIGHTING->enable_lighting();

	GPU_CHECK_NO_ERROR();
}



BLI_INLINE void gpuDisableLighting(void)
{
	GPU_CHECK_NO_ERROR();

	GPU_LIGHTING->disable_lighting();

	GPU_CHECK_NO_ERROR();
}



BLI_INLINE GLboolean gpuIsLightingEnabled(void)
{
	GPU_CHECK_NO_ERROR();

	return GPU_LIGHTING->is_lighting_enabled();

	GPU_CHECK_NO_ERROR();
}

#endif

#endif /* GPU_LIGHTING_INLINE_H */

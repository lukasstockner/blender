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

/** \file gpu_lighting.h
 *  \ingroup gpu
 */

#ifndef GPU_LIGHTING_H
#define GPU_LIGHTING_H



#include "BLI_utildefines.h"

#include <GL/glew.h>



#ifdef __cplusplus
extern "C" {
#endif



typedef struct GPUlighting {
	void (*material_fv)(GLenum face, GLenum pname, const GLfloat *params);
	void (*material_i)(GLenum face, GLenum pname, GLint param);
	void (*get_material_fv)(GLenum face, GLenum pname, GLfloat *params);
	void (*color_material)(GLenum face, GLenum mode);
	void (*enable_color_material)(void);
	void (*disable_color_material)(void);
	void (*light_f)(GLint light, GLenum pname, GLfloat param);
	void (*light_fv)(GLint light, GLenum pname, const GLfloat* params);
	void (*enable_light)(GLint light);
	void (*disable_light)(GLint light);
	GLboolean (*is_light_enabled)(GLint light);
	void (*light_model_i)(GLenum pname, GLint param);
	void (*light_model_fv)(GLenum pname, const GLfloat* params);
	void (*enable_lighting)(void);
	void (*disable_lighting)(void);
	GLboolean (*is_lighting_enabled)(void);
} GPUlighting;




extern GPUlighting *restrict GPU_LIGHTING;



void gpuInitializeLighting(void);
void gpuShutdownLighting(void);

BLI_INLINE void gpuColorMaterial(GLenum face, GLenum mode);
BLI_INLINE void gpuEnableColorMaterial(void);
BLI_INLINE void gpuDisableColorMaterial(void);

BLI_INLINE void gpuMaterialfv(GLenum face, GLenum pname, const GLfloat *params);
BLI_INLINE void gpuMateriali(GLenum face, GLenum pname, GLint param);
BLI_INLINE void gpuGetMaterialfv(GLenum face, GLenum pname, GLfloat *params);

BLI_INLINE void gpuLightf(GLint light, GLenum pname, GLfloat param);
BLI_INLINE void gpuLightfv(GLint light, GLenum pname, const GLfloat* params);

BLI_INLINE void gpuEnableLight(GLint light);
BLI_INLINE void gpuDisableLight(GLint light);
BLI_INLINE GLboolean gpuIsLightEnabled(GLint light);

BLI_INLINE void gpuLightModeli(GLenum pname, GLint param);
BLI_INLINE void gpuLightModelfv(GLenum pname, const GLfloat* params);

BLI_INLINE void gpuEnableLighting(void);
BLI_INLINE void gpuDisableLighting(void);
BLI_INLINE GLboolean gpuIsLightingEnabled(void);



#ifdef __cplusplus
}
#endif



#endif /* GPU_LIGHTING_H */

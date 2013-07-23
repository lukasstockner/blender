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
* Contributor(s):
*
* ***** END GPL LICENSE BLOCK *****
*/

/** \file blender/gpu/intern/gpu_object_gles.h
*  \ingroup gpu
*/

#ifndef _GPU_OBJECT_GLES_H_
#define _GPU_OBJECT_GLES_H_



#ifdef __cplusplus 
extern "C" {
#endif



typedef struct GPUGLSL_ES_info {
		int viewmatloc;
		int normalmatloc;
		int projectionmatloc;
		int texturematloc;
	
		int texidloc;

		int vertexloc;
		int normalloc;	
		int colorloc;
		int texturecoordloc;
} GPUGLSL_ES_info;

extern struct GPUGLSL_ES_info *curglslesi;

void gpu_assign_gles_loc(struct GPUGLSL_ES_info * glslesinfo, unsigned int program);

void gpu_set_shader_es(struct GPUGLSL_ES_info * s, int update);

void gpuVertexPointer_gles(int size, int type, int stride, const void *pointer);
void gpuNormalPointer_gles(          int type, int stride, const void *pointer);
void gpuColorPointer_gles (int size, int type, int stride, const void *pointer);
void gpuTexCoordPointer_gles(int size, int type, int stride, const void *pointer);
void gpuClientActiveTexture_gles(int texture);

void gpuCleanupAfterDraw_gles(void);



extern GPUGLSL_ES_info shader_main_info;
extern int shader_main;

extern GPUGLSL_ES_info shader_alphatexture_info;
extern int shader_alphatexture;

extern GPUGLSL_ES_info shader_rgbatexture_info;
extern int shader_rgbatexture;

extern GPUGLSL_ES_info shader_pixels_info;
extern int shader_pixels;



void gpu_object_init_gles(void);



#ifdef __cplusplus 
}
#endif



#endif

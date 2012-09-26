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
* Contributor(s): Alexandr Kuznetsov, Jason Wilkins.
*
* ***** END GPL LICENSE BLOCK *****
*/

/** \file blender/gpu/intern/gpu_immediate_glsl.c
*  \ingroup gpu
*/
#include "gpu_immediate_internal.h"
#include "GPU_matrix.h"
#include "MEM_guardedalloc.h"
#include "BLI_math_vector.h"

#include <string.h>

#include "GPU_functions.h"
#include "gpu_glew.h"
#include "gpu_object_gles.h"
#include "GPU_object.h"
#include REAL_GL_MODE

typedef struct bufferDataGLSL {
	size_t   size;
	GLubyte* ptr;
} bufferDataGLSL;

typedef struct stateDataGLSL {
	GLfloat curcolor[4];
	GLfloat curnormal[3];
} stateDataGLSL;

stateDataGLSL stateData = {{1, 1, 1, 1}, {0, 0, 1}};

static void allocate(void)
{
	size_t newSize;
	bufferDataGLSL* bufferData;

	GPU_IMMEDIATE->stride = gpu_calc_stride();

	newSize = (size_t)(GPU_IMMEDIATE->stride * GPU_IMMEDIATE->maxVertexCount);

	if (GPU_IMMEDIATE->bufferData) {
		bufferData = GPU_IMMEDIATE->bufferData;

		if (newSize > bufferData->size) {
			bufferData->ptr = MEM_reallocN(bufferData->ptr, newSize);
			GPU_ASSERT(bufferData->ptr != NULL);

			bufferData->size = newSize;
		}
	}
	else {
		bufferData = MEM_mallocN(
			sizeof(bufferDataGLSL),
			"bufferDataGLSL");

		GPU_ASSERT(bufferData != NULL);

		GPU_IMMEDIATE->bufferData = bufferData;

		bufferData->ptr = MEM_mallocN(newSize, "bufferDataGLSL->ptr");
		GPU_ASSERT(bufferData->ptr != NULL);

		bufferData->size = newSize;
	}
}



static void setup(void)
{
	size_t i;
	size_t offset;
	bufferDataGLSL* bufferData = GPU_IMMEDIATE->bufferData;

	offset = 0;

	glEnable(GL_BLEND); /* XXX: why? */

	GPU_CHECK_NO_ERROR();

	/* setup vertex arrays
	   Assume that vertex arrays have been disabled for everything
	   and only enable what is needed */
	   if (GPU_IMMEDIATE->format.textureUnitCount == 1)
	   {
	   
	   	   		gpu_set_shader_es(&shader_alphatexture_info, 0);
	   		
	   		gpu_glUseProgram(shader_alphatexture);	

		} else
		{
	   		gpu_set_shader_es(&shader_main_info, 0);
	   		
	   		gpu_glUseProgram(shader_main);	
		
		}
		
		

	/* vertex */

gpugameobj.gpuVertexPointer(GPU_IMMEDIATE->format.vertexSize,
		GL_FLOAT,
		GPU_IMMEDIATE->stride,
		bufferData->ptr + offset);

	offset += (size_t)(GPU_IMMEDIATE->format.vertexSize) * sizeof(GLfloat);


	/* normal */

	if (GPU_IMMEDIATE->format.normalSize != 0) {
	gpugameobj.gpuNormalPointer(
			GL_FLOAT,
			GPU_IMMEDIATE->stride,
			bufferData->ptr + offset);

		offset += 3 * sizeof(GLfloat);

	}

	/* color */

	if (GPU_IMMEDIATE->format.colorSize != 0) {
	gpugameobj.gpuColorPointer(
			4 * sizeof(GLubyte),
			GL_UNSIGNED_BYTE,
			GPU_IMMEDIATE->stride,
			bufferData->ptr + offset);

		/* 4 bytes are always reserved for color, for efficient memory alignment */
		offset += 4 * sizeof(GLubyte);


	}

	/* texture coordinate */

	if (GPU_IMMEDIATE->format.textureUnitCount == 1) {
		gpugameobj.gpuTexCoordPointer(
			GPU_IMMEDIATE->format.texCoordSize[0],
			GL_FLOAT,
			GPU_IMMEDIATE->stride,
			bufferData->ptr + offset);

		offset +=
			(size_t)(GPU_IMMEDIATE->format.texCoordSize[0]) * sizeof(GLfloat);

	}
	else if (GPU_IMMEDIATE->format.textureUnitCount > 1) {
		for (i = 0; i < GPU_IMMEDIATE->format.textureUnitCount; i++) {
			/*glClientActiveTexture(GPU_IMMEDIATE->format.textureUnitMap[i]);

			glTexCoordPointer(
				GPU_IMMEDIATE->format.texCoordSize[i],
				GL_FLOAT,
				GPU_IMMEDIATE->stride,
				bufferData->ptr + offset);*/

			offset +=
				(size_t)(GPU_IMMEDIATE->format.texCoordSize[i]) * sizeof(GLfloat);

		//	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		}

	//	glClientActiveTexture(GL_TEXTURE0);
	}

	/* float vertex attribute */

	for (i = 0; i < GPU_IMMEDIATE->format.attribCount_f; i++) {
	/*	gpu_glVertexAttribPointer(
			GPU_IMMEDIATE->format.attribIndexMap_f[i],
			GPU_IMMEDIATE->format.attribSize_f[i],
			GL_FLOAT,
			GPU_IMMEDIATE->format.attribNormalized_f[i],
			GPU_IMMEDIATE->stride,
			bufferData->ptr + offset);*/

		offset +=
			(size_t)(GPU_IMMEDIATE->format.attribSize_f[i]) * sizeof(GLfloat);

		/*gpu_glEnableVertexAttribArray(
			GPU_IMMEDIATE->format.attribIndexMap_f[i]);*/
	}

	/* byte vertex attribute */

	for (i = 0; i < GPU_IMMEDIATE->format.attribCount_ub; i++) {
		if (GPU_IMMEDIATE->format.attribSize_ub[i] > 0) {
			/*glVertexAttribPointer(
				GPU_IMMEDIATE->format.attribIndexMap_ub[i],
				GPU_IMMEDIATE->format.attribSize_ub[i],
				GL_UNSIGNED_BYTE,
				GPU_IMMEDIATE->format.attribNormalized_ub[i],
				GPU_IMMEDIATE->stride,
				bufferData->ptr + offset);*/

			offset += 4 * sizeof(GLubyte);

			/*glEnableVertexAttribArray(
				GPU_IMMEDIATE->format.attribIndexMap_ub[i]);*/
		}
	}

	GPU_CHECK_NO_ERROR();
}

typedef struct indexbufferDataGLSL {
	void*  ptr;
	size_t size;
} indexbufferDataGLSL;

static void allocateIndex(void)
{
	if (GPU_IMMEDIATE->index) {
		indexbufferDataGLSL* bufferData;
		GPUindex* index;
		size_t newSize;

		index = GPU_IMMEDIATE->index;
		newSize = index->maxIndexCount * sizeof(GLuint);

		if (index->bufferData) {
			bufferData = index->bufferData;

			if (newSize > bufferData->size) {
				bufferData->ptr = MEM_reallocN(bufferData->ptr, newSize);
				GPU_ASSERT(bufferData->ptr != NULL);

				bufferData->size = newSize;
			}
		}
		else {
			bufferData = MEM_mallocN(
				sizeof(indexbufferDataGLSL),
				"indexBufferDataG11");

			GPU_ASSERT(bufferData != NULL);

			index->bufferData = bufferData;

			bufferData->ptr = MEM_mallocN(newSize, "indexBufferData->ptr");
			GPU_ASSERT(bufferData->ptr != NULL);

			bufferData->size = newSize;
		}
	}
}


void gpu_lock_buffer_glsl(void)
{
	allocate();
	allocateIndex();
	setup();
}



void gpu_begin_buffer_glsl(void)
{
	bufferDataGLSL* bufferData = GPU_IMMEDIATE->bufferData;
	GPU_IMMEDIATE->buffer = bufferData->ptr;
}


 short vqeos[65536*3/2];
 char vqeoc[256*3/2];

void gpu_quad_elements_init(void)
{

	int i, j;
	/* init once*/
	static char init = 0;	
	
	if(init) return;
	init = 1;
	
	for(i=0, j=0; i<65536; i++, j++)
	{
		vqeos[j] = i;
		if((i%4)==3)
		{
			vqeos[++j] = i-3;
			vqeos[++j] = i-1;
		}
	}
	
	for(i=0, j=0; i<256; i++, j++)
	{
		vqeoc[j] = i;
		if((i%4)==3)
		{
			vqeoc[++j] = i-3;
			vqeoc[++j] = i-1;
		}
	}
}


void gpu_end_buffer_glsl(void)
{
	if (GPU_IMMEDIATE->count > 0) {
		GPU_CHECK_NO_ERROR();


		if (GPU_IMMEDIATE->format.colorSize == 0) {
			gpugameobj.gpuColorSet(	stateData.curcolor);
		}
		
		
		gpuMatrixCommit();
		
		if(GPU_IMMEDIATE->mode != GL_QUADS )
		{
	
			glDrawArrays(GPU_IMMEDIATE->mode, 0, GPU_IMMEDIATE->count);
			
			}
		else
		{
		
			if(GPU_IMMEDIATE->count <= 256){

				glDrawElements(GL_TRIANGLES,GPU_IMMEDIATE->count *3/2, GL_UNSIGNED_BYTE,  vqeoc);

				}
			else if(GPU_IMMEDIATE->count <= 65536)
			{

				glDrawElements(GL_TRIANGLES,GPU_IMMEDIATE->count *3/2, GL_UNSIGNED_SHORT,  vqeos);

				
				}
			else
			{
				printf("To big GL_QUAD object to draw. Vertices: %i", GPU_IMMEDIATE->count);
			}
			
			
		}

		GPU_CHECK_NO_ERROR();
	}
}



void gpu_unlock_buffer_glsl(void)
{
	size_t i;

	/* Disable any arrays that were used so that everything is off again. */

	GPU_CHECK_NO_ERROR();
gpu_glUseProgram(0);
gpu_set_shader_es(0, 0);
	/* vertex */

	//glDisableClientState(GL_VERTEX_ARRAY);

	/* normal */

	/*if (GPU_IMMEDIATE->format.normalSize != 0) {
		glDisableClientState(GL_NORMAL_ARRAY);
	}*/

	/* color */

	/*if (GPU_IMMEDIATE->format.colorSize != 0) {
		glDisableClientState(GL_COLOR_ARRAY);
	}*/

	/* texture coordinate */

	/*if (GPU_IMMEDIATE->format.textureUnitCount == 1) {
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	}
	else if (GPU_IMMEDIATE->format.textureUnitCount > 1) {
		for (i = 0; i < GPU_IMMEDIATE->format.textureUnitCount; i++) {
			glClientActiveTexture(GPU_IMMEDIATE->format.textureUnitMap[i]);
			glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		}

		//glClientActiveTexture(GL_TEXTURE0);
	}*/

	/* float vertex attribute */

	for (i = 0; i < GPU_IMMEDIATE->format.attribCount_f; i++) {
		//gpu_glDisableVertexAttribArray(GPU_IMMEDIATE->format.attribIndexMap_f[i]);
	}

	/* byte vertex attribute */

	for (i = 0; i < GPU_IMMEDIATE->format.attribCount_ub; i++) {
		//gpu_glDisableVertexAttribArray(GPU_IMMEDIATE->format.attribIndexMap_ub[i]);
	}

	GPU_CHECK_NO_ERROR();
}



void gpu_shutdown_buffer_glsl(GPUimmediate *restrict immediate)
{
	if (immediate->bufferData) {
		bufferDataGLSL* bufferData = immediate->bufferData;

		if (bufferData->ptr) {
			MEM_freeN(bufferData->ptr);
			bufferData->ptr = NULL;
		}

		MEM_freeN(immediate->bufferData);
		immediate->bufferData = NULL;

		gpu_index_shutdown_buffer_glsl(immediate->index);
	}
}

void gpu_index_shutdown_buffer_glsl(GPUindex *restrict index)
{
	if (index && index->bufferData) {
		indexbufferDataGLSL* bufferData = index->bufferData;

		if (bufferData->ptr) {
			MEM_freeN(bufferData->ptr);
			bufferData->ptr = NULL;
		}

		MEM_freeN(index->bufferData);
		index->bufferData = NULL;
	}
}



void gpu_current_color_glsl(void)
{
	stateData.curcolor[0] = (float)GPU_IMMEDIATE->color[0]/255.0f;
	stateData.curcolor[1] = (float)GPU_IMMEDIATE->color[1]/255.0f;
	stateData.curcolor[2] = (float)GPU_IMMEDIATE->color[2]/255.0f;
	stateData.curcolor[3] = (float)GPU_IMMEDIATE->color[3]/255.0f;;//(float)GPU_IMMEDIATE->color[3]/255.0f;
}

void gpu_get_current_color_glsl(GLfloat *color)
{
	
	copy_v4_v4(color, stateData.curcolor);

}



void gpu_current_normal_glsl(void)
{

	copy_v3_v3(stateData.curnormal, GPU_IMMEDIATE->normal);
}



void gpu_index_begin_buffer_glsl(void)
{
	GPUindex *restrict index = GPU_IMMEDIATE->index;
	indexbufferDataGLSL* bufferData = index->bufferData;
	index->buffer = bufferData->ptr;
	index->unmappedBuffer = NULL;
}

void gpu_index_end_buffer_glsl(void)
{
	GPUindex *restrict index = GPU_IMMEDIATE->index;
	indexbufferDataGLSL* bufferData = index->bufferData;
	index->unmappedBuffer = bufferData->ptr;
}

void gpu_draw_elements_glsl(void)
{
	GPUindex* index = GPU_IMMEDIATE->index;

	GPU_CHECK_NO_ERROR();
	
	if (GPU_IMMEDIATE->format.colorSize == 0) {
			gpugameobj.gpuColorSet(	stateData.curcolor);
	}
		
	
	gpuMatrixCommit();
	
	glDrawElements(
		GPU_IMMEDIATE->mode,
		index->count,
		GL_UNSIGNED_INT,
		index->unmappedBuffer);

	GPU_CHECK_NO_ERROR();
}

void gpu_draw_range_elements_glsl(void)
{
	GPUindex* index = GPU_IMMEDIATE->index;

	GPU_CHECK_NO_ERROR();
	//gpuMatrixCommit();
//glDrawArrays(GPU_IMMEDIATE->mode, 0, GPU_IMMEDIATE->count);
	/*glDrawRangeElements(
		GPU_IMMEDIATE->mode,
		index->indexMin,
		index->indexMax,
		index->count,
		GL_UNSIGNED_INT,
		index->unmappedBuffer);*/
		
		

	GPU_CHECK_NO_ERROR();
}

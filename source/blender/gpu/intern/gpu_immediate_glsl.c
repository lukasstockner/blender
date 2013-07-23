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

#include "gpu_extension_wrapper.h"
#include "gpu_glew.h"
#include "gpu_object_gles.h"
#include "GPU_object.h"

typedef struct bufferDataGLSL {
	size_t   size;
	GLubyte* ptr;
} bufferDataGLSL;

typedef struct stateDataGLSL {
	GLfloat stateCurrentColor[4];
	GLfloat stateCurrentNormal[3];
} stateDataGLSL;

stateDataGLSL stateData = {{1, 1, 1, 1}, {0, 0, 1}};

static void allocate(void)
{
	size_t newSize;
	bufferDataGLSL* bufferData;

	GPU_IMMEDIATE->stride = gpu_calc_stride();

	newSize = (size_t)(GPU_IMMEDIATE->stride * GPU_IMMEDIATE->maxVertexCount);

	if (GPU_IMMEDIATE->bufferData) {
		bufferData = (bufferDataGLSL*)GPU_IMMEDIATE->bufferData;

		if (newSize > bufferData->size) {
			bufferData->ptr = (GLubyte*)MEM_reallocN(bufferData->ptr, newSize);
			GPU_ASSERT(bufferData->ptr != NULL);

			bufferData->size = newSize;
		}
	}
	else {
		bufferData = (bufferDataGLSL*)MEM_mallocN(
			sizeof(bufferDataGLSL),
			"bufferDataGLSL");

		GPU_ASSERT(bufferData != NULL);

		GPU_IMMEDIATE->bufferData = bufferData;

		bufferData->ptr = (GLubyte*)MEM_mallocN(newSize, "bufferDataGLSL->ptr");
		GPU_ASSERT(bufferData->ptr != NULL);

		bufferData->size = newSize;
	}
}



static void setup(void)
{
	size_t i;
	size_t offset;
	bufferDataGLSL* bufferData = (bufferDataGLSL*)(GPU_IMMEDIATE->bufferData);

	offset = 0;

	GPU_CHECK_NO_ERROR();

	/* vertex */

	gpugameobj.gpuVertexPointer(
		GPU_IMMEDIATE->format.vertexSize,
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
	bufferDataGLSL* bufferData = (bufferDataGLSL*)(GPU_IMMEDIATE->bufferData);
	GPU_IMMEDIATE->buffer      = (char*)(bufferData->ptr);
}



static short vqeos[3 * 65536 / 2];
static char  vqeoc[3 * 256   / 2];

void gpu_quad_elements_init(void)
{
	int i, j;
	/* init once*/
	static char init = 0;	

	if (init)
		return;

	init = 1;

	for (i = 0, j = 0; i < 65535; i++, j++) {
		vqeos[j] = i;

		if (i%4 == 3) {
			vqeos[++j] = i-3;
			vqeos[++j] = i-1;
		}
	}

	for (i = 0, j = 0; i < 255; i++, j++) {
		vqeoc[j] = i;

		if(i%4 == 3) {
			vqeoc[++j] = i-3;
			vqeoc[++j] = i-1;
		}
	}
}



void gpu_end_buffer_glsl(void)
{
	if (GPU_IMMEDIATE->count > 0) {
		GPU_CHECK_NO_ERROR();

		gpuMatrixCommit();

		if(/*GPU_IMMEDIATE->format.colorSize == 0 &&*/ curglslesi && (curglslesi->colorloc != -1)) {
			glVertexAttrib4fv(curglslesi->colorloc, stateData.stateCurrentColor);
		}

		if (GPU_IMMEDIATE->mode != GL_QUADS) {
			glDrawArrays(GPU_IMMEDIATE->mode, 0, GPU_IMMEDIATE->count);
		}
		else {
			if (GPU_IMMEDIATE->count <= 255){
				glDrawElements(GL_TRIANGLES, 3 * GPU_IMMEDIATE->count / 2, GL_UNSIGNED_BYTE,  vqeoc);
			}
			else if(GPU_IMMEDIATE->count <= 65535) {
				glDrawElements(GL_TRIANGLES, 3 * GPU_IMMEDIATE->count / 2, GL_UNSIGNED_SHORT, vqeos);
			}
			else {
				printf("To big GL_QUAD object to draw. Vertices: %i", GPU_IMMEDIATE->count);
			}
		}

		GPU_CHECK_NO_ERROR();
	}
}



void gpu_unlock_buffer_glsl(void)
{
	GPU_CHECK_NO_ERROR();

	gpugameobj.gpuCleanupAfterDraw();

	GPU_CHECK_NO_ERROR();
}



void gpu_shutdown_buffer_glsl(GPUimmediate *restrict immediate)
{
	if (immediate->bufferData) {
		bufferDataGLSL* bufferData = (bufferDataGLSL*)(immediate->bufferData);

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
		indexbufferDataGLSL* bufferData = (indexbufferDataGLSL*)(index->bufferData);

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
	stateData.stateCurrentColor[0] = (GLfloat)(GPU_IMMEDIATE->color[0]) / 255.0f;
	stateData.stateCurrentColor[1] = (GLfloat)(GPU_IMMEDIATE->color[1]) / 255.0f;
	stateData.stateCurrentColor[2] = (GLfloat)(GPU_IMMEDIATE->color[2]) / 255.0f;
	stateData.stateCurrentColor[3] = (GLfloat)(GPU_IMMEDIATE->color[3]) / 255.0f;

	//if(curglslesi && (curglslesi->colorloc != -1)) {
	//	glVertexAttrib4fv(curglslesi->colorloc, stateData.stateCurrentColor);
	//}
}



void gpu_get_current_color_glsl(GLfloat *color)
{
	copy_v4_v4(color, stateData.stateCurrentColor);
}



void gpu_current_normal_glsl(void)
{
	copy_v3_v3(stateData.stateCurrentNormal, GPU_IMMEDIATE->normal);
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

	gpuMatrixCommit();

	if(/*GPU_IMMEDIATE->format.colorSize == 0 && */ curglslesi && (curglslesi->colorloc != -1)) {
		glVertexAttrib4fv(curglslesi->colorloc, stateData.stateCurrentColor);
	}

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

	if(/*GPU_IMMEDIATE->format.colorSize == 0 && */ curglslesi && (curglslesi->colorloc != -1)) {
		glVertexAttrib4fv(curglslesi->colorloc, stateData.stateCurrentColor);
	}

	gpuMatrixCommit();

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

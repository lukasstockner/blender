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

/** \file blender/gpu/intern/gpu_immediate_gl11.c
*  \ingroup gpu
*/

#include "gpu_immediate_internal.h"

#include "MEM_guardedalloc.h"

#include <string.h>



typedef struct bufferDataGL11 {
	size_t   size;
	GLubyte* ptr;
} bufferDataGL11;



static void allocate(void)
{
	size_t newSize;
	bufferDataGL11* bufferData;

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
			sizeof(bufferDataGL11),
			"bufferDataGL11");

		GPU_ASSERT(bufferData != NULL);

		GPU_IMMEDIATE->bufferData = bufferData;

		bufferData->ptr = MEM_mallocN(newSize, "bufferDataGL11->ptr");
		GPU_ASSERT(bufferData->ptr != NULL);

		bufferData->size = newSize;
	}
}



static void setup(void)
{
	size_t i;
	size_t offset;
	bufferDataGL11* bufferData = GPU_IMMEDIATE->bufferData;

	offset = 0;

	GPU_CHECK_NO_ERROR();

	/* setup vertex arrays
	   Assume that vertex arrays have been disabled for everything
	   and only enable what is needed */

	/* vertex */

	glVertexPointer(
		GPU_IMMEDIATE->format.vertexSize,
		GL_FLOAT,
		GPU_IMMEDIATE->stride,
		bufferData->ptr + offset);

	offset += (size_t)(GPU_IMMEDIATE->format.vertexSize) * sizeof(GLfloat);

	glEnableClientState(GL_VERTEX_ARRAY);

	/* normal */

	if (GPU_IMMEDIATE->format.normalSize != 0) {
		glNormalPointer(
			GL_FLOAT,
			GPU_IMMEDIATE->stride,
			bufferData->ptr + offset);

		offset += 3 * sizeof(GLfloat);

		glEnableClientState(GL_NORMAL_ARRAY);
	}

	/* color */

	if (GPU_IMMEDIATE->format.colorSize != 0) {
		glColorPointer(
			4, //-V112
			GL_UNSIGNED_BYTE,
			GPU_IMMEDIATE->stride,
			bufferData->ptr + offset);

		/* 4 bytes are always reserved for color, for efficient memory alignment */
		offset += 4; //-V112

		glEnableClientState(GL_COLOR_ARRAY);
	}

	/* texture coordinate */

	if (GPU_IMMEDIATE->format.textureUnitCount == 1) {
		glTexCoordPointer(
			GPU_IMMEDIATE->format.texCoordSize[0],
			GL_FLOAT,
			GPU_IMMEDIATE->stride,
			bufferData->ptr + offset);

		offset +=
			(size_t)(GPU_IMMEDIATE->format.texCoordSize[0]) * sizeof(GLfloat);

		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	}
	else if (GPU_IMMEDIATE->format.textureUnitCount > 1) {
		for (i = 0; i < GPU_IMMEDIATE->format.textureUnitCount; i++) {
			glClientActiveTexture(GPU_IMMEDIATE->format.textureUnitMap[i]);

			glTexCoordPointer(
				GPU_IMMEDIATE->format.texCoordSize[i],
				GL_FLOAT,
				GPU_IMMEDIATE->stride,
				bufferData->ptr + offset);

			offset +=
				(size_t)(GPU_IMMEDIATE->format.texCoordSize[i]) * sizeof(GLfloat);

			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		}

		glClientActiveTexture(GL_TEXTURE0);
	}

	/* float vertex attribute */

	for (i = 0; i < GPU_IMMEDIATE->format.attribCount_f; i++) {
		glVertexAttribPointer(
			GPU_IMMEDIATE->format.attribIndexMap_f[i],
			GPU_IMMEDIATE->format.attribSize_f[i],
			GL_FLOAT,
			GPU_IMMEDIATE->format.attribNormalized_f[i],
			GPU_IMMEDIATE->stride,
			bufferData->ptr + offset);

		offset +=
			(size_t)(GPU_IMMEDIATE->format.attribSize_f[i]) * sizeof(GLfloat);

		glEnableVertexAttribArray(
			GPU_IMMEDIATE->format.attribIndexMap_f[i]);
	}

	/* byte vertex attribute */

	for (i = 0; i < GPU_IMMEDIATE->format.attribCount_ub; i++) {
		if (GPU_IMMEDIATE->format.attribSize_ub[i] > 0) {
			glVertexAttribPointer(
				GPU_IMMEDIATE->format.attribIndexMap_ub[i],
				GPU_IMMEDIATE->format.attribSize_ub[i],
				GL_FLOAT,
				GPU_IMMEDIATE->format.attribNormalized_ub[i],
				GPU_IMMEDIATE->stride,
				bufferData->ptr + offset);

			offset +=
				(size_t)(GPU_IMMEDIATE->format.attribSize_ub[i]) * sizeof(GLfloat);

			glEnableVertexAttribArray(
				GPU_IMMEDIATE->format.attribIndexMap_ub[i]);
		}
	}

	GPU_CHECK_NO_ERROR();
}

typedef struct indexBufferDataGL11 {
	void*  ptr;
	size_t size;
} indexBufferDataGL11;

static void allocateIndex(void)
{
	if (GPU_IMMEDIATE->index) {
		indexBufferDataGL11* bufferData;
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
				sizeof(indexBufferDataGL11),
				"indexBufferDataG11");

			GPU_ASSERT(bufferData != NULL);

			index->bufferData = bufferData;

			bufferData->ptr = MEM_mallocN(newSize, "indexBufferData->ptr");
			GPU_ASSERT(bufferData->ptr != NULL);

			bufferData->size = newSize;
		}
	}
}


void gpu_lock_buffer_gl11(void)
{
	allocate();
	allocateIndex();
	setup();
}



void gpu_begin_buffer_gl11(void)
{
	bufferDataGL11* bufferData = GPU_IMMEDIATE->bufferData;
	GPU_IMMEDIATE->buffer = bufferData->ptr;
}



void gpu_end_buffer_gl11(void)
{
	if (GPU_IMMEDIATE->count > 0) {
		GPU_CHECK_NO_ERROR();

		glDrawArrays(GPU_IMMEDIATE->mode, 0, GPU_IMMEDIATE->count);

		GPU_CHECK_NO_ERROR();
	}
}



void gpu_unlock_buffer_gl11(void)
{
	size_t i;

	/* Disable any arrays that were used so that everything is off again. */

	GPU_CHECK_NO_ERROR();

	/* vertex */

	glDisableClientState(GL_VERTEX_ARRAY);

	/* normal */

	if (GPU_IMMEDIATE->format.normalSize != 0) {
		glDisableClientState(GL_NORMAL_ARRAY);
	}

	/* color */

	if (GPU_IMMEDIATE->format.colorSize != 0) {
		glDisableClientState(GL_COLOR_ARRAY);
	}

	/* texture coordinate */

	if (GPU_IMMEDIATE->format.textureUnitCount == 1) {
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	}
	else if (GPU_IMMEDIATE->format.textureUnitCount > 1) {
		for (i = 0; i < GPU_IMMEDIATE->format.textureUnitCount; i++) {
			glClientActiveTexture(GPU_IMMEDIATE->format.textureUnitMap[i]);
			glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		}

		glClientActiveTexture(GL_TEXTURE0);
	}

	/* float vertex attribute */

	for (i = 0; i < GPU_IMMEDIATE->format.attribCount_f; i++) {
		glDisableVertexAttribArray(GPU_IMMEDIATE->format.attribIndexMap_f[i]);
	}

	/* byte vertex attribute */

	for (i = 0; i < GPU_IMMEDIATE->format.attribCount_ub; i++) {
		glDisableVertexAttribArray(GPU_IMMEDIATE->format.attribIndexMap_ub[i]);
	}

	GPU_CHECK_NO_ERROR();
}



void gpu_shutdown_buffer_gl11(GPUimmediate *restrict immediate)
{
	if (immediate->bufferData) {
		bufferDataGL11* bufferData = immediate->bufferData;

		if (bufferData->ptr) {
			MEM_freeN(bufferData->ptr);
			bufferData->ptr = NULL;
		}

		MEM_freeN(immediate->bufferData);
		immediate->bufferData = NULL;

		gpu_index_shutdown_buffer_gl11(immediate->index);
	}
}

void gpu_index_shutdown_buffer_gl11(GPUindex *restrict index)
{
	if (index && index->bufferData) {
		indexBufferDataGL11* bufferData = index->bufferData;

		if (bufferData->ptr) {
			MEM_freeN(bufferData->ptr);
			bufferData->ptr = NULL;
		}

		MEM_freeN(index->bufferData);
		index->bufferData = NULL;
	}
}



void gpu_current_color_gl11(void)
{
	GPU_CHECK_NO_ERROR();

	glColor4ubv(GPU_IMMEDIATE->color);

	GPU_CHECK_NO_ERROR();
}

void gpu_get_current_color_gl11(GLfloat *color)
{
	GPU_CHECK_NO_ERROR();

	glGetFloatv(GL_CURRENT_COLOR, color);

	GPU_CHECK_NO_ERROR();
}



void gpu_current_normal_gl11(void)
{
	GPU_CHECK_NO_ERROR();

	glNormal3fv(GPU_IMMEDIATE->normal);

	GPU_CHECK_NO_ERROR();
}



void gpu_index_begin_buffer_gl11(void)
{
	GPUindex *restrict index = GPU_IMMEDIATE->index;
	indexBufferDataGL11* bufferData = index->bufferData;
	index->buffer = bufferData->ptr;
	index->unmappedBuffer = NULL;
}

void gpu_index_end_buffer_gl11(void)
{
	GPUindex *restrict index = GPU_IMMEDIATE->index;
	indexBufferDataGL11* bufferData = index->bufferData;
	index->unmappedBuffer = bufferData->ptr;
}

void gpu_draw_elements_gl11(void)
{
	GPUindex* index = GPU_IMMEDIATE->index;

	GPU_CHECK_NO_ERROR();

	glDrawElements(
		GPU_IMMEDIATE->mode,
		index->count,
		GL_UNSIGNED_INT,
		index->unmappedBuffer);

	GPU_CHECK_NO_ERROR();
}

void gpu_draw_range_elements_gl11(void)
{
	GPUindex* index = GPU_IMMEDIATE->index;

	GPU_CHECK_NO_ERROR();

	glDrawRangeElements(
		GPU_IMMEDIATE->mode,
		index->indexMin,
		index->indexMax,
		index->count,
		GL_UNSIGNED_INT,
		index->unmappedBuffer);

	GPU_CHECK_NO_ERROR();
}

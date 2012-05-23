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
* Contributor(s): Jason Wilkins.
*
* ***** END GPL LICENSE BLOCK *****
*/

/** \file blender/gpu/intern/gpu_immediate.c
*  \ingroup gpu
*/

#include "GPU_immediate.h"

#include "MEM_guardedalloc.h"



/* global symbol needed because the immediate drawing functons are inline */
GPUimmediate* GPU_IMMEDIATE;



static GLsizei calcStride()
{
	size_t i;
	size_t stride = 0;

	/* vertex */

	if (GPU_IMMEDIATE->vertexSize > 0) {
		stride += (size_t)(GPU_IMMEDIATE->vertexSize) * sizeof(GLfloat);
	}

	/* normal */

	if (GPU_IMMEDIATE->normalSize == 3) {
		stride += 3 * sizeof(GLfloat);
	}

	/* color */

	if (GPU_IMMEDIATE->colorSize > 0) {
		/* 4 bytes are always reserved for color, for efficient memory alignment */
		stride += 4 * sizeof(GLubyte);
	}

	/* texture coordinate */

	for (i = 0; i < GPU_IMMEDIATE->textureUnitCount; i++) {
		if (GPU_IMMEDIATE->texCoordSize[i] > 0) {
			stride +=
				(size_t)(GPU_IMMEDIATE->texCoordSize[i]) * sizeof(GLfloat);
		}
	}

	/* float vertex attribute */

	for (i = 0; i < GPU_IMMEDIATE->attribCount_f; i++) {
		if (GPU_IMMEDIATE->attribSize_f[i] > 0) {
			stride +=
				(size_t)(GPU_IMMEDIATE->attribSize_f[i]) * sizeof(GLfloat);
		}
	}

	/* byte vertex attribute */

	for (i = 0; i < GPU_IMMEDIATE->attribCount_ub; i++) {
		if (GPU_IMMEDIATE->attribSize_ub[i] > 0) {
			stride +=
				(size_t)(GPU_IMMEDIATE->attribSize_ub[i]) * sizeof(GLfloat);
		}
	}

	return (GLsizei)stride;
}



typedef struct bufferDataVBO {
	GLint bufferObject;
} bufferDataVBO;

void beginBufferVBO(void)
{
}

void endBufferVBO(void)
{
}

void shutdownBufferVBO(GPUimmediate *restrict immediate)
{
}



typedef struct bufferDataGL11 {
	size_t   size;
	GLubyte* ptr;
	GLsizei stride;
} bufferDataGL11;

void beginBufferGL11(void)
{
	const GLsizei stride = calcStride();
	bufferDataGL11* bufferData;
	size_t newSize;
	size_t offset;
	GLint savedActiveTexture;
	size_t i;

	newSize = (size_t)(stride * GPU_IMMEDIATE->maxVertexCount);

	if (GPU_IMMEDIATE->bufferData) {
		bufferData = (bufferDataGL11*)(GPU_IMMEDIATE->bufferData);

		if (newSize > bufferData->size) {
			MEM_reallocN(bufferData->ptr, newSize);
			bufferData->size = newSize;
		}
	}
	else {
		bufferData =
			(bufferDataGL11*)MEM_mallocN(
				sizeof(bufferDataGL11),
				"bufferDataGL11");

		assert(bufferData);

		bufferData->ptr = MEM_mallocN(newSize, "bufferDataGL11->ptr");
		assert(bufferData->ptr);

		bufferData->size = newSize;
	}

	bufferData->stride = stride;

	/* Assume that vertex arrays have been disabled for everything
	   and only enable what is needed */

	offset = 0;

	/* vertex */

	glVertexPointer(
		GPU_IMMEDIATE->vertexSize,
		GL_FLOAT,
		stride,
		bufferData->ptr + offset);

	offset += (size_t)(GPU_IMMEDIATE->vertexSize) * sizeof(GLfloat);

	glEnableClientState(GL_VERTEX_ARRAY);

	/* normal */

	if (GPU_IMMEDIATE->normalSize == 3) {
		glNormalPointer(
			GL_FLOAT,
			stride,
			bufferData->ptr + offset);

		offset += 3 * sizeof(GLfloat);

		glEnableClientState(GL_NORMAL_ARRAY);
	}

	/* color */

	if (GPU_IMMEDIATE->colorSize > 0) {
		glColorPointer(
			GPU_IMMEDIATE->colorSize,
			GL_UNSIGNED_BYTE,
			stride,
			bufferData->ptr + offset);

		/* 4 bytes are always reserved for color, for efficient memory alignment */
		offset += 4 * sizeof(GLubyte);

		glEnableClientState(GL_COLOR_ARRAY);
	}

	/* texture coordinate */

	glGetIntegerv(GL_ACTIVE_TEXTURE, &savedActiveTexture);

	for (i = 0; i < GPU_IMMEDIATE->textureUnitCount; i++) {
		if (GPU_IMMEDIATE->texCoordSize[i] > 0) {
			glActiveTexture(GPU_IMMEDIATE->textureUnitMap[i]);

			glTexCoordPointer(
				GPU_IMMEDIATE->texCoordSize[i],
				GL_FLOAT,
				stride,
				bufferData->ptr + offset);

			offset +=
				(size_t)(GPU_IMMEDIATE->texCoordSize[i]) * sizeof(GLfloat);

			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		}
	}

	glActiveTexture(savedActiveTexture);

	/* float vertex attribute */

	for (i = 0; i < GPU_IMMEDIATE->attribCount_f; i++) {
		if (GPU_IMMEDIATE->attribSize_f[i] > 0) {
			glVertexAttribPointer(
				GPU_IMMEDIATE->attribIndexMap_f[i],
				GPU_IMMEDIATE->attribSize_f[i],
				GL_FLOAT,
				GPU_IMMEDIATE->attribNormalized_f[i],
				stride,
				bufferData->ptr + offset);

			offset +=
				(size_t)(GPU_IMMEDIATE->attribSize_f[i]) * sizeof(GLfloat);

			glEnableVertexAttribArray(
				GPU_IMMEDIATE->attribIndexMap_f[i]);
		}
	}

	/* byte vertex attribute */

	for (i = 0; i < GPU_IMMEDIATE->attribCount_ub; i++) {
		if (GPU_IMMEDIATE->attribSize_ub[i] > 0) {
			glVertexAttribPointer(
				GPU_IMMEDIATE->attribIndexMap_ub[i],
				GPU_IMMEDIATE->attribSize_ub[i],
				GL_FLOAT,
				GPU_IMMEDIATE->attribNormalized_ub[i],
				stride,
				bufferData->ptr + offset);

			offset +=
				(size_t)(GPU_IMMEDIATE->attribSize_ub[i]) * sizeof(GLfloat);

			glEnableVertexAttribArray(
				GPU_IMMEDIATE->attribIndexMap_ub[i]);
		}
	}

	GPU_IMMEDIATE->buffer     = bufferData->ptr;
	GPU_IMMEDIATE->bufferData = bufferData;
}

void endBufferGL11(void)
{
	GLint savedActiveTexture;
	size_t i;

	glDrawArrays(GPU_IMMEDIATE->mode, 0, GPU_IMMEDIATE->count);

	/* Disable any arrays that were used so that everything is off again. */

	/* vertex */

	glDisableClientState(GL_VERTEX_ARRAY);

	/* normal */

	if (GPU_IMMEDIATE->normalSize == 3) {
		glDisableClientState(GL_NORMAL_ARRAY);
	}

	/* color */

	if (GPU_IMMEDIATE->colorSize > 0) {
		glDisableClientState(GL_COLOR_ARRAY);
	}

	/* texture coordinate */

	glGetIntegerv(GL_ACTIVE_TEXTURE, &savedActiveTexture);

	for (i = 0; i < GPU_IMMEDIATE->textureUnitCount; i++) {
		if (GPU_IMMEDIATE->texCoordSize[i] > 0) {
			glActiveTexture(GPU_IMMEDIATE->textureUnitMap[i]);
			glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		}
	}

	glActiveTexture(savedActiveTexture);

	/* float vertex attribute */

	for (i = 0; i < GPU_IMMEDIATE->attribCount_f; i++) {
		if (GPU_IMMEDIATE->attribSize_f[i] > 0) {
			glDisableVertexAttribArray(
				GPU_IMMEDIATE->attribIndexMap_f[i]);
		}
	}

	/* byte vertex attribute */

	for (i = 0; i < GPU_IMMEDIATE->attribCount_ub; i++) {
		if (GPU_IMMEDIATE->attribSize_ub[i] > 0) {
			glDisableVertexAttribArray(
				GPU_IMMEDIATE->attribIndexMap_ub[i]);
		}
	}
}

void shutdownBufferGL11(GPUimmediate *restrict immediate)
{
	if (immediate->bufferData) {
		bufferDataGL11* bufferData =
			(bufferDataGL11*)(immediate->bufferData);

		if (bufferData->ptr) {
			MEM_freeN(bufferData->ptr);
			bufferData->ptr = NULL;
		}

		MEM_freeN(immediate->bufferData);
		immediate->bufferData = NULL;
	}
}


static calcLastTexture(GPUimmediate* immediate)
{
	GLint maxTextureCoords;
	GLint maxCombinedTextureImageUnits;

	glGetIntegerv(
		GL_MAX_TEXTURE_COORDS,
		&maxTextureCoords);

	glGetIntegerv(
		GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS,
		&maxCombinedTextureImageUnits);

	immediate->lastTexture =
		GL_TEXTURE0 + MAX2(maxTextureCoords, maxCombinedTextureImageUnits) - 1;
}

GPUimmediate* gpuNewImmediate(void)
{
	GPUimmediate* immediate =
		MEM_callocN(sizeof(GPUimmediate), "GPUimmediate");

	assert(immediate);

	immediate->vertexSize = 3;

	//if (GLEW_ARB_vertex_buffer_object) {
	//	immediate->beginBuffer    = beginBufferVBO;
	//	immediate->endBuffer      = endBufferVBO;
	//	immediate->shutdownBuffer = shutdownBufferVBO;
	//}
	//else {
		immediate->beginBuffer    = beginBufferGL11;
		immediate->endBuffer      = endBufferGL11;
		immediate->shutdownBuffer = shutdownBufferGL11;
	//}

	calcLastTexture(immediate);

	return immediate;
}

void gpuImmediateMakeCurrent(GPUimmediate* immediate)
{
	GPU_IMMEDIATE = immediate;
}

void gpuDeleteImmediate(GPUimmediate* immediate)
{
	if (!immediate) {
		return;
	}

	assert(!(immediate->buffer));

	if (immediate->buffer) {
		SWAP(GPUimmediate*, immediate, GPU_IMMEDIATE);
		gpuEnd();
		SWAP(GPUimmediate*, immediate, GPU_IMMEDIATE);
	}

	if (GPU_IMMEDIATE == immediate) {
		gpuImmediateMakeCurrent(NULL);
	}

	immediate->shutdownBuffer(immediate);

	MEM_freeN(immediate);
}



#ifdef GPU_LEGACY_DEBUG

/* For legacy source compatibility.
   Copies the current OpenGL state into the GPU_IMMEDIATE */
void gpuImmediateLegacyGetState(void)
{
	GLint savedActiveTexture;
	size_t i;
	GLfloat color[4];

	GPU_CHECK_NO_BEGIN();

	glGetFloatv(GL_CURRENT_COLOR, color);
	GPU_IMMEDIATE->color[0] = (GLubyte)(255.0f * color[0]);
	GPU_IMMEDIATE->color[1] = (GLubyte)(255.0f * color[1]);
	GPU_IMMEDIATE->color[2] = (GLubyte)(255.0f * color[2]);
	GPU_IMMEDIATE->color[3] = (GLubyte)(255.0f * color[3]);

	glGetFloatv(GL_CURRENT_NORMAL, GPU_IMMEDIATE->normal);

	glGetIntegerv(GL_ACTIVE_TEXTURE, &savedActiveTexture);

	for (i = 0; i < GPU_IMMEDIATE->textureUnitCount; i++) {
		glActiveTexture(GPU_IMMEDIATE->textureUnitMap[i]);
		glGetFloatv(
			GL_CURRENT_TEXTURE_COORDS,
			GPU_IMMEDIATE->texCoord[i]);
	}

	glActiveTexture(savedActiveTexture);

	for (i = 0; i < GPU_IMMEDIATE->attribCount_f; i++) {
		glGetVertexAttribfv(
			GPU_IMMEDIATE->attribIndexMap_f[i],
			GL_CURRENT_VERTEX_ATTRIB,
			GPU_IMMEDIATE->attrib_f[i]);
	}

	for (i = 0; i < GPU_IMMEDIATE->attribCount_ub; i++) {
		GLfloat attrib[4];

		glGetVertexAttribfv(
			GPU_IMMEDIATE->attribIndexMap_ub[i],
			GL_CURRENT_VERTEX_ATTRIB,
			attrib);

		GPU_IMMEDIATE->attrib_ub[i][0]
			= (GLubyte)(255.0f * attrib[0]);

		GPU_IMMEDIATE->attrib_ub[i][1]
			= (GLubyte)(255.0f * attrib[1]);

		GPU_IMMEDIATE->attrib_ub[i][2]
			= (GLubyte)(255.0f * attrib[2]);

		GPU_IMMEDIATE->attrib_ub[i][3]
			= (GLubyte)(255.0f * attrib[3]);
	}
}

/* For legacy source compatibility.
   Copies GPU_IMMEDIATE state back into the current OpenGL context */
void gpuImmediateLegacyPutState(void)
{
	GLint savedActiveTexture;
	size_t i;

	GPU_CHECK_NO_BEGIN();

	glColor4ubv(GPU_IMMEDIATE->color);

	glNormal3fv(GPU_IMMEDIATE->normal);

	glGetIntegerv(GL_ACTIVE_TEXTURE, &savedActiveTexture);

	for (i = 0; i < GPU_IMMEDIATE->textureUnitCount; i++) {
		glActiveTexture(GPU_IMMEDIATE->textureUnitMap[i]);
		glTexCoord4fv(GPU_IMMEDIATE->texCoord[i]);
	}

	glActiveTexture(savedActiveTexture);

	for (i = 0; i < GPU_IMMEDIATE->attribCount_f; i++) {
		glVertexAttrib4fv(
			GPU_IMMEDIATE->attribIndexMap_f[i],
			GPU_IMMEDIATE->attrib_f[i]);
	}

	for (i = 0; i < GPU_IMMEDIATE->attribCount_ub; i++) {
		glVertexAttrib4ubv(
			GPU_IMMEDIATE->attribIndexMap_ub[i],
			GPU_IMMEDIATE->attrib_ub[i]);
	}
}

#endif /* GPU_LEGACY_DEBUG */


void gpuImmediateElementSizes(
	GLint vertexSize,
	GLint normalSize,
	GLint colorSize)
{
	GLboolean vertexOK =
		vertexSize > 0 && vertexSize <= GPU_MAX_ELEMENT_SIZE;

	GLboolean normalOK =
		normalSize == 0 || normalSize == 3;

	GLboolean colorOK  =
		colorSize == 0 || colorSize == 3 || colorSize == 4; //-V112

	GPU_CHECK_NO_BEGIN();
	assert(vertexOK);
	assert(normalOK);
	assert(colorOK);

	if (vertexOK) {
		GPU_IMMEDIATE->vertexSize = vertexSize;
	}

	if (normalOK) {
		GPU_IMMEDIATE->normalSize = normalSize;
	}

	if (colorOK) {
		GPU_IMMEDIATE->colorSize  = colorSize;
	}
}

void gpuImmediateMaxVertexCount(GLsizei maxVertexCount)
{
	GLboolean maxVertexCountOK = maxVertexCount >= 0;

	GPU_CHECK_NO_BEGIN();
	assert(maxVertexCountOK);

	if (maxVertexCountOK) {
		GPU_IMMEDIATE->maxVertexCount = maxVertexCount;
	}
}

void gpuImmediateTextureUnitCount(size_t count)
{
	GLboolean countOK = count <= GPU_MAX_TEXTURE_UNITS;

	GPU_CHECK_NO_BEGIN();
	assert(countOK);

	if (countOK) {
		GPU_IMMEDIATE->textureUnitCount = count;
	}
}

void gpuImmediateTexCoordSizes(const GLint *restrict sizes)
{
	size_t i;

	GPU_CHECK_NO_BEGIN();

	for (i = 0; i < GPU_IMMEDIATE->textureUnitCount; i++) {
		GLboolean texCoordSizeOK = sizes[i] >= 0 && sizes[i] <= 4; //-V112

		assert(texCoordSizeOK);

		if (texCoordSizeOK) {
			GPU_IMMEDIATE->texCoordSize[i] = sizes[i];
		}
	}
}

void gpuImmediateTextureUnitMap(const GLenum *restrict map)
{
	size_t i;

	GPU_CHECK_NO_BEGIN();

	for (i = 0; i < GPU_IMMEDIATE->textureUnitCount; i++) {
		GLboolean mapOK =
			map[i] >= GL_TEXTURE0 &&  map[i] <= GPU_IMMEDIATE->lastTexture;

		if (mapOK) {
			GPU_IMMEDIATE->textureUnitMap[i] = map[i];
		}
	}
}


void gpuImmediateFloatAttribCount(size_t count)
{
	GLboolean countOK = count <= GPU_MAX_FLOAT_ATTRIBS;

	GPU_CHECK_NO_BEGIN();
	assert(countOK);

	if (countOK) {
		GPU_IMMEDIATE->attribCount_f = count;
	}
}

void gpuImmediateFloatAttribSizes(const GLint *restrict sizes)
{
	size_t i;

	GPU_CHECK_NO_BEGIN();

	for (i = 0; i < GPU_IMMEDIATE->attribCount_f; i++) {
		GLboolean sizeOK =
			sizes[i] >= 0 && sizes[i] <= GPU_MAX_ELEMENT_SIZE;

		assert(sizeOK);

		if (sizeOK) {
			GPU_IMMEDIATE->attribSize_f[i] = sizes[i];
		}
	}
}

void gpuImmediateFloatAttribIndexMap(const GLuint *restrict map)
{
	size_t i;

	GPU_CHECK_NO_BEGIN();

	for (i = 0; i < GPU_IMMEDIATE->attribCount_f; i++) {
		GPU_IMMEDIATE->attribIndexMap_f[i] = map[i];
	}
}

void gpuImmediateUbyteAttribCount(size_t count)
{
	GLboolean countOK = count <= GPU_MAX_UBYTE_ATTRIBS;

	GPU_CHECK_NO_BEGIN();

	assert(countOK);

	if (countOK) {
		GPU_IMMEDIATE->attribCount_ub = count;
	}
}

void gpuImmediateUbyteAttribSizes(const GLint *restrict sizes)
{
	size_t i;

	GPU_CHECK_NO_BEGIN();

	for (i = 0; i < GPU_IMMEDIATE->attribCount_f; i++) {
		GLboolean sizeOK = sizes[i] >= 0 && sizes[i] <= 4; //-V112

		assert(sizeOK);

		if (sizeOK) {
			GPU_IMMEDIATE->attribSize_f[i] = sizes[i];
		}
	}
}

void gpuImmediateUbyteAttribIndexMap(const GLuint *restrict map)
{
	size_t i;

	GPU_CHECK_NO_BEGIN();

	for (i = 0; i < GPU_IMMEDIATE->attribCount_f; i++) {
		GPU_IMMEDIATE->attribIndexMap_f[i] = map[i];
	}
}

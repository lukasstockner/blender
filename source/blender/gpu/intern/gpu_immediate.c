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

#include "gpu_immediate_internal.h"

#include "MEM_guardedalloc.h"

#include <string.h>



/* global symbol needed because the immediate drawing functons are inline */
GPUimmediate *restrict GPU_IMMEDIATE = NULL;



GLsizei gpu_calc_stride(void)
{
	size_t i;
	size_t stride = 0;

	/* vertex */

	if (GPU_IMMEDIATE->format.vertexSize != 0) {
		stride += (size_t)(GPU_IMMEDIATE->format.vertexSize) * sizeof(GLfloat);
	}

	/* normal */

	if (GPU_IMMEDIATE->format.normalSize != 0) {
		/* normals always have 3 components */
		stride += 3 * sizeof(GLfloat);
	}

	/* color */

	if (GPU_IMMEDIATE->format.colorSize != 0) {
		/* color always get 4 bytes for efficient memory alignment */
		stride += 4; //-V112
	}

	/* texture coordinate */

	for (i = 0; i < GPU_IMMEDIATE->format.textureUnitCount; i++) {
		stride +=
			(size_t)(GPU_IMMEDIATE->format.texCoordSize[i]) * sizeof(GLfloat);
	}

	/* float vertex attribute */

	for (i = 0; i < GPU_IMMEDIATE->format.attribCount_f; i++) {
		stride +=
			(size_t)(GPU_IMMEDIATE->format.attribSize_f[i]) * sizeof(GLfloat);
	}

	/* byte vertex attribute */

	for (i = 0; i < GPU_IMMEDIATE->format.attribCount_ub; i++) {
		/* byte attributes always get 4 bytes for efficient memory alignment */
		stride +=
			(size_t)(GPU_IMMEDIATE->format.attribSize_ub[i]) * sizeof(GLfloat);
	}

	return (GLsizei)stride;
}



void gpuImmediateLock(void)
{
	GPU_CHECK_CAN_LOCK();

	if (GPU_IMMEDIATE->lockCount == 0) {
		GPU_IMMEDIATE->lockBuffer();
	}

	GPU_IMMEDIATE->lockCount++;
}



void gpuImmediateUnlock(void)
{
	GPU_CHECK_CAN_UNLOCK();

	GPU_IMMEDIATE->lockCount--;

	if (GPU_IMMEDIATE->lockCount == 0) {
		GPU_IMMEDIATE->unlockBuffer();

		/* reset vertex format */
		memset(&(GPU_IMMEDIATE->format), 0, sizeof(GPU_IMMEDIATE->format));
		GPU_IMMEDIATE->format.vertexSize = 3;
	}
}



GLint gpuImmediateLockCount(void)
{
	BLI_assert(GPU_IMMEDIATE);

	if (!GPU_IMMEDIATE) {
		return GL_FALSE;
	}

	return GPU_IMMEDIATE->lockCount;
}



#if GPU_SAFETY
static void calc_last_texture(GPUimmediate* immediate)
{
	GLint maxTextureCoords = 1;
	GLint maxCombinedTextureImageUnits = 0;

	if (GLEW_VERSION_1_3 || GLEW_ARB_multitexture) {
		glGetIntegerv(
			GL_MAX_TEXTURE_COORDS,
			&maxTextureCoords);
	}

	if (GLEW_VERSION_2_0) {
		glGetIntegerv(
			GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS,
			&maxCombinedTextureImageUnits);
	}

	immediate->lastTexture =
		GL_TEXTURE0 + MAX2(maxTextureCoords, maxCombinedTextureImageUnits) - 1;
}
#endif



GPUimmediate * gpuNewImmediate(void)
{
	GPUimmediate *restrict immediate =
		MEM_callocN(sizeof(GPUimmediate), "GPUimmediate");

	BLI_assert(immediate);

	immediate->format.vertexSize = 3;

	//if (GLEW_ARB_vertex_buffer_object) {
	//	immediate->lockBuffer     = gpu_lock_buffer_vbo;
	//	immediate->beginBuffer    = gpu_begin_buffer_vbo;
	//	immediate->endBuffer      = gpu_end_buffer_vbo;
	//	immediate->unlockBuffer   = gpu_unlock_buffer_vbo;
	//	immediate->shutdownBuffer = gpu_shutdown_buffer_vbo;
	//}
	//else {
		immediate->lockBuffer     = gpu_lock_buffer_gl11;
		immediate->unlockBuffer   = gpu_unlock_buffer_gl11;
		immediate->beginBuffer    = gpu_begin_buffer_gl11;
		immediate->endBuffer      = gpu_end_buffer_gl11;
		immediate->shutdownBuffer = gpu_shutdown_buffer_gl11;
	//}

#if GPU_SAFETY
	calc_last_texture(immediate);
#endif

	return immediate;
}



void gpuImmediateMakeCurrent(GPUimmediate *restrict immediate)
{
	GPU_IMMEDIATE = immediate;
}



void gpuDeleteImmediate(GPUimmediate *restrict immediate)
{
	BLI_assert(immediate);

	if (!immediate) {
		return;
	}

	BLI_assert(!(immediate->buffer));

	if (immediate->buffer) {
		SWAP(GPUimmediate*, immediate, GPU_IMMEDIATE);
		gpuEnd();
		SWAP(GPUimmediate*, immediate, GPU_IMMEDIATE);
	}

	if (GPU_IMMEDIATE == immediate) {
		gpuImmediateMakeCurrent(NULL);
	}

	BLI_assert(immediate->shutdownBuffer);

	immediate->shutdownBuffer(immediate);

	MEM_freeN(immediate);
}



void gpuImmediateElementSizes(
	GLint vertexSize,
	GLint normalSize,
	GLint colorSize)
{
	GLboolean vertexOK;
	GLboolean normalOK;
	GLboolean colorOK;

	GPU_CHECK_CAN_SETUP();

	GPU_SAFE_STMT(
		vertexOK,
		vertexSize > 0 && vertexSize <= GPU_MAX_ELEMENT_SIZE,
		GPU_IMMEDIATE->format.vertexSize = vertexSize);

	GPU_SAFE_STMT(
		normalOK,
		normalSize == 0 || normalSize == 3,
		GPU_IMMEDIATE->format.normalSize = normalSize);

	GPU_SAFE_STMT(
		colorOK,
		colorSize == 0 || colorSize == 4, //-V112
		GPU_IMMEDIATE->format.colorSize = colorSize);
}



void gpuImmediateMaxVertexCount(GLsizei maxVertexCount)
{
	GLboolean maxVertexCountOK;

	GPU_CHECK_CAN_SETUP();

	GPU_SAFE_STMT(
		maxVertexCountOK,
		maxVertexCount >= 0,
		GPU_IMMEDIATE->maxVertexCount = maxVertexCount);
}



void gpuImmediateTextureUnitCount(size_t count)
{
	GLboolean countOK;

	GPU_CHECK_CAN_SETUP();
	
	GPU_SAFE_STMT(
		countOK,
		count <= GPU_MAX_TEXTURE_UNITS,
		GPU_IMMEDIATE->format.textureUnitCount = count);
}



void gpuImmediateTexCoordSizes(const GLint *restrict sizes)
{
	size_t i;

	GPU_CHECK_CAN_SETUP();

	for (i = 0; i < GPU_IMMEDIATE->format.textureUnitCount; i++) {
		GLboolean texCoordSizeOK;

		GPU_SAFE_STMT(
			texCoordSizeOK,
			sizes[i] > 0 && sizes[i] <= GPU_MAX_ELEMENT_SIZE,
			GPU_IMMEDIATE->format.texCoordSize[i] = sizes[i]);
	}
}



void gpuImmediateTextureUnitMap(const GLenum *restrict map)
{
	size_t i;

	GPU_CHECK_CAN_SETUP();

	for (i = 0; i < GPU_IMMEDIATE->format.textureUnitCount; i++) {
		GLboolean mapOK;

		GPU_SAFE_STMT(
			mapOK,
			map[i] >= GL_TEXTURE0 &&  map[i] <= GPU_IMMEDIATE->lastTexture,
			GPU_IMMEDIATE->format.textureUnitMap[i] = map[i]);
	}
}



void gpuImmediateFloatAttribCount(size_t count)
{
	GLboolean countOK;

	GPU_CHECK_CAN_SETUP();

	GPU_SAFE_STMT(
		countOK,
		count <= GPU_MAX_FLOAT_ATTRIBS,
		GPU_IMMEDIATE->format.attribCount_f = count);
}



void gpuImmediateFloatAttribSizes(const GLint *restrict sizes)
{
	size_t i;

	GPU_CHECK_CAN_SETUP();

	for (i = 0; i < GPU_IMMEDIATE->format.attribCount_f; i++) {
		GLboolean sizeOK;

		GPU_SAFE_STMT(
			sizeOK,
			sizes[i] > 0 && sizes[i] <= GPU_MAX_ELEMENT_SIZE,
			GPU_IMMEDIATE->format.attribSize_f[i] = sizes[i]);
	}
}



void gpuImmediateFloatAttribIndexMap(const GLuint *restrict map)
{
	size_t i;

	GPU_CHECK_CAN_SETUP();

	for (i = 0; i < GPU_IMMEDIATE->format.attribCount_f; i++) {
		GPU_IMMEDIATE->format.attribIndexMap_f[i] = map[i];
	}
}



void gpuImmediateUbyteAttribCount(size_t count)
{
	GLboolean countOK;

	GPU_CHECK_CAN_SETUP();

	GPU_SAFE_STMT(
		countOK,
		count <= GPU_MAX_UBYTE_ATTRIBS,
		GPU_IMMEDIATE->format.attribCount_ub = count);
}

void gpuImmediateUbyteAttribSizes(const GLint *restrict sizes)
{
	size_t i;

	GPU_CHECK_CAN_SETUP();

	for (i = 0; i < GPU_IMMEDIATE->format.attribCount_ub; i++) {
		GLboolean sizeOK;
		
		GPU_SAFE_STMT(
			sizeOK,
			sizes[i] > 0 && sizes[i] <= 4, //-V112
			GPU_IMMEDIATE->format.attribSize_ub[i] = sizes[i]);
	}
}



void gpuImmediateUbyteAttribIndexMap(const GLuint *restrict map)
{
	size_t i;

	GPU_CHECK_CAN_SETUP();

	for (i = 0; i < GPU_IMMEDIATE->format.attribCount_ub; i++) {
		GPU_IMMEDIATE->format.attribIndexMap_ub[i] = map[i];
	}
}



static void end_begin(void)
{
	GPU_IMMEDIATE->endBuffer();

	GPU_IMMEDIATE->buffer = NULL;
	GPU_IMMEDIATE->offset = 0;
	GPU_IMMEDIATE->count  = 1; /* count the vertex that triggered this */

	GPU_IMMEDIATE->beginBuffer();
}



void gpu_vector_copy(void)
{
	size_t i;
	size_t size;
	size_t offset;
	char *restrict buffer;
	int maxVertexCountOK = -1;

	GPU_SAFE_RETURN(GPU_IMMEDIATE->maxVertexCount != 0, maxVertexCountOK);

	if (GPU_IMMEDIATE->count == GPU_IMMEDIATE->maxVertexCount) {
		end_begin(); /* draw and clear buffer */
	}
	else {
		GPU_IMMEDIATE->count++;
	}

	buffer = GPU_IMMEDIATE->buffer;
	offset = GPU_IMMEDIATE->offset;

	/* vertex */

	size = (size_t)(GPU_IMMEDIATE->format.vertexSize) * sizeof(GLfloat);
	memcpy(buffer + offset, GPU_IMMEDIATE->vertex, size);
	offset += size;

	/* normal */

	if (GPU_IMMEDIATE->format.normalSize != 0) {
		/* normals are always have 3 components */
		memcpy(buffer + offset, GPU_IMMEDIATE->normal, 3*sizeof(GLfloat));
		offset += 3*sizeof(GLfloat);
	}

	/* color */

	if (GPU_IMMEDIATE->format.colorSize != 0) {
		/* 4 bytes are always reserved for color, for efficient memory alignment */
		memcpy(buffer + offset, GPU_IMMEDIATE->color, 4); //-V112
		offset += 4; //-V112
	}

	/* texture coordinate(s) */

	for (i = 0; i < GPU_IMMEDIATE->format.textureUnitCount; i++) {
		size = (size_t)(GPU_IMMEDIATE->format.texCoordSize[i]) * sizeof(GLfloat);
		memcpy(buffer + offset, GPU_IMMEDIATE->texCoord[i], size);
		offset += size;
	}

	/* float vertex attribute(s) */

	for (i = 0; i < GPU_IMMEDIATE->format.attribCount_f; i++) {
		size = (size_t)(GPU_IMMEDIATE->format.attribSize_f[i]) * sizeof(GLfloat);
		memcpy(buffer + offset, GPU_IMMEDIATE->attrib_f[i], size);
		offset += size;
	}

	/* unsigned byte vertex attirbute(s) */

	for (i = 0; i < GPU_IMMEDIATE->format.attribCount_ub; i++) {
		/* 4 bytes are always reserved for byte attributes, for efficient memory alignment */
		memcpy(buffer + offset, GPU_IMMEDIATE->attrib_ub[i], 4*sizeof(GLubyte));
		offset += 4*sizeof(GLubyte);
	}

	GPU_IMMEDIATE->offset = offset;
}



/* vertex formats */

void gpuImmediateFormat_V2(void)
{
	if (gpuImmediateLockCount() == 0) {
		gpuImmediateElementSizes(2, 0, 0);
	}

	gpuImmediateLock();
}

void gpuImmediateFormat_V3(void)
{
	if (gpuImmediateLockCount() == 0) {
		gpuImmediateElementSizes(3, 0, 0);
	}

	gpuImmediateLock();
}

void gpuImmediateFormat_N3_V3(void)
{
	if (gpuImmediateLockCount() == 0) {
		gpuImmediateElementSizes(3, 3, 0);
	}

	gpuImmediateLock();
}

void gpuImmediateFormat_C4_V3(void)
{
	if (gpuImmediateLockCount() == 0) {
		gpuImmediateElementSizes(3, 0, 4);
	}

	gpuImmediateLock();
}

void gpuImmediateFormat_C4_N3_V3(void)
{
	if (gpuImmediateLockCount() == 0) {
		gpuImmediateElementSizes(3, 3, 3);
	}

	gpuImmediateLock();
}

void gpuImmediateFormat_T2_C4_N3_V3(void)
{
	if (gpuImmediateLockCount() == 0) {

		GLint texCoordSizes[1] = { 2 };
		GLint texUnitMap[1]    = { GL_TEXTURE0 };

		gpuImmediateElementSizes(3, 3, 4);
		gpuImmediateTextureUnitCount(1);
		gpuImmediateTexCoordSizes(texCoordSizes);
		gpuImmediateTextureUnitMap(texUnitMap);
	}

	gpuImmediateLock();
}

void gpuImmediateUnformat(void)
{
	gpuImmediateUnlock();
}

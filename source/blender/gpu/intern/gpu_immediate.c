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
	GPU_CHECK_NO_BEGIN();

	if (GPU_IMMEDIATE->lockCount == 0) {
		assert(GPU_IMMEDIATE->lockBuffer);

		if (GPU_IMMEDIATE->lockBuffer) {
			GPU_IMMEDIATE->lockBuffer();
		}
	}

	GPU_IMMEDIATE->lockCount++;
}



static reset(void)
{
	memset(&(GPU_IMMEDIATE->format), 0, sizeof(GPU_IMMEDIATE->format));
	GPU_IMMEDIATE->format.vertexSize = 3;
}



void gpuImmediateUnlock(void)
{
	GPU_CHECK_NO_BEGIN();

	assert(GPU_IMMEDIATE->lockCount > 0);

	if (GPU_IMMEDIATE->lockCount == 1) {
		assert(GPU_IMMEDIATE->unlockBuffer);

		if (GPU_IMMEDIATE->unlockBuffer) {
			GPU_IMMEDIATE->unlockBuffer();
		}

		reset();
	}

	GPU_IMMEDIATE->lockCount--;
}



GLint gpuImmediateLockCount(void)
{
	assert(GPU_IMMEDIATE);

	if (!GPU_IMMEDIATE) {
		return GL_FALSE;
	}

	return GPU_IMMEDIATE->lockCount;
}



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



GPUimmediate *restrict gpuNewImmediate(void)
{
	GPUimmediate *restrict immediate =
		MEM_callocN(sizeof(GPUimmediate), "GPUimmediate");

	assert(immediate);

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

	calc_last_texture(immediate);

	return immediate;
}



void gpuImmediateMakeCurrent(GPUimmediate *restrict immediate)
{
	GPU_IMMEDIATE = immediate;
}



void gpuDeleteImmediate(GPUimmediate *restrict immediate)
{
	assert(immediate);

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

	assert(immediate->shutdownBuffer);

	if (immediate->shutdownBuffer) {
		immediate->shutdownBuffer(immediate);
	}

	MEM_freeN(immediate);
}



#ifdef GPU_LEGACY_INTEROP

/* For legacy source compatibility.
   Copies the current OpenGL state into the GPU_IMMEDIATE */
void gpu_legacy_get_state(void)
{
	size_t i;

	GPU_CHECK_NO_BEGIN();

	if (GPU_IMMEDIATE->format.colorSize != 0) {
		GLfloat color[4];
		glGetFloatv(GL_CURRENT_COLOR, color);
		GPU_IMMEDIATE->color[0] = (GLubyte)(255.0f * color[0]);
		GPU_IMMEDIATE->color[1] = (GLubyte)(255.0f * color[1]);
		GPU_IMMEDIATE->color[2] = (GLubyte)(255.0f * color[2]);
		GPU_IMMEDIATE->color[3] = (GLubyte)(255.0f * color[3]);
	}

	if (GPU_IMMEDIATE->format.normalSize != 0) {
		glGetFloatv(GL_CURRENT_NORMAL, GPU_IMMEDIATE->normal);
	}

	if (GPU_IMMEDIATE->format.textureUnitCount == 1) {
		glGetFloatv(GL_CURRENT_TEXTURE_COORDS, GPU_IMMEDIATE->texCoord[0]);
	}
	else if (GPU_IMMEDIATE->format.textureUnitCount > 1) {
		for (i = 0; i < GPU_IMMEDIATE->format.textureUnitCount; i++) {
			glClientActiveTexture(GPU_IMMEDIATE->format.textureUnitMap[i]);
			glGetFloatv(GL_CURRENT_TEXTURE_COORDS, GPU_IMMEDIATE->texCoord[i]);
		}

		glClientActiveTexture(GL_TEXTURE0);
	}

	for (i = 0; i < GPU_IMMEDIATE->format.attribCount_f; i++) {
		glGetVertexAttribfv(
			GPU_IMMEDIATE->format.attribIndexMap_f[i],
			GL_CURRENT_VERTEX_ATTRIB,
			GPU_IMMEDIATE->attrib_f[i]);
	}

	for (i = 0; i < GPU_IMMEDIATE->format.attribCount_ub; i++) {
		GLfloat attrib[4];

		glGetVertexAttribfv(
			GPU_IMMEDIATE->format.attribIndexMap_ub[i],
			GL_CURRENT_VERTEX_ATTRIB,
			attrib);

		GPU_IMMEDIATE->attrib_ub[i][0] = (GLubyte)(255.0f * attrib[0]);
		GPU_IMMEDIATE->attrib_ub[i][1] = (GLubyte)(255.0f * attrib[1]);
		GPU_IMMEDIATE->attrib_ub[i][2] = (GLubyte)(255.0f * attrib[2]);
		GPU_IMMEDIATE->attrib_ub[i][3] = (GLubyte)(255.0f * attrib[3]);
	}
}



/* For legacy source compatibility.
   Copies GPU_IMMEDIATE state back into the current OpenGL context */
void gpu_legacy_put_state(void)
{
	size_t i;

	GPU_CHECK_NO_BEGIN();

	if (GPU_IMMEDIATE->format.colorSize != 0) {
		glColor4ubv(GPU_IMMEDIATE->color);
	}

	if (GPU_IMMEDIATE->format.normalSize != 0) {
		glNormal3fv(GPU_IMMEDIATE->normal);
	}

	if (GPU_IMMEDIATE->format.textureUnitCount == 1) {
		glTexCoord4fv(GPU_IMMEDIATE->texCoord[0]);
	}
	else if (GPU_IMMEDIATE->format.textureUnitCount > 1) {
		for (i = 0; i < GPU_IMMEDIATE->format.textureUnitCount; i++) {
			glClientActiveTexture(GPU_IMMEDIATE->format.textureUnitMap[i]);
			glTexCoord4fv(GPU_IMMEDIATE->texCoord[i]);
		}

		glClientActiveTexture(GL_TEXTURE0);
	}

	for (i = 0; i < GPU_IMMEDIATE->format.attribCount_f; i++) {
		glVertexAttrib4fv(
			GPU_IMMEDIATE->format.attribIndexMap_f[i],
			GPU_IMMEDIATE->attrib_f[i]);
	}

	for (i = 0; i < GPU_IMMEDIATE->format.attribCount_ub; i++) {
		glVertexAttrib4ubv(
			GPU_IMMEDIATE->format.attribIndexMap_ub[i],
			GPU_IMMEDIATE->attrib_ub[i]);
	}
}

#endif /* GPU_LEGACY_INTEROP */



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

	GPU_CHECK_NO_LOCK();
	GPU_CHECK_NO_BEGIN();
	assert(vertexOK);
	assert(normalOK);
	assert(colorOK);

	if (vertexOK) {
		GPU_IMMEDIATE->format.vertexSize = vertexSize;
	}

	if (normalOK) {
		GPU_IMMEDIATE->format.normalSize = normalSize;
	}

	if (colorOK) {
		GPU_IMMEDIATE->format.colorSize  = colorSize;
	}
}



void gpuImmediateMaxVertexCount(GLsizei maxVertexCount)
{
	GLboolean maxVertexCountOK = maxVertexCount >= 0;

	GPU_CHECK_NO_LOCK();
	assert(maxVertexCountOK);

	if (maxVertexCountOK) {
		GPU_IMMEDIATE->maxVertexCount = maxVertexCount;
	}
}



void gpuImmediateTextureUnitCount(size_t count)
{
	GLboolean countOK = count <= GPU_MAX_TEXTURE_UNITS;

	GPU_CHECK_NO_LOCK();
	assert(countOK);

	if (countOK) {
		GPU_IMMEDIATE->format.textureUnitCount = count;
	}
}



void gpuImmediateTexCoordSizes(const GLint *restrict sizes)
{
	size_t i;

	GPU_CHECK_NO_LOCK();

	for (i = 0; i < GPU_IMMEDIATE->format.textureUnitCount; i++) {
		GLboolean texCoordSizeOK =
			sizes[i] > 0 && sizes[i] <= GPU_MAX_ELEMENT_SIZE;

		assert(texCoordSizeOK);

		if (texCoordSizeOK) {
			GPU_IMMEDIATE->format.texCoordSize[i] = sizes[i];
		}
	}
}



void gpuImmediateTextureUnitMap(const GLenum *restrict map)
{
	size_t i;

	GPU_CHECK_NO_LOCK();

	for (i = 0; i < GPU_IMMEDIATE->format.textureUnitCount; i++) {
		GLboolean mapOK =
			map[i] >= GL_TEXTURE0 &&  map[i] <= GPU_IMMEDIATE->lastTexture;

		if (mapOK) {
			GPU_IMMEDIATE->format.textureUnitMap[i] = map[i];
		}
	}
}



void gpuImmediateFloatAttribCount(size_t count)
{
	GLboolean countOK = count <= GPU_MAX_FLOAT_ATTRIBS;

	GPU_CHECK_NO_LOCK();
	assert(countOK);

	if (countOK) {
		GPU_IMMEDIATE->format.attribCount_f = count;
	}
}



void gpuImmediateFloatAttribSizes(const GLint *restrict sizes)
{
	size_t i;

	GPU_CHECK_NO_LOCK();

	for (i = 0; i < GPU_IMMEDIATE->format.attribCount_f; i++) {
		GLboolean sizeOK =
			sizes[i] > 0 && sizes[i] <= GPU_MAX_ELEMENT_SIZE;

		assert(sizeOK);

		if (sizeOK) {
			GPU_IMMEDIATE->format.attribSize_f[i] = sizes[i];
		}
	}
}



void gpuImmediateFloatAttribIndexMap(const GLuint *restrict map)
{
	size_t i;

	GPU_CHECK_NO_LOCK();

	for (i = 0; i < GPU_IMMEDIATE->format.attribCount_f; i++) {
		GPU_IMMEDIATE->format.attribIndexMap_f[i] = map[i];
	}
}



void gpuImmediateUbyteAttribCount(size_t count)
{
	GLboolean countOK = count <= GPU_MAX_UBYTE_ATTRIBS;

	GPU_CHECK_NO_LOCK();

	assert(countOK);

	if (countOK) {
		GPU_IMMEDIATE->format.attribCount_ub = count;
	}
}



void gpuImmediateUbyteAttribSizes(const GLint *restrict sizes)
{
	size_t i;

	GPU_CHECK_NO_LOCK();

	for (i = 0; i < GPU_IMMEDIATE->format.attribCount_ub; i++) {
		GLboolean sizeOK = sizes[i] > 0 && sizes[i] <= 4; //-V112

		assert(sizeOK);

		if (sizeOK) {
			GPU_IMMEDIATE->format.attribSize_ub[i] = sizes[i];
		}
	}
}



void gpuImmediateUbyteAttribIndexMap(const GLuint *restrict map)
{
	size_t i;

	GPU_CHECK_NO_LOCK();

	for (i = 0; i < GPU_IMMEDIATE->format.attribCount_ub; i++) {
		GPU_IMMEDIATE->format.attribIndexMap_ub[i] = map[i];
	}
}



void gpu_vector_copy(void)
{
	size_t i;
	size_t size;
	size_t offset;
	char *restrict buffer;
	GLboolean countOK = GPU_IMMEDIATE->count < GPU_IMMEDIATE->maxVertexCount;

	assert(countOK);

	if (!countOK) {
		return;
	}

	GPU_IMMEDIATE->count++;

	assert(GPU_IMMEDIATE->buffer);

	if (!(GPU_IMMEDIATE->buffer)) {
		return;
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
		memcpy(buffer + offset, GPU_IMMEDIATE->color, 4*sizeof(GLubyte));
		offset += 4*sizeof(GLubyte);
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

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

/** \file blender/gpu/intern/gpu_immediate.c
*  \ingroup gpu
*/

#include "gpu_immediate_internal.h"

#include "MEM_guardedalloc.h"

#include <math.h>
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



void gpuImmediateFormatReset(void)
{
	/* reset vertex format */
	memset(&(GPU_IMMEDIATE->format), 0, sizeof(GPU_IMMEDIATE->format));
	GPU_IMMEDIATE->format.vertexSize = 3;
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
	}
}



GLint gpuImmediateLockCount(void)
{
	GPU_ASSERT(GPU_IMMEDIATE);

	if (!GPU_IMMEDIATE) {
		return GL_FALSE;
	}

	return GPU_IMMEDIATE->lockCount;
}



#if GPU_SAFETY
static void calc_last_texture(GPUimmediate* immediate)
{
	GLint maxTextureUnits;
	GLint maxTextureCoords;
	GLint maxCombinedTextureImageUnits;

	if (GLEW_VERSION_1_3 || GLEW_ARB_multitexture) {
		glGetIntegerv(
			GL_MAX_TEXTURE_UNITS,
			&maxTextureUnits);
	}
	else {
		maxTextureUnits = 1;
	}

	if (GLEW_VERSION_2_0 || GLEW_ARB_fragment_program) {
		glGetIntegerv(
			GL_MAX_TEXTURE_COORDS,
			&maxTextureCoords);

		glGetIntegerv(
			GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS,
			&maxCombinedTextureImageUnits);
	}
	else {
		maxTextureCoords             = 0;
		maxCombinedTextureImageUnits = 0;
	}

	immediate->lastTexture =
		GL_TEXTURE0 + MAX3(maxTextureUnits, maxTextureCoords, maxCombinedTextureImageUnits) - 1;
}
#endif



static void gpu_copy_vertex(void);

static void gpu_append_client_arrays(
	const GPUarrays *restrict arrays,
	GLint first,
	GLsizei count);



GPUimmediate* gpuNewImmediate(void)
{
	GPUimmediate *restrict immediate =
		MEM_callocN(sizeof(GPUimmediate), "GPUimmediate");

	GPU_ASSERT(immediate);

	immediate->format.vertexSize = 3;

	immediate->copyVertex   = gpu_copy_vertex;
	immediate->appendClientArrays = gpu_append_client_arrays;

	//	immediate->lockBuffer          = gpu_lock_buffer_vbo;
	//	immediate->beginBuffer         = gpu_begin_buffer_vbo;
	//	immediate->endBuffer           = gpu_end_buffer_vbo;
	//	immediate->unlockBuffer        = gpu_unlock_buffer_vbo;
	//	immediate->shutdownBuffer      = gpu_shutdown_buffer_vbo;
	//	immediate->currentColor        = gpu_current_color_vbo;
	//	immediate->getCurrentColor     = gpu_get_current_color_vbo;
	//	immediate->currentNormal       = gpu_current_normal_vbo;
	//	immediate->indexBeginBuffer    = gpu_index_begin_buffer_vbo;
	//	immediate->indexEndBuffe r     = gpu_index_end_buffer_vbo;
	//	immediate->indexShutdownBuffer = gpu_index_shutdown_buffer_vbo;
	//	immediate->drawElements        = gpu_draw_elements_vbo;
	//	immediate->drawRangeElements   = gpu_draw_range_elements_vbo;
	//}
	//else {
		immediate->lockBuffer          = gpu_lock_buffer_gl11;
		immediate->unlockBuffer        = gpu_unlock_buffer_gl11;
		immediate->beginBuffer         = gpu_begin_buffer_gl11;
		immediate->endBuffer           = gpu_end_buffer_gl11;
		immediate->shutdownBuffer      = gpu_shutdown_buffer_gl11;
		immediate->currentColor        = gpu_current_color_gl11;
		immediate->getCurrentColor     = gpu_get_current_color_gl11;
		immediate->currentNormal       = gpu_current_normal_gl11;
		immediate->indexBeginBuffer    = gpu_index_begin_buffer_gl11;
		immediate->indexShutdownBuffer = gpu_index_shutdown_buffer_gl11;
		immediate->indexEndBuffer      = gpu_index_end_buffer_gl11;
		immediate->drawElements        = gpu_draw_elements_gl11;
		immediate->drawRangeElements   = gpu_draw_range_elements_gl11;
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
	if (!immediate) {
		return;
	}

	if (GPU_IMMEDIATE == immediate) {
		gpuImmediateMakeCurrent(NULL);
	}

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



static GLboolean end_begin(void)
{
#if GPU_SAFETY
	GPU_IMMEDIATE->hasOverflowed = GL_TRUE;
#endif

	if (GPU_IMMEDIATE->mode != GL_NOOP) {
		GPU_IMMEDIATE->endBuffer();

		GPU_IMMEDIATE->buffer = NULL;
		GPU_IMMEDIATE->offset = 0;
		GPU_IMMEDIATE->count  = 1; /* count the vertex that triggered this */

		GPU_IMMEDIATE->beginBuffer();

		return GL_TRUE;
	}
	else {
		return GL_FALSE;
	}
}



static void gpu_copy_vertex(void)
{
	size_t i;
	size_t size;
	size_t offset;
	char *restrict buffer;

#if GPU_SAFETY
	{
	int maxVertexCountOK;
	GPU_SAFE_RETURN(GPU_IMMEDIATE->maxVertexCount != 0, maxVertexCountOK,);
	}
#endif

	if (GPU_IMMEDIATE->count == GPU_IMMEDIATE->maxVertexCount) {
		GLboolean restarted;

		restarted = end_begin(); /* draw and clear buffer */

		GPU_ASSERT(restarted);

		if (!restarted) {
			return;
		}
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

#if GPU_SAFETY
void gpuSafetyImmediateFormat_V2(const char* file, int line)
#else
void gpuImmediateFormat_V2(void)
#endif
{
#if GPU_SAFETY
	printf("%s(%d): gpuImmediateFormat_V2\n", file, line);
#endif

	if (gpuImmediateLockCount() == 0) {
		gpuImmediateFormatReset();

		gpuImmediateElementSizes(2, 0, 0);
	}

	gpuImmediateLock();
}

#if GPU_SAFETY
void gpuSafetyImmediateFormat_C4_V2(const char* file, int line)
#else
void gpuImmediateFormat_C4_V2(void)
#endif
{
#if GPU_SAFETY
	printf("%s(%d): gpuImmediateFormat_C4_V2\n", file, line);
#endif

	if (gpuImmediateLockCount() == 0) {
		gpuImmediateFormatReset();

		gpuImmediateElementSizes(2, 0, 4); //-V112
	}

	gpuImmediateLock();
}

#if GPU_SAFETY
void gpuSafetyImmediateFormat_T2_V2(const char* file, int line)
#else
void gpuImmediateFormat_T2_V2(void)
#endif
{
#if GPU_SAFETY
	printf("%s(%d): gpuImmediateFormat_T2_V2\n", file, line);
#endif

	if (gpuImmediateLockCount() == 0) {
		GLint texCoordSizes[1] = { 2 };
		GLenum texUnitMap[1]    = { GL_TEXTURE0 };

		gpuImmediateFormatReset();

		gpuImmediateElementSizes(2, 0, 0);

		gpuImmediateTextureUnitCount(1);
		gpuImmediateTexCoordSizes(texCoordSizes);
		gpuImmediateTextureUnitMap(texUnitMap);
	}

	gpuImmediateLock();
}

#if GPU_SAFETY
void gpuSafetyImmediateFormat_T2_C4_V2(const char* file, int line)
#else
void gpuImmediateFormat_T2_C4_V2(void)
#endif
{
#if GPU_SAFETY
	printf("%s(%d): gpuImmediateFormat_T2_C4_V2\n", file, line);
#endif

	if (gpuImmediateLockCount() == 0) {
		GLint texCoordSizes[1] = { 2 };
		GLenum texUnitMap[1]    = { GL_TEXTURE0 };

		gpuImmediateFormatReset();

		gpuImmediateElementSizes(2, 0, 4); //-V112

		gpuImmediateTextureUnitCount(1);
		gpuImmediateTexCoordSizes(texCoordSizes);
		gpuImmediateTextureUnitMap(texUnitMap);
	}

	gpuImmediateLock();
}

#if GPU_SAFETY
void gpuSafetyImmediateFormat_V3(const char* file, int line)
#else
void gpuImmediateFormat_V3(void)
#endif
{
#if GPU_SAFETY
	printf("%s(%d): gpuImmediateFormat_V3\n", file, line);
#endif

	if (gpuImmediateLockCount() == 0) {
		gpuImmediateFormatReset();

		gpuImmediateElementSizes(3, 0, 0);
	}

	gpuImmediateLock();
}

#if GPU_SAFETY
void gpuSafetyImmediateFormat_N3_V3(const char* file, int line)
#else
void gpuImmediateFormat_N3_V3(void)
#endif
{
#if GPU_SAFETY
	printf("%s(%d): gpuImmediateFormat_N3_V3\n", file, line);
#endif

	if (gpuImmediateLockCount() == 0) {
		gpuImmediateFormatReset();

		gpuImmediateElementSizes(3, 3, 0);
	}

	gpuImmediateLock();
}

#if GPU_SAFETY
void gpuSafetyImmediateFormat_C4_V3(const char* file, int line)
#else
void gpuImmediateFormat_C4_V3(void)
#endif
{
#if GPU_SAFETY
	printf("%s(%d): gpuImmediateFormat_C4_V3\n", file, line);
#endif

	if (gpuImmediateLockCount() == 0) {
		gpuImmediateFormatReset();

		gpuImmediateElementSizes(3, 0, 4); //-V112
	}

	gpuImmediateLock();
}

#if GPU_SAFETY
void gpuSafetyImmediateFormat_C4_N3_V3(const char* file, int line)
#else
void gpuImmediateFormat_C4_N3_V3(void)
#endif
{
#if GPU_SAFETY
	printf("%s(%d): gpuImmediateFormat_C4_N3_V3\n", file, line);
#endif

	if (gpuImmediateLockCount() == 0) {
		gpuImmediateFormatReset();

		gpuImmediateElementSizes(3, 3, 4); //-V112
	}

	gpuImmediateLock();
}

#if GPU_SAFETY
void gpuSafetyImmediateFormat_T2_C4_N3_V3(const char* file, int line)
#else
void gpuImmediateFormat_T2_C4_N3_V3(void)
#endif
{
#if GPU_SAFETY
	printf("%s(%d): gpuImmediateFormat_T2_C4_N3_V3\n", file, line);
#endif

	if (gpuImmediateLockCount() == 0) {
		GLint texCoordSizes[1] = { 2 };
		GLenum texUnitMap[1]    = { GL_TEXTURE0 };

		gpuImmediateFormatReset();

		gpuImmediateElementSizes(3, 3, 4); //-V112
		gpuImmediateTextureUnitCount(1);
		gpuImmediateTexCoordSizes(texCoordSizes);
		gpuImmediateTextureUnitMap(texUnitMap);
	}

	gpuImmediateLock();
}

#if GPU_SAFETY
void gpuSafetyImmediateFormat_T3_C4_V3(const char* file, int line)
#else
void gpuImmediateFormat_T3_C4_V3(void)
#endif
{
#if GPU_SAFETY
	printf("%s(%d): gpuImmediateFormat_T3_C4_V3\n", file, line);
#endif

	if (gpuImmediateLockCount() == 0) {
		GLint texCoordSizes[1] = { 3 };
		GLenum texUnitMap[1]    = { GL_TEXTURE0 };

		gpuImmediateFormatReset();

		gpuImmediateElementSizes(3, 0, 4); //-V112
		gpuImmediateTextureUnitCount(1);
		gpuImmediateTexCoordSizes(texCoordSizes);
		gpuImmediateTextureUnitMap(texUnitMap);
	}

	gpuImmediateLock();
}

#if GPU_SAFETY
void gpuSafetyImmediateUnformat(const char* file, int line)
#else
void gpuImmediateUnformat(void)
#endif
{
#if GPU_SAFETY
	printf("%s(%d): gpuImmediateUnformat\n", file, line);
#endif

	gpuImmediateUnlock();
}



BLI_INLINE void currentColor(void)
{
	GPU_IMMEDIATE->currentColor();

#if GPU_SAFETY
	GPU_IMMEDIATE->isColorValid = GL_TRUE;
#endif
}

void gpuCurrentColor3f(GLfloat r, GLfloat g, GLfloat b)
{
	GPU_CHECK_CAN_CURRENT();

	GPU_IMMEDIATE->color[0] = (GLubyte)(255.0f * r);
	GPU_IMMEDIATE->color[1] = (GLubyte)(255.0f * g);
	GPU_IMMEDIATE->color[2] = (GLubyte)(255.0f * b);
	GPU_IMMEDIATE->color[3] = 255;

	currentColor();
}

void gpuCurrentColor3fv(const GLfloat *restrict v)
{
	GPU_CHECK_CAN_CURRENT();

	GPU_IMMEDIATE->color[0] = (GLubyte)(255.0f * v[0]);
	GPU_IMMEDIATE->color[1] = (GLubyte)(255.0f * v[1]);
	GPU_IMMEDIATE->color[2] = (GLubyte)(255.0f * v[2]);
	GPU_IMMEDIATE->color[3] = 255;

	currentColor();
}

void gpuCurrentColor3ub(GLubyte r, GLubyte g, GLubyte b)
{
	GPU_CHECK_CAN_CURRENT();

	GPU_IMMEDIATE->color[0] = r;
	GPU_IMMEDIATE->color[1] = g;
	GPU_IMMEDIATE->color[2] = b;
	GPU_IMMEDIATE->color[3] = 255;

	currentColor();
}

void gpuCurrentColor3ubv(const GLubyte *restrict v)
{
	GPU_CHECK_CAN_CURRENT();

	GPU_IMMEDIATE->color[0] = v[0];
	GPU_IMMEDIATE->color[1] = v[1];
	GPU_IMMEDIATE->color[2] = v[2];
	GPU_IMMEDIATE->color[3] = 255;

	currentColor();
}

void gpuCurrentColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
	GPU_CHECK_CAN_CURRENT();

	GPU_IMMEDIATE->color[0] = (GLubyte)(255.0f * r);
	GPU_IMMEDIATE->color[1] = (GLubyte)(255.0f * g);
	GPU_IMMEDIATE->color[2] = (GLubyte)(255.0f * b);
	GPU_IMMEDIATE->color[3] = (GLubyte)(255.0f * a);

	currentColor();
}

void gpuCurrentColor4fv(const GLfloat *restrict v)
{
	GPU_CHECK_CAN_CURRENT();

	GPU_IMMEDIATE->color[0] = (GLubyte)(255.0f * v[0]);
	GPU_IMMEDIATE->color[1] = (GLubyte)(255.0f * v[1]);
	GPU_IMMEDIATE->color[2] = (GLubyte)(255.0f * v[2]);
	GPU_IMMEDIATE->color[3] = (GLubyte)(255.0f * v[3]);

	currentColor();
}

void gpuCurrentColor4ub(GLubyte r, GLubyte g, GLubyte b, GLubyte a)
{
	GPU_CHECK_CAN_CURRENT();

	GPU_IMMEDIATE->color[0] = r;
	GPU_IMMEDIATE->color[1] = g;
	GPU_IMMEDIATE->color[2] = b;
	GPU_IMMEDIATE->color[3] = a;

	currentColor();
}

void gpuCurrentColor4ubv(const GLubyte *restrict v)
{
	GPU_CHECK_CAN_CURRENT();

	GPU_IMMEDIATE->color[0] = v[0];
	GPU_IMMEDIATE->color[1] = v[1];
	GPU_IMMEDIATE->color[2] = v[2];
	GPU_IMMEDIATE->color[3] = v[3];

	currentColor();
}

void gpuCurrentColor4d(GLdouble r, GLdouble g, GLdouble b, GLdouble a)
{
	GPU_CHECK_CAN_CURRENT();

	GPU_IMMEDIATE->color[0] = (GLubyte)r;
	GPU_IMMEDIATE->color[1] = (GLubyte)g;
	GPU_IMMEDIATE->color[2] = (GLubyte)b;
	GPU_IMMEDIATE->color[3] = (GLubyte)a;

	currentColor();
}

void gpuCurrentGrey3f(GLfloat luminance)
{
	GLubyte c;

	GPU_CHECK_CAN_CURRENT();

	c = (GLubyte)(255.0 * luminance);

	GPU_IMMEDIATE->color[0] = c;
	GPU_IMMEDIATE->color[1] = c;
	GPU_IMMEDIATE->color[2] = c;
	GPU_IMMEDIATE->color[3] = 255;

	currentColor();
}

void gpuCurrentGrey4f(GLfloat luminance, GLfloat alpha)
{
	GLubyte c;

	GPU_CHECK_CAN_CURRENT();

	c = (GLubyte)(255.0 * luminance);

	GPU_IMMEDIATE->color[0] = c;
	GPU_IMMEDIATE->color[1] = c;
	GPU_IMMEDIATE->color[2] = c;
	GPU_IMMEDIATE->color[3] = (GLubyte)(255.0 * alpha);

	currentColor();
}



/* This function converts a numerical value to the equivalent 24-bit
 * color, while not being endian-sensitive. On little-endians, this
 * is the same as doing a 'naive' indexing, on big-endian, it is not! */
void gpuCurrentColor3x(GLuint rgb)
{
	GPU_CHECK_CAN_CURRENT();

	GPU_IMMEDIATE->color[0] = (rgb >>  0) & 0xFF;
	GPU_IMMEDIATE->color[1] = (rgb >>  8) & 0xFF;
	GPU_IMMEDIATE->color[2] = (rgb >> 16) & 0xFF;
	GPU_IMMEDIATE->color[3] = 255;

	currentColor();
}

void gpuCurrentColor4x(GLuint rgb, GLfloat a)
{
	GPU_CHECK_CAN_CURRENT();

	GPU_IMMEDIATE->color[0] = (rgb >>  0) & 0xFF;
	GPU_IMMEDIATE->color[1] = (rgb >>  8) & 0xFF;
	GPU_IMMEDIATE->color[2] = (rgb >> 16) & 0xFF;
	GPU_IMMEDIATE->color[3] = (GLubyte)(255 * a);

	currentColor();
}


void gpuCurrentAlpha(GLfloat a)
{
	GPU_CHECK_CAN_GET_COLOR();

	GPU_IMMEDIATE->color[3] = (GLubyte)a;

	currentColor();
}

void gpuMultCurrentAlpha(GLfloat factor)
{
	GPU_CHECK_CAN_GET_COLOR();

	GPU_IMMEDIATE->color[3] *= factor;

	currentColor();
}



void gpuGetCurrentColor4fv(GLfloat *restrict color)
{
	GPU_CHECK_CAN_GET_COLOR();

	GPU_IMMEDIATE->getCurrentColor(color);
}

void gpuGetCurrentColor4ubv(GLubyte *restrict color)
{
	GLfloat v[4]; //V-112

	GPU_CHECK_CAN_GET_COLOR();

	GPU_IMMEDIATE->getCurrentColor(v);

	color[0] = (GLubyte)(255.0f * v[0]);
	color[1] = (GLubyte)(255.0f * v[1]);
	color[2] = (GLubyte)(255.0f * v[2]);
	color[3] = (GLubyte)(255.0f * v[3]);
}



BLI_INLINE void currentNormal()
{
	GPU_IMMEDIATE->currentNormal();

#if GPU_SAFETY
	GPU_IMMEDIATE->isNormalValid = GL_TRUE;
#endif
}

void gpuCurrentNormal3fv(const GLfloat *restrict v)
{
	GPU_CHECK_CAN_CURRENT();

	GPU_IMMEDIATE->normal[0] = v[0];
	GPU_IMMEDIATE->normal[1] = v[1];
	GPU_IMMEDIATE->normal[2] = v[2];

	currentNormal();
}



static GPUimmediate* immediateStack = NULL; /* stack size of one */



void gpuPushImmediate(void)
{
	GPUimmediate* newImmediate;

	GPU_CHECK_CAN_PUSH();

	newImmediate   = gpuNewImmediate();
	immediateStack = GPU_IMMEDIATE;
	GPU_IMMEDIATE  = newImmediate;
}

GPUimmediate* gpuPopImmediate(void)
{
	GPUimmediate* newImmediate;

	GPU_CHECK_CAN_POP();

	newImmediate = GPU_IMMEDIATE;
	GPU_IMMEDIATE = immediateStack;
	immediateStack = NULL;

	return newImmediate;
}



static void gpu_append_client_arrays(
	const GPUarrays *restrict arrays,
	GLint first,
	GLsizei count)
{
	GLsizei i;
	size_t size;
	size_t offset;
	char *restrict buffer;

	char *restrict colorPointer;
	char *restrict normalPointer;
	char *restrict vertexPointer;

#if GPU_SAFETY
		{
		int newVertexCountOK;
		GPU_SAFE_RETURN(GPU_IMMEDIATE->count + count <= GPU_IMMEDIATE->maxVertexCount, newVertexCountOK,);
		}
#endif

	vertexPointer = (char *restrict)(arrays->vertexPointer) + (first * arrays->vertexStride);
	normalPointer = (char *restrict)(arrays->normalPointer) + (first * arrays->normalStride);
	colorPointer  = (char *restrict)(arrays->colorPointer)  + (first * arrays->colorStride);

	buffer   = GPU_IMMEDIATE->buffer;
	offset   = GPU_IMMEDIATE->offset;

	for (i = 0; i < count; i++) {
		size = arrays->vertexSize * sizeof(GLfloat);
		memcpy(buffer + offset, vertexPointer, size);
		offset += size;
		vertexPointer += arrays->vertexStride;

		if (normalPointer) {
			memcpy(buffer + offset, normalPointer, 3*sizeof(GLfloat));
			offset += 3*sizeof(GLfloat);
			normalPointer += arrays->normalStride;
		}

		if (colorPointer) {
			if (arrays->colorType == GL_FLOAT) {
				GLubyte color[4];
				GLfloat *floatPointer = (float*)colorPointer;

				color[0] = (GLubyte)(colorPointer[0] * 255.0f);
				color[1] = (GLubyte)(colorPointer[1] * 255.0f);
				color[2] = (GLubyte)(colorPointer[2] * 255.0f);

				if (arrays->colorSize == 4) {
					color[3] = (GLubyte)(colorPointer[3] * 255.0f);
				}
				else {
					color[3] = 255;;
				}

				memcpy(buffer + offset, color, 4);
			}
			else /* assume four GL_UNSIGNED_BYTE */ {
				memcpy(buffer + offset, colorPointer, 4);
			}

			offset += 4;
			colorPointer += arrays->colorStride;
		}
	}

	GPU_IMMEDIATE->offset = offset;
	GPU_IMMEDIATE->count  += count;
}



const GPUarrays GPU_ARRAYS_V2F = {
	0,    /* GLenum colorType;    */
	0,    /* GLint  colorSize;    */
	0,    /* GLint  colorStride;  */
	NULL, /* void*  colorPointer; */

	0,    /* GLenum normalType;    */
	0,    /* GLint  normalStride;  */
	NULL, /* void*  normalPointer; */

	GL_FLOAT,            /* GLenum vertexType;    */
	2,                   /* GLint  vertexSize;    */
	2 * sizeof(GLfloat), /* GLint  vertexStride;  */
	NULL,                /* void*  vertexPointer; */
};

const GPUarrays GPU_ARRAYS_C4UB_V2F = {
	GL_UNSIGNED_BYTE, /* GLenum colorType;    */
	4,                /* GLint  colorSize;    */
	4,                /* GLint  colorStride;  */
	NULL,             /* void*  colorPointer; */

	0,    /* GLenum normalType;    */
	0,    /* GLint  normalStride;  */
	NULL, /* void*  normalPointer; */

	GL_FLOAT,            /* GLenum vertexType;    */
	2,                   /* GLint  vertexSize;    */
	2 * sizeof(GLfloat), /* GLint  vertexStride;  */
	NULL,                /* void*  vertexPointer; */
};

const GPUarrays GPU_ARRAYS_V3F = {
	0,    /* GLenum colorType;    */
	0,    /* GLint  colorSize;    */
	0,    /* GLint  colorStride;  */
	NULL, /* void*  colorPointer; */

	0,    /* GLenum normalType;    */
	0,    /* GLint  normalStride;  */
	NULL, /* void*  normalPointer; */

	GL_FLOAT,            /* GLenum vertexType;    */
	3,                   /* GLint  vertexSize;    */
	3 * sizeof(GLfloat), /* GLint  vertexStride;  */
	NULL,                /* void*  vertexPointer; */
};

const GPUarrays GPU_ARRAYS_C3F_V3F = {
	GL_FLOAT,            /* GLenum colorType;    */
	3,                   /* GLint  colorSize;    */
	3 * sizeof(GLfloat), /* GLint  colorStride;  */
	NULL,                /* void*  colorPointer; */

	0,    /* GLenum normalType;    */
	0,    /* GLint  normalStride;  */
	NULL, /* void*  normalPointer; */

	GL_FLOAT,            /* GLenum vertexType;    */
	3,                   /* GLint  vertexSize;    */
	3 * sizeof(GLfloat), /* GLint  vertexStride;  */
	NULL,                /* void*  vertexPointer; */
};

const GPUarrays GPU_ARRAYS_C4F_V3F = {
	GL_FLOAT,            /* GLenum colorType;    */
	4,                   /* GLint  colorSize;    */
	4 * sizeof(GLfloat), /* GLint  colorStride;  */
	NULL,                /* void*  colorPointer; */

	0,    /* GLenum normalType;    */
	0,    /* GLint  normalStride;  */
	NULL, /* void*  normalPointer; */

	GL_FLOAT,            /* GLenum vertexType;    */
	3,                   /* GLint  vertexSize;    */
	3 * sizeof(GLfloat), /* GLint  vertexStride;  */
	NULL,                /* void*  vertexPointer; */
};

const GPUarrays GPU_ARRAYS_N3F_V3F = {
	0,    /* GLenum colorType;    */
	0,    /* GLint  colorSize;    */
	0,    /* GLint  colorStride;  */
	NULL, /* void*  colorPointer; */

	GL_FLOAT,            /* GLenum normalType;    */
	3 * sizeof(GLfloat), /* GLint  normalStride;  */
	NULL,                /* void*  normalPointer; */

	GL_FLOAT,            /* GLenum vertexType;    */
	3,                   /* GLint  vertexSize;    */
	3 * sizeof(GLfloat), /* GLint  vertexStride;  */
	NULL,                /* void*  vertexPointer; */
};

const GPUarrays GPU_ARRAYS_C3F_N3F_V3F = {
	GL_FLOAT,            /* GLenum colorType;    */
	3,                   /* GLint  colorSize;    */
	3 * sizeof(GLfloat), /* GLint  colorStride;  */
	NULL,                /* void*  colorPointer; */

	GL_FLOAT,            /* GLenum normalType;    */
	3 * sizeof(GLfloat), /* GLint  normalStride;  */
	NULL,                /* void*  normalPointer; */

	GL_FLOAT,            /* GLenum vertexType;    */
	3,                   /* GLint  vertexSize;    */
	3 * sizeof(GLfloat), /* GLint  vertexStride;  */
	NULL,                /* void*  vertexPointer; */
};



void gpuAppendClientArrays(
	const GPUarrays* arrays,
	GLint first,
	GLsizei count)
{
	GPU_IMMEDIATE->appendClientArrays(arrays, first, count);
}



void gpuDrawClientArrays(
	GLenum mode,
	const GPUarrays *arrays,
	GLint first,
	GLsizei count)
{
	gpuBegin(mode);
	gpuAppendClientArrays(arrays, first, count);
	gpuEnd();
}



void gpuSingleClientArrays_V2F(
	GLenum mode,
	const void *restrict vertexPointer,
	GLint vertexStride,
	GLint first,
	GLsizei count)
{
	GPUarrays arrays = GPU_ARRAYS_V2F;

	arrays.vertexPointer = vertexPointer;
	arrays.vertexStride  = vertexStride != 0 ? vertexStride : 2*sizeof(GLfloat);

	gpuImmediateFormat_V2();
	gpuDrawClientArrays(mode, &arrays, first, count);
	gpuImmediateUnformat();
}

void gpuSingleClientArrays_V3F(
	GLenum mode,
	const void *restrict vertexPointer,
	GLint vertexStride,
	GLint first,
	GLsizei count)
{
	GPUarrays arrays = GPU_ARRAYS_V3F;

	arrays.vertexPointer = vertexPointer;
	arrays.vertexStride  = vertexStride != 0 ? vertexStride : 3*sizeof(GLfloat);

	gpuImmediateFormat_V3();
	gpuDrawClientArrays(mode, &arrays, first, count);
	gpuImmediateUnformat();
}

void gpuSingleClientArrays_C3F_V3F(
	GLenum mode,
	const void *restrict colorPointer,
	GLint colorStride,
	const void *restrict vertexPointer,
	GLint vertexStride,
	GLint first,
	GLsizei count)
{
	GPUarrays arrays = GPU_ARRAYS_C3F_V3F;

	arrays.colorPointer = colorPointer;
	arrays.colorStride  = colorStride != 0 ? colorStride : 3*sizeof(GLfloat);

	arrays.vertexPointer = vertexPointer;
	arrays.vertexStride  = vertexStride != 0 ? vertexStride : 3*sizeof(GLfloat);

	gpuImmediateFormat_C4_V3();
	gpuDrawClientArrays(mode, &arrays, first, count);
	gpuImmediateUnformat();
}

void gpuSingleClientArrays_C4F_V3F(
	GLenum mode,
	const void *restrict colorPointer,
	GLint colorStride,
	const void *restrict vertexPointer,
	GLint vertexStride,
	GLint first,
	GLsizei count)
{
	GPUarrays arrays = GPU_ARRAYS_C4F_V3F;

	arrays.colorPointer = colorPointer;
	arrays.colorStride  = colorStride != 0 ? colorStride : 4*sizeof(GLfloat);

	arrays.vertexPointer = vertexPointer;
	arrays.vertexStride  = vertexStride != 0 ? vertexStride : 3*sizeof(GLfloat);

	gpuImmediateFormat_C4_V3();
	gpuDrawClientArrays(mode, &arrays, first, count);
	gpuImmediateUnformat();
}

void gpuSingleClientArrays_N3F_V3F(
	GLenum mode,
	const void *restrict normalPointer,
	GLint normalStride,
	const void *restrict vertexPointer,
	GLint vertexStride,
	GLint first,
	GLsizei count)
{
	GPUarrays arrays = GPU_ARRAYS_N3F_V3F;

	arrays.normalPointer = normalPointer;
	arrays.normalStride  = normalStride != 0 ? normalStride : 3*sizeof(GLfloat);

	arrays.vertexPointer = vertexPointer;
	arrays.vertexStride  = vertexStride != 0 ? vertexStride : 3*sizeof(GLfloat);

	gpuImmediateFormat_N3_V3();
	gpuDrawClientArrays(mode, &arrays, first, count);
	gpuImmediateUnformat();
}

void gpuSingleClientArrays_C3F_N3F_V3F(
	GLenum mode,
	const void *restrict colorPointer,
	GLint colorStride,
	const void *restrict normalPointer,
	GLint normalStride,
	const void *restrict vertexPointer,
	GLint vertexStride,
	GLint first,
	GLsizei count)
{
	GPUarrays arrays = GPU_ARRAYS_C3F_N3F_V3F;

	arrays.colorPointer = colorPointer;
	arrays.colorStride  = colorStride != 0 ? colorStride : 3*sizeof(GLfloat);

	arrays.normalPointer = normalPointer;
	arrays.normalStride  = normalStride != 0 ? normalStride : 3*sizeof(GLfloat);

	arrays.vertexPointer = vertexPointer;
	arrays.vertexStride  = vertexStride != 0 ? vertexStride : 3*sizeof(GLfloat);

	gpuImmediateFormat_C4_N3_V3();
	gpuDrawClientArrays(mode, &arrays, first, count);
	gpuImmediateUnformat();
}

void gpuSingleClientArrays_C4UB_V2F(
	GLenum mode,
	const void *restrict colorPointer,
	GLint colorStride,
	const void *restrict vertexPointer,
	GLint vertexStride,
	GLint first,
	GLsizei count)
{
	GPUarrays arrays = GPU_ARRAYS_C4UB_V2F;

	arrays.colorPointer = colorPointer;
	arrays.colorStride  = colorStride != 0 ? colorStride : 4;

	arrays.vertexPointer = vertexPointer;
	arrays.vertexStride  = vertexStride != 0 ? vertexStride : 2*sizeof(GLfloat);

	gpuImmediateFormat_C4_V2();
	gpuDrawClientArrays(mode, &arrays, first, count);
	gpuImmediateUnformat();
}



void gpuImmediateIndexRange(GLuint indexMin, GLuint indexMax)
{
	GPU_IMMEDIATE->index->indexMin = indexMin;
	GPU_IMMEDIATE->index->indexMax = indexMax;
}

static void find_range(
	GLuint *restrict arrayFirst,
	GLuint *restrict arrayLast,
	GLsizei count,
	const GLuint *restrict indexes)
{
	int i;

	GPU_ASSERT(count > 0);

	*arrayFirst = indexes[0];
	*arrayLast  = indexes[0];

	for (i = 1; i < count; i++) {
		if (indexes[i] < *arrayFirst) {
			*arrayFirst = indexes[i];
		}
		else if (indexes[i] > *arrayLast) {
			*arrayLast = indexes[i];
		}
	}
}

void gpuImmediateIndexComputeRange(void)
{
	GLuint indexMin, indexMax;

	find_range(&indexMin, &indexMax, GPU_IMMEDIATE->index->count, GPU_IMMEDIATE->index->buffer);
	gpuImmediateIndexRange(indexMin, indexMax);
}



void gpuSingleClientElements_V3F(
	GLenum mode,
	const void *restrict vertexPointer,
	GLint vertexStride,
	GLsizei count,
	const void *restrict indexes)
{
	GLuint indexMin, indexMax;

	find_range(&indexMin, &indexMax, count, indexes);

	gpuSingleClientRangeElements_V3F(
		mode,
		vertexPointer,
		vertexStride,
		indexMin,
		indexMax,
		count,
		indexes);
}

void gpuSingleClientElements_N3F_V3F(
	GLenum mode,
	const void *restrict normalPointer,
	GLint normalStride,
	const void *restrict vertexPointer,
	GLint vertexStride,
	GLsizei count,
	void *restrict indexes)
{
	GLuint indexMin, indexMax;

	find_range(&indexMin, &indexMax, count, indexes);

	gpuSingleClientRangeElements_N3F_V3F(
		mode,
		normalPointer,
		normalStride,
		vertexPointer,
		vertexStride,
		indexMin,
		indexMax,
		count,
		indexes);
}



void gpuDrawClientRangeElements(
	GLenum mode,
	const GPUarrays *restrict arrays,
	GLuint indexMin,
	GLuint indexMax,
	GLsizei count,
	const void *restrict indexes)
{
	GLuint indexRange = indexMax - indexMin + 1;

	gpuBegin(GL_NOOP);
	gpuAppendClientArrays(arrays, indexMin, indexRange);
	gpuEnd();

	gpuIndexBegin();
	gpuIndexRelativev(indexRange + indexMin, count, indexes);
	gpuIndexEnd();

	gpuImmediateIndexRange(indexMin, indexMax);
	gpuDrawRangeElements(mode);
}

void gpuSingleClientRangeElements_V3F(
	GLenum mode,
	const void *restrict vertexPointer,
	GLint vertexStride,
	GLuint indexMin,
	GLuint indexMax,
	GLsizei count,
	const void *restrict indexes)
{
	GPUarrays arrays = GPU_ARRAYS_V3F;

	arrays.vertexPointer = vertexPointer;
	arrays.vertexStride  = vertexStride != 0 ? vertexStride : 3*sizeof(GLfloat);

	gpuImmediateFormat_V3();
	gpuDrawClientRangeElements(mode, &arrays, indexMin, indexMax, count, indexes);
	gpuImmediateUnformat();
}

void gpuSingleClientRangeElements_N3F_V3F(
	GLenum mode,
	const void *restrict normalPointer,
	GLint normalStride,
	const void *restrict vertexPointer,
	GLint vertexStride,
	GLuint indexMin,
	GLuint indexMax,
	GLsizei count,
	const GLvoid *restrict indexes)
{
	GPUarrays arrays = GPU_ARRAYS_N3F_V3F;

	arrays.normalPointer = normalPointer;
	arrays.normalStride  = normalStride != 0 ? normalStride : 3*sizeof(GLfloat);

	arrays.vertexPointer = vertexPointer;
	arrays.vertexStride  = vertexStride != 0 ? vertexStride : 3*sizeof(GLfloat);

	gpuImmediateFormat_N3_V3();
	gpuDrawClientRangeElements(mode, &arrays, indexMin, indexMax, count, indexes);
	gpuImmediateUnformat();
}



GPUindex* gpuNewIndex(void)
{
	GPUindex* index;

	index = MEM_callocN(sizeof(GPUindex), "GPUindex");

	return index;
}

void gpuDeleteIndex(GPUindex *restrict index)
{
	if (index) {
		GPUimmediate* immediate = index->immediate;

		immediate->indexShutdownBuffer(index);
		immediate->index = NULL;

		MEM_freeN(index);
	}
}

void gpuImmediateIndex(GPUindex * index)
{
	GPU_ASSERT(GPU_IMMEDIATE->index == NULL);

	if (index) {
		index->immediate = GPU_IMMEDIATE;
	}

	GPU_IMMEDIATE->index = index;
}

void gpuImmediateMaxIndexCount(GLsizei maxIndexCount)
{
	GPU_IMMEDIATE->index->maxIndexCount = maxIndexCount;
}

void gpuIndexBegin(void)
{
	GPUindex* index;

	index = GPU_IMMEDIATE->index;

	index->count    = 0;
	index->indexMin = 0;
	index->indexMax = 0;

	GPU_IMMEDIATE->indexBeginBuffer();
}

void gpuIndexRelativev(GLint offset, GLsizei count, const void *restrict indexes)
{
	int i;
	int start;
	int indexStart;
	GPUindex* index;

	{
		GLboolean indexCountOK;
		GPU_SAFE_RETURN(GPU_IMMEDIATE->index->count + count <= GPU_IMMEDIATE->index->maxIndexCount, indexCountOK,);
	}

	start = GPU_IMMEDIATE->count;
	index = GPU_IMMEDIATE->index;
	indexStart = index->count;

	for (i = 0; i < count; i++) {
		index->buffer[indexStart+i] = start - offset + ((GLuint*)indexes)[i];
	}

	index->count += count;
}

void gpuIndex(GLuint nextIndex)
{
	GPUindex* index;

	{
		GLboolean indexCountOK;
		GPU_SAFE_RETURN(GPU_IMMEDIATE->index->count < GPU_IMMEDIATE->index->maxIndexCount, indexCountOK,);
	}

	index = GPU_IMMEDIATE->index;
	index->buffer[index->count] = nextIndex;
	index->count++;
}

void gpuIndexEnd(void)
{
	GPU_IMMEDIATE->indexEndBuffer();

	GPU_IMMEDIATE->index->buffer = NULL;
}



const char* gpuErrorString(GLenum err)
{
	switch(err) {
		case GL_NO_ERROR:
			return "No Error\n";

		case GLU_INVALID_ENUM:
		case GL_INVALID_ENUM:
			return "Invalid Enum\n";

		case GLU_INVALID_VALUE:
		case GL_INVALID_VALUE:
			return "Invalid Value\n";

		case GL_INVALID_OPERATION:
			return "Invalid Operation\n";

		case GL_STACK_OVERFLOW:
			return "Stack Overflow\n";

		case GL_STACK_UNDERFLOW:
			return "Stack Underflow\n";

		case GLU_OUT_OF_MEMORY:
		case GL_OUT_OF_MEMORY:
			return "Out of Memory\n";

		case GL_TABLE_TOO_LARGE:
			return "Table Too Large\n";

		default:
			return "Unknown Error\n";
	}
}

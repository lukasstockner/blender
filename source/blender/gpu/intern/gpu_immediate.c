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

/* my interface */
#include "intern/gpu_immediate.h"

/* my library */
#include "GPU_extensions.h"

/* internal */
#include "intern/gpu_immediate_gl.h"
#include "intern/gpu_profile.h"

/* external */
#include "MEM_guardedalloc.h"

/* standard */
#include <string.h>



#if GPU_SAFETY

/* Define some useful, but potentially slow, checks for correct API usage. */

/* Each block contains variables that can be inspected by a
   debugger in the event that a break point is triggered. */

#define GPU_CHECK_CAN_SETUP()     \
    {                             \
    GLboolean immediateOK;        \
    GLboolean noLockOK;           \
    GLboolean noBeginOK;          \
    GPU_CHECK_BASE(immediateOK);  \
    GPU_CHECK_NO_LOCK(noLockOK)   \
    GPU_CHECK_NO_BEGIN(noBeginOK) \
    }

#define GPU_CHECK_CAN_PUSH()                                          \
    {                                                                 \
    GLboolean immediateStackOK;                                       \
    GLboolean noLockOK;                                               \
    GLboolean noBeginOK;                                              \
    GPU_SAFE_RETURN(immediateStack == NULL, immediateStackOK,);       \
    GPU_SAFE_RETURN(GPU_IMMEDIATE->mappedBuffer == NULL, noLockOK,);  \
    GPU_SAFE_RETURN(GPU_IMMEDIATE->lockCount == 0, noBeginOK,);       \
    }

#define GPU_CHECK_CAN_POP()                                                \
    {                                                                      \
    GLboolean immediateOK;                                                 \
    GLboolean noLockOK;                                                    \
    GLboolean noBeginOK;                                                   \
    GPU_SAFE_RETURN(GPU_IMMEDIATE != NULL, immediateOK, NULL);             \
    GPU_SAFE_RETURN(GPU_IMMEDIATE->mappedBuffer == NULL, noLockOK, NULL);  \
    GPU_SAFE_RETURN(GPU_IMMEDIATE->lockCount == 0, noBeginOK, NULL);       \
    }

#define GPU_CHECK_CAN_LOCK()       \
    {                              \
    GLboolean immediateOK;         \
    GLboolean noBeginOK;           \
    GLboolean noLockOK;            \
    GPU_CHECK_BASE(immediateOK);   \
    GPU_CHECK_NO_BEGIN(noBeginOK); \
    GPU_CHECK_NO_LOCK(noLockOK);   \
    }

#define GPU_CHECK_CAN_UNLOCK()      \
    {                               \
    GLboolean immediateOK;          \
    GLboolean isLockedOK;           \
    GLboolean noBeginOK;            \
    GPU_CHECK_BASE(immediateOK);    \
    GPU_CHECK_IS_LOCKED(isLockedOK) \
    GPU_CHECK_NO_BEGIN(noBeginOK)   \
    }

#define GPU_SAFE_STMT(var, test, stmt) \
    var = (GLboolean)(test);           \
    GPU_ASSERT((#test, var));          \
    if (var) {                         \
        stmt;                          \
    }

#else

#define GPU_CHECK_CAN_SETUP()
#define GPU_CHECK_CAN_PUSH()
#define GPU_CHECK_CAN_POP()
#define GPU_CHECK_CAN_LOCK()
#define GPU_CHECK_CAN_UNLOCK()

#define GPU_CHECK_CAN_CURRENT()
#define GPU_CHECK_CAN_GET_COLOR()
#define GPU_CHECK_CAN_GET_NORMAL()

#define GPU_SAFE_STMT(var, test, stmt) { (void)(var); stmt; }

#endif



/* global symbol needed because the immediate drawing functons are inline */
GPUimmediate *restrict GPU_IMMEDIATE = NULL;



void gpuBegin(GLenum mode)
{
	int primMod;
	int primOff;

	GPU_CHECK_CAN_BEGIN();

#if GPU_SAFETY
	GPU_IMMEDIATE->hasOverflowed = GL_FALSE;
#endif

	GPU_IMMEDIATE->mode   = mode;
	GPU_IMMEDIATE->offset = 0;
	GPU_IMMEDIATE->count  = 0;

	switch (mode) {
		case GL_LINES:
			primMod = 2;
			primOff = 0;
			break;

		case GL_QUAD_STRIP:
		case GL_TRIANGLE_STRIP:
			primMod = 2;
			primOff = 2;
			break;

		case GL_TRIANGLES:
			primMod = 3;
			primOff = 0;
			break;

		case GL_QUADS:
			primMod = 4;
			primOff = 2;
			break;

		default:
			primMod = 1;
			primOff = 0;
			break;
	}

	GPU_IMMEDIATE->lastPrimVertex = GPU_IMMEDIATE->maxVertexCount - (GPU_IMMEDIATE->maxVertexCount % primMod);

	gpu_begin_buffer_gl();
}



void gpuEnd(void)
{
	GPU_CHECK_CAN_END();
	GPU_ASSERT(GPU_IMMEDIATE->mode != GL_NOOP || !(GPU_IMMEDIATE->hasOverflowed));

	gpu_end_buffer_gl();

	GPU_IMMEDIATE->mappedBuffer = NULL;
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

	if (GPU_IMMEDIATE->lockCount == 0)
		gpu_lock_buffer_gl();

	GPU_IMMEDIATE->lockCount++;
}

void gpuImmediateUnlock(void)
{
	GPU_CHECK_CAN_UNLOCK();

	GPU_IMMEDIATE->lockCount--;

	if (GPU_IMMEDIATE->lockCount == 0)
		gpu_unlock_buffer_gl();
}



GLint gpuImmediateLockCount(void)
{
	GPU_ASSERT(GPU_IMMEDIATE);

	if (!GPU_IMMEDIATE) {
		return GL_FALSE;
	}

	return GPU_IMMEDIATE->lockCount;
}



GPUimmediate* gpuNewImmediate(void)
{
	GPUimmediate* immediate =
		(GPUimmediate*)MEM_callocN(sizeof(GPUimmediate), "GPUimmediate");

#if GPU_SAFETY
	immediate->lastTexture = GPU_max_textures() - 1;
#endif

	return immediate;
}



void gpuImmediateMakeCurrent(GPUimmediate *restrict immediate)
{
	GPU_IMMEDIATE = immediate;
}



void gpuDeleteImmediate(GPUimmediate *restrict immediate)
{
	if (!immediate)
		return;

	if (GPU_IMMEDIATE == immediate)
		gpuImmediateMakeCurrent(NULL);

	gpu_shutdown_buffer_gl(immediate);

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
		vertexSize > 0 && vertexSize <= 4,
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



void gpuImmediateTexCoordCount(size_t count)
{
	GLboolean countOK;

	GPU_CHECK_CAN_SETUP();
	
	GPU_SAFE_STMT(
		countOK,
		count <= GPU_MAX_COMMON_TEXCOORDS,
		GPU_IMMEDIATE->format.texCoordCount = count);
}



void gpuImmediateTexCoordSizes(const GLint *restrict sizes)
{
	size_t i;

	GPU_CHECK_CAN_SETUP();

	for (i = 0; i < GPU_IMMEDIATE->format.texCoordCount; i++) {
		GLboolean texCoordSizeOK;

		GPU_SAFE_STMT(
			texCoordSizeOK,
			sizes[i] > 0 && sizes[i] <= 4,
			GPU_IMMEDIATE->format.texCoordSize[i] = sizes[i]);
	}
}



void gpuImmediateSamplerCount(size_t count)
{
	GLboolean countOK;

	GPU_CHECK_CAN_SETUP();
	
	GPU_SAFE_STMT(
		countOK,
		count <= GPU_MAX_COMMON_SAMPLERS,
		GPU_IMMEDIATE->format.samplerCount = count);
}



void gpuImmediateSamplerMap(const GLint *restrict map)
{
	size_t i;

	GPU_CHECK_CAN_SETUP();

	for (i = 0; i < GPU_IMMEDIATE->format.samplerCount; i++) {
		GLboolean mapOK;

		GPU_SAFE_STMT(
			mapOK,
			map[i] >= 0 &&  map[i] <= GPU_IMMEDIATE->lastTexture,
			GPU_IMMEDIATE->format.samplerMap[i] = map[i]);
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
			sizes[i] > 0 && sizes[i] <= 4,
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

	if (!ELEM6(
			GPU_IMMEDIATE->mode,
			GL_NOOP,
			GL_LINE_LOOP,
			GL_POLYGON,
			GL_QUAD_STRIP,
			GL_LINE_STRIP,
			GL_TRIANGLE_STRIP)) // XXX jwilkins: can restart some of these, but need to put in the logic (could be problematic with mapped VBOs?)
	{
		gpu_end_buffer_gl();

		GPU_IMMEDIATE->mappedBuffer = NULL;
		GPU_IMMEDIATE->offset       = 0;
		GPU_IMMEDIATE->count        = 1; /* count the vertex that triggered this */

		gpu_begin_buffer_gl();

		return GL_TRUE;
	}
	else {
		return GL_FALSE;
	}
}



void gpu_copy_vertex(void)
{
	size_t i;
	size_t size;
	size_t offset;
	GLubyte *restrict mappedBuffer;

#if GPU_SAFETY
	{
	int maxVertexCountOK;
	GPU_SAFE_RETURN(GPU_IMMEDIATE->maxVertexCount != 0, maxVertexCountOK,);
	}
#endif

	if (GPU_IMMEDIATE->count == GPU_IMMEDIATE->lastPrimVertex) {
		GLboolean restarted;

		restarted = end_begin(); /* draw and clear buffer */

		GPU_ASSERT(restarted);

		if (!restarted)
			return;
	}
	else {
		GPU_IMMEDIATE->count++;
	}

	mappedBuffer = GPU_IMMEDIATE->mappedBuffer;
	offset = GPU_IMMEDIATE->offset;

	/* vertex */

	size = (size_t)(GPU_IMMEDIATE->format.vertexSize) * sizeof(GLfloat);
	memcpy(mappedBuffer + offset, GPU_IMMEDIATE->vertex, size);
	offset += size;

	/* normal */

	if (GPU_IMMEDIATE->format.normalSize != 0) {
		/* normals are always have 3 components */
		memcpy(mappedBuffer + offset, GPU_IMMEDIATE->normal, 3*sizeof(GLfloat));
		offset += 3*sizeof(GLfloat);
	}

	/* color */

	if (GPU_IMMEDIATE->format.colorSize != 0) {
		/* 4 bytes are always reserved for color, for efficient memory alignment */
		memcpy(mappedBuffer + offset, GPU_IMMEDIATE->color, 4*sizeof(GLubyte));
		offset += 4*sizeof(GLubyte);
	}

	/* texture coordinate(s) */

	for (i = 0; i < GPU_IMMEDIATE->format.texCoordCount; i++) {
		size = (size_t)(GPU_IMMEDIATE->format.texCoordSize[i]) * sizeof(GLfloat);
		memcpy(mappedBuffer + offset, GPU_IMMEDIATE->texCoord[i], size);
		offset += size;
	}

	/* float vertex attribute(s) */

	for (i = 0; i < GPU_IMMEDIATE->format.attribCount_f; i++) {
		size = (size_t)(GPU_IMMEDIATE->format.attribSize_f[i]) * sizeof(GLfloat);
		memcpy(mappedBuffer + offset, GPU_IMMEDIATE->attrib_f[i], size);
		offset += size;
	}

	/* unsigned byte vertex attribute(s) */

	for (i = 0; i < GPU_IMMEDIATE->format.attribCount_ub; i++) {
		/* 4 bytes are always reserved for byte attributes, for efficient memory alignment */
		memcpy(mappedBuffer + offset, GPU_IMMEDIATE->attrib_ub[i], 4*sizeof(GLubyte));
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
	//printf("%s(%d): gpuImmediateFormat_V2\n", file, line);
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
	//printf("%s(%d): gpuImmediateFormat_C4_V2\n", file, line);
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
	//printf("%s(%d): gpuImmediateFormat_T2_V2\n", file, line);
#endif

	if (gpuImmediateLockCount() == 0) {
		GLint texCoordSizes[1] = { 2 };
		GLint samplerMap   [1] = { 0 };

		gpuImmediateFormatReset();

		gpuImmediateElementSizes(2, 0, 0);

		gpuImmediateTexCoordCount(1);
		gpuImmediateTexCoordSizes(texCoordSizes);

		gpuImmediateSamplerCount(1);
		gpuImmediateSamplerMap(samplerMap);
	}

	gpuImmediateLock();
}

#if GPU_SAFETY
void gpuSafetyImmediateFormat_T2_V3(const char* file, int line)
#else
void gpuImmediateFormat_T2_V3(void)
#endif
{
#if GPU_SAFETY
	//printf("%s(%d): gpuImmediateFormat_T2_V3\n", file, line);
#endif

	if (gpuImmediateLockCount() == 0) {
		GLint texCoordSizes[1] = { 2 };
		GLint samplerMap   [1] = { 0 };

		gpuImmediateFormatReset();

		gpuImmediateElementSizes(3, 0, 0);

		gpuImmediateTexCoordCount(1);
		gpuImmediateTexCoordSizes(texCoordSizes);

		gpuImmediateSamplerCount(1);
		gpuImmediateSamplerMap(samplerMap);
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
	//printf("%s(%d): gpuImmediateFormat_T2_C4_V2\n", file, line);
#endif

	if (gpuImmediateLockCount() == 0) {
		GLint texCoordSizes[1] = { 2 };
		GLint samplerMap   [1] = { 0 };

		gpuImmediateFormatReset();

		gpuImmediateElementSizes(2, 0, 4); //-V112

		gpuImmediateTexCoordCount(1);
		gpuImmediateTexCoordSizes(texCoordSizes);

		gpuImmediateSamplerCount(1);
		gpuImmediateSamplerMap(samplerMap);
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
	//printf("%s(%d): gpuImmediateFormat_V3\n", file, line);
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
	//printf("%s(%d): gpuImmediateFormat_N3_V3\n", file, line);
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
	//printf("%s(%d): gpuImmediateFormat_C4_V3\n", file, line);
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
	//printf("%s(%d): gpuImmediateFormat_C4_N3_V3\n", file, line);
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
	//printf("%s(%d): gpuImmediateFormat_T2_C4_N3_V3\n", file, line);
#endif

	if (gpuImmediateLockCount() == 0) {
		GLint texCoordSizes[1] = { 2 };
		GLint samplerMap   [1] = { 0 };

		gpuImmediateFormatReset();

		gpuImmediateElementSizes(3, 3, 4); //-V112

		gpuImmediateTexCoordCount(1);
		gpuImmediateTexCoordSizes(texCoordSizes);

		gpuImmediateSamplerCount(1);
		gpuImmediateSamplerMap(samplerMap);
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
	//printf("%s(%d): gpuImmediateFormat_T3_C4_V3\n", file, line);
#endif

	if (gpuImmediateLockCount() == 0) {
		GLint texCoordSizes[1] = { 3 };
		GLint samplerMap   [1] = { 0 };

		gpuImmediateFormatReset();

		gpuImmediateElementSizes(3, 0, 4); //-V112

		gpuImmediateTexCoordCount(1);
		gpuImmediateTexCoordSizes(texCoordSizes);

		gpuImmediateSamplerCount(1);
		gpuImmediateSamplerMap(samplerMap);
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
	//printf("%s(%d): gpuImmediateUnformat\n", file, line);
#endif

	gpuImmediateUnlock();
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
	char *restrict mappedBuffer;

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
	colorPointer  = (char *restrict)(arrays->colorPointer ) + (first * arrays->colorStride );

	mappedBuffer   = GPU_IMMEDIATE->mappedBuffer;
	offset   = GPU_IMMEDIATE->offset;

	for (i = 0; i < count; i++) {
		size = arrays->vertexSize * sizeof(GLfloat);
		memcpy(mappedBuffer + offset, vertexPointer, size);
		offset += size;
		vertexPointer += arrays->vertexStride;

		if (normalPointer) {
			memcpy(mappedBuffer + offset, normalPointer, 3*sizeof(GLfloat));
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

				memcpy(mappedBuffer + offset, color, 4);
			}
			else /* assume four GL_UNSIGNED_BYTE */ {
				memcpy(mappedBuffer + offset, colorPointer, 4);
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

const GPUarrays GPU_ARRAYS_C4UB_V3F = {
	GL_UNSIGNED_BYTE, /* GLenum colorType;    */
	4,                /* GLint  colorSize;    */
	4,                /* GLint  colorStride;  */
	NULL,             /* void*  colorPointer; */

	0,    /* GLenum normalType;    */
	0,    /* GLint  normalStride;  */
	NULL, /* void*  normalPointer; */

	GL_FLOAT,            /* GLenum vertexType;    */
	3,                   /* GLint  vertexSize;    */
	3 * sizeof(GLfloat), /* GLint  vertexStride;  */
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
	gpu_append_client_arrays(arrays, first, count);
}



void gpuDrawClientArrays(
	GLenum mode,
	const GPUarrays *arrays,
	GLint first,
	GLsizei count)
{
	gpuBegin(mode);
	gpu_append_client_arrays(arrays, first, count);
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



void gpuSingleClientArrays_C4UB_V3F(
	GLenum mode,
	const void *restrict colorPointer,
	GLint colorStride,
	const void *restrict vertexPointer,
	GLint vertexStride,
	GLint first,
	GLsizei count)
{
	GPUarrays arrays = GPU_ARRAYS_C4UB_V3F;

	arrays.colorPointer = colorPointer;
	arrays.colorStride  = colorStride != 0 ? colorStride : 4;

	arrays.vertexPointer = vertexPointer;
	arrays.vertexStride  = vertexStride != 0 ? vertexStride : 3*sizeof(GLfloat);

	gpuImmediateFormat_C4_V3();
	gpuDrawClientArrays(mode, &arrays, first, count);
	gpuImmediateUnformat();
}



void gpuImmediateIndexRange(GLuint indexMin, GLuint indexMax)
{
	GPU_IMMEDIATE->index->indexMin = indexMin;
	GPU_IMMEDIATE->index->indexMax = indexMax;
}



#define FIND_RANGE(suffix, ctype)                     \
	static void find_range_##suffix(                  \
    GLuint *restrict arrayFirst,                      \
    GLuint *restrict arrayLast,                       \
    GLsizei count,                                    \
    const ctype *restrict indexes)                    \
{                                                     \
    int i;                                            \
                                                      \
    GPU_ASSERT(count > 0);                            \
                                                      \
    *arrayFirst = indexes[0];                         \
    *arrayLast  = indexes[0];                         \
                                                      \
    for (i = 1; i < count; i++) {                     \
        if (indexes[i] < *arrayFirst) {               \
            *arrayFirst = indexes[i];                 \
        }                                             \
        else if (indexes[i] > *arrayLast) {           \
            *arrayLast = indexes[i];                  \
        }                                             \
    }                                                 \
}

FIND_RANGE(ub, GLubyte )
FIND_RANGE(us, GLushort)
FIND_RANGE(ui, GLuint  )



void gpuImmediateIndexComputeRange(void)
{
	GLuint indexMin, indexMax;

	GPUindex* index = GPU_IMMEDIATE->index;

	switch (index->type) {
		case GL_UNSIGNED_BYTE:
			find_range_ub(&indexMin, &indexMax, index->count, (GLubyte* )(index->mappedBuffer));
			break;

		case GL_UNSIGNED_SHORT:
			find_range_us(&indexMin, &indexMax, index->count, (GLushort*)(index->mappedBuffer));
			break;

		case GL_UNSIGNED_INT:
			find_range_ui(&indexMin, &indexMax, index->count, (GLuint*  )(index->mappedBuffer));
			break;

		default:
			GPU_ABORT();
			return;
	}

	gpuImmediateIndexRange(indexMin, indexMax);
}



void gpuSingleClientElements_V3F(
	GLenum mode,
	const void *restrict vertexPointer,
	GLint vertexStride,
	GLsizei count,
	const GLuint *restrict indexes)
{
	GLuint indexMin, indexMax;

	find_range_ui(&indexMin, &indexMax, count, indexes);

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
	const GLuint *restrict indexes)
{
	GLuint indexMin, indexMax;

	find_range_ui(&indexMin, &indexMax, count, indexes);

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

void gpuSingleClientElements_C4UB_V3F(
	GLenum mode,
	const void *restrict colorPointer,
	GLint colorStride,
	const void *restrict vertexPointer,
	GLint vertexStride,
	GLsizei count,
	const GLuint *restrict indexes)
{
	GLuint indexMin, indexMax;

	find_range_ui(&indexMin, &indexMax, count, indexes);

	gpuSingleClientRangeElements_C4UB_V3F(
		mode,
		colorPointer,
		colorStride,
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
	const GLuint *restrict indexes)
{
	GLuint indexRange = indexMax - indexMin + 1;

	gpuBegin(GL_NOOP);
	gpuAppendClientArrays(arrays, indexMin, indexRange);
	gpuEnd();

	gpuIndexBegin(GL_UNSIGNED_INT);
	gpuIndexRelativeuiv(indexRange + indexMin, count, indexes);
	gpuIndexEnd();

	gpuImmediateIndexRange(indexMin, indexMax);

	GPU_IMMEDIATE->mode = mode;
	gpu_draw_range_elements_gl();
}

void gpuSingleClientRangeElements_V3F(
	GLenum mode,
	const void *restrict vertexPointer,
	GLint vertexStride,
	GLuint indexMin,
	GLuint indexMax,
	GLsizei count,
	const GLuint *restrict indexes)
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
	const GLuint *restrict indexes)
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

void gpuSingleClientRangeElements_C4UB_V3F(
	GLenum mode,
	const void *restrict colorPointer,
	GLint colorStride,
	const void *restrict vertexPointer,
	GLint vertexStride,
	GLuint indexMin,
	GLuint indexMax,
	GLsizei count,
	const GLuint *restrict indexes)
{
	GPUarrays arrays = GPU_ARRAYS_C4UB_V3F;

	arrays.normalPointer = colorPointer;
	arrays.normalStride  = colorStride != 0 ? colorStride : 4*sizeof(GLubyte);

	arrays.vertexPointer = vertexPointer;
	arrays.vertexStride  = vertexStride != 0 ? vertexStride : 3*sizeof(GLfloat);

	gpuImmediateFormat_C4_V3();
	gpuDrawClientRangeElements(mode, &arrays, indexMin, indexMax, count, indexes);
	gpuImmediateUnformat();
}



GPUindex* gpuNewIndex(void)
{
	return (GPUindex*)MEM_callocN(sizeof(GPUindex), "GPUindex");
}



void gpuDeleteIndex(GPUindex *restrict index)
{
	if (index) {
		GPUimmediate* immediate = index->immediate;

		gpu_index_shutdown_buffer_gl(index);
		immediate->index = NULL;

		MEM_freeN(index);
	}
}



void gpuImmediateIndex(GPUindex * index)
{
	//GPU_ASSERT(GPU_IMMEDIATE->index == NULL);

	if (index)
		index->immediate = GPU_IMMEDIATE;

	GPU_IMMEDIATE->index = index;
}



GPUindex* gpuGetImmediateIndex()
{
	return GPU_IMMEDIATE->index;
}



void gpuImmediateMaxIndexCount(GLsizei maxIndexCount, GLenum type)
{
	GPU_ASSERT(GPU_IMMEDIATE);
	GPU_ASSERT(GPU_IMMEDIATE->index);

	GPU_IMMEDIATE->index->maxIndexCount = maxIndexCount;
	GPU_IMMEDIATE->index->type          = type;
}



void gpuIndexBegin(GLenum type)
{
	GPUindex* index;

	GPU_ASSERT(ELEM3(type, GL_UNSIGNED_BYTE, GL_UNSIGNED_SHORT, GL_UNSIGNED_INT));

	index = GPU_IMMEDIATE->index;

	index->count    = 0;
	index->indexMin = 0;
	index->indexMax = 0;
	index->type     = type;

	gpu_index_begin_buffer_gl();
}



#define INDEX_RELATIVE(suffix, ctype, glsymbol)                                                                     \
void gpuIndexRelative##suffix(GLint offset, GLsizei count, const ctype *restrict indexes)                           \
{                                                                                                                   \
    int i;                                                                                                          \
    int start;                                                                                                      \
    int indexStart;                                                                                                 \
    GPUindex* index;                                                                                                \
                                                                                                                    \
    GPU_ASSERT(GPU_IMMEDIATE);                                                                                      \
    GPU_ASSERT(GPU_IMMEDIATE->index);                                                                               \
    GPU_ASSERT(GPU_IMMEDIATE->index->type == glsymbol);                                                             \
                                                                                                                    \
    {                                                                                                               \
        GLboolean indexCountOK;                                                                                     \
        GPU_SAFE_RETURN(GPU_IMMEDIATE->index->count + count <= GPU_IMMEDIATE->index->maxIndexCount, indexCountOK,); \
    }                                                                                                               \
                                                                                                                    \
    start = GPU_IMMEDIATE->count;                                                                                   \
    index = GPU_IMMEDIATE->index;                                                                                   \
    indexStart = index->count;                                                                                      \
                                                                                                                    \
    for (i = 0; i < count; i++) {                                                                                   \
        ((ctype*)(index->mappedBuffer))[indexStart+i] = start - offset + ((ctype*)indexes)[i];                      \
    }                                                                                                               \
                                                                                                                    \
    index->count += count;                                                                                          \
    index->offset = count*sizeof(ctype);                                                                            \
}

INDEX_RELATIVE(ubv, GLubyte,  GL_UNSIGNED_BYTE )
INDEX_RELATIVE(usv, GLushort, GL_UNSIGNED_SHORT)
INDEX_RELATIVE(uiv, GLuint,   GL_UNSIGNED_INT  )



#define INDEX(suffix, ctype, glsymbol)                                                                     \
void gpuIndex##suffix(ctype nextIndex)                                                                     \
{                                                                                                          \
    GPUindex* index;                                                                                       \
                                                                                                           \
    GPU_ASSERT(GPU_IMMEDIATE);                                                                             \
    GPU_ASSERT(GPU_IMMEDIATE->index);                                                                      \
    GPU_ASSERT(GPU_IMMEDIATE->index->type == glsymbol);                                                    \
                                                                                                           \
    {                                                                                                      \
        GLboolean indexCountOK;                                                                            \
        GPU_SAFE_RETURN(GPU_IMMEDIATE->index->count < GPU_IMMEDIATE->index->maxIndexCount, indexCountOK,); \
    }                                                                                                      \
                                                                                                           \
    index = GPU_IMMEDIATE->index;                                                                          \
    ((ctype*)(index->mappedBuffer))[index->count] = nextIndex;                                             \
    index->count++;                                                                                        \
    index->offset += sizeof(ctype);                                                                        \
}

INDEX(ub, GLubyte,  GL_UNSIGNED_BYTE )
INDEX(us, GLushort, GL_UNSIGNED_SHORT)
INDEX(ui, GLuint,   GL_UNSIGNED_INT  )



void gpuIndexEnd(void)
{
	gpu_index_end_buffer_gl();

	GPU_IMMEDIATE->index->mappedBuffer = NULL;
}



const char* gpuErrorString(GLenum err)
{
	switch(err) {
		case GL_NO_ERROR:
			return "No Error";

		case GL_INVALID_ENUM:
			return "Invalid Enum";

		case GL_INVALID_VALUE:
			return "Invalid Value";

		case GL_INVALID_OPERATION:
			return "Invalid Operation";

		case GL_STACK_OVERFLOW:
			return "Stack Overflow";

		case GL_STACK_UNDERFLOW:
			return "Stack Underflow";

		case GL_OUT_OF_MEMORY:
			return "Out of Memory";

#if GL_ARB_imagining
		case GL_TABLE_TOO_LARGE:
			return "Table Too Large";
#endif

#if defined(WITH_GLU)
		case GLU_INVALID_ENUM:
			return "Invalid Enum (GLU)";

		case GLU_INVALID_VALUE:
			return "Invalid Value (GLU)";

		case GLU_OUT_OF_MEMORY:
			return "Out of Memory (GLU)";
#endif

		default:
			return "<unknown error>";
	}
}



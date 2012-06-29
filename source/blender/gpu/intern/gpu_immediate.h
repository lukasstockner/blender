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

/** \file blender/gpu/intern/gpu_immediate.h
 *  \ingroup gpu
 */

#ifndef __GPU_IMMEDIATE_H__
#define __GPU_IMMEDIATE_H__

#ifdef GLES
#include <GLES2/gl2.h>
#else
#include <GL/glew.h>
#endif

#include <stdlib.h>

#include "BLI_utildefines.h"



/* Are restricted pointers available? (C99) */
#if (__STDC_VERSION__ < 199901L)
	/* Not a C99 compiler */
	#ifdef __GNUC__
		#define restrict __restrict__
	#else
		#define restrict /* restrict */
	#endif
#endif



#ifndef GPU_SAFETY
#define GPU_SAFETY (DEBUG && WITH_GPU_SAFETY)
#endif

#if GPU_SAFETY

/* Define some useful, but slow, checks for correct API usage. */

#define GPU_ASSERT(test) BLI_assert(test)

/* Bails out of function even if assert or abort are disabled.
   Needs a variable in scope to store results of the test.
   Can only be used in functions that return void. */
#define GPU_SAFE_RETURN(test, var, ret) \
    var = (GLboolean)(test);            \
    GPU_ASSERT(((void)#test, var));     \
    if (!var) {                         \
        return ret;                     \
    }

#define GPU_CHECK_BASE(var) \
    GPU_SAFE_RETURN(GPU_IMMEDIATE != NULL, var,);

#define GPU_CHECK_NO_BEGIN(var) \
    GPU_SAFE_RETURN(GPU_IMMEDIATE->buffer == NULL, var,);

#define GPU_CHECK_IS_LOCKED(var) \
    GPU_SAFE_RETURN(GPU_IMMEDIATE->lockCount > 0, var,);

#define GPU_CHECK_NO_LOCK(var) \
    GPU_SAFE_RETURN(GPU_IMMEDIATE->lockCount == 0, var,);

/* Each block contains variables that can be inspected by a
   debugger in the event that an assert is triggered. */

#define GPU_CHECK_CAN_BEGIN()             \
    {                                     \
    GLboolean immediateOK;                \
    GLboolean isLockedOK;                 \
    GLboolean noBeginOK;                  \
    GPU_CHECK_BASE(immediateOK);          \
    GPU_CHECK_IS_LOCKED(isLockedOK)       \
    GPU_CHECK_NO_BEGIN(noBeginOK)         \
    }

#define GPU_CHECK_CAN_END()                                      \
    {                                                            \
    GLboolean immediateOK;                                       \
    GLboolean isLockedOK;                                        \
    GLboolean hasBegunOK;                                        \
    GPU_CHECK_BASE(immediateOK);                                 \
    GPU_CHECK_IS_LOCKED(isLockedOK)                              \
    GPU_SAFE_RETURN(GPU_IMMEDIATE->buffer != NULL, hasBegunOK,); \
    }

#define GPU_CHECK_CAN_VERTEX_ATTRIB() GPU_CHECK_CAN_END()

#define GPU_CHECK_MODE(_mode)                                   \
    {                                                           \
    GLboolean immediateOK;                                      \
    GLboolean isModeOK;                                         \
    GPU_CHECK_BASE(immediateOK);                                \
    GPU_SAFE_RETURN(GPU_IMMEDIATE->mode == (_mode), isModeOK,); \
    }

#define GPU_CHECK_FORMAT(field, size)                                   \
    {                                                                   \
    GLboolean fieldSizeOK;                                              \
    GPU_SAFE_RETURN(GPU_IMMEDIATE-> field##Size == size, fieldSizeOK,); \
    }

#define GPU_CHECK_CAN_REPEAT() GPU_CHECK_CAN_BEGIN()

#define GPU_CURRENT_COLOR_VALID(v)  (GPU_IMMEDIATE->isColorValid  = (v))
#define GPU_CURRENT_NORMAL_VALID(v) (GPU_IMMEDIATE->isNormalValid = (v))

#else

#define GPU_ASSERT(test)

#define GPU_SAFE_RETURN(test, var, ret) { (void)var; }

#define GPU_CHECK_CAN_BEGIN()
#define GPU_CHECK_CAN_END()
#define GPU_CHECK_CAN_VERTEX_ATTRIB()
#define GPU_CHECK_MODE(mode)
#define GPU_CHECK_FORMAT()
#define GPU_CHECK_CAN_REPEAT()

#define GPU_CURRENT_COLOR_VALID(valid)
#define GPU_CURRENT_NORMAL_VALID(valid)

#endif



#ifdef __cplusplus
extern "C" {
#endif



const char* gpuErrorString(GLenum err);



void gpuImmediateElementSizes(
	GLint vertexSize,
	GLint normalSize,
	GLint colorSize);

void gpuImmediateMaxVertexCount(GLsizei maxVertexCount);

void gpuImmediateTextureUnitCount(size_t count);
void gpuImmediateTexCoordSizes(const GLint *restrict sizes);
void gpuImmediateTextureUnitMap(const GLenum *restrict map);

void gpuImmediateFloatAttribCount(size_t count);
void gpuImmediateFloatAttribSizes(const GLint *restrict sizes);
void gpuImmediateFloatAttribIndexMap(const GLuint *restrict map);

void gpuImmediateUbyteAttribCount(size_t count);
void gpuImmediateUbyteAttribSizes(const GLint *restrict sizes);
void gpuImmediateUbyteAttribIndexMap(const GLuint *restrict map);

void gpuImmediateFormatReset(void);
void gpuImmediateLock(void);
void gpuImmediateUnlock(void);
GLint gpuImmediateLockCount(void);



typedef struct GPUarrays {
	GLenum colorType;
	GLint  colorSize;
	GLint  colorStride;
	const void *restrict colorPointer;

	GLenum normalType;
	GLint  normalStride;
	const void *restrict normalPointer;

	GLenum vertexType;
	GLint  vertexSize;
	GLint  vertexStride;
	const void *restrict vertexPointer;
} GPUarrays;

#define GPU_MAX_ELEMENT_SIZE   4
#define GPU_COLOR_COMPS        4
#define GPU_MAX_TEXTURE_UNITS 32
#define GPU_MAX_FLOAT_ATTRIBS 32
#define GPU_MAX_UBYTE_ATTRIBS 32

typedef struct GPUimmediate {
	GLenum mode;

	/* All variables that determine the vertex array format
	   go in one structure so they can be easily cleared. */
	struct {
		GLint vertexSize;
		GLint normalSize;
		GLint texCoordSize[GPU_MAX_TEXTURE_UNITS];
		GLint colorSize;
		GLint attribSize_f[GPU_MAX_FLOAT_ATTRIBS];
		GLint attribSize_ub[GPU_MAX_UBYTE_ATTRIBS];

		GLenum textureUnitMap[GPU_MAX_TEXTURE_UNITS];
		size_t textureUnitCount;

		GLuint attribIndexMap_f[GPU_MAX_FLOAT_ATTRIBS];
		size_t attribCount_f;
		GLboolean attribNormalized_f[GPU_MAX_FLOAT_ATTRIBS];

		GLuint attribIndexMap_ub[GPU_MAX_UBYTE_ATTRIBS];
		size_t attribCount_ub;
		GLboolean attribNormalized_ub[GPU_MAX_UBYTE_ATTRIBS];
	} format;

	GLfloat vertex[GPU_MAX_ELEMENT_SIZE];
	GLfloat normal[3];
	GLfloat texCoord[GPU_MAX_TEXTURE_UNITS][GPU_MAX_ELEMENT_SIZE];
	GLubyte color[GPU_COLOR_COMPS];
	GLfloat attrib_f[GPU_MAX_FLOAT_ATTRIBS][GPU_MAX_ELEMENT_SIZE];
	GLubyte attrib_ub[GPU_MAX_UBYTE_ATTRIBS][GPU_COLOR_COMPS];

	char *restrict buffer;
	void *restrict bufferData;
	GLsizei stride;
	size_t  offset;
	GLsizei maxVertexCount;
	GLsizei count;

	int lockCount;

	void (*copyVertex)(void);

	void (*appendClientArrays)(
		const GPUarrays *arrays,
		GLint first,
		GLsizei count);

	void (*lockBuffer)(void);
	void (*unlockBuffer)(void);
	void (*beginBuffer)(void);
	void (*endBuffer)(void);
	void (*shutdownBuffer)(struct GPUimmediate *restrict immediate);

	void (*currentColor)(void);
	void (*getCurrentColor)(GLfloat *restrict color);

	void (*currentNormal)(void);

	struct GPUindex *restrict index;

	void (*indexBeginBuffer)(void);
	void (*indexEndBuffer)(void);
	void (*indexShutdownBuffer)(struct GPUindex *restrict index);

	void (*drawElements)(void);
	void (*drawRangeElements)(void);

#if GPU_SAFETY
	GLenum    lastTexture;
	GLboolean hasOverflowed;
	GLboolean isColorValid;
	GLboolean isNormalValid;
#endif
} GPUimmediate;

extern GPUimmediate *restrict GPU_IMMEDIATE;



GPUimmediate* gpuNewImmediate(void);
void gpuImmediateMakeCurrent(GPUimmediate *restrict  immediate);
void gpuDeleteImmediate(GPUimmediate *restrict  immediate);



void gpuPushImmediate(void);
GPUimmediate* gpuPopImmediate(void);
void gpuImmediateSingleDraw(GLenum mode, GPUimmediate *restrict immediate);
void gpuImmediateSingleRepeat(GPUimmediate *restrict immediate);

void gpuImmediateSingleDrawElements(GLenum mode, GPUimmediate *restrict immediate);
void gpuImmediateSingleRepeatElements(GPUimmediate *restrict immediate);

void gpuImmediateSingleDrawRangeElements(GLenum mode, GPUimmediate *restrict immediate);
void gpuImmediateSingleRepeatRangeElements(GPUimmediate *restrict immediate);



void gpuCurrentColor3f(GLfloat r, GLfloat g, GLfloat b);
void gpuCurrentColor3fv(const GLfloat *restrict v);
void gpuCurrentColor3ub(GLubyte r, GLubyte g, GLubyte b);
void gpuCurrentColor3ubv(const GLubyte *restrict v);
void gpuCurrentColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a);
void gpuCurrentColor4fv(const GLfloat *restrict v);
void gpuCurrentColor4ub(GLubyte r, GLubyte g, GLubyte b, GLubyte a);
void gpuCurrentColor4ubv(const GLubyte *restrict v);
void gpuCurrentColor4d(GLdouble r, GLdouble g, GLdouble b, GLdouble a);

void gpuCurrentColor3x(GLuint rgb);
void gpuCurrentColor4x(GLuint rgb, GLfloat a);

void gpuCurrentAlpha(GLfloat a);
void gpuMultCurrentAlpha(GLfloat factor);



void gpuGetCurrentColor4fv(GLfloat *restrict color);
void gpuGetCurrentColor4ubv(GLubyte *restrict color);

void gpuCurrentGrey3f(GLfloat luminance);

void gpuCurrentGrey4f(GLfloat luminance, GLfloat alpha);


void gpuCurrentNormal3fv(const GLfloat *restrict v);



/* utility functions to setup vertex format and lock */
#if GPU_SAFETY

void gpuSafetyImmediateFormat_V2          (const char* file, int line);
void gpuSafetyImmediateFormat_C4_V2       (const char* file, int line);
void gpuSafetyImmediateFormat_T2_V2       (const char* file, int line);
void gpuSafetyImmediateFormat_T2_C4_V2    (const char* file, int line);
void gpuSafetyImmediateFormat_V3          (const char* file, int line);
void gpuSafetyImmediateFormat_N3_V3       (const char* file, int line);
void gpuSafetyImmediateFormat_C4_V3       (const char* file, int line);
void gpuSafetyImmediateFormat_C4_N3_V3    (const char* file, int line);
void gpuSafetyImmediateFormat_T2_C4_N3_V3 (const char* file, int line);
void gpuSafetyImmediateFormat_T3_C4_V3    (const char* file, int line);
void gpuSafetyImmediateUnformat           (const char* file, int line);

#define gpuImmediateFormat_V2()          gpuSafetyImmediateFormat_V2          (__FILE__, __LINE__)
#define gpuImmediateFormat_C4_V2()       gpuSafetyImmediateFormat_C4_V2       (__FILE__, __LINE__)
#define gpuImmediateFormat_T2_V2()       gpuSafetyImmediateFormat_T2_V2       (__FILE__, __LINE__)
#define gpuImmediateFormat_T2_C4_V2()    gpuSafetyImmediateFormat_T2_C4_V2    (__FILE__, __LINE__)
#define gpuImmediateFormat_V3()          gpuSafetyImmediateFormat_V3          (__FILE__, __LINE__)
#define gpuImmediateFormat_N3_V3()       gpuSafetyImmediateFormat_N3_V3       (__FILE__, __LINE__)
#define gpuImmediateFormat_C4_V3()       gpuSafetyImmediateFormat_C4_V3       (__FILE__, __LINE__)
#define gpuImmediateFormat_C4_N3_V3()    gpuSafetyImmediateFormat_C4_N3_V3    (__FILE__, __LINE__)
#define gpuImmediateFormat_T2_C4_N3_V3() gpuSafetyImmediateFormat_T2_C4_N3_V3 (__FILE__, __LINE__)
#define gpuImmediateFormat_T3_C4_V3()    gpuSafetyImmediateFormat_T3_C4_V3    (__FILE__, __LINE__)
#define gpuImmediateUnformat()           gpuSafetyImmediateUnformat           (__FILE__, __LINE__)

#else

void gpuImmediateFormat_V2(void);
void gpuImmediateFormat_C4_V2(void);
void gpuImmediateFormat_T2_V2(void);
void gpuImmediateFormat_T2_C4_V2(void);
void gpuImmediateFormat_V3(void);
void gpuImmediateFormat_N3_V3(void);
void gpuImmediateFormat_C4_V3(void);
void gpuImmediateFormat_C4_N3_V3(void);
void gpuImmediateFormat_T2_C4_N3_V3(void);
void gpuImmediateFormat_T3_C4_V3(void);
void gpuImmediateUnformat(void);

#endif



extern const GPUarrays GPU_ARRAYS_V2F;
extern const GPUarrays GPU_ARRAYS_C4UB_V2F;
extern const GPUarrays GPU_ARRAYS_V3F;
extern const GPUarrays GPU_ARRAYS_C3F_V3F;
extern const GPUarrays GPU_ARRAYS_C4F_V3F;
extern const GPUarrays GPU_ARRAYS_N3F_V3F;
extern const GPUarrays GPU_ARRAYS_C3F_N3F_V3F;



typedef struct GPUindex {
	struct GPUimmediate *restrict immediate;

	void   *restrict bufferData;
	GLuint *restrict buffer;         /* mapped pointer for editing */
	void   *restrict unmappedBuffer; /* for passing to draw api */
	GLsizei maxIndexCount;
	GLsizei count;

	GLuint indexMin;
	GLuint indexMax;

	GLuint restart;
} GPUindex;

GPUindex* gpuNewIndex(void);
void gpuDeleteIndex(GPUindex *restrict index);

void gpuImmediateIndex(GPUindex * index);
void gpuImmediateMaxIndexCount(GLsizei maxIndexCount);
void gpuImmediateIndexRange(GLuint indexMin, GLuint indexMax);
void gpuImmediateIndexComputeRange(void);
void gpuImmediateIndexRestartValue(GLuint restart);

void gpuIndexBegin(void);
void gpuIndexRelativev(GLint offset, GLsizei count, const void *restrict indexes);
void gpuIndex(GLuint index);
void gpuIndexRestart(void);
void gpuIndexEnd(void);



void gpuAppendClientArrays(
	const GPUarrays* arrays,
	GLint first,
	GLsizei count);

void gpuDrawClientArrays(
	GLenum mode,
	const GPUarrays *arrays,
	GLint first,
	GLsizei count);

void gpuSingleClientArrays_V2F(
	GLenum mode,
	const void *restrict vertexPointer,
	GLint vertexStride,
	GLint first,
	GLsizei count);

void gpuSingleClientArrays_V3F(
	GLenum mode,
	const void *restrict vertexPointer,
	GLint vertexStride,
	GLint first,
	GLsizei count);

void gpuSingleClientArrays_C3F_V3F(
	GLenum mode,
	const void *restrict colorPointer,
	GLint colorStride,
	const void *restrict vertexPointer,
	GLint vertexStride,
	GLint first,
	GLsizei count);

void gpuSingleClientArrays_C4F_V3F(
	GLenum mode,
	const void *restrict colorPointer,
	GLint colorStride,
	const void *restrict vertexPointer,
	GLint vertexStride,
	GLint first,
	GLsizei count);

void gpuSingleClientArrays_N3F_V3F(
	GLenum mode,
	const void *restrict normalPointer,
	GLint normalStride,
	const void *restrict vertexPointer,
	GLint vertexStride,
	GLint first,
	GLsizei count);

void gpuSingleClientArrays_C3F_N3F_V3F(
	GLenum mode,
	const void *restrict colorPointer,
	GLint colorStride,
	const void *restrict normalPointer,
	GLint normalStride,
	const void *restrict vertexPointer,
	GLint vertexStride,
	GLint first,
	GLsizei count);

void gpuSingleClientArrays_C4UB_V2F(
	GLenum mode,
	const void *restrict colorPointer,
	GLint colorStride,
	const void *restrict vertexPointer,
	GLint vertexStride,
	GLint first,
	GLsizei count);



void gpuSingleClientElements_V3F(
	GLenum mode,
	const void *restrict vertexPointer,
	GLint vertexStride,
	GLsizei count,
	const void *restrict index);

void gpuSingleClientElements_N3F_V3F(
	GLenum mode,
	const void *restrict normalPointer,
	GLint normalStride,
	const void *restrict vertexPointer,
	GLint vertexStride,
	GLsizei count,
	void *restrict indexes);


void gpuDrawClientRangeElements(
	GLenum mode,
	const GPUarrays *restrict arrays,
	GLuint indexMin,
	GLuint indexMax,
	GLsizei count,
	const void *restrict indexes);

void gpuSingleClientRangeElements_V3F(
	GLenum mode,
	const void *restrict vertexPointer,
	GLint vertexStride,
	GLuint indexMin,
	GLuint indexMax,
	GLsizei count,
	const void *restrict indexes);

void gpuSingleClientRangeElements_N3F_V3F(
	GLenum mode,
	const void *restrict normalPointer,
	GLint normalStride,
	const void *restrict vertexPointer,
	GLint vertexStride,
	GLuint indexMin,
	GLuint indexMax,
	GLsizei count,
	const GLvoid *restrict indexes);



#ifdef __cplusplus
}
#endif

#endif /* __GPU_IMMEDIATE_H_ */

#ifndef GPU_INTERN_IMMEDIATE_H
#define GPU_INTERN_IMMEDIATE_H

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

/*
*/

/* internal */
#include "intern/gpu_common.h"
#include "intern/gpu_glew.h"
#include "intern/gpu_safety.h"

/* external */
#include "BLI_utildefines.h"

/* standard */
#include <string.h> /* for size_t */



#ifdef __cplusplus
extern "C" {
#endif



#if GPU_SAFETY

/* Define some useful, but potentially slow, checks for correct API usage. */

#define GPU_CHECK_BASE(var)                       \
    GPU_CHECK_NO_ERROR();                         \
    GPU_SAFE_RETURN(GPU_IMMEDIATE != NULL, var,);

#define GPU_CHECK_NO_BEGIN(var) \
    GPU_SAFE_RETURN(GPU_IMMEDIATE->mappedBuffer == NULL, var,);

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

#define GPU_CHECK_CAN_END()                                            \
    {                                                                  \
    GLboolean immediateOK;                                             \
    GLboolean isLockedOK;                                              \
    GLboolean hasBegunOK;                                              \
    GPU_CHECK_BASE(immediateOK);                                       \
    GPU_CHECK_IS_LOCKED(isLockedOK)                                    \
    GPU_SAFE_RETURN(GPU_IMMEDIATE->mappedBuffer != NULL, hasBegunOK,); \
    }

#define GPU_CHECK_MODE(_mode)                                   \
    {                                                           \
    GLboolean immediateOK;                                      \
    GLboolean isModeOK;                                         \
    GPU_CHECK_BASE(immediateOK);                                \
    GPU_SAFE_RETURN(GPU_IMMEDIATE->mode == (_mode), isModeOK,); \
    }

#define GPU_CHECK_CAN_REPEAT() GPU_CHECK_CAN_BEGIN()

#else

#define GPU_CHECK_CAN_BEGIN()
#define GPU_CHECK_CAN_END()
#define GPU_CHECK_MODE(mode)
#define GPU_CHECK_CAN_REPEAT()

#endif



void gpuImmediateElementSizes(
	GLint vertexSize,
	GLint normalSize,
	GLint colorSize);

void gpuImmediateMaxVertexCount(GLsizei maxVertexCount);

void gpuImmediateSamplerCount(size_t count);
void gpuImmediateSamplerMap(const GLint *restrict map);

void gpuImmediateTexCoordCount(size_t count);
void gpuImmediateTexCoordSizes(const GLint *restrict sizes);

void gpuImmediateFloatAttribCount(size_t count);
void gpuImmediateFloatAttribSizes(const GLint *restrict sizes);
void gpuImmediateFloatAttribIndexMap(const GLuint *restrict map);

void gpuImmediateUbyteAttribCount(size_t count);
void gpuImmediateUbyteAttribSizes(const GLint *restrict sizes);
void gpuImmediateUbyteAttribIndexMap(const GLuint *restrict map);

void  gpuImmediateFormatReset(void);
void  gpuImmediateLock(void);
void  gpuImmediateUnlock(void);
GLint gpuImmediateLockCount(void);

void gpuBegin(GLenum mode);
void gpuEnd(void);


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

#define GPU_MAX_FLOAT_ATTRIBS 32
#define GPU_MAX_UBYTE_ATTRIBS 32

typedef struct GPUimmediateformat {
	GLint     vertexSize;
	GLint     normalSize;
	GLint     colorSize;
	GLint     texCoordSize [GPU_MAX_COMMON_TEXCOORDS];
	GLint     attribSize_f [GPU_MAX_FLOAT_ATTRIBS];
	GLint     attribSize_ub[GPU_MAX_UBYTE_ATTRIBS];

	size_t    texCoordCount;

	GLint     samplerMap[GPU_MAX_COMMON_SAMPLERS];
	size_t    samplerCount;

	GLuint    attribIndexMap_f  [GPU_MAX_FLOAT_ATTRIBS];
	GLboolean attribNormalized_f[GPU_MAX_FLOAT_ATTRIBS];
	size_t    attribCount_f;

	GLuint    attribIndexMap_ub  [GPU_MAX_UBYTE_ATTRIBS];
	GLboolean attribNormalized_ub[GPU_MAX_UBYTE_ATTRIBS];
	size_t    attribCount_ub;
} GPUimmediateformat;

typedef struct GPUimmediate {
	GLenum mode;

	GPUimmediateformat format;

	GLfloat vertex[4];
	GLfloat normal[3];
	GLfloat texCoord[GPU_MAX_COMMON_TEXCOORDS][4];
	GLubyte color[4];
	GLfloat attrib_f[GPU_MAX_FLOAT_ATTRIBS][4];
	GLubyte attrib_ub[GPU_MAX_UBYTE_ATTRIBS][4];

	GLubyte *restrict mappedBuffer;
	void *restrict bufferData;
	GLsizei stride;
	size_t  offset;
	GLsizei maxVertexCount;
	GLsizei lastPrimVertex;
	GLsizei count;

	int lockCount;

	struct GPUindex *restrict index;

#if GPU_SAFETY
	GLint     lastTexture;
	GLboolean hasOverflowed;
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



/* utility functions to setup vertex format and lock */
#if GPU_SAFETY

void gpuSafetyImmediateFormat_V2          (const char* file, int line);
void gpuSafetyImmediateFormat_C4_V2       (const char* file, int line);
void gpuSafetyImmediateFormat_T2_V2       (const char* file, int line);
void gpuSafetyImmediateFormat_T2_V3       (const char* file, int line);
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
#define gpuImmediateFormat_T2_V3()       gpuSafetyImmediateFormat_T2_V3       (__FILE__, __LINE__)
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
void gpuImmediateFormat_T2_V3(void);
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
extern const GPUarrays GPU_ARRAYS_C4UB_V3F;
extern const GPUarrays GPU_ARRAYS_V3F;
extern const GPUarrays GPU_ARRAYS_C3F_V3F;
extern const GPUarrays GPU_ARRAYS_C4F_V3F;
extern const GPUarrays GPU_ARRAYS_N3F_V3F;
extern const GPUarrays GPU_ARRAYS_C3F_N3F_V3F;



typedef struct GPUindex {
	struct GPUimmediate *restrict immediate;

	void   *restrict bufferData;
	void   *restrict mappedBuffer;
	GLsizei maxIndexCount;
	GLsizei count;

	GLuint indexMin;
	GLuint indexMax;

	GLuint restart;

	GLenum  type;
	GLsizei offset;
} GPUindex;

GPUindex* gpuNewIndex(void);
void gpuDeleteIndex(GPUindex *restrict index);

void gpuImmediateIndex(GPUindex * index);
GPUindex* gpuGetImmediateIndex(void);
void gpuImmediateMaxIndexCount(GLsizei maxIndexCount, GLenum type);
void gpuImmediateIndexRange(GLuint indexMin, GLuint indexMax);
void gpuImmediateIndexComputeRange(void);
void gpuImmediateIndexRestartValue(GLuint restart);

void gpuIndexBegin(GLenum type);

void gpuIndexRelativeubv(GLint offset, GLsizei count, const GLubyte  *restrict indexes);
void gpuIndexRelativeusv(GLint offset, GLsizei count, const GLushort *restrict indexes);
void gpuIndexRelativeuiv(GLint offset, GLsizei count, const GLuint   *restrict indexes);

void gpuIndexub(GLubyte  index);
void gpuIndexus(GLushort index);
void gpuIndexui(GLuint   index);

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

void gpuSingleClientArrays_C4UB_V3F(
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
	const GLuint *restrict index);

void gpuSingleClientElements_N3F_V3F(
	GLenum mode,
	const void *restrict normalPointer,
	GLint normalStride,
	const void *restrict vertexPointer,
	GLint vertexStride,
	GLsizei count,
	const GLuint *restrict indexes);

void gpuSingleClientElements_C4UB_V3F(
	GLenum mode,
	const void *restrict colorPointer,
	GLint colorStride,
	const void *restrict vertexPointer,
	GLint vertexStride,
	GLsizei count,
	const GLuint *restrict indexes);



void gpuDrawClientRangeElements(
	GLenum mode,
	const GPUarrays *restrict arrays,
	GLuint indexMin,
	GLuint indexMax,
	GLsizei count,
	const GLuint *restrict indexes);

void gpuSingleClientRangeElements_V3F(
	GLenum mode,
	const void *restrict vertexPointer,
	GLint vertexStride,
	GLuint indexMin,
	GLuint indexMax,
	GLsizei count,
	const GLuint *restrict indexes);

void gpuSingleClientRangeElements_N3F_V3F(
	GLenum mode,
	const void *restrict normalPointer,
	GLint normalStride,
	const void *restrict vertexPointer,
	GLint vertexStride,
	GLuint indexMin,
	GLuint indexMax,
	GLsizei count,
	const GLuint *restrict indexes);

void gpuSingleClientRangeElements_C4UB_V3F(
	GLenum mode,
	const void *restrict colorPointer,
	GLint colorStride,
	const void *restrict vertexPointer,
	GLint vertexStride,
	GLuint indexMin,
	GLuint indexMax,
	GLsizei count,
	const GLuint *restrict indexes);



void gpu_commit_current (void);
void gpu_commit_samplers(void);



#if defined(GLEW_ES_ONLY)

/* ES 2.0 doesn't define QUADS, but the immediate mode replacement library emulates QUADS */
/* (GL core has deprecated QUADS, but it should still be in the header) */

#ifndef GL_QUADS
#define GL_QUADS 0x0007
#endif

#ifndef GL_QUAD_STRIP
#define GL_QUAD_STRIP 0x0008
#endif

#ifndef GL_POLYGON
#define GL_POLYGON 0x0009
#endif

#endif



#ifdef __cplusplus
}
#endif

#endif /* __GPU_IMMEDIATE_H_ */

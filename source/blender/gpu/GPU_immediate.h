#ifndef _GPU_IMMEDIATE_INLINE_H_
#define _GPU_IMMEDIATE_INLINE_H_

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

/** \file blender/gpu/GPU_immediate.h
 *  \ingroup gpu
 */

#include "BLI_math_base.h"
#include "BLI_utildefines.h"

#include "GPU_common.h"
#include "GPU_glew.h"
#include "GPU_debug.h"

#include <limits.h>
#include <string.h> /* for size_t */



#ifdef __cplusplus
extern "C" {
#endif

struct GPUImmediate;
struct GPUindex;

void gpu_lock_buffer_gl(void);
void gpu_unlock_buffer_gl(void);
void gpu_begin_buffer_gl(void);
void gpu_end_buffer_gl(void);
void gpu_shutdown_buffer_gl(struct GPUImmediate *immediate);
void gpu_current_normal_gl(void);
void gpu_index_begin_buffer_gl(void);
void gpu_index_end_buffer_gl(void);
void gpu_index_shutdown_buffer_gl(struct GPUindex *index);
void gpu_draw_elements_gl(void);
void gpu_draw_range_elements_gl(void);

#ifdef GPU_SAFETY

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
void gpuImmediateSamplerMap(const int * map);

void gpuImmediateTexCoordCount(size_t count);
void gpuImmediateTexCoordSizes(const int * sizes);

void gpuImmediateFloatAttribCount(size_t count);
void gpuImmediateFloatAttribSizes(const int * sizes);
void gpuImmediateFloatAttribIndexMap(const unsigned int * map);

void gpuImmediateUbyteAttribCount(size_t count);
void gpuImmediateUbyteAttribSizes(const int * sizes);
void gpuImmediateUbyteAttribIndexMap(const unsigned int * map);

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
	const void * colorPointer;

	GLenum normalType;
	GLint  normalStride;
	const void *normalPointer;

	GLenum vertexType;
	GLint  vertexSize;
	GLint  vertexStride;
	const void *vertexPointer;
} GPUarrays;

#define GPU_MAX_FLOAT_ATTRIBS 32
#define GPU_MAX_UBYTE_ATTRIBS 32

typedef struct GPUImmediateFormat {
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
} GPUImmediateFormat;

typedef struct GPUImmediate {
	GLenum mode;

	GPUImmediateFormat format;

	GLfloat vertex[4];
	GLfloat normal[3];
	GLfloat texCoord[GPU_MAX_COMMON_TEXCOORDS][4];
	GLubyte color[4];
	GLfloat attrib_f[GPU_MAX_FLOAT_ATTRIBS][4];
	GLubyte attrib_ub[GPU_MAX_UBYTE_ATTRIBS][4];

	GLubyte *mappedBuffer;
	void *vertex_stream;
	GLsizei stride;
	size_t  offset;
	GLsizei maxVertexCount;
	GLsizei lastPrimVertex;
	GLsizei count;

	int lockCount;

	struct GPUindex *index;

	void (*copyVertex)(void);

	GLint     lastTexture;
	GLboolean hasOverflowed;
} GPUImmediate;

extern GPUImmediate *GPU_IMMEDIATE;



GPUImmediate* gpuNewImmediate(void);
void gpuImmediateMakeCurrent(GPUImmediate *immediate);
void gpuDeleteImmediate(GPUImmediate *immediate);

void gpuPushImmediate(void);
GPUImmediate* gpuPopImmediate(void);
void gpuImmediateSingleDraw(GLenum mode, GPUImmediate *immediate);
void gpuImmediateSingleRepeat(GPUImmediate *immediate);

void gpuImmediateSingleDrawElements(GLenum mode, GPUImmediate *immediate);
void gpuImmediateSingleRepeatElements(GPUImmediate *immediate);

void gpuImmediateSingleDrawRangeElements(GLenum mode, GPUImmediate *immediate);
void gpuImmediateSingleRepeatRangeElements(GPUImmediate *immediate);



/* utility functions to setup vertex format and lock */
#ifdef GPU_SAFETY

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
	struct GPUImmediate *immediate;

	void   *element_stream;
	void   *mappedBuffer;
	GLsizei maxIndexCount;
	GLsizei count;

	GLuint indexMin;
	GLuint indexMax;

	GLuint restart;

	GLenum  type;
	GLsizei offset;
} GPUindex;

GPUindex* gpuNewIndex(void);
void gpuDeleteIndex(GPUindex *index);

void gpuImmediateIndex(GPUindex * index);
GPUindex* gpuGetImmediateIndex(void);
void gpuImmediateMaxIndexCount(GLsizei maxIndexCount, GLenum type);
void gpuImmediateIndexRange(GLuint indexMin, GLuint indexMax);
void gpuImmediateIndexComputeRange(void);
void gpuImmediateIndexRestartValue(GLuint restart);

void gpuIndexBegin(GLenum type);

void gpuIndexRelativeubv(GLint offset, GLsizei count, const GLubyte  *indexes);
void gpuIndexRelativeusv(GLint offset, GLsizei count, const GLushort *indexes);
void gpuIndexRelativeuiv(GLint offset, GLsizei count, const GLuint   *indexes);

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
	const void *vertexPointer,
	GLint vertexStride,
	GLint first,
	GLsizei count);

void gpuSingleClientArrays_V3F(
	GLenum mode,
	const void *vertexPointer,
	GLint vertexStride,
	GLint first,
	GLsizei count);

void gpuSingleClientArrays_C3F_V3F(
	GLenum mode,
	const void *colorPointer,
	GLint colorStride,
	const void *vertexPointer,
	GLint vertexStride,
	GLint first,
	GLsizei count);

void gpuSingleClientArrays_C4F_V3F(
	GLenum mode,
	const void *colorPointer,
	GLint colorStride,
	const void *vertexPointer,
	GLint vertexStride,
	GLint first,
	GLsizei count);

void gpuSingleClientArrays_N3F_V3F(
	GLenum mode,
	const void *normalPointer,
	GLint normalStride,
	const void *vertexPointer,
	GLint vertexStride,
	GLint first,
	GLsizei count);

void gpuSingleClientArrays_C3F_N3F_V3F(
	GLenum mode,
	const void *colorPointer,
	GLint colorStride,
	const void *normalPointer,
	GLint normalStride,
	const void *vertexPointer,
	GLint vertexStride,
	GLint first,
	GLsizei count);

void gpuSingleClientArrays_C4UB_V2F(
	GLenum mode,
	const void *colorPointer,
	GLint colorStride,
	const void *vertexPointer,
	GLint vertexStride,
	GLint first,
	GLsizei count);

void gpuSingleClientArrays_C4UB_V3F(
	GLenum mode,
	const void *colorPointer,
	GLint colorStride,
	const void *vertexPointer,
	GLint vertexStride,
	GLint first,
	GLsizei count);



void gpuSingleClientElements_V3F(
	GLenum mode,
	const void *vertexPointer,
	GLint vertexStride,
	GLsizei count,
	const GLuint *index);

void gpuSingleClientElements_N3F_V3F(
	GLenum mode,
	const void *normalPointer,
	GLint normalStride,
	const void *vertexPointer,
	GLint vertexStride,
	GLsizei count,
	const GLuint *indexes);

void gpuSingleClientElements_C4UB_V3F(
	GLenum mode,
	const void *colorPointer,
	GLint colorStride,
	const void *vertexPointer,
	GLint vertexStride,
	GLsizei count,
	const GLuint *indexes);



void gpuDrawClientRangeElements(
	GLenum mode,
	const GPUarrays *arrays,
	GLuint indexMin,
	GLuint indexMax,
	GLsizei count,
	const GLuint *indexes);

void gpuSingleClientRangeElements_V3F(
	GLenum mode,
	const void *vertexPointer,
	GLint vertexStride,
	GLuint indexMin,
	GLuint indexMax,
	GLsizei count,
	const GLuint *indexes);

void gpuSingleClientRangeElements_N3F_V3F(
	GLenum mode,
	const void *normalPointer,
	GLint normalStride,
	const void *vertexPointer,
	GLint vertexStride,
	GLuint indexMin,
	GLuint indexMax,
	GLsizei count,
	const GLuint *indexes);

void gpuSingleClientRangeElements_C4UB_V3F(
	GLenum mode,
	const void *colorPointer,
	GLint colorStride,
	const void *vertexPointer,
	GLint vertexStride,
	GLuint indexMin,
	GLuint indexMax,
	GLsizei count,
	const GLuint *indexes);



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



BLI_INLINE void gpuColor3f(GLfloat r, GLfloat g, GLfloat b)
{
	GPU_IMMEDIATE->color[0] = (GLubyte)(255.0f * r);
	GPU_IMMEDIATE->color[1] = (GLubyte)(255.0f * g);
	GPU_IMMEDIATE->color[2] = (GLubyte)(255.0f * b);
	GPU_IMMEDIATE->color[3] = 255;
}

BLI_INLINE void gpuColor3fv(const GLfloat *v)
{
	GPU_IMMEDIATE->color[0] = (GLubyte)(255.0f * v[0]);
	GPU_IMMEDIATE->color[1] = (GLubyte)(255.0f * v[1]);
	GPU_IMMEDIATE->color[2] = (GLubyte)(255.0f * v[2]);
	GPU_IMMEDIATE->color[3] = 255;
}

BLI_INLINE void gpuColor3ub(GLubyte r, GLubyte g, GLubyte b)
{
	GPU_IMMEDIATE->color[0] = r;
	GPU_IMMEDIATE->color[1] = g;
	GPU_IMMEDIATE->color[2] = b;
	GPU_IMMEDIATE->color[3] = 255;
}

BLI_INLINE void gpuColor3ubv(const GLubyte *v)
{
	GPU_IMMEDIATE->color[0] = v[0];
	GPU_IMMEDIATE->color[1] = v[1];
	GPU_IMMEDIATE->color[2] = v[2];
	GPU_IMMEDIATE->color[3] = 255;
}

BLI_INLINE void gpuColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
	GPU_IMMEDIATE->color[0] = (GLubyte)(255.0f * r);
	GPU_IMMEDIATE->color[1] = (GLubyte)(255.0f * g);
	GPU_IMMEDIATE->color[2] = (GLubyte)(255.0f * b);
	GPU_IMMEDIATE->color[3] = (GLubyte)(255.0f * a);
}

BLI_INLINE void gpuColor4fv(const GLfloat *v)
{
	GPU_IMMEDIATE->color[0] = (GLubyte)(255.0f * v[0]);
	GPU_IMMEDIATE->color[1] = (GLubyte)(255.0f * v[1]);
	GPU_IMMEDIATE->color[2] = (GLubyte)(255.0f * v[2]);
	GPU_IMMEDIATE->color[3] = (GLubyte)(255.0f * v[3]);
}

BLI_INLINE void gpuColor4ub(GLubyte r, GLubyte g, GLubyte b, GLubyte a)
{
	GPU_IMMEDIATE->color[0] = r;
	GPU_IMMEDIATE->color[1] = g;
	GPU_IMMEDIATE->color[2] = b;
	GPU_IMMEDIATE->color[3] = a;
}

BLI_INLINE void gpuColor4ubv(const GLubyte *v)
{
	GPU_IMMEDIATE->color[0] = v[0];
	GPU_IMMEDIATE->color[1] = v[1];
	GPU_IMMEDIATE->color[2] = v[2];
	GPU_IMMEDIATE->color[3] = v[3];
}

BLI_INLINE void gpuColor4d(GLdouble r, GLdouble g, GLdouble b, GLdouble a)
{
	GPU_IMMEDIATE->color[0] = (GLubyte)(255.0 * r);
	GPU_IMMEDIATE->color[1] = (GLubyte)(255.0 * g);
	GPU_IMMEDIATE->color[2] = (GLubyte)(255.0 * b);
	GPU_IMMEDIATE->color[3] = (GLubyte)(255.0 * a);
}



/* This function converts a numerical value to the equivalent 24-bit
   color, while not being endian-sensitive. On little-endians, this
   is the same as doing a 'naive' indexing, on big-endian, it is not! */

BLI_INLINE void gpuColor3P(GLuint rgb)
{
	GPU_IMMEDIATE->color[0] = (rgb >>  0) & 0xFF;
	GPU_IMMEDIATE->color[1] = (rgb >>  8) & 0xFF;
	GPU_IMMEDIATE->color[2] = (rgb >> 16) & 0xFF;
	GPU_IMMEDIATE->color[3] = 255;
}

BLI_INLINE void gpuColor4P(GLuint rgb, GLfloat a)
{
	GPU_IMMEDIATE->color[0] = (rgb >>  0) & 0xFF;
	GPU_IMMEDIATE->color[1] = (rgb >>  8) & 0xFF;
	GPU_IMMEDIATE->color[2] = (rgb >> 16) & 0xFF;
	GPU_IMMEDIATE->color[3] = (GLubyte)(255.0f * a);
}



BLI_INLINE void gpuGray3f(GLfloat luminance)
{
	GLubyte c = (GLubyte)(255.0 * luminance);

	GPU_IMMEDIATE->color[0] = c;
	GPU_IMMEDIATE->color[1] = c;
	GPU_IMMEDIATE->color[2] = c;
	GPU_IMMEDIATE->color[3] = 255;
}

BLI_INLINE void gpuGray4f(GLfloat luminance, GLfloat alpha)
{
	GLubyte c = (GLubyte)(255.0 * luminance);

	GPU_IMMEDIATE->color[0] = c;
	GPU_IMMEDIATE->color[1] = c;
	GPU_IMMEDIATE->color[2] = c;
	GPU_IMMEDIATE->color[3] = (GLubyte)(255.0f * alpha);
}



BLI_INLINE void gpuAlpha(GLfloat a)
{
	GPU_IMMEDIATE->color[3] = (GLubyte)(255.0f * a);
}

BLI_INLINE void gpuMultAlpha(GLfloat factor)
{
	GPU_IMMEDIATE->color[3] = (GLubyte)(factor * (GLfloat)(GPU_IMMEDIATE->color[3]));
}



BLI_INLINE void gpuGetColor4fv(GLfloat *color)
{
	color[0] = (GLfloat)(GPU_IMMEDIATE->color[0]) / 255.0f;
	color[1] = (GLfloat)(GPU_IMMEDIATE->color[1]) / 255.0f;
	color[2] = (GLfloat)(GPU_IMMEDIATE->color[2]) / 255.0f;
	color[3] = (GLfloat)(GPU_IMMEDIATE->color[3]) / 255.0f;
}

BLI_INLINE void gpuGetColor4ubv(GLubyte *color)
{
	color[0] = GPU_IMMEDIATE->color[0];
	color[1] = GPU_IMMEDIATE->color[1];
	color[2] = GPU_IMMEDIATE->color[2];
	color[3] = GPU_IMMEDIATE->color[3];
}



BLI_INLINE void gpuNormal3f(GLfloat x, GLfloat y, GLfloat z)
{
	GPU_IMMEDIATE->normal[0] = x;
	GPU_IMMEDIATE->normal[1] = y;
	GPU_IMMEDIATE->normal[2] = z;
}

BLI_INLINE void gpuNormal3fv(const GLfloat *v)
{
	GPU_IMMEDIATE->normal[0] = v[0];
	GPU_IMMEDIATE->normal[1] = v[1];
	GPU_IMMEDIATE->normal[2] = v[2];
}

BLI_INLINE void gpuNormal3sv(const GLshort *v)
{
	GPU_IMMEDIATE->normal[0] = v[0] / (float)SHRT_MAX;
	GPU_IMMEDIATE->normal[1] = v[1] / (float)SHRT_MAX;
	GPU_IMMEDIATE->normal[2] = v[2] / (float)SHRT_MAX;
}



BLI_INLINE void gpuTexCoord2f(GLfloat s, GLfloat t)
{
	GPU_IMMEDIATE->texCoord[0][0] = s;
	GPU_IMMEDIATE->texCoord[0][1] = t;
	GPU_IMMEDIATE->texCoord[0][2] = 0;
	GPU_IMMEDIATE->texCoord[0][3] = 1;
}

BLI_INLINE void gpuTexCoord2fv(const GLfloat *v)
{
	GPU_IMMEDIATE->texCoord[0][0] = v[0];
	GPU_IMMEDIATE->texCoord[0][1] = v[1];
	GPU_IMMEDIATE->texCoord[0][2] = 0;
	GPU_IMMEDIATE->texCoord[0][3] = 1;
}

BLI_INLINE void gpuTexCoord2iv(const GLint *v)
{
	GPU_IMMEDIATE->texCoord[0][0] = (GLfloat)(v[0]);
	GPU_IMMEDIATE->texCoord[0][1] = (GLfloat)(v[1]);
	GPU_IMMEDIATE->texCoord[0][2] = 0;
	GPU_IMMEDIATE->texCoord[0][3] = 1;
}

BLI_INLINE void gpuTexCoord3f(const GLfloat s, const GLfloat t, const GLfloat u)
{
	GPU_IMMEDIATE->texCoord[0][0] = s;
	GPU_IMMEDIATE->texCoord[0][1] = t;
	GPU_IMMEDIATE->texCoord[0][2] = u;
	GPU_IMMEDIATE->texCoord[0][3] = 1;
}

BLI_INLINE void gpuTexCoord3fv (const GLfloat *v)
{
	GPU_IMMEDIATE->texCoord[0][0] = v[0];
	GPU_IMMEDIATE->texCoord[0][1] = v[1];
	GPU_IMMEDIATE->texCoord[0][2] = v[2];
	GPU_IMMEDIATE->texCoord[0][3] = 1;
}



BLI_INLINE void gpuMultiTexCoord2f(GLint index, GLfloat s, GLfloat t)
{
	GPU_IMMEDIATE->texCoord[index][0] = s;
	GPU_IMMEDIATE->texCoord[index][1] = t;
	GPU_IMMEDIATE->texCoord[index][2] = 0;
	GPU_IMMEDIATE->texCoord[index][3] = 1;
}

BLI_INLINE void gpuMultiTexCoord2fv(GLint index, const GLfloat *v)
{
	GPU_IMMEDIATE->texCoord[index][0] = v[0];
	GPU_IMMEDIATE->texCoord[index][1] = v[1];
	GPU_IMMEDIATE->texCoord[index][2] = 0;
	GPU_IMMEDIATE->texCoord[index][3] = 1;
}

BLI_INLINE void gpuMultiTexCoord3fv(GLint index, const GLfloat *v)
{
	GPU_IMMEDIATE->texCoord[index][0] = v[0];
	GPU_IMMEDIATE->texCoord[index][1] = v[1];
	GPU_IMMEDIATE->texCoord[index][2] = v[2];
	GPU_IMMEDIATE->texCoord[index][3] = 1;
}

BLI_INLINE void gpuMultiTexCoord4fv(GLint index, const GLfloat *v)
{
	GPU_IMMEDIATE->texCoord[index][0] = v[0];
	GPU_IMMEDIATE->texCoord[index][1] = v[1];
	GPU_IMMEDIATE->texCoord[index][2] = v[2];
	GPU_IMMEDIATE->texCoord[index][3] = v[3];
}




BLI_INLINE void gpuVertexAttrib2fv(GLsizei index, const GLfloat *v)
{
	GPU_IMMEDIATE->attrib_f[index][0] = v[0];
	GPU_IMMEDIATE->attrib_f[index][1] = v[1];
	GPU_IMMEDIATE->attrib_f[index][2] = 0;
	GPU_IMMEDIATE->attrib_f[index][3] = 1;
}

BLI_INLINE void gpuVertexAttrib3fv(GLsizei index, const GLfloat *v)
{
	GPU_IMMEDIATE->attrib_f[index][0] = v[0];
	GPU_IMMEDIATE->attrib_f[index][1] = v[1];
	GPU_IMMEDIATE->attrib_f[index][2] = v[2];
	GPU_IMMEDIATE->attrib_f[index][3] = 1;
}

BLI_INLINE void gpuVertexAttrib4fv(GLsizei index, const GLfloat *v)
{
	GPU_IMMEDIATE->attrib_f[index][0] = v[0];
	GPU_IMMEDIATE->attrib_f[index][1] = v[1];
	GPU_IMMEDIATE->attrib_f[index][2] = v[2];
	GPU_IMMEDIATE->attrib_f[index][3] = v[3];
}

BLI_INLINE void gpuVertexAttrib4ubv(GLsizei index, const GLubyte *v)
{
	GPU_IMMEDIATE->attrib_ub[index][0] = v[0];
	GPU_IMMEDIATE->attrib_ub[index][1] = v[1];
	GPU_IMMEDIATE->attrib_ub[index][2] = v[2];
	GPU_IMMEDIATE->attrib_ub[index][3] = v[3];
}



BLI_INLINE void gpuVertex2f(GLfloat x, GLfloat y)
{
	GPU_IMMEDIATE->vertex[0] = x;
	GPU_IMMEDIATE->vertex[1] = y;
	GPU_IMMEDIATE->vertex[2] = 0;
	GPU_IMMEDIATE->vertex[3] = 1;

	GPU_IMMEDIATE->copyVertex();
}

BLI_INLINE void gpuVertex2fv(const GLfloat *v)
{
	GPU_IMMEDIATE->vertex[0] = v[0];
	GPU_IMMEDIATE->vertex[1] = v[1];
	GPU_IMMEDIATE->vertex[2] = 0;
	GPU_IMMEDIATE->vertex[3] = 1;

	GPU_IMMEDIATE->copyVertex();
}

BLI_INLINE void gpuVertex3f(GLfloat x, GLfloat y, GLfloat z)
{
	GPU_IMMEDIATE->vertex[0] = x;
	GPU_IMMEDIATE->vertex[1] = y;
	GPU_IMMEDIATE->vertex[2] = z;
	GPU_IMMEDIATE->vertex[3] = 1;

	GPU_IMMEDIATE->copyVertex();
 }

BLI_INLINE void gpuVertex3fv(const GLfloat *v)
{
	GPU_IMMEDIATE->vertex[0] = v[0];
	GPU_IMMEDIATE->vertex[1] = v[1];
	GPU_IMMEDIATE->vertex[2] = v[2];
	GPU_IMMEDIATE->vertex[3] = 1;

	GPU_IMMEDIATE->copyVertex();
}

BLI_INLINE void gpuVertex3d(GLdouble x, GLdouble y, GLdouble z)
{
	GPU_IMMEDIATE->vertex[0] = (GLfloat)(x);
	GPU_IMMEDIATE->vertex[1] = (GLfloat)(y);
	GPU_IMMEDIATE->vertex[2] = (GLfloat)(z);
	GPU_IMMEDIATE->vertex[3] = 1;

	GPU_IMMEDIATE->copyVertex();
}

BLI_INLINE void gpuVertex3dv(const GLdouble *v)
{
	GPU_IMMEDIATE->vertex[0] = (GLfloat)(v[0]);
	GPU_IMMEDIATE->vertex[1] = (GLfloat)(v[1]);
	GPU_IMMEDIATE->vertex[2] = (GLfloat)(v[2]);
	GPU_IMMEDIATE->vertex[3] = 1;

	GPU_IMMEDIATE->copyVertex();
}

BLI_INLINE void gpuVertex2i(GLint x, GLint y)
{
	GPU_IMMEDIATE->vertex[0] = (GLfloat)(x);
	GPU_IMMEDIATE->vertex[1] = (GLfloat)(y);
	GPU_IMMEDIATE->vertex[2] = 0;
	GPU_IMMEDIATE->vertex[3] = 1;

	GPU_IMMEDIATE->copyVertex();
}

BLI_INLINE void gpuVertex2iv(const GLint *v)
{
	GPU_IMMEDIATE->vertex[0] = (GLfloat)(v[0]);
	GPU_IMMEDIATE->vertex[1] = (GLfloat)(v[1]);
	GPU_IMMEDIATE->vertex[2] = 0;
	GPU_IMMEDIATE->vertex[3] = 1;

	GPU_IMMEDIATE->copyVertex();
}

BLI_INLINE void gpuVertex2sv(const GLshort *v)
{
	GPU_IMMEDIATE->vertex[0] = (GLfloat)(v[0]);
	GPU_IMMEDIATE->vertex[1] = (GLfloat)(v[1]);
	GPU_IMMEDIATE->vertex[2] = 0;
	GPU_IMMEDIATE->vertex[3] = 1;

	GPU_IMMEDIATE->copyVertex();
}

BLI_INLINE void gpuDraw(GLenum mode)
{
	GPU_CHECK_CAN_REPEAT();

	GPU_IMMEDIATE->mode = mode;
	gpu_end_buffer_gl();
}

BLI_INLINE void gpuRepeat(void)
{
	GPU_CHECK_CAN_REPEAT();

	gpu_end_buffer_gl();
}

BLI_INLINE void gpuDrawElements(GLenum mode)
{
	GPU_IMMEDIATE->mode = mode;
	gpu_draw_elements_gl();
}

BLI_INLINE void gpuRepeatElements(void)
{
	gpu_draw_elements_gl();
}

BLI_INLINE void gpuDrawRangeElements(GLenum mode)
{
	GPU_IMMEDIATE->mode = mode;
	gpu_draw_range_elements_gl();
}

BLI_INLINE void gpuRepeatRangeElements(void)
{
	gpu_draw_range_elements_gl();
}


#ifdef __cplusplus
}
#endif

#endif /* __GPU_IMMEDIATE_INLINE_H_ */

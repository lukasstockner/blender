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

#include <GL/glew.h>

#include <stdlib.h>



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
#define GPU_SAFETY DEBUG && WITH_GPU_SAFETY
#endif

#if GPU_SAFETY

/* Define some useful, but slow, checks for correct API usage. */

/* Bails out of function even if assert or abort are disabled.
   Needs a variable in scope to store results of the test.
   Can only be used in functions that return void. */
#define GPU_SAFE_RETURN(test, var) \
    var = (GLboolean)(test);       \
    BLI_assert((#test, var));      \
    if (!var) {                    \
        return;                    \
    }

#define GPU_CHECK_BASE(var) \
    GPU_SAFE_RETURN(GPU_IMMEDIATE, var);

#define GPU_CHECK_HAS_BEGUN(var) \
    GPU_SAFE_RETURN(GPU_IMMEDIATE->buffer != NULL, var);

#define GPU_CHECK_NO_BEGIN(var) \
    GPU_SAFE_RETURN(!(GPU_IMMEDIATE->buffer != NULL), var);

#define GPU_CHECK_IS_LOCKED(var) \
    GPU_SAFE_RETURN(GPU_IMMEDIATE->lockCount > 0, var);

#define GPU_CHECK_NO_LOCK(var) \
    GPU_SAFE_RETURN(GPU_IMMEDIATE->lockCount == 0, var);

#define GPU_CHECK_BUFFER_BEGIN(var) \
    GPU_SAFE_RETURN(GPU_IMMEDIATE->bufferBegin != NULL, var);

#define GPU_CHECK_BUFFER_END(var) \
    GPU_SAFE_RETURN(GPU_IMMEDIATE->bufferEnd != NULL, var);

/* Each block contains variables that can be inspected by a
   debugger in the event that an assert is triggered. */

#define GPU_CHECK_CAN_BEGIN()             \
    {                                     \
    GLboolean immediateOK;                \
    GLboolean isLockedOK;                 \
    GLboolean noBeginOK;                  \
    GLboolean bufferBeginOK;              \
    GPU_CHECK_BASE(immediateOK);          \
    GPU_CHECK_IS_LOCKED(isLockedOK)       \
    GPU_CHECK_NO_BEGIN(noBeginOK)         \
    GPU_CHECK_BUFFER_BEGIN(bufferBeginOK) \
    }

#define GPU_CHECK_CAN_END()             \
    {                                   \
    GLboolean immediateOK;              \
    GLboolean isLockedOK;               \
    GLboolean hasBegunOK;               \
    GLboolean bufferBeginOK;            \
    GPU_CHECK_BASE(immediateOK);        \
    GPU_CHECK_IS_LOCKED(isLockedOK)     \
    GPU_CHECK_HAS_BEGUN(hasBegunOK)     \
    GPU_CHECK_BUFFER_END(bufferBeginOK) \
    }

#define GPU_CHECK_CAN_CURRENT()  \
    {                            \
    GLboolean immediateOK;       \
    GPU_CHECK_BASE(immediateOK); \
    }

#else

#define GPU_SAFE_RETURN(test, var) { (void)var; }

#define GPU_CHECK_CAN_BEGIN()
#define GPU_CHECK_CAN_END()
#define GPU_CHECK_CAN_CURRENT()

#endif



#ifdef __cplusplus
extern "C" {
#endif



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

void gpuImmediateLock(void);
void gpuImmediateUnlock(void);
GLint gpuImmediateLockCount(void);



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


#if GPU_SAFETY
	GLenum lastTexture;
#endif

	GLfloat vertex[GPU_MAX_ELEMENT_SIZE];
	GLfloat normal[3];
	GLfloat texCoord[GPU_MAX_TEXTURE_UNITS][GPU_MAX_ELEMENT_SIZE];
	GLubyte color[GPU_COLOR_COMPS]; //-V112
	GLfloat attrib_f[GPU_MAX_FLOAT_ATTRIBS][GPU_MAX_ELEMENT_SIZE];
	GLubyte attrib_ub[GPU_MAX_UBYTE_ATTRIBS][GPU_COLOR_COMPS]; //-V112

	char *restrict buffer;
	void *restrict bufferData;
	GLsizei stride;
	size_t  offset;
	GLsizei maxVertexCount;
	GLsizei count;

	int lockCount;

	void (*lockBuffer)(void);
	void (*unlockBuffer)(void);
	void (*beginBuffer)(void);
	void (*endBuffer)(void);
	void (*shutdownBuffer)(struct GPUimmediate *restrict immediate);
} GPUimmediate;

extern GPUimmediate *restrict GPU_IMMEDIATE;



GPUimmediate *restrict gpuNewImmediate(void);
void gpuImmediateMakeCurrent(GPUimmediate *restrict  immediate);
void gpuDeleteImmediate(GPUimmediate *restrict  immediate);



/* utility functions to setup vertex format and lock */
void gpuImmediateFormat_V2(void);
void gpuImmediateFormat_V3(void);
void gpuImmediateFormat_N3_V3(void);
void gpuImmediateFormat_C4_V3(void);
void gpuImmediateFormat_C4_N3_V3(void);
void gpuImmediateFormat_T2_C4_N3_V3(void);
void gpuImmediateUnformat(void);



#ifdef __cplusplus
}
#endif

#endif /* __GPU_IMMEDIATE_H_ */

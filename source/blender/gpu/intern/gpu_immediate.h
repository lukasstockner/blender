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

#include "BLI_utildefines.h"

#include "GL/glew.h"

#include <assert.h>
#include <limits.h>
#include <string.h>



/* Are restricted pointers available? (C99) */
#if (__STDC_VERSION__ < 199901L)
	/* Not a C99 compiler */
	#ifdef __GNUC__
		#define restrict __restrict__
	#else
		#define restrict /* restrict */
	#endif
#endif



#define GPU_LEGACY_DEBUG 1



#ifdef __cplusplus
extern "C" {
#endif



extern void gpuImmediateElementSizes(
	GLint vertexSize,
	GLint normalSize,
	GLint colorSize);

extern void gpuImmediateMaxVertexCount(GLsizei maxVertexCount);

extern void gpuImmediateTextureUnitCount(size_t count);
extern void gpuImmediateTexCoordSizes(const GLint *restrict sizes);
extern void gpuImmediateTextureUnitMap(const GLenum *restrict map);

extern void gpuImmediateFloatAttribCount(size_t count);
extern void gpuImmediateFloatAttribSizes(const GLint *restrict sizes);
extern void gpuImmediateFloatAttribIndexMap(const GLuint *restrict map);

extern void gpuImmediateUbyteAttribCount(size_t count);
extern void gpuImmediateUbyteAttribSizes(const GLint *restrict sizes);
extern void gpuImmediateUbyteAttribIndexMap(const GLuint *restrict map);



#define GPU_MAX_ELEMENT_SIZE   4
#define GPU_MAX_TEXTURE_UNITS 32
#define GPU_MAX_FLOAT_ATTRIBS 32
#define GPU_MAX_UBYTE_ATTRIBS 32

typedef struct GPUimmediate {
	GLenum mode;

	GLint vertexSize;
	GLint normalSize;
	GLint texCoordSize[GPU_MAX_TEXTURE_UNITS];
	GLint colorSize;
	GLint attribSize_f[GPU_MAX_FLOAT_ATTRIBS];
	GLint attribSize_ub[GPU_MAX_UBYTE_ATTRIBS];

	GLsizei maxVertexCount;

	GLenum lastTexture;

	GLenum textureUnitMap[GPU_MAX_TEXTURE_UNITS];
	size_t textureUnitCount;

	GLuint attribIndexMap_f[GPU_MAX_FLOAT_ATTRIBS];
	size_t attribCount_f;
	GLboolean attribNormalized_f[GPU_MAX_FLOAT_ATTRIBS];

	GLuint attribIndexMap_ub[GPU_MAX_UBYTE_ATTRIBS];
	size_t attribCount_ub;
	GLboolean attribNormalized_ub[GPU_MAX_UBYTE_ATTRIBS];

	GLfloat vertex[GPU_MAX_ELEMENT_SIZE];
	GLfloat normal[3];
	GLfloat texCoord[GPU_MAX_TEXTURE_UNITS][GPU_MAX_ELEMENT_SIZE];
	GLubyte color[4]; //-V112

	GLfloat attrib_f[GPU_MAX_FLOAT_ATTRIBS][GPU_MAX_ELEMENT_SIZE];
	GLubyte attrib_ub[GPU_MAX_UBYTE_ATTRIBS][4]; //-V112

	char *restrict buffer;
	void *restrict bufferData;
	size_t offset;
	GLsizei count;

	int hasBegun;

	void (*beginBuffer)(void);
	void (*endBuffer)(void);
	void (*shutdownBuffer)(struct GPUimmediate *restrict immediate);
} GPUimmediate;

extern GPUimmediate *restrict GPU_IMMEDIATE;

extern GPUimmediate* gpuNewImmediate(void);
extern void gpuImmediateMakeCurrent(GPUimmediate* immediate);
extern void gpuDeleteImmediate(GPUimmediate* immediate);

extern void gpuImmediateLegacyGetState(void);
extern void gpuImmediateLegacyPutState(void);



#define GPU_CHECK_IMMEDIATE() \
	assert(GPU_IMMEDIATE);    \
	                          \
	if (!GPU_IMMEDIATE) {     \
	    return;               \
	}

#define GPU_CHECK_NO_BEGIN()              \
	assert(!(GPU_IMMEDIATE->buffer)); \
	                                  \
	if (GPU_IMMEDIATE->buffer) {      \
	    return;                       \
	}

BLI_INLINE void gpuBegin(GLenum mode)
{
	GPU_CHECK_IMMEDIATE();
	GPU_CHECK_NO_BEGIN();

	GPU_IMMEDIATE->mode = mode;

#ifdef GPU_LEGACY_DEBUG
	gpuImmediateLegacyGetState();
#endif

	if (GPU_IMMEDIATE->beginBuffer) {
		GPU_IMMEDIATE->beginBuffer();
	}
}



BLI_INLINE void gpuColor3f(GLfloat r, GLfloat g, GLfloat b)
{
	GPU_CHECK_IMMEDIATE();

	GPU_IMMEDIATE->color[0] = (GLubyte)(255.0f * r);
	GPU_IMMEDIATE->color[1] = (GLubyte)(255.0f * g);
	GPU_IMMEDIATE->color[2] = (GLubyte)(255.0f * b);
}

BLI_INLINE void gpuColor3fv(const GLfloat *restrict v)
{
	GPU_CHECK_IMMEDIATE();

	GPU_IMMEDIATE->color[0] = (GLubyte)(255.0f * v[0]);
	GPU_IMMEDIATE->color[1] = (GLubyte)(255.0f * v[1]);
	GPU_IMMEDIATE->color[2] = (GLubyte)(255.0f * v[2]);
}

BLI_INLINE void gpuColor3ub(GLubyte r, GLubyte g, GLubyte b)
{
	GPU_CHECK_IMMEDIATE();

	GPU_IMMEDIATE->color[0] = r;
	GPU_IMMEDIATE->color[1] = g;
	GPU_IMMEDIATE->color[2] = b;
	GPU_IMMEDIATE->color[3] = 1;
}

BLI_INLINE void gpuColor3ubv(const GLubyte *restrict v)
{
	GPU_CHECK_IMMEDIATE();

	GPU_IMMEDIATE->color[0] = v[0];
	GPU_IMMEDIATE->color[1] = v[1];
	GPU_IMMEDIATE->color[2] = v[2];
	GPU_IMMEDIATE->color[3] = 1;
}

BLI_INLINE void gpuColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
	GPU_CHECK_IMMEDIATE();

	GPU_IMMEDIATE->color[0] = (GLubyte)(255.0f * r);
	GPU_IMMEDIATE->color[1] = (GLubyte)(255.0f * g);
	GPU_IMMEDIATE->color[2] = (GLubyte)(255.0f * b);
	GPU_IMMEDIATE->color[3] = (GLubyte)(255.0f * a);
}

BLI_INLINE void gpuColor4fv(const GLfloat *restrict v)
{
	GPU_CHECK_IMMEDIATE();

	GPU_IMMEDIATE->color[0] = (GLubyte)(255.0f * v[0]);
	GPU_IMMEDIATE->color[1] = (GLubyte)(255.0f * v[1]);
	GPU_IMMEDIATE->color[2] = (GLubyte)(255.0f * v[2]);
	GPU_IMMEDIATE->color[3] = (GLubyte)(255.0f * v[3]);
}

BLI_INLINE void gpuColor4ub(GLubyte r, GLubyte g, GLubyte b, GLubyte a)
{
	GPU_CHECK_IMMEDIATE();

	GPU_IMMEDIATE->color[0] = r;
	GPU_IMMEDIATE->color[1] = g;
	GPU_IMMEDIATE->color[2] = b;
	GPU_IMMEDIATE->color[3] = a;
}

BLI_INLINE void gpuColor4ubv(const GLubyte *restrict v)
{
	GPU_CHECK_IMMEDIATE();

	GPU_IMMEDIATE->color[0] = v[0];
	GPU_IMMEDIATE->color[1] = v[1];
	GPU_IMMEDIATE->color[2] = v[2];
	GPU_IMMEDIATE->color[3] = v[3];
}



BLI_INLINE void gpuNormal3f(GLfloat x, GLfloat y, GLfloat z)
{
	GPU_CHECK_IMMEDIATE();

	GPU_IMMEDIATE->normal[0] = x;
	GPU_IMMEDIATE->normal[1] = y;
	GPU_IMMEDIATE->normal[2] = z;
}

BLI_INLINE void gpuNormal3fv(const GLfloat *restrict v)
{
	GPU_CHECK_IMMEDIATE();

	GPU_IMMEDIATE->normal[0] = v[0];
	GPU_IMMEDIATE->normal[1] = v[1];
	GPU_IMMEDIATE->normal[2] = v[2];
}

BLI_INLINE void gpuNormal3sv(const GLshort *restrict v)
{
	GPU_CHECK_IMMEDIATE();

	GPU_IMMEDIATE->normal[0] = v[0] / (float)SHRT_MAX;
	GPU_IMMEDIATE->normal[1] = v[1] / (float)SHRT_MAX;
	GPU_IMMEDIATE->normal[2] = v[2] / (float)SHRT_MAX;
}



BLI_INLINE void gpuTexCoord2f(GLfloat s, GLfloat t)
{
	GPU_CHECK_IMMEDIATE();

	GPU_IMMEDIATE->texCoord[0][0] = s;
	GPU_IMMEDIATE->texCoord[0][1] = t;
	GPU_IMMEDIATE->texCoord[0][2] = 0;
	GPU_IMMEDIATE->texCoord[0][3] = 1;
}

BLI_INLINE void gpuTexCoord2fv(const GLfloat *restrict v)
{
	GPU_CHECK_IMMEDIATE();

	GPU_IMMEDIATE->texCoord[0][0] = v[0];
	GPU_IMMEDIATE->texCoord[0][1] = v[1];
	GPU_IMMEDIATE->texCoord[0][2] = 0;
	GPU_IMMEDIATE->texCoord[0][3] = 1;
}

BLI_INLINE void gpuTexCoord3fv (const GLfloat *restrict v)
{
	GPU_CHECK_IMMEDIATE();

	GPU_IMMEDIATE->texCoord[0][0] = v[0];
	GPU_IMMEDIATE->texCoord[0][1] = v[1];
	GPU_IMMEDIATE->texCoord[0][2] = v[2];
	GPU_IMMEDIATE->texCoord[0][3] = 1;
}



BLI_INLINE void gpuMultiTexCoord2f(GLenum index, GLfloat s, GLfloat t)
{
	GPU_CHECK_IMMEDIATE();

	GPU_IMMEDIATE->texCoord[index][0] = s;
	GPU_IMMEDIATE->texCoord[index][1] = t;
	GPU_IMMEDIATE->texCoord[index][2] = 0;
	GPU_IMMEDIATE->texCoord[index][3] = 1;
}

BLI_INLINE void gpuMultiTexCoord2fv(GLenum index, GLfloat *restrict v)
{
	GPU_CHECK_IMMEDIATE();

	GPU_IMMEDIATE->texCoord[index][0] = v[0];
	GPU_IMMEDIATE->texCoord[index][1] = v[1];
	GPU_IMMEDIATE->texCoord[index][2] = 0;
	GPU_IMMEDIATE->texCoord[index][3] = 1;
}

BLI_INLINE void gpuMultiTexCoord3fv(GLenum index, GLfloat *restrict v)
{
	GPU_CHECK_IMMEDIATE();

	GPU_IMMEDIATE->texCoord[index][0] = v[0];
	GPU_IMMEDIATE->texCoord[index][1] = v[1];
	GPU_IMMEDIATE->texCoord[index][2] = v[2];
	GPU_IMMEDIATE->texCoord[index][3] = 1;
}

BLI_INLINE void gpuMultiTexCoord4fv(GLenum index, GLfloat *restrict v)
{
	GPU_CHECK_IMMEDIATE();

	GPU_IMMEDIATE->texCoord[index][0] = v[0];
	GPU_IMMEDIATE->texCoord[index][1] = v[1];
	GPU_IMMEDIATE->texCoord[index][2] = v[2];
	GPU_IMMEDIATE->texCoord[index][3] = v[3];
}




BLI_INLINE void gpuVertexAttrib2fv(GLsizei index, const GLfloat *restrict v)
{
	GPU_CHECK_IMMEDIATE();

	GPU_IMMEDIATE->attrib_f[index][0] = v[0];
	GPU_IMMEDIATE->attrib_f[index][1] = v[1];
	GPU_IMMEDIATE->attrib_f[index][2] = 0;
	GPU_IMMEDIATE->attrib_f[index][3] = 1;
}

BLI_INLINE void gpuVertexAttrib3fv(GLsizei index, const GLfloat *restrict v)
{
	GPU_CHECK_IMMEDIATE();

	GPU_IMMEDIATE->attrib_f[index][0] = v[0];
	GPU_IMMEDIATE->attrib_f[index][1] = v[1];
	GPU_IMMEDIATE->attrib_f[index][2] = v[2];
	GPU_IMMEDIATE->attrib_f[index][3] = 1;
}

BLI_INLINE void gpuVertexAttrib4fv(GLsizei index, const GLfloat *restrict v)
{
	GPU_CHECK_IMMEDIATE();

	GPU_IMMEDIATE->attrib_f[index][0] = v[0];
	GPU_IMMEDIATE->attrib_f[index][1] = v[1];
	GPU_IMMEDIATE->attrib_f[index][2] = v[2];
	GPU_IMMEDIATE->attrib_f[index][3] = v[3];
}

BLI_INLINE void gpuVertexAttrib4ubv(GLsizei index, const GLubyte *restrict v)
{
	GPU_CHECK_IMMEDIATE();

	GPU_IMMEDIATE->attrib_ub[index][0] = v[0];
	GPU_IMMEDIATE->attrib_ub[index][1] = v[1];
	GPU_IMMEDIATE->attrib_ub[index][2] = v[2];
	GPU_IMMEDIATE->attrib_ub[index][3] = v[3];
}



BLI_INLINE void gpu_vertex_copy()
{
	size_t i;
	size_t size;
	GLboolean countOK = GPU_IMMEDIATE->count < GPU_IMMEDIATE->maxVertexCount;

	assert(countOK);

	if (!countOK) {
		return;
	}

	assert(GPU_IMMEDIATE->buffer);

	if (!(GPU_IMMEDIATE->buffer)) {
		return;
	}

	/* copy vertex */

	size = (size_t)(GPU_IMMEDIATE->vertexSize) * sizeof(GLfloat);

	memcpy(
		GPU_IMMEDIATE->buffer + GPU_IMMEDIATE->offset,
		GPU_IMMEDIATE->vertex, size);

	GPU_IMMEDIATE->offset += size;


	/* copy normal */

	size = (size_t)(GPU_IMMEDIATE->normalSize) * sizeof(GLfloat);

	memcpy(
		GPU_IMMEDIATE->buffer + GPU_IMMEDIATE->offset,
		GPU_IMMEDIATE->normal, size);

	GPU_IMMEDIATE->offset += size;


	/* copy color */

	/* 4 bytes are always reserved for color, for efficient memory alignment */
	size = 4 * sizeof(GLubyte);

	memcpy(
		GPU_IMMEDIATE->buffer + GPU_IMMEDIATE->offset,
		GPU_IMMEDIATE->color, size);

	GPU_IMMEDIATE->offset += size;


	/* copy texture coordinate(s) */

	for (i = 0; i < GPU_IMMEDIATE->textureUnitCount; i++) {
		size = (size_t)(GPU_IMMEDIATE->texCoordSize[i]) * sizeof(GLfloat);
		memcpy(
			GPU_IMMEDIATE->buffer + GPU_IMMEDIATE->offset,
			GPU_IMMEDIATE->texCoord[i], size);
		GPU_IMMEDIATE->offset += size;
	}

	/* copy float vertex attribute(s) */

	for (i = 0; i < GPU_IMMEDIATE->attribCount_f; i++) {
		size = (size_t)(GPU_IMMEDIATE->attribSize_f[i]) * sizeof(GLfloat);

		memcpy(
			GPU_IMMEDIATE->buffer + GPU_IMMEDIATE->offset,
			GPU_IMMEDIATE->attrib_f[i], size);

		GPU_IMMEDIATE->offset += size;
	}

	/* copy unsigned byte vertex attirbute(s) */

	/* 4 bytes are always reserved for byte attributes, for efficient memory alignment */
	size = 4 * sizeof(GLubyte);

	for (i = 0; i < GPU_IMMEDIATE->attribCount_ub; i++) {
		memcpy(
			GPU_IMMEDIATE->buffer + GPU_IMMEDIATE->offset,
			GPU_IMMEDIATE->attrib_ub[i], size);

		GPU_IMMEDIATE->offset += size;
	}

	GPU_IMMEDIATE->count++;
}

BLI_INLINE void gpuVertex2f(GLfloat x, GLfloat y)
{
	GPU_CHECK_IMMEDIATE();

	GPU_IMMEDIATE->vertex[0] = x;
	GPU_IMMEDIATE->vertex[1] = y;
	GPU_IMMEDIATE->vertex[2] = 0;
	GPU_IMMEDIATE->vertex[3] = 1;

	gpu_vertex_copy();
}

BLI_INLINE void gpuVertex2fv(const GLfloat *restrict v)
{
	GPU_CHECK_IMMEDIATE();

	GPU_IMMEDIATE->vertex[0] = v[0];
	GPU_IMMEDIATE->vertex[1] = v[1];
	GPU_IMMEDIATE->vertex[2] = 0;
	GPU_IMMEDIATE->vertex[3] = 1;

	gpu_vertex_copy();
}

BLI_INLINE void gpuVertex3f(GLfloat x, GLfloat y, GLfloat z)
{
	GPU_CHECK_IMMEDIATE();

	GPU_IMMEDIATE->vertex[0] = x;
	GPU_IMMEDIATE->vertex[1] = y;
	GPU_IMMEDIATE->vertex[2] = z;
	GPU_IMMEDIATE->vertex[3] = 1;

	gpu_vertex_copy();
 }

BLI_INLINE void gpuVertex3fv(const GLfloat *restrict v)
{
	GPU_CHECK_IMMEDIATE();

	GPU_IMMEDIATE->vertex[0] = v[0];
	GPU_IMMEDIATE->vertex[1] = v[1];
	GPU_IMMEDIATE->vertex[2] = v[2];
	GPU_IMMEDIATE->vertex[3] = 1;

	gpu_vertex_copy();
}



BLI_INLINE void gpuEnd()
{
	GPU_CHECK_IMMEDIATE();

	if (GPU_IMMEDIATE->endBuffer) {
		GPU_IMMEDIATE->endBuffer();
	}

	GPU_IMMEDIATE->buffer = NULL;
	GPU_IMMEDIATE->offset = 0;
	GPU_IMMEDIATE->count  = 0;

#ifdef GPU_LEGACY_DEBUG
	gpuImmediateLegacyPutState();
#endif
}



#ifdef __cplusplus
}
#endif



#endif /* __GPU_IMMEDIATE_H_ */

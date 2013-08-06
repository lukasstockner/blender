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
* Contributor(s): Alexandr Kuznetsov, Jason Wilkins.
*
* ***** END GPL LICENSE BLOCK *****
*/

/** \file blender/gpu/intern/gpu_immediate_gl.c
*  \ingroup gpu
*/

/* my interface */
#include "intern/gpu_immediate_gl.h"

/* internal */
#include "intern/gpu_aspect.h"
#include "intern/gpu_common.h"
#include "intern/gpu_immediate.h"
#include "intern/gpu_extension_wrapper.h"
#include "intern/gpu_glew.h"

/* my library */
#include "GPU_matrix.h"

/* external */
#include "MEM_guardedalloc.h"

/* standard */
#include <stddef.h>



#define ALIGN64(p) (((p) + 63) & ~63)



static const GLsizeiptr VQEOS_SIZE =  sizeof(GLushort) * 3 * 65536 / 2;
static const GLsizeiptr VQEOC_SIZE =  sizeof(GLubyte)  * 3 *   256 / 2;

static GLushort* vqeos;
static GLubyte*  vqeoc;

static GLuint vqeos_buf;
static GLuint vqeoc_buf;



typedef struct bufferDataGLSL {
	size_t   size;
	GLuint   vao;
	GLuint   vbo;
	GLintptr unalignedPtr;
	GLubyte* mappedBuffer;
	GLubyte* unmappedBuffer;
} bufferDataGLSL;



static GLsizei calc_stride(void)
{
	size_t              stride = 0;
	GPUimmediateformat* format = &(GPU_IMMEDIATE->format);
	size_t i;

	/* vertex */
	if (format->vertexSize != 0)
		stride += (size_t)(format->vertexSize) * sizeof(GLfloat);

	/* normal */
	if (format->normalSize != 0)
		stride += 3 * sizeof(GLfloat); /* normals always have 3 components */

	/* color */
	if (format->colorSize != 0)
		stride += 4 * sizeof(GLubyte); /* color always get 4 bytes for efficient memory alignment */

	/* texture coordinate */
	for (i = 0; i < format->texCoordCount; i++)
		stride += (size_t)(format->texCoordSize[i]) * sizeof(GLfloat);

	/* float vertex attribute */
	for (i = 0; i < format->attribCount_f; i++)
		stride += (size_t)(format->attribSize_f[i]) * sizeof(GLfloat);

	/* byte vertex attribute */
	for (i = 0; i < format->attribCount_ub; i++)
		stride += 4 * sizeof(GLubyte); /* byte attributes always get 4 bytes for efficient memory alignment */

	return (GLsizei)stride;
}



static void allocate(void)
{
	size_t newSize;

	GPU_CHECK_NO_ERROR();

	GPU_IMMEDIATE->stride = calc_stride();

	newSize = (size_t)(GPU_IMMEDIATE->stride * GPU_IMMEDIATE->maxVertexCount);

	if (GPU_IMMEDIATE->bufferData) {
		bufferDataGLSL* bufferData = (bufferDataGLSL*)GPU_IMMEDIATE->bufferData;

		if (bufferData->vbo != 0)
			gpu_glBindBuffer(GL_ARRAY_BUFFER, bufferData->vbo);

		if (newSize > bufferData->size) {
			if (bufferData->vbo != 0)
				gpu_glBufferData(GL_ARRAY_BUFFER, newSize, NULL, GL_STREAM_DRAW);

			if (bufferData->unalignedPtr != 0) {
				bufferData->unalignedPtr   = (GLintptr)MEM_reallocN((GLubyte*)(bufferData->unalignedPtr), newSize+63);
				bufferData->unmappedBuffer = (GLubyte*)ALIGN64(bufferData->unalignedPtr);
			}

			bufferData->size = newSize;
		}
	}
	else {
		bufferDataGLSL* bufferData = (bufferDataGLSL*)MEM_callocN(sizeof(bufferDataGLSL), "bufferDataGLSL");

		if (GLEW_VERSION_1_5 || GLEW_ES_VERSION_2_0 || GLEW_ARB_vertex_buffer_object) {
			gpu_glGenBuffers(1, &(bufferData->vbo));
			GPU_ASSERT(bufferData->vbo != 0);
			gpu_glBindBuffer(GL_ARRAY_BUFFER, bufferData->vbo);
			gpu_glBufferData(GL_ARRAY_BUFFER, newSize, NULL, GL_STREAM_DRAW);
		}

		if (GLEW_VERSION_1_5 || GLEW_OES_mapbuffer || GLEW_ARB_vertex_buffer_object) {
			bufferData->unalignedPtr   = 0;
			bufferData->unmappedBuffer = NULL;
		}
		else {
			bufferData->unalignedPtr   = (GLintptr)MEM_mallocN(newSize+63, "bufferDataGLSL->unalignedPtr");
			bufferData->unmappedBuffer = (void*)ALIGN64(bufferData->unalignedPtr);
		}

		bufferData->size = newSize;

		GPU_IMMEDIATE->bufferData = bufferData;
	}

	GPU_CHECK_NO_ERROR();
}



static void setup(void)
{
	GPUimmediateformat* format     = &(GPU_IMMEDIATE->format);
	const GLsizei       stride     = GPU_IMMEDIATE->stride;
	bufferDataGLSL*     bufferData = (bufferDataGLSL*)(GPU_IMMEDIATE->bufferData);
	const GLubyte*      base       = bufferData->vbo != 0 ? NULL : (GLubyte*)(bufferData->unmappedBuffer);

	size_t offset = 0;

	size_t i;

	/* vertex */
	gpu_enable_vertex_array();
	gpu_vertex_pointer(format->vertexSize, GL_FLOAT, stride, base + offset);
	offset += (size_t)(format->vertexSize) * sizeof(GLfloat);

	/* normal */
	if (format->normalSize != 0) {
		gpu_enable_normal_array();
		gpu_normal_pointer(GL_FLOAT, stride, base + offset);
		offset += 3 * sizeof(GLfloat);
	}

	/* color */
	if (format->colorSize != 0) {
		gpu_enable_color_array();
		gpu_color_pointer(format->colorSize, GL_UNSIGNED_BYTE, stride, base + offset);
		offset += 4 * sizeof(GLubyte); /* 4 bytes are always reserved for color, for efficient memory alignment */
	}

	/* texture coordinate */

	for (i = 0; i < format->texCoordCount; i++) {
		gpu_set_common_active_texture(i);
		gpu_enable_texcoord_array();
		gpu_texcoord_pointer(format->texCoordSize[i], GL_FLOAT, stride, base + offset);
		offset += (size_t)(format->texCoordSize[i]) * sizeof(GLfloat);
	}

	gpu_set_common_active_texture(0);

	/* float vertex attribute */
	for (i = 0; i < format->attribCount_f; i++) {
		if (format->attribSize_f[i] > 0) {
			gpu_vertex_attrib_pointer(
				format->attribIndexMap_f[i],
				format->attribSize_f[i],
				GL_FLOAT,
				format->attribNormalized_f[i],
				stride,
				base + offset);

			offset += (size_t)(format->attribSize_f[i]) * sizeof(GLfloat);

			gpu_enable_vertex_attrib_array(format->attribIndexMap_f[i]);
		}
	}

	/* byte vertex attribute */
	for (i = 0; i < format->attribCount_ub; i++) {
		if (format->attribSize_ub[i] > 0) {
			gpu_vertex_attrib_pointer(
				format->attribIndexMap_ub[i],
				format->attribSize_ub[i],
				GL_UNSIGNED_BYTE,
				format->attribNormalized_ub[i],
				stride,
				base + offset);

			offset += 4 * sizeof(GLubyte);

			gpu_enable_vertex_attrib_array(format->attribIndexMap_ub[i]);
		}
	}
}



static void unsetup(void)
{
	size_t i;

	/* vertex */
	gpu_disable_vertex_array();

	/* normal */
//	if (GPU_IMMEDIATE->format.normalSize != 0)
		gpu_enable_normal_array();

	/* color */
//	if (GPU_IMMEDIATE->format.colorSize != 0)
		gpu_disable_color_array();

	/* texture coordinate */

	for (i = 0; i < GPU_IMMEDIATE->format.texCoordCount; i++) {
		gpu_set_common_active_texture(i);

//		if (GPU_IMMEDIATE->format.texCoordSize[i] != 0)
			gpu_disable_texcoord_array();
	}

	gpu_set_common_active_texture(0);

	/* float vertex attribute */
	for (i = 0; i < GPU_IMMEDIATE->format.attribCount_f; i++)
//		if (GPU_IMMEDIATE->format.attribSize_f[i] > 0)
			gpu_disable_vertex_attrib_array(GPU_IMMEDIATE->format.attribIndexMap_f[i]);

	/* byte vertex attribute */
	for (i = 0; i < GPU_IMMEDIATE->format.attribCount_ub; i++)
//		if (GPU_IMMEDIATE->format.attribSize_ub[i] > 0)
			gpu_disable_vertex_attrib_array(GPU_IMMEDIATE->format.attribIndexMap_ub[i]);
}



typedef struct indexBufferDataGLSL {
	GLuint   vbo;
	GLintptr unalignedPtr;
	GLubyte* unmappedBuffer;
	GLubyte* mappedBuffer;
	size_t   size;
} indexBufferDataGLSL;

static void allocateIndex(void)
{
	if (GPU_IMMEDIATE->index) {
		GPUindex* index;
		size_t newSize;

		GPU_CHECK_NO_ERROR();

		index = GPU_IMMEDIATE->index;

		switch(index->type) {
		case GL_UNSIGNED_BYTE:
			newSize = index->maxIndexCount * sizeof(GLubyte);
			break;
		case GL_UNSIGNED_SHORT:
			newSize = index->maxIndexCount * sizeof(GLushort);
			break;
		case GL_UNSIGNED_INT:
			newSize = index->maxIndexCount * sizeof(GLuint);
			break;
		default:
			GPU_ABORT();
			return;
		}

		if (index->bufferData) {
			indexBufferDataGLSL* bufferData = (indexBufferDataGLSL*)(index->bufferData);

			if (bufferData->vbo != 0)
				gpu_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bufferData->vbo);

			if (newSize > bufferData->size) {
				if (bufferData->vbo)
					gpu_glBufferData(GL_ELEMENT_ARRAY_BUFFER, newSize, NULL, GL_STREAM_DRAW);

				if (bufferData->unalignedPtr != 0) {
					bufferData->unalignedPtr   = (GLintptr)MEM_reallocN((GLubyte*)(bufferData->unalignedPtr), newSize+63);
					bufferData->unmappedBuffer = (GLubyte*)ALIGN64(bufferData->unalignedPtr);
				}

				bufferData->size = newSize;
			}
		}
		else {
			indexBufferDataGLSL* bufferData = (indexBufferDataGLSL*)MEM_callocN(sizeof(indexBufferDataGLSL), "indexBufferDataGLSL");

			if (GLEW_VERSION_1_5 || GLEW_ES_VERSION_2_0 || GLEW_ARB_vertex_buffer_object) {
				gpu_glGenBuffers(1, &(bufferData->vbo));
				gpu_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bufferData->vbo);
				gpu_glBufferData(GL_ELEMENT_ARRAY_BUFFER, newSize, NULL, GL_STREAM_DRAW);
			}

			if (GLEW_VERSION_1_5 || GLEW_OES_mapbuffer || GLEW_ARB_vertex_buffer_object) {
				bufferData->unalignedPtr   = 0;
				bufferData->unmappedBuffer = NULL;
			}
			else {
				bufferData->unalignedPtr   = (GLintptr)MEM_mallocN(newSize+63, "indexBufferDataGLSL->unalignedPtr");
				bufferData->unmappedBuffer = (GLubyte*)ALIGN64(bufferData->unalignedPtr);
			}

			bufferData->size = newSize;

			index->bufferData = bufferData;
		}

		GPU_CHECK_NO_ERROR();
	}
}



static void static_element_array(GLuint* idOut, GLsizeiptr size, const GLvoid* indexes)
{
	gpu_glGenBuffers(1, idOut);
	gpu_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *idOut);
	gpu_glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, indexes, GL_STATIC_DRAW);
}



static void quad_elements_init(void)
{
	/* init once*/
	static char init = 0;

	int i, j;

	if (init) return;

	init = 1;

	vqeos = (GLushort*)MEM_mallocN(VQEOS_SIZE, "vqeos");

	j = 0;
	for (i = 0; i < 65535; i++) {
		vqeos[j++] = (GLushort)i;

		if (i % 4 == 3) {
			vqeos[j++] = i-3;
			vqeos[j++] = i-1;
		}
	}

	vqeoc = (GLubyte* )MEM_mallocN(VQEOC_SIZE, "vqeoc");

	for (i = 0; i < 255; i++)
		vqeoc[i] = (GLubyte)(vqeos[i]);

	if (GLEW_VERSION_1_5 || GLEW_ES_VERSION_2_0 || GLEW_ARB_vertex_buffer_object) {
		static_element_array(&vqeoc_buf, VQEOC_SIZE, vqeoc);
		static_element_array(&vqeos_buf, VQEOS_SIZE, vqeos);

		MEM_freeN(vqeoc);
		MEM_freeN(vqeos);

		vqeoc = NULL;
		vqeos = NULL;
	}
}



void gpu_lock_buffer_gl(void)
{
	allocate();
	allocateIndex();
	quad_elements_init();

	//if (gpu_glGenVertexArrays != NULL) {
	//	bufferDataGLSL* bufferData = (bufferDataGLSL*)(GPU_IMMEDIATE->bufferData);
	//	bool init = (bufferData->vao == 0);

	//	if (init)
	//		gpu_glGenVertexArrays(1, &(bufferData->vao));

	//	gpu_glBindVertexArray(bufferData->vao);

	//	if (init)
	//		setup();
	//}
	//else {
	//	setup();
	//}
}



void gpu_begin_buffer_gl(void)
{
	bufferDataGLSL* bufferData = (bufferDataGLSL*)(GPU_IMMEDIATE->bufferData);

	bufferData->mappedBuffer =
		(GLubyte*)GPU_buffer_start_update(GL_ARRAY_BUFFER, bufferData->unmappedBuffer);

	GPU_IMMEDIATE->mappedBuffer = bufferData->mappedBuffer;
}



void gpu_end_buffer_gl(void)
{
	bufferDataGLSL* bufferData = (bufferDataGLSL*)(GPU_IMMEDIATE->bufferData);

	if (bufferData->mappedBuffer != NULL) {
		GPU_buffer_finish_update(GL_ARRAY_BUFFER, GPU_IMMEDIATE->offset, bufferData->mappedBuffer);

		bufferData   ->mappedBuffer = NULL;
		GPU_IMMEDIATE->mappedBuffer = NULL;
	}

	if (!(GPU_IMMEDIATE->mode == GL_NOOP || GPU_IMMEDIATE->count == 0)) {
		GPU_CHECK_NO_ERROR();

		gpu_commit_aspect  ();
		unsetup();
		setup();
		GPU_CHECK_NO_ERROR();
		gpu_commit_matrixes();
		GPU_CHECK_NO_ERROR();
		gpu_commit_current ();
		GPU_CHECK_NO_ERROR();
		gpu_commit_samplers();
		GPU_CHECK_NO_ERROR();

		if (GPU_IMMEDIATE->mode != GL_QUADS) {
			glDrawArrays(GPU_IMMEDIATE->mode, 0, GPU_IMMEDIATE->count);
		GPU_CHECK_NO_ERROR();
		}
		else {
			if (GPU_IMMEDIATE->count <= 255){
				if (vqeoc_buf != 0)
					gpu_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vqeoc_buf);
		GPU_CHECK_NO_ERROR();

				glDrawElements(GL_TRIANGLES, 3 * GPU_IMMEDIATE->count / 2, GL_UNSIGNED_BYTE, vqeoc);
		GPU_CHECK_NO_ERROR();
			}
			else if(GPU_IMMEDIATE->count <= 65535) {
				if (vqeos_buf != 0)
					gpu_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vqeos_buf);
		GPU_CHECK_NO_ERROR();

				glDrawElements(GL_TRIANGLES, 3 * GPU_IMMEDIATE->count / 2, GL_UNSIGNED_SHORT, vqeos);
		GPU_CHECK_NO_ERROR();
			}
			else {
				printf("To big GL_QUAD object to draw. Vertices: %i", GPU_IMMEDIATE->count);
			}

			if (vqeoc_buf != 0 || vqeos_buf != 0)
				gpu_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ((indexBufferDataGLSL*)(GPU_IMMEDIATE->index->bufferData))->vbo);
		GPU_CHECK_NO_ERROR();
		}

		unsetup();
		GPU_CHECK_NO_ERROR();
	}
}



void gpu_unlock_buffer_gl(void)
{
	//bufferDataGLSL* bufferData = (bufferDataGLSL*)(GPU_IMMEDIATE->bufferData);

	//if (bufferData->vao != 0)
	//	glBindVertexArray(0);
	//else
	//	unsetup();
}



void gpu_index_shutdown_buffer_gl(GPUindex *restrict index)
{
	if (index && index->bufferData) {
		indexBufferDataGLSL* bufferData = (indexBufferDataGLSL*)(index->bufferData);

		if (bufferData->vbo != 0)
			gpu_glDeleteBuffers(1, &(bufferData->vbo));

		if (bufferData->unalignedPtr != 0)
			MEM_freeN((GLubyte*)(bufferData->unalignedPtr));

		MEM_freeN(index->bufferData);
	}
}



void gpu_shutdown_buffer_gl(GPUimmediate *restrict immediate)
{
	if (immediate->bufferData) {
		bufferDataGLSL* bufferData = (bufferDataGLSL*)(immediate->bufferData);

		if (bufferData->unalignedPtr != 0) {
			MEM_freeN((GLubyte*)(bufferData->unalignedPtr));
		}

		if (bufferData->vao != 0)
			gpu_glDeleteVertexArrays(1, &(bufferData->vao));

		if (bufferData->vbo != 0)
			gpu_glDeleteBuffers(1, &(bufferData->vbo));

		MEM_freeN(immediate->bufferData);
		immediate->bufferData = NULL;

		gpu_index_shutdown_buffer_gl(immediate->index);
	}
}



void gpu_index_begin_buffer_gl(void)
{
	GPUindex *restrict   index      = GPU_IMMEDIATE->index;
	indexBufferDataGLSL* bufferData = (indexBufferDataGLSL*)(index->bufferData);

	bufferData->mappedBuffer =
		(GLubyte*)GPU_buffer_start_update(GL_ELEMENT_ARRAY_BUFFER, bufferData->unmappedBuffer);

	index->mappedBuffer = bufferData->mappedBuffer;
}



void gpu_index_end_buffer_gl(void)
{
	GPUindex *restrict   index      = GPU_IMMEDIATE->index;
	indexBufferDataGLSL* bufferData = (indexBufferDataGLSL*)(index->bufferData);

	GPU_buffer_finish_update(GL_ELEMENT_ARRAY_BUFFER, index->offset, bufferData->mappedBuffer);

	bufferData->mappedBuffer = NULL;
	index     ->mappedBuffer = NULL;
}



void gpu_draw_elements_gl(void)
{
	GPUindex* index = GPU_IMMEDIATE->index;
	indexBufferDataGLSL* bufferData = (indexBufferDataGLSL*)(index->bufferData);

	GPU_CHECK_NO_ERROR();

	gpu_commit_aspect  ();
	unsetup();
	setup();
	gpu_commit_matrixes();
	gpu_commit_current ();
	gpu_commit_samplers();

	glDrawElements(
		GPU_IMMEDIATE->mode,
		index->count,
		index->type,
		bufferData->vbo != 0 ? NULL : bufferData->unmappedBuffer);

	unsetup();

	GPU_CHECK_NO_ERROR();
}

void gpu_draw_range_elements_gl(void)
{
	GPUindex* index = GPU_IMMEDIATE->index;
	indexBufferDataGLSL* bufferData = (indexBufferDataGLSL*)(index->bufferData);

	GPU_CHECK_NO_ERROR();

	gpu_commit_aspect  ();
	unsetup();
	setup();
	gpu_commit_matrixes();
	gpu_commit_current ();
	gpu_commit_samplers();

#if defined(WITH_GL_PROFILE_CORE) || defined(WITH_GL_PROFILE_COMPAT)
	glDrawRangeElements(
		GPU_IMMEDIATE->mode,
		index->indexMin,
		index->indexMax,
		index->count,
		index->type,
		bufferData->vbo != 0 ? NULL : bufferData->unmappedBuffer);
#else
	glDrawElements(
		GPU_IMMEDIATE->mode,
		index->count,
		index->type,
		bufferData->vbo != 0 ? NULL : bufferData->unmappedBuffer);
#endif

	unsetup();

	GPU_CHECK_NO_ERROR();
}



void gpu_commit_current(void)
{
	const GPUcommon* common = gpu_get_common();

	if (common) {
		if (GPU_IMMEDIATE->format.colorSize == 0 && common->color != -1) {
			glVertexAttrib4f(
				common->color,
				(float)(GPU_IMMEDIATE->color[0])/255.0f,
				(float)(GPU_IMMEDIATE->color[1])/255.0f,
				(float)(GPU_IMMEDIATE->color[2])/255.0f,
				(float)(GPU_IMMEDIATE->color[3])/255.0f);
		}

		if (GPU_IMMEDIATE->format.normalSize == 0 && common->normal != -1)
			glVertexAttrib3fv(common->normal, GPU_IMMEDIATE->normal);
	}

#if defined(WITH_GL_PROFILE_COMPAT)
	glColor4ubv(GPU_IMMEDIATE->color);
	glNormal3fv(GPU_IMMEDIATE->normal);
#endif
}



void gpu_commit_samplers(void)
{
	const GPUcommon* common = gpu_get_common();

	if (common) {
		glUniform1iv(
			common->sampler[0],
			GPU_IMMEDIATE->format.samplerCount,
			GPU_IMMEDIATE->format.samplerMap);
	}
}

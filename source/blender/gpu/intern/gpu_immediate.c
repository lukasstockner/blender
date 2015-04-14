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

#include "BLI_sys_types.h"

#include "GPU_matrix.h"
#include "GPU_aspect.h"
#include "GPU_extensions.h"
#include "GPU_immediate.h"
#include "intern/gpu_private.h"

/* external */
#include "MEM_guardedalloc.h"

/* standard */
#include <string.h>

#define ALIGN64(p) ((((uintptr_t)p) + 63) & ~63)

typedef struct GPUVertexStream {
	/* type of stream (array buffer/element array buffer) */
	int type;

	/* size of buffer */
	size_t size;

	/* bind buffers to their attribute slots */
	void *(*bind)(struct GPUVertexStream *);
	/* unbind the buffers from their attribute slots */
	void (*unbind)(struct GPUVertexStream *);

	/* map the buffer - will give pointer to user that can be used to
	 * fill the buffer. Pointer will be placed in mappedBuffer */
	GLubyte *(*map)(struct GPUVertexStream *);
	void (*unmap)(struct GPUVertexStream *);

	void (*alloc)(struct GPUVertexStream *stream, size_t newsize);
	void (*free)(struct GPUVertexStream *stream);
} GPUVertexStream;

typedef struct GPUVertexBufferStream {
	GPUVertexStream stream;
	GLuint vbo;
} GPUVertexBufferStream;

typedef struct GPUVertexArrayStream {
	GPUVertexBufferStream vstream;
	GLuint vao;
} GPUVertexArrayStream;

typedef struct GPURAMArrayStream {
	GPUVertexStream stream;
	void *unalignedPtr;
	GLubyte *unmappedBuffer;
} GPURAMArrayStream;

enum StreamTypes {
	eStreamTypeVertexArray  = 0,
	eStreamTypeRAM          = 1,
	eStreamTypeVertexBuffer = 2,
};

static void alloc_stream_ram(GPUVertexStream *stream, size_t newsize)
{
	if (newsize > stream->size) {
		GPURAMArrayStream *ram_stream = (GPURAMArrayStream *)stream;
		if (ram_stream->unalignedPtr != 0) {
			ram_stream->unalignedPtr   = MEM_reallocN((GLubyte*)(ram_stream->unalignedPtr), newsize+63);
		}
		else {
			ram_stream->unalignedPtr   = MEM_mallocN(newsize + 63, "vertex_stream_ram");
		}
		ram_stream->unmappedBuffer = (GLubyte*)ALIGN64(ram_stream->unalignedPtr);
		stream->size = newsize;
	}
}

static void alloc_stream_vbuffer(GPUVertexStream *stream, size_t newsize)
{
	if (newsize > stream->size) {
		GPUVertexBufferStream *va_stream = (GPUVertexBufferStream *)stream;

		if (!va_stream->vbo)
			glGenBuffers(1, &va_stream->vbo);
		glBindBuffer(stream->type, va_stream->vbo);
		glBufferData(stream->type, newsize, NULL, GL_STREAM_DRAW);
		stream->size = newsize;
	}
}

static void alloc_stream_vabuffer(GPUVertexStream *stream, size_t newsize)
{
	if (newsize > stream->size) {
		GPUVertexArrayStream *va_stream = (GPUVertexArrayStream *)stream;

		if (!va_stream->vao)
			glGenVertexArrays(1, &va_stream->vao);

		if (!va_stream->vstream.vbo)
			glGenBuffers(1, &va_stream->vstream.vbo);
		glBindBuffer(stream->type, va_stream->vstream.vbo);
		glBufferData(stream->type, newsize, NULL, GL_STREAM_DRAW);
		stream->size = newsize;
	}
}


static GLubyte *map_stream_ram(GPUVertexStream *stream)
{
	GPURAMArrayStream *ram_stream = (GPURAMArrayStream *)stream;
	return ram_stream->unmappedBuffer;
}

static GLubyte *map_stream_vbuffer(GPUVertexStream *stream)
{
	GPUVertexBufferStream *va_stream = (GPUVertexBufferStream *)stream;
	glBindBuffer(stream->type, va_stream->vbo);
	return glMapBufferARB(stream->type, GL_WRITE_ONLY);
}

static void unmap_stream_ram(GPUVertexStream *UNUSED(stream))
{
}

static void unmap_stream_vbuffer(GPUVertexStream *stream)
{
	GPUVertexBufferStream *va_stream = (GPUVertexBufferStream *)stream;
	glBindBuffer(stream->type, va_stream->vbo);
	glUnmapBufferARB(stream->type);
}

static void free_stream_ram(GPUVertexStream *stream)
{
	GPURAMArrayStream *ram_stream = (GPURAMArrayStream *)stream;
	if (ram_stream->unalignedPtr)
		MEM_freeN(ram_stream->unalignedPtr);
	MEM_freeN(stream);
}

static void free_stream_varray(GPUVertexStream *stream)
{
	GPUVertexArrayStream *va_stream = (GPUVertexArrayStream *)stream;
	if (va_stream->vao != 0)
		glDeleteVertexArrays(1, &va_stream->vao);

	if (va_stream->vstream.vbo != 0)
		glDeleteBuffers(1, &va_stream->vstream.vbo);

	MEM_freeN(stream);
}

static void free_stream_vbuffer(GPUVertexStream *stream)
{
	GPUVertexBufferStream *va_stream = (GPUVertexBufferStream *)stream;

	if (va_stream->vbo != 0)
		glDeleteBuffers(1, &va_stream->vbo);

	MEM_freeN(stream);
}

static void *bind_stream_ram(GPUVertexStream *stream)
{
	GPURAMArrayStream *ram_stream = (GPURAMArrayStream *)stream;
	return ram_stream->unmappedBuffer;
}

static void unbind_stream_ram(GPUVertexStream *UNUSED(stream))
{
}

static void *bind_stream_vbuffer(GPUVertexStream *stream)
{
	GPUVertexBufferStream *va_stream = (GPUVertexBufferStream *)stream;
	glBindBuffer(stream->type, va_stream->vbo);
	return NULL;
}

static void unbind_stream_vbuffer(GPUVertexStream *stream)
{
	glBindBuffer(stream->type, 0);
}

static void *bind_stream_varray(GPUVertexStream *stream)
{
	GPUVertexArrayStream *va_stream = (GPUVertexArrayStream *)stream;
	glBindVertexArray(va_stream->vao);
	glBindBuffer(stream->type, va_stream->vstream.vbo);
	return NULL;
}

static void unbind_stream_varray(GPUVertexStream *stream)
{
	glBindVertexArray(0);
	glBindBuffer(stream->type, 0);
}

static GPUVertexStream *gpu_new_vertex_stream(enum StreamTypes type, int array_type)
{
	GPUVertexStream *ret;
	switch (type) {
		case eStreamTypeVertexArray:
		{
			GPUVertexArrayStream *stream = MEM_callocN(sizeof(GPUVertexArrayStream), "GPUVertexArrayStream");
			ret = &stream->vstream.stream;
			ret->alloc = alloc_stream_vabuffer;
			ret->map = map_stream_vbuffer;
			ret->unmap = unmap_stream_vbuffer;
			ret->free = free_stream_varray;
			ret->bind = bind_stream_varray;
			ret->unbind = unbind_stream_varray;
			break;
		}

		case eStreamTypeRAM:
		{
			GPURAMArrayStream *stream = MEM_callocN(sizeof(GPURAMArrayStream), "GPURAMArrayStream");
			ret = &stream->stream;
			ret->alloc = alloc_stream_ram;
			ret->map = map_stream_ram;
			ret->unmap = unmap_stream_ram;
			ret->free = free_stream_ram;
			ret->bind = bind_stream_ram;
			ret->unbind = unbind_stream_ram;
			break;
		}

		case eStreamTypeVertexBuffer:
		{
			GPUVertexBufferStream *stream = MEM_callocN(sizeof(GPUVertexBufferStream), "GPUVertexBufferStream");
			ret = &stream->stream;
			ret->alloc = alloc_stream_vbuffer;
			ret->map = map_stream_vbuffer;
			ret->unmap = unmap_stream_vbuffer;
			ret->free = free_stream_vbuffer;
			ret->bind = bind_stream_vbuffer;
			ret->unbind = unbind_stream_vbuffer;
			break;
		}

		default:
			ret = NULL;
			break;
	}

	if (ret) {
		ret->type = array_type;
	}

	return ret;
}

static GLsizei calc_stride(void)
{
	size_t stride = 0;
	GPUImmediateFormat *format = &(GPU_IMMEDIATE->format);
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
	GPUVertexStream *vertex_stream;

	GPU_ASSERT_NO_GL_ERRORS("allocate start");

	GPU_IMMEDIATE->stride = calc_stride();

	newSize = (size_t)(GPU_IMMEDIATE->stride * GPU_IMMEDIATE->maxVertexCount);

	if (!GPU_IMMEDIATE->vertex_stream) {
		GPU_IMMEDIATE->vertex_stream = gpu_new_vertex_stream(eStreamTypeRAM, GL_ARRAY_BUFFER);
	}

	vertex_stream = (GPUVertexStream*)GPU_IMMEDIATE->vertex_stream;
	vertex_stream->alloc(vertex_stream, newSize);

	GPU_ASSERT_NO_GL_ERRORS("allocate end");
}



static void setup(void)
{
	GPUImmediateFormat *format         = &(GPU_IMMEDIATE->format);
	const GLsizei      stride         = GPU_IMMEDIATE->stride;
	GPUVertexStream    *vertex_stream = (GPUVertexStream*)(GPU_IMMEDIATE->vertex_stream);
	const GLubyte      *base          = vertex_stream->bind(vertex_stream);

	size_t offset = 0;

	size_t i;

	/* vertex */
	GPU_common_enable_vertex_array();
	GPU_common_vertex_pointer(format->vertexSize, GL_FLOAT, stride, base + offset);
	offset += (size_t)(format->vertexSize) * sizeof(GLfloat);

	/* normal */
	if (format->normalSize != 0) {
		GPU_common_enable_normal_array();
		GPU_common_normal_pointer(GL_FLOAT, stride, GL_FALSE, base + offset);
		offset += 3 * sizeof(GLfloat);
	}

	/* color */
	if (format->colorSize != 0) {
		GPU_common_enable_color_array();
		GPU_common_color_pointer(format->colorSize, GL_UNSIGNED_BYTE, stride, base + offset);
		offset += 4 * sizeof(GLubyte); /* 4 bytes are always reserved for color, for efficient memory alignment */
	}

	/* texture coordinate */

	for (i = 0; i < format->texCoordCount; i++) {
		GPU_set_common_active_texture(i);
		GPU_common_enable_texcoord_array();
		GPU_common_texcoord_pointer(format->texCoordSize[i], GL_FLOAT, stride, base + offset);
		offset += (size_t)(format->texCoordSize[i]) * sizeof(GLfloat);
	}

	GPU_set_common_active_texture(0);

	/* float vertex attribute */
	for (i = 0; i < format->attribCount_f; i++) {
		if (format->attribSize_f[i] > 0) {
			glVertexAttribPointer(
				format->attribIndexMap_f[i],
				format->attribSize_f[i],
				GL_FLOAT,
				format->attribNormalized_f[i],
				stride,
				base + offset);

			offset += (size_t)(format->attribSize_f[i]) * sizeof(GLfloat);

			glEnableVertexAttribArray(format->attribIndexMap_f[i]);
		}
	}

	/* byte vertex attribute */
	for (i = 0; i < format->attribCount_ub; i++) {
		if (format->attribSize_ub[i] > 0) {
			glVertexAttribPointer(
				format->attribIndexMap_ub[i],
				format->attribSize_ub[i],
				GL_UNSIGNED_BYTE,
				format->attribNormalized_ub[i],
				stride,
				base + offset);

			offset += 4 * sizeof(GLubyte);

			glEnableVertexAttribArray(format->attribIndexMap_ub[i]);
		}
	}
}



static void unsetup(void)
{
	size_t i;
	GPUVertexStream *vertex_stream = (GPUVertexStream*)(GPU_IMMEDIATE->vertex_stream);

	/* vertex */
	GPU_common_disable_vertex_array();

	/* normal */
//	if (GPU_IMMEDIATE->format.normalSize != 0)
		GPU_common_disable_normal_array();

	/* color */
//	if (GPU_IMMEDIATE->format.colorSize != 0)
		GPU_common_disable_color_array();

	/* texture coordinate */

	for (i = 0; i < GPU_IMMEDIATE->format.texCoordCount; i++) {
		GPU_set_common_active_texture(i);

//		if (GPU_IMMEDIATE->format.texCoordSize[i] != 0)
			GPU_common_disable_texcoord_array();
	}

	GPU_set_common_active_texture(0);

	/* float vertex attribute */
	for (i = 0; i < GPU_IMMEDIATE->format.attribCount_f; i++)
//		if (GPU_IMMEDIATE->format.attribSize_f[i] > 0)
			glDisableVertexAttribArray(GPU_IMMEDIATE->format.attribIndexMap_f[i]);

	/* byte vertex attribute */
	for (i = 0; i < GPU_IMMEDIATE->format.attribCount_ub; i++)
//		if (GPU_IMMEDIATE->format.attribSize_ub[i] > 0)
			glDisableVertexAttribArray(GPU_IMMEDIATE->format.attribIndexMap_ub[i]);

	vertex_stream->unbind(vertex_stream);
}

static void allocateIndex(void)
{
	if (GPU_IMMEDIATE->index) {
		GPUindex *index;
		size_t newSize;
		GPUVertexStream *element_stream;

		GPU_ASSERT_NO_GL_ERRORS("allocateIndex start");

		index = GPU_IMMEDIATE->index;

		switch (index->type) {
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
				GPU_print_error_debug("allocateIndex, unknown type");
				return;
		}

		if (!index->element_stream) {
			index->element_stream = gpu_new_vertex_stream(eStreamTypeRAM, GL_ELEMENT_ARRAY_BUFFER);
		}

		element_stream = index->element_stream;
		element_stream->alloc(element_stream, newSize);

		GPU_ASSERT_NO_GL_ERRORS("allocateIndex end");
	}
}



static void static_element_array(GLuint *idOut, GLsizeiptr size, const GLvoid *indexes)
{
	glGenBuffers(1, idOut);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *idOut);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, indexes, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}



/* quad emulation */

static bool quad_init = false;


static const GLsizeiptr VQEOS_SIZE = sizeof(GLushort) * 3 * 65536 / 2;
static const GLsizeiptr VQEOC_SIZE = sizeof(GLubyte)  * 3 *   256 / 2;

static GLushort *vqeos;
static GLubyte  *vqeoc;

static GLuint vqeos_buf;
static GLuint vqeoc_buf;

static void quad_free_heap(void)
{
	if (vqeoc) {
		MEM_freeN(vqeoc);
		vqeoc = NULL;
	}

	if (vqeos) {
		MEM_freeN(vqeos);
		vqeos = NULL;
	}
}



static void quad_elements_init(void)
{
	int i, j;

	BLI_assert(!quad_init);

	vqeos = (GLushort*)MEM_mallocN(VQEOS_SIZE, "vqeos");

	j = 0;
	for (i = 0; i < 65535; i++) {
		vqeos[j++] = (GLushort)i;

		if (i % 4 == 3) {
			vqeos[j++] = i-3;
			vqeos[j++] = i-1;
		}
	}

	vqeoc = (GLubyte*)MEM_mallocN(VQEOC_SIZE, "vqeoc");

	for (i = 0; i < 255; i++)
		vqeoc[i] = (GLubyte)(vqeos[i]);

	if (GLEW_ARB_vertex_buffer_object) {
		static_element_array(&vqeoc_buf, VQEOC_SIZE, vqeoc);
		static_element_array(&vqeos_buf, VQEOS_SIZE, vqeos);

		quad_free_heap();
	}

	quad_init = true;
}



static void quad_elements_exit(void)
{
	quad_free_heap();

	if (vqeoc_buf != 0) {
		glDeleteBuffers(1, &vqeoc_buf);
		vqeoc_buf = 0;
	}

	if (vqeos_buf != 0) {
		glDeleteBuffers(1, &vqeos_buf);
		vqeos_buf = 0;
	}

	quad_init = false;
}


void gpu_immediate_init(void)
{
	quad_elements_init();
}



void gpu_immediate_exit(void)
{
	quad_elements_exit();
}



void gpu_lock_buffer_gl(void)
{
	allocate();
	allocateIndex();

	/*
	if (GLEW_ARB_vertex_buffer_object) {
		bufferDataGLSL *bufferData = (bufferDataGLSL*)(GPU_IMMEDIATE->vertex_stream);
		bool do_init = (bufferData->vao == 0);

		if (do_init)
			glGenVertexArrays(1, &(bufferData->vao));

		glBindVertexArray(bufferData->vao);

		if (do_init)
			setup();

	}
	else {
	*/
		setup();
	//}
}



void gpu_begin_buffer_gl(void)
{
	GPUVertexStream *stream = (GPUVertexStream*)(GPU_IMMEDIATE->vertex_stream);

	if (stream == NULL) {
		allocate();
		stream = (GPUVertexStream*)(GPU_IMMEDIATE->vertex_stream);
	}

	GPU_IMMEDIATE->mappedBuffer = stream->map(stream);
}



void gpu_end_buffer_gl(void)
{
	GPUVertexStream *stream = (GPUVertexStream*)(GPU_IMMEDIATE->vertex_stream);

	GPU_ASSERT_NO_GL_ERRORS("gpu_end_buffer_gl start");

	stream->unmap(stream);
	GPU_IMMEDIATE->mappedBuffer = NULL;

	if (!(GPU_IMMEDIATE->mode == GL_NOOP || GPU_IMMEDIATE->count == 0)) {
		if (!GPU_commit_aspect())
			return;

		unsetup();
		setup();
		gpu_commit_current();
		gpu_commit_samplers();

		if (GPU_IMMEDIATE->mode != GL_QUADS) {
			glDrawArrays(GPU_IMMEDIATE->mode, 0, GPU_IMMEDIATE->count);
			GPU_ASSERT_NO_GL_ERRORS("gpu_end_buffer_gl afterdraw");
		}
		else {
			if (GPU_IMMEDIATE->count <= 255) {
				if (vqeoc_buf != 0)
					glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vqeoc_buf);

				glDrawElements(GL_TRIANGLES, 3 * GPU_IMMEDIATE->count / 2, GL_UNSIGNED_BYTE, vqeoc);
				GPU_ASSERT_NO_GL_ERRORS("gpu_end_buffer_gl afterdraw");
			}
			else if (GPU_IMMEDIATE->count <= 65535) {
				if (vqeos_buf != 0)
					glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vqeos_buf);

				glDrawElements(GL_TRIANGLES, 3 * GPU_IMMEDIATE->count / 2, GL_UNSIGNED_SHORT, vqeos);
				GPU_ASSERT_NO_GL_ERRORS("gpu_end_buffer_gl afterdraw");
			}
			else {
				printf("Too big GL_QUAD object to draw. Vertices: %i", GPU_IMMEDIATE->count);
			}

			if (vqeoc_buf != 0 || vqeos_buf != 0) {
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
			}

			GPU_ASSERT_NO_GL_ERRORS("gpu_end_buffer_gl end");
		}

		unsetup();
		GPU_shader_unbind();
		GPU_ASSERT_NO_GL_ERRORS("gpu_end_buffer_gl end");
	}
}



void gpu_unlock_buffer_gl(void)
{
	unsetup();
}



void gpu_index_shutdown_buffer_gl(GPUindex *index)
{
	if (index && index->element_stream) {
		GPUVertexStream *stream = (GPUVertexStream*)(index->element_stream);

		stream->free(stream);
	}
}



void gpu_shutdown_buffer_gl(GPUImmediate *immediate)
{
	if (immediate->vertex_stream) {
		GPUVertexStream *stream = (GPUVertexStream*)(immediate->vertex_stream);

		stream->free(stream);

		immediate->vertex_stream = NULL;

		gpu_index_shutdown_buffer_gl(immediate->index);
	}
}



void gpu_index_begin_buffer_gl(void)
{
	GPUindex *index = GPU_IMMEDIATE->index;
	GPUVertexStream *stream = (GPUVertexStream*)(index->element_stream);

	if (!stream) {
		allocateIndex();
		stream = (GPUVertexStream*)(index->element_stream);
	}

	index->mappedBuffer = stream->map(stream);
}



void gpu_index_end_buffer_gl(void)
{
	GPUindex *index = GPU_IMMEDIATE->index;
	GPUVertexStream *stream = (GPUVertexStream*)(index->element_stream);

	stream->unmap(stream);

	index->mappedBuffer = NULL;
}



void gpu_draw_elements_gl(void)
{
	GPUindex *index = GPU_IMMEDIATE->index;
	GPUVertexStream *element_stream = (GPUVertexStream*)index->element_stream;
	void *base = element_stream->bind(element_stream);

	GPU_ASSERT_NO_GL_ERRORS("gpu_draw_elements_gl start");;

	if (!GPU_commit_aspect())
		return;

	unsetup();
	setup();
	gpu_commit_current();
	gpu_commit_samplers();

	glDrawElements(
		GPU_IMMEDIATE->mode,
		index->count,
		index->type,
		base);

	unsetup();
	GPU_shader_unbind();
	element_stream->unbind(element_stream);

	GPU_ASSERT_NO_GL_ERRORS("gpu_draw_elements_gl end");
}

void gpu_draw_range_elements_gl(void)
{
	GPUindex *index = GPU_IMMEDIATE->index;
	GPUVertexStream *element_stream = (GPUVertexStream*)index->element_stream;
	void *base = element_stream->bind(element_stream);

	GPU_ASSERT_NO_GL_ERRORS("gpu_draw_range_elements_gl start");;

	if (!GPU_commit_aspect())
		return;

	unsetup();
	setup();
	gpu_commit_current();
	gpu_commit_samplers();

#if defined(WITH_GL_PROFILE_CORE) || defined(WITH_GL_PROFILE_COMPAT)
	glDrawRangeElements(
		GPU_IMMEDIATE->mode,
		index->indexMin,
		index->indexMax,
		index->count,
		index->type,
		base);
#else
	glDrawElements(
		GPU_IMMEDIATE->mode,
		index->count,
		index->type,
		bufferData->vbo != 0 ? NULL : bufferData->unmappedBuffer);
#endif

	unsetup();
	GPU_shader_unbind();
	element_stream->unbind(element_stream);

	GPU_ASSERT_NO_GL_ERRORS("gpu_draw_range_elements_gl end");;
}



void gpu_commit_current(void)
{
	if (GPU_IMMEDIATE->format.colorSize == 0)
		GPU_common_color_4ubv(GPU_IMMEDIATE->color);

	if (GPU_IMMEDIATE->format.normalSize == 0)
		GPU_common_normal_3fv(GPU_IMMEDIATE->normal);
}



void gpu_commit_samplers(void)
{
	const struct GPUcommon *common = gpu_get_common();

	if (common) {
		GPU_ASSERT_NO_GL_ERRORS("gpu_commit_samplers start");

		glUniform1iv(
			common->sampler[0],
			GPU_IMMEDIATE->format.samplerCount,
			GPU_IMMEDIATE->format.samplerMap);

		GPU_ASSERT_NO_GL_ERRORS("gpu_commit_samplers end");
	}
}

#ifdef GPU_SAFETY

/* Define some useful, but potentially slow, checks for correct API usage. */

/* Each block contains variables that can be inspected by a
 * debugger in the event that a break point is triggered. */

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

/* XXX jwilkins: make this assert prettier */
#define GPU_SAFE_STMT(var, test, stmt) \
    var = (GLboolean)(test);           \
    BLI_assert(((void)#test, var));    \
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
GPUImmediate *GPU_IMMEDIATE = NULL;

void gpuBegin(GLenum mode)
{
	int primMod;

	GPU_CHECK_CAN_BEGIN();

#ifdef GPU_SAFETY
	GPU_IMMEDIATE->hasOverflowed = GL_FALSE;
#endif

	GPU_IMMEDIATE->mode   = mode;
	GPU_IMMEDIATE->offset = 0;
	GPU_IMMEDIATE->count  = 0;

	switch (mode) {
		case GL_LINES:
			primMod = 2;
			break;

		case GL_QUAD_STRIP:
		case GL_TRIANGLE_STRIP:
			primMod = 2;
			break;

		case GL_TRIANGLES:
			primMod = 3;
			break;

		case GL_QUADS:
			primMod = 4;
			break;

		default:
			primMod = 1;
			break;
	}

	GPU_IMMEDIATE->lastPrimVertex = GPU_IMMEDIATE->maxVertexCount - (GPU_IMMEDIATE->maxVertexCount % primMod);

	gpu_begin_buffer_gl();
}

void gpuEnd(void)
{
	GPU_CHECK_CAN_END();
	BLI_assert(GPU_IMMEDIATE->mode != GL_NOOP || !(GPU_IMMEDIATE->hasOverflowed));

	gpu_end_buffer_gl();
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
	BLI_assert(GPU_IMMEDIATE);

	if (!GPU_IMMEDIATE) {
		return GL_FALSE;
	}

	return GPU_IMMEDIATE->lockCount;
}



static void gpu_copy_vertex(void);



GPUImmediate *gpuNewImmediate(void)
{
	GPUImmediate *immediate =
		(GPUImmediate*)MEM_callocN(sizeof(GPUImmediate), "GPUimmediate");

	immediate->copyVertex = gpu_copy_vertex;

#ifdef GPU_SAFETY
	immediate->lastTexture = GPU_max_textures() - 1;
#endif

	return immediate;
}



void gpuImmediateMakeCurrent(GPUImmediate *immediate)
{
	GPU_IMMEDIATE = immediate;
}



void gpuDeleteImmediate(GPUImmediate *immediate)
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



void gpuImmediateTexCoordSizes(const GLint *sizes)
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

void gpuImmediateSamplerMap(const GLint *map)
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

void gpuImmediateFloatAttribSizes(const GLint *sizes)
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

void gpuImmediateFloatAttribIndexMap(const GLuint *map)
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

void gpuImmediateUbyteAttribSizes(const GLint *sizes)
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

void gpuImmediateUbyteAttribIndexMap(const GLuint *map)
{
	size_t i;

	GPU_CHECK_CAN_SETUP();

	for (i = 0; i < GPU_IMMEDIATE->format.attribCount_ub; i++) {
		GPU_IMMEDIATE->format.attribIndexMap_ub[i] = map[i];
	}
}

static GLboolean end_begin(void)
{
#ifdef GPU_SAFETY
	GPU_IMMEDIATE->hasOverflowed = GL_TRUE;
#endif

	if (!ELEM(
			GPU_IMMEDIATE->mode,
			GL_NOOP,
			GL_LINE_LOOP,
			GL_POLYGON,
			GL_QUAD_STRIP,
			GL_LINE_STRIP,
			GL_TRIANGLE_STRIP)) /* XXX jwilkins: can restart some of these, but need to put in the logic (could be problematic with mapped VBOs?) */
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

static void gpu_copy_vertex(void)
{
	size_t i;
	size_t size;
	size_t offset;
	GLubyte *mappedBuffer;

#ifdef GPU_SAFETY
	{
	int maxVertexCountOK;
	GPU_SAFE_RETURN(GPU_IMMEDIATE->maxVertexCount != 0, maxVertexCountOK,);
	}
#endif

	if (GPU_IMMEDIATE->count == GPU_IMMEDIATE->lastPrimVertex) {
		GLboolean restarted;

		restarted = end_begin(); /* draw and clear buffer */

		BLI_assert(restarted);

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
		memcpy(mappedBuffer + offset, GPU_IMMEDIATE->normal, 3 * sizeof(GLfloat));
		offset += 3 * sizeof(GLfloat);
	}

	/* color */
	if (GPU_IMMEDIATE->format.colorSize != 0) {
		/* 4 bytes are always reserved for color, for efficient memory alignment */
		memcpy(mappedBuffer + offset, GPU_IMMEDIATE->color, 4 * sizeof(GLubyte));
		offset += 4 * sizeof(GLubyte);
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
		memcpy(mappedBuffer + offset, GPU_IMMEDIATE->attrib_ub[i], 4 * sizeof(GLubyte));
		offset += 4 * sizeof(GLubyte);
	}

	GPU_IMMEDIATE->offset = offset;
}

/* vertex formats */

#ifdef GPU_SAFETY
void gpuSafetyImmediateFormat_V2(const char *file, int line)
#else
void gpuImmediateFormat_V2(void)
#endif
{
#ifdef GPU_SAFETY
	printf("%s(%d): gpuImmediateFormat_V2\n", file, line);
#endif

	if (gpuImmediateLockCount() == 0) {
		gpuImmediateFormatReset();

		gpuImmediateElementSizes(2, 0, 0);
	}

	gpuImmediateLock();
}

#ifdef GPU_SAFETY
void gpuSafetyImmediateFormat_C4_V2(const char *file, int line)
#else
void gpuImmediateFormat_C4_V2(void)
#endif
{
#ifdef GPU_SAFETY
	printf("%s(%d): gpuImmediateFormat_C4_V2\n", file, line);
#endif

	if (gpuImmediateLockCount() == 0) {
		gpuImmediateFormatReset();

		gpuImmediateElementSizes(2, 0, 4); //-V112
	}

	gpuImmediateLock();
}

#ifdef GPU_SAFETY
void gpuSafetyImmediateFormat_T2_V2(const char *file, int line)
#else
void gpuImmediateFormat_T2_V2(void)
#endif
{
#ifdef GPU_SAFETY
	printf("%s(%d): gpuImmediateFormat_T2_V2\n", file, line);
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

#ifdef GPU_SAFETY
void gpuSafetyImmediateFormat_T2_V3(const char *file, int line)
#else
void gpuImmediateFormat_T2_V3(void)
#endif
{
#ifdef GPU_SAFETY
	printf("%s(%d): gpuImmediateFormat_T2_V3\n", file, line);
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

#ifdef GPU_SAFETY
void gpuSafetyImmediateFormat_T2_C4_V2(const char *file, int line)
#else
void gpuImmediateFormat_T2_C4_V2(void)
#endif
{
#ifdef GPU_SAFETY
	printf("%s(%d): gpuImmediateFormat_T2_C4_V2\n", file, line);
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

#ifdef GPU_SAFETY
void gpuSafetyImmediateFormat_V3(const char *file, int line)
#else
void gpuImmediateFormat_V3(void)
#endif
{
#ifdef GPU_SAFETY
	printf("%s(%d): gpuImmediateFormat_V3\n", file, line);
#endif

	if (gpuImmediateLockCount() == 0) {
		gpuImmediateFormatReset();

		gpuImmediateElementSizes(3, 0, 0);
	}

	gpuImmediateLock();
}

#ifdef GPU_SAFETY
void gpuSafetyImmediateFormat_N3_V3(const char *file, int line)
#else
void gpuImmediateFormat_N3_V3(void)
#endif
{
#ifdef GPU_SAFETY
	printf("%s(%d): gpuImmediateFormat_N3_V3\n", file, line);
#endif

	if (gpuImmediateLockCount() == 0) {
		gpuImmediateFormatReset();

		gpuImmediateElementSizes(3, 3, 0);
	}

	gpuImmediateLock();
}

#ifdef GPU_SAFETY
void gpuSafetyImmediateFormat_C4_V3(const char *file, int line)
#else
void gpuImmediateFormat_C4_V3(void)
#endif
{
#ifdef GPU_SAFETY
	printf("%s(%d): gpuImmediateFormat_C4_V3\n", file, line);
#endif

	if (gpuImmediateLockCount() == 0) {
		gpuImmediateFormatReset();

		gpuImmediateElementSizes(3, 0, 4); //-V112
	}

	gpuImmediateLock();
}

#ifdef GPU_SAFETY
void gpuSafetyImmediateFormat_C4_N3_V3(const char *file, int line)
#else
void gpuImmediateFormat_C4_N3_V3(void)
#endif
{
#ifdef GPU_SAFETY
	printf("%s(%d): gpuImmediateFormat_C4_N3_V3\n", file, line);
#endif

	if (gpuImmediateLockCount() == 0) {
		gpuImmediateFormatReset();

		gpuImmediateElementSizes(3, 3, 4); //-V112
	}

	gpuImmediateLock();
}

#ifdef GPU_SAFETY
void gpuSafetyImmediateFormat_T2_C4_N3_V3(const char *file, int line)
#else
void gpuImmediateFormat_T2_C4_N3_V3(void)
#endif
{
#ifdef GPU_SAFETY
	printf("%s(%d): gpuImmediateFormat_T2_C4_N3_V3\n", file, line);
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

#ifdef GPU_SAFETY
void gpuSafetyImmediateFormat_T3_C4_V3(const char *file, int line)
#else
void gpuImmediateFormat_T3_C4_V3(void)
#endif
{
#ifdef GPU_SAFETY
	printf("%s(%d): gpuImmediateFormat_T3_C4_V3\n", file, line);
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

#ifdef GPU_SAFETY
void gpuSafetyImmediateUnformat(const char *file, int line)
#else
void gpuImmediateUnformat(void)
#endif
{
#ifdef GPU_SAFETY
	printf("%s(%d): gpuImmediateUnformat\n", file, line);
#endif

	gpuImmediateUnlock();
}

static GPUImmediate *immediateStack = NULL; /* stack size of one */

void gpuPushImmediate(void)
{
	GPUImmediate *newImmediate;

	GPU_CHECK_CAN_PUSH();

	newImmediate   = gpuNewImmediate();
	immediateStack = GPU_IMMEDIATE;
	GPU_IMMEDIATE  = newImmediate;
}

GPUImmediate *gpuPopImmediate(void)
{
	GPUImmediate *newImmediate;

	GPU_CHECK_CAN_POP();

	newImmediate = GPU_IMMEDIATE;
	GPU_IMMEDIATE = immediateStack;
	immediateStack = NULL;

	return newImmediate;
}

static void gpu_append_client_arrays(
	const GPUarrays *arrays,
	GLint first,
	GLsizei count)
{
	GLsizei i;
	size_t size;
	size_t offset;

	GLubyte *mappedBuffer;

	char *colorPointer;
	char *normalPointer;
	char *vertexPointer;

#ifdef GPU_SAFETY
		{
		int newVertexCountOK;
		GPU_SAFE_RETURN(GPU_IMMEDIATE->count + count <= GPU_IMMEDIATE->maxVertexCount, newVertexCountOK,);
		}
#endif

	vertexPointer = (char *)(arrays->vertexPointer) + (first * arrays->vertexStride);
	normalPointer = (char *)(arrays->normalPointer) + (first * arrays->normalStride);
	colorPointer  = (char *)(arrays->colorPointer ) + (first * arrays->colorStride );

	mappedBuffer = GPU_IMMEDIATE->mappedBuffer;

	offset = GPU_IMMEDIATE->offset;

	for (i = 0; i < count; i++) {
		size = arrays->vertexSize * sizeof(GLfloat);
		memcpy(mappedBuffer + offset, vertexPointer, size);
		offset += size;
		vertexPointer += arrays->vertexStride;

		if (normalPointer) {
			memcpy(mappedBuffer + offset, normalPointer, 3 * sizeof(GLfloat));
			offset += 3 * sizeof(GLfloat);
			normalPointer += arrays->normalStride;
		}

		if (colorPointer) {
			if (arrays->colorType == GL_FLOAT) {
				GLubyte color[4];

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
	const GPUarrays *arrays,
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
	const void *vertexPointer,
	GLint vertexStride,
	GLint first,
	GLsizei count)
{
	GPUarrays arrays = GPU_ARRAYS_V2F;

	arrays.vertexPointer = vertexPointer;
	arrays.vertexStride  = vertexStride != 0 ? vertexStride : 2 * sizeof(GLfloat);

	gpuImmediateFormat_V2();
	gpuDrawClientArrays(mode, &arrays, first, count);
	gpuImmediateUnformat();
}

void gpuSingleClientArrays_V3F(
	GLenum mode,
	const void *vertexPointer,
	GLint vertexStride,
	GLint first,
	GLsizei count)
{
	GPUarrays arrays = GPU_ARRAYS_V3F;

	arrays.vertexPointer = vertexPointer;
	arrays.vertexStride  = vertexStride != 0 ? vertexStride : 3 * sizeof(GLfloat);

	gpuImmediateFormat_V3();
	gpuDrawClientArrays(mode, &arrays, first, count);
	gpuImmediateUnformat();
}

void gpuSingleClientArrays_C3F_V3F(
	GLenum mode,
	const void *colorPointer,
	GLint colorStride,
	const void *vertexPointer,
	GLint vertexStride,
	GLint first,
	GLsizei count)
{
	GPUarrays arrays = GPU_ARRAYS_C3F_V3F;

	arrays.colorPointer = colorPointer;
	arrays.colorStride  = colorStride != 0 ? colorStride : 3 * sizeof(GLfloat);

	arrays.vertexPointer = vertexPointer;
	arrays.vertexStride  = vertexStride != 0 ? vertexStride : 3 * sizeof(GLfloat);

	gpuImmediateFormat_C4_V3();
	gpuDrawClientArrays(mode, &arrays, first, count);
	gpuImmediateUnformat();
}

void gpuSingleClientArrays_C4F_V3F(
	GLenum mode,
	const void *colorPointer,
	GLint colorStride,
	const void *vertexPointer,
	GLint vertexStride,
	GLint first,
	GLsizei count)
{
	GPUarrays arrays = GPU_ARRAYS_C4F_V3F;

	arrays.colorPointer = colorPointer;
	arrays.colorStride  = colorStride != 0 ? colorStride : 4 * sizeof(GLfloat);

	arrays.vertexPointer = vertexPointer;
	arrays.vertexStride  = vertexStride != 0 ? vertexStride : 3 * sizeof(GLfloat);

	gpuImmediateFormat_C4_V3();
	gpuDrawClientArrays(mode, &arrays, first, count);
	gpuImmediateUnformat();
}

void gpuSingleClientArrays_N3F_V3F(
	GLenum mode,
	const void *normalPointer,
	GLint normalStride,
	const void *vertexPointer,
	GLint vertexStride,
	GLint first,
	GLsizei count)
{
	GPUarrays arrays = GPU_ARRAYS_N3F_V3F;

	arrays.normalPointer = normalPointer;
	arrays.normalStride  = normalStride != 0 ? normalStride : 3 * sizeof(GLfloat);

	arrays.vertexPointer = vertexPointer;
	arrays.vertexStride  = vertexStride != 0 ? vertexStride : 3 * sizeof(GLfloat);

	gpuImmediateFormat_N3_V3();
	gpuDrawClientArrays(mode, &arrays, first, count);
	gpuImmediateUnformat();
}

void gpuSingleClientArrays_C3F_N3F_V3F(
	GLenum mode,
	const void *colorPointer,
	GLint colorStride,
	const void *normalPointer,
	GLint normalStride,
	const void *vertexPointer,
	GLint vertexStride,
	GLint first,
	GLsizei count)
{
	GPUarrays arrays = GPU_ARRAYS_C3F_N3F_V3F;

	arrays.colorPointer = colorPointer;
	arrays.colorStride  = colorStride != 0 ? colorStride : 3 * sizeof(GLfloat);

	arrays.normalPointer = normalPointer;
	arrays.normalStride  = normalStride != 0 ? normalStride : 3 * sizeof(GLfloat);

	arrays.vertexPointer = vertexPointer;
	arrays.vertexStride  = vertexStride != 0 ? vertexStride : 3 * sizeof(GLfloat);

	gpuImmediateFormat_C4_N3_V3();
	gpuDrawClientArrays(mode, &arrays, first, count);
	gpuImmediateUnformat();
}

void gpuSingleClientArrays_C4UB_V2F(
	GLenum mode,
	const void *colorPointer,
	GLint colorStride,
	const void *vertexPointer,
	GLint vertexStride,
	GLint first,
	GLsizei count)
{
	GPUarrays arrays = GPU_ARRAYS_C4UB_V2F;

	arrays.colorPointer = colorPointer;
	arrays.colorStride  = colorStride != 0 ? colorStride : 4;

	arrays.vertexPointer = vertexPointer;
	arrays.vertexStride  = vertexStride != 0 ? vertexStride : 2 * sizeof(GLfloat);

	gpuImmediateFormat_C4_V2();
	gpuDrawClientArrays(mode, &arrays, first, count);
	gpuImmediateUnformat();
}



void gpuSingleClientArrays_C4UB_V3F(
	GLenum mode,
	const void *colorPointer,
	GLint colorStride,
	const void *vertexPointer,
	GLint vertexStride,
	GLint first,
	GLsizei count)
{
	GPUarrays arrays = GPU_ARRAYS_C4UB_V3F;

	arrays.colorPointer = colorPointer;
	arrays.colorStride  = colorStride != 0 ? colorStride : 4;

	arrays.vertexPointer = vertexPointer;
	arrays.vertexStride  = vertexStride != 0 ? vertexStride : 3 * sizeof(GLfloat);

	gpuImmediateFormat_C4_V3();
	gpuDrawClientArrays(mode, &arrays, first, count);
	gpuImmediateUnformat();
}



void gpuImmediateIndexRange(GLuint indexMin, GLuint indexMax)
{
	GPU_IMMEDIATE->index->indexMin = indexMin;
	GPU_IMMEDIATE->index->indexMax = indexMax;
}



#define FIND_RANGE(suffix, ctype) \
	static void find_range_##suffix( \
    GLuint *arrayFirst, \
    GLuint *arrayLast, \
    GLsizei count, \
    const ctype *indexes) \
{ \
    int i; \
    BLI_assert(count > 0); \
    *arrayFirst = indexes[0]; \
    *arrayLast  = indexes[0]; \
    for (i = 1; i < count; i++) { \
        if (indexes[i] < *arrayFirst) { \
            *arrayFirst = indexes[i]; \
        } \
        else if (indexes[i] > *arrayLast) { \
            *arrayLast = indexes[i];  \
        } \
    } \
}

FIND_RANGE(ub, GLubyte)
FIND_RANGE(us, GLushort)
FIND_RANGE(ui, GLuint)



void gpuImmediateIndexComputeRange(void)
{
	GLuint indexMin, indexMax;

	GPUindex *index = GPU_IMMEDIATE->index;

	switch (index->type) {
		case GL_UNSIGNED_BYTE:
			find_range_ub(&indexMin, &indexMax, index->count, (GLubyte*)(index->mappedBuffer));
			break;

		case GL_UNSIGNED_SHORT:
			find_range_us(&indexMin, &indexMax, index->count, (GLushort*)(index->mappedBuffer));
			break;

		case GL_UNSIGNED_INT:
			find_range_ui(&indexMin, &indexMax, index->count, (GLuint*)(index->mappedBuffer));
			break;

		default:
			GPU_print_error_debug("wrong index type");
			return;
	}

	gpuImmediateIndexRange(indexMin, indexMax);
}



void gpuSingleClientElements_V3F(
	GLenum mode,
	const void *vertexPointer,
	GLint vertexStride,
	GLsizei count,
	const GLuint *indexes)
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
	const void *normalPointer,
	GLint normalStride,
	const void *vertexPointer,
	GLint vertexStride,
	GLsizei count,
	const GLuint *indexes)
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
	const void *colorPointer,
	GLint colorStride,
	const void *vertexPointer,
	GLint vertexStride,
	GLsizei count,
	const GLuint *indexes)
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
	const GPUarrays *arrays,
	GLuint indexMin,
	GLuint indexMax,
	GLsizei count,
	const GLuint *indexes)
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
	const void *vertexPointer,
	GLint vertexStride,
	GLuint indexMin,
	GLuint indexMax,
	GLsizei count,
	const GLuint *indexes)
{
	GPUarrays arrays = GPU_ARRAYS_V3F;

	arrays.vertexPointer = vertexPointer;
	arrays.vertexStride  = vertexStride != 0 ? vertexStride : 3 * sizeof(GLfloat);

	gpuImmediateFormat_V3();
	gpuDrawClientRangeElements(mode, &arrays, indexMin, indexMax, count, indexes);
	gpuImmediateUnformat();
}

void gpuSingleClientRangeElements_N3F_V3F(
	GLenum mode,
	const void *normalPointer,
	GLint normalStride,
	const void *vertexPointer,
	GLint vertexStride,
	GLuint indexMin,
	GLuint indexMax,
	GLsizei count,
	const GLuint *indexes)
{
	GPUarrays arrays = GPU_ARRAYS_N3F_V3F;

	arrays.normalPointer = normalPointer;
	arrays.normalStride  = normalStride != 0 ? normalStride : 3 * sizeof(GLfloat);

	arrays.vertexPointer = vertexPointer;
	arrays.vertexStride  = vertexStride != 0 ? vertexStride : 3 * sizeof(GLfloat);

	gpuImmediateFormat_N3_V3();
	gpuDrawClientRangeElements(mode, &arrays, indexMin, indexMax, count, indexes);
	gpuImmediateUnformat();
}

void gpuSingleClientRangeElements_C4UB_V3F(
	GLenum mode,
	const void *colorPointer,
	GLint colorStride,
	const void *vertexPointer,
	GLint vertexStride,
	GLuint indexMin,
	GLuint indexMax,
	GLsizei count,
	const GLuint *indexes)
{
	GPUarrays arrays = GPU_ARRAYS_C4UB_V3F;

	arrays.normalPointer = colorPointer;
	arrays.normalStride  = colorStride != 0 ? colorStride : 4 * sizeof(GLubyte);

	arrays.vertexPointer = vertexPointer;
	arrays.vertexStride  = vertexStride != 0 ? vertexStride : 3 * sizeof(GLfloat);

	gpuImmediateFormat_C4_V3();
	gpuDrawClientRangeElements(mode, &arrays, indexMin, indexMax, count, indexes);
	gpuImmediateUnformat();
}



GPUindex *gpuNewIndex(void)
{
	return (GPUindex*)MEM_callocN(sizeof(GPUindex), "GPUindex");
}



void gpuDeleteIndex(GPUindex *index)
{
	if (index) {
		GPUImmediate *immediate = index->immediate;

		gpu_index_shutdown_buffer_gl(index);
		immediate->index = NULL;

		MEM_freeN(index);
	}
}



void gpuImmediateIndex(GPUindex *index)
{
	//BLI_assert(GPU_IMMEDIATE->index == NULL);

	if (index)
		index->immediate = GPU_IMMEDIATE;

	GPU_IMMEDIATE->index = index;
}



GPUindex *gpuGetImmediateIndex()
{
	return GPU_IMMEDIATE->index;
}



void gpuImmediateMaxIndexCount(GLsizei maxIndexCount, GLenum type)
{
	BLI_assert(GPU_IMMEDIATE);
	BLI_assert(GPU_IMMEDIATE->index);

	GPU_IMMEDIATE->index->maxIndexCount = maxIndexCount;
	GPU_IMMEDIATE->index->type          = type;
}



void gpuIndexBegin(GLenum type)
{
	GPUindex *index;

	BLI_assert(ELEM(type, GL_UNSIGNED_BYTE, GL_UNSIGNED_SHORT, GL_UNSIGNED_INT));

	index = GPU_IMMEDIATE->index;

	index->count    = 0;
	index->indexMin = 0;
	index->indexMax = 0;
	index->type     = type;

	gpu_index_begin_buffer_gl();
}



#define INDEX_RELATIVE(suffix, ctype, glsymbol) \
void gpuIndexRelative##suffix(GLint offset, GLsizei count, const ctype *indexes) \
{ \
    int i; \
    int start; \
    int indexStart; \
    GPUindex *index; \
    BLI_assert(GPU_IMMEDIATE);  \
    BLI_assert(GPU_IMMEDIATE->index); \
    BLI_assert(GPU_IMMEDIATE->index->type == glsymbol); \
    { \
        bool indexCountOK = GPU_IMMEDIATE->index->count + count <= GPU_IMMEDIATE->index->maxIndexCount; \
        if (!indexCountOK) \
            return; \
    } \
    start = GPU_IMMEDIATE->count; \
    index = GPU_IMMEDIATE->index; \
    indexStart = index->count; \
    for (i = 0; i < count; i++) { \
        ((ctype*)(index->mappedBuffer))[indexStart+i] = start - offset + ((ctype*)indexes)[i]; \
    } \
    index->count += count; \
    index->offset = count*sizeof(ctype); \
}

INDEX_RELATIVE(ubv, GLubyte,  GL_UNSIGNED_BYTE)
INDEX_RELATIVE(usv, GLushort, GL_UNSIGNED_SHORT)
INDEX_RELATIVE(uiv, GLuint,   GL_UNSIGNED_INT)



#define INDEX(suffix, ctype, glsymbol)                                     \
void gpuIndex##suffix(ctype nextIndex)                                     \
{                                                                          \
    GPUindex *index;                                                       \
                                                                           \
    BLI_assert(GPU_IMMEDIATE);                                             \
    BLI_assert(GPU_IMMEDIATE->index);                                      \
    BLI_assert(GPU_IMMEDIATE->index->type == glsymbol);                    \
                                                                           \
    if (GPU_IMMEDIATE->index->count < GPU_IMMEDIATE->index->maxIndexCount) \
        return;                                                            \
                                                                           \
    index = GPU_IMMEDIATE->index;                                          \
    ((ctype*)(index->mappedBuffer))[index->count] = nextIndex;             \
    index->count++;                                                        \
    index->offset += sizeof(ctype);                                        \
}

INDEX(ub, GLubyte,  GL_UNSIGNED_BYTE)
INDEX(us, GLushort, GL_UNSIGNED_SHORT)
INDEX(ui, GLuint,   GL_UNSIGNED_INT)



void gpuIndexEnd(void)
{
	gpu_index_end_buffer_gl();

	GPU_IMMEDIATE->index->mappedBuffer = NULL;
}

void GPUBegin(GLenum mode)
{
	gpu_commit_matrix();
	glBegin(mode);
}

void GPUDrawArrays(GLenum mode, int start, int count)
{
	gpu_commit_matrix();
	glDrawArrays(mode, start, count);
}

void GPUDrawElements(GLenum mode, int count, int type, void *p)
{
	gpu_commit_matrix();
	glDrawElements(mode, count, type, p);
}



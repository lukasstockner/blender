/* This program is free software; you can redistribute it and/or
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
 * Contributor(s):
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/gpu/intern/gpu_extension_wrapper.c
 *  \ingroup gpu
 */

#define GPU_FUNC_INTERN
#include "gpu_extension_wrapper.h"

#include "GPU_extensions.h"

#include <stdlib.h>

//#include REAL_GL_MODE

#if GPU_SAFETY

#define GPU_CHECK_INVALID_PNAME(symbol)           \
    {                                             \
    GLboolean paramOK;                            \
    GPU_SAFE_RETURN(pname != (symbol), paramOK,); \
    }

#else

#define GPU_CHECK_INVALID_PNAME(symbol)

#endif


#if !defined(GLEW_ES_ONLY)
static void GLAPIENTRY check_glGetObjectParameterivARB(GLuint shader, GLuint pname, GLint *params)
{
	GPU_CHECK_INVALID_PNAME(GL_SHADER_TYPE);

	glGetObjectParameterivARB(shader, pname, params);
}
#endif

static void GLAPIENTRY check_glGetShaderiv(GLuint shader, GLuint pname, GLint *params)
{
	GPU_CHECK_INVALID_PNAME(GL_SHADER_TYPE);

	glGetShaderiv(shader, pname, params);
}

static GLboolean init_shader_objects(void)
{
	if (GLEW_VERSION_2_0 || GLEW_ES_VERSION_2_0) {
		gpu_glAttachShader       = glAttachShader;
		gpu_glCompileShader      = glCompileShader;
		gpu_glCreateProgram      = glCreateProgram;
		gpu_glCreateShader       = glCreateShader;
		gpu_glDeleteShader       = glDeleteShader;
		gpu_glGetProgramInfoLog  = glGetProgramInfoLog;
		gpu_glGetShaderiv        = check_glGetShaderiv;
		gpu_glGetShaderInfoLog   = glGetShaderInfoLog;
		gpu_glGetUniformLocation = glGetUniformLocation;
		gpu_glLinkProgram        = glLinkProgram;
		gpu_glShaderSource       = glShaderSource;
		gpu_glUniform1i          = glUniform1i;
		gpu_glUniform1f          = glUniform1f;
		gpu_glUniform1iv         = glUniform1iv;
		gpu_glUniform2iv         = glUniform2iv;
		gpu_glUniform3iv         = glUniform3iv;
		gpu_glUniform4iv         = glUniform4iv;
		gpu_glUniform1fv         = glUniform1fv;
		gpu_glUniform2fv         = glUniform2fv;
		gpu_glUniform3fv         = glUniform3fv;
		gpu_glUniform4fv         = glUniform4fv;
		gpu_glUniformMatrix3fv   = glUniformMatrix3fv;
		gpu_glUniformMatrix4fv   = glUniformMatrix4fv;
		gpu_glUseProgram         = glUseProgram;
		gpu_glValidateProgram    = glValidateProgram;

		return GL_TRUE;
	}

#if !defined(GLEW_ES_ONLY)
	if (GLEW_ARB_shader_objects) {
		gpu_glAttachShader       = glAttachObjectARB;
		gpu_glCompileShader      = glCompileShaderARB;
		gpu_glCreateProgram      = glCreateProgramObjectARB;
		gpu_glCreateShader       = glCreateShaderObjectARB;
		gpu_glDeleteShader       = glDeleteObjectARB;
		gpu_glGetProgramInfoLog  = glGetInfoLogARB;
		gpu_glGetShaderiv        = check_glGetObjectParameterivARB;
		gpu_glGetShaderInfoLog   = glGetInfoLogARB;
		gpu_glGetUniformLocation = glGetUniformLocationARB;
		gpu_glLinkProgram        = glLinkProgramARB;
		gpu_glShaderSource       = glShaderSourceARB;
		gpu_glUniform1i          = glUniform1iARB;
		gpu_glUniform1f          = glUniform1fARB;
		gpu_glUniform1iv         = glUniform1ivARB;
		gpu_glUniform2iv         = glUniform2ivARB;
		gpu_glUniform3iv         = glUniform3ivARB;
		gpu_glUniform4iv         = glUniform4ivARB;
		gpu_glUniform1fv         = glUniform1fvARB;
		gpu_glUniform2fv         = glUniform2fvARB;
		gpu_glUniform3fv         = glUniform3fvARB;
		gpu_glUniform4fv         = glUniform4fvARB;
		gpu_glUniformMatrix3fv   = glUniformMatrix3fvARB;
		gpu_glUniformMatrix4fv   = glUniformMatrix4fvARB;
		gpu_glUseProgram         = glUseProgramObjectARB;
		gpu_glValidateProgram    = glValidateProgramARB;

		return GL_TRUE;
	}
#endif

	return GL_FALSE;
}

static GLboolean init_vertex_shader(void)
{
	if (GLEW_VERSION_2_0 || GLEW_ES_VERSION_2_0) {
		gpu_glGetAttribLocation  = glGetAttribLocation;
		gpu_glBindAttribLocation = glBindAttribLocation;

		return GL_TRUE;
	}

#if !defined(GLEW_ES_ONLY)
	if (GLEW_ARB_vertex_shader) {
		gpu_glBindAttribLocation = (void (GLAPIENTRY*)(GLuint,GLuint,const GLchar*))glBindAttribLocationARB;
		gpu_glGetAttribLocation  = glGetAttribLocationARB;

		return GL_TRUE;
	}
#endif

	return GL_FALSE;
}

#if !defined(GLEW_ES_ONLY)
static void GLAPIENTRY check_glGetProgramivARB(GLuint shader, GLuint pname, GLint *params)
{
	GPU_CHECK_INVALID_PNAME(GL_ACTIVE_ATTRIBUTES);
	GPU_CHECK_INVALID_PNAME(GL_ACTIVE_ATTRIBUTE_MAX_LENGTH);

	glGetProgramivARB(shader, pname, params);
}
#endif

static void GLAPIENTRY check_glGetProgramiv(GLuint shader, GLuint pname, GLint *params)
{
	GPU_CHECK_INVALID_PNAME(GL_ACTIVE_ATTRIBUTES);
	GPU_CHECK_INVALID_PNAME(GL_ACTIVE_ATTRIBUTE_MAX_LENGTH);

	glGetProgramiv(shader, pname, params);
}

static GLboolean init_vertex_program(void)
{
	if (GLEW_VERSION_2_0 || GLEW_ES_VERSION_2_0) {
		gpu_glDeleteProgram            = glDeleteProgram;
		gpu_glDisableVertexAttribArray = glDisableVertexAttribArray;
		gpu_glEnableVertexAttribArray  = glEnableVertexAttribArray;
		gpu_glGetProgramiv             = check_glGetProgramiv;
		gpu_glVertexAttribPointer      = glVertexAttribPointer;

		return GL_TRUE;
	}

#if !defined(GLEW_ES_ONLY)
	if (GLEW_ARB_vertex_program) {
		gpu_glDeleteProgram            = glDeleteObjectARB;
		gpu_glDisableVertexAttribArray = glDisableVertexAttribArrayARB;
		gpu_glEnableVertexAttribArray  = glEnableVertexAttribArrayARB;
		gpu_glGetProgramiv             = check_glGetProgramivARB;
		gpu_glVertexAttribPointer      = glVertexAttribPointerARB;

		return GL_TRUE;
	}
#endif

	return GL_FALSE;
}

static void init_buffers(void)
{
	if (GLEW_VERSION_1_5 || GLEW_ES_VERSION_2_0) {
		gpu_glBindBuffer    = glBindBuffer;
		gpu_glBufferData    = glBufferData;
		gpu_glBufferSubData = glBufferSubData;
		gpu_glDeleteBuffers = glDeleteBuffers;
		gpu_glGenBuffers    = glGenBuffers;

		return;
	}

#if !defined(GLEW_ES_ONLY)
	if (GLEW_ARB_vertex_buffer_object) {
		gpu_glBindBuffer    = glBindBufferARB;
		gpu_glBufferData    = glBufferDataARB;
		gpu_glBufferSubData = glBufferSubDataARB;
		gpu_glDeleteBuffers = glDeleteBuffersARB;
		gpu_glGenBuffers    = glGenBuffersARB;

		return;
	}
#endif
}

static void init_mapbuffer(void)
{
#if !defined(GLEW_ES_ONLY)
	if (GLEW_VERSION_1_5) {
		gpu_glMapBuffer   = glMapBuffer;
		gpu_glUnmapBuffer = glUnmapBuffer;

		return;
	}

	if (GLEW_ARB_vertex_buffer_object) {
		gpu_glMapBuffer   = glMapBufferARB;
		gpu_glUnmapBuffer = glUnmapBufferARB;

		return;
	}
#endif

#if !defined(GLEW_NO_ES)
	if (GLEW_OES_mapbuffer) {
		gpu_glMapBuffer   = glMapBufferOES;
		gpu_glUnmapBuffer = glUnmapBufferOES;

		return;
	}
#endif
}

static GLboolean init_framebuffer_object(void)
{
	if (GLEW_VERSION_3_0 || GLEW_ES_VERSION_2_0 || GLEW_ARB_framebuffer_object) {
		gpu_glGenFramebuffers        = glGenFramebuffers;
		gpu_glBindFramebuffer        = glBindFramebuffer;
		gpu_glDeleteFramebuffers     = glDeleteFramebuffers;
		gpu_glFramebufferTexture2D   = glFramebufferTexture2D;
		gpu_glCheckFramebufferStatus = glCheckFramebufferStatus;

		return GL_TRUE;
	}

#if !defined(GLEW_ES_ONLY)
	if (GLEW_EXT_framebuffer_object) {
		gpu_glGenFramebuffers        = glGenFramebuffersEXT;
		gpu_glBindFramebuffer        = glBindFramebufferEXT;
		gpu_glDeleteFramebuffers     = glDeleteFramebuffersEXT;
		gpu_glFramebufferTexture2D   = glFramebufferTexture2DEXT;
		gpu_glCheckFramebufferStatus = glCheckFramebufferStatusEXT;

		return GL_TRUE;
	}
#endif

#if !defined(GLEW_NO_ES)
	if (GLEW_OES_framebuffer_object) {
		gpu_glGenFramebuffers        = glGenFramebuffersOES;
		gpu_glBindFramebuffer        = glBindFramebufferOES;
		gpu_glDeleteFramebuffers     = glDeleteFramebuffersOES;
		gpu_glFramebufferTexture2D   = glFramebufferTexture2DOES;
		gpu_glCheckFramebufferStatus = glCheckFramebufferStatusOES;

		return GL_TRUE;
	}
#endif

	return GL_FALSE;
}

static void init_vertex_arrays(void)
{
	if (GLEW_VERSION_3_0 || GLEW_ARB_vertex_array_object) {
		gpu_glGenVertexArrays    = glGenVertexArrays;
		gpu_glBindVertexArray    = glBindVertexArray;
		gpu_glDeleteVertexArrays = glDeleteVertexArrays;

		return;
	}

#if !defined(GLEW_NO_ES)
	if (GLEW_OES_vertex_array_object) {
		gpu_glGenVertexArrays    = glGenVertexArraysOES;
		gpu_glBindVertexArray    = glBindVertexArrayOES;
		gpu_glDeleteVertexArrays = glDeleteVertexArraysOES;

		return;
	}
#endif
}

static GPUFUNC void (GLAPIENTRY* _GenerateMipmap)(GLenum target);

static void init_generate_mipmap(void)
{
#if !defined(GLEW_ES_ONLY)
	if (GLEW_VERSION_3_0 || GLEW_ARB_framebuffer_object) {
		_GenerateMipmap = glGenerateMipmap;
		return;
	}

	if (GLEW_EXT_framebuffer_object) {
		_GenerateMipmap = glGenerateMipmapEXT;
		return;
	}
#endif

#if !defined(GLEW_NO_ES)
	if (GLEW_OES_framebuffer_object) {
		_GenerateMipmap = glGenerateMipmapOES;
	}
#endif
}

void gpu_glGenerateMipmap(GLenum target)
{
	GLboolean workaround;

	/* Work around bug in ATI driver, need to have GL_TEXTURE_2D enabled.
	 * http://www.opengl.org/wiki/Common_Mistakes#Automatic_mipmap_generation */
	if (GPU_type_matches(GPU_DEVICE_ATI, GPU_OS_ANY, GPU_DRIVER_ANY)) {
		workaround = !glIsEnabled(target);

		if (workaround) {
			glEnable(target);
		}
	}
	else {
		workaround = GL_FALSE;
	}

	_GenerateMipmap(target);

	if (workaround) {
		glDisable(target);
	}
}

static void* GPU_buffer_start_update_dummy(GLenum UNUSED(target), void* data)
{
	GPU_ASSERT(data != NULL);
	return data;
}

static void* GPU_buffer_start_update_copy(GLenum UNUSED(target), void* data)
{
	GPU_ASSERT(data != NULL);
	return data;
}

static void* GPU_buffer_start_update_map(GLenum target, void* UNUSED(data))
{
	void* mappedData;
	GPU_ASSERT(UNUSED_data == NULL);
	mappedData = gpu_glMapBuffer(target, GL_WRITE_ONLY);
	GPU_CHECK_NO_ERROR();
	return mappedData;
}

static void GPU_buffer_finish_update_dummy(GLenum UNUSED(target), GLsizeiptr UNUSED(dataSize), const GLvoid* UNUSED(data))
{
	GPU_ASSERT(UNUSED_data != NULL);
}

static void GPU_buffer_finish_update_copy(GLenum target, GLsizeiptr dataSize, const GLvoid* data)
{
	GPU_ASSERT(data != NULL);
	gpu_glBufferSubData(target, 0, dataSize, data);
	GPU_CHECK_NO_ERROR();
}

static void GPU_buffer_finish_update_map(GLenum target, GLsizeiptr UNUSED(dataSize), const GLvoid* UNUSED(data))
{
	GPU_ASSERT(UNUSED_data != NULL);
	gpu_glUnmapBuffer(target);
	GPU_CHECK_NO_ERROR();
}

void GPU_wrap_extensions(GLboolean* glslsupport_out, GLboolean* framebuffersupport_out)
{
	*glslsupport_out = true;
	
	if (!init_shader_objects())
		*glslsupport_out = false;

	if (!init_vertex_shader())
		*glslsupport_out = false;

	if (init_vertex_program())
		*glslsupport_out = false;

	if (!(GLEW_ARB_multitexture || GLEW_VERSION_1_3))
		*glslsupport_out = false;

	*framebuffersupport_out = init_framebuffer_object();

	init_vertex_arrays();
	init_buffers();
	init_mapbuffer();

	if (GLEW_VERSION_1_5 || GLEW_OES_mapbuffer || GLEW_ARB_vertex_buffer_object) {
		GPU_buffer_start_update  = GPU_buffer_start_update_map;
		GPU_buffer_finish_update = GPU_buffer_finish_update_map;
	}
	else if (GLEW_ES_VERSION_2_0) {
		GPU_buffer_start_update  = GPU_buffer_start_update_copy;
		GPU_buffer_finish_update = GPU_buffer_finish_update_copy;
	}
	else {
		GPU_buffer_start_update  = GPU_buffer_start_update_dummy;
		GPU_buffer_finish_update = GPU_buffer_finish_update_dummy;
	}

	init_generate_mipmap();
}

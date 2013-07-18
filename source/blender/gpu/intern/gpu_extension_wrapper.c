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

#include <stdlib.h>
#include <stdio.h>

#define GPU_FUNC_INTERN
#include "gpu_extension_wrapper.h"

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



static void GLAPIENTRY check_glGetObjectParameterivARB(GLuint shader, GLuint pname, GLint *params)
{
	GPU_CHECK_INVALID_PNAME(GL_SHADER_TYPE);

	glGetObjectParameterivARB(shader, pname, params);
}

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
	else if (GLEW_ARB_shader_objects) {
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
	else {
		return GL_FALSE;
	}
}

static GLboolean init_vertex_shader(void)
{
	if (GLEW_VERSION_2_0 || GLEW_ES_VERSION_2_0) {
		gpu_glGetAttribLocation  = glGetAttribLocation;
		gpu_glBindAttribLocation = glBindAttribLocation;

		return GL_TRUE;
	}
	else if (GLEW_ARB_vertex_shader) {
		gpu_glBindAttribLocation = (void (GLAPIENTRY*)(GLuint,GLuint,const GLchar*))glBindAttribLocationARB;
		gpu_glGetAttribLocation  = glGetAttribLocationARB;

		return GL_TRUE;
	}
	else {
		return GL_FALSE;
	}
}

static void GLAPIENTRY check_glGetProgramivARB(GLuint shader, GLuint pname, GLint *params)
{
	GPU_CHECK_INVALID_PNAME(GL_ACTIVE_ATTRIBUTES);
	GPU_CHECK_INVALID_PNAME(GL_ACTIVE_ATTRIBUTE_MAX_LENGTH);

	glGetProgramivARB(shader, pname, params);
}

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
	else if (GLEW_ARB_vertex_program) {
		gpu_glDeleteProgram            = glDeleteObjectARB;
		gpu_glDisableVertexAttribArray = glDisableVertexAttribArrayARB;
		gpu_glEnableVertexAttribArray  = glEnableVertexAttribArrayARB;
		gpu_glGetProgramiv             = check_glGetProgramivARB;
		gpu_glVertexAttribPointer      = glVertexAttribPointerARB;

		return GL_TRUE;
	}
	else {
		return GL_FALSE;
	}
}

static void init_buffers(void)
{
	if (GLEW_VERSION_1_5 || GLEW_ES_VERSION_2_0) {
		gpu_glBindBuffer    = glBindBuffer;
		gpu_glBufferData    = glBufferData;
		gpu_glDeleteBuffers = glDeleteBuffers;
		gpu_glGenBuffers    = glGenBuffers;
	}
	else if (GLEW_ARB_vertex_buffer_object) {
		gpu_glBindBuffer    = glBindBufferARB;
		gpu_glBufferData    = glBufferDataARB;
		gpu_glDeleteBuffers = glDeleteBuffersARB;
		gpu_glGenBuffers    = glGenBuffersARB;
	}
}

static void init_mapbuffer(void)
{
	if (GLEW_VERSION_1_5) {
		gpu_glMapBuffer   = glMapBuffer;
		gpu_glUnmapBuffer = glUnmapBuffer;
	}
	else if (GLEW_ARB_vertex_buffer_object) {
		gpu_glMapBuffer   = glMapBufferARB;
		gpu_glUnmapBuffer = glUnmapBufferARB;
	}
	else if (GLEW_OES_mapbuffer) {
		gpu_glMapBuffer   = glMapBufferOES;
		gpu_glUnmapBuffer = glUnmapBufferOES;
	}
}

static GLboolean init_framebuffer_object(void)
{
	if (GLEW_VERSION_3_0 || GLEW_ARB_framebuffer_object) {
		gpu_glGenFramebuffers    = glGenFramebuffers;
		gpu_glBindFramebuffer    = glBindFramebuffer;
		gpu_glDeleteFramebuffers = glDeleteFramebuffers;

		return GL_TRUE;
	}
	else if (GLEW_EXT_framebuffer_object) {
		gpu_glGenFramebuffers    = glGenFramebuffersEXT;
		gpu_glBindFramebuffer    = glBindFramebufferEXT;
		gpu_glDeleteFramebuffers = glDeleteFramebuffersEXT;

		return GL_TRUE;
	}
	else if (GLEW_OES_framebuffer_object) {
		gpu_glGenFramebuffers    = glGenFramebuffersOES;
		gpu_glBindFramebuffer    = glBindFramebufferOES;
		gpu_glDeleteFramebuffers = glDeleteFramebuffersOES;

		return GL_TRUE;
	}
	else {
		return GL_FALSE;
	}
}

static const void* GLAPIENTRY GPU_buffer_start_update_buffer(GLenum target, GLsizeiptr size, const GLvoid* data, GLenum usage)
{
	gpu_glBufferData(target, 0, NULL, usage);
	return data;
}

static void* GLAPIENTRY GPU_buffer_start_update_map(GLenum target, GLsizeiptr size, const GLvoid* data, GLenum usage)
{
	gpu_glBufferData(target, size, NULL, usage);
	return gpu_glMapBuffer(target, GL_WRITE_ONLY);
}

static void GLAPIENTRY GPU_buffer_finish_update_buffer(GLenum target, GLsizeiptr size, const GLvoid* data, GLenum usage)
{
	gpu_glBufferData(target, size, data, usage);
}

static void GLAPIENTRY GPU_buffer_finish_update_map(GLenum target, GLsizeiptr size, const GLvoid* data, GLenum usage)
{
	gpu_glUnmapBuffer(target);
}

void GPU_wrap_extensions(GLboolean* glslsupport_out, GLboolean* framebuffersupport_out)
{
	/* written like this so that operator&& doesn't shortcut */
	*glslsupport_out = init_shader_objects();
	*glslsupport_out = *glslsupport_out && init_vertex_shader();
	*glslsupport_out = *glslsupport_out && init_vertex_program();
	*glslsupport_out = *glslsupport_out && (GLEW_ARB_multitexture || GLEW_VERSION_1_3);

	*framebuffersupport_out = init_framebuffer_object();

	init_buffers();
	init_mapbuffer();

	if(gpu_glMapBuffer) {
		GPU_buffer_start_update  = GPU_buffer_start_update_map;
		GPU_buffer_finish_update = GPU_buffer_finish_update_map;
	}
	else {
		GPU_buffer_start_update  = GPU_buffer_start_update_buffer;
		GPU_buffer_finish_update = GPU_buffer_finish_update_buffer;
	}
}

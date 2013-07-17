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

/** \file blender/gpu/intern/gpu_functions.c
 *  \ingroup gpu
 */

#include <stdlib.h>
#include <stdio.h>

#define GPU_FUNC_INTERN
#define GIVE_ME_APIENTRY
#include "GPU_functions.h"

#include "GPU_extensions.h"

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



#ifndef WITH_GLES

static void GLAPIENTRY check_glGetObjectParameterivARB(GLuint shader, GLuint pname, GLint *params)
{
	GPU_CHECK_INVALID_PNAME(GL_SHADER_TYPE);

	glGetObjectParameterivARB(shader, pname, params);
}

static void GLAPIENTRY check_glGetProgramivARB(GLuint shader, GLuint pname, GLint *params)
{
	GPU_CHECK_INVALID_PNAME(GL_ACTIVE_ATTRIBUTES);
	GPU_CHECK_INVALID_PNAME(GL_ACTIVE_ATTRIBUTE_MAX_LENGTH);

	glGetProgramivARB(shader, pname, params);
}

static void init_glsl_arb(void)
{
	gpu_glCreateShader     = glCreateShaderObjectARB;
	gpu_glAttachShader     = glAttachObjectARB;
	gpu_glShaderSource     = glShaderSourceARB;
	gpu_glCompileShader    = glCompileShaderARB;
	gpu_glGetShaderiv      = check_glGetObjectParameterivARB;
	gpu_glGetShaderInfoLog = glGetInfoLogARB;

	gpu_glCreateProgram     = glCreateProgramObjectARB;
	gpu_glLinkProgram       = glLinkProgramARB;
	gpu_glGetProgramiv      = check_glGetProgramivARB;
	gpu_glGetProgramInfoLog = glGetInfoLogARB;
	gpu_glValidateProgram		= glValidateProgramARB;

	gpu_glUniform1i = glUniform1iARB;
	gpu_glUniform1f = glUniform1fARB;
	
	gpu_glUniform1iv = glUniform1ivARB;
	gpu_glUniform2iv = glUniform2ivARB;
	gpu_glUniform3iv = glUniform3ivARB;
	gpu_glUniform4iv = glUniform4ivARB;

	gpu_glUniform1fv = glUniform1fvARB;
	gpu_glUniform2fv = glUniform2fvARB;
	gpu_glUniform3fv = glUniform3fvARB;
	gpu_glUniform4fv = glUniform4fvARB;

	gpu_glUniformMatrix3fv = glUniformMatrix3fvARB;
	gpu_glUniformMatrix4fv = glUniformMatrix4fvARB;

	gpu_glGetAttribLocation  = glGetAttribLocationARB;
	gpu_glBindAttribLocation = (void (GLAPIENTRY*)(GLuint,GLuint,const GLchar*))glBindAttribLocationARB;
	gpu_glGetUniformLocation = glGetUniformLocationARB;
	
	gpu_glVertexAttribPointer = glVertexAttribPointerARB;
	
	gpu_glEnableVertexAttribArray = glEnableVertexAttribArrayARB;
	gpu_glDisableVertexAttribArray = glDisableVertexAttribArrayARB;

	gpu_glUseProgram    = glUseProgramObjectARB;
	gpu_glDeleteShader  = glDeleteObjectARB;
	gpu_glDeleteProgram = glDeleteObjectARB;
}

static void init_buffers_arb(void)
{
	gpu_glGenBuffers = glGenBuffersARB;
	gpu_glBindBuffer = glBindBufferARB;
	gpu_glBufferData =  glBufferDataARB;
	gpu_glDeleteBuffers = glDeleteBuffersARB;
	
	gpu_glMapBuffer = glMapBufferARB;
	gpu_glUnmapBuffer = glUnmapBufferARB;
}

static void init_mapbuffers_standard()
{
	gpu_glMapBuffer = glMapBuffer;
	gpu_glUnmapBuffer = glUnmapBuffer;
}
	
	
#else

static void init_mapbuffers_oes(void)
{
//	gpu_glMapBuffer   = glMapBufferOES;
//	gpu_glUnmapBuffer = glUnmapBufferOES;
	gpu_glMapBuffer   = glMapBuffer;
	gpu_glUnmapBuffer = glUnmapBuffer;
}

#endif

static void check_glGetShaderiv(GLuint shader, GLuint pname, GLint *params)
{
	GPU_CHECK_INVALID_PNAME(GL_SHADER_TYPE);

	glGetShaderiv(shader, pname, params);
}

static void check_glGetProgramiv(GLuint shader, GLuint pname, GLint *params)
{
	GPU_CHECK_INVALID_PNAME(GL_ACTIVE_ATTRIBUTES);
	GPU_CHECK_INVALID_PNAME(GL_ACTIVE_ATTRIBUTE_MAX_LENGTH);

	glGetProgramiv(shader, pname, params);
}

static void init_glsl_standard(void)
{
	gpu_glCreateShader     = glCreateShader;
	gpu_glAttachShader     = glAttachShader;
	gpu_glShaderSource     = glShaderSource;
	gpu_glCompileShader    = glCompileShader;
	gpu_glGetShaderiv      = glGetShaderiv;
	gpu_glGetShaderInfoLog = glGetShaderInfoLog;

	gpu_glCreateProgram     = glCreateProgram;
	gpu_glLinkProgram       = glLinkProgram;
	gpu_glGetProgramiv      = glGetProgramiv;
	gpu_glGetProgramInfoLog = glGetProgramInfoLog;
	gpu_glValidateProgram	= glValidateProgram;

	gpu_glUniform1i = glUniform1i;
	gpu_glUniform1f = glUniform1f;

	gpu_glUniform1iv = glUniform1iv;
	gpu_glUniform2iv = glUniform2iv;
	gpu_glUniform3iv = glUniform3iv;
	gpu_glUniform4iv = glUniform4iv;

	gpu_glUniform1fv = glUniform1fv;
	gpu_glUniform2fv = glUniform2fv;
	gpu_glUniform3fv = glUniform3fv;
	gpu_glUniform4fv = glUniform4fv;

	gpu_glUniformMatrix3fv = glUniformMatrix3fv;
	gpu_glUniformMatrix4fv = glUniformMatrix4fv;

	gpu_glGetAttribLocation  = glGetAttribLocation;
	gpu_glBindAttribLocation = glBindAttribLocation;
	gpu_glGetUniformLocation = glGetUniformLocation;
	
	gpu_glVertexAttribPointer = glVertexAttribPointer;

	gpu_glEnableVertexAttribArray = glEnableVertexAttribArray;
	gpu_glDisableVertexAttribArray = glDisableVertexAttribArray;

	gpu_glUseProgram    = glUseProgram;
	gpu_glDeleteShader  = glDeleteShader;
	gpu_glDeleteProgram = glDeleteProgram;
}

static void init_buffers_standard(void)
{
	gpu_glGenBuffers = glGenBuffers;
	gpu_glBindBuffer = glBindBuffer;
	gpu_glBufferData =  glBufferData;
	gpu_glDeleteBuffers = glDeleteBuffers;
}

static void init_framebuffers_standard(void)
{
	gpu_glGenFramebuffers    = glGenFramebuffers;
	gpu_glBindFramebuffer    = glBindFramebuffer;
	gpu_glDeleteFramebuffers = glDeleteFramebuffers;
}



#ifndef WITH_GLES

static void init_framebuffers_ext(void)
{
	gpu_glGenFramebuffers    = glGenFramebuffersEXT;
	gpu_glBindFramebuffer    = glBindFramebufferEXT;
	gpu_glDeleteFramebuffers = glDeleteFramebuffersEXT;
}

#endif

static const void * GLAPIENTRY gpuBufferStartUpdate_buffer(GLenum target, GLsizeiptr size, const GLvoid * data, GLenum usage)
{
	gpu_glBufferData(target, 0, NULL, usage);
	return data;
}

static void * GLAPIENTRY gpuBufferStartUpdate_map(GLenum target, GLsizeiptr size, const GLvoid * data, GLenum usage)
{
	gpu_glBufferData(target, size, NULL, usage);
	return gpu_glMapBuffer(target, GL_WRITE_ONLY);
}

static void GLAPIENTRY gpuBufferFinishUpdate_buffer(GLenum target, GLsizeiptr size, const GLvoid * data, GLenum usage)
{
	gpu_glBufferData(target, size, data, usage);

}

static void GLAPIENTRY gpuBufferFinishUpdate_map(GLenum target, GLsizeiptr size, const GLvoid * data, GLenum usage)
{
	gpu_glUnmapBuffer(target);

}

void GPU_func_comp_init(void)
{
#ifdef WITH_GLES
	init_glsl_standard();
	init_framebuffers_standard();
	
	init_buffers_standard();
	init_mapbuffers_oes();
#else
	/*	Here we rely on GLEW
	We expect all symbols be present, even if they are only 0,
	We use GLEW to fill the arrays with zero even if extensions are not avalable
	*/

	if(GLEW_VERSION_1_5)
	{
		init_buffers_standard();
		init_mapbuffers_standard();
	}
	else
		init_buffers_arb();

	if(GLEW_VERSION_2_0)
		init_glsl_standard();
	else
		init_glsl_arb();

	if(GLEW_VERSION_3_0)
		init_framebuffers_standard();
	else
		init_framebuffers_ext();
#endif
/* Some android has unimplemented glUnMapBuffer? */
	if(gpu_glMapBuffer) {
		GPU_ext_config |= GPU_EXT_MAPBUFFER;
		gpuBufferStartUpdate  = gpuBufferStartUpdate_map;
		gpuBufferFinishUpdate = gpuBufferFinishUpdate_map;
	}
	else {
		gpuBufferStartUpdate  = gpuBufferStartUpdate_buffer;
		gpuBufferFinishUpdate = gpuBufferFinishUpdate_buffer;
	}
}

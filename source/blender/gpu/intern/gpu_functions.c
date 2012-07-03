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

#ifdef GLES
#include <GLES2/gl2.h>
#endif

#define GPU_FUNC_INTERN
#include "GPU_functions.h"
#include REAL_GL_MODE

#if GPU_SAFETY

#define GPU_CHECK_INVALID_PNAME(symbol)           \
    {                                             \
    GLboolean paramOK;                            \
    GPU_SAFE_RETURN(pname != (symbol), paramOK,); \
    }

#else
#define GPU_CHECK_INVALID_PNAME(symbol)

#endif



#ifndef GLES

static void check_glGetObjectParameterivARB(GLuint shader, GLuint pname, GLint *params)
{
	GPU_CHECK_INVALID_PNAME(GL_SHADER_TYPE);

	glGetObjectParameterivARB(shader, pname, params);
}

static void check_glGetProgramivARB(GLuint shader, GLuint pname, GLint *params)
{
	GPU_CHECK_INVALID_PNAME(GL_ACTIVE_ATTRIBUTES);
	GPU_CHECK_INVALID_PNAME(GL_ACTIVE_ATTRIBUTE_MAX_LENGTH);

	glGetProgramivARB(shader, pname, params);
}

static void init_glsl_arb(void)
{
	gpuCreateShader     = glCreateShaderObjectARB;
	gpuAttachShader     = glAttachObjectARB;
	gpuShaderSource     = glShaderSourceARB;
	gpuCompileShader    = glCompileShaderARB;
	gpuGetShaderiv      = check_glGetObjectParameterivARB;
	gpuGetShaderInfoLog = glGetInfoLogARB;

	gpuCreateProgram     = glCreateProgramObjectARB;
	gpuLinkProgram       = glLinkProgramARB;
	gpuGetProgramiv      = check_glGetProgramivARB;
	gpuGetProgramInfoLog = glGetInfoLogARB;

	gpuUniform1i = glUniform1iARB;

	gpuUniform1fv = glUniform1fvARB;
	gpuUniform2fv = glUniform2fvARB;
	gpuUniform3fv = glUniform3fvARB;
	gpuUniform4fv = glUniform4fvARB;

	gpuUniformMatrix3fv = glUniformMatrix3fvARB;
	gpuUniformMatrix4fv = glUniformMatrix4fvARB;

	gpuGetAttribLocation  = glGetAttribLocationARB;
	gpuGetUniformLocation = glGetUniformLocationARB;

	gpuUseProgram    = glUseProgramObjectARB;
	gpuDeleteShader  = glDeleteObjectARB;
	gpuDeleteProgram = glDeleteObjectARB;
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
	gpuCreateShader     = glCreateShader;
	gpuAttachShader     = glAttachShader;
	gpuShaderSource     = glShaderSource;
	gpuCompileShader    = glCompileShader;
	gpuGetShaderiv      = check_glGetShaderiv;
	gpuGetShaderInfoLog = glGetShaderInfoLog;

	gpuCreateProgram     = glCreateProgram;
	gpuLinkProgram       = glLinkProgram;
	gpuGetProgramiv      = check_glGetProgramiv;
	gpuGetProgramInfoLog = glGetProgramInfoLog;

	gpuUniform1i = glUniform1i;

	gpuUniform1fv = glUniform1fv;
	gpuUniform2fv = glUniform2fv;
	gpuUniform3fv = glUniform3fv;
	gpuUniform4fv = glUniform4fv;

	gpuUniformMatrix3fv = glUniformMatrix3fv;
	gpuUniformMatrix4fv = glUniformMatrix4fv;

	gpuGetAttribLocation  = glGetAttribLocation;
	gpuGetUniformLocation = glGetUniformLocation;

	gpuUseProgram    = glUseProgram;
	gpuDeleteShader  = glDeleteShader;
	gpuDeleteProgram = glDeleteProgram;
}

static void init_framebuffers_standard(void)
{
	gpuGenFramebuffers    = glGenFramebuffers;
	gpuBindFramebuffer    = glBindFramebuffer;
	gpuDeleteFramebuffers = glDeleteFramebuffers;
}



#ifndef GLES

static void init_framebuffers_ext(void)
{
	gpuGenFramebuffers    = glGenFramebuffersEXT;
	gpuBindFramebuffer    = glBindFramebufferEXT;
	gpuDeleteFramebuffers = glDeleteFramebuffersEXT;
}

#endif



void GPU_func_comp_init(void)
{
#ifdef GLES
	init_glsl_standard();
	init_framebuffers_standard();
#else
	/*	Here we rely on GLEW
	We expect all symbols be present, even if they are only 0,
	We use GLEW to fill the arrays with zero even if extensions are not avalable
	*/

	if(GLEW_VERSION_2_0)
		init_glsl_standard();
	else
		init_glsl_arb();

	if(GLEW_VERSION_3_0)
		init_framebuffers_standard();
	else
		init_framebuffers_ext();
#endif
}

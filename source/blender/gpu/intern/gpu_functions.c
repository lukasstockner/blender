#ifdef GLES
#include <GLES2/gl2.h>
#else
#include <GL/glew.h>
#endif



#define GPU_INTERN_FUNC
#include <GPU_functions.h>

#ifndef GLES

unsigned int gpuCreateShaderObjectARB(unsigned int shaderType)
{
switch (shaderType)
{
	case GL_VERTEX_SHADER:		shaderType = GL_VERTEX_SHADER_ARB; break;
	case GL_FRAGMENT_SHADER:	shaderType = GL_FRAGMENT_SHADER_ARB; break;
}
	return glCreateShaderObjectARB(shaderType);

}

void gpuGetObjectParameterivARB(unsigned int shader, unsigned int pname, int *params)
{
switch (pname)
{
	case GL_SHADER_TYPE:			pname = 0; break;
	case GL_DELETE_STATUS:			pname = GL_OBJECT_DELETE_STATUS_ARB; break;
	case GL_COMPILE_STATUS:			pname = GL_OBJECT_COMPILE_STATUS_ARB; break;
	case GL_INFO_LOG_LENGTH:		pname = GL_OBJECT_INFO_LOG_LENGTH_ARB; break;
	case GL_SHADER_SOURCE_LENGTH:	pname = GL_OBJECT_SHADER_SOURCE_LENGTH_ARB; break;
}
	glGetObjectParameterivARB(shader, pname, params);
}


void gpuGetProgramivARB(unsigned int shader, unsigned int pname, int *params)
{
switch (pname)
{
	case GL_DELETE_STATUS:		pname = GL_OBJECT_DELETE_STATUS_ARB; break;
	case GL_LINK_STATUS:		pname = GL_OBJECT_LINK_STATUS_ARB; break;
	case GL_VALIDATE_STATUS:	pname = GL_OBJECT_VALIDATE_STATUS_ARB; break;
	case GL_INFO_LOG_LENGTH:	pname = GL_OBJECT_INFO_LOG_LENGTH_ARB; break;
	case GL_ATTACHED_SHADERS:	pname = GL_OBJECT_ATTACHED_OBJECTS_ARB; break;
	case GL_ACTIVE_ATTRIBUTES:	pname = 0; break;
	case GL_ACTIVE_ATTRIBUTE_MAX_LENGTH:	pname = 0; break;
	case GL_ACTIVE_UNIFORMS:				pname = GL_OBJECT_ACTIVE_UNIFORMS_ARB; break;
	case GL_ACTIVE_UNIFORM_MAX_LENGTH:		pname = GL_OBJECT_ACTIVE_UNIFORM_MAX_LENGTH_ARB; break;
}
	glGetProgramivARB(shader, pname, params);
}



static void init_glsl_arb(void)
{
gpuCreateShader = gpuCreateShaderObjectARB;
gpuAttachShader = glAttachObjectARB;
gpuShaderSource = glShaderSourceARB;
gpuCompileShader = glCompileShaderARB;
gpuGetShaderiv = gpuGetObjectParameterivARB;
gpuGetShaderInfoLog = glGetInfoLogARB;

gpuCreateProgram = glCreateProgramObjectARB;
gpuLinkProgram = glLinkProgramARB;
gpuGetProgramiv = gpuGetProgramivARB;
gpuGetProgramInfoLog = glGetInfoLogARB;

gpuUniform1i = glUniform1iARB;

gpuUniform1fv = glUniform1fvARB;
gpuUniform2fv = glUniform2fvARB;
gpuUniform3fv = glUniform3fvARB;
gpuUniform4fv = glUniform4fvARB;
gpuUniformMatrix3fv = glUniformMatrix3fvARB;
gpuUniformMatrix4fv = glUniformMatrix4fvARB;


gpuGetAttribLocation = glGetAttribLocationARB;
gpuGetUniformLocation = glGetUniformLocationARB;

gpuUseProgram = glUseProgramObjectARB;
gpuDeleteShader = glDeleteObjectARB;
gpuDeleteProgram = glDeleteObjectARB;

}

#endif


static void init_glsl_standard(void)
{
gpuCreateShader = glCreateShader;
gpuAttachShader = glAttachShader;
gpuShaderSource = glShaderSource;
gpuCompileShader = glCompileShader;
gpuGetShaderiv = glGetShaderiv;
gpuGetShaderInfoLog = glGetShaderInfoLog;

gpuCreateProgram = glCreateProgram;
gpuLinkProgram = glLinkProgram;
gpuGetProgramiv = glGetProgramiv;
gpuGetProgramInfoLog = glGetProgramInfoLog;

gpuUniform1i = glUniform1i;

gpuUniform1fv = glUniform1fv;
gpuUniform2fv = glUniform2fv;
gpuUniform3fv = glUniform3fv;
gpuUniform4fv = glUniform4fv;
gpuUniformMatrix3fv = glUniformMatrix3fv;
gpuUniformMatrix4fv = glUniformMatrix4fv;


gpuGetAttribLocation = glGetAttribLocation;
gpuGetUniformLocation = glGetUniformLocation;

gpuUseProgram = glUseProgram;
gpuDeleteShader = glDeleteShader;
gpuDeleteProgram = glDeleteProgram;

}

static void init_framebuffers_standard(void)
{
	gpuGenFramebuffers = glGenFramebuffers;
	gpuBindFramebuffer = glBindFramebuffer;
	gpuDeleteFramebuffers = glDeleteFramebuffers;

}

#ifndef GLES
void gpuBindFramebufferEXT(unsigned int target, unsigned int framebuffer)
{
switch(target)
{
	case GL_DRAW_FRAMEBUFFER:	target = GL_DRAW_FRAMEBUFFER_EXT; break;
	case GL_READ_FRAMEBUFFER:	target = GL_READ_FRAMEBUFFER_EXT; break;
	case GL_FRAMEBUFFER:		target = GL_FRAMEBUFFER_EXT; break;
}

glBindFramebufferEXT(target, framebuffer);

}

static void init_framebuffers_ext(void)
{
	gpuGenFramebuffers = glGenFramebuffersEXT;
	gpuBindFramebuffer = gpuBindFramebufferEXT;
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

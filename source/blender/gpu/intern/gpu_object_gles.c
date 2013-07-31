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
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s):
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file gpu_object_gles.c
 *  \ingroup gpu
 */

#include "gpu_object_gles.h"

#include "gpu_glew.h"
#include "gpu_extension_wrapper.h"
#include "gpu_profile.h"

#include "MEM_guardedalloc.h"

//#include REAL_GL_MODE

struct GPUGLSL_ES_info *curglslesi = 0;



void gpuVertexPointer_gles(int size, int type, int stride, const void *pointer)
{
	if(curglslesi && (curglslesi->vertexloc != -1)) {
		glEnableVertexAttribArray(curglslesi->vertexloc);
		glVertexAttribPointer(curglslesi->vertexloc, size, type, GL_FALSE, stride, pointer);
	}
}

void gpuNormalPointer_gles(int type, int stride, const void *pointer)
{
	if(curglslesi && (curglslesi->normalloc != -1)) {
		glEnableVertexAttribArray(curglslesi->normalloc);
		glVertexAttribPointer(curglslesi->normalloc, 3, type, GL_FALSE, stride, pointer);
	}
}

void gpuColorPointer_gles (int size, int type, int stride, const void *pointer)
{
	if(curglslesi && (curglslesi->colorloc != -1)) {
		glEnableVertexAttribArray(curglslesi->colorloc);
		glVertexAttribPointer(curglslesi->colorloc, size, type, GL_TRUE, stride, pointer);
	}
}

void gpuTexCoordPointer_gles(int size, int type, int stride, const void *pointer)
{
	if(curglslesi && (curglslesi->texturecoordloc!=-1)) {
		glEnableVertexAttribArray(curglslesi->texturecoordloc);
		glVertexAttribPointer(curglslesi->texturecoordloc, size, type, GL_FALSE, stride, pointer);
	}

	if(curglslesi && curglslesi->texidloc!=-1) {
		glUniform1i(curglslesi->texidloc, 0);
	}
}

void gpuClientActiveTexture_gles(int texture)
{

}

void gpuCleanupAfterDraw_gles(void)
{
	//int i;

	/* Disable any arrays that were used so that everything is off again. */

	if (!curglslesi)
		return;

	/* vertex */

	if(curglslesi->vertexloc != -1) {
		glDisableVertexAttribArray(curglslesi->vertexloc);
	}

	/* normal */

	if (GPU_IMMEDIATE->format.normalSize != 0) {
		if(curglslesi->normalloc != -1) {
			glDisableVertexAttribArray(curglslesi->normalloc);
		}
	}

	/* color */

	if (GPU_IMMEDIATE->format.colorSize != 0) {
		if(curglslesi->colorloc != -1) {
			glDisableVertexAttribArray(curglslesi->colorloc);
		}
	}

	/* texture coordinate */

	if (GPU_IMMEDIATE->format.textureUnitCount == 1) {
		if(curglslesi && (curglslesi->texturecoordloc!=-1)) {
			glDisableVertexAttribArray(curglslesi->texturecoordloc);
		}
	}
	//else if (GPU_IMMEDIATE->format.textureUnitCount > 1) {
	//	for (i = 0; i < GPU_IMMEDIATE->format.textureUnitCount; i++) {
	//		glClientActiveTexture(GPU_IMMEDIATE->format.textureUnitMap[i]);
	//		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	//	}

	//	glClientActiveTexture(GL_TEXTURE0);
	//}

	/* float vertex attribute */

	//for (i = 0; i < GPU_IMMEDIATE->format.attribCount_f; i++) {
	//	gpu_glDisableVertexAttribArray(GPU_IMMEDIATE->format.attribIndexMap_f[i]);
	//}

	/* byte vertex attribute */

	//for (i = 0; i < GPU_IMMEDIATE->format.attribCount_ub; i++) {
	//	gpu_glDisableVertexAttribArray(GPU_IMMEDIATE->format.attribIndexMap_ub[i]);
	//}
}

void gpu_set_shader_es(struct GPUGLSL_ES_info * s, int update)
{
	curglslesi = s;
//	if(update)
//		GPU_matrix_forced_update();
}



void gpu_assign_gles_loc(GPUGLSL_ES_info * glslesinfo, unsigned int program)
{
	glslesinfo->normalmatloc     = gpu_glGetUniformLocation(program, "b_NormalMatrix");
	glslesinfo->viewmatloc       = gpu_glGetUniformLocation(program, "b_ModelViewMatrix");
	glslesinfo->projectionmatloc = gpu_glGetUniformLocation(program, "b_ProjectionMatrix");
	glslesinfo->texturematloc    = gpu_glGetUniformLocation(program, "b_TextureMatrix");

	glslesinfo->texidloc         = gpu_glGetUniformLocation(program, "v_texid");

	glslesinfo->vertexloc        = gpu_glGetAttribLocation(program, "b_Vertex");
	glslesinfo->normalloc        = gpu_glGetAttribLocation(program, "b_Normal");
	glslesinfo->colorloc         = gpu_glGetAttribLocation(program, "b_Color");
	glslesinfo->texturecoordloc  = gpu_glGetAttribLocation(program, "b_Coord");
}



GLuint static compile_shader(GLenum type, const char** src, int count)
{
	GLint status;
	GLuint shader = gpu_glCreateShader(type);
	
	
	if(shader == 0)
		return 0;
		
		
	gpu_glShaderSource(shader, count, src, NULL);
	gpu_glCompileShader(shader);
	
	
	GPU_CHECK_NO_ERROR();

	gpu_glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	
	if(status == 0)
	{
		GLint len = 0;
		gpu_glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
		
	GPU_CHECK_NO_ERROR();
		if(len > 0)
		{
			char * log = MEM_mallocN(len, "GLSLErrLog");
			
			gpu_glGetShaderInfoLog(shader, len, NULL, log);
			printf("Error compiling GLSL: \n %s\n", log);
			
			MEM_freeN(log);
			
			gpu_glGetShaderiv(shader, GL_SHADER_SOURCE_LENGTH, &len);
			
	GPU_CHECK_NO_ERROR();
			if(len > 0)
			{
				char* shadersrc = MEM_mallocN(len, "GLSLErrLog");

				//glGetShaderSource(shader, len, rlen, log);
				//printf("Objet GLSL source: \n %s", shadersrc);
				
				MEM_freeN(shadersrc);
				
				
				
					
			}
				
		}
		
		gpu_glDeleteShader(shader);
	
	GPU_CHECK_NO_ERROR();
		return 0;
	}
	
	return shader;

}


GLuint static create_program(GLuint vertex, GLuint fragment)
{
	GLint status = 0;
	GLuint program = gpu_glCreateProgram();
	
	if(program == 0)
	{
		printf("Cannot create GLSL program object\n");
		return 0;	
	}
	
	gpu_glAttachShader(program, vertex);
	gpu_glAttachShader(program, fragment);
	
	GPU_CHECK_NO_ERROR();
	/* b_Color cannot be 0 because b_Color can be a singular color and glVertexAttrib* won't work */
	glBindAttribLocation(program, 1, "b_Color");
	
	GPU_CHECK_NO_ERROR();
	gpu_glLinkProgram(program);
	
	GPU_CHECK_NO_ERROR();
	gpu_glGetProgramiv(program, GL_LINK_STATUS, &status);
	
	GPU_CHECK_NO_ERROR();
	if(status == 0) 
	{
		GLint len = 0;
		
		gpu_glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);
		
		if(len > 0)
		{
			char * log = (char*)MEM_mallocN(len, "GLSLErrProgLog");
			
			gpu_glGetProgramInfoLog(program, len, NULL, log);
			printf("Error in generating Program GLSL: \n %s\n", log);
			
			MEM_freeN(log);
				
		}
		
	GPU_CHECK_NO_ERROR();
		gpu_glDeleteProgram(program);
		return 0;	
	}

	GPU_CHECK_NO_ERROR();
	return program;
}

char * object_shader_vector_basic = 
#if defined(WITH_GL_PROFILE_CORE)
	"#version 130\n"
#endif
	"uniform mat4 b_ProjectionMatrix ;	\n"
	"uniform mat4 b_ModelViewMatrix ;	\n"
	"attribute vec4 b_Vertex;	\n"
	"attribute vec4 b_Color;	\n"
	"varying vec4 v_Color;	\n"
	"void main()	\n"
	"{	\n"
	"	gl_Position = b_ProjectionMatrix * b_ModelViewMatrix * b_Vertex;	\n"
	"	v_Color = b_Color;	\n"
	"}	\n"
	;

char * object_shader_fragment_basic = 
#if defined(WITH_GL_PROFILE_CORE)
	"#version 130\n"
#endif
//#if defined(WITH_GL_PROFILE_ES20)
	"precision mediump float;	\n"
//#endif
	"varying vec4 v_Color;	\n"
	"void main()	\n"
	"{	\n"
	"	gl_FragColor = v_Color;	\n"
	"}	\n"
	;


char * object_shader_vector_alphatexture = 
#if defined(WITH_GL_PROFILE_CORE)
	"#version 130\n"
#endif
	"uniform mat4 b_ProjectionMatrix ;	\n"
	"uniform mat4 b_ModelViewMatrix ;	\n"
	"uniform mat4 b_TextureMatrix ;	\n"
	"attribute vec4 b_Vertex;	\n"
	"attribute vec4 b_Color;	\n"
	"varying vec4 v_Color;	\n"
	"attribute vec2 b_Coord;	\n"
	"varying vec2 v_Coord;	\n"
	"void main()	\n"
	"{	\n"
	"	gl_Position = b_ProjectionMatrix * b_ModelViewMatrix * b_Vertex;	\n"
	"	v_Coord = mat2(b_TextureMatrix) * b_Coord;	\n"
	"	v_Color = b_Color;	\n"
	"}	\n"
	;

char * object_shader_fragment_alphatexture = 
#if defined(WITH_GL_PROFILE_CORE)
	"#version 130\n"
#endif
//#if defined(WITH_GL_PROFILE_ES20)
	"precision mediump float;	\n"
//#endif
	"varying vec4 v_Color;\n"
	"varying vec2 v_Coord;\n"
	"uniform sampler2D v_texid;\n"
	"void main()\n"
	"{\n"
	"	gl_FragColor = vec4(v_Color.rgb, texture2D(v_texid, v_Coord).a*v_Color.a);\n"
	"}\n"
;

char * object_shader_vector_redtexture = 
#if defined(WITH_GL_PROFILE_CORE)
	"#version 130\n"
#endif
	"uniform mat4 b_ProjectionMatrix ;	\n"
	"uniform mat4 b_ModelViewMatrix ;	\n"
	"uniform mat4 b_TextureMatrix ;	\n"
	"attribute vec4 b_Vertex;	\n"
	"attribute vec4 b_Color;	\n"
	"varying vec4 v_Color;	\n"
	"attribute vec2 b_Coord;	\n"
	"varying vec2 v_Coord;	\n"
	"void main()	\n"
	"{	\n"
	"	gl_Position = b_ProjectionMatrix * b_ModelViewMatrix * b_Vertex;	\n"
	"	v_Coord = mat2(b_TextureMatrix) * b_Coord;	\n"
	"	v_Color = b_Color;	\n"
	"}	\n"
	;

char * object_shader_fragment_redtexture = 
#if defined(WITH_GL_PROFILE_CORE)
	"#version 130\n"
#endif
//#if defined(WITH_GL_PROFILE_ES20)
	"precision mediump float;	\n"
//#endif
	"varying vec4 v_Color;\n"
	"varying vec2 v_Coord;\n"
	"uniform sampler2D v_texid;\n"
	"void main()\n"
	"{\n"
	"	gl_FragColor = vec4(v_Color.rgb, texture2D(v_texid, v_Coord).r * v_Color.a);\n"
	"}\n"
;

char * object_shader_vector_rgbatexture = 
#if defined(WITH_GL_PROFILE_CORE)
	"#version 130\n"
#endif
	"uniform mat4 b_ProjectionMatrix ;\n"
	"uniform mat4 b_ModelViewMatrix ;\n"
	"uniform mat4 b_TextureMatrix ;\n"
	"attribute vec4 b_Vertex;\n"
	"attribute vec4 b_Color;\n"
	"varying vec4 v_Color;\n"
	"attribute vec2 b_Coord;\n"
	"varying vec2 v_Coord;\n"
	"void main()\n"
	"{\n"
	"	gl_Position = b_ProjectionMatrix * b_ModelViewMatrix * b_Vertex;\n"
	"	v_Coord = mat2(b_TextureMatrix) * b_Coord;\n"
	"	v_Color = b_Color;\n"
	"}\n"
;

char * object_shader_fragment_rgbatexture = 
#if defined(WITH_GL_PROFILE_CORE)
	"#version 130\n"
#endif
//#if defined(WITH_GL_PROFILE_ES20)
	"precision mediump float;	\n"
//#endif
	"varying vec4 v_Color;\n"
	"varying vec2 v_Coord;\n"
	"uniform sampler2D v_texid;\n"
	"void main()	\n"
	"{\n"
	"	gl_FragColor = v_Color*texture2D(v_texid, v_Coord);\n"
	"}\n"
;

char * object_shader_vector_pixels = 
#if defined(WITH_GL_PROFILE_CORE)
	"#version 130\n"
#endif
	"uniform mat4 b_ProjectionMatrix ;\n"
	"uniform mat4 b_ModelViewMatrix ;\n"
	"uniform mat4 b_TextureMatrix ;\n"
	"attribute vec4 b_Vertex;\n"
	"attribute vec4 b_Color;\n"
	"varying vec4 v_Color;\n"
	"attribute vec2 b_Coord;\n"
	"varying vec2 v_Coord;\n"
	"void main()\n"
	"{\n"
	"	gl_Position = b_ProjectionMatrix * b_ModelViewMatrix * b_Vertex;\n"
	"	v_Coord = mat2(b_TextureMatrix) * b_Coord;\n"
	"	v_Color = b_Color;\n"
	"}\n"
;

char * object_shader_fragment_pixels = 
#if defined(WITH_GL_PROFILE_CORE)
	"#version 130\n"
#endif
//#if defined(WITH_GL_PROFILE_ES20)
	"precision mediump float;	\n"
//#endif
	"varying vec4 v_Color;\n"
	"varying vec2 v_Coord;\n"
	"uniform sampler2D v_texid;\n"
	"void main()	\n"
	"{\n"
	"	gl_FragColor = texture2D(v_texid, v_Coord);\n"
	"}\n"
;



GPUGLSL_ES_info shader_main_info;
int shader_main;

GPUGLSL_ES_info shader_alphatexture_info;
int shader_alphatexture;

GPUGLSL_ES_info shader_redtexture_info;
int shader_redtexture;

GPUGLSL_ES_info shader_rgbatexture_info;
int shader_rgbatexture;

GPUGLSL_ES_info shader_pixels_info;
int shader_pixels;



void gpu_object_init_gles(void)
{
	GPU_CHECK_NO_ERROR();
	{
		GLuint vo = compile_shader(GL_VERTEX_SHADER,   &object_shader_vector_basic,   1);
		GLuint fo = compile_shader(GL_FRAGMENT_SHADER, &object_shader_fragment_basic, 1);
		shader_main = create_program(vo, fo);
		gpu_assign_gles_loc(&shader_main_info, shader_main);
	}

	GPU_CHECK_NO_ERROR();
	{
		GLuint vo = compile_shader(GL_VERTEX_SHADER,   &object_shader_vector_alphatexture,   1);
		GLuint fo = compile_shader(GL_FRAGMENT_SHADER, &object_shader_fragment_alphatexture, 1);
		shader_alphatexture = create_program(vo, fo);	
		gpu_assign_gles_loc(&shader_alphatexture_info, shader_alphatexture);
	}
	GPU_CHECK_NO_ERROR();
	{
		GLuint vo = compile_shader(GL_VERTEX_SHADER,   &object_shader_vector_redtexture,   1);
		GLuint fo = compile_shader(GL_FRAGMENT_SHADER, &object_shader_fragment_redtexture, 1);
		shader_redtexture = create_program(vo, fo);	
		gpu_assign_gles_loc(&shader_redtexture_info, shader_redtexture);
	}
	GPU_CHECK_NO_ERROR();

	{
		GLuint vo = compile_shader(GL_VERTEX_SHADER,   &object_shader_vector_rgbatexture,   1);
		GLuint fo = compile_shader(GL_FRAGMENT_SHADER, &object_shader_fragment_rgbatexture, 1);
		shader_rgbatexture = create_program(vo, fo);	
		gpu_assign_gles_loc(&shader_rgbatexture_info, shader_rgbatexture);
	}
	GPU_CHECK_NO_ERROR();

	{
		GLuint vo = compile_shader(GL_VERTEX_SHADER,   &object_shader_vector_pixels,   1);
		GLuint fo = compile_shader(GL_FRAGMENT_SHADER, &object_shader_fragment_pixels, 1);
		shader_pixels = create_program(vo, fo);	
		gpu_assign_gles_loc(&shader_pixels_info, shader_pixels);
	}

	GPU_CHECK_NO_ERROR();
// XXX jwilkins: until I figure out what the 'base aspect' should be, set shader_main as the default shader state
#if defined(WITH_GL_PROFILE_CORE) || defined(WITH_GL_PROFILE_ES20)
	if (GPU_PROFILE_CORE || GPU_PROFILE_ES20) {
		gpu_set_shader_es(&shader_main_info, 0);
		gpu_glUseProgram(shader_main);
	GPU_CHECK_NO_ERROR();
		return;
	}
#else
	if (GPU_PROFILE_COMPAT) {
		gpu_glUseProgram(0);
	}
#endif
}

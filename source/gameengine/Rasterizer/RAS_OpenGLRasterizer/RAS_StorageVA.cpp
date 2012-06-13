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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifdef GLES
#include <GLES2/gl2.h>
#endif

#include "GPU_matrix.h"
#include "RAS_StorageVA.h"

#include <GL/glew.h>

RAS_StorageVA::RAS_StorageVA(int *texco_num, RAS_IRasterizer::TexCoGen *texco, int *attrib_num, RAS_IRasterizer::TexCoGen *attrib) :
	m_texco_num(texco_num),
	m_texco(texco),
	m_attrib_num(attrib_num),
	m_attrib(attrib),
	m_last_texco_num(0),
	m_last_attrib_num(0)
{
}

RAS_StorageVA::~RAS_StorageVA()
{
}

bool RAS_StorageVA::Init()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	return true;
}

void RAS_StorageVA::Exit()
{
}


#include REAL_GL_MODE
#include <stdio.h>
 GLuint programObject = 0;


GLuint MobLoadShader(GLenum type, const char *src)
{
	GLuint shader;
	GLint compiled;

	shader = glCreateShader (type);

	if (!shader)
		return 0;

	glShaderSource(shader, 1, &src, NULL);
	glCompileShader (shader);

	glGetShaderiv (shader, GL_COMPILE_STATUS, &compiled);

	if (!compiled)
	{
		GLint infoLen = 0;

		glGetShaderiv (shader, GL_INFO_LOG_LENGTH, &infoLen);

		if(infoLen > 0)
		{
		char* log = (char*)malloc(sizeof(char) * infoLen);

		 glGetShaderInfoLog(shader, infoLen, NULL, log);
		 printf ("Error:\n%s\n", log);

		 free(log);
		}

		glDeleteShader(shader);
		return 0;
	}

	return shader;
}


int MobInit ( void)
{

	const char vShaderStr[] =
			"precision mediump float;		\n"
			"attribute vec4 vPosition;	\n"
			"attribute vec4 vNorm;	\n"
			"varying vec3 pos; 	\n"
			"varying vec3 normv; 	\n"
			"uniform mat4 bProjectionMatrix ;	\n"
			"uniform mat4 bModelViewMatrix ;	\n"
			"varying vec3 lightp; 		\n"
			"void main()							\n"
			"{			 	\n"
			"lightp = vec3(6.0, 6.0, 6.0);"
			"vec4 posmodel = bModelViewMatrix * vPosition;\n"
			"pos = vec3( posmodel);	\n"
			"normv = normalize(vec3(bModelViewMatrix *vNorm));	\n"
			"	gl_Position = bProjectionMatrix * posmodel;  	\n"
			"}					\n"
			"				  \n";

	const char fShaderStr[] =
			"precision mediump float;		\n"
			"varying vec3 pos; 		\n"
			"varying vec3 normv; 		\n"
			"varying vec3 lightp ;		\n"
			"void main()			\n"
			"{					\n"
			"vec3 diff = pos-lightp;		\n"
			"float mull = (1000.0*dot(normalize(diff),normv)/sqrt(dot(diff,diff))/dot(diff,diff));		\n"
			  "if(mull>1.0){mull=1.0;};\n"
			  //"if(mull<0.0){mull*=-1.0;};\n"
			"gl_FragColor = vec4 (vec3(mull), 1.0 );		\n"
			"}			\n";

	GLuint vertexShader;
	GLuint fragmentShader;
	GLint linked;

	vertexShader = MobLoadShader(GL_VERTEX_SHADER, vShaderStr);
	fragmentShader = MobLoadShader(GL_FRAGMENT_SHADER, fShaderStr);

	programObject = glCreateProgram();

	if (programObject == 0)
	  return 0;

	glAttachShader(programObject, vertexShader);
	glAttachShader(programObject, fragmentShader);

	glBindAttribLocation(programObject, 0, "vPosition");
	glBindAttribLocation(programObject, 1, "vNorm");

	glLinkProgram(programObject);

	glGetProgramiv(programObject, GL_LINK_STATUS, &linked);

	if (!linked)
	{
	  GLint size = 0;

	  glGetProgramiv(programObject, GL_INFO_LOG_LENGTH, &size);
	  if (size > 0)
	  {
		 char* log = (char*)malloc(sizeof(char) * size);

		 glGetProgramInfoLog(programObject, size, NULL, log);
		 printf("Error:\n%s\n", log);

		 free (log);
	  }

	  glDeleteProgram(programObject);
	  return GL_FALSE;
	}



	return GL_TRUE;
}


#include FAKE_GL_MODE


#ifdef GLES

void RAS_StorageVA::IndexPrimitives(RAS_MeshSlot& ms)
{
	static const GLsizei stride = sizeof(RAS_TexVert);
	bool wireframe = m_drawingmode <= RAS_IRasterizer::KX_WIREFRAME;
	RAS_MeshSlot::iterator it;
	GLenum drawmode;

	GLint posmat;
	float mat[16];


	if (!wireframe)
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_NORMAL_ARRAY);

	// use glDrawElements to draw each vertexarray
	for (ms.begin(it); !ms.end(it); ms.next(it)) {
		if (it.totindex == 0)
			continue;

		// drawing mode
		if (it.array->m_type == RAS_DisplayArray::TRIANGLE)
			drawmode = GL_TRIANGLES;
		else if (it.array->m_type == RAS_DisplayArray::QUAD)
			drawmode = GL_QUADS;
		else
			drawmode = GL_LINES;

		// colors
		if (drawmode != GL_LINES && !wireframe) {
			if (ms.m_bObjectColor) {
				const MT_Vector4& rgba = ms.m_RGBAcolor;

				glDisableClientState(GL_COLOR_ARRAY);
				glColor4d(rgba[0], rgba[1], rgba[2], rgba[3]);
			}
			else {
				glColor4f(0.0f, 0.0f, 0.0f, 1.0f);
				glEnableClientState(GL_COLOR_ARRAY);
			}
		}
		else
			glColor4f(0.0f, 0.0f, 0.0f, 1.0f);

#include REAL_GL_MODE
		//glVertexPointer(3, GL_FLOAT, stride, it.vertex->getXYZ());
		if(programObject==0)
		{

		MobInit();
		//programObject =1;

		glEnable(GL_DEPTH_TEST);


		}
glFrontFace(GL_CCW);
		glDisable(GL_BLEND);
		glDisable(GL_ALPHA_TEST);
		glClear(GL_DEPTH_BUFFER_BIT);
		glEnable(GL_DEPTH_TEST);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
glDepthMask( true );
		glUseProgram (programObject );

		gpuMatrixMode(GPU_MODELVIEW);
		gpuGetMatrix(mat);
		posmat = glGetUniformLocation(programObject, "bModelViewMatrix");
		glUniformMatrix4fv(posmat,1,0,mat);

		gpuMatrixMode(GPU_PROJECTION);
		gpuGetMatrix(mat);
		posmat = glGetUniformLocation(programObject, "bProjectionMatrix");
		glUniformMatrix4fv(posmat,1,0,mat);
		gpuMatrixMode(GPU_MODELVIEW);
		int infoLen;
		glGetProgramiv ( programObject, GL_ACTIVE_UNIFORMS, &infoLen );

		glVertexAttribPointer(0, 3, GL_FLOAT, 0,stride, it.vertex->getXYZ());
		glEnableVertexAttribArray ( 0 );
				glVertexAttribPointer(1, 3, GL_FLOAT, 0,stride, it.vertex->getNormal());
		glEnableVertexAttribArray ( 1 );

#include FAKE_GL_MODE
		glNormalPointer(GL_FLOAT, stride, it.vertex->getNormal());

		if (!wireframe) {
			glTexCoordPointer(2, GL_FLOAT, stride, it.vertex->getUV(0));
			if (glIsEnabled(GL_COLOR_ARRAY))
				glColorPointer(4, GL_UNSIGNED_BYTE, stride, it.vertex->getRGBA());
		}

		// here the actual drawing takes places

#include REAL_GL_MODE
		glDrawElements(drawmode, it.totindex, GL_UNSIGNED_SHORT, it.index);
#include FAKE_GL_MODE
	}
	
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);
	if (!wireframe) {
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glDisableClientState(GL_COLOR_ARRAY);
	}
}


#else

void RAS_StorageVA::IndexPrimitives(RAS_MeshSlot& ms)
{
	static const GLsizei stride = sizeof(RAS_TexVert);
	bool wireframe = m_drawingmode <= RAS_IRasterizer::KX_WIREFRAME;
	RAS_MeshSlot::iterator it;
	GLenum drawmode;

	gpuMatrixCommit();

	if (!wireframe)
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_NORMAL_ARRAY);

	// use glDrawElements to draw each vertexarray
	for (ms.begin(it); !ms.end(it); ms.next(it)) {
		if (it.totindex == 0)
			continue;

		// drawing mode
		if (it.array->m_type == RAS_DisplayArray::TRIANGLE)
			drawmode = GL_TRIANGLES;
		else if (it.array->m_type == RAS_DisplayArray::QUAD)
			drawmode = GL_QUADS;
		else
			drawmode = GL_LINES;

		// colors
		if (drawmode != GL_LINES && !wireframe) {
			if (ms.m_bObjectColor) {
				const MT_Vector4& rgba = ms.m_RGBAcolor;

				glDisableClientState(GL_COLOR_ARRAY);
				glColor4d(rgba[0], rgba[1], rgba[2], rgba[3]);
			}
			else {
				glColor4f(0.0f, 0.0f, 0.0f, 1.0f);
				glEnableClientState(GL_COLOR_ARRAY);
			}
		}
		else
			glColor4f(0.0f, 0.0f, 0.0f, 1.0f);

		glVertexPointer(3, GL_FLOAT, stride, it.vertex->getXYZ());
		glNormalPointer(GL_FLOAT, stride, it.vertex->getNormal());
		if (!wireframe) {
			glTexCoordPointer(2, GL_FLOAT, stride, it.vertex->getUV(0));
			if (glIsEnabled(GL_COLOR_ARRAY))
				glColorPointer(4, GL_UNSIGNED_BYTE, stride, it.vertex->getRGBA());
		}

		// here the actual drawing takes places
		glDrawElements(drawmode, it.totindex, GL_UNSIGNED_SHORT, it.index);
	}

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);
	if (!wireframe) {
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		glDisableClientState(GL_COLOR_ARRAY);
	}
}



#endif

void RAS_StorageVA::IndexPrimitivesMulti(class RAS_MeshSlot& ms)
{
	static const GLsizei stride = sizeof(RAS_TexVert);
	bool wireframe = m_drawingmode <= RAS_IRasterizer::KX_WIREFRAME;
	RAS_MeshSlot::iterator it;
	GLenum drawmode;

	if (!wireframe)
		EnableTextures(true);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_NORMAL_ARRAY);

	// use glDrawElements to draw each vertexarray
	for (ms.begin(it); !ms.end(it); ms.next(it)) {
		if (it.totindex == 0)
			continue;

		// drawing mode
		if (it.array->m_type == RAS_DisplayArray::TRIANGLE)
			drawmode = GL_TRIANGLES;
		else if (it.array->m_type == RAS_DisplayArray::QUAD)
			drawmode = GL_QUADS;
		else
			drawmode = GL_LINES;

		// colors
		if (drawmode != GL_LINES && !wireframe) {
			if (ms.m_bObjectColor) {
				const MT_Vector4& rgba = ms.m_RGBAcolor;

				glDisableClientState(GL_COLOR_ARRAY);
				glColor4d(rgba[0], rgba[1], rgba[2], rgba[3]);
			}
			else {
				glColor4f(0.0f, 0.0f, 0.0f, 1.0f);
				glEnableClientState(GL_COLOR_ARRAY);
			}
		}
		else
			glColor4f(0.0f, 0.0f, 0.0f, 1.0f);

		glVertexPointer(3, GL_FLOAT, stride, it.vertex->getXYZ());
		glNormalPointer(GL_FLOAT, stride, it.vertex->getNormal());
		if (!wireframe) {
			TexCoordPtr(it.vertex);
			if (glIsEnabled(GL_COLOR_ARRAY))
				glColorPointer(4, GL_UNSIGNED_BYTE, stride, it.vertex->getRGBA());
		}

		// here the actual drawing takes places
		glDrawElements(drawmode, it.totindex, GL_UNSIGNED_SHORT, it.index);
	}
	
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);
	if (!wireframe) {
		glDisableClientState(GL_COLOR_ARRAY);
		EnableTextures(false);
	}
}

void RAS_StorageVA::TexCoordPtr(const RAS_TexVert *tv)
{
	/* note: this function must closely match EnableTextures to enable/disable
	 * the right arrays, otherwise coordinate and attribute pointers from other
	 * materials can still be used and cause crashes */
	int unit;

	if (GLEW_ARB_multitexture)
	{
		for (unit = 0; unit < *m_texco_num; unit++)
		{
			glClientActiveTextureARB(GL_TEXTURE0_ARB+unit);
			switch (m_texco[unit])
			{
				case RAS_IRasterizer::RAS_TEXCO_ORCO:
				case RAS_IRasterizer::RAS_TEXCO_GLOB:
					glTexCoordPointer(3, GL_FLOAT, sizeof(RAS_TexVert),tv->getXYZ());
					break;
				case RAS_IRasterizer::RAS_TEXCO_UV:
					glTexCoordPointer(2, GL_FLOAT, sizeof(RAS_TexVert),tv->getUV(unit));
					break;
				case RAS_IRasterizer::RAS_TEXCO_NORM:
					glTexCoordPointer(3, GL_FLOAT, sizeof(RAS_TexVert),tv->getNormal());
					break;
				case RAS_IRasterizer::RAS_TEXTANGENT:
					glTexCoordPointer(4, GL_FLOAT, sizeof(RAS_TexVert),tv->getTangent());
					break;
				default:
					break;
			}
		}

		glClientActiveTextureARB(GL_TEXTURE0_ARB);
	}

	if (GLEW_ARB_vertex_program) {
		int uv = 0;
		for (unit = 0; unit < *m_attrib_num; unit++) {
			switch (m_attrib[unit]) {
				case RAS_IRasterizer::RAS_TEXCO_ORCO:
				case RAS_IRasterizer::RAS_TEXCO_GLOB:
					glVertexAttribPointerARB(unit, 3, GL_FLOAT, GL_FALSE, sizeof(RAS_TexVert), tv->getXYZ());
					break;
				case RAS_IRasterizer::RAS_TEXCO_UV:
					glVertexAttribPointerARB(unit, 2, GL_FLOAT, GL_FALSE, sizeof(RAS_TexVert), tv->getUV(uv++));
					break;
				case RAS_IRasterizer::RAS_TEXCO_NORM:
					glVertexAttribPointerARB(unit, 3, GL_FLOAT, GL_FALSE, sizeof(RAS_TexVert), tv->getNormal());
					break;
				case RAS_IRasterizer::RAS_TEXTANGENT:
					glVertexAttribPointerARB(unit, 4, GL_FLOAT, GL_FALSE, sizeof(RAS_TexVert), tv->getTangent());
					break;
				case RAS_IRasterizer::RAS_TEXCO_VCOL:
					glVertexAttribPointerARB(unit, 4, GL_UNSIGNED_BYTE, GL_FALSE, sizeof(RAS_TexVert), tv->getRGBA());
					break;
				default:
					break;
			}
		}
	}
}

void RAS_StorageVA::EnableTextures(bool enable)
{
	RAS_IRasterizer::TexCoGen *texco, *attrib;
	int unit, texco_num, attrib_num;

	/* we cache last texcoords and attribs to ensure we disable the ones that
	 * were actually last set */
	if (enable) {
		texco = m_texco;
		texco_num = *m_texco_num;
		attrib = m_attrib;
		attrib_num = *m_attrib_num;
		
		memcpy(m_last_texco, m_texco, sizeof(RAS_IRasterizer::TexCoGen)*(*m_texco_num));
		m_last_texco_num = *m_texco_num;
		memcpy(m_last_attrib, m_attrib, sizeof(RAS_IRasterizer::TexCoGen)*(*m_attrib_num));
		m_last_attrib_num = *m_attrib_num;
	}
	else {
		texco = m_last_texco;
		texco_num = m_last_texco_num;
		attrib = m_last_attrib;
		attrib_num = m_last_attrib_num;
	}

	if (GLEW_ARB_multitexture) {
		for (unit = 0; unit < texco_num; unit++) {
			glClientActiveTextureARB(GL_TEXTURE0_ARB + unit);

			switch (texco[unit])
			{
				case RAS_IRasterizer::RAS_TEXCO_ORCO:
				case RAS_IRasterizer::RAS_TEXCO_GLOB:
				case RAS_IRasterizer::RAS_TEXCO_UV:
				case RAS_IRasterizer::RAS_TEXCO_NORM:
				case RAS_IRasterizer::RAS_TEXTANGENT:
					if (enable) glEnableClientState(GL_TEXTURE_COORD_ARRAY);
					else glDisableClientState(GL_TEXTURE_COORD_ARRAY);
					break;
				default:
					glDisableClientState(GL_TEXTURE_COORD_ARRAY);
					break;
			}
		}

		glClientActiveTextureARB(GL_TEXTURE0_ARB);
	}
	else {
		if (texco_num) {
			if (enable) glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			else glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		}
	}

	if (GLEW_ARB_vertex_program) {
		for (unit = 0; unit < attrib_num; unit++) {
			switch (attrib[unit]) {
				case RAS_IRasterizer::RAS_TEXCO_ORCO:
				case RAS_IRasterizer::RAS_TEXCO_GLOB:
				case RAS_IRasterizer::RAS_TEXCO_UV:
				case RAS_IRasterizer::RAS_TEXCO_NORM:
				case RAS_IRasterizer::RAS_TEXTANGENT:
				case RAS_IRasterizer::RAS_TEXCO_VCOL:
					if (enable) glEnableVertexAttribArrayARB(unit);
					else glDisableVertexAttribArrayARB(unit);
					break;
				default:
					glDisableVertexAttribArrayARB(unit);
					break;
			}
		}
	}

	if (!enable) {
		m_last_texco_num = 0;
		m_last_attrib_num = 0;
	}
}


#ifndef _GPU_MATRIX_H_
#define _GPU_MATRIX_H_

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
 * Contributor(s): Alexandr Kuznetsov, Jason Wilkins
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file source/blender/gpu/GPU_matrix.h
 *  \ingroup gpu
 */

#include "GPU_glew.h"



#ifdef __cplusplus
extern "C" {
#endif



void gpuPushMatrix(void);
void gpuPopMatrix(void);

void   gpuMatrixMode(GLenum mode);
GLenum gpuGetMatrixMode(void);

void gpuLoadMatrix(const GLfloat m[16]);
const GLfloat* gpuGetMatrix(GLenum type, GLfloat m[16]);

void gpuLoadIdentity(void);

void gpuMultMatrix(const GLfloat m[16]);
void gpuMultMatrixd(const GLdouble m[16]);

void gpuTranslate(GLfloat x, GLfloat y, GLfloat z);
void gpuScale(GLfloat x, GLfloat y, GLfloat z);
void gpuRotateVector(GLfloat deg, GLfloat vector[3]);
void gpuRotateAxis(GLfloat deg, char axis);
void gpuRotateRight(char type);

void gpuOrtho(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat nearVal, GLfloat farVal);
void gpuFrustum(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat nearVal, GLfloat farVal);

void gpuLoadOrtho(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat nearVal, GLfloat farVal);
void gpuLoadFrustum(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat nearVal, GLfloat farVal);

void gpuLookAt(GLfloat eyeX, GLfloat eyeY, GLfloat eyeZ, GLfloat centerX, GLfloat centerY, GLfloat centerZ, GLfloat upX, GLfloat upY, GLfloat upZ);

void gpuProject(const GLfloat obj[3], const GLfloat model[16], const GLfloat proj[16], const GLint view[4], GLfloat win[3]);
GLboolean gpuUnProject(const GLfloat win[3], const GLfloat model[16], const GLfloat proj[16], const GLint view[4], GLfloat obj[3]);

void GPU_feedback_vertex_3fv(GLenum type, GLfloat x, GLfloat y, GLfloat z,            GLfloat out[3]);
void GPU_feedback_vertex_4fv(GLenum type, GLfloat x, GLfloat y, GLfloat z, GLfloat w, GLfloat out[4]);
void GPU_feedback_vertex_4dv(GLenum type, GLdouble x, GLdouble y, GLdouble z, GLdouble w, GLdouble out[4]);



#if defined(GLEW_ES_ONLY)

/* ES 2.0 doesn't define these symbolic constants, but the matrix stack replacement library emulates them
 * (GL core has deprecated matrix stacks, but it should still be in the header) */

#ifndef GL_MODELVIEW_MATRIX
#define GL_MODELVIEW_MATRIX 0x0BA6
#endif

#ifndef GL_PROJECTION_MATRIX
#define GL_PROJECTION_MATRIX 0x0BA7
#endif

#ifndef GL_TEXTURE_MATRIX
#define GL_TEXTURE_MATRIX 0x0BA8
#endif

#endif



#ifdef __cplusplus
}
#endif

#endif /* GPU_MATRIX_H */

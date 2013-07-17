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
* Contributor(s): 
*
* ***** END GPL LICENSE BLOCK *****
*/

/** \file blender/gpu/GPU_matrix.h
*  \ingroup gpu
*/

#ifndef GPU_MATRIX_H
#define GPU_MATRIX_H

#include "intern/gpu_glew.h"

#ifdef __cplusplus
extern "C" {
#endif

void GPU_matrix_forced_update(void);

void GPU_ms_init(void);
void GPU_ms_exit(void);

void gpuMatrixLock(void);
void gpuMatrixUnlock(void);

void gpuMatrixCommit(void);

void gpuPushMatrix(void);
void gpuPopMatrix(void);

void gpuMatrixMode(GLenum mode);
GLenum gpuGetMatrixMode(void);

void gpuLoadMatrix(const GLfloat * m);
const GLfloat * gpuGetMatrix(GLenum type, GLfloat * m);

void gpuLoadIdentity(void);

void gpuMultMatrix(const GLfloat *m);
void gpuMultMatrixd(const double *m);

void gpuTranslate(GLfloat x, GLfloat y, GLfloat z);
void gpuScale(GLfloat x, GLfloat y, GLfloat z);
void gpuRotateVector(GLfloat angle, GLfloat * vector);
void gpuRotateAxis(GLfloat angle, char axis);
void gpuRotateRight(char type);

void gpuOrtho(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat nearVal, GLfloat farVal);
void gpuFrustum(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat nearVal, GLfloat farVal);

void gpuLoadOrtho(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat nearVal, GLfloat farVal);
void gpuLoadFrustum(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat nearVal, GLfloat farVal);

void gpuLookAt(GLfloat eyeX, GLfloat eyeY, GLfloat eyeZ, GLfloat centerX, GLfloat centerY, GLfloat centerZ, GLfloat upX, GLfloat upY, GLfloat upZ);

void gpuProject(const GLfloat obj[3], const GLfloat model[4][4], const GLfloat proj[4][4], const GLint view[4], GLfloat win[3]);
int gpuUnProject(const GLfloat win[3], const GLfloat model[4][4], const GLfloat proj[4][4], const GLint view[4], GLfloat obj[3]);


#ifndef GPU_MAT_CAST_ANY
#define GPU_MAT_CAST_ANY 1
#endif

#if GPU_MAT_CAST_ANY

#define gpuLoadMatrix(m)  gpuLoadMatrix((const GLfloat *)(m))
#define gpuMultMatrix(m)  gpuMultMatrix((const GLfloat *)(m))
#define gpuMultMatrixd(m) gpuMultMatrixd((const double *)(m))

#define gpuProject(o, m, p, v, w)   gpuProject   (o, (const GLfloat (*)[4])(m), (const GLfloat (*)[4])(p), v, w)
#define gpuUnProject(w, m, p, v, o) gpuUnProject (w, (const GLfloat (*)[4])(m), (const GLfloat (*)[4])(p), v, o)

#endif



#ifdef __cplusplus
}
#endif


#endif /* GPU_MATRIX_H */

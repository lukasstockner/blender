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
 * Contributor(s): Jason Wilkins
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file GPU_primitives.h
 *  \ingroup gpu
 */
 
#ifndef __GPU_PRIMITIVES_H__
#define __GPU_PRIMITIVES_H__



#include <GL/glew.h>



#ifdef __cplusplus
extern "C" {
#endif



void gpuSingleLinef(GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2);
void gpuSingleLinei(GLint x1, GLint y1, GLint x2, GLint y2);

void gpuSingleRectf(GLenum mode, GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2);
void gpuSingleRecti(GLenum mode, GLint x1, GLint y1, GLint x2, GLint y2);



void gpuAppendArc(
	GLfloat x,
	GLfloat y,
	GLfloat start,
	GLfloat angle,
	GLfloat xradius,
	GLfloat yradius,
	GLint   nsegments);

void gpuDrawArc(
	GLfloat x,
	GLfloat y,
	GLfloat start,
	GLfloat angle,
	GLfloat xradius,
	GLfloat yradius,
	GLint nsegments);

void gpuSingleArc(
	GLfloat x,
	GLfloat y,
	GLfloat start,
	GLfloat angle,
	GLfloat xradius,
	GLfloat yradius,
	GLint nsegments);



void gpuAppendFastCircleXZ(GLfloat radius);
void gpuDrawFastCircleXZ(GLfloat radius);
void gpuSingleFastCircleXZ(GLfloat radius);

void gpuAppendFastCircleXY(GLfloat radius);
void gpuDrawFastCircleXY(GLfloat radius);
void gpuSingleFastCircleXY(GLfloat radius);


void gpuAppendFastBall(
	const GLfloat position[3],
	float radius,
	const GLfloat matrix[4][4]);

void gpuDrawFastBall(
	int mode,
	const GLfloat position[3],
	float radius,
	const GLfloat matrix[4][4]);

void gpuSingleFastBall(
	int mode,
	const GLfloat position[3],
	float radius,
	const GLfloat matrix[4][4]);



void gpuAppendSpiral(
	const GLfloat position[3],
	float radius,
	const GLfloat matrix[4][4],
	int start);

void gpuDrawSpiral(
	const GLfloat position[3],
	GLfloat radius,
	GLfloat matrix[4][4],
	int start);

void gpuSingleSpiral(
	const GLfloat position[3],
	GLfloat radius,
	GLfloat matrix[4][4],
	int start);



void gpuAppendDisk(GLfloat x, GLfloat y, GLfloat radius, GLint nsectors);
void gpuDrawDisk(GLfloat x, GLfloat y, GLfloat radius, GLint nsectors);
void gpuSingleDisk(GLfloat x, GLfloat y, GLfloat radius, GLint nsectors);



void gpuBeginSprites(void);
void gpuSprite3fv(const GLfloat v[3]);
void gpuSprite3f(GLfloat x, GLfloat y, GLfloat z);
void gpuSprite2f(GLfloat x, GLfloat y);
void gpuSprite2fv(const GLfloat vec[2]);
void gpuEndSprites(void);



void gpuAppendCone(
	GLfloat   height,
	GLfloat   baseWidth,
	GLint     slices,
	GLboolean isFilled);

void gpuDrawCone(
	GLfloat   height,
	GLfloat   baseWidth,
	GLint     slices,
	GLboolean isFilled);

void gpuSingleCone(
	GLfloat   height,
	GLfloat   baseWidth,
	GLint     slices,
	GLboolean isFilled);



#ifdef __cplusplus
}
#endif



#include "intern/gpu_primitives_inline.h"



#endif /* __GPU_PRIMITIVES_H_ */

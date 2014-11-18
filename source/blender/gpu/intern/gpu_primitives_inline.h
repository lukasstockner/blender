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

#ifndef __GPU_PRIMITIVES_INLINE_H__
#define __GPU_PRIMITIVES_INLINE_H__


#include "BLI_utildefines.h"
#include <math.h>


#ifdef __cplusplus
extern "C" {
#endif


BLI_INLINE void gpuAppendLinef(GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2)
{
	glVertex2f(x1, y1);
	glVertex2f(x2, y2);
}

BLI_INLINE void gpuAppendLinei(GLint x1, GLint y1, GLint x2, GLint y2)
{
	glVertex2i(x1, y1);
	glVertex2i(x2, y2);
}

BLI_INLINE void gpuDrawLinef(GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2)
{
	glBegin(GL_LINES);
	gpuAppendLinef(x1, y1, x2, y2);
	glEnd();
}

BLI_INLINE void gpuDrawLinei(GLint x1, GLint y1, GLint x2, GLint y2)
{
	glBegin(GL_LINES);
	gpuAppendLinei(x1, y1, x2, y2);
	glEnd();
}



BLI_INLINE void gpuAppendFilledRectf(GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2)
{
	glVertex2f(x1, y1);
	glVertex2f(x2, y1);
	glVertex2f(x2, y2);

	glVertex2f(x1, y1);
	glVertex2f(x2, y2);
	glVertex2f(x1, y2);
}

BLI_INLINE void gpuAppendFilledRecti(GLint x1, GLint y1, GLint x2, GLint y2)
{
	glVertex2i(x1, y1);
	glVertex2i(x2, y1);
	glVertex2i(x2, y2);

	glVertex2i(x1, y1);
	glVertex2i(x2, y2);
	glVertex2i(x1, y2);
}

BLI_INLINE void gpuAppendWireRectf(
	GLfloat x1,
	GLfloat y1,
	GLfloat x2,
	GLfloat y2)
{
	glVertex2f(x1, y1);
	glVertex2f(x2, y1);
	glVertex2f(x2, y2);
	glVertex2f(x1, y2);
}

BLI_INLINE void gpuAppendWireRecti(
	GLint x1,
	GLint y1,
	GLint x2,
	GLint y2)
{
	glVertex2i(x1, y1);
	glVertex2i(x2, y1);
	glVertex2i(x2, y2);
	glVertex2i(x1, y2);
}

BLI_INLINE void gpuDrawFilledRectf(
	GLfloat x1,
	GLfloat y1,
	GLfloat x2,
	GLfloat y2)
{
	glBegin(GL_TRIANGLES);
	gpuAppendFilledRectf(x1, y1, x2, y2);
	glEnd();
}

BLI_INLINE void gpuDrawFilledRecti(
	GLint x1,
	GLint y1,
	GLint x2,
	GLint y2)
{
	glBegin(GL_TRIANGLES);
	gpuAppendFilledRecti(x1, y1, x2, y2);
	glEnd();
}

BLI_INLINE void gpuDrawWireRectf(
	GLfloat x1,
	GLfloat y1,
	GLfloat x2,
	GLfloat y2)
{
	glBegin(GL_LINE_LOOP);
	gpuAppendWireRectf(x1, y1, x2, y2);
	glEnd();
}

BLI_INLINE void gpuDrawWireRecti(
	GLint x1,
	GLint y1,
	GLint x2,
	GLint y2)
{
	glBegin(GL_LINE_LOOP);
	gpuAppendWireRecti(x1, y1, x2, y2);
	glEnd();
}



BLI_INLINE void gpuAppendEllipse(
	GLfloat x,
	GLfloat y,
	GLfloat xradius,
	GLfloat yradius,
	GLint   nsegments)
{
	gpuAppendArc(x, y, 0, (float)(2.0 * M_PI), xradius, yradius, nsegments);
}

BLI_INLINE void gpuDrawEllipse(
	GLfloat x,
	GLfloat y,
	GLfloat xradius,
	GLfloat yradius,
	GLint nsegments)
{
	gpuDrawArc(x, y, 0, (float)(2.0 * M_PI), xradius, yradius, nsegments);
}

BLI_INLINE void gpuSingleEllipse(
	GLfloat x,
	GLfloat y,
	GLfloat xradius,
	GLfloat yradius,
	GLint nsegments)
{
	gpuSingleArc(x, y, 0, (float)(2.0 * M_PI), xradius, yradius, nsegments);
}



BLI_INLINE void gpuAppendCircle(
	GLfloat x,
	GLfloat y,
	GLfloat radius,
	GLint   nsegments)
{
	gpuAppendEllipse(x, y, radius, radius, nsegments);
}

BLI_INLINE void gpuDrawCircle(
	GLfloat x,
	GLfloat y,
	GLfloat radius,
	GLint nsegments)
{
	gpuDrawEllipse(x, y, radius, radius, nsegments);
}

/**
 * Draw a lined (non-looping) arc with the given
 * \a radius, starting at angle \a start and arcing
 * through \a angle. The arc is centered at the origin
 * and drawn in the XY plane.
 *
 * \param start The initial angle (in radians).
 * \param angle The length of the arc (in radians).
 * \param radius The arc radius.
 * \param nsegments The number of segments to use in drawing the arc.
 */
BLI_INLINE void gpuSingleCircle(
	GLfloat x,
	GLfloat y,
	GLfloat radius,
	GLint nsegments)
{
	gpuSingleEllipse(x, y, radius, radius, nsegments);
}



#ifdef __cplusplus
}
#endif



#endif /* __GPU_PRIMITIVES_INLINE_H_ */
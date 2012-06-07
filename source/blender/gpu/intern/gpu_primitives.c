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

/** \file GPU_lighting.c
 *  \ingroup gpu
 */

#include "GPU_primitives.h"



void gpuSingleLinef(GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2)
{
	gpuImmediateFormat_V2();
	gpuDrawLinef(x1, y1, x2, y2);
	gpuImmediateUnformat();
}

void gpuSingleLinei(GLint x1, GLint y1, GLint x2, GLint y2)
{
	gpuImmediateFormat_V2();
	gpuDrawLinei(x1, y1, x2, y2);
	gpuImmediateUnformat();
}



void gpuSingleRectf(GLenum mode, GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2)
{
	gpuImmediateFormat_V2();
	gpuDrawRectf(mode, x1, y1, x2, y2);
	gpuImmediateUnformat();
}

void gpuSingleRecti(GLenum mode, GLint x1, GLint y1, GLint x2, GLint y2)
{
	gpuImmediateFormat_V2();
	gpuDrawRecti(mode, x1, y1, x2, y2);
	gpuImmediateUnformat();
}



//void gpuAppendLitSweep(
//	GLfloat x,
//	GLfloat y,
//	GLfloat z,
//	GLfloat height,
//	GLfloat radiusBot,
//	GLfloat radiusTop,
//	GLfloat startAngle,
//	GLfloat sweepAngle,
//	GLint sectors)
//{
//	int i;
//
//	const GLfloat dr = radiusTop - radiusBot;
//	const GLfloat zheight = z+height;
//	GLfloat nz = cosf(atan2(height, dr));
//	GLfloat ns = 1.0f / sqrtf(nz*nz + 1);
//
//	GPU_ASSERT(sectors > 0);
//
//	for (i = 0; i <= sectors; i++) {
//		GLfloat a = startAngle  +  i * sweepAngle / sectors;
//		GLfloat c = cosf(a);
//		GLfloat s = sinf(a);
//		GLfloat n[3] = { c, s, nz };
//
//		mul_v3_fl(n, ns);
//
//		if (normals) {
//			glNormal3fv(n);
//		}
//
//		glVertex3f(radiusBot * c + x, radiusBot * s + y, z);
//
//		if (normals) {
//			glNormal3fv(n);
//		}
//
//		glVertex3f(radiusTop * c + x, radiusTop * s + y, zheight);
//	}
//}



void gpuAppendArc(
	GLfloat x,
	GLfloat y,
	GLfloat start,
	GLfloat angle,
	GLfloat xradius,
	GLfloat yradius,
	GLint   nsegments)
{
	int i;

	GPU_CHECK_MODE(GL_LINE_STRIP);

	GPU_ASSERT(nsegments > 0);

	for (i = 0; i <= nsegments; i++) {
		const GLfloat t = (GLfloat)i / (GLfloat)nsegments;
		GLfloat cur = t*angle + start;

		gpuVertex2f(cosf(cur)*xradius + x, sinf(cur)*yradius + y);
	}
}

void gpuDrawArc(
	GLfloat x,
	GLfloat y,
	GLfloat start,
	GLfloat angle,
	GLfloat xradius,
	GLfloat yradius,
	GLint nsegments)
{
	gpuBegin(GL_LINE_STRIP);
	gpuAppendArc(x, y, start, angle, xradius, yradius, nsegments);
	gpuEnd();
}

void gpuSingleArc(
	GLfloat x,
	GLfloat y,
	GLfloat start,
	GLfloat angle,
	GLfloat xradius,
	GLfloat yradius,
	GLint nsegments)
{
	gpuImmediateFormat_V2();
	gpuDrawArc(x, y, start, angle, xradius, yradius, nsegments);
	gpuImmediateUnformat();
}


void gpuImmediateSingleDraw(GLenum mode, GPUimmediate *restrict immediate)
{
	GPUimmediate* oldImmediate = GPU_IMMEDIATE;

	GPU_IMMEDIATE = immediate;
	gpuImmediateLock();
	gpuDraw(mode);
	gpuImmediateUnlock();
	GPU_IMMEDIATE = oldImmediate;
}

/* ----------------- OpenGL Circle Drawing - Tables for Optimized Drawing Speed ------------------ */
/* 32 values of sin function (still same result!) */
#define CIRCLE_RESOL 32

static const GLfloat sinval[CIRCLE_RESOL] = {
	0.00000000,
	0.20129852,
	0.39435585,
	0.57126821,
	0.72479278,
	0.84864425,
	0.93775213,
	0.98846832,
	0.99871650,
	0.96807711,
	0.89780453,
	0.79077573,
	0.65137248,
	0.48530196,
	0.29936312,
	0.10116832,
	-0.10116832,
	-0.29936312,
	-0.48530196,
	-0.65137248,
	-0.79077573,
	-0.89780453,
	-0.96807711,
	-0.99871650,
	-0.98846832,
	-0.93775213,
	-0.84864425,
	-0.72479278,
	-0.57126821,
	-0.39435585,
	-0.20129852,
	0.00000000
};

/* 32 values of cos function (still same result!) */
static const GLfloat cosval[CIRCLE_RESOL] = {
	1.00000000,
	0.97952994,
	0.91895781,
	0.82076344,
	0.68896691,
	0.52896401,
	0.34730525,
	0.15142777,
	-0.05064916,
	-0.25065253,
	-0.44039415,
	-0.61210598,
	-0.75875812,
	-0.87434661,
	-0.95413925,
	-0.99486932,
	-0.99486932,
	-0.95413925,
	-0.87434661,
	-0.75875812,
	-0.61210598,
	-0.44039415,
	-0.25065253,
	-0.05064916,
	0.15142777,
	0.34730525,
	0.52896401,
	0.68896691,
	0.82076344,
	0.91895781,
	0.97952994,
	1.00000000
};

/* draws a circle on x-z plane given the scaling of the circle, assuming that 
 * all required matrices have been set (used for drawing empties)
 */
void gpuAppendFastCircleXZ(GLfloat radius)
{
	int i;

	/* coordinates are: cos(i*11.25) = x, 0 = y, sin(i*11.25) = z */
	for (i = 0; i < CIRCLE_RESOL; i++) {
		gpuVertex3f(cosval[i] * radius, 0, sinval[i] * radius);
	}
}

void gpuAppendFastCircleXY(GLfloat radius)
{
	int i;

	/* coordinates are: cos(i*11.25) = x, sin(i*11.25) = y, z=0 */
	for (i = 0; i < CIRCLE_RESOL; i++) {
		gpuVertex3f(cosval[i] * radius, sinval[i] * radius, 0);
	}
}


void gpuDrawFastCircleXZ(GLfloat radius)
{
	gpuBegin(GL_LINE_LOOP);
	gpuAppendFastCircleXZ(radius);
	gpuEnd();
}

void gpuDrawFastCircleXY(GLfloat radius)
{
	gpuBegin(GL_LINE_LOOP);
	gpuAppendFastCircleXY(radius);
	gpuEnd();
}

void gpuSingleFastCircleXZ(GLfloat radius)
{
	gpuImmediateFormat_V3();
	gpuDrawFastCircleXZ(radius);
	gpuImmediateUnformat();
}

void gpuSingleFastCircleXY(GLfloat radius)
{
	gpuImmediateFormat_V3();
	gpuDrawFastCircleXY(radius);
	gpuImmediateUnformat();
}

void gpuAppendFastBall(
	const GLfloat position[3],
	GLfloat radius,
	const GLfloat matrix[4][4])
{
	GLfloat vx[3], vy[3];
	GLuint a;

	mul_v3_v3fl(vx, matrix[0], radius);
	mul_v3_v3fl(vy, matrix[1], radius);

	for (a = 0; a < CIRCLE_RESOL; a++) {
		gpuVertex3f(
			position[0] + sinval[a] * vx[0] + cosval[a] * vy[0],
			position[1] + sinval[a] * vx[1] + cosval[a] * vy[1],
			position[2] + sinval[a] * vx[2] + cosval[a] * vy[2]);
	}
}

void gpuDrawFastBall(
	int mode,
	const GLfloat position[3],
	GLfloat radius,
	const GLfloat matrix[4][4])
{
	gpuBegin(mode);
	gpuAppendFastBall(position, radius, matrix);
	gpuEnd();
}

void gpuSingleFastBall(
	int mode,
	const GLfloat position[3],
	GLfloat radius,
	const GLfloat matrix[4][4])
{
	gpuImmediateFormat_V3();
	gpuBegin(mode);
	gpuAppendFastBall(position, radius, matrix);
	gpuEnd();
	gpuImmediateUnformat();
}

void gpuAppendSpiral(
	const GLfloat position[3],
	GLfloat radius,
	const GLfloat matrix[4][4],
	int start)
{
	GLfloat vec[3], vx[3], vy[3];
	const GLfloat tot_inv = (1.0f / (GLfloat)CIRCLE_RESOL);
	int a;
	char inverse = FALSE;
	GLfloat x, y, fac;

	if (start < 0) {
		inverse = TRUE;
		start = -start;
	}

	mul_v3_v3fl(vx, matrix[0], radius);
	mul_v3_v3fl(vy, matrix[1], radius);

	if (inverse == 0) {
		copy_v3_v3(vec, position);
		gpuVertex3fv(vec);

		for (a = 0; a < CIRCLE_RESOL; a++) {
			if (a + start >= CIRCLE_RESOL)
				start = -a + 1;

			fac = (GLfloat)a * tot_inv;
			x = sinval[a + start] * fac;
			y = cosval[a + start] * fac;

			vec[0] = position[0] + (x * vx[0] + y * vy[0]);
			vec[1] = position[1] + (x * vx[1] + y * vy[1]);
			vec[2] = position[2] + (x * vx[2] + y * vy[2]);

			gpuVertex3fv(vec);
		}
	}
	else {
		fac = (GLfloat)(CIRCLE_RESOL - 1) * tot_inv;
		x = sinval[start] * fac;
		y = cosval[start] * fac;

		vec[0] = position[0] + (x * vx[0] + y * vy[0]);
		vec[1] = position[1] + (x * vx[1] + y * vy[1]);
		vec[2] = position[2] + (x * vx[2] + y * vy[2]);

		gpuVertex3fv(vec);

		for (a = 0; a < CIRCLE_RESOL; a++) {
			if (a + start >= CIRCLE_RESOL)
				start = -a + 1;

			fac = (GLfloat)(-a + (CIRCLE_RESOL - 1)) * tot_inv;
			x = sinval[a + start] * fac;
			y = cosval[a + start] * fac;

			vec[0] = position[0] + (x * vx[0] + y * vy[0]);
			vec[1] = position[1] + (x * vx[1] + y * vy[1]);
			vec[2] = position[2] + (x * vx[2] + y * vy[2]);
			gpuVertex3fv(vec);
		}
	}
}

void gpuDrawSpiral(
	const GLfloat position[3],
	GLfloat radius,
	GLfloat matrix[4][4],
	int start)
{
	gpuBegin(GL_LINE_STRIP);
	gpuAppendSpiral(position, radius, matrix, start);
	gpuEnd();
}

void gpuSingleSpiral(
	const GLfloat position[3],
	GLfloat radius,
	GLfloat matrix[4][4],
	int start)
{
	gpuImmediateFormat_V3();
	gpuDrawSpiral(position, radius, matrix, start);
	gpuImmediateUnformat();
}

void gpuAppendDisk(
	GLfloat x,
	GLfloat y,
	GLfloat radius,
	GLint   nsectors)
{
	int i;
	GLfloat x0 = 0, y0 = 0;
	GLfloat x1, y1;

	GPU_CHECK_MODE(GL_TRIANGLES);
	GPU_ASSERT(nsectors > 0);

	for (i = 0; i <= nsectors; i++) {
		GLfloat angle = (GLfloat)(2.0*i*M_PI / nsectors);

		GLfloat c = cosf(angle)*radius + x;
		GLfloat s = sinf(angle)*radius + y;

		if (i == 0) {
			x0 = c;
			y0 = s;
		}
		else {
			x1 = c;
			y1 = s;

			gpuVertex2f(x, y);
			gpuVertex2f(x0, y0);
			gpuVertex2f(x1, y1);

			x0 = x1;
			y0 = y1;
		}
	}
}

void gpuDrawDisk(
	GLfloat x,
	GLfloat y,
	GLfloat radius,
	GLint   nsectors)
{
	gpuBegin(GL_TRIANGLES);
	gpuAppendDisk(x, y, radius, nsectors);
	gpuEnd();
}

void gpuSingleDisk(
	GLfloat x,
	GLfloat y,
	GLfloat radius,
	GLint   nsectors)
{
	gpuImmediateFormat_V3();
	gpuDrawDisk(x, y, radius, nsectors);
	gpuImmediateUnformat();
}


// lit, solid, wire, solid w/ base, solid w/ end caps
//void gpuAppendCone(GLfloat baseRadius, GLfloat height, GLint )
//{
//	int i;
//
//	GPU_CHECK_MODE(GL_TRIANGLES);
//	GPU_ASSERT(nsectors > 0);
//
//	for (i = 0; i <= nsectors; i++) {
//		GLfloat x0, y0;
//		GLfloat x1, y1;
//		GLfloat angle = (GLfloat)(2.0*i*M_PI / nsectors);
//
//		GLfloat c = cosf(angle)*radius + x;
//		GLfloat s = sinf(angle)*radius + y;
//
//		if (i == 0) {
//			x0 = c;
//			y0 = s;
//		}
//		else {
//			x1 = c;
//			y1 = s;
//
//			gpuVertex2f(x, y);
//			gpuVertex2f(x0, y0);
//			gpuVertex2f(x1, y1);
//
//			x0 = x1;
//			y0 = y1;
//		}
//	}
//}
//
//void gpuAppendCylinder()
//{
//}



/* **************** GL_POINT hack ************************ */

static int pointhack = 0;

static GLubyte Squaredot[16] = {
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff};

void gpuBeginSprites(void)
{
	GLfloat range[4];
	glGetFloatv(GL_ALIASED_POINT_SIZE_RANGE, range);

	if (range[1] < 2.0f) {
		GLfloat size[4];
		glGetFloatv(GL_POINT_SIZE, size);

		pointhack = floor(size[0] + 0.5f);

		if (pointhack > 4) { //-V112
			pointhack = 4; //-V112
		}
	}
	else {
		gpuBegin(GL_POINTS);
	}
}

void gpuSprite3fv(const GLfloat vec[3])
{
	if (pointhack) {
		glRasterPos3fv(vec);
		glBitmap(
			pointhack,
			pointhack,
			pointhack / 2.0f, pointhack / 2.0f,
			0,
			0,
			Squaredot);
	}
	else {
		gpuVertex3fv(vec);
	}
}

void gpuSprite3f(GLfloat x, GLfloat y, GLfloat z)
{
	if (pointhack) {
		glRasterPos3f(x, y, z);
		glBitmap(
			pointhack,
			pointhack,
			pointhack / 2.0f,
			pointhack / 2.0f,
			0,
			0,
			Squaredot);
	}
	else {
		gpuVertex3f(x, y, z);
	}
}

void gpuSprite2f(GLfloat x, GLfloat y)
{
	if (pointhack) {
		glRasterPos2f(x, y);
		glBitmap(
			pointhack,
			pointhack,
			pointhack / 2.0f,
			pointhack / 2.0f,
			0,
			0,
			Squaredot);
	}
	else {
		gpuVertex2f(x, y);
	}
}

void gpuSprite2fv(const GLfloat vec[2])
{
	if (pointhack) {
		glRasterPos2fv(vec);
		glBitmap(
			pointhack,
			pointhack,
			pointhack / 2.0f,
			pointhack / 2.0f,
			0,
			0,
			Squaredot);
	}
	else {
		gpuVertex2fv(vec);
	}
}

void gpuEndSprites(void)
{
	if (pointhack) {
		pointhack = 0;
	}
	else {
		gpuEnd();
	}
}



void gpuAppendCone(
	GLfloat   height,
	GLfloat   baseRadius,
	GLint     slices,
	GLboolean isFilled)
{
	GLfloat s, c;
	GLfloat x[2], y[2];
	int i;

	const GLfloat half = height / 2;

	GPU_ASSERT(slices > 0);

	x[0] = baseRadius;
	y[0] = 0;

	for (i = 1; i <= slices; i++) {
		GLfloat angle = (float)(2.0*M_PI*i / slices);

		c = cosf(angle);
		s = sinf(angle);

		x[1] = c * baseRadius;
		y[1] = s * baseRadius;

		/* segment along base */
		gpuVertex3f(x[0], y[0], -half);
		gpuVertex3f(x[1], y[1], -half);

		if (!isFilled) {
			/* repeat vertex for next segment */
			gpuVertex3f(x[1], y[1], -half);
		}

		/* top of cone */
		gpuVertex3f(0, 0, half);

		x[0] = x[1];
		y[0] = y[1];
	}
}



void gpuDrawCone(
	GLfloat   height,
	GLfloat   baseRadius,
	GLint     slices,
	GLboolean isSolid)
{
	gpuBegin(isSolid ? GL_TRIANGLES : GL_LINES);
	gpuAppendCone(height, baseRadius, slices, isSolid);
	gpuEnd();
}



void gpuSingleCone(
	GLfloat   height,
	GLfloat   baseRadius,
	GLint     slices,
	GLboolean isFilled)
{
	gpuImmediateFormat_V3();
	gpuDrawCone(height, baseRadius, slices, isFilled);
	gpuImmediateUnformat();
}

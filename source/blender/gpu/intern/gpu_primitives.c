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
#include "GPU_immediate.h"

#include "BLI_math_vector.h"

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



void gpuSingleFilledRectf(GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2)
{
	gpuImmediateFormat_V2();
	gpuDrawFilledRectf(x1, y1, x2, y2);
	gpuImmediateUnformat();
}

void gpuSingleFilledRecti(GLint x1, GLint y1, GLint x2, GLint y2)
{
	gpuImmediateFormat_V2();
	gpuDrawFilledRecti(x1, y1, x2, y2);
	gpuImmediateUnformat();
}



void gpuSingleWireRectf(GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2)
{
	gpuImmediateFormat_V2();
	gpuDrawWireRectf(x1, y1, x2, y2);
	gpuImmediateUnformat();
}

void gpuSingleWireRecti(GLint x1, GLint y1, GLint x2, GLint y2)
{
	gpuImmediateFormat_V2();
	gpuDrawWireRecti(x1, y1, x2, y2);
	gpuImmediateUnformat();
}


#if 0 /* unused */
void gpuAppendLitSweep(
	GLfloat x,
	GLfloat y,
	GLfloat z,
	GLfloat height,
	GLfloat radiusBot,
	GLfloat radiusTop,
	GLfloat startAngle,
	GLfloat sweepAngle,
	GLint sectors)
{
	int i;

	const GLfloat dr = radiusTop - radiusBot;
	const GLfloat zheight = z + height;
	GLfloat nz = cosf(atan2(height, dr));
	GLfloat ns = 1.0f / sqrtf(nz * nz + 1);

	BLI_assert(sectors > 0);

	for (i = 0; i <= sectors; i++) {
		GLfloat a = startAngle + i * sweepAngle / sectors;
		GLfloat c = cosf(a);
		GLfloat s = sinf(a);
		GLfloat n[3] = { c, s, nz };

		mul_v3_fl(n, ns);

		if (normals) {
			glNormal3fv(n);
		}

		glVertex3f(radiusBot * c + x, radiusBot * s + y, z);

		if (normals) {
			glNormal3fv(n);
		}

		glVertex3f(radiusTop * c + x, radiusTop * s + y, zheight);
	}
}
#endif /* unused */


void gpuAppendArc(
	GLfloat x,
	GLfloat y,
	GLfloat start,
	GLfloat angle,
	GLfloat xradius,
	GLfloat yradius,
	GLint nsegments)
{
	int i;

	GPU_CHECK_MODE(GL_LINE_STRIP);

	BLI_assert(nsegments > 0);

	for (i = 0; i <= nsegments; i++) {
		const GLfloat t = (GLfloat)i / (GLfloat)nsegments;
		GLfloat cur = t * angle + start;

		gpuVertex2f(cosf(cur) * xradius + x, sinf(cur) * yradius + y);
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


void gpuImmediateSingleDraw(GLenum mode, GPUImmediate *immediate)
{
	GPUImmediate *oldImmediate = GPU_IMMEDIATE;

	GPU_IMMEDIATE = immediate;
	gpuImmediateLock();
	gpuDraw(mode);
	gpuImmediateUnlock();
	GPU_IMMEDIATE = oldImmediate;
}

void gpuImmediateSingleRepeat(GPUImmediate *immediate)
{
	GPUImmediate *oldImmediate = GPU_IMMEDIATE;

	GPU_IMMEDIATE = immediate;
	gpuImmediateLock();
	gpuRepeat();
	gpuImmediateUnlock();
	GPU_IMMEDIATE = oldImmediate;
}

void gpuImmediateSingleDrawElements(GLenum mode, GPUImmediate *immediate)
{
	GPUImmediate *oldImmediate = GPU_IMMEDIATE;

	GPU_IMMEDIATE = immediate;
	gpuImmediateLock();
	gpuDrawElements(mode);
	gpuImmediateUnlock();
	GPU_IMMEDIATE = oldImmediate;
}

void gpuImmediateSingleRepeatElements(GPUImmediate *immediate)
{
	GPUImmediate *oldImmediate = GPU_IMMEDIATE;

	GPU_IMMEDIATE = immediate;
	gpuImmediateLock();
	gpuRepeatElements();
	gpuImmediateUnlock();
	GPU_IMMEDIATE = oldImmediate;
}

void gpuImmediateSingleDrawRangeElements(GLenum mode, GPUImmediate *immediate)
{
	GPUImmediate *oldImmediate = GPU_IMMEDIATE;

	GPU_IMMEDIATE = immediate;
	gpuImmediateLock();
	gpuDrawRangeElements(mode);
	gpuImmediateUnlock();
	GPU_IMMEDIATE = oldImmediate;
}

void gpuImmediateSingleRepeatRangeElements(GPUImmediate *immediate)
{
	GPUImmediate *oldImmediate = GPU_IMMEDIATE;

	GPU_IMMEDIATE = immediate;
	gpuImmediateLock();
	gpuRepeatRangeElements();
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
 * all required matrices have been set (used for drawing empties) */
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
	GLfloat matrix[4][4],
	int start)
{
	GLfloat vec[3], vx[3], vy[3];
	const GLfloat tot_inv = 1.0f / (GLfloat)CIRCLE_RESOL;
	int a;
	char inverse = false;
	GLfloat x, y, fac;

	if (start < 0) {
		inverse = true;
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
	GLint nsectors)
{
	int i;
	GLfloat x0 = 0, y0 = 0;
	GLfloat x1, y1;

	GPU_CHECK_MODE(GL_TRIANGLES);
	BLI_assert(nsectors > 0);

	for (i = 0; i <= nsectors; i++) {
		GLfloat angle = (GLfloat)(2.0 * i * M_PI / nsectors);

		GLfloat c = cosf(angle) * radius + x;
		GLfloat s = sinf(angle) * radius + y;

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
	GLint nsectors)
{
	gpuBegin(GL_TRIANGLES);
	gpuAppendDisk(x, y, radius, nsectors);
	gpuEnd();
}

void gpuSingleDisk(
	GLfloat x,
	GLfloat y,
	GLfloat radius,
	GLint nsectors)
{
	gpuImmediateFormat_V3();
	gpuDrawDisk(x, y, radius, nsectors);
	gpuImmediateUnformat();
}


#if 0 /* unused */
/* lit, solid, wire, solid w/ base, solid w/ end caps */
void gpuAppendCone(GLfloat radiusBase, GLfloat height, GLint)
{
	int i;

	GPU_CHECK_MODE(GL_TRIANGLES);
	BLI_assert(nsectors > 0);

	for (i = 0; i <= nsectors; i++) {
		GLfloat x0, y0;
		GLfloat x1, y1;
		GLfloat angle = (GLfloat)(2.0 * i * M_PI / nsectors);

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

void gpuAppendCylinder()
{
}
#endif /* unused */


BLI_INLINE void primFormat(GPUprim3 *prim)
{
	BLI_assert(
		ELEM(prim->normals,
			GPU_NORMALS_SMOOTH,
			GPU_NORMALS_FLAT,
			GPU_NORMALS_NONE));

	switch (prim->normals) {
		case GPU_NORMALS_NONE:
			gpuImmediateFormat_V3();
			break;

		case GPU_NORMALS_SMOOTH:
		case GPU_NORMALS_FLAT:
		default:
			gpuImmediateFormat_N3_V3();
			break;
	}
}

BLI_INLINE void primDraw(GPUprim3 *prim)
{
	BLI_assert(
		ELEM(prim->drawStyle,
			GPU_DRAW_STYLE_FILL,
			GPU_DRAW_STYLE_SILHOUETTE,
			GPU_DRAW_STYLE_LINES,
			GPU_DRAW_STYLE_POINTS));

	switch (prim->drawStyle) {
		case GPU_DRAW_STYLE_FILL:
			gpuDrawElements(GL_TRIANGLES);
			break;

		case GPU_DRAW_STYLE_SILHOUETTE:
		case GPU_DRAW_STYLE_LINES:
			gpuDrawElements(GL_LINES);
			break;

		case GPU_DRAW_STYLE_POINTS:
			gpuDraw(GL_POINTS);
			break;

		default:
			break;
	}
}

static GLfloat sweep(GPUprim3 *prim, GLfloat z)
{
	float l[3];
	float l0[3];
	float lz;
	float d;
	float x[3];
	float p0[3];
	float r[3];

	copy_v3_v3(l0, prim->params.sweep.point1);
	sub_v3_v3v3(l, prim->params.sweep.point2, l0);
	lz = prim->params.sweep.point1[2];
	d = (z - lz) / l[2];
	madd_v3_v3v3fl(x, l0, l, d);
	copy_v3_flflfl(p0, 0, 0, z);
	sub_v3_v3v3(r, p0, x);
	return len_v3(r);
}

static GLfloat sphere(GPUprim3 *prim, GLfloat z)
{
	float a = z / prim->params.sphere.radius;
	float b = asinf(a);
	return prim->params.sphere.radius * cosf(b);
}

static void sphereNormals(
	GLfloat no[GPU_MAX_SEGS+1][GPU_MAX_SEGS+1][3],
	GLfloat co[GPU_MAX_SEGS+1][GPU_MAX_SEGS+1][3],
	int usegs,
	int vsegs)
{
	int i, j;

	for (j = 0; j <= vsegs; j++) {
		for (i = 0; i <= usegs; i++) {
			normalize_v3_v3(no[j][i], co[j][i]);
		}
	}
}

BLI_INLINE float modfi(float a, int *b)
{
	double c, d;
	d = modf(a, &c);
	*b = (int)c;
	return d;
}

typedef void (*calc_normals_func)(
	GLfloat out[GPU_MAX_SEGS+1][GPU_MAX_SEGS+1][3],
	GLfloat pos[GPU_MAX_SEGS+1][GPU_MAX_SEGS+1][3],
	int usegs,
	int vsegs);

BLI_INLINE void shape3(
	GPUprim3 *prim,
	GLfloat (*radius)(GPUprim3 *prim, GLfloat z),
	calc_normals_func calc_normals,
	GLfloat zMin,
	GLfloat zMax,
	GLboolean linearV)
{
	GLfloat uArc[GPU_MAX_SEGS+1][2];
	GLfloat vArc[GPU_MAX_SEGS+1];
	GLfloat co[GPU_MAX_SEGS+1][GPU_MAX_SEGS+1][3];
	GLfloat no[GPU_MAX_SEGS+1][GPU_MAX_SEGS+1][3];
	GLfloat uFracSegs;
	int uWholeSegs;
	GLfloat vFracSegs;
	int vWholeSegs;
	GLfloat sweepAngle;
	GLfloat uFracAngle;
	GLfloat z;
	GLfloat r;
	int usegs, vsegs;
	int i, uIndex;
	int j, vIndex;
	GLboolean uCycle;
	GLushort base;
	GLboolean doNormals;


	uFracSegs = modfi(prim->usegs, &uWholeSegs);

	sweepAngle = prim->thetaMax - prim->thetaMin;

	uFracAngle = (uFracSegs / 2) * (sweepAngle / prim->usegs);

	uIndex = 0;

	if (uFracAngle != 0) {
		copy_v2_fl2(uArc[uIndex++], cosf(prim->thetaMin), sinf(prim->thetaMin));
	}

	for (i = 0; i <= uWholeSegs; i++) {
		GLfloat a = prim->thetaMin + uFracAngle + i * sweepAngle / prim->usegs;

		copy_v2_fl2(uArc[uIndex++], cosf(a), sinf(a));
	}

	if (uFracSegs == 0) {
		usegs = uIndex - 1;
	}
	else {
		usegs = uIndex;
		copy_v2_fl2(uArc[usegs], cosf(prim->thetaMax), sinf(prim->thetaMax));
	}

	uCycle =
		fabs(uArc[usegs][0] - uArc[0][0]) < 0.001f &&
		fabs(uArc[usegs][1] - uArc[0][1]) < 0.001f;

	if (uCycle) {
		usegs--;
	}

	vFracSegs = modfi(prim->vsegs, &vWholeSegs);

	if (linearV) {
		GLfloat sweepHeight;
		GLfloat vFracHeight;

		sweepHeight = zMax - zMin;

		vFracHeight = (vFracSegs / 2) * (sweepHeight / prim->vsegs);

		vIndex = 0;

		if (vFracHeight != 0) {
			vArc[vIndex++] = zMin;
		}

		for (j = 0; j <= vWholeSegs; j++) {
			vArc[vIndex++] = zMin + vFracHeight + j * sweepHeight / prim->vsegs;
		}
	}
	else {
		GLfloat angleMin;
		GLfloat angleMax;
		GLfloat vSweepAngle;
		GLfloat zMinNorm;
		GLfloat zMaxNorm;
		GLfloat vFracAngle;
		GLfloat zDiff;

		zMinNorm = zMin / prim->params.sphere.radius;
		angleMin = asinf(zMinNorm);

		zMaxNorm = zMax / prim->params.sphere.radius;
		angleMax = asinf(zMaxNorm);

		vSweepAngle = angleMax - angleMin;

		vFracAngle = (vFracSegs / 2) * (vSweepAngle / prim->vsegs);

		zDiff = (zMax - zMin) / 2;

		vIndex = 0;

		if (vFracAngle != 0) {
			vArc[vIndex++] = zMin;
		}

		for (j = 0; j <= vWholeSegs; j++) {
			GLfloat a = angleMin + vFracAngle + j * vSweepAngle / prim->vsegs;
			vArc[vIndex++] = zDiff * sinf(a);
		}
	}

	if (vFracSegs == 0) {
		vsegs = vIndex - 1;
	}
	else {
		vsegs = vIndex;
		vArc[vsegs] = zMax;
	}

	for (j = 0; j <= vsegs; j++) {
		z = vArc[j];
		r = radius(prim, z);

		for (i = 0; i <= usegs; i++) {
			copy_v3_flflfl(co[j][i], r * uArc[i][0], r * uArc[i][1], z);
		}
	}

	doNormals = calc_normals && prim->normals != GPU_NORMALS_NONE;

	if (doNormals) {
		calc_normals(no, co, usegs, vsegs);
	}

	for (j = 0; j <= vsegs; j++) {
		for (i = 0; i <= usegs; i++) {
			if (doNormals) {
				gpuNormal3fv(no[j][i]);
			}

			gpuVertex3fv(co[j][i]);
		}
	}

	gpuIndexBegin(GL_UNSIGNED_SHORT);

	switch (prim->drawStyle) {
		case GPU_DRAW_STYLE_FILL:
			for (j = 0; j < vsegs; j++) {
				base = (usegs + 1) * j;
				for (i = 0; i < usegs; i++) {
					if (prim->normals == GPU_NORMALS_FLAT || (i + j) % 2 == 0) {
						gpuIndexus(base + 1);
						gpuIndexus(base + usegs + 2);
						gpuIndexus(base);

						gpuIndexus(base + usegs + 2);
						gpuIndexus(base + usegs + 1);
						gpuIndexus(base);
					}
					else {
						gpuIndexus(base + 1);
						gpuIndexus(base + usegs + 1);
						gpuIndexus(base);

						gpuIndexus(base + usegs + 2);
						gpuIndexus(base + usegs + 1);
						gpuIndexus(base + 1);
					}

					base++;
				}
			}

			if (uCycle) {
				base = usegs;

				for (j = 0; j < vsegs; j++) {
					if (prim->normals == GPU_NORMALS_FLAT || (usegs + j) % 2 == 0) {
						gpuIndexus(base - usegs);
						gpuIndexus(base + 1);
						gpuIndexus(base);

						gpuIndexus(base + 1);
						gpuIndexus(base + usegs + 1);
						gpuIndexus(base);
					}
					else {
						gpuIndexus(base);
						gpuIndexus(base - usegs);
						gpuIndexus(base + usegs + 1);

						gpuIndexus(base + usegs + 1);
						gpuIndexus(base - usegs);
						gpuIndexus(base + 1);
					}

					base += usegs + 1;
				}
			}

			break;

		case GPU_DRAW_STYLE_SILHOUETTE:
			for (j = 0; j < vsegs; j++) {
				base = (usegs + 1) * j;
				for (i = 0; i < usegs; i++) {
					gpuIndexus(base + usegs + 1);
					gpuIndexus(base);

					gpuIndexus(base + 1);
					gpuIndexus(base);

					base++;
				}
			}

			base = (usegs + 1) * vsegs;

			for (i = 0; i < usegs; i++) {
				gpuIndexus(base + 1);
				gpuIndexus(base);

				base++;
			}

			base = usegs;

			for (j = 0; j < vsegs; j++) {
				gpuIndexus(base + usegs + 1);
				gpuIndexus(base);

				if (uCycle) {
					gpuIndexus(base - usegs);
					gpuIndexus(base);
				}

				base += usegs + 1;
			}

			if (uCycle) {
				gpuIndexus(base - usegs);
				gpuIndexus(base);
			}

			break;

		case GPU_DRAW_STYLE_LINES:
			for (j = 0; j < vsegs; j++) {
				base = (usegs + 1) * j;
				for (i = 0; i < usegs; i++) {
					gpuIndexus(base + usegs + 1);
					gpuIndexus(base);

					gpuIndexus(base + 1);
					gpuIndexus(base);

					gpuIndexus(base + usegs + 2);
					gpuIndexus(base);

					base++;
				}
			}

			base = (usegs+1) * vsegs;

			for (i = 0; i < usegs; i++) {
				gpuIndexus(base+1);
				gpuIndexus(base);

				base++;
			}

			base = usegs;

			for (j = 0; j < vsegs; j++) {
				gpuIndexus(base + usegs + 1);
				gpuIndexus(base);

				if (uCycle) {
					gpuIndexus(base - usegs);
					gpuIndexus(base);

					gpuIndexus(base + 1);
					gpuIndexus(base);
				}

				base += usegs + 1;
			}

			if (uCycle) {
				gpuIndexus(base - usegs);
				gpuIndexus(base);
			}

			break;

		case GPU_DRAW_STYLE_POINTS:
			/* making an index would be wasteful */
			break;

		default:
			break;
	}

	gpuIndexEnd();
}



void gpuAppendCone(
	GPUprim3 *prim,
	GLfloat radiusBase,
	GLfloat height)
{
	copy_v3_flflfl(prim->params.sweep.point1, radiusBase, 0, 0);
	copy_v3_flflfl(prim->params.sweep.point2, 0, 0, height);

	shape3(prim, sweep, NULL, 0, height, GL_TRUE);
}



void gpuDrawCone(
	GPUprim3 *prim,
	GLfloat radiusBase,
	GLfloat height)
{
	gpuBegin(GL_NOOP);
	gpuAppendCone(prim, radiusBase, height);
	gpuEnd();

	primDraw(prim);
}



void gpuSingleCone(
	GPUprim3 *prim,
	GLfloat radiusBase,
	GLfloat height)
{
	primFormat(prim);
	gpuDrawCone(prim, radiusBase, height);
	gpuImmediateUnformat();
}



void gpuAppendCylinder(
	GPUprim3 *prim,
	GLfloat radiusBase,
	GLfloat radiusTop,
	GLfloat height)
{
	copy_v3_flflfl(prim->params.sweep.point1, radiusBase, 0, 0);
	copy_v3_flflfl(prim->params.sweep.point2, radiusTop, 0, height);

	shape3(prim, sweep, NULL, 0, height, GL_TRUE);
}



void gpuDrawCylinder(
	GPUprim3 *prim,
	GLfloat radiusBase,
	GLfloat radiusTop,
	GLfloat height)
{
	gpuBegin(GL_NOOP);
	gpuAppendCylinder(prim, radiusBase, radiusTop, height);
	gpuEnd();

	primDraw(prim);
}



void gpuSingleCylinder(
	GPUprim3 *prim,
	GLfloat radiusBase,
	GLfloat radiusTop,
	GLfloat height)
{
	primFormat(prim);
	gpuDrawCylinder(prim, radiusBase, radiusTop, height);
	gpuImmediateUnformat();
}



void gpuAppendSphere(
	GPUprim3 *prim,
	GLfloat radius)
{
	prim->params.sphere.radius =  radius;
	prim->params.sphere.zMin   = -radius;
	prim->params.sphere.zMax   =  radius;

	shape3(
		prim,
		sphere,
		sphereNormals,
		prim->params.sphere.zMin,
		prim->params.sphere.zMax,
		GL_FALSE);
}



void gpuDrawSphere(
	GPUprim3 *prim,
	GLfloat radius)
{
	gpuBegin(GL_NOOP);
	gpuAppendSphere(prim, radius);
	gpuEnd();

	primDraw(prim);
}



void gpuSingleSphere(
	GPUprim3 *prim,
	GLfloat radius)
{
	primFormat(prim);
	gpuDrawSphere(prim, radius);
	gpuImmediateUnformat();
}



const GPUprim3 GPU_PRIM_LOFI_SOLID = {
	GPU_LOD_LO,          /* GLfloat usegs;     */
	GPU_LOD_LO / 2.0f,   /* GLfloat vsegs;     */
	GPU_NORMALS_SMOOTH,  /* GLenum  normals;   */
	GPU_DRAW_STYLE_FILL, /* GLenum  drawStyle; */
	GL_FALSE,            /* GLboolean flipNormals */
	GL_FALSE,            /* GLboolean texCoords */
	0,                   /* GLfloat thetaMin   */
	(float)(2.0 * M_PI), /* GLfloat thetaMax   */
};

const GPUprim3 GPU_PRIM_LOFI_SHADELESS = {
	GPU_LOD_LO,          /* GLfloat usegs;     */
	GPU_LOD_LO / 2.0f,   /* GLfloat vsegs;     */
	GPU_NORMALS_NONE,    /* GLenum  normals;   */
	GPU_DRAW_STYLE_FILL, /* GLenum  drawStyle; */
	GL_FALSE,            /* GLboolean flipNormals */
	GL_FALSE,            /* GLboolean texCoords */
	0,                   /* GLfloat thetaMin   */
	(float)(2.0 * M_PI), /* GLfloat thetaMax   */
};

const GPUprim3 GPU_PRIM_LOFI_WIRE = {
	GPU_LOD_LO,                /* GLfloat usegs;     */
	GPU_LOD_LO / 2.0f,         /* GLfloat vsegs;     */
	GPU_NORMALS_NONE,          /* GLenum  normals;   */
	GPU_DRAW_STYLE_SILHOUETTE, /* GLenum  drawStyle; */
	GL_FALSE,                  /* GLboolean flipNormals */
	GL_FALSE,                  /* GLboolean texCoords */
	0,                         /* GLfloat thetaMin   */
	(float)(2.0 * M_PI)        /* GLfloat thetaMax   */
};

const GPUprim3 GPU_PRIM_MIDFI_SOLID = {
	GPU_LOD_MID,          /* GLfloat usegs;     */
	GPU_LOD_MID / 2.0f,   /* GLfloat vsegs;     */
	GPU_NORMALS_SMOOTH,   /* GLenum  normals;   */
	GPU_DRAW_STYLE_FILL,  /* GLenum  drawStyle; */
	GL_FALSE,             /* GLboolean flipNormals */
	GL_FALSE,             /* GLboolean texCoords */
	0,                    /* GLfloat thetaMin   */
	(float)(2.0 * M_PI),  /* GLfloat thetaMax   */
};

const GPUprim3 GPU_PRIM_MIDFI_WIRE = {
	GPU_LOD_MID,               /* GLfloat usegs;     */
	GPU_LOD_MID / 2.0f,        /* GLfloat vsegs;     */
	GPU_NORMALS_NONE,          /* GLenum  normals;   */
	GPU_DRAW_STYLE_SILHOUETTE, /* GLenum  drawStyle; */
	GL_FALSE,                  /* GLboolean flipNormals */
	GL_FALSE,                  /* GLboolean texCoords */
	0,                         /* GLfloat thetaMin   */
	(float)(2.0 * M_PI),       /* GLfloat thetaMax   */
};

const GPUprim3 GPU_PRIM_HIFI_SOLID = {
	GPU_LOD_HI,          /* GLfloat usegs;     */
	GPU_LOD_HI / 2.0f,   /* GLfloat vsegs;     */
	GPU_NORMALS_SMOOTH,  /* GLenum  normals;   */
	GPU_DRAW_STYLE_FILL, /* GLenum  drawStyle; */
	GL_FALSE,            /* GLboolean flipNormals */
	GL_FALSE,            /* GLboolean texCoords */
	0,                   /* GLfloat thetaMin   */
	(float)(2.0 * M_PI), /* GLfloat thetaMax   */
};


static const float cube[8][3] = {
	{-1.0, -1.0, -1.0},
	{-1.0, -1.0,  1.0},
	{-1.0,  1.0,  1.0},
	{-1.0,  1.0, -1.0},
	{ 1.0, -1.0, -1.0},
	{ 1.0, -1.0,  1.0},
	{ 1.0,  1.0,  1.0},
	{ 1.0,  1.0, -1.0},
};

void gpuSingleWireUnitCube(void)
{
	gpuImmediateFormat_V3();

	gpuBegin(GL_LINE_STRIP);
	gpuVertex3fv(cube[0]); gpuVertex3fv(cube[1]); gpuVertex3fv(cube[2]); gpuVertex3fv(cube[3]);
	gpuVertex3fv(cube[0]); gpuVertex3fv(cube[4]); gpuVertex3fv(cube[5]); gpuVertex3fv(cube[6]);
	gpuVertex3fv(cube[7]); gpuVertex3fv(cube[4]);
	gpuEnd();

	gpuBegin(GL_LINES);
	gpuVertex3fv(cube[1]); gpuVertex3fv(cube[5]);
	gpuVertex3fv(cube[2]); gpuVertex3fv(cube[6]);
	gpuVertex3fv(cube[3]); gpuVertex3fv(cube[7]);
	gpuEnd();

	gpuImmediateUnformat();
}

/* draws a cube given the scaling of the cube, assuming that
 * all required matrices have been set (used for drawing empties) */
void gpuSingleWireCube(GLfloat size)
{
	gpuImmediateFormat_V3();

	gpuBegin(GL_LINE_STRIP);
	gpuVertex3f(-size, -size, -size); gpuVertex3f(-size, -size, size);
	gpuVertex3f(-size, size, size); gpuVertex3f(-size, size, -size);

	gpuVertex3f(-size, -size, -size); gpuVertex3f(size, -size, -size);
	gpuVertex3f(size, -size, size); gpuVertex3f(size, size, size);

	gpuVertex3f(size, size, -size); gpuVertex3f(size, -size, -size);
	gpuEnd();

	gpuBegin(GL_LINES);
	gpuVertex3f(-size, -size, size); gpuVertex3f(size, -size, size);
	gpuVertex3f(-size, size, size); gpuVertex3f(size, size, size);
	gpuVertex3f(-size, size, -size); gpuVertex3f(size, size, -size);
	gpuEnd();

	gpuImmediateUnformat();
}

/* half the cube, in Y */
static const float half_cube[8][3] = {
	{-1.0,  0.0, -1.0},
	{-1.0,  0.0,  1.0},
	{-1.0,  1.0,  1.0},
	{-1.0,  1.0, -1.0},
	{ 1.0,  0.0, -1.0},
	{ 1.0,  0.0,  1.0},
	{ 1.0,  1.0,  1.0},
	{ 1.0,  1.0, -1.0},
};

void gpuDrawSolidHalfCube(void)
{
	float n[3] = {0,0,0};

	gpuBegin(GL_QUADS);
	n[0] = -1.0;
	gpuNormal3fv(n);
	gpuVertex3fv(half_cube[0]); gpuVertex3fv(half_cube[1]); gpuVertex3fv(half_cube[2]); gpuVertex3fv(half_cube[3]);
	n[0] = 0;
	n[1] = -1.0;
	gpuNormal3fv(n);
	gpuVertex3fv(half_cube[0]); gpuVertex3fv(half_cube[4]); gpuVertex3fv(half_cube[5]); gpuVertex3fv(half_cube[1]);
	n[1] = 0;
	n[0] = 1.0;
	gpuNormal3fv(n);
	gpuVertex3fv(half_cube[4]); gpuVertex3fv(half_cube[7]); gpuVertex3fv(half_cube[6]); gpuVertex3fv(half_cube[5]);
	n[0] = 0;
	n[1] = 1.0;
	gpuNormal3fv(n);
	gpuVertex3fv(half_cube[7]); gpuVertex3fv(half_cube[3]); gpuVertex3fv(half_cube[2]); gpuVertex3fv(half_cube[6]);
	n[1] = 0;
	n[2] = 1.0;
	gpuNormal3fv(n);
	gpuVertex3fv(half_cube[1]); gpuVertex3fv(half_cube[5]); gpuVertex3fv(half_cube[6]); gpuVertex3fv(half_cube[2]);
	n[2] = -1.0;
	gpuNormal3fv(n);
	gpuVertex3fv(half_cube[7]); gpuVertex3fv(half_cube[4]); gpuVertex3fv(half_cube[0]); gpuVertex3fv(half_cube[3]);
	gpuEnd();
}

void gpuDrawWireHalfCube(void)
{
	gpuBegin(GL_LINE_STRIP);
	gpuVertex3fv(half_cube[0]); gpuVertex3fv(half_cube[1]); gpuVertex3fv(half_cube[2]); gpuVertex3fv(half_cube[3]);
	gpuVertex3fv(half_cube[0]); gpuVertex3fv(half_cube[4]); gpuVertex3fv(half_cube[5]); gpuVertex3fv(half_cube[6]);
	gpuVertex3fv(half_cube[7]); gpuVertex3fv(half_cube[4]);
	gpuEnd();

	gpuBegin(GL_LINES);
	gpuVertex3fv(half_cube[1]); gpuVertex3fv(half_cube[5]);
	gpuVertex3fv(half_cube[2]); gpuVertex3fv(half_cube[6]);
	gpuVertex3fv(half_cube[3]); gpuVertex3fv(half_cube[7]);
	gpuEnd();
}

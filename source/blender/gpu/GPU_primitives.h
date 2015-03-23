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



#include "GPU_glew.h"



#ifdef __cplusplus
extern "C" {
#endif


#define GPU_NORMALS_SMOOTH 0
#define GPU_NORMALS_FLAT   1
#define GPU_NORMALS_NONE   2

#define GPU_DRAW_STYLE_FILL       0
#define GPU_DRAW_STYLE_LINES      1
#define GPU_DRAW_STYLE_SILHOUETTE 2
#define GPU_DRAW_STYLE_POINTS     3

#define GPU_MAX_SEGS 128

typedef struct GPUprim3 {
	GLfloat usegs;
	GLfloat vsegs;

	GLenum normals;
	GLenum drawStyle;
	GLboolean flipNormals;
	GLboolean texCoords;

	GLfloat thetaMin;
	GLfloat thetaMax;

	union {
		struct {
			GLfloat point1[3];
			GLfloat point2[3];
		} sweep; /* cone, cylinder, disk , hyperboloid */

		struct {
			GLfloat radius;
			GLfloat zMin;
			GLfloat zMax;
		} sphere;

		struct {
			GLfloat rMax;
			GLfloat zMin;
			GLfloat zMax;
		} paraboloid;

		struct {
			GLfloat majorRadius;
			GLfloat minorRadius;
			GLfloat phiMin;
			GLfloat phiMax;
		} torus;
	} params;
} GPUprim3;

#define GPU_LOD_LO   8
#define GPU_LOD_MID 16
#define GPU_LOD_HI  32

extern const GPUprim3 GPU_PRIM_LOFI_SOLID;
extern const GPUprim3 GPU_PRIM_LOFI_SHADELESS;
extern const GPUprim3 GPU_PRIM_LOFI_WIRE;

extern const GPUprim3 GPU_PRIM_MIDFI_SOLID;
extern const GPUprim3 GPU_PRIM_MIDFI_WIRE;

extern const GPUprim3 GPU_PRIM_HIFI_SOLID;

void gpuAppendCone(GPUprim3 *prim3, GLfloat radius, GLfloat height);
void gpuAppendCylinder(GPUprim3 *prim3, GLfloat radiusBase, GLfloat radiusTop, GLfloat height);
void gpuAppendSphere(GPUprim3 *prim3, GLfloat radius);

void gpuDrawCone(GPUprim3 *prim3, GLfloat radius, GLfloat height);
void gpuDrawCylinder(GPUprim3 *prim3, GLfloat radiusBase, GLfloat radiusTop, GLfloat height);
void gpuDrawSphere(GPUprim3 *prim3, GLfloat radius);

void gpuSingleCone(GPUprim3 *prim3, GLfloat radius, GLfloat height);
void gpuSingleCylinder(GPUprim3 *prim3, GLfloat radiusBase, GLfloat radiusTop, GLfloat height);
void gpuSingleSphere(GPUprim3 *prim3, GLfloat radius);



void gpuSingleLinef(GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2);
void gpuSingleLinei(GLint x1, GLint y1, GLint x2, GLint y2);

void gpuSingleFilledRectf(GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2);
void gpuSingleFilledRecti(GLint x1, GLint y1, GLint x2, GLint y2);

void gpuSingleWireRectf(GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2);
void gpuSingleWireRecti(GLint x1, GLint y1, GLint x2, GLint y2);


void gpuAppendArc(
	GLfloat x,
	GLfloat y,
	GLfloat start,
	GLfloat angle,
	GLfloat xradius,
	GLfloat yradius,
	GLint nsegments);

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
	GLfloat matrix[4][4],
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



void gpuSingleWireUnitCube(void);
void gpuSingleWireCube(GLfloat size);

void gpuDrawSolidHalfCube(void);
void gpuDrawWireHalfCube(void);



#ifdef __cplusplus
}
#endif



#include "intern/gpu_primitives_inline.h"



#endif /* __GPU_PRIMITIVES_H_ */

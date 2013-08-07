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
 * Contributor(s): Alexandr Kuznetsov
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "gpu_view_gl.h"
#include "gpu_glew.h"



void gpuColorAndClear_gl(float r, float g, float b, float a)
{
	gpuClearColor_gl(r, g, b, a);
	glClear(GL_COLOR_BUFFER_BIT);
}



void gpuClearColor_gl(float r, float g, float b, float a)
{
	glClearColor(r, g, b, a);
	//glClearColor((float)rand()/RAND_MAX,(float)rand()/RAND_MAX,(float)rand()/RAND_MAX,0);
}



void gpuColorAndClearvf_gl(float c[3], float a)
{
	gpuClearColorvf_gl(c, a);
	glClear(GL_COLOR_BUFFER_BIT);
}



void gpuClearColorvf_gl(float c[3], float a)
{
	glClearColor(c[0], c[1], c[2], a);
	//glClearColor((float)rand()/RAND_MAX,(float)rand()/RAND_MAX,(float)rand()/RAND_MAX,0);
}


static GLint viewport[4];

void gpuViewport_gl(int x, int y, unsigned int width, unsigned int height)
{
	viewport[0] = x;
	viewport[1] = y;
	viewport[2] = width;
	viewport[3] = height;

	glViewport(x, y, width, height);
}



void gpuFeedbackViewport2fv(GLfloat x, GLfloat y, GLfloat out[2])
{
	const GLfloat halfw = (GLfloat)viewport[2] / 2.0f;
	const GLfloat halfh = (GLfloat)viewport[3] / 2.0f;

	out[0] = halfw*x + halfw + (GLfloat)viewport[0];
	out[1] = halfh*y + halfh + (GLfloat)viewport[1];
}


void gpuScissor_gl(int x, int y, unsigned int width, unsigned int height)
{
	glEnable(GL_SCISSOR_TEST);
	glScissor(x, y, width, height);
	//gpuColorAndClear_gl((float)rand()/RAND_MAX,(float)rand()/RAND_MAX,(float)rand()/RAND_MAX,0);
	//gpuClearColor_gl((float)rand()/RAND_MAX,(float)rand()/RAND_MAX,(float)rand()/RAND_MAX,0);
}



void gpuGetSizeBox_gl(int type, int *box)
{
	glGetIntegerv(type, box);
}



void gpuViewportScissor_gl(int x, int y, unsigned int width, unsigned int height)
{
	gpuViewport_gl(x, y, width, height);
	gpuScissor_gl(x, y, width, height);
}



void gpuClear_gl(int mask)
{
	glClear(mask);
}

/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the ipmlied warranty of
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

/** \file source/blender/gpu/intern/gpu_matrix.c
 *  \ingroup gpu
 */

#if WITH_GL_PROFILE_COMPAT
#define GPU_MANGLE_DEPRECATED 0 /* Allow use of deprecated OpenGL functions in this file */
#endif

#include "BLI_sys_types.h"

#include "GPU_common.h"
#include "GPU_debug.h"
#include "GPU_extensions.h"
#include "GPU_matrix.h"

/* internal */
#include "intern/gpu_private.h"

/* external */

#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"

#include "MEM_guardedalloc.h"



typedef GLfloat GPU_matrix[4][4];

typedef struct GPU_matrix_stack
{
	GLsizei     size;
	GLsizei     pos;
	GPU_matrix* dynstack;
} GPU_matrix_stack;


static GPU_matrix_stack ms_modelview;
static GPU_matrix_stack ms_projection;
static GPU_matrix_stack ms_texture;

static GPU_matrix_stack* ms_current;
static GLenum ms_current_mode;

#define current_matrix() (ms_current->dynstack[ms_current->pos])


/* Check if we have a good matrix */
#ifdef WITH_GPU_SAFETY

static void checkmat(GLfloat *m)
{
	GLint i;

	for(i = 0; i < 16; i++) {
#if _MSC_VER
		GPU_ASSERT(_finite(m[i]));
#else
		GPU_ASSERT(!isinf(m[i]));
#endif
	}
}

#define CHECKMAT(m) checkmat((GLfloat*)m)

#else

#define CHECKMAT(m)

#endif

static void ms_init(GPU_matrix_stack* ms, GLint initsize)
{
	BLI_assert(initsize > 0);

	ms->size     = initsize;
	ms->pos      = 0;
	ms->dynstack = (GPU_matrix*)MEM_mallocN(ms->size*sizeof(*(ms->dynstack)), "MatrixStack");
}

static void ms_free(GPU_matrix_stack * ms)
{
	ms->size     = 0;
	ms->pos      = 0;
	MEM_freeN(ms->dynstack);
	ms->dynstack = NULL;
}

void gpu_matrix_init(void)
{
	ms_init(&ms_texture,    16);
	ms_init(&ms_projection, 16);
	ms_init(&ms_modelview,  32);

	gpuMatrixMode(GL_TEXTURE);
	gpuLoadIdentity();

	gpuMatrixMode(GL_PROJECTION);
	gpuLoadIdentity();

	gpuMatrixMode(GL_MODELVIEW);
	gpuLoadIdentity();
}

void gpu_matrix_exit(void)
{
	ms_free(&ms_modelview);
	ms_free(&ms_projection);
	ms_free(&ms_texture);

	ms_current_mode = 0;
	ms_current      = NULL;
}

void gpu_commit_matrix(void)
{
	const struct GPUcommon* common = gpu_get_common();

	GPU_ASSERT_NO_GL_ERRORS("gpu_commit_matrix start");

	if (common) {
		int i;

		float (*m)[4] = (float (*)[4])gpuGetMatrix(GL_MODELVIEW_MATRIX,  NULL);
		float (*p)[4] = (float (*)[4])gpuGetMatrix(GL_PROJECTION_MATRIX, NULL);

		if (common->modelview_matrix != -1)
			glUniformMatrix4fv(common->modelview_matrix, 1, GL_FALSE, m[0]);

		if (common->normal_matrix != -1) {
			float n[3][3];
			copy_m3_m4(n, m);
			invert_m3(n);
			transpose_m3(n);
			glUniformMatrix3fv(common->normal_matrix, 1, GL_FALSE, n[0]);
		}

		if (common->modelview_matrix_inverse != -1) {
			float i[4][4];
			invert_m4_m4(i, m);
			glUniformMatrix4fv(common->modelview_matrix_inverse, 1, GL_FALSE, i[0]);
		}

		if (common->modelview_projection_matrix != -1) {
			float pm[4][4];
			mul_m4_m4m4(pm, p, m);
			glUniformMatrix4fv(common->modelview_projection_matrix, 1, GL_FALSE, pm[0]);
		}

		if (common->projection_matrix != -1)
			glUniformMatrix4fv(common->projection_matrix, 1, GL_FALSE, p[0]);

		for (i = 0; i < GPU_MAX_COMMON_TEXCOORDS; i++) {
			if (common->texture_matrix[i] != -1) {
				GPU_set_common_active_texture(i);
				glUniformMatrix4fv(common->texture_matrix[i], 1, GL_FALSE, gpuGetMatrix(GL_TEXTURE_MATRIX, NULL));
			}
		}

		GPU_set_common_active_texture(0);

		GPU_ASSERT_NO_GL_ERRORS("gpu_commit_matrix end");

		return;
	}

#if defined(WITH_GL_PROFILE_COMPAT)
	glMatrixMode(GL_TEXTURE);
	glLoadMatrixf(gpuGetMatrix(GL_TEXTURE_MATRIX, NULL));

	glMatrixMode(GL_PROJECTION);
	glLoadMatrixf(gpuGetMatrix(GL_PROJECTION_MATRIX, NULL));

	glMatrixMode(GL_MODELVIEW);
	glLoadMatrixf(gpuGetMatrix(GL_MODELVIEW_MATRIX, NULL));
#endif

	GPU_ASSERT_NO_GL_ERRORS("gpu_commit_matrix end");
}

void gpuPushMatrix(void)
{
	GLsizei new_pos = ms_current->pos + 1;

	BLI_assert(new_pos < ms_current->size);

	if (new_pos < ms_current->size) {
		ms_current->pos++;

		gpuLoadMatrix((GLfloat*)ms_current->dynstack[ms_current->pos-1]);
	}
}

void gpuPopMatrix(void)
{
	BLI_assert(ms_current->pos != 0);

	if (ms_current->pos != 0) {
		ms_current->pos--;

		CHECKMAT(ms_current);
	}
}

void gpuMatrixMode(GLenum mode)
{
	BLI_assert(ELEM(mode, GL_MODELVIEW, GL_PROJECTION, GL_TEXTURE));

	ms_current_mode = mode;

	switch(mode) {
		case GL_MODELVIEW:
			ms_current = &ms_modelview;
			break;
		case GL_PROJECTION:
			ms_current = &ms_projection;
			break;
		case GL_TEXTURE:
			ms_current = &ms_texture;
			break;
		default:
			/* ignore */
			break;
	}
}

GLenum gpuGetMatrixMode(void)
{
	return ms_current_mode;
}

void gpuLoadMatrix(const GLfloat* m)
{
	copy_m4_m4(current_matrix(), (GLfloat (*)[4])m);

	CHECKMAT(ms_current);
}

const GLfloat* gpuGetMatrix(GLenum type, GLfloat *m)
{
	GPU_matrix_stack* ms_select;

	BLI_assert(ELEM(type, GL_MODELVIEW_MATRIX, GL_PROJECTION_MATRIX, GL_TEXTURE_MATRIX));

	switch(type) {
		case GL_MODELVIEW_MATRIX:
			ms_select = &ms_modelview;
			break;
		case GL_PROJECTION_MATRIX:
			ms_select = &ms_projection;
			break;
		case GL_TEXTURE_MATRIX:
			ms_select = &ms_texture;
			break;
		default:
			return NULL;
	}

	if (m) {
		copy_m4_m4((GLfloat (*)[4])m, ms_select->dynstack[ms_select->pos]);
		return m;
	}
	else {
		return (GLfloat*)(ms_select->dynstack[ms_select->pos]);
	}
}

void gpuLoadIdentity(void)
{
	unit_m4(current_matrix());

	CHECKMAT(ms_current);
}

void gpuTranslate(GLfloat x, GLfloat y, GLfloat z)
{
	translate_m4(current_matrix(), x, y, z);

	CHECKMAT(ms_current);
}

void gpuScale(GLfloat x, GLfloat y, GLfloat z)
{
	scale_m4(current_matrix(), x, y, z);

	CHECKMAT(ms_current);
}

void gpuMultMatrix(const GLfloat *m)
{
	GPU_matrix cm;

	copy_m4_m4(cm, current_matrix());
	mult_m4_m4m4_q(current_matrix(), cm, (GLfloat (*)[4])m);

	CHECKMAT(ms_current);
}

void gpuMultMatrixd(const double *m)
{
	GLfloat mf[16];
	GLint i;

	for(i = 0; i < 16; i++) {
		mf[i] = m[i];
	}

	gpuMultMatrix(mf);
}

void gpuRotateVector(GLfloat deg, GLfloat vector[3])
{
	float rm[3][3];
	GPU_matrix cm;

	axis_angle_to_mat3(rm, vector, DEG2RADF(deg));

	copy_m4_m4(cm, current_matrix());
	mult_m4_m3m4_q(current_matrix(), cm, rm);

	CHECKMAT(ms_current);
}

void gpuRotateAxis(GLfloat deg, char axis)
{
	rotate_m4(current_matrix(), axis, DEG2RADF(deg));

	CHECKMAT(ms_current);
}

void gpuRotateRight(char type)
{
	rotate_m4_right(current_matrix(), type);

	CHECKMAT(ms_current);
}

void gpuLoadOrtho(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat nearVal, GLfloat farVal)
{
	mat4_ortho_set(current_matrix(), left, right, bottom, top, nearVal, farVal);

	CHECKMAT(ms_current);
}

void gpuOrtho(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat nearVal, GLfloat farVal)
{
	GPU_matrix om;

	mat4_ortho_set(om, left, right, bottom, top, nearVal, farVal);

	gpuMultMatrix((GLfloat*)om);
}

void gpuFrustum(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat nearVal, GLfloat farVal)
{
	GPU_matrix fm;

	mat4_frustum_set(fm, left, right, bottom, top, nearVal, farVal);

	gpuMultMatrix((GLfloat*) fm);
}

void gpuLoadFrustum(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat nearVal, GLfloat farVal)
{
	mat4_frustum_set(current_matrix(), left, right, bottom, top, nearVal, farVal);

	CHECKMAT(ms_current);
}

void gpuLookAt(GLfloat eyeX, GLfloat eyeY, GLfloat eyeZ, GLfloat centerX, GLfloat centerY, GLfloat centerZ, GLfloat upX, GLfloat upY, GLfloat upZ)
{
	GPU_matrix cm;
	GLfloat lookdir[3];
	GLfloat camup[3] = {upX, upY, upZ};

	lookdir[0] =  centerX - eyeX;
	lookdir[1] =  centerY - eyeY;
	lookdir[2] =  centerZ - eyeZ;

	mat4_look_from_origin(cm, lookdir, camup);

	gpuMultMatrix((GLfloat*) cm);
	gpuTranslate(-eyeX, -eyeY, -eyeZ);
}

void gpuProject(const GLfloat obj[3], const GLfloat model[16], const GLfloat proj[16], const GLint view[4], GLfloat win[3])
{
	float v[4];

	mul_v4_m4v3(v, (float(*)[4])model, obj);
	mul_m4_v4((float(*)[4])proj, v);

	win[0]=view[0]+(view[2]*(v[0]+1))*0.5f;
	win[1]=view[1]+(view[3]*(v[1]+1))*0.5f;
	win[2]=(v[2]+1)*0.5f;
}

GLboolean gpuUnProject(const GLfloat win[3], const GLfloat model[16], const GLfloat proj[16], const GLint view[4], GLfloat obj[3])
{
	GLfloat pm[4][4];
	GLfloat in[4];
	GLfloat out[4];

	mul_m4_m4m4(pm, (float(*)[4])proj, (float(*)[4])model);

	if (!invert_m4(pm)) {
		return GL_FALSE;
	}

	in[0]=win[0];
	in[1]=win[1];
	in[2]=win[2];
	in[3]=1;

	/* Map x and y from window coordinates */
	in[0] = (in[0] - view[0]) / view[2];
	in[1] = (in[1] - view[1]) / view[3];

	/* Map to range -1 to 1 */
	in[0] = 2 * in[0] - 1;
	in[1] = 2 * in[1] - 1;
	in[2] = 2 * in[2] - 1;

	mul_v4_m4v3(out, pm, in);

	if (out[3] == 0.0) {
		return GL_FALSE;
	}
	else {
		out[0] /= out[3];
		out[1] /= out[3];
		out[2] /= out[3];

		obj[0] = out[0];
		obj[1] = out[1];
		obj[2] = out[2];

		return GL_TRUE;
	}
}

void GPU_feedback_vertex_3fv(GLenum type, GLfloat x, GLfloat y, GLfloat z, GLfloat out[3])
{
	GPU_matrix* m = (GPU_matrix*)gpuGetMatrix(type, NULL);
	float in[3] = {x, y, z};
	mul_v3_m4v3(out, m[0], in);
}

void GPU_feedback_vertex_4fv(GLenum type, GLfloat x, GLfloat y, GLfloat z, GLfloat w, GLfloat out[3])
{
	GPU_matrix* m = (GPU_matrix*)gpuGetMatrix(type, NULL);
	float in[4] = {x, y, z, w};
	mul_v4_m4v4(out, m[0], in);
}

void GPU_feedback_vertex_4dv(GLenum type, GLdouble x, GLdouble y, GLdouble z, GLdouble w, GLdouble out[3])
{
	GPU_matrix* m = (GPU_matrix*)gpuGetMatrix(type, NULL);
	double in[4] = {x, y, z, w};
	mul_v4d_m4v4d(out, m[0], in);
}
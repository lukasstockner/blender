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
 * Contributor(s): Jason Wilkins, Alexandr Kuznetsov
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file GPU_copmatibility.h
 *  \ingroup gpu
 */

#include "MEM_guardedalloc.h"

#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"

#include "intern/gpu_safety.h"
#include "intern/gpu_glew.h"

#define GPU_MAT_CAST_ANY 0
#include "GPU_matrix.h"

#include "gpu_object_gles.h"
#include "GPU_extensions.h"



typedef GLfloat GPU_matrix[4][4];

typedef struct GPU_matrix_stack
{
	GLsizei     size;
	GLsizei     pos;
	GLboolean   changed;
	GPU_matrix* dynstack;
} GPU_matrix_stack;



static GLboolean glslneedupdate = GL_TRUE;

static GPU_matrix_stack ms_modelview;
static GPU_matrix_stack ms_projection;
static GPU_matrix_stack ms_texture;

static GPU_matrix_stack* ms_current;
static GLenum ms_current_mode;

static GLint glstackpos[3];
static GLint glstackmode;




void GPU_matrix_forced_update(void)
{
	glslneedupdate = GL_TRUE;
	gpuMatrixCommit();
	glslneedupdate = GL_TRUE;
}



#define current_matrix() (ms_current->dynstack[ms_current->pos])



/* Check if we have a good matrix */
#if WITH_GPU_SAFETY

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
	ms->changed  = GL_TRUE;
	ms->dynstack = (GPU_matrix*)MEM_mallocN(ms->size*sizeof(*(ms->dynstack)), "MatrixStack");
}



static void ms_free(GPU_matrix_stack * ms)
{
	ms->size     = 0;
	ms->pos      = 0;
	ms->changed  = GL_FALSE;
	MEM_freeN(ms->dynstack);
	ms->dynstack = NULL;
}



void GPU_ms_init(void)
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

void GPU_ms_exit(void)
{
	ms_free(&ms_modelview);
	ms_free(&ms_projection);
	ms_free(&ms_texture);

	ms_current_mode = 0;
	ms_current      = NULL;
}



void gpuMatrixCommit(void)
{
	GPU_CHECK_NO_ERROR();

#if defined(WITH_GL_PROFILE_COMPAT)
	if(ms_modelview.changed) {
		ms_modelview.changed = GL_FALSE;
		glMatrixMode(GL_MODELVIEW);
		glLoadMatrixf((GLfloat*)ms_modelview.dynstack[ms_modelview.pos]);
	}

	if(ms_projection.changed) {
		ms_projection.changed = GL_FALSE;
		glMatrixMode(GL_PROJECTION);
		glLoadMatrixf((GLfloat*)ms_projection.dynstack[ms_projection.pos]);
	}

	if(ms_texture.changed) {
		ms_texture.changed = GL_FALSE;
		glMatrixMode(GL_TEXTURE);
		glLoadMatrixf((GLfloat*)ms_texture.dynstack[ms_texture.pos]);
	}
#endif

#if defined(WITH_GL_PROFILE_CORE) || defined(WITH_GL_PROFILE_ES20)
	if (curglslesi) {
		if(ms_modelview.changed || glslneedupdate) {
			if(curglslesi->viewmatloc!=-1) {
				glUniformMatrix4fv(curglslesi->viewmatloc, 1, 0, ms_modelview.dynstack[ms_modelview.pos][0]);
			}

			if(curglslesi->normalmatloc!=-1) {
				GLfloat t[3][3] = {1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0};
				copy_m3_m4(t, ms_modelview.dynstack[ms_modelview.pos]);
				glUniformMatrix3fv(curglslesi->normalmatloc, 1, 0, t[0]);
			}
		}

		if(ms_projection.changed || glslneedupdate) {
			if(curglslesi->projectionmatloc!=-1) {
				glUniformMatrix4fv(curglslesi->projectionmatloc, 1, 0, ms_projection.dynstack[ms_projection.pos][0]);
			}
		}
	
		if(ms_texture.changed|| glslneedupdate) {
			if(curglslesi->texturematloc != -1) {
				glUniformMatrix4fv(curglslesi->texturematloc, 1, 0, ms_texture.dynstack[ms_texture.pos][0]);
			}
		}
	}
#endif

	GPU_CHECK_NO_ERROR();
}



void gpuPushMatrix(void)
{
	ms_current->pos++;

	GPU_ASSERT(ms_current->pos < ms_current->size);

	gpuLoadMatrix((GLfloat*)ms_current->dynstack[ms_current->pos-1]);
}



void gpuPopMatrix(void)
{
	GPU_CHECK_NO_ERROR();
	GPU_ASSERT(ms_current->pos != 0);

	ms_current->pos--;

	ms_current->changed = GL_TRUE;

	CHECKMAT(ms_current);
	GPU_CHECK_NO_ERROR();
}



void gpuMatrixMode(GLenum mode)
{
	GPU_CHECK_NO_ERROR();
	GPU_ASSERT(ELEM3(mode, GL_MODELVIEW, GL_PROJECTION, GL_TEXTURE));

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
	GPU_CHECK_NO_ERROR();

	copy_m4_m4(current_matrix(), (GLfloat (*)[4])m);

	ms_current->changed = GL_TRUE;

	CHECKMAT(ms_current);
}



const GLfloat* gpuGetMatrix(GLenum type, GLfloat *m)
{
	GPU_matrix_stack* ms_select;

	GPU_CHECK_NO_ERROR();
	GPU_ASSERT(ELEM3(type, GL_MODELVIEW_MATRIX, GL_PROJECTION_MATRIX, GL_TEXTURE_MATRIX));

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
	GPU_CHECK_NO_ERROR();

	unit_m4(current_matrix());

	ms_current->changed = GL_TRUE;

	CHECKMAT(ms_current);
}



void gpuTranslate(GLfloat x, GLfloat y, GLfloat z)
{
	GPU_CHECK_NO_ERROR();

	translate_m4(current_matrix(), x, y, z);

	ms_current->changed = GL_TRUE;

	CHECKMAT(ms_current);
}



void gpuScale(GLfloat x, GLfloat y, GLfloat z)
{
	GPU_CHECK_NO_ERROR();

	scale_m4(current_matrix(), x, y, z);

	ms_current->changed = GL_TRUE;

	CHECKMAT(ms_current);
}



void gpuMultMatrix(const GLfloat *m)
{
	GPU_matrix cm;

	GPU_CHECK_NO_ERROR();

	copy_m4_m4(cm, current_matrix());
	mult_m4_m4m4_q(current_matrix(), cm, (GLfloat (*)[4])m);

	ms_current->changed = GL_TRUE;

	CHECKMAT(ms_current);
}



void gpuMultMatrixd(const double *m)
{
	GLfloat mf[16];
	GLint i;

	GPU_CHECK_NO_ERROR();

	for(i = 0; i < 16; i++) {
		mf[i] = m[i];
	}

	gpuMultMatrix(mf);
}



void gpuRotateVector(GLfloat deg, GLfloat vector[3])
{
	float rm[3][3];
	GPU_matrix cm;

	GPU_CHECK_NO_ERROR();

	axis_angle_to_mat3(rm, vector, DEG2RADF(deg));

	copy_m4_m4(cm, current_matrix());
	mult_m4_m3m4_q(current_matrix(), cm, rm);

	ms_current->changed = GL_TRUE;

	CHECKMAT(ms_current);
}



void gpuRotateAxis(GLfloat deg, char axis)
{
	GPU_CHECK_NO_ERROR();

	rotate_m4(current_matrix(), axis, DEG2RADF(deg));

	ms_current->changed = GL_TRUE;

	CHECKMAT(ms_current);
}



void gpuRotateRight(char type)
{
	GPU_CHECK_NO_ERROR();

	rotate_m4_right(current_matrix(), type);

	ms_current->changed = GL_TRUE;

	CHECKMAT(ms_current);
}



void gpuLoadOrtho(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat nearVal, GLfloat farVal)
{
	GPU_CHECK_NO_ERROR();

	mat4_ortho_set(current_matrix(), left, right, bottom, top, nearVal, farVal);

	ms_current->changed = GL_TRUE;

	CHECKMAT(ms_current);
}



void gpuOrtho(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat nearVal, GLfloat farVal)
{
	GPU_matrix om;

	GPU_CHECK_NO_ERROR();

	mat4_ortho_set(om, left, right, bottom, top, nearVal, farVal);

	gpuMultMatrix((GLfloat*)om);
}



void gpuFrustum(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat nearVal, GLfloat farVal)
{
	GPU_matrix fm;

	GPU_CHECK_NO_ERROR();

	mat4_frustum_set(fm, left, right, bottom, top, nearVal, farVal);

	gpuMultMatrix((GLfloat*) fm);
}



void gpuLoadFrustum(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat nearVal, GLfloat farVal)
{
	GPU_CHECK_NO_ERROR();

	mat4_frustum_set(current_matrix(), left, right, bottom, top, nearVal, farVal);

	ms_current->changed = GL_TRUE;

	CHECKMAT(ms_current);
}



void gpuLookAt(GLfloat eyeX, GLfloat eyeY, GLfloat eyeZ, GLfloat centerX, GLfloat centerY, GLfloat centerZ, GLfloat upX, GLfloat upY, GLfloat upZ)
{
	GPU_matrix cm;
	GLfloat lookdir[3];
	GLfloat camup[3] = {upX, upY, upZ};

	GPU_CHECK_NO_ERROR();

	lookdir[0] =  centerX - eyeX;
	lookdir[1] =  centerY - eyeY;
	lookdir[2] =  centerZ - eyeZ;

	mat4_look_from_origin(cm, lookdir, camup);

	gpuMultMatrix((GLfloat*) cm);
	gpuTranslate(-eyeX, -eyeY, -eyeZ);
}



void gpuProject(const GLfloat obj[3], const GLfloat model[4][4], const GLfloat proj[4][4], const GLint view[4], GLfloat win[3])
{
	float v[4];

	GPU_CHECK_NO_ERROR();

	mul_v4_m4v3(v, model, obj);
	mul_m4_v4(proj, v);

	win[0]=view[0]+(view[2]*(v[0]+1))*0.5f;
	win[1]=view[1]+(view[3]*(v[1]+1))*0.5f;
	win[2]=(v[2]+1)*0.5f;
}



GLboolean gpuUnProject(const GLfloat win[3], const GLfloat model[4][4], const GLfloat proj[4][4], const GLint view[4], GLfloat obj[3])
{
	GLfloat pm[4][4];
	GLfloat in[4];
	GLfloat out[4];

	GPU_CHECK_NO_ERROR();

	mul_m4_m4m4(pm, proj, model);

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

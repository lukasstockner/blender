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
 * Contributor(s): Jason Wilkins, Alexandr Kuznetsov
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file GPU_compatibility.h
 *  \ingroup gpu
 */

#include <assert.h>

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

#if WITH_GPU_SAFETY
#define GPU_STACK_DEBUG
#endif

static GLint glslneedupdate = 1;

typedef GLfloat GPU_matrix[4][4];

typedef struct GPU_matrix_stack
{
	GLint size;
	GLint pos;
	GLint changed;
	GPU_matrix * dynstack;


} GPU_matrix_stack;

static GPU_matrix_stack ms_modelview;
static GPU_matrix_stack ms_projection;
static GPU_matrix_stack ms_texture;

static GPU_matrix_stack * ms_current;
static GLenum ms_current_mode;

void GPU_matrix_forced_update(void)
{

	glslneedupdate = 1;
		gpuMatrixCommit();
	glslneedupdate = 1;	
	
}

#define CURMATRIX (ms_current->dynstack[ms_current->pos])


/* Check if we have a good matrix */
#if WITH_GPU_SAFETY
static void checkmat(GLfloat *m)
{
	GLint i;
	for(i=0;i<16;i++) {
#if _MSC_VER
		BLI_assert(_finite(m[i]));
#else
		BLI_assert(!isinf(m[i]));
#endif
	}
}

#define CHECKMAT checkmat((GLfloat*)CURMATRIX);
#else
#define CHECKMAT
#endif

static void ms_init(GPU_matrix_stack * ms, GLint initsize)
{
	if(initsize == 0)
		initsize = 32;
	ms->size = initsize;
	ms->pos = 0;
	ms->changed = 1;
	ms->dynstack = MEM_mallocN(ms->size*sizeof(*(ms->dynstack)), "MatrixStack");
	//gpuLoadIdentity();
}

static void ms_free(GPU_matrix_stack * ms)
{
	ms->size = 0;
	ms->pos = 0;
	MEM_freeN(ms->dynstack);
	ms->dynstack = NULL;
}


static GLint glstackpos[3] = {0};
static GLint glstackmode;

void GPU_ms_init(void)
{
	ms_init(&ms_modelview, 32);
	ms_init(&ms_projection, 16);
	ms_init(&ms_texture, 16);

	ms_current = &ms_modelview;
	ms_current_mode = GL_MODELVIEW;



}

void GPU_ms_exit(void)
{
	ms_free(&ms_modelview);
	ms_free(&ms_projection);
	ms_free(&ms_texture);
}

void gpuMatrixLock(void)
{
#ifndef GLES
	GPU_matrix tm;
	glGetIntegerv(GL_MODELVIEW_STACK_DEPTH, glstackpos);
	glGetIntegerv(GL_PROJECTION_STACK_DEPTH, glstackpos+1);
	glGetIntegerv(GL_TEXTURE_STACK_DEPTH, glstackpos+2);
	glGetIntegerv(GL_MATRIX_MODE, &glstackmode);

	glGetFloatv(GL_MODELVIEW_MATRIX, (GLfloat*)tm);
	gpuMatrixMode(GL_MODELVIEW);
	gpuLoadMatrix((GLfloat*)tm);

	glGetFloatv(GL_PROJECTION_MATRIX, (GLfloat*)tm);
	gpuMatrixMode(GL_PROJECTION);
	gpuLoadMatrix((GLfloat*)tm);

	glGetFloatv(GL_TEXTURE_MATRIX, (GLfloat*)tm);
	gpuMatrixMode(GL_TEXTURE);
	gpuLoadMatrix((GLfloat*)tm);




	glMatrixMode(GL_TEXTURE);
	glPushMatrix();
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();

	glMatrixMode(glstackmode);
	gpuMatrixMode(glstackmode);

#endif

}


void gpuMatrixUnlock(void)
{

#ifndef GLES
	GLint curval;


	glMatrixMode(GL_TEXTURE);
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	glGetIntegerv(GL_MODELVIEW_STACK_DEPTH, &curval);


	glGetIntegerv(GL_PROJECTION_STACK_DEPTH, &curval);
	glGetIntegerv(GL_TEXTURE_STACK_DEPTH, &curval);






	glMatrixMode(glstackmode);

#endif

}

void gpuMatrixCommit(void)
{
if(GPU_GLTYPE_FIXED_ENABLED)
{
#ifndef GLES
	if(ms_modelview.changed)
	{
		ms_modelview.changed = 0;
		glMatrixMode(GL_MODELVIEW);
		glLoadMatrixf((GLfloat*)ms_modelview.dynstack[ms_modelview.pos]);
	}
	if(ms_projection.changed)
	{
		ms_projection.changed = 0;
		glMatrixMode(GL_PROJECTION);
		glLoadMatrixf((GLfloat*)ms_projection.dynstack[ms_projection.pos]);
	}
	if(ms_texture.changed)
	{
		ms_texture.changed = 0;
		glMatrixMode(GL_TEXTURE);
		glLoadMatrixf((GLfloat*)ms_texture.dynstack[ms_texture.pos]);
	}

#endif
} else if(curglslesi)
{
#include REAL_GL_MODE

	if(ms_modelview.changed || glslneedupdate)
	{

		 
		if(curglslesi->viewmatloc!=-1)
			glUniformMatrix4fv(curglslesi->viewmatloc, 1, 0, ms_modelview.dynstack[ms_modelview.pos][0]);
			
		if(curglslesi->normalmatloc!=-1)
		{
			GLfloat t[3][3] = {1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0};
			copy_m3_m4(t, ms_modelview.dynstack[ms_modelview.pos]);
			glUniformMatrix3fv(curglslesi->normalmatloc, 1, 0, t[0]);
		}
		
		
	}
	if(ms_projection.changed|| glslneedupdate)
	{
		if(curglslesi->projectionmatloc!=-1)
		glUniformMatrix4fv(curglslesi->projectionmatloc, 1, 0, ms_projection.dynstack[ms_projection.pos][0]);
	}
	
	//if(ms_texture.changed|| glslneedupdate)
	{
		if(curglslesi->texturematloc!=-1)
		glUniformMatrix4fv(curglslesi->texturematloc, 1, 0, ms_texture.dynstack[ms_texture.pos][0]);
	}
}

}




void gpuPushMatrix(void)
{
	ms_current->pos++;
	
	if(ms_current->pos >= ms_current->size)
	{
		ms_current->size += ((ms_current->size-1)>>1)+1; 
		/* increases size by 50% */
		ms_current->dynstack = MEM_reallocN(ms_current->dynstack,
											ms_current->size*sizeof(*(ms_current->dynstack)));
											
	
	}

	gpuLoadMatrix((GLfloat*)ms_current->dynstack[ms_current->pos-1]);
	CHECKMAT

}

void gpuPopMatrix(void)
{
	ms_current->pos--;




	ms_current->changed = 1;


#ifdef GPU_STACK_DEBUG
	if(ms_current->pos < 0)
		assert(0);
#endif	
	CHECKMAT
}


void gpuMatrixMode(GLenum mode)
{
	GPU_ASSERT(ELEM3(mode, GL_MODELVIEW, GL_PROJECTION, GL_TEXTURE));

	ms_current_mode = mode;

	switch(mode)
	{
		case GL_MODELVIEW:
			ms_current = &ms_modelview;
			break;
		case GL_PROJECTION:
			ms_current = &ms_projection;
			break;
		case GL_TEXTURE:
			ms_current = & ms_texture;
			break;
		default:
			/* ignore */
			break;
	}

CHECKMAT
}

GLenum gpuGetMatrixMode(void)
{
	return ms_current_mode;
}

void gpuLoadMatrix(const GLfloat * m)
{
	copy_m4_m4((GLfloat (*)[4])CURMATRIX, (GLfloat (*)[4])m);
	ms_current->changed = 1;
	CHECKMAT
}

const GLfloat * gpuGetMatrix(GLenum type, GLfloat *m)
{
	GPU_matrix_stack * ms_select;

	GPU_ASSERT(ELEM3(type, GL_MODELVIEW_MATRIX, GL_PROJECTION_MATRIX, GL_TEXTURE_MATRIX));

	switch(type)
	{
		case GL_MODELVIEW_MATRIX:
			ms_select = &ms_modelview;
			break;
		case GL_PROJECTION_MATRIX:
			ms_select = &ms_projection;
			break;
		case GL_TEXTURE_MATRIX:
			ms_select = & ms_texture;
			break;
		default:
			return 0;
	}

	if (m)
		copy_m4_m4((GLfloat (*)[4])m, ms_select->dynstack[ms_select->pos]);
	else
		return (GLfloat*)(ms_select->dynstack[ms_select->pos]);

	return 0;


}

void gpuLoadIdentity(void)
{
	unit_m4(CURMATRIX);
	ms_current->changed = 1;
	CHECKMAT
}




void gpuTranslate(GLfloat x, GLfloat y, GLfloat z)
{

	translate_m4(CURMATRIX, x, y, z);
	ms_current->changed = 1;
	CHECKMAT

}

void gpuScale(GLfloat x, GLfloat y, GLfloat z)
{

	scale_m4(CURMATRIX, x, y, z);
	ms_current->changed = 1;
	CHECKMAT
}


void gpuMultMatrix(const GLfloat *m)
{
	GPU_matrix cm;

	copy_m4_m4((GLfloat (*)[4])cm, (GLfloat (*)[4])CURMATRIX);

	mult_m4_m4m4_q(CURMATRIX, cm, (GLfloat (*)[4])m);
	ms_current->changed = 1;
	CHECKMAT

}


void gpuMultMatrixd(const double *m)
{
	GLfloat mf[16];
	GLint i;
	for(i=0; i<16; i++)
		mf[i] = m[i];
	gpuMultMatrix(mf);

}


void gpuRotateVector(GLfloat angle, GLfloat * vector)
{
	float rm[3][3];
	GPU_matrix cm;
	copy_m4_m4((GLfloat (*)[4])cm, (GLfloat (*)[4])CURMATRIX);

	axis_angle_to_mat3(rm, vector, angle);
	mult_m4_m3m4_q(CURMATRIX, cm, rm);

	ms_current->changed = 1;

}

void gpuRotateAxis(GLfloat angle, char axis)
{

	rotate_m4((GLfloat (*)[4])CURMATRIX, axis, angle);
	ms_current->changed = 1;
}

void gpuRotateRight(char type)
{
	rotate_m4_right((GLfloat (*)[4])CURMATRIX, type);

	ms_current->changed = 1;

}

void gpuLoadOrtho(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat nearVal, GLfloat farVal)
{
	mat4_ortho_set(CURMATRIX, left, right, bottom, top, nearVal, farVal);
	ms_current->changed = 1;
	CHECKMAT
}


void gpuOrtho(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat nearVal, GLfloat farVal)
{
	GPU_matrix om;

	mat4_ortho_set(om, left, right, bottom, top, nearVal, farVal);

	gpuMultMatrix((GLfloat*)om);
	CHECKMAT
}


void gpuFrustum(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat nearVal, GLfloat farVal)
{
	GPU_matrix fm;
	mat4_frustum_set(fm, left, right, bottom, top, nearVal, farVal);
	gpuMultMatrix((GLfloat*) fm);
	CHECKMAT
}

void gpuLoadFrustum(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat nearVal, GLfloat farVal)
{
	mat4_frustum_set(CURMATRIX, left, right, bottom, top, nearVal, farVal);
	ms_current->changed = 1;
	CHECKMAT
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
	CHECKMAT

}

void gpuProject(const GLfloat obj[3], const GLfloat model[4][4], const GLfloat proj[4][4], const GLint view[4], GLfloat win[3])
{
	float v[4];

	mul_v4_m4v3(v, model, obj);
	mul_m4_v4(proj, v);

	win[0]=view[0]+(view[2]*(v[0]+1))*0.5f;
	win[1]=view[1]+(view[3]*(v[1]+1))*0.5f;
	win[2]=(v[2]+1)*0.5f;
}


int gpuUnProject(const GLfloat win[3], const GLfloat model[4][4], const GLfloat proj[4][4], const GLint view[4], GLfloat obj[3])
{

	float v[3];
	GPU_matrix pm;

	int i;
	double modeld[16], projd[16];
	double objd[3] = {0};
	for(i=0; i<16; i++)
	{
		modeld[i] = model[0][i];
		projd[i] = proj[0][i];
	}
	if(!gluUnProject(win[0], win[1], win[2], modeld, projd, view, objd, objd+1, objd+2))
		BLI_assert(0);

	mult_m4_m4m4_q(pm, proj, model);

	if(!invert_m4(pm))
		return FALSE;




	v[0] = 2.0*(win[0]-view[0])/view[2] - 1.0f;
	v[1] = 2.0*(win[1]-view[1])/view[3] - 1.0f;
	v[2] = 2.0*(win[2]		   )		 - 1.0f;


	mul_v3_m4v3_q(obj, pm, v);
	/*obj[0]/=10;
obj[1]/=10;
obj[2]/=10;
obj[0]=0;
obj[1]=0;
obj[2]=0;*/
	obj[0] = objd[0];
	obj[1] = objd[1];
	obj[2] = objd[2];
	return TRUE;

}

#include <assert.h>

#include "MEM_guardedalloc.h"

#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"


#include "GPU_matrix.h"

#ifdef GLES
#include <GLES2/gl2.h>
#else 
#include <GL/glew.h>
#endif

typedef float GPU_matrix[4][4];

typedef struct GPU_matrix_stack
{
	int size;
	int pos;
	int changed;
	GPU_matrix * dynstack;


} GPU_matrix_stack;

GPU_matrix_stack ms_modelview;
GPU_matrix_stack ms_projection;
GPU_matrix_stack ms_texture;

GPU_matrix_stack * ms_current;

#define CURMATRIX (ms_current->dynstack[ms_current->pos])

static void ms_init(GPU_matrix_stack * ms, int initsize)
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


static int glstackpos[3] = {0};
static int glstackmode;

void GPU_ms_init(void)
{
	ms_init(&ms_modelview, 32);
	ms_init(&ms_projection, 16);
	ms_init(&ms_texture, 16);

	ms_current = &ms_modelview;

	printf("Stack init\n");



}

void GPU_ms_exit(void)
{
	ms_free(&ms_modelview);
	ms_free(&ms_projection);
	ms_free(&ms_texture);

	printf("Stack exit\n");



}

void gpuMatrixLock(void)
{
#ifndef GLES
	GPU_matrix tm;
	glGetIntegerv(GL_MODELVIEW_STACK_DEPTH, glstackpos);
	glGetIntegerv(GL_PROJECTION_STACK_DEPTH, glstackpos+1);
	glGetIntegerv(GL_TEXTURE_STACK_DEPTH, glstackpos+2);
	glGetIntegerv(GL_MATRIX_MODE, &glstackmode);

	glGetFloatv(GL_MODELVIEW_MATRIX, tm);
	gpuMatrixMode(GPU_MODELVIEW);
	gpuLoadMatrix(tm);

	glGetFloatv(GL_PROJECTION_MATRIX, tm);
	gpuMatrixMode(GPU_PROJECTION);
	gpuLoadMatrix(tm);

	glGetFloatv(GL_TEXTURE_MATRIX, tm);
	gpuMatrixMode(GPU_TEXTURE);
	gpuLoadMatrix(tm);




	glMatrixMode(GL_TEXTURE);
	glPushMatrix();
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();

	glMatrixMode(glstackmode);
	switch(glstackmode)
	{
		case GL_MODELVIEW:
			gpuMatrixMode(GPU_MODELVIEW);
			break;
		case GL_TEXTURE:
			gpuMatrixMode(GPU_TEXTURE);
			break;
		case GL_PROJECTION:
			gpuMatrixMode(GPU_PROJECTION);
			break;

	}


#endif

}


void gpuMatrixUnlock(void)
{

#ifndef GLES
	int curval;


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
#ifndef GLES
	if(ms_modelview.changed)
	{
		ms_modelview.changed = 0;
		glMatrixMode(GL_MODELVIEW);
		glLoadMatrixf(ms_modelview.dynstack[ms_modelview.pos]);
	}
	if(ms_projection.changed)
	{
		ms_projection.changed = 0;
		glMatrixMode(GL_PROJECTION);
		glLoadMatrixf(ms_projection.dynstack[ms_projection.pos]);
	}
	if(ms_texture.changed)
	{
		ms_texture.changed = 0;
		glMatrixMode(GL_TEXTURE);
		glLoadMatrixf(ms_texture.dynstack[ms_texture.pos]);
	}

#endif

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

	gpuLoadMatrix(ms_current->dynstack[ms_current->pos-1]);

}

void gpuPopMatrix(void)
{
	ms_current->pos--;
	ms_current->changed = 1;
#ifdef GLU_STACK_DEBUG
	if(ms_current->pos < 0)
		assert(0);
#endif	
}


void gpuMatrixMode(int mode)
{

	switch(mode)
	{
		case GPU_MODELVIEW:
			ms_current = &ms_modelview;
			break;
		case GPU_PROJECTION:
			ms_current = &ms_projection;
			break;
		case GPU_TEXTURE:
			ms_current = & ms_texture;
			break;
	#if 1
		default:
			assert(0);
	
	#endif
	
	}


}


void gpuLoadMatrix(const float * m)
{
	copy_m4_m4(CURMATRIX, m);
	ms_current->changed = 1;
}

void gpuGetMatrix(float * m)
{
	copy_m4_m4((float (*)[4])m,CURMATRIX);
	ms_current->changed = 1;
}


void gpuLoadIdentity(void)
{
	unit_m4(CURMATRIX);
	ms_current->changed = 1;
}




void gpuTranslate(float x, float y, float z)
{

	translate_m4(CURMATRIX, x, y, z);
	ms_current->changed = 1;

}

void gpuScale(float x, float y, float z)
{

	scale_m4(CURMATRIX, x, y, z);
	ms_current->changed = 1;

}



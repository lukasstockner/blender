/**
 * $Id: gpu_buffers.c 19820 2009-04-20 15:06:46Z imbusy $
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2005 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Brecht Van Lommel.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <string.h>

#include "GL/glew.h"

#include "gpu_buffers.h"
#include "MEM_guardedalloc.h"
#include "BKE_DerivedMesh.h"
#include "BKE_utildefines.h"
#include "DNA_meshdata_types.h"
#include "BLI_arithb.h"

/* -1 - undefined, 0 - vertex arrays, 1 - VBOs */
int useVBOs = -1;
GPUBufferPool *globalPool = 0;

GPUBufferPool *GPU_buffer_pool_new()
{
	GPUBufferPool *pool;

	if( useVBOs < 0 ) {
		if( GL_ARB_vertex_buffer_object )
			useVBOs = 1;
		else
			useVBOs = 0;
	}

	pool = MEM_mallocN(sizeof(GPUBufferPool), "GPU_buffer_pool_new");
	pool->size = 0;
	pool->start = 0;

	return pool;
}

void GPU_buffer_pool_free(GPUBufferPool *pool)
{
	int i;

	while( pool->start < 0 )
		pool->start += MAX_FREE_GPU_BUFFERS;

	for( i = 0; i < pool->size; i++ ) {
		if( useVBOs ) {
			glDeleteBuffersARB( 1, &pool->buffers[(pool->start+i)%MAX_FREE_GPU_BUFFERS]->id );
		}
		else {
			MEM_freeN( pool->buffers[(pool->start+i) % MAX_FREE_GPU_BUFFERS ]->pointer );
		}
	}
}

void GPU_buffer_pool_remove( int index, GPUBufferPool *pool )
{
	int i;
	while( pool->start < 0 )
		pool->start += MAX_FREE_GPU_BUFFERS;
	for( i = index; i < pool->size-1; i++ ) {
		pool->buffers[(pool->start+i)%MAX_FREE_GPU_BUFFERS] = pool->buffers[(pool->start+i+1)%MAX_FREE_GPU_BUFFERS];
	}
	pool->size--;
}

void GPU_buffer_pool_delete_last( GPUBufferPool *pool )
{
	int last;

	while( pool->start < 0 )
		pool->start += MAX_FREE_GPU_BUFFERS;
	last = (pool->start+pool->size)%MAX_FREE_GPU_BUFFERS;

	if( useVBOs ) {
		glDeleteBuffersARB(1,&pool->buffers[last]->id);
		MEM_freeN( pool->buffers[last] );
	}
	else {
		MEM_freeN( pool->buffers[last]->pointer );
		MEM_freeN( pool->buffers[last] );
	}
	pool->size--;
}

GPUBuffer *GPU_buffer_alloc( int size, GPUBufferPool *pool )
{
	int i;
	int cursize;
	GPUBuffer *allocated;
	int bestfit = -1;

	while( pool->start < 0 )
		pool->start += MAX_FREE_GPU_BUFFERS;

	for( i = 0; i < pool->size; i++ ) {
		cursize = pool->buffers[(pool->start+i)%MAX_FREE_GPU_BUFFERS]->size;
		if( cursize == size ) {
			allocated = pool->buffers[pool->start+i];
			GPU_buffer_pool_remove(i,pool);
			return allocated;
		}
		/* smaller buffers won't fit data and buffers at least twice as big are a waste of memory */
		else if( cursize > size && size > cursize/2 ) {
			/* is it closer to the required size than the last appropriate buffer found. try to save memory */
			if( bestfit == -1 || pool->buffers[(pool->start+bestfit)%MAX_FREE_GPU_BUFFERS]->size > cursize ) {
				bestfit = i;
			}
		}
	}
	if( bestfit == -1 ) {
		allocated = MEM_mallocN(sizeof(GPUBuffer), "GPU_buffer_alloc");
		allocated->size = size;
		if( useVBOs == 1 ) {
			glGenBuffersARB( 1, &allocated->id );
			glBindBufferARB( GL_ARRAY_BUFFER_ARB, allocated->id );
			glBufferDataARB( GL_ARRAY_BUFFER_ARB, size, 0, GL_STATIC_DRAW_ARB );
		}
		else {
			allocated->pointer = MEM_mallocN(size, "GPU_buffer_alloc_vertexarray");
			while( allocated->pointer == 0 && pool->size > 0 ) {
				GPU_buffer_pool_delete_last(pool);
				allocated->pointer = MEM_mallocN(size, "GPU_buffer_alloc_vertexarray");
			}
			if( allocated->pointer == 0 && pool->size == 0 ) {
				/* report an out of memory error. not sure how to do that */
			}
		}
	}
	else {
		allocated = pool->buffers[pool->start+bestfit];
		GPU_buffer_pool_remove(bestfit,pool);
	}
	return allocated;
}

void GPU_buffer_free( GPUBuffer *buffer, GPUBufferPool *pool )
{
	int place;

	if( buffer == 0 )
		return;

	while( pool->start < 0 )
		pool->start += MAX_FREE_GPU_BUFFERS;
	place = (pool->start-1 + MAX_FREE_GPU_BUFFERS)%MAX_FREE_GPU_BUFFERS;

	/* free the last used buffer in the queue if no more space */
	if( pool->size == MAX_FREE_GPU_BUFFERS ) {
		GPU_buffer_pool_delete_last( pool );
	}

	pool->start = place;
	pool->buffers[place] = buffer;
}

GPUDrawObject *GPU_drawobject_new( DerivedMesh *dm )
{
	GPUDrawObject *object;
	MVert *mvert;
	MFace *mface;
	int numverts[256];	/* material number is an 8-bit char so there's at most 256 materials */
	int i;
	int curmat, curverts;

	object = MEM_callocN(sizeof(GPUDrawObject),"GPU_drawobject_new");

	memset(numverts,0,sizeof(int)*256);

	mvert = dm->getVertArray(dm);
	mface = dm->getFaceArray(dm);

	for( i=0; i < dm->getNumFaces(dm); i++ ) {
		if( mface[i].v4 )
			numverts[mface[i].mat_nr] += 6;	/* split every quad into two triangles */
		else
			numverts[mface[i].mat_nr] += 3;
	}

	//float *array = MEM_calloc(dm->getNumVerts(dm)*sizeof(float)*3); for(i = 0; i < dm->getNumVerts(dm); i++) { VECCOPY(array[i], mvert[i].co);}

	for( i = 0; i < 256; i++ ) {
		if( numverts[i] > 0 ) {
			object->nmaterials++;
			object->nelements += numverts[i];
		}
	}
	object->materials = MEM_mallocN(sizeof(GPUBufferMaterial)*object->nmaterials,"GPU_drawobject_new_materials");

	curmat = curverts = 0;
	for( i = 0; i < 256; i++ ) {
		if( numverts[i] > 0 ) {
			object->materials[curmat].mat_nr = i;
			object->materials[curmat].start = curverts;
			object->materials[curmat].end = curverts+numverts[i];
			curverts += numverts[i];
			curmat++;
		}
	}

	return object;
}

void GPU_drawobject_free( GPUDrawObject *object )
{
	if( object == 0 )
		return;

	MEM_freeN(object->materials);

	GPU_buffer_free( object->vertices, globalPool );
	GPU_buffer_free( object->normals, globalPool );
	GPU_buffer_free( object->uv, globalPool );
	GPU_buffer_free( object->colors, globalPool );

	MEM_freeN(object);
}

GPUBuffer *GPU_buffer_setup( DerivedMesh *dm, GPUDrawObject *object, int size, void (*copy_f)(DerivedMesh *, float *, int *, int *) )
{
	GPUBuffer *buffer;
	float *varray;
	int redir[256];
	int *index;
	int i;
	GLboolean uploaded;

	index = MEM_mallocN(sizeof(int)*object->nmaterials,"GPU_buffer_setup");
	for( i = 0; i < object->nmaterials; i++ ) {
		index[i] = object->materials[i].start;
		redir[object->materials[i].mat_nr] = i;
	}

	buffer = GPU_buffer_alloc(size,globalPool);

	if( useVBOs ) {
		glBindBufferARB( GL_ARRAY_BUFFER_ARB, buffer->id );
		glBufferDataARB( GL_ARRAY_BUFFER_ARB, buffer->size, 0, GL_STATIC_DRAW_ARB );	/* discard previous data, avoid stalling gpu */
		varray = glMapBufferARB( GL_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB );
		if( varray == 0 )
			printf( "Failed to map buffer to client address space" );

		uploaded = GL_FALSE;
		while( !uploaded ) {
			(*copy_f)( dm, varray, index, redir );
			uploaded = glUnmapBufferARB( GL_ARRAY_BUFFER_ARB );	/* returns false if data got corruped during transfer */
		}
	}
	else {
		varray = buffer->pointer;
		(*copy_f)( dm, varray, index, redir );
	}

	MEM_freeN(index);

	return buffer;
}

void GPU_buffer_copy_vertex( DerivedMesh *dm, float *varray, int *index, int *redir )
{
	int start;
	int i;

	MVert *mvert;
	MFace *mface;

	mvert = dm->getVertArray(dm);
	mface = dm->getFaceArray(dm);

	for( i=0; i < dm->getNumFaces(dm); i++ ) {
		start = index[redir[mface[i].mat_nr]];
		if( mface[i].v4 )
			index[redir[mface[i].mat_nr]] += 18;
		else
			index[redir[mface[i].mat_nr]] += 9;

		/* v1 v2 v3 */
		VECCOPY(&varray[start],mvert[mface[i].v1].co);
		VECCOPY(&varray[start+3],mvert[mface[i].v2].co);
		VECCOPY(&varray[start+6],mvert[mface[i].v3].co);

		if( mface[i].v4 ) {
			/* v3 v4 v1 */
			VECCOPY(&varray[start+9],mvert[mface[i].v3].co);
			VECCOPY(&varray[start+12],mvert[mface[i].v4].co);
			VECCOPY(&varray[start+15],mvert[mface[i].v1].co);
		}
	}
}

GPUBuffer *GPU_buffer_vertex( DerivedMesh *dm, GPUDrawObject *object )
{
	return GPU_buffer_setup( dm, object, sizeof(float)*3*object->nelements, GPU_buffer_copy_vertex);
}

void GPU_buffer_copy_normal( DerivedMesh *dm, float *varray, int *index, int *redir )
{
	int i;
	int start;
	float norm[3];

	MVert *mvert;
	MFace *mface;

	mvert = dm->getVertArray(dm);
	mface = dm->getFaceArray(dm);

	for( i=0; i < dm->getNumFaces(dm); i++ ) {
		start = index[redir[mface[i].mat_nr]];
		if( mface[i].v4 )
			index[redir[mface[i].mat_nr]] += 18;
		else
			index[redir[mface[i].mat_nr]] += 9;

		/* v1 v2 v3 */
		if( mface->flag & ME_SMOOTH ) {
			VECCOPY(&varray[start],mvert[mface[i].v1].no);
			VECCOPY(&varray[start+3],mvert[mface[i].v2].no);
			VECCOPY(&varray[start+6],mvert[mface[i].v3].no);
		}
		else {
			if( mface[i].v4 )
				CalcNormFloat4(mvert[mface[i].v1].co, mvert[mface[i].v2].co, mvert[mface[i].v3].co, mvert[mface[i].v4].co, norm);
			else
				CalcNormFloat(mvert[mface[i].v1].co, mvert[mface[i].v2].co, mvert[mface[i].v3].co, norm);
			VECCOPY(&varray[start],norm);
			VECCOPY(&varray[start+3],norm);
			VECCOPY(&varray[start+6],norm);
		}

		if( mface[i].v4 ) {
			/* v3 v4 v1 */
			if( mface->flag & ME_SMOOTH ) {
				VECCOPY(&varray[start],mvert[mface[i].v3].no);
				VECCOPY(&varray[start+3],mvert[mface[i].v4].no);
				VECCOPY(&varray[start+6],mvert[mface[i].v1].no);
			}
			else {
				VECCOPY(&varray[start],norm);
				VECCOPY(&varray[start+3],norm);
				VECCOPY(&varray[start+6],norm);
			}
		}
	}
}

GPUBuffer *GPU_buffer_normal( struct DerivedMesh *dm, GPUDrawObject *object )
{
	return GPU_buffer_setup( dm, object, sizeof(float)*3*object->nelements, GPU_buffer_copy_normal);
}

void GPU_buffer_copy_uv( DerivedMesh *dm, float *varray, int *index, int *redir )
{
	int start;
	int i;

	MTFace *mtface;
	MFace *mface;

	mface = dm->getFaceArray(dm);
	mtface = DM_get_face_data_layer(dm, CD_MTFACE);

	for( i=0; i < dm->getNumFaces(dm); i++ ) {
		start = index[redir[mface[i].mat_nr]];
		if( mface[i].v4 )
			index[redir[mface[i].mat_nr]] += 12;
		else
			index[redir[mface[i].mat_nr]] += 6;

		/* v1 v2 v3 */
		VECCOPY2D(&varray[start],mtface[i].uv[0]);
		VECCOPY2D(&varray[start+2],mtface[i].uv[1]);
		VECCOPY2D(&varray[start+4],mtface[i].uv[2]);

		if( mface[i].v4 ) {
			/* v3 v4 v1 */
			VECCOPY2D(&varray[start+6],mtface[i].uv[2]);
			VECCOPY2D(&varray[start+8],mtface[i].uv[3]);
			VECCOPY2D(&varray[start+10],mtface[i].uv[0]);
		}
	}
}

GPUBuffer *GPU_buffer_uv( struct DerivedMesh *dm, GPUDrawObject *object )
{
	return GPU_buffer_setup( dm, object, sizeof(float)*2*object->nelements, GPU_buffer_copy_uv);
}

void GPU_buffer_copy_color( DerivedMesh *dm, float *varray_, int *index, int *redir )
{
	int start;
	int i;
	
	char *varray;
	MFace *mface;
	MCol *mcol;

	varray = (char *)varray_;

	mface = dm->getFaceArray(dm);
	mcol = DM_get_face_data_layer(dm, CD_WEIGHT_MCOL);
	if(!mcol)
		mcol = DM_get_face_data_layer(dm, CD_MCOL);

	for( i=0; i < dm->getNumFaces(dm); i++ ) {
		start = index[redir[mface[i].mat_nr]];
		if( mface[i].v4 )
			index[redir[mface[i].mat_nr]] += 18;
		else
			index[redir[mface[i].mat_nr]] += 9;

		/* v1 v2 v3 */
		varray[start] = mcol[i*4].r;
		varray[start+1] = mcol[i*4].g;
		varray[start+2] = mcol[i*4].b;
		varray[start+3] = mcol[i*4+1].r;
		varray[start+4] = mcol[i*4+1].g;
		varray[start+5] = mcol[i*4+1].b;
		varray[start+6] = mcol[i*4+2].r;
		varray[start+7] = mcol[i*4+2].g;
		varray[start+8] = mcol[i*4+2].b;

		if( mface[i].v4 ) {
			/* v3 v4 v1 */
			varray[start+9] = mcol[i*4+2].r;
			varray[start+10] = mcol[i*4+2].g;
			varray[start+11] = mcol[i*4+2].b;
			varray[start+12] = mcol[i*4+3].r;
			varray[start+13] = mcol[i*4+3].g;
			varray[start+14] = mcol[i*4+3].b;
			varray[start+15] = mcol[i*4].r;
			varray[start+16] = mcol[i*4].g;
			varray[start+17] = mcol[i*4].b;
		}
	}
}

GPUBuffer *GPU_buffer_color( struct DerivedMesh *dm, GPUDrawObject *object )
{
	return GPU_buffer_setup( dm, object, sizeof(char)*3*object->nelements, GPU_buffer_copy_color);
}

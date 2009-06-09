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
#include "DNA_meshdata_types.h"

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
			glGenBuffersARB( 1, &allocated->id );	/* the actual size is specified later when copying data. Theoretically could be different */
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

	object = MEM_mallocN(sizeof(GPUDrawObject),"GPU_drawobject_new");
	memset(object,0,sizeof(GPUDrawObject));

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

GPUBuffer *GPU_buffer_vertex( DerivedMesh *dm, GPUDrawObject *object )
{
	GPUBuffer *buffer;
	MVert *mvert;
	MFace *mface;
	float *varray;
	int start;
	int redir[256];
	int *index;
	int i;

	index = MEM_mallocN(sizeof(int)*object->nmaterials,"GPU_buffer_vertex");
	for( i = 0; i < object->nmaterials; i++ )
	{
		index[i] = object->materials[i].start;
		redir[object->materials[i].mat_nr] = i;
	}
	mvert = dm->getVertArray(dm);
	mface = dm->getFaceArray(dm);

	buffer = GPU_buffer_alloc(sizeof(float)*3*object->nelements,globalPool);

	if( useVBOs ) {
		printf("GPU_buffer_vertex VBO not implemented yet");
	}
	else {
		varray = buffer->pointer;
		for( i=0; i < dm->getNumFaces(dm); i++ ) {
			start = index[redir[mface[i].mat_nr]];
			if( mface[i].v4 )
				index[redir[mface[i].mat_nr]] += 18;
			else
				index[redir[mface[i].mat_nr]] += 9;

			varray[start] = mvert[mface[i].v1].co[0];
			varray[start+1] = mvert[mface[i].v1].co[1];
			varray[start+2] = mvert[mface[i].v1].co[2];
			varray[start+3] = mvert[mface[i].v2].co[0];
			varray[start+4] = mvert[mface[i].v2].co[1];
			varray[start+5] = mvert[mface[i].v2].co[2];
			varray[start+6] = mvert[mface[i].v3].co[0];
			varray[start+7] = mvert[mface[i].v3].co[1];
			varray[start+8] = mvert[mface[i].v3].co[2];

			if( mface[i].v4 ) {
				varray[start+9] = mvert[mface[i].v3].co[0];
				varray[start+10] = mvert[mface[i].v3].co[1];
				varray[start+11] = mvert[mface[i].v3].co[2];
				varray[start+12] = mvert[mface[i].v4].co[0];
				varray[start+13] = mvert[mface[i].v4].co[1];
				varray[start+14] = mvert[mface[i].v4].co[2];
				varray[start+15] = mvert[mface[i].v1].co[0];
				varray[start+16] = mvert[mface[i].v1].co[1];
				varray[start+17] = mvert[mface[i].v1].co[2];
			}
		}
	}

	MEM_freeN(index);

	return buffer;
}
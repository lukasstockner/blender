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

#include "GL/glew.h"

#include "gpu_buffers.h"
#include "MEM_guardedalloc.h"

/* -1 - undefined, 0 - vertex arrays, 1 - VBOs */
int useVBOs = -1;

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

	while( pool->start < 0 )
		pool->start += MAX_FREE_GPU_BUFFERS;
	place = (pool->start-1 + MAX_FREE_GPU_BUFFERS)%MAX_FREE_GPU_BUFFERS;

	/* free the last used buffer in the queue if no more space */
	if( pool->size == MAX_FREE_GPU_BUFFERS ) {
		if( useVBOs ) {
			glDeleteBuffersARB(1,&pool->buffers[place]->id);
			MEM_freeN( pool->buffers[place] );
		}
		else {
			MEM_freeN( pool->buffers[place]->pointer );
			MEM_freeN( pool->buffers[place] );
		}
		pool->size--;
	}

	pool->start = place;
	pool->buffers[pool->start] = buffer;
}

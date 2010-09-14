/**
 * $Id$
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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

#include <limits.h>
#include <stddef.h>
#include <string.h>

#include "GL/glew.h"
#include "ptex.h"

#include "MEM_guardedalloc.h"

#include "BLI_ghash.h"
#include "BLI_math.h"
#include "BLI_threads.h"
#include "BLI_pbvh.h"

#include "DNA_meshdata_types.h"

#include "BKE_DerivedMesh.h"
#include "BKE_dmgrid.h"
#include "BKE_paint.h"
#include "BKE_utildefines.h"

#include "DNA_userdef_types.h"

#include "GPU_buffers.h"

#define GPU_BUFFER_VERTEX_STATE 1
#define GPU_BUFFER_NORMAL_STATE 2
#define GPU_BUFFER_TEXCOORD_STATE 4
#define GPU_BUFFER_COLOR_STATE 8
#define GPU_BUFFER_ELEMENT_STATE 16

#define MAX_GPU_ATTRIB_DATA 32

/* -1 - undefined, 0 - vertex arrays, 1 - VBOs */
int useVBOs = -1;
GPUBufferPool *globalPool = 0;
int GLStates = 0;
GPUAttrib attribData[MAX_GPU_ATTRIB_DATA] = { { -1, 0, 0 } };

GPUBufferPool *GPU_buffer_pool_new()
{
	GPUBufferPool *pool;

	DEBUG_VBO("GPU_buffer_pool_new\n");

	if( useVBOs < 0 ) {
		if( GL_ARB_vertex_buffer_object ) {
			DEBUG_VBO( "Vertex Buffer Objects supported.\n" );
			useVBOs = 1;
		}
		else {
			DEBUG_VBO( "Vertex Buffer Objects NOT supported.\n" );
			useVBOs = 0;
		}
	}

	pool = MEM_callocN(sizeof(GPUBufferPool), "GPU_buffer_pool_new");
	pool->maxsize = MAX_FREE_GPU_BUFFERS;
	pool->buffers = MEM_callocN(sizeof(GPUBuffer*)*pool->maxsize, "GPU_buffer_pool_new buffers");

	return pool;
}

void GPU_buffer_pool_remove( int index, GPUBufferPool *pool )
{
	int i;

	if( index >= pool->size || index < 0 ) {
		ERROR_VBO("Wrong index, out of bounds in call to GPU_buffer_pool_remove");
		return;
	}
	DEBUG_VBO("GPU_buffer_pool_remove\n");

	for( i = index; i < pool->size-1; i++ ) {
		pool->buffers[i] = pool->buffers[i+1];
	}
	if( pool->size > 0 )
		pool->buffers[pool->size-1] = 0;

	pool->size--;
}

void GPU_buffer_pool_delete_last( GPUBufferPool *pool )
{
	int last;

	DEBUG_VBO("GPU_buffer_pool_delete_last\n");

	if( pool->size <= 0 )
		return;

	last = pool->size-1;

	if( pool->buffers[last] != 0 ) {
		if( useVBOs ) {
			glDeleteBuffersARB(1,&pool->buffers[last]->id);
			MEM_freeN( pool->buffers[last] );
		}
		else {
			MEM_freeN( pool->buffers[last]->pointer );
			MEM_freeN( pool->buffers[last] );
		}
		pool->buffers[last] = 0;
	} else {
		DEBUG_VBO("Why are we accessing a null buffer?\n");
	}
	pool->size--;
}

void GPU_buffer_pool_free(GPUBufferPool *pool)
{
	DEBUG_VBO("GPU_buffer_pool_free\n");

	if( pool == 0 )
		pool = globalPool;
	if( pool == 0 )
		return;
	
	while( pool->size )
		GPU_buffer_pool_delete_last(pool);

	MEM_freeN(pool->buffers);
	MEM_freeN(pool);
	/* if we are releasing the global pool, stop keeping a reference to it */
	if (pool == globalPool)
		globalPool = NULL;
}

void GPU_buffer_pool_free_unused(GPUBufferPool *pool)
{
	DEBUG_VBO("GPU_buffer_pool_free_unused\n");

	if( pool == 0 )
		pool = globalPool;
	if( pool == 0 )
		return;
	
	while( pool->size > MAX_FREE_GPU_BUFFERS )
		GPU_buffer_pool_delete_last(pool);
}

GPUBuffer *GPU_buffer_alloc( int size, GPUBufferPool *pool )
{
	char buffer[60];
	int i;
	int cursize;
	GPUBuffer *allocated;
	int bestfit = -1;

	DEBUG_VBO("GPU_buffer_alloc\n");

	if( pool == 0 ) {
		if( globalPool == 0 )
			globalPool = GPU_buffer_pool_new();
		pool = globalPool;
	}

	for( i = 0; i < pool->size; i++ ) {
		cursize = pool->buffers[i]->size;
		if( cursize == size ) {
			allocated = pool->buffers[i];
			GPU_buffer_pool_remove(i,pool);
			DEBUG_VBO("free buffer of exact size found\n");
			return allocated;
		}
		/* smaller buffers won't fit data and buffers at least twice as big are a waste of memory */
		else if( cursize > size && size > cursize/2 ) {
			/* is it closer to the required size than the last appropriate buffer found. try to save memory */
			if( bestfit == -1 || pool->buffers[bestfit]->size > cursize ) {
				bestfit = i;
			}
		}
	}
	if( bestfit == -1 ) {
		DEBUG_VBO("allocating a new buffer\n");

		allocated = MEM_mallocN(sizeof(GPUBuffer), "GPU_buffer_alloc");
		allocated->size = size;
		if( useVBOs == 1 ) {
			glGenBuffersARB( 1, &allocated->id );
			glBindBufferARB( GL_ARRAY_BUFFER_ARB, allocated->id );
			glBufferDataARB( GL_ARRAY_BUFFER_ARB, size, 0, GL_STATIC_DRAW_ARB );
			glBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );
		}
		else {
			allocated->pointer = MEM_mallocN(size, "GPU_buffer_alloc_vertexarray");
			while( allocated->pointer == 0 && pool->size > 0 ) {
				GPU_buffer_pool_delete_last(pool);
				allocated->pointer = MEM_mallocN(size, "GPU_buffer_alloc_vertexarray");
			}
			if( allocated->pointer == 0 && pool->size == 0 ) {
				return 0;
			}
		}
	}
	else {
		sprintf(buffer,"free buffer found. Wasted %d bytes\n", pool->buffers[bestfit]->size-size);
		DEBUG_VBO(buffer);

		allocated = pool->buffers[bestfit];
		GPU_buffer_pool_remove(bestfit,pool);
	}
	return allocated;
}

void GPU_buffer_free( GPUBuffer *buffer, GPUBufferPool *pool )
{
	int i;

	DEBUG_VBO("GPU_buffer_free\n");

	if( buffer == 0 )
		return;
	if( pool == 0 )
		pool = globalPool;
	if( pool == 0 )
		globalPool = GPU_buffer_pool_new();

	/* free the last used buffer in the queue if no more space, but only
	   if we are in the main thread. for e.g. rendering or baking it can
	   happen that we are in other thread and can't call OpenGL, in that
	   case cleanup will be done GPU_buffer_pool_free_unused */
	if( BLI_thread_is_main() ) {
		while( pool->size >= MAX_FREE_GPU_BUFFERS )
			GPU_buffer_pool_delete_last( pool );
	}
	else {
		if( pool->maxsize == pool->size ) {
			pool->maxsize += MAX_FREE_GPU_BUFFERS;
			pool->buffers = MEM_reallocN(pool->buffers, sizeof(GPUBuffer*)*pool->maxsize);
		}
	}

	for( i =pool->size; i > 0; i-- ) {
		pool->buffers[i] = pool->buffers[i-1];
	}
	pool->buffers[0] = buffer;
	pool->size++;
}

GPUDrawObject *GPU_drawobject_new( DerivedMesh *dm )
{
	GPUDrawObject *object;
	MVert *mvert;
	MFace *mface;
	int numverts[32768];	/* material number is an 16-bit short so there's at most 32768 materials */
	int redir[32768];		/* material number is an 16-bit short so there's at most 32768 materials */
	int *index;
	int i;
	int curmat, curverts, numfaces;

	DEBUG_VBO("GPU_drawobject_new\n");

	object = MEM_callocN(sizeof(GPUDrawObject),"GPU_drawobject_new_object");
	object->nindices = dm->getNumVerts(dm);
	object->indices = MEM_mallocN(sizeof(IndexLink)*object->nindices, "GPU_drawobject_new_indices");
	object->nedges = dm->getNumEdges(dm);

	for( i = 0; i < object->nindices; i++ ) {
		object->indices[i].element = -1;
		object->indices[i].next = 0;
	}
	/*object->legacy = 1;*/
	memset(numverts,0,sizeof(int)*32768);

	mvert = dm->getVertArray(dm);
	mface = dm->getFaceArray(dm);

	numfaces= dm->getNumFaces(dm);
	for( i=0; i < numfaces; i++ ) {
		if( mface[i].v4 )
			numverts[mface[i].mat_nr+16383] += 6;	/* split every quad into two triangles */
		else
			numverts[mface[i].mat_nr+16383] += 3;
	}

	for( i = 0; i < 32768; i++ ) {
		if( numverts[i] > 0 ) {
			object->nmaterials++;
			object->nelements += numverts[i];
		}
	}
	object->materials = MEM_mallocN(sizeof(GPUBufferMaterial)*object->nmaterials,"GPU_drawobject_new_materials");
	index = MEM_mallocN(sizeof(int)*object->nmaterials,"GPU_drawobject_new_index");

	curmat = curverts = 0;
	for( i = 0; i < 32768; i++ ) {
		if( numverts[i] > 0 ) {
			object->materials[curmat].mat_nr = i-16383;
			object->materials[curmat].start = curverts;
			index[curmat] = curverts/3;
			object->materials[curmat].end = curverts+numverts[i];
			curverts += numverts[i];
			curmat++;
		}
	}
	object->faceRemap = MEM_mallocN(sizeof(int)*object->nelements/3,"GPU_drawobject_new_faceRemap");
	for( i = 0; i < object->nmaterials; i++ ) {
		redir[object->materials[i].mat_nr+16383] = i;	/* material number -> material index */
	}

	object->indexMem = MEM_callocN(sizeof(IndexLink)*object->nelements,"GPU_drawobject_new_indexMem");
	object->indexMemUsage = 0;

#define ADDLINK( INDEX, ACTUAL ) \
		if( object->indices[INDEX].element == -1 ) { \
			object->indices[INDEX].element = ACTUAL; \
		} else { \
			IndexLink *lnk = &object->indices[INDEX]; \
			while( lnk->next != 0 ) lnk = lnk->next; \
			lnk->next = &object->indexMem[object->indexMemUsage]; \
			lnk->next->element = ACTUAL; \
			object->indexMemUsage++; \
		}

	for( i=0; i < numfaces; i++ ) {
		int curInd = index[redir[mface[i].mat_nr+16383]];
		object->faceRemap[curInd] = i; 
		ADDLINK( mface[i].v1, curInd*3 );
		ADDLINK( mface[i].v2, curInd*3+1 );
		ADDLINK( mface[i].v3, curInd*3+2 );
		if( mface[i].v4 ) {
			object->faceRemap[curInd+1] = i;
			ADDLINK( mface[i].v3, curInd*3+3 );
			ADDLINK( mface[i].v4, curInd*3+4 );
			ADDLINK( mface[i].v1, curInd*3+5 );

			index[redir[mface[i].mat_nr+16383]]+=2;
		}
		else {
			index[redir[mface[i].mat_nr+16383]]++;
		}
	}

	for( i = 0; i < object->nindices; i++ ) {
		if( object->indices[i].element == -1 ) {
			object->indices[i].element = object->nelements + object->nlooseverts;
			object->nlooseverts++;
		}
	}
#undef ADDLINK

	MEM_freeN(index);
	return object;
}

void GPU_drawobject_free( DerivedMesh *dm )
{
	GPUDrawObject *object;

	DEBUG_VBO("GPU_drawobject_free\n");

	if( dm == 0 )
		return;
	object = dm->drawObject;
	if( object == 0 )
		return;

	MEM_freeN(object->materials);
	MEM_freeN(object->faceRemap);
	MEM_freeN(object->indices);
	MEM_freeN(object->indexMem);
	GPU_buffer_free( object->vertices, globalPool );
	GPU_buffer_free( object->normals, globalPool );
	GPU_buffer_free( object->uv, globalPool );
	GPU_buffer_free( object->colors, globalPool );
	GPU_buffer_free( object->edges, globalPool );
	GPU_buffer_free( object->uvedges, globalPool );

	MEM_freeN(object);
	dm->drawObject = 0;
}

/* XXX: don't merge this to trunk

   I'm having graphics problems with GL_SHORT normals;
   this is on experimental drivers that I'm sure not
   very many other people are using, so not worth really
   fixing.
*/
#define VBO_FLOATS 1

/* Convenience struct for building the VBO. */
typedef struct {
	float co[3];
#ifdef VBO_FLOATS
	float no[3];
#else
	short no[3];
#endif
} VertexBufferFormat;

struct GPU_Buffers {
	/* opengl buffer handles */
	GLuint vert_buf, index_buf, color_buf, uv_buf;
	GLenum index_type;

	GLuint *ptex;
	int totptex;

	int use_grids;
	unsigned int tot_tri;
	int gridsize;
};

static void delete_buffer(GLuint *buf)
{
	glDeleteBuffersARB(1, buf);
	*buf = 0;
}

static void gpu_colors_from_floats(unsigned char out[3],
				   float fcol[3],
				   float mask_strength)
{
	CLAMP(mask_strength, 0, 1);
	/* avoid making the mask output completely black */
	mask_strength = (1 - mask_strength) * 0.75 + 0.25;

	out[0] = fcol[0] * mask_strength * 255;
	out[1] = fcol[1] * mask_strength * 255;
	out[2] = fcol[2] * mask_strength * 255;
}

/* create or destroy a buffer as needed, return a pointer to the buffer data.
   if the return value is not null, it must be freed with glUnmapBuffer */
static void *map_buffer(GPU_Buffers *buffers, GLuint *id, int needed, int totelem, int elemsize)
{
	void *data = NULL;

	if(needed && !(*id))
		glGenBuffersARB(1, id);
	else if(!needed && (*id))
		delete_buffer(id);

	if(needed && (*id)) {
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, *id);
		glBufferDataARB(GL_ARRAY_BUFFER_ARB,
				elemsize * totelem,
				NULL, GL_STATIC_DRAW_ARB);
		data = glMapBufferARB(GL_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB);
		if(!data)
			delete_buffer(id);
	}

	return data;
}

static unsigned char *map_color_buffer(GPU_Buffers *buffers, int have_colors, int totelem)
{
	return map_buffer(buffers, &buffers->color_buf, have_colors, totelem, sizeof(char) * 3);
}

static void *map_uv_buffer(GPU_Buffers *buffers, int need_uvs, int totelem)
{
	return map_buffer(buffers, &buffers->uv_buf, need_uvs, totelem, sizeof(float) * 2);
}

static void color_from_face_corner(CustomData *fdata, int mcol_first_layer,
				   int mcol_totlayer, int cndx, float v[3])
{
	int i;

	v[0] = v[1] = v[2] = 1;
	
	for(i = mcol_first_layer; i < mcol_first_layer+mcol_totlayer; ++i) {
		MCol *mcol;
		float col[3];

		mcol = fdata->layers[i].data;
		mcol += cndx;

		col[0] = mcol->b / 255.0f;
		col[1] = mcol->g / 255.0f;
		col[2] = mcol->r / 255.0f;

		interp_v3_v3v3(v, v, col,
			       mcol->a / 255.0f);
	}
}

void GPU_update_mesh_color_buffers(GPU_Buffers *buffers, PBVH *bvh,
				   PBVHNode *node, DMDrawFlags flags)
{
	unsigned char *color_data;
	CustomData *vdata, *fdata;
	MFace *mface;
	int totvert, *vert_indices;
	int totface, *face_indices, *face_vert_indices;
	int mcol_totlayer, pmask_totlayer;
	int color_needed;

	if(!buffers->vert_buf)
		return;

	BLI_pbvh_get_customdata(bvh, &vdata, &fdata);
	BLI_pbvh_node_num_verts(bvh, node, NULL, &totvert);
	BLI_pbvh_node_get_verts(bvh, node, &vert_indices, NULL);
	BLI_pbvh_node_get_faces(bvh, node, &mface, &face_indices,
				&face_vert_indices, &totface);
	
	mcol_totlayer = CustomData_number_of_layers(fdata, CD_MCOL);
	pmask_totlayer = CustomData_number_of_layers(vdata, CD_PAINTMASK);

	/* avoid creating color buffer if not needed */
	color_needed = (flags & DM_DRAW_PAINT_MASK) && pmask_totlayer;

	/* Make a color buffer if there's a mask layer and
	   get rid of any color buffer if there's no mask layer */
	color_data = map_color_buffer(buffers, color_needed, totvert);

	if(color_data) {
		int i, j, mcol_first_layer, pmask_first_layer;

		mcol_first_layer = CustomData_get_layer_index(fdata, CD_MCOL);
		pmask_first_layer = CustomData_get_layer_index(vdata, CD_PAINTMASK);


		for(i = 0; i < totface; ++i) {
			int face_index = face_indices[i];
			int S = mface[face_index].v4 ? 4 : 3;

			/* for now this arbitrarily chooses one face's corner's
			   mcol to be assigned to a vertex; alternatives would
			   be to combine multiple colors through averaging or
			   draw separate quads so that mcols can abruptly
			   transition from one face to another */
			for(j = 0; j < S; ++j) {
				int node_vert_index = face_vert_indices[i*4 + j];
				float col[3], mask;

				color_from_face_corner(fdata,
						       mcol_first_layer,
						       mcol_totlayer,
						       face_index*4+j, col);

				mask = paint_mask_from_vertex(vdata,
							      vert_indices[node_vert_index],
							      pmask_totlayer,
							      pmask_first_layer);

				gpu_colors_from_floats(color_data +
						       node_vert_index*3,
						       col, mask);
			}
		}

		glUnmapBufferARB(GL_ARRAY_BUFFER_ARB);
	}
}

void GPU_update_mesh_vert_buffers(GPU_Buffers *buffers, MVert *mvert,
				  int *vert_indices, int totvert)
{
	VertexBufferFormat *vert_data;
	int i;

	if(buffers->vert_buf) {
		/* Build VBO */
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, buffers->vert_buf);
		glBufferDataARB(GL_ARRAY_BUFFER_ARB,
				 sizeof(VertexBufferFormat) * totvert,
				 NULL, GL_STATIC_DRAW_ARB);
		vert_data = glMapBufferARB(GL_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB);

		if(vert_data) {
			for(i = 0; i < totvert; ++i) {
				MVert *v = mvert + vert_indices[i];
				VertexBufferFormat *out = vert_data + i;

				copy_v3_v3(out->co, v->co);
#ifdef VBO_FLOATS
				normal_short_to_float_v3(out->no, v->no);
#else
				memcpy(out->no, v->no, sizeof(short) * 3);
#endif
			}

			glUnmapBufferARB(GL_ARRAY_BUFFER_ARB);
		}
		else
			delete_buffer(&buffers->vert_buf);

		glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
	}
}

GPU_Buffers *GPU_build_mesh_buffers(GHash *map, MVert *mvert, MFace *mface,
				    CustomData *vdata, CustomData *fdata,
				    int *face_indices, int totface,
				    int *vert_indices, int tot_uniq_verts,
				    int totvert)
{
	GPU_Buffers *buffers;
	unsigned short *tri_data;
	int i, j, k, tottri;

	buffers = MEM_callocN(sizeof(GPU_Buffers), "GPU_Buffers");
	buffers->index_type = GL_UNSIGNED_SHORT;

	/* Count the number of triangles */
	for(i = 0, tottri = 0; i < totface; ++i)
		tottri += mface[face_indices[i]].v4 ? 2 : 1;
	
	if(GL_ARB_vertex_buffer_object && !(U.gameflags & USER_DISABLE_VBO))
		glGenBuffersARB(1, &buffers->index_buf);

	if(buffers->index_buf) {
		/* Generate index buffer object */
		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, buffers->index_buf);
		glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB,
				 sizeof(unsigned short) * tottri * 3, NULL, GL_STATIC_DRAW_ARB);

		/* Fill the triangle buffer */
		tri_data = glMapBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB);
		if(tri_data) {
			for(i = 0; i < totface; ++i) {
				MFace *f = mface + face_indices[i];
				int v[3] = {f->v1, f->v2, f->v3};

				for(j = 0; j < (f->v4 ? 2 : 1); ++j) {
					for(k = 0; k < 3; ++k) {
						void *value, *key = SET_INT_IN_POINTER(v[k]);
						int vbo_index;

						value = BLI_ghash_lookup(map, key);
						vbo_index = GET_INT_FROM_POINTER(value);

						if(vbo_index < 0) {
							vbo_index = -vbo_index +
								tot_uniq_verts - 1;
						}

						*tri_data = vbo_index;
						++tri_data;
					}
					v[0] = f->v4;
					v[1] = f->v1;
					v[2] = f->v3;
				}
			}
			glUnmapBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB);
		}
		else
			delete_buffer(&buffers->index_buf);

		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
	}

	if(buffers->index_buf)
		glGenBuffersARB(1, &buffers->vert_buf);

	buffers->tot_tri = tottri;

	return buffers;
}

static void color_from_gridelem(DMGridData *elem, GridKey *gridkey, float col[3])
{
	int i;

	col[0] = col[1] = col[2] = 1;

	/* combine colors */
	for(i = 0; i < gridkey->color; ++i) {
		float *c = GRIDELEM_COLOR(elem, gridkey)[i];

		/* TODO: check layer enabled/strength */

		/* for now we just combine layers in order
		   interpolating using the alpha component
		   ("order" is ill-defined here since we
		   don't guarantee the order of cdm data) */
		interp_v3_v3v3(col, col, c, c[3]);
	}
}

void GPU_update_grid_color_buffers(GPU_Buffers *buffers, DMGridData **grids, int *grid_indices,
				   int totgrid, int gridsize, GridKey *gridkey, CustomData *vdata,
				   DMDrawFlags flags)
{
	unsigned char *color_data;
	int totvert;
	int color_needed;

	if(!buffers->vert_buf)
		return;

	/* avoid creating color buffer if not needed */
	color_needed = (flags & DM_DRAW_PAINT_MASK) && gridkey->mask;

	totvert= gridsize*gridsize*totgrid;
	color_data= map_color_buffer(buffers, color_needed, totvert);

	if(color_data) {
		int i, j;

		for(i = 0; i < totgrid; ++i) {
			DMGridData *grid= grids[grid_indices[i]];

			for(j = 0; j < gridsize*gridsize; ++j, color_data += 3) {
				DMGridData *elem = GRIDELEM_AT(grid, j, gridkey);
				float col[3], mask;

				color_from_gridelem(elem, gridkey, col);
				mask = paint_mask_from_gridelem(elem, gridkey, vdata);

				gpu_colors_from_floats(color_data, col, mask);
			}
		}

		glUnmapBufferARB(GL_ARRAY_BUFFER_ARB);
	}
}

void GPU_update_grid_uv_buffer(GPU_Buffers *buffers, PBVH *pbvh, PBVHNode *node, DMDrawFlags flags)
{
	CustomData *fdata;
	GridToFace *grid_face_map;
	MPtex *mptex;
	float (*uv_data)[2];
	int *grid_indices, totgrid, gridsize, totvert;

	if(!buffers->vert_buf)
		return;

	BLI_pbvh_get_customdata(pbvh, NULL, &fdata);
	mptex = CustomData_get_layer(fdata, CD_MPTEX);
	grid_face_map = BLI_pbvh_get_grid_face_map(pbvh);
	BLI_pbvh_node_get_grids(pbvh, node,
				&grid_indices, &totgrid, NULL, &gridsize,
				NULL, NULL, NULL);

	/* for now, pbvh is required to give one node per subface in ptex mode */
	assert(totgrid == 1);

	totvert= gridsize*gridsize*totgrid;
	uv_data= map_uv_buffer(buffers, (flags & DM_DRAW_PTEX), totvert);

	if(uv_data) {
		GridToFace *gtf = &grid_face_map[grid_indices[0]];
		MPtex *pt = &mptex[gtf->face];
		MPtexSubface *subface = &pt->subfaces[gtf->offset];
		float u, v, ustep, vstep, vstart = 0;
		int x, y;

		if(flags & DM_DRAW_PTEX_TEXELS) {
			ustep = subface->res[0] >> 1;
			vstep = subface->res[1] >> 1;
			/* make quad texel pattern appear uniform across all four subfaces */
			if(gtf->offset % 2)
				vstart = 0.5;
		}
		else {
			ustep = 1;
			vstep = 1;
		}

		ustep /= gridsize - 1.0f;
		vstep /= gridsize - 1.0f;

		for(y = 0, v = vstart; y < gridsize; ++y, v += vstep) {
			for(x = 0, u = 0; x < gridsize; ++x, u += ustep, ++uv_data) {
				uv_data[0][0] = u;
				uv_data[0][1] = v;
			}
		}

		glUnmapBufferARB(GL_ARRAY_BUFFER_ARB);
	}
}

GLenum gl_type_from_ptex(MPtex *pt)
{
	switch(pt->type) {
	case PTEX_DT_UINT8:
		return GL_UNSIGNED_BYTE;
	case PTEX_DT_UINT16:
		return GL_UNSIGNED_SHORT;
	case PTEX_DT_FLOAT:
		return GL_FLOAT;
	default:
		return 0;
	}
}

GLenum gl_format_from_ptex(MPtex *pt)
{
	switch(pt->channels) {
	case 1:
		return GL_LUMINANCE;
	case 2:
		return GL_LUMINANCE_ALPHA;
	case 3:
		return GL_RGB;
	case 4:
		return GL_RGBA;
	default:
		return 0;
	}
}

static void gpu_create_ptex_textures(GPU_Buffers *buffers)
{
	buffers->ptex = MEM_callocN(sizeof(GLuint) * buffers->totptex, "PTex IDs");
	glGenTextures(buffers->totptex, buffers->ptex);
}

static void gpu_init_ptex_texture(GLuint id, GLenum glformat, GLenum gltype,
				  int ures, int vres, void *data)
{
	glBindTexture(GL_TEXTURE_2D, id);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTexImage2D(GL_TEXTURE_2D, 0, glformat, ures, vres,
		     0, glformat, gltype, data);
}

void GPU_update_ptex(GPU_Buffers *buffers, PBVH *pbvh, PBVHNode *node)
{
	int *grid_indices, totgrid;
	GridToFace *grid_face_map;
	CustomData *fdata;
	MPtex *mptex;
	int i;

	BLI_pbvh_get_customdata(pbvh, NULL, &fdata);
	grid_face_map = BLI_pbvh_get_grid_face_map(pbvh);

	assert(BLI_pbvh_uses_grids(pbvh));

	BLI_pbvh_node_get_grids(pbvh, node,
				&grid_indices, &totgrid,
				NULL, NULL, NULL, NULL, NULL);

	/* TODO: composite multiple layers */
	mptex = CustomData_get_layer(fdata, CD_MPTEX);
	
	/* one texture per grid
	   TODO: pack multiple textures together? */
	if(!buffers->ptex) {
		buffers->totptex = totgrid;
		gpu_create_ptex_textures(buffers);
	}
	
	for(i = 0; i < totgrid; ++i) {
		GridToFace *gtf = &grid_face_map[grid_indices[i]];
		MPtex *pt = &mptex[gtf->face];
		MPtexSubface *subface = &pt->subfaces[gtf->offset];

		gpu_init_ptex_texture(buffers->ptex[i],
				      gl_format_from_ptex(pt),
				      gl_type_from_ptex(pt),
				      subface->res[0], subface->res[1],
				      subface->data);
	}

	glBindTexture(GL_TEXTURE_2D, 0);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
}

typedef struct {
	float co[3];
	float no[3];
} GridVBO;

void GPU_update_grid_vert_buffers(GPU_Buffers *buffers, DMGridData **grids,
				  int *grid_indices, int totgrid, int gridsize, GridKey *gridkey, int smooth)
{
	GridVBO *vert_data;
	int i, j, k, totvert;

	totvert= gridsize*gridsize*totgrid;

	/* Build VBO */
	if(buffers->vert_buf) {
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, buffers->vert_buf);
		glBufferDataARB(GL_ARRAY_BUFFER_ARB,
				 sizeof(GridVBO) * totvert,
				 NULL, GL_STATIC_DRAW_ARB);
		vert_data = glMapBufferARB(GL_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB);
		if(vert_data) {
			for(i = 0; i < totgrid; ++i) {
				DMGridData *grid= grids[grid_indices[i]];
				
				for(j = 0; j < gridsize; ++j) {
					for(k = 0; k < gridsize; ++k) {
						copy_v3_v3(vert_data[k + j*gridsize].co, GRIDELEM_CO_AT(grid, k + j*gridsize, gridkey));
						copy_v3_v3(vert_data[k + j*gridsize].no, GRIDELEM_NO_AT(grid, k + j*gridsize, gridkey));
					}
				}

				if(!smooth) {
					/* for flat shading, recalc normals and set the last vertex of
					   each quad in the index buffer to have the flat normal as
					   that is what opengl will use */
					for(j = 0; j < gridsize-1; ++j) {
						for(k = 0; k < gridsize-1; ++k) {
							float norm[3];
							normal_quad_v3(norm,
								GRIDELEM_CO_AT(grid, (j+1)*gridsize + k, gridkey),
								GRIDELEM_CO_AT(grid, (j+1)*gridsize + k+1, gridkey),
								GRIDELEM_CO_AT(grid, j*gridsize + k+1, gridkey),
								GRIDELEM_CO_AT(grid, j*gridsize + k, gridkey));
							copy_v3_v3(vert_data[(j+1)*gridsize + (k+1)].no, norm);
						}
					}
				}

				vert_data += gridsize*gridsize;
			}
			glUnmapBufferARB(GL_ARRAY_BUFFER_ARB);
		}
		else
			delete_buffer(&buffers->vert_buf);

		glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
	}

	//printf("node updated %p\n", buffers_v);
}

static int gpu_build_grid_ibo(int gridsize)
{
	GLuint index_buf;
	int totndx, use_ushorts, i, j;
	unsigned short *quads_ushort;
	unsigned int *quads_uint;

	/* count the number of quads */
	totndx = (gridsize-1)*(gridsize-1) * 4;

	/* generate index buffer object */
	if(GL_ARB_vertex_buffer_object && !(U.gameflags & USER_DISABLE_VBO))
		glGenBuffersARB(1, &index_buf);

	/* bad failure */
	if(!index_buf)
		return 0;

	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, index_buf);

	/* if possible, restrict indices to unsigned shorts to save space */
	use_ushorts = totndx < USHRT_MAX;

	/* allocate empty buffer data */
	glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB,
			(use_ushorts ? sizeof(unsigned short) :
			               sizeof(unsigned int)) * totndx,
			NULL, GL_STATIC_DRAW_ARB);

	/* map the buffer into memory */
	quads_ushort = (void*)(quads_uint =
			       glMapBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,
					      GL_WRITE_ONLY_ARB));
	if(!quads_ushort) {
		delete_buffer(&index_buf);
		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
		return 0;
	}

	/* fill the quad buffer */
	for(i = 0; i < gridsize-1; ++i) {
		for(j = 0; j < gridsize-1; ++j) {
#define IBO_ASSIGN(val) do {						\
				if(use_ushorts)				\
					*(quads_ushort++) = val;	\
				else					\
					*(quads_uint++) = val;		\
		} while(0)
			
			IBO_ASSIGN(i*gridsize + j+1);
			IBO_ASSIGN(i*gridsize + j);
			IBO_ASSIGN((i+1)*gridsize + j);
			IBO_ASSIGN((i+1)*gridsize + j+1);

#undef IBO_ASSIGN
		}
	}

	glUnmapBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB);
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);

	return index_buf;
}

/* cache grid IBOs, uses reference counting to free them */
static int gpu_grid_ibo(int gridsize, int release)
{
	/* {reference count, buffer id} */
	static unsigned int grid_buffers[32][2], inited = 0;
	int lvl, i;

	if(!inited) {
		memset(grid_buffers, 0, sizeof(int)*32*2);
		inited = 1;
	}

	for(i = 0, --gridsize; i < 32; ++i) {
		if(gridsize & (1 << i)) {
			lvl = i;
			break;
		}
	}

	if(release) {
		if(grid_buffers[lvl][0] > 0) {
			--grid_buffers[lvl][0];
			if(!grid_buffers[lvl][0])
				delete_buffer(&grid_buffers[lvl][1]);
		}
		else
			fprintf(stderr, "gpu_grid_ibo: bad reference count\n");
	}
	else {
		++grid_buffers[lvl][0];

		if(!grid_buffers[lvl][1])
			grid_buffers[lvl][1] = gpu_build_grid_ibo(gridsize + 1);
	}

	return grid_buffers[lvl][1];
}

GPU_Buffers *GPU_build_grid_buffers(int gridsize)
{
	GPU_Buffers *buffers;

	buffers = MEM_callocN(sizeof(GPU_Buffers), "GPU_Buffers");

	buffers->index_buf = gpu_grid_ibo(gridsize, 0);
	buffers->gridsize = gridsize;
	buffers->use_grids = 1;

	/* Build VBO */
	if(buffers->index_buf)
		glGenBuffersARB(1, &buffers->vert_buf);

	return buffers;
}

/* create a global texture for visualizing ptex texels */
static void gpu_bind_ptex_pattern()
{
	static int inited = 0;
	static GLuint ptex_pattern_gltex = 0;

	if(!inited) {
		#define color1 64, 255, 255
		#define color2 255, 128, 255
		unsigned char pattern[2*2*3] = {
			color1, color2,
			color2, color1
		};
		unsigned char avg[3] = {160, 192, 255};

		glGenTextures(1, &ptex_pattern_gltex);

		glBindTexture(GL_TEXTURE_2D, ptex_pattern_gltex);

		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 2, 2,
			     0, GL_RGB, GL_UNSIGNED_BYTE, pattern);

		glTexImage2D(GL_TEXTURE_2D, 1, GL_RGB, 1, 1,
			     0, GL_RGB, GL_UNSIGNED_BYTE, avg);

		inited = 1;
	}

	glBindTexture(GL_TEXTURE_2D, ptex_pattern_gltex);
}

static void gpu_draw_node_without_vb(GPU_Buffers *buffers, PBVH *pbvh, PBVHNode *node, DMDrawFlags flags)
{
	DMGridData **grids;
	GridKey *gridkey;
	int *grid_indices, totgrid, gridsize;
	CustomData *vdata = NULL, *fdata = NULL;
	MPtex *mptex = NULL;
	int mcol_first_layer, pmask_first_layer;
	int i, use_grids, use_color, use_ptex, ptex_edit = 0;

	use_grids = BLI_pbvh_uses_grids(pbvh);
	BLI_pbvh_get_customdata(pbvh, &vdata, &fdata);

	/* see if color data is needed */
	if(use_grids) {
		BLI_pbvh_node_get_grids(pbvh, node, &grid_indices,
					&totgrid, NULL, &gridsize,
					&grids, NULL, &gridkey);
		use_color = gridkey->color || gridkey->mask;
		if(use_color)
			BLI_pbvh_get_customdata(pbvh, &vdata, NULL);
	}
	else {
		mcol_first_layer = CustomData_get_layer_index(fdata, CD_MCOL);
		pmask_first_layer = CustomData_get_layer_index(vdata, CD_PAINTMASK);

		use_color = (flags & DM_DRAW_PAINT_MASK) && pmask_first_layer != -1;
	}
	
	if((use_ptex = (buffers->ptex && (flags & DM_DRAW_PTEX)))) {
		mptex = CustomData_get_layer(fdata, CD_MPTEX);
		glEnable(GL_TEXTURE_2D);
		ptex_edit = flags & DM_DRAW_PTEX_TEXELS;
	}

	if(use_color || use_ptex) {
		glColorMaterial(GL_FRONT_AND_BACK, GL_DIFFUSE);
		glEnable(GL_COLOR_MATERIAL);
	}
	if(use_grids) {	 
		GridToFace *grid_face_map = BLI_pbvh_get_grid_face_map(pbvh);
		int x, y;

		BLI_pbvh_node_get_grids(pbvh, node, &grid_indices,
					&totgrid, NULL, &gridsize,
					&grids, NULL, &gridkey);

		if(ptex_edit)
			gpu_bind_ptex_pattern();

		for(i = 0; i < totgrid; ++i) {
			DMGridData *grid = grids[grid_indices[i]];
			GridToFace *gtf = &grid_face_map[grid_indices[i]];
			MPtex *pt;
			MPtexSubface *subface;
			float u, v, ustep, vstep, vstart = 0;

			if(mptex) {
				pt = &mptex[gtf->face];
				subface = &pt->subfaces[gtf->offset];

				if(subface->flag & MPTEX_SUBFACE_HIDDEN)
					continue;
			}

			if(ptex_edit) {
				ustep = subface->res[0] >> 1;
				vstep = subface->res[1] >> 1;
			}
			else {
				ustep = 1;
				vstep = 1;
			}

			if(gridsize > 1) {
				ustep /= gridsize - 1;
				vstep /= gridsize - 1;
			}

			/* make quad texel pattern appear uniform across all four subfaces */
			if(ptex_edit && (gtf->offset % 2))
				vstart = 0.5;

			if(ptex_edit) {
				if(subface->flag & MPTEX_SUBFACE_SELECTED)
					glColor3ub(255, 255, 255);
				else if(subface->flag & MPTEX_SUBFACE_MASKED)
					glColor3ub(96, 96, 96);
				else	 
					glColor3ub(128, 128, 128);	
			}
			else if(use_ptex) {
				if(subface->flag & MPTEX_SUBFACE_MASKED)
					glColor3ub(128, 128, 128);
				else
					glColor3ub(255, 255, 255);
				glBindTexture(GL_TEXTURE_2D, buffers->ptex[i]);
			}

			for(y = 0, v = vstart; y < gridsize-1; y++, v += vstep) {
				glBegin(GL_QUAD_STRIP);
				for(x = 0, u = 0; x < gridsize; x++, u += ustep) {
					DMGridData *a = GRIDELEM_AT(grid, y*gridsize + x, gridkey);
					DMGridData *b = GRIDELEM_AT(grid, (y+1)*gridsize + x, gridkey);
					float acol[3], bcol[3], amask, bmask;
					unsigned char aglc[3], bglc[3];

					color_from_gridelem(a, gridkey, acol);
					color_from_gridelem(b, gridkey, bcol);
					amask = paint_mask_from_gridelem(a, gridkey, vdata);
					bmask = paint_mask_from_gridelem(b, gridkey, vdata);
					
					if(use_color) {
						gpu_colors_from_floats(aglc, acol, amask);
						gpu_colors_from_floats(bglc, bcol, bmask);
					}

					if(use_color)
						glColor3ubv(aglc);
					if(use_ptex)
						glTexCoord2f(u, v);
					glNormal3fv(GRIDELEM_NO(a, gridkey));
					glVertex3fv(GRIDELEM_CO(a, gridkey));
					if(use_color)
						glColor3ubv(bglc);
					if(use_ptex)
						glTexCoord2f(u, v + vstep);
					glNormal3fv(GRIDELEM_NO(b, gridkey));
					glVertex3fv(GRIDELEM_CO(b, gridkey));
				}
				glEnd();
			}
		}
	}
	else {
		MFace *mface;
		MVert *mvert;
		int totface, *face_indices;
		int j, mcol_totlayer, pmask_totlayer;

		BLI_pbvh_node_get_verts(pbvh, node, NULL, &mvert);
		BLI_pbvh_node_get_faces(pbvh, node, &mface, &face_indices, NULL, &totface);

		if(mcol_first_layer)
			mcol_totlayer = CustomData_number_of_layers(fdata, CD_MCOL);

		if(pmask_first_layer)
			pmask_totlayer = CustomData_number_of_layers(vdata, CD_PAINTMASK);

		for(i = 0; i < totface; ++i) {
			int face_index = face_indices[i];
			MFace *f = mface + face_index;
			int S = f->v4 ? 4 : 3;

			glBegin((f->v4)? GL_QUADS: GL_TRIANGLES);

			for(j = 0; j < S; ++j) {
				int vndx = (&f->v1)[j];
				float col[3], mask;
				unsigned char glc[3];

				color_from_face_corner(fdata,
						       mcol_first_layer,
						       mcol_totlayer,
						       face_index*4+j, col);

				mask = paint_mask_from_vertex(vdata, vndx,
							      pmask_totlayer,
							      pmask_first_layer);

				gpu_colors_from_floats(glc, col, mask);

				glColor3ubv(glc);
				glNormal3sv(mvert[vndx].no);
				glVertex3fv(mvert[vndx].co);
			}

			glEnd();
		}
	}

	if(use_color || use_ptex)
		glDisable(GL_COLOR_MATERIAL);
}

static void gpu_draw_grids(GPU_Buffers *buffers, PBVH *pbvh, PBVHNode *node, DMDrawFlags flags)
{
	int g, totgrid, *grid_indices, gridsize, offset, totndx;

	BLI_pbvh_node_get_grids(pbvh, node, &grid_indices, &totgrid,
				NULL, &gridsize, NULL, NULL, NULL);

	totndx = (gridsize-1)*(gridsize-1) * 4;

	for(g = 0; g < totgrid; ++g) {
		offset = gridsize * gridsize * g;

		glVertexPointer(3, GL_FLOAT, sizeof(GridVBO),
				(char*)0 + offset*sizeof(GridVBO) + offsetof(GridVBO, co));
		glNormalPointer(GL_FLOAT, sizeof(GridVBO),
				(char*)0 + offset*sizeof(GridVBO) + offsetof(GridVBO, no));

		if(buffers->color_buf) {
			glBindBufferARB(GL_ARRAY_BUFFER_ARB, buffers->color_buf);
			glColorPointer(3, GL_UNSIGNED_BYTE, 0, (char*)0 + (offset * 3));
		}

		if(buffers->uv_buf) {
			CustomData *fdata;
			GridToFace *gtf;
			MPtex *mptex, *pt;
			MPtexSubface *subface;
			GridToFace *grid_face_map;

			/* note: code here assumes there's only one
			   ptex subface per node */

			BLI_pbvh_get_customdata(pbvh, NULL, &fdata);
			mptex = CustomData_get_layer(fdata, CD_MPTEX);
			grid_face_map = BLI_pbvh_get_grid_face_map(pbvh);
					
			gtf = &grid_face_map[grid_indices[0]];
			pt = &mptex[gtf->face];
			subface = &pt->subfaces[gtf->offset];

			glEnable(GL_TEXTURE_2D);
			if(flags & DM_DRAW_PTEX_TEXELS) {
				gpu_bind_ptex_pattern();
				if(subface->flag & MPTEX_SUBFACE_SELECTED)
					glColor3ub(255, 255, 255);
				else if(subface->flag & MPTEX_SUBFACE_MASKED)
					glColor3ub(96, 96, 96);
				else	 
					glColor3ub(128, 128, 128);
			}
			else {
				glBindTexture(GL_TEXTURE_2D, buffers->ptex[0]);

				if(subface->flag & MPTEX_SUBFACE_MASKED)
					glColor3ub(128, 128, 128);
				else
					glColor3ub(255, 255, 255);
			}

			if(subface->flag & MPTEX_SUBFACE_HIDDEN)
				continue;

			glBindBufferARB(GL_ARRAY_BUFFER_ARB, buffers->uv_buf);
			glTexCoordPointer(2, GL_FLOAT, 0, (void*)0);
		}

		glDrawElements(GL_QUADS, totndx,
			       (totndx < USHRT_MAX ?
				GL_UNSIGNED_SHORT : GL_UNSIGNED_INT), 0);
	}
}

void GPU_draw_buffers(GPU_Buffers *buffers, PBVH *pbvh, PBVHNode *node, DMDrawFlags flags)
{
	glShadeModel((flags & DM_DRAW_FULLY_SMOOTH) ? GL_SMOOTH: GL_FLAT);

	if(buffers->vert_buf && buffers->index_buf) {
		GLboolean use_colmat, colmat;

		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_NORMAL_ARRAY);
		if(buffers->color_buf)
			glEnableClientState(GL_COLOR_ARRAY);
		if(buffers->uv_buf)
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);

		glBindBufferARB(GL_ARRAY_BUFFER_ARB, buffers->vert_buf);
		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, buffers->index_buf);

		use_colmat = buffers->color_buf || (flags & DM_DRAW_PTEX);
		if(use_colmat) {
			glColorMaterial(GL_FRONT_AND_BACK, GL_DIFFUSE);
			glGetBooleanv(GL_COLOR_MATERIAL, &colmat);
			glEnable(GL_COLOR_MATERIAL);
		}

		if(buffers->use_grids) {
			gpu_draw_grids(buffers, pbvh, node, flags);
		}
		else {
			glVertexPointer(3, GL_FLOAT, sizeof(VertexBufferFormat), (void*)offsetof(VertexBufferFormat, co));
#ifdef VBO_FLOATS			
			glNormalPointer(GL_FLOAT, sizeof(VertexBufferFormat), (void*)offsetof(VertexBufferFormat, no));
#else
			glNormalPointer(GL_SHORT, sizeof(VertexBufferFormat), (void*)offsetof(VertexBufferFormat, no));
#endif
			if(buffers->color_buf) {
				glBindBufferARB(GL_ARRAY_BUFFER_ARB, buffers->color_buf);
				glColorPointer(3, GL_UNSIGNED_BYTE, 0, (void*)0);
			}

			glDrawElements(GL_TRIANGLES, buffers->tot_tri * 3, buffers->index_type, 0);
		}

		if(use_colmat && !colmat)
			glDisable(GL_COLOR_MATERIAL);

		glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
		glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);

		glDisableClientState(GL_VERTEX_ARRAY);
		glDisableClientState(GL_NORMAL_ARRAY);
		glDisableClientState(GL_COLOR_ARRAY);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	}
	else {
		/* fallback to regular drawing if out of memory or if VBO is switched off */
		gpu_draw_node_without_vb(buffers, pbvh, node, flags);
	}

	glShadeModel(GL_FLAT);
}

void GPU_free_buffers(GPU_Buffers *buffers_v)
{
	if(buffers_v) {
		GPU_Buffers *buffers = buffers_v;
		
		if(buffers->vert_buf)
			glDeleteBuffersARB(1, &buffers->vert_buf);
		if(buffers->index_buf) {
			if(buffers->use_grids)
				gpu_grid_ibo(buffers->gridsize, 1);
			else
				glDeleteBuffersARB(1, &buffers->index_buf);
		}
		if(buffers->ptex) {
			glDeleteTextures(buffers->totptex, buffers->ptex);
			MEM_freeN(buffers->ptex);
		}

		MEM_freeN(buffers);
	}
}

GPUBuffer *GPU_buffer_setup( DerivedMesh *dm, GPUDrawObject *object, int size, GLenum target, void *user, void (*copy_f)(DerivedMesh *, float *, int *, int *, void *) )
{
	GPUBuffer *buffer;
	float *varray;
	int redir[32768];
	int *index;
	int i;
	int success;
	GLboolean uploaded;

	DEBUG_VBO("GPU_buffer_setup\n");

	if( globalPool == 0 )
		globalPool = GPU_buffer_pool_new();

	buffer = GPU_buffer_alloc(size,globalPool);
	if( buffer == 0 ) {
		dm->drawObject->legacy = 1;
	}
	if( dm->drawObject->legacy ) {
		return 0;
	}

	index = MEM_mallocN(sizeof(int)*object->nmaterials,"GPU_buffer_setup");
	for( i = 0; i < object->nmaterials; i++ ) {
		index[i] = object->materials[i].start*3;
		redir[object->materials[i].mat_nr+16383] = i;
	}

	if( useVBOs ) {
		success = 0;
		while( success == 0 ) {
			glBindBufferARB( target, buffer->id );
			glBufferDataARB( target, buffer->size, 0, GL_STATIC_DRAW_ARB );	/* discard previous data, avoid stalling gpu */
			varray = glMapBufferARB( target, GL_WRITE_ONLY_ARB );
			if( varray == 0 ) {
				DEBUG_VBO( "Failed to map buffer to client address space\n" ); 
				GPU_buffer_free( buffer, globalPool );
				GPU_buffer_pool_delete_last( globalPool );
				buffer= NULL;
				if( globalPool->size > 0 ) {
					GPU_buffer_pool_delete_last( globalPool );
					buffer = GPU_buffer_alloc( size, globalPool );
					if( buffer == 0 ) {
						dm->drawObject->legacy = 1;
						success = 1;
					}
				}
				else {
					dm->drawObject->legacy = 1;
					success = 1;
				}
			}
			else {
				success = 1;
			}
		}

		if( dm->drawObject->legacy == 0 ) {
			uploaded = GL_FALSE;
			while( !uploaded ) {
				(*copy_f)( dm, varray, index, redir, user );
				uploaded = glUnmapBufferARB( target );	/* returns false if data got corruped during transfer */
			}
		}
		glBindBufferARB(target, 0);
	}
	else {
		if( buffer->pointer != 0 ) {
			varray = buffer->pointer;
			(*copy_f)( dm, varray, index, redir, user );
		}
		else {
			dm->drawObject->legacy = 1;
		}
	}

	MEM_freeN(index);

	return buffer;
}

void GPU_buffer_copy_vertex( DerivedMesh *dm, float *varray, int *index, int *redir, void *user )
{
	int start;
	int i, j, numfaces;

	MVert *mvert;
	MFace *mface;

	DEBUG_VBO("GPU_buffer_copy_vertex\n");

	mvert = dm->getVertArray(dm);
	mface = dm->getFaceArray(dm);

	numfaces= dm->getNumFaces(dm);
	for( i=0; i < numfaces; i++ ) {
		start = index[redir[mface[i].mat_nr+16383]];
		if( mface[i].v4 )
			index[redir[mface[i].mat_nr+16383]] += 18;
		else
			index[redir[mface[i].mat_nr+16383]] += 9;

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
	j = dm->drawObject->nelements*3;
	for( i = 0; i < dm->drawObject->nindices; i++ ) {
		if( dm->drawObject->indices[i].element >= dm->drawObject->nelements ) {
			VECCOPY(&varray[j],mvert[i].co);
			j+=3;
		}
	}
}

GPUBuffer *GPU_buffer_vertex( DerivedMesh *dm )
{
	DEBUG_VBO("GPU_buffer_vertex\n");

	return GPU_buffer_setup( dm, dm->drawObject, sizeof(float)*3*(dm->drawObject->nelements+dm->drawObject->nlooseverts), GL_ARRAY_BUFFER_ARB, 0, GPU_buffer_copy_vertex);
}

void GPU_buffer_copy_normal( DerivedMesh *dm, float *varray, int *index, int *redir, void *user )
{
	int i, numfaces;
	int start;
	float norm[3];

	float *nors= dm->getFaceDataArray(dm, CD_NORMAL);
	MVert *mvert = dm->getVertArray(dm);
	MFace *mface = dm->getFaceArray(dm);

	DEBUG_VBO("GPU_buffer_copy_normal\n");

	numfaces= dm->getNumFaces(dm);
	for( i=0; i < numfaces; i++ ) {
		start = index[redir[mface[i].mat_nr+16383]];
		if( mface[i].v4 )
			index[redir[mface[i].mat_nr+16383]] += 18;
		else
			index[redir[mface[i].mat_nr+16383]] += 9;

		/* v1 v2 v3 */
		if( mface[i].flag & ME_SMOOTH ) {
			VECCOPY(&varray[start],mvert[mface[i].v1].no);
			VECCOPY(&varray[start+3],mvert[mface[i].v2].no);
			VECCOPY(&varray[start+6],mvert[mface[i].v3].no);
		}
		else {
			if( nors ) {
				VECCOPY(&varray[start],&nors[i*3]);
				VECCOPY(&varray[start+3],&nors[i*3]);
				VECCOPY(&varray[start+6],&nors[i*3]);
			}
			if( mface[i].v4 )
				normal_quad_v3( norm,mvert[mface[i].v1].co, mvert[mface[i].v2].co, mvert[mface[i].v3].co, mvert[mface[i].v4].co);
			else
				normal_tri_v3( norm,mvert[mface[i].v1].co, mvert[mface[i].v2].co, mvert[mface[i].v3].co);
			VECCOPY(&varray[start],norm);
			VECCOPY(&varray[start+3],norm);
			VECCOPY(&varray[start+6],norm);
		}

		if( mface[i].v4 ) {
			/* v3 v4 v1 */
			if( mface[i].flag & ME_SMOOTH ) {
				VECCOPY(&varray[start+9],mvert[mface[i].v3].no);
				VECCOPY(&varray[start+12],mvert[mface[i].v4].no);
				VECCOPY(&varray[start+15],mvert[mface[i].v1].no);
			}
			else {
				VECCOPY(&varray[start+9],norm);
				VECCOPY(&varray[start+12],norm);
				VECCOPY(&varray[start+15],norm);
			}
		}
	}
}

GPUBuffer *GPU_buffer_normal( DerivedMesh *dm )
{
	DEBUG_VBO("GPU_buffer_normal\n");

	return GPU_buffer_setup( dm, dm->drawObject, sizeof(float)*3*dm->drawObject->nelements, GL_ARRAY_BUFFER_ARB, 0, GPU_buffer_copy_normal);
}

void GPU_buffer_copy_uv( DerivedMesh *dm, float *varray, int *index, int *redir, void *user )
{
	int start;
	int i, numfaces;

	MTFace *mtface;
	MFace *mface;

	DEBUG_VBO("GPU_buffer_copy_uv\n");

	mface = dm->getFaceArray(dm);
	mtface = DM_get_face_data_layer(dm, CD_MTFACE);

	if( mtface == 0 ) {
		DEBUG_VBO("Texture coordinates do not exist for this mesh");
		return;
	}
		
	numfaces= dm->getNumFaces(dm);
	for( i=0; i < numfaces; i++ ) {
		start = index[redir[mface[i].mat_nr+16383]];
		if( mface[i].v4 )
			index[redir[mface[i].mat_nr+16383]] += 12;
		else
			index[redir[mface[i].mat_nr+16383]] += 6;

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

GPUBuffer *GPU_buffer_uv( DerivedMesh *dm )
{
	DEBUG_VBO("GPU_buffer_uv\n");
	if( DM_get_face_data_layer(dm, CD_MTFACE) != 0 ) /* was sizeof(float)*2 but caused buffer overrun  */
		return GPU_buffer_setup( dm, dm->drawObject, sizeof(float)*3*dm->drawObject->nelements, GL_ARRAY_BUFFER_ARB, 0, GPU_buffer_copy_uv);
	else
		return 0;
}

void GPU_buffer_copy_color3( DerivedMesh *dm, float *varray_, int *index, int *redir, void *user )
{
	int i, numfaces;
	unsigned char *varray = (unsigned char *)varray_;
	unsigned char *mcol = (unsigned char *)user;
	MFace *mface = dm->getFaceArray(dm);

	DEBUG_VBO("GPU_buffer_copy_color3\n");

	numfaces= dm->getNumFaces(dm);
	for( i=0; i < numfaces; i++ ) {
		int start = index[redir[mface[i].mat_nr+16383]];
		if( mface[i].v4 )
			index[redir[mface[i].mat_nr+16383]] += 18;
		else
			index[redir[mface[i].mat_nr+16383]] += 9;

		/* v1 v2 v3 */
		VECCOPY(&varray[start],&mcol[i*12]);
		VECCOPY(&varray[start+3],&mcol[i*12+3]);
		VECCOPY(&varray[start+6],&mcol[i*12+6]);
		if( mface[i].v4 ) {
			/* v3 v4 v1 */
			VECCOPY(&varray[start+9],&mcol[i*12+6]);
			VECCOPY(&varray[start+12],&mcol[i*12+9]);
			VECCOPY(&varray[start+15],&mcol[i*12]);
		}
	}
}

void GPU_buffer_copy_color4( DerivedMesh *dm, float *varray_, int *index, int *redir, void *user )
{
	int i, numfaces;
	unsigned char *varray = (unsigned char *)varray_;
	unsigned char *mcol = (unsigned char *)user;
	MFace *mface = dm->getFaceArray(dm);

	DEBUG_VBO("GPU_buffer_copy_color4\n");

	numfaces= dm->getNumFaces(dm);
	for( i=0; i < numfaces; i++ ) {
		int start = index[redir[mface[i].mat_nr+16383]];
		if( mface[i].v4 )
			index[redir[mface[i].mat_nr+16383]] += 18;
		else
			index[redir[mface[i].mat_nr+16383]] += 9;

		/* v1 v2 v3 */
		VECCOPY(&varray[start],&mcol[i*16]);
		VECCOPY(&varray[start+3],&mcol[i*16+4]);
		VECCOPY(&varray[start+6],&mcol[i*16+8]);
		if( mface[i].v4 ) {
			/* v3 v4 v1 */
			VECCOPY(&varray[start+9],&mcol[i*16+8]);
			VECCOPY(&varray[start+12],&mcol[i*16+12]);
			VECCOPY(&varray[start+15],&mcol[i*16]);
		}
	}
}

GPUBuffer *GPU_buffer_color( DerivedMesh *dm )
{
	unsigned char *colors;
	int i, numfaces;
	MCol *mcol;
	GPUBuffer *result;
	DEBUG_VBO("GPU_buffer_color\n");

	mcol = DM_get_face_data_layer(dm, CD_ID_MCOL);
	dm->drawObject->colType = CD_ID_MCOL;
	if(!mcol) {
		mcol = DM_get_face_data_layer(dm, CD_WEIGHT_MCOL);
		dm->drawObject->colType = CD_WEIGHT_MCOL;
	}
	if(!mcol) {
		mcol = DM_get_face_data_layer(dm, CD_MCOL);
		dm->drawObject->colType = CD_MCOL;
	}

	numfaces= dm->getNumFaces(dm);
	colors = MEM_mallocN(numfaces*12*sizeof(unsigned char), "GPU_buffer_color");
	for( i=0; i < numfaces*4; i++ ) {
		colors[i*3] = mcol[i].b;
		colors[i*3+1] = mcol[i].g;
		colors[i*3+2] = mcol[i].r;
	}

	result = GPU_buffer_setup( dm, dm->drawObject, sizeof(char)*3*dm->drawObject->nelements, GL_ARRAY_BUFFER_ARB, colors, GPU_buffer_copy_color3 );

	MEM_freeN(colors);
	return result;
}

void GPU_buffer_copy_edge( DerivedMesh *dm, float *varray, int *index, int *redir, void *user )
{
	int i;

	MVert *mvert;
	MEdge *medge;
	unsigned int *varray_ = (unsigned int *)varray;
	int numedges;
 
	DEBUG_VBO("GPU_buffer_copy_edge\n");

	mvert = dm->getVertArray(dm);
	medge = dm->getEdgeArray(dm);

	numedges= dm->getNumEdges(dm);
	for(i = 0; i < numedges; i++) {
		varray_[i*2] = (unsigned int)dm->drawObject->indices[medge[i].v1].element;
		varray_[i*2+1] = (unsigned int)dm->drawObject->indices[medge[i].v2].element;
	}
}

GPUBuffer *GPU_buffer_edge( DerivedMesh *dm )
{
	DEBUG_VBO("GPU_buffer_edge\n");

	return GPU_buffer_setup( dm, dm->drawObject, sizeof(int)*2*dm->drawObject->nedges, GL_ELEMENT_ARRAY_BUFFER_ARB, 0, GPU_buffer_copy_edge);
}

void GPU_buffer_copy_uvedge( DerivedMesh *dm, float *varray, int *index, int *redir, void *user )
{
	MTFace *tf = DM_get_face_data_layer(dm, CD_MTFACE);
	int i, j=0;

	DEBUG_VBO("GPU_buffer_copy_uvedge\n");

	if(tf) {
		for(i = 0; i < dm->numFaceData; i++, tf++) {
			MFace mf;
			dm->getFace(dm,i,&mf);

			VECCOPY2D(&varray[j],tf->uv[0]);
			VECCOPY2D(&varray[j+2],tf->uv[1]);

			VECCOPY2D(&varray[j+4],tf->uv[1]);
			VECCOPY2D(&varray[j+6],tf->uv[2]);

			if(!mf.v4) {
				VECCOPY2D(&varray[j+8],tf->uv[2]);
				VECCOPY2D(&varray[j+10],tf->uv[0]);
				j+=12;
			} else {
				VECCOPY2D(&varray[j+8],tf->uv[2]);
				VECCOPY2D(&varray[j+10],tf->uv[3]);

				VECCOPY2D(&varray[j+12],tf->uv[3]);
				VECCOPY2D(&varray[j+14],tf->uv[0]);
				j+=16;
			}
		}
	}
	else {
		DEBUG_VBO("Could not get MTFACE data layer");
	}
}

GPUBuffer *GPU_buffer_uvedge( DerivedMesh *dm )
{
	DEBUG_VBO("GPU_buffer_uvedge\n");

	return GPU_buffer_setup( dm, dm->drawObject, sizeof(float)*2*(dm->drawObject->nelements/3)*2, GL_ARRAY_BUFFER_ARB, 0, GPU_buffer_copy_uvedge);
}


void GPU_vertex_setup( DerivedMesh *dm )
{
	DEBUG_VBO("GPU_vertex_setup\n");
	if( dm->drawObject == 0 )
		dm->drawObject = GPU_drawobject_new( dm );
	if( dm->drawObject->vertices == 0 )
		dm->drawObject->vertices = GPU_buffer_vertex( dm );
	if( dm->drawObject->vertices == 0 ) {
		DEBUG_VBO( "Failed to setup vertices\n" );
		return;
	}

	glEnableClientState( GL_VERTEX_ARRAY );
	if( useVBOs ) {
		glBindBufferARB( GL_ARRAY_BUFFER_ARB, dm->drawObject->vertices->id );
		glVertexPointer( 3, GL_FLOAT, 0, 0 );
	}
	else {
		glVertexPointer( 3, GL_FLOAT, 0, dm->drawObject->vertices->pointer );
	}
	
	GLStates |= GPU_BUFFER_VERTEX_STATE;
}

void GPU_normal_setup( DerivedMesh *dm )
{
	DEBUG_VBO("GPU_normal_setup\n");
	if( dm->drawObject == 0 )
		dm->drawObject = GPU_drawobject_new( dm );
	if( dm->drawObject->normals == 0 )
		dm->drawObject->normals = GPU_buffer_normal( dm );
	if( dm->drawObject->normals == 0 ) {
		DEBUG_VBO( "Failed to setup normals\n" );
		return;
	}
	glEnableClientState( GL_NORMAL_ARRAY );
	if( useVBOs ) {
		glBindBufferARB( GL_ARRAY_BUFFER_ARB, dm->drawObject->normals->id );
		glNormalPointer( GL_FLOAT, 0, 0 );
	}
	else {
		glNormalPointer( GL_FLOAT, 0, dm->drawObject->normals->pointer );
	}

	GLStates |= GPU_BUFFER_NORMAL_STATE;
}

void GPU_uv_setup( DerivedMesh *dm )
{
	DEBUG_VBO("GPU_uv_setup\n");
	if( dm->drawObject == 0 )
		dm->drawObject = GPU_drawobject_new( dm );
	if( dm->drawObject->uv == 0 )
		dm->drawObject->uv = GPU_buffer_uv( dm );
	
	if( dm->drawObject->uv != 0 ) {
		glEnableClientState( GL_TEXTURE_COORD_ARRAY );
		if( useVBOs ) {
			glBindBufferARB( GL_ARRAY_BUFFER_ARB, dm->drawObject->uv->id );
			glTexCoordPointer( 2, GL_FLOAT, 0, 0 );
		}
		else {
			glTexCoordPointer( 2, GL_FLOAT, 0, dm->drawObject->uv->pointer );
		}

		GLStates |= GPU_BUFFER_TEXCOORD_STATE;
	}
}

void GPU_color_setup( DerivedMesh *dm )
{
	DEBUG_VBO("GPU_color_setup\n");
	if( dm->drawObject == 0 )
		dm->drawObject = GPU_drawobject_new( dm );
	if( dm->drawObject->colors == 0 )
		dm->drawObject->colors = GPU_buffer_color( dm );
	if( dm->drawObject->colors == 0 ) {
		DEBUG_VBO( "Failed to setup colors\n" );
		return;
	}
	glEnableClientState( GL_COLOR_ARRAY );
	if( useVBOs ) {
		glBindBufferARB( GL_ARRAY_BUFFER_ARB, dm->drawObject->colors->id );
		glColorPointer( 3, GL_UNSIGNED_BYTE, 0, 0 );
	}
	else {
		glColorPointer( 3, GL_UNSIGNED_BYTE, 0, dm->drawObject->colors->pointer );
	}

	GLStates |= GPU_BUFFER_COLOR_STATE;
}

void GPU_edge_setup( DerivedMesh *dm )
{
	DEBUG_VBO("GPU_edge_setup\n");
	if( dm->drawObject == 0 )
		dm->drawObject = GPU_drawobject_new( dm );
	if( dm->drawObject->edges == 0 )
		dm->drawObject->edges = GPU_buffer_edge( dm );
	if( dm->drawObject->edges == 0 ) {
		DEBUG_VBO( "Failed to setup edges\n" );
		return;
	}
	if( dm->drawObject->vertices == 0 )
		dm->drawObject->vertices = GPU_buffer_vertex( dm );
	if( dm->drawObject->vertices == 0 ) {
		DEBUG_VBO( "Failed to setup vertices\n" );
		return;
	}

	glEnableClientState( GL_VERTEX_ARRAY );
	if( useVBOs ) {
		glBindBufferARB( GL_ARRAY_BUFFER_ARB, dm->drawObject->vertices->id );
		glVertexPointer( 3, GL_FLOAT, 0, 0 );
	}
	else {
		glVertexPointer( 3, GL_FLOAT, 0, dm->drawObject->vertices->pointer );
	}
	
	GLStates |= GPU_BUFFER_VERTEX_STATE;

	if( useVBOs ) {
		glBindBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, dm->drawObject->edges->id );
	}

	GLStates |= GPU_BUFFER_ELEMENT_STATE;
}

void GPU_uvedge_setup( DerivedMesh *dm )
{
	DEBUG_VBO("GPU_uvedge_setup\n");
	if( dm->drawObject == 0 )
		dm->drawObject = GPU_drawobject_new( dm );
	if( dm->drawObject->uvedges == 0 )
		dm->drawObject->uvedges = GPU_buffer_uvedge( dm );
	if( dm->drawObject->uvedges == 0 ) {
		DEBUG_VBO( "Failed to setup UV edges\n" );
		return;
	}

	glEnableClientState( GL_VERTEX_ARRAY );
	if( useVBOs ) {
		glBindBufferARB( GL_ARRAY_BUFFER_ARB, dm->drawObject->uvedges->id );
		glVertexPointer( 2, GL_FLOAT, 0, 0 );
	}
	else {
		glVertexPointer( 2, GL_FLOAT, 0, dm->drawObject->uvedges->pointer );
	}
	
	GLStates |= GPU_BUFFER_VERTEX_STATE;
}

void GPU_interleaved_setup( GPUBuffer *buffer, int data[] ) {
	int i;
	int elementsize = 0;
	intptr_t offset = 0;

	DEBUG_VBO("GPU_interleaved_setup\n");

	for( i = 0; data[i] != GPU_BUFFER_INTER_END; i++ ) {
		switch( data[i] ) {
			case GPU_BUFFER_INTER_V3F:
				elementsize += 3*sizeof(float);
				break;
			case GPU_BUFFER_INTER_N3F:
				elementsize += 3*sizeof(float);
				break;
			case GPU_BUFFER_INTER_T2F:
				elementsize += 2*sizeof(float);
				break;
			case GPU_BUFFER_INTER_C3UB:
				elementsize += 3*sizeof(unsigned char);
				break;
			case GPU_BUFFER_INTER_C4UB:
				elementsize += 4*sizeof(unsigned char);
				break;
			default:
				DEBUG_VBO( "Unknown element in data type array in GPU_interleaved_setup\n" );
		}
	}

	if( useVBOs ) {
		glBindBufferARB( GL_ARRAY_BUFFER_ARB, buffer->id );
		for( i = 0; data[i] != GPU_BUFFER_INTER_END; i++ ) {
			switch( data[i] ) {
				case GPU_BUFFER_INTER_V3F:
					glEnableClientState( GL_VERTEX_ARRAY );
					glVertexPointer( 3, GL_FLOAT, elementsize, (void *)offset );
					GLStates |= GPU_BUFFER_VERTEX_STATE;
					offset += 3*sizeof(float);
					break;
				case GPU_BUFFER_INTER_N3F:
					glEnableClientState( GL_NORMAL_ARRAY );
					glNormalPointer( GL_FLOAT, elementsize, (void *)offset );
					GLStates |= GPU_BUFFER_NORMAL_STATE;
					offset += 3*sizeof(float);
					break;
				case GPU_BUFFER_INTER_T2F:
					glEnableClientState( GL_TEXTURE_COORD_ARRAY );
					glTexCoordPointer( 2, GL_FLOAT, elementsize, (void *)offset );
					GLStates |= GPU_BUFFER_TEXCOORD_STATE;
					offset += 2*sizeof(float);
					break;
				case GPU_BUFFER_INTER_C3UB:
					glEnableClientState( GL_COLOR_ARRAY );
					glColorPointer( 3, GL_UNSIGNED_BYTE, elementsize, (void *)offset );
					GLStates |= GPU_BUFFER_COLOR_STATE;
					offset += 3*sizeof(unsigned char);
					break;
				case GPU_BUFFER_INTER_C4UB:
					glEnableClientState( GL_COLOR_ARRAY );
					glColorPointer( 4, GL_UNSIGNED_BYTE, elementsize, (void *)offset );
					GLStates |= GPU_BUFFER_COLOR_STATE;
					offset += 4*sizeof(unsigned char);
					break;
			}
		}
	}
	else {
		for( i = 0; data[i] != GPU_BUFFER_INTER_END; i++ ) {
			switch( data[i] ) {
				case GPU_BUFFER_INTER_V3F:
					glEnableClientState( GL_VERTEX_ARRAY );
					glVertexPointer( 3, GL_FLOAT, elementsize, offset+(char *)buffer->pointer );
					GLStates |= GPU_BUFFER_VERTEX_STATE;
					offset += 3*sizeof(float);
					break;
				case GPU_BUFFER_INTER_N3F:
					glEnableClientState( GL_NORMAL_ARRAY );
					glNormalPointer( GL_FLOAT, elementsize, offset+(char *)buffer->pointer );
					GLStates |= GPU_BUFFER_NORMAL_STATE;
					offset += 3*sizeof(float);
					break;
				case GPU_BUFFER_INTER_T2F:
					glEnableClientState( GL_TEXTURE_COORD_ARRAY );
					glTexCoordPointer( 2, GL_FLOAT, elementsize, offset+(char *)buffer->pointer );
					GLStates |= GPU_BUFFER_TEXCOORD_STATE;
					offset += 2*sizeof(float);
					break;
				case GPU_BUFFER_INTER_C3UB:
					glEnableClientState( GL_COLOR_ARRAY );
					glColorPointer( 3, GL_UNSIGNED_BYTE, elementsize, offset+(char *)buffer->pointer );
					GLStates |= GPU_BUFFER_COLOR_STATE;
					offset += 3*sizeof(unsigned char);
					break;
				case GPU_BUFFER_INTER_C4UB:
					glEnableClientState( GL_COLOR_ARRAY );
					glColorPointer( 4, GL_UNSIGNED_BYTE, elementsize, offset+(char *)buffer->pointer );
					GLStates |= GPU_BUFFER_COLOR_STATE;
					offset += 4*sizeof(unsigned char);
					break;
			}
		}
	}
}

static int GPU_typesize( int type ) {
	switch( type ) {
		case GL_FLOAT:
			return sizeof(float);
		case GL_INT:
			return sizeof(int);
		case GL_UNSIGNED_INT:
			return sizeof(unsigned int);
		case GL_BYTE:
			return sizeof(char);
		case GL_UNSIGNED_BYTE:
			return sizeof(unsigned char);
		default:
			return 0;
	}
}

int GPU_attrib_element_size( GPUAttrib data[], int numdata ) {
	int i, elementsize = 0;

	for( i = 0; i < numdata; i++ ) {
		int typesize = GPU_typesize(data[i].type);
		if( typesize == 0 )
			DEBUG_VBO( "Unknown element in data type array in GPU_attrib_element_size\n" );
		else {
			elementsize += typesize*data[i].size;
		}
	}
	return elementsize;
}

void GPU_interleaved_attrib_setup( GPUBuffer *buffer, GPUAttrib data[], int numdata ) {
	int i;
	int elementsize;
	intptr_t offset = 0;

	DEBUG_VBO("GPU_interleaved_attrib_setup\n");

	for( i = 0; i < MAX_GPU_ATTRIB_DATA; i++ ) {
		if( attribData[i].index != -1 ) {
			glDisableVertexAttribArrayARB( attribData[i].index );
		}
		else
			break;
	}
	elementsize = GPU_attrib_element_size( data, numdata );

	if( useVBOs ) {
		glBindBufferARB( GL_ARRAY_BUFFER_ARB, buffer->id );
		for( i = 0; i < numdata; i++ ) {
			glEnableVertexAttribArrayARB( data[i].index );
			glVertexAttribPointerARB( data[i].index, data[i].size, data[i].type, GL_TRUE, elementsize, (void *)offset );
			offset += data[i].size*GPU_typesize(data[i].type);

			attribData[i].index = data[i].index;
			attribData[i].size = data[i].size;
			attribData[i].type = data[i].type;
		}
		attribData[numdata].index = -1;
	}
	else {
		for( i = 0; i < numdata; i++ ) {
			glEnableVertexAttribArrayARB( data[i].index );
			glVertexAttribPointerARB( data[i].index, data[i].size, data[i].type, GL_TRUE, elementsize, (char *)buffer->pointer + offset );
			offset += data[i].size*GPU_typesize(data[i].type);
		}
	}
}


void GPU_buffer_unbind()
{
	int i;
	DEBUG_VBO("GPU_buffer_unbind\n");

	if( GLStates & GPU_BUFFER_VERTEX_STATE )
		glDisableClientState( GL_VERTEX_ARRAY );
	if( GLStates & GPU_BUFFER_NORMAL_STATE )
		glDisableClientState( GL_NORMAL_ARRAY );
	if( GLStates & GPU_BUFFER_TEXCOORD_STATE )
		glDisableClientState( GL_TEXTURE_COORD_ARRAY );
	if( GLStates & GPU_BUFFER_COLOR_STATE )
		glDisableClientState( GL_COLOR_ARRAY );
	if( GLStates & GPU_BUFFER_ELEMENT_STATE ) {
		if( useVBOs ) {
			glBindBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, 0 );
		}
	}
	GLStates &= !(GPU_BUFFER_VERTEX_STATE | GPU_BUFFER_NORMAL_STATE | GPU_BUFFER_TEXCOORD_STATE | GPU_BUFFER_COLOR_STATE | GPU_BUFFER_ELEMENT_STATE);

	for( i = 0; i < MAX_GPU_ATTRIB_DATA; i++ ) {
		if( attribData[i].index != -1 ) {
			glDisableVertexAttribArrayARB( attribData[i].index );
		}
		else
			break;
	}
	if( GLStates != 0 ) {
		DEBUG_VBO( "Some weird OpenGL state is still set. Why?" );
	}
	if( useVBOs )
		glBindBufferARB( GL_ARRAY_BUFFER_ARB, 0 );
}

void GPU_color3_upload( DerivedMesh *dm, unsigned char *data )
{
	if( dm->drawObject == 0 )
		dm->drawObject = GPU_drawobject_new(dm);
	GPU_buffer_free(dm->drawObject->colors,globalPool);
	dm->drawObject->colors = GPU_buffer_setup( dm, dm->drawObject, sizeof(char)*3*dm->drawObject->nelements, GL_ARRAY_BUFFER_ARB, data, GPU_buffer_copy_color3 );
}
void GPU_color4_upload( DerivedMesh *dm, unsigned char *data )
{
	if( dm->drawObject == 0 )
		dm->drawObject = GPU_drawobject_new(dm);
	GPU_buffer_free(dm->drawObject->colors,globalPool);
	dm->drawObject->colors = GPU_buffer_setup( dm, dm->drawObject, sizeof(char)*3*dm->drawObject->nelements, GL_ARRAY_BUFFER_ARB, data, GPU_buffer_copy_color4 );
}

void GPU_color_switch( int mode )
{
	if( mode ) {
		if( !(GLStates & GPU_BUFFER_COLOR_STATE) )
			glEnableClientState( GL_COLOR_ARRAY );
		GLStates |= GPU_BUFFER_COLOR_STATE;
	}
	else {
		if( GLStates & GPU_BUFFER_COLOR_STATE )
			glDisableClientState( GL_COLOR_ARRAY );
		GLStates &= (!GPU_BUFFER_COLOR_STATE);
	}
}

int GPU_buffer_legacy( DerivedMesh *dm )
{
	int test= (U.gameflags & USER_DISABLE_VBO);
	if( test )
		return 1;

	if( dm->drawObject == 0 )
		dm->drawObject = GPU_drawobject_new(dm);
	return dm->drawObject->legacy;
}

void *GPU_buffer_lock( GPUBuffer *buffer )
{
	float *varray;

	DEBUG_VBO("GPU_buffer_lock\n");
	if( buffer == 0 ) {
		DEBUG_VBO( "Failed to lock NULL buffer\n" );
		return 0;
	}

	if( useVBOs ) {
		glBindBufferARB( GL_ARRAY_BUFFER_ARB, buffer->id );
		varray = glMapBufferARB( GL_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB );
		if( varray == 0 ) {
			DEBUG_VBO( "Failed to map buffer to client address space\n" ); 
		}
		return varray;
	}
	else {
		return buffer->pointer;
	}
}

void *GPU_buffer_lock_stream( GPUBuffer *buffer )
{
	float *varray;

	DEBUG_VBO("GPU_buffer_lock_stream\n");
	if( buffer == 0 ) {
		DEBUG_VBO( "Failed to lock NULL buffer\n" );
		return 0;
	}

	if( useVBOs ) {
		glBindBufferARB( GL_ARRAY_BUFFER_ARB, buffer->id );
		glBufferDataARB( GL_ARRAY_BUFFER_ARB, buffer->size, 0, GL_STREAM_DRAW_ARB );	/* discard previous data, avoid stalling gpu */
		varray = glMapBufferARB( GL_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB );
		if( varray == 0 ) {
			DEBUG_VBO( "Failed to map buffer to client address space\n" ); 
		}
		return varray;
	}
	else {
		return buffer->pointer;
	}
}

void GPU_buffer_unlock( GPUBuffer *buffer )
{
	DEBUG_VBO( "GPU_buffer_unlock\n" ); 
	if( useVBOs ) {
		if( buffer != 0 ) {
			if( glUnmapBufferARB( GL_ARRAY_BUFFER_ARB ) == 0 ) {
				DEBUG_VBO( "Failed to copy new data\n" ); 
			}
		}
		glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
	}
}

void GPU_buffer_draw_elements( GPUBuffer *elements, unsigned int mode, int start, int count )
{
	if( useVBOs ) {
		glDrawElements( mode, count, GL_UNSIGNED_INT, (void *)(start*sizeof(unsigned int)) );
	}
	else {
		glDrawElements( mode, count, GL_UNSIGNED_INT, ((int *)elements->pointer)+start );
	}
}

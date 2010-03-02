/**
 * $Id$
 *
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): 2004-2006, Blender Foundation, full recode
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

/*
 * Storage, retrieval and query of render specific data.
 *
 * All data from a Blender scene is converted by the renderconverter/
 * into a special format that is used by the render module to make
 * images out of. These functions interface to the render-specific
 * database.  
 *
 * The blo{ha/ve/vl} arrays store pointers to blocks of 256 data
 * entries each.
 *
 * The index of an entry is >>8 (the highest 24 * bits), to find an
 * offset in a 256-entry block.
 *
 * - If the 256-entry block entry has an entry in the
 * vertnodes/vlaknodes/bloha array of the current block, the i-th entry in
 * that block is allocated to this entry.
 *
 * - If the entry has no block allocated for it yet, memory is
 * allocated.
 *
 * The pointer to the correct entry is returned. Memory is guarateed
 * to exist (as long as the malloc does not break). Since guarded
 * allocation is used, memory _must_ be available. Otherwise, an
 * exit(0) would occur.
 * 
 */

#include <limits.h>
#include <math.h>
#include <string.h>

#include "MEM_guardedalloc.h"
#include "BKE_utildefines.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_ghash.h"
#include "BLI_memarena.h"

#include "DNA_material_types.h" 
#include "DNA_mesh_types.h" 
#include "DNA_meshdata_types.h" 
#include "DNA_texture_types.h" 

#include "BKE_customdata.h"
#include "BKE_DerivedMesh.h"
#include "BKE_texture.h" 

#include "RE_render_ext.h"	/* externtex */
#include "RE_raytrace.h"

#include "camera.h"
#include "database.h"
#include "object.h"
#include "object_halo.h"
#include "object_mesh.h"
#include "object_strand.h"
#include "render_types.h"
#include "texture.h"
#include "zbuf.h"

/*************************** Chunked Allocation ******************************/

/* More dynamic allocation of options for render vertices and faces, so we dont
   have to reserve this space inside vertices.
   Important; vertices and faces, should have been created already (to get
   tables checked) that's a reason why the calls demand VertRen/VlakRen * as
   arg, not the index */

/* NOTE! the hardcoded table size 256 is used still in code for going quickly
   over vertices/faces */

int render_object_chunk_get(void **array_r, int *len_r, int nr, size_t size)
{
	void *array = *array_r;
	void *temp;
	int a, len = *len_r;

#if 0
	if(nr<0) {
		printf("error in render_db_chunk_get: %d\n", nr);
		return NULL;
	}
#endif

	a= nr>>8;
	
	/* need to allocate more columns..., and keep last element NULL for free loop */
	if (a >= len-1) {
		temp= array;
		
		array= MEM_callocN(size*(len+TABLEINITSIZE), "render_object_chunk_get");
		if(temp) memcpy(array, temp, len*size);
		memset(((char*)array) + len*size, 0, TABLEINITSIZE*size);
		len += TABLEINITSIZE; /* does this really need to be power of 2? */
		if(temp) MEM_freeN(temp);	

		*len_r = len;
		*array_r = array;
	}
	
	return a;
}

/****************************** CustomData Layers ****************************/

void render_object_customdata_set(ObjectRen *obr, CustomData *data)
{
	/* CustomData layer names are stored per object here, because the
	   DerivedMesh which stores the layers is freed */
	
	CustomDataLayer *layer;
	int numtf = 0, numcol = 0, i, mtfn, mcn;

	if (CustomData_has_layer(data, CD_MTFACE)) {
		numtf= CustomData_number_of_layers(data, CD_MTFACE);
		obr->mtface= MEM_callocN(sizeof(*obr->mtface)*numtf, "mtfacenames");
	}

	if (CustomData_has_layer(data, CD_MCOL)) {
		numcol= CustomData_number_of_layers(data, CD_MCOL);
		obr->mcol= MEM_callocN(sizeof(*obr->mcol)*numcol, "mcolnames");
	}

	for (i=0, mtfn=0, mcn=0; i < data->totlayer; i++) {
		layer= &data->layers[i];

		if (layer->type == CD_MTFACE) {
			strcpy(obr->mtface[mtfn++], layer->name);
			obr->actmtface= CLAMPIS(layer->active_rnd, 0, numtf);
			obr->bakemtface= layer->active;
		}
		else if (layer->type == CD_MCOL) {
			strcpy(obr->mcol[mcn++], layer->name);
			obr->actmcol= CLAMPIS(layer->active_rnd, 0, numcol);
		}
	}
}

/***************************** Objects ***************************************/

ObjectRen *render_object_create(RenderDB *rdb, Object *ob, Object *par, int index, int psysindex, int lay)
{
	ObjectRen *obr= MEM_callocN(sizeof(ObjectRen), "object render struct");
	
	BLI_addtail(&rdb->objecttable, obr);
	obr->ob= ob;
	obr->par= par;
	obr->index= index;
	obr->psysindex= psysindex;
	obr->lay= lay;

	obr->lowres= obr;

	return obr;
}

static void free_object_vertnodes(VertTableNode *vertnodes)
{
	int a;
	
	if(vertnodes==NULL) return;
	
	for(a=0; vertnodes[a].vert; a++) {
		MEM_freeN(vertnodes[a].vert);
		
		if(vertnodes[a].rad)
			MEM_freeN(vertnodes[a].rad);
		if(vertnodes[a].sticky)
			MEM_freeN(vertnodes[a].sticky);
		if(vertnodes[a].strand)
			MEM_freeN(vertnodes[a].strand);
		if(vertnodes[a].tangent)
			MEM_freeN(vertnodes[a].tangent);
		if(vertnodes[a].stress)
			MEM_freeN(vertnodes[a].stress);
		if(vertnodes[a].winspeed)
			MEM_freeN(vertnodes[a].winspeed);
	}
	
	MEM_freeN(vertnodes);
}

static void free_object_vlaknodes(VlakTableNode *vlaknodes)
{
	int a;
	
	if(vlaknodes==NULL) return;
	
	for(a=0; vlaknodes[a].vlak; a++) {
		MEM_freeN(vlaknodes[a].vlak);
		
		if(vlaknodes[a].mtface)
			MEM_freeN(vlaknodes[a].mtface);
		if(vlaknodes[a].mcol)
			MEM_freeN(vlaknodes[a].mcol);
		if(vlaknodes[a].surfnor)
			MEM_freeN(vlaknodes[a].surfnor);
		if(vlaknodes[a].tangent)
			MEM_freeN(vlaknodes[a].tangent);
	}
	
	MEM_freeN(vlaknodes);
}

static void free_object_strandnodes(StrandTableNode *strandnodes)
{
	int a;
	
	if(strandnodes==NULL) return;
	
	for(a=0; strandnodes[a].strand; a++) {
		MEM_freeN(strandnodes[a].strand);
		
		if(strandnodes[a].uv)
			MEM_freeN(strandnodes[a].uv);
		if(strandnodes[a].mcol)
			MEM_freeN(strandnodes[a].mcol);
		if(strandnodes[a].winspeed)
			MEM_freeN(strandnodes[a].winspeed);
		if(strandnodes[a].surfnor)
			MEM_freeN(strandnodes[a].surfnor);
		if(strandnodes[a].simplify)
			MEM_freeN(strandnodes[a].simplify);
		if(strandnodes[a].face)
			MEM_freeN(strandnodes[a].face);
	}
	
	MEM_freeN(strandnodes);
}

void render_object_free(ObjectRen *obr)
{
	StrandBuffer *strandbuf;
	int a;

	if(obr->vertnodes)
		free_object_vertnodes(obr->vertnodes);

	if(obr->vlaknodes)
		free_object_vlaknodes(obr->vlaknodes);

	if(!(obr->flag & R_TEMP_COPY)) {
		if(obr->bloha) {
			for(a=0; obr->bloha[a]; a++)
				MEM_freeN(obr->bloha[a]);

			MEM_freeN(obr->bloha);
		}

		if(obr->strandnodes)
			free_object_strandnodes(obr->strandnodes);

		strandbuf= obr->strandbuf;
		if(strandbuf) {
			if(strandbuf->vert) MEM_freeN(strandbuf->vert);
			if(strandbuf->bound) MEM_freeN(strandbuf->bound);
			MEM_freeN(strandbuf);
		}

		if(obr->mtface)
			MEM_freeN(obr->mtface);
		if(obr->mcol)
			MEM_freeN(obr->mcol);
			
		if(obr->rayfaces)
			MEM_freeN(obr->rayfaces);
		if(obr->rayprimitives)
			MEM_freeN(obr->rayprimitives);
		if(obr->raytree)
			RE_rayobject_free(obr->raytree);
	}
}

/******************************* Instances **********************************/

ObjectInstanceRen *render_instance_create(RenderDB *rdb,
	ObjectRen *obr, Object *ob, Object *par,
	int index, int psysindex, float mat[][4], int lay)
{
	ObjectInstanceRen *obi;
	float mat3[3][3];

	obi= MEM_callocN(sizeof(ObjectInstanceRen), "ObjectInstanceRen");
	obi->obr= obr;
	obi->ob= ob;
	obi->par= par;
	obi->index= index;
	obi->psysindex= psysindex;
	obi->lay= lay;

	if(mat) {
		copy_m4_m4(obi->mat, mat);
		copy_m3_m4(mat3, mat);
		invert_m3_m3(obi->nmat, mat3);
		transpose_m3(obi->nmat);
		obi->flag |= R_DUPLI_TRANSFORMED;
	}

	BLI_addtail(&rdb->instancetable, obi);

	return obi;
}

void render_instance_free(ObjectInstanceRen *obi)
{
	if(obi->vectors)
		MEM_freeN(obi->vectors);
	if(obi->raytree)
		RE_rayobject_free(obi->raytree);
}

void render_instances_init(RenderDB *rdb)
{
	ObjectInstanceRen *obi, *oldobi;
	ListBase newlist;
	int tot;

	/* convert list of object instances to an array for index based lookup */
	tot= BLI_countlist(&rdb->instancetable);
	rdb->objectinstance= MEM_callocN(sizeof(ObjectInstanceRen)*tot, "ObjectInstance");
	rdb->totinstance= tot;
	newlist.first= newlist.last= NULL;

	obi= rdb->objectinstance;
	for(oldobi=rdb->instancetable.first; oldobi; oldobi=oldobi->next) {
		*obi= *oldobi;

		if(obi->obr) {
			obi->prev= obi->next= NULL;
			BLI_addtail(&newlist, obi);
			obi++;
		}
		else
			rdb->totinstance--;
	}

	BLI_freelistN(&rdb->instancetable);
	rdb->instancetable= newlist;
}

void render_instances_bound(RenderDB *db, float boundbox[2][3])
{
	ObjectInstanceRen *obi= db->objectinstance;
	int a, tot= db->totinstance;
	float min[3], max[3];

	/* TODO this could be cached somewhere.. */

	INIT_MINMAX(min, max);

	for(a=0; a<tot; a++, obi++) {
		if(obi->flag & R_DUPLI_TRANSFORMED) {
			box_minmax_bounds_m4(min, max, obi->obr->boundbox, obi->mat);
		}
		else {
			DO_MIN(obi->obr->boundbox[0], min);
			DO_MAX(obi->obr->boundbox[1], max);
		}
	}

	copy_v3_v3(boundbox[0], min);
	copy_v3_v3(boundbox[1], max);
}


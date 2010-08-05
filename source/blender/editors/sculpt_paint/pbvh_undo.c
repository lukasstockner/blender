/*
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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2006 by Nicholas Bishop
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

#include "BLI_math.h"
#include "BLI_ghash.h"
#include "BLI_threads.h"

#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_mesh_types.h"
#include "DNA_key_types.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_dmgrid.h"
#include "BKE_modifier.h"
#include "BKE_multires.h"
#include "BKE_paint.h"
#include "BKE_mesh.h"
#include "BKE_key.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_sculpt.h"
#include "paint_intern.h"
#include "sculpt_intern.h"

/************************** Undo *************************/

struct PBVHUndoNode {
	struct PBVHUndoNode *next, *prev;

	/* object id name */
	char idname[MAX_ID_NAME];
	/* only during push, not valid afterwards! */
	struct PBVHNode *node;
	
	/* total unique verts in node */
	int totvert;

	/* actual undo data */
	float (*co)[3];
	/* paint mask */
	float *pmask;
	char pmask_name[32];
	/* vertex colors */
	float (*color)[4];
	char color_name[32];

	/* non-multires */
	/* to verify if me->totvert it still the same */
	int maxvert;
	/* to restore into the right location */
	int *vert_indices;
	int *face_indices;
	/* for per-facecorner data (colors) */
	int totface;

	/* multires */
	/* to verify total number of grids is still the same */
	int maxgrid;
	int gridsize;
	int totgrid;
	/* to restore into the right location */
	int *grid_indices;

	/* shape keys */
	char *shapeName[32]; /* keep size in sync with keyblock dna */

	/* only during push, not stored */
	short (*no)[3];
	/* layer brush */
	float *layer_disp;
};

static void pbvh_undo_restore_mesh_co(PBVHUndoNode *unode, bContext *C, Scene *scene, Object *ob)
{
	SculptSession *ss = ob->paint->sculpt;
	Mesh *me = ob->data;
	MVert *mvert;
	char *shapeName= (char*)unode->shapeName;
	int *index, i;

	if(ss && ss->kb && strcmp(ss->kb->name, shapeName)) {
		/* shape key has been changed before calling undo operator */

		Key *key= ob_get_key(ob);
		KeyBlock *kb= key_get_named_keyblock(key, shapeName);

		if (kb) {
			ob->shapenr= BLI_findindex(&key->block, kb) + 1;
			ob->shapeflag|= OB_SHAPE_LOCK;

			sculpt_update_mesh_elements(scene, ob, 0);
			WM_event_add_notifier(C, NC_OBJECT|ND_DATA, ob);
		} else {
			/* key has been removed -- skip this undo node */
			return;
		}
	}

	mvert= me->mvert;
	index= unode->vert_indices;

	if(ss && ss->kb) {
		float (*vertCos)[3];
		vertCos= key_to_vertcos(ob, ss->kb);

		for(i=0; i<unode->totvert; i++)
			swap_v3_v3(vertCos[index[i]], unode->co[i]);

		/* propagate new coords to keyblock */
		sculpt_vertcos_to_key(ob, ss->kb, vertCos);

		/* pbvh uses it's own mvert array, so coords should be */
		/* propagated to pbvh here */
		BLI_pbvh_apply_vertCos(ob->paint->pbvh, vertCos);

		MEM_freeN(vertCos);
	}
	else {
		for(i=0; i<unode->totvert; i++) {
			swap_v3_v3(mvert[index[i]].co, unode->co[i]);
			mvert[index[i]].flag |= ME_VERT_PBVH_UPDATE;
		}

	}
}

static void pbvh_undo_restore(bContext *C, ListBase *lb)
{
	Scene *scene = CTX_data_scene(C);
	Object *ob = CTX_data_active_object(C);
	Mesh *me = ob->data;
	DerivedMesh *dm = mesh_get_derived_final(scene, ob, 0);
	PBVH *pbvh;
	PBVHUndoNode *unode;
	MultiresModifierData *mmd;
	int i, j, update= 0, update_co = 0;

	// XXX: sculpt_update_mesh_elements(scene, ob, 0);

	pbvh = dm->getPBVH(ob, dm);

	for(unode=lb->first; unode; unode=unode->next) {
		CustomData *vdata, *fdata;

		if(!(strcmp(unode->idname, ob->id.name)==0))
			continue;

		BLI_pbvh_get_customdata(pbvh, &vdata, &fdata);

		if(unode->maxvert) {
			/* regular mesh restore */
			if(dm->getNumVerts(dm) != unode->maxvert)
				continue;

			if(unode->co) {
				pbvh_undo_restore_mesh_co(unode, C, scene, ob);
			}
			if(unode->pmask) {
				float *pmask;

				pmask = CustomData_get_layer_named(vdata,
								   CD_PAINTMASK,
								   unode->pmask_name);
			
				for(i=0; i<unode->totvert; i++)
					SWAP(float, pmask[unode->vert_indices[i]],
					     unode->pmask[i]);
			}
			if(unode->color) {
				MCol *mcol;

				mcol = CustomData_get_layer_named(fdata,
								  CD_MCOL,
								  unode->color_name);

				for(i=0; i<unode->totface; i++) {
					int face_index = unode->face_indices[i];
					MFace *f = me->mface + face_index;
					int S = f->v4?4:3;

					for(j=0; j<S; ++j) {
						MCol *col;
						float fcol[4];
						
						col = mcol + face_index*4+j;

						fcol[0] = mcol->b / 255.0f;
						fcol[1] = mcol->g / 255.0f;
						fcol[2] = mcol->r / 255.0f;
						fcol[3] = mcol->a / 255.0f;

						swap_v4_v4(fcol, unode->color[i*4+j]);

						mcol->b = fcol[0] * 255.0f;
						mcol->g = fcol[0] * 255.0f;
						mcol->r = fcol[0] * 255.0f;
						mcol->a = fcol[0] * 255.0f;
					}
				}
			}
		}
		else if(unode->maxgrid && dm->getGridData) {
			/* multires restore */
			DMGridData **grids, *grid;
			GridKey *gridkey;
			float (*co)[3] = NULL, *pmask = NULL, (*color)[4] = NULL;
			int gridsize, active_pmask, active_color;

			if(dm->getNumGrids(dm) != unode->maxgrid)
				continue;
			if(dm->getGridSize(dm) != unode->gridsize)
				continue;

			grids= dm->getGridData(dm);
			gridsize= dm->getGridSize(dm);
			gridkey= dm->getGridKey(dm);

			if(unode->co) {
				co = unode->co;
				update_co = 1;
			}
			if(unode->pmask) {
				pmask = unode->pmask;
				active_pmask = gridelem_active_offset(vdata, gridkey, CD_PAINTMASK);
			}
			if(unode->color) {
				color = unode->color;
				active_color = gridelem_active_offset(fdata, gridkey, CD_MCOL);
			}

			for(j=0; j<unode->totgrid; j++) {
				grid= grids[unode->grid_indices[j]];

				for(i=0; i<gridsize*gridsize; i++) {
					DMGridData *elem = GRIDELEM_AT(grid, i, gridkey);

					if(co) {
						swap_v3_v3(GRIDELEM_CO(elem, gridkey), co[0]);
						++co;
					}
					if(pmask) {
						SWAP(float, GRIDELEM_MASK(elem, gridkey)[active_pmask],
						     *pmask);
						++pmask;
					}
					if(color) {
						swap_v4_v4(GRIDELEM_COLOR(elem, gridkey)[active_color],
							   *color);
						++color;
					}
				}
			}
		}

		update= 1;
	}

	if(update) {
		SculptSession *ss = ob->paint->sculpt;
		int update_flags = PBVH_UpdateRedraw;

		/* we update all nodes still, should be more clever, but also
		   needs to work correct when exiting/entering sculpt mode and
		   the nodes get recreated, though in that case it could do all */
		BLI_pbvh_search_callback(ob->paint->pbvh, NULL, NULL, BLI_pbvh_node_set_flags,
					 SET_INT_IN_POINTER(PBVH_UpdateAll));

		if(update_co)
			update_flags |= PBVH_UpdateBB|PBVH_UpdateOriginalBB;

		BLI_pbvh_update(ob->paint->pbvh, update_flags, NULL);

		if((mmd=ED_paint_multires_active(scene, ob)))
			multires_mark_as_modified(ob);

		/* TODO: should work with other paint modes too */
		if(ss && (ss->modifiers_active || ((Mesh*)ob->data)->id.us > 1))
			DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
	}
}

static void pbvh_undo_free(ListBase *lb)
{
	PBVHUndoNode *unode;

	for(unode=lb->first; unode; unode=unode->next) {
		if(unode->co)
			MEM_freeN(unode->co);
		if(unode->no)
			MEM_freeN(unode->no);
		if(unode->pmask)
			MEM_freeN(unode->pmask);
		if(unode->color)
			MEM_freeN(unode->color);
		if(unode->vert_indices)
			MEM_freeN(unode->vert_indices);
		if(unode->face_indices)
			MEM_freeN(unode->face_indices);
		if(unode->grid_indices)
			MEM_freeN(unode->grid_indices);
		if(unode->layer_disp)
			MEM_freeN(unode->layer_disp);
	}
}

PBVHUndoNode *pbvh_undo_get_node(PBVHNode *node)
{
	ListBase *lb= undo_paint_push_get_list(UNDO_PAINT_MESH);
	PBVHUndoNode *unode;

	if(!lb)
		return NULL;

	for(unode=lb->first; unode; unode=unode->next)
		if(unode->node == node)
			return unode;

	return NULL;
}

PBVHUndoNode *pbvh_undo_push_node(PBVHNode *node, PBVHUndoFlag flag,
				  Object *ob)
{
	ListBase *lb= undo_paint_push_get_list(UNDO_PAINT_MESH);
	PBVHUndoNode *unode;
	PBVH *pbvh = ob->paint->pbvh;
	PBVHVertexIter vd;
	int totbytes= 0;

	CustomData *vdata, *fdata;
	SculptSession *ss = NULL; /* for shapekey */

	MFace *mface;
	int totface, *face_indices;

	GridKey *gridkey;
	int uses_grids, totgrid, *grid_indices;
	int grids_active_color;

	/* list is manipulated by multiple threads, so we lock */
	BLI_lock_thread(LOCK_CUSTOM1);

	if((unode= pbvh_undo_get_node(node))) {
		BLI_unlock_thread(LOCK_CUSTOM1);
		return unode;
	}

	unode= MEM_callocN(sizeof(PBVHUndoNode), "PBVHUndoNode");
	strcpy(unode->idname, ob->id.name);
	unode->node= node;

	/* XXX: changed this to just use unique verts rather than all,
	   seems like only unique are restored from anyway? -nicholas */
	BLI_pbvh_node_num_verts(pbvh, node, &unode->totvert, NULL);

	BLI_pbvh_get_customdata(pbvh, &vdata, &fdata);
	uses_grids = BLI_pbvh_uses_grids(pbvh);

	if(uses_grids) {
		/* multires */
		BLI_pbvh_node_get_grids(pbvh, node, &grid_indices,
					&totgrid, &unode->maxgrid, &unode->gridsize,
					NULL, NULL, &gridkey);
		
		unode->totgrid= totgrid;
		unode->grid_indices= MEM_mapallocN(sizeof(int)*totgrid, "PBVHUndoNode.grid_indices");
		totbytes += sizeof(int) * totgrid;
	}
	else {
		/* regular mesh */
		unode->maxvert= get_mesh(ob)->totvert;

		if(flag & (PBVH_UNDO_CO_NO|PBVH_UNDO_PMASK)) {
			unode->vert_indices= MEM_mapallocN(sizeof(int)*unode->totvert,
							   "PBVHUndoNode.vert_indices");
			totbytes += sizeof(int) * unode->totvert;
		}
	}

	/* allocate only the necessary undo data
	   XXX: we will use this while sculpting, is mapalloc slow to access then? */

	if(flag & PBVH_UNDO_CO_NO) {
		unode->co= MEM_mapallocN(sizeof(float)*3*unode->totvert, "PBVHUndoNode.co");
		unode->no= MEM_mapallocN(sizeof(short)*3*unode->totvert, "PBVHUndoNode.no");
		totbytes += (sizeof(float)*3 + sizeof(short)*3) * unode->totvert;
	}
	if(flag & PBVH_UNDO_PMASK) {
		int active;
		active= CustomData_get_active_layer_index(vdata, CD_PAINTMASK);

		if(active == -1)
			flag &= ~PBVH_UNDO_PMASK;
		else {
			BLI_strncpy(unode->pmask_name,
				    vdata->layers[active].name,
				    sizeof(unode->pmask_name));
			unode->pmask= MEM_mapallocN(sizeof(float)*unode->totvert, "PBVHUndoNode.pmask");
			totbytes += sizeof(float) * unode->totvert;
		}
	}
	if(flag & PBVH_UNDO_COLOR) {
		int totcol, active;
		active= CustomData_get_active_layer_index(fdata, CD_MCOL);

		if(active == -1)
			flag &= ~PBVH_UNDO_COLOR;
		else {
			BLI_strncpy(unode->color_name,
				    fdata->layers[active].name,
				    sizeof(unode->color_name));

			if(uses_grids) {
				totcol= unode->totvert;
				grids_active_color = gridelem_active_offset(fdata, gridkey, CD_MCOL);
			}
			else {
				BLI_pbvh_node_get_faces(pbvh, node,
							&mface, &face_indices,
							NULL, &totface);
				unode->totface= totface;

				unode->face_indices= MEM_mapallocN(sizeof(int)*unode->totface,
								   "PBVHUndoNode.face_indices");
				totbytes += sizeof(int)*unode->totface;

				totcol= totface * 4;
			}

			unode->color= MEM_mapallocN(sizeof(float)*4*totcol, "PBVHUndoNode.color");
			totbytes += sizeof(float)*4*totcol;
		}
	}

	/* push undo node onto paint undo list */
	undo_paint_push_count_alloc(UNDO_PAINT_MESH, totbytes);
	BLI_addtail(lb, unode);

	BLI_unlock_thread(LOCK_CUSTOM1);

	/* the rest is threaded, hopefully this is the performance critical part */

	if(uses_grids || (flag & (PBVH_UNDO_CO_NO|PBVH_UNDO_PMASK))) {
		BLI_pbvh_vertex_iter_begin(pbvh, node, vd, PBVH_ITER_UNIQUE) {
			if(flag & PBVH_UNDO_CO_NO) {
				copy_v3_v3(unode->co[vd.i], vd.co);
				if(vd.no) VECCOPY(unode->no[vd.i], vd.no)
				else normal_float_to_short_v3(unode->no[vd.i], vd.fno);
			}
			if(flag & PBVH_UNDO_PMASK) {
				if(vd.mask_active)
					unode->pmask[vd.i]= *vd.mask_active;			
			}
			if(flag & PBVH_UNDO_COLOR) {
				/* only copy for multires here */
				if(uses_grids) {
					copy_v4_v4(unode->color[vd.i],
						   GRIDELEM_COLOR(vd.elem, vd.gridkey)[grids_active_color]);
				}
			}

			if(vd.vert_indices)
				unode->vert_indices[vd.i]= vd.vert_indices[vd.i];
		}
		BLI_pbvh_vertex_iter_end;
	}

	if(unode->grid_indices)
		memcpy(unode->grid_indices, grid_indices, sizeof(int)*totgrid);

	/* non-multires: per face copy of color data */
	if(!uses_grids && (flag & PBVH_UNDO_COLOR)) {
		int active, i, j;

		active = CustomData_get_active_layer_index(fdata, CD_MCOL);

		for(i = 0; i < totface; ++i) {
			int face_index = face_indices[i];
			MFace *f = mface + face_index;
			int S = f->v4 ? 4 : 3;

			unode->face_indices[i]= face_index;

			for(j = 0; j < S; ++j) {
				MCol *mcol = fdata->layers[active].data;
				float fcol[4];

				mcol += face_index*4 + j;
				fcol[0] = mcol->b / 255.0f;
				fcol[1] = mcol->g / 255.0f;
				fcol[2] = mcol->r / 255.0f;
				fcol[3] = mcol->a / 255.0f;

				copy_v4_v4(unode->color[i*4 + j], fcol);
			}
		}
	}

	/* store active shape key */
	ss= ob->paint->sculpt;
	if(ss && ss->kb)
		BLI_strncpy((char*)unode->shapeName, ss->kb->name, sizeof(ss->kb->name));
	else
		unode->shapeName[0]= '\0';

	return unode;
}

void pbvh_undo_push_begin(char *name)
{
	undo_paint_push_begin(UNDO_PAINT_MESH, name,
		pbvh_undo_restore, pbvh_undo_free);
}

void pbvh_undo_push_end(void)
{
	ListBase *lb= undo_paint_push_get_list(UNDO_PAINT_MESH);
	PBVHUndoNode *unode;

	if(!lb || !lb->first)
		return;
		
	/* remove data that's only used during stroke */
	for(unode=lb->first; unode; unode=unode->next) {
		if(unode->no) {
			MEM_freeN(unode->no);
			unode->no= NULL;
		}

		if(unode->layer_disp) {
			MEM_freeN(unode->layer_disp);
			unode->layer_disp= NULL;
		}
	}

	undo_paint_push_end(UNDO_PAINT_MESH);
}

int pbvh_undo_node_totvert(PBVHUndoNode *unode)
{
	return unode->totvert;
}

pbvh_undo_f3 pbvh_undo_node_co(PBVHUndoNode *unode)
{
	return unode->co;
}

pbvh_undo_s3 pbvh_undo_node_no(PBVHUndoNode *unode)
{
	return unode->no;
}

float *pbvh_undo_node_layer_disp(PBVHUndoNode *unode)
{
	return unode->layer_disp;
}

void pbvh_undo_node_set_layer_disp(PBVHUndoNode *unode, float *layer_disp)
{
	unode->layer_disp = layer_disp;
}

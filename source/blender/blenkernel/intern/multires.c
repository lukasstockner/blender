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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2007 by Nicholas Bishop
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "MEM_guardedalloc.h"

#include "DNA_key_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_customdata.h"
#include "BKE_depsgraph.h"
#include "BKE_DerivedMesh.h"
#include "BKE_global.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_multires.h"
#include "BKE_object.h"
#include "BKE_subsurf.h"

#include "blendef.h"

#include <math.h>
#include <string.h>

/* Does not actually free lvl itself */
void multires_free_level(MultiresLevel *lvl)
{
	if(lvl) {
		if(lvl->faces) MEM_freeN(lvl->faces);
		if(lvl->edges) MEM_freeN(lvl->edges);
		if(lvl->colfaces) MEM_freeN(lvl->colfaces);
	}
}

void multires_free(Multires *mr)
{
	if(mr) {
		MultiresLevel* lvl= mr->levels.first;

		/* Free the first-level data */
		if(lvl) {
			CustomData_free(&mr->vdata, lvl->totvert);
			CustomData_free(&mr->fdata, lvl->totface);
			MEM_freeN(mr->edge_flags);
			MEM_freeN(mr->edge_creases);
		}

		while(lvl) {
			multires_free_level(lvl);			
			lvl= lvl->next;
		}

		MEM_freeN(mr->verts);

		BLI_freelistN(&mr->levels);

		MEM_freeN(mr);
	}
}

void create_vert_face_map(ListBase **map, IndexNode **mem, const MFace *mface, const int totvert, const int totface)
{
	int i,j;
	IndexNode *node = NULL;
	
	(*map) = MEM_callocN(sizeof(ListBase) * totvert, "vert face map");
	(*mem) = MEM_callocN(sizeof(IndexNode) * totface*4, "vert face map mem");
	node = *mem;
	
	/* Find the users */
	for(i = 0; i < totface; ++i){
		for(j = 0; j < (mface[i].v4?4:3); ++j, ++node) {
			node->index = i;
			BLI_addtail(&(*map)[((unsigned int*)(&mface[i]))[j]], node);
		}
	}
}

/* MULTIRES MODIFIER */
static const int multires_max_levels = 13;
static const int multires_quad_tot[] = {4, 9, 25, 81, 289, 1089, 4225, 16641, 66049, 263169, 1050625, 4198401, 16785409};
static const int multires_tri_tot[]  = {3, 7, 19, 61, 217, 817,  3169, 12481, 49537, 197377, 787969,  3148801, 12589057};
static const int multires_side_tot[] = {2, 3, 5,  9,  17,  33,   65,   129,   257,   513,    1025,    2049,    4097};

static void create_old_vert_face_map(ListBase **map, IndexNode **mem, const MultiresFace *mface,
				     const int totvert, const int totface)
{
	int i,j;
	IndexNode *node = NULL;
	
	(*map) = MEM_callocN(sizeof(ListBase) * totvert, "vert face map");
	(*mem) = MEM_callocN(sizeof(IndexNode) * totface*4, "vert face map mem");
	node = *mem;
	
	/* Find the users */
	for(i = 0; i < totface; ++i){
		for(j = 0; j < (mface[i].v[3]?4:3); ++j, ++node) {
			node->index = i;
			BLI_addtail(&(*map)[mface[i].v[j]], node);
		}
	}
}

static void create_old_vert_edge_map(ListBase **map, IndexNode **mem, const MultiresEdge *medge,
				     const int totvert, const int totedge)
{
	int i,j;
	IndexNode *node = NULL;
	
	(*map) = MEM_callocN(sizeof(ListBase) * totvert, "vert edge map");
	(*mem) = MEM_callocN(sizeof(IndexNode) * totedge*2, "vert edge map mem");
	node = *mem;
	
	/* Find the users */
	for(i = 0; i < totedge; ++i){
		for(j = 0; j < 2; ++j, ++node) {
			node->index = i;
			BLI_addtail(&(*map)[medge[i].v[j]], node);
		}
	}
}

static MultiresFace *find_old_face(ListBase *map, MultiresFace *faces, int v1, int v2, int v3, int v4)
{
	IndexNode *n1;
	int v[4] = {v1, v2, v3, v4}, i, j;

	for(n1 = map[v1].first; n1; n1 = n1->next) {
		int fnd[4] = {0, 0, 0, 0};

		for(i = 0; i < 4; ++i) {
			for(j = 0; j < 4; ++j) {
				if(v[i] == faces[n1->index].v[j])
					fnd[i] = 1;
			}
		}

		if(fnd[0] && fnd[1] && fnd[2] && fnd[3])
			return &faces[n1->index];
	}

	return NULL;
}

static MultiresEdge *find_old_edge(ListBase *map, MultiresEdge *edges, int v1, int v2)
{
	IndexNode *n1, *n2;

	for(n1 = map[v1].first; n1; n1 = n1->next) {
		for(n2 = map[v2].first; n2; n2 = n2->next) {
			if(n1->index == n2->index)
				return &edges[n1->index];
		}
	}

	return NULL;
}

static void multires_load_old_edges(ListBase **emap, MultiresLevel *lvl, int *vvmap, int dst, int v1, int v2, int mov)
{
	int emid = find_old_edge(emap[2], lvl->edges, v1, v2)->mid;
	vvmap[dst + mov] = emid;

	if(lvl->next->next) {
		multires_load_old_edges(emap + 1, lvl->next, vvmap, dst + mov, v1, emid, mov / 2);
		multires_load_old_edges(emap + 1, lvl->next, vvmap, dst + mov, v2, emid, -mov / 2);
	}
}

static void multires_load_old_faces(ListBase **fmap, ListBase **emap, MultiresLevel *lvl, int *vvmap, int dst,
				    int v1, int v2, int v3, int v4, int st2, int st3)
{
	int fmid;
	int emid13, emid14, emid23, emid24;

	if(lvl && lvl->next) {
		fmid = find_old_face(fmap[1], lvl->faces, v1, v2, v3, v4)->mid;
		vvmap[dst] = fmid;

		emid13 = find_old_edge(emap[1], lvl->edges, v1, v3)->mid;
		emid14 = find_old_edge(emap[1], lvl->edges, v1, v4)->mid;
		emid23 = find_old_edge(emap[1], lvl->edges, v2, v3)->mid;
		emid24 = find_old_edge(emap[1], lvl->edges, v2, v4)->mid;


		multires_load_old_faces(fmap + 1, emap + 1, lvl->next, vvmap, dst + st2 * st3 + st3,
					fmid, v2, emid23, emid24, st2, st3 / 2);

		multires_load_old_faces(fmap + 1, emap + 1, lvl->next, vvmap, dst - st2 * st3 + st3,
					emid14, emid24, fmid, v4, st2, st3 / 2);

		multires_load_old_faces(fmap + 1, emap + 1, lvl->next, vvmap, dst + st2 * st3 - st3,
					emid13, emid23, v3, fmid, st2, st3 / 2);

		multires_load_old_faces(fmap + 1, emap + 1, lvl->next, vvmap, dst - st2 * st3 - st3,
					v1, fmid, emid13, emid14, st2, st3 / 2);

		if(lvl->next->next) {
			multires_load_old_edges(emap, lvl->next, vvmap, dst, emid24, fmid, st3);
			multires_load_old_edges(emap, lvl->next, vvmap, dst, emid13, fmid, -st3);
			multires_load_old_edges(emap, lvl->next, vvmap, dst, emid14, fmid, -st2 * st3);
			multires_load_old_edges(emap, lvl->next, vvmap, dst, emid23, fmid, st2 * st3);
		}
	}
}

void multires_load_old(DerivedMesh *dm, Multires *mr)
{
	MultiresLevel *lvl, *lvl1;
	MVert *vsrc, *vdst;
	int src, dst;
	int totlvl = MultiresDM_get_totlvl(dm);
	int st = multires_side_tot[totlvl - 2] - 1;
	int extedgelen = multires_side_tot[totlvl - 1] - 2;
	int *vvmap; // inorder for dst, map to src
	int crossedgelen;
	int i, j, s, x, totvert, tottri, totquad;

	src = 0;
	dst = 0;
	vsrc = mr->verts;
	vdst = CDDM_get_verts(dm);
	totvert = dm->getNumVerts(dm);
	vvmap = MEM_callocN(sizeof(int) * totvert, "multires vvmap");

	lvl1 = mr->levels.first;
	/* Load base verts */
	for(i = 0; i < lvl1->totvert; ++i) {
		vvmap[totvert - lvl1->totvert + i] = src;
		++src;
	}

	/* Original edges */
	dst = totvert - lvl1->totvert - extedgelen * lvl1->totedge;
	for(i = 0; i < lvl1->totedge; ++i) {
		int ldst = dst + extedgelen * i;
		int lsrc = src;
		lvl = lvl1->next;

		for(j = 2; j <= mr->level_count; ++j) {
			int base = multires_side_tot[totlvl - j] - 2;
			int skip = multires_side_tot[totlvl - j + 1] - 1;
			int st = multires_side_tot[j - 2] - 1;

			for(x = 0; x < st; ++x)
				vvmap[ldst + base + x * skip] = lsrc + st * i + x;

			lsrc += lvl->totvert - lvl->prev->totvert;
			lvl = lvl->next;
		}
	}

	/* Center points */
	dst = 0;
	for(i = 0; i < lvl1->totface; ++i) {
		int sides = lvl1->faces[i].v[3] ? 4 : 3;

		vvmap[dst] = src + lvl1->totedge + i;
		dst += 1 + sides * (st - 1) * st;
	}


	/* The rest is only for level 3 and up */
	if(lvl1->next && lvl1->next->next) {
		ListBase **fmap, **emap;
		IndexNode **fmem, **emem;

		/* Face edge cross */
		tottri = totquad = 0;
		crossedgelen = multires_side_tot[totlvl - 2] - 2;
		dst = 0;
		for(i = 0; i < lvl1->totface; ++i) {
			int sides = lvl1->faces[i].v[3] ? 4 : 3;

			lvl = lvl1->next->next;
			++dst;

			for(j = 3; j <= mr->level_count; ++j) {
				int base = multires_side_tot[totlvl - j] - 2;
				int skip = multires_side_tot[totlvl - j + 1] - 1;
				int st = pow(2, j - 2);
				int st2 = pow(2, j - 3);
				int lsrc = lvl->prev->totvert;

				/* Skip exterior edge verts */
				lsrc += lvl1->totedge * st;

				/* Skip earlier face edge crosses */
				lsrc += st2 * (tottri * 3 + totquad * 4);

				for(s = 0; s < sides; ++s) {
					for(x = 0; x < st2; ++x) {
						vvmap[dst + crossedgelen * (s + 1) - base - x * skip - 1] = lsrc;
						++lsrc;
					}
				}

				lvl = lvl->next;
			}

			dst += sides * (st - 1) * st;

			if(sides == 4) ++totquad;
			else ++tottri;

		}

		/* calculate vert to edge/face maps for each level (except the last) */
		fmap = MEM_callocN(sizeof(ListBase*) * (mr->level_count-1), "multires fmap");
		emap = MEM_callocN(sizeof(ListBase*) * (mr->level_count-1), "multires emap");
		fmem = MEM_callocN(sizeof(IndexNode*) * (mr->level_count-1), "multires fmem");
		emem = MEM_callocN(sizeof(IndexNode*) * (mr->level_count-1), "multires emem");
		lvl = lvl1;
		for(i = 0; i < mr->level_count - 1; ++i) {
			create_old_vert_face_map(fmap + i, fmem + i, lvl->faces, lvl->totvert, lvl->totface);
			create_old_vert_edge_map(emap + i, emem + i, lvl->edges, lvl->totvert, lvl->totedge);
			lvl = lvl->next;
		}

		/* Interior face verts */
		lvl = lvl1->next->next;
		dst = 0;
		for(j = 0; j < lvl1->totface; ++j) {
			int sides = lvl1->faces[j].v[3] ? 4 : 3;
			int ldst = dst + 1 + sides * (st - 1);

			for(s = 0; s < sides; ++s) {
				int st2 = multires_side_tot[totlvl - 2] - 2;
				int st3 = multires_side_tot[totlvl - 3] - 2;
				int st4 = st3 == 0 ? 1 : (st3 + 1) / 2;
				int mid = ldst + st2 * st3 + st3;
				int cv = lvl1->faces[j].v[s];
				int nv = lvl1->faces[j].v[s == sides - 1 ? 0 : s + 1];
				int pv = lvl1->faces[j].v[s == 0 ? sides - 1 : s - 1];

				multires_load_old_faces(fmap, emap, lvl1->next, vvmap, mid,
							vvmap[dst], cv,
							find_old_edge(emap[0], lvl1->edges, pv, cv)->mid,
							find_old_edge(emap[0], lvl1->edges, cv, nv)->mid,
							st2, st4);

				ldst += (st - 1) * (st - 1);
			}


			dst = ldst;
		}

		lvl = lvl->next;

		for(i = 0; i < mr->level_count - 1; ++i) {
			MEM_freeN(fmap[i]);
			MEM_freeN(fmem[i]);
			MEM_freeN(emap[i]);
			MEM_freeN(emem[i]);
		}

		MEM_freeN(fmap);
		MEM_freeN(emap);
		MEM_freeN(fmem);
		MEM_freeN(emem);
	}

	/* Transfer verts */
	for(i = 0; i < totvert; ++i)
		VecCopyf(vdst[i].co, vsrc[vvmap[i]].co);

	MEM_freeN(vvmap);
}

int multiresModifier_switch_level(Object *ob, const int distance)
{
	ModifierData *md = NULL;
	MultiresModifierData *mmd = NULL;

	for(md = ob->modifiers.first; md; md = md->next) {
		if(md->type == eModifierType_Multires)
			mmd = (MultiresModifierData*)md;
	}

	if(mmd) {
		mmd->lvl += distance;
		if(mmd->lvl < 1) mmd->lvl = 1;
		else if(mmd->lvl > mmd->totlvl) mmd->lvl = mmd->totlvl;
		DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);
		object_handle_update(ob);
		return 1;
	}
	else
		return 0;
}

void multiresModifier_join(Object *ob)
{
	Base *base = NULL;
	int highest_lvl = 0;

	/* First find the highest level of subdivision */
	base = FIRSTBASE;
	while(base) {
		if(TESTBASELIB_BGMODE(base) && base->object->type==OB_MESH) {
			ModifierData *md;
			for(md = base->object->modifiers.first; md; md = md->next) {
				if(md->type == eModifierType_Multires) {
					int totlvl = ((MultiresModifierData*)md)->totlvl;
					if(totlvl > highest_lvl)
						highest_lvl = totlvl;

					/* Ensure that all updates are processed */
					multires_force_update(base->object);
				}
			}
		}
		base = base->next;
	}

	/* No multires meshes selected */
	if(highest_lvl == 0)
		return;

	/* Subdivide all the displacements to the highest level */
	base = FIRSTBASE;
	while(base) {
		if(TESTBASELIB_BGMODE(base) && base->object->type==OB_MESH) {
			ModifierData *md = NULL;
			MultiresModifierData *mmd = NULL;

			for(md = base->object->modifiers.first; md; md = md->next) {
				if(md->type == eModifierType_Multires)
					mmd = (MultiresModifierData*)md;
			}

			/* If the object didn't have multires enabled, give it a new modifier */
			if(!mmd) {
				ModifierData *md = base->object->modifiers.first;
				
				while(md && modifierType_getInfo(md->type)->type == eModifierTypeType_OnlyDeform)
					md = md->next;
				
				mmd = (MultiresModifierData*)modifier_new(eModifierType_Multires);
				BLI_insertlinkbefore(&base->object->modifiers, md, mmd);
			}

			if(mmd) {
				int i;

				/* TODO: subdivision should be doable in one step rather than iteratively. */
				for(i = mmd->totlvl; i < highest_lvl; ++i)
					multiresModifier_subdivide(mmd, base->object, 0, 0);
			}
		}
		base = base->next;
	}
}

static void Mat3FromColVecs(float mat[][3], float v1[3], float v2[3], float v3[3])
{
	VecCopyf(mat[0], v1);
	VecCopyf(mat[1], v2);
	VecCopyf(mat[2], v3);
}

static void calc_ts_mat(float out[][3], float center[3], float spintarget[3], float normal[3])
{
	float tan[3], cross[3];

	VecSubf(tan, spintarget, center);
	Normalize(tan);

	Crossf(cross, normal, tan);

	Mat3FromColVecs(out, tan, cross, normal);
}

static void face_center(float *out, float *a, float *b, float *c, float *d)
{
	VecAddf(out, a, b);
	VecAddf(out, out, c);
	if(d)
		VecAddf(out, out, d);

	VecMulf(out, d ? 0.25 : 1.0 / 3.0);
}

static void calc_norm(float *norm, float *a, float *b, float *c, float *d)
{
	if(d)
		CalcNormFloat4(a, b, c, d, norm);
	else
		CalcNormFloat(a, b, c, norm);
}

static void calc_face_ts_mat(float out[][3], float *v1, float *v2, float *v3, float *v4)
{
	float center[3], norm[3];

	face_center(center, v1, v2, v3, v4);
	calc_norm(norm, v1, v2, v3, v4);
	calc_ts_mat(out, center, v1, norm);
}

static void calc_face_ts_mat_dm(float out[][3], float (*orco)[3], MFace *f)
{
	calc_face_ts_mat(out, orco[f->v1], orco[f->v2], orco[f->v3], (f->v4 ? orco[f->v4] : NULL));
}

static void calc_face_ts_partial(float center[3], float target[3], float norm[][3], float (*orco)[3], MFace *f)
{
	face_center(center, orco[f->v1], orco[f->v2], orco[f->v3], (f->v4 ? orco[f->v4] : NULL));
	VecCopyf(target, orco[f->v1]);
}

DerivedMesh *multires_subdisp_pre(DerivedMesh *mrdm, int distance, int simple)
{
	DerivedMesh *final;
	SubsurfModifierData smd;

	memset(&smd, 0, sizeof(SubsurfModifierData));
	smd.levels = distance;
	if(simple)
		smd.subdivType = ME_SIMPLE_SUBSURF;

	final = subsurf_make_derived_from_derived_with_multires(mrdm, &smd, NULL, 0, NULL, 0, 0);

	return final;
}

void VecAddUf(float a[3], float b[3])
{
	a[0] += b[0];
	a[1] += b[1];
	a[2] += b[2];
}

static void multires_subdisp(DerivedMesh *orig, Mesh *me, DerivedMesh *final, int lvl, int totlvl,
			     int totsubvert, int totsubedge, int totsubface, int addverts)
{
	DerivedMesh *mrdm;
	MultiresModifierData mmd_sub;
	MVert *mvs = CDDM_get_verts(final);
	MVert *mvd, *mvd_f1, *mvs_f1, *mvd_f3, *mvd_f4;
	MVert *mvd_f2, *mvs_f2, *mvs_e1, *mvd_e1, *mvs_e2;
	int totvert;
	int slo1 = multires_side_tot[lvl - 1];
	int sll = slo1 / 2;
	int slo2 = multires_side_tot[totlvl - 2];
	int shi2 = multires_side_tot[totlvl - 1];
	int skip = multires_side_tot[totlvl - lvl] - 1;
	int i, j, k;

	mmd_sub.lvl = mmd_sub.totlvl = totlvl;
	mrdm = multires_dm_create_from_derived(&mmd_sub, orig, me, 0, 0);
		
	mvd = CDDM_get_verts(mrdm);
	/* Need to map from ccg to mrdm */
	totvert = mrdm->getNumVerts(mrdm);

	if(!addverts) {
		for(i = 0; i < totvert; ++i) {
			float z[3] = {0,0,0};
			VecCopyf(mvd[i].co, z);
		}
	}

	/* Load base verts */
	for(i = 0; i < me->totvert; ++i)
		VecAddUf(mvd[totvert - me->totvert + i].co, mvs[totvert - me->totvert + i].co);

	mvd_f1 = mvd;
	mvs_f1 = mvs;
	mvd_f2 = mvd;
	mvs_f2 = mvs + totvert - totsubvert;
	mvs_e1 = mvs + totsubface * (skip-1) * (skip-1);

	for(i = 0; i < me->totface; ++i) {
		const int end = me->mface[i].v4 ? 4 : 3;
		int x, y, x2, y2, mov;

		mvd_f1 += 1 + end * (slo2-2); //center+edgecross
		mvd_f3 = mvd_f4 = mvd_f1;

		for(j = 0; j < end; ++j) {
			mvd_f1 += (skip/2 - 1) * (slo2 - 2) + (skip/2 - 1);
			/* Update sub faces */
			for(y = 0; y < sll; ++y) {
				for(x = 0; x < sll; ++x) {
					/* Face center */
					VecAddUf(mvd_f1->co, mvs_f1->co);
					mvs_f1 += 1;

					/* Now we hold the center of the subface at mvd_f1
					   and offset it to the edge cross and face verts */

					/* Edge cross */
					for(k = 0; k < 4; ++k) {
						if(k == 0) mov = -1;
						else if(k == 1) mov = slo2 - 2;
						else if(k == 2) mov = 1;
						else if(k == 3) mov = -(slo2 - 2);

						for(x2 = 1; x2 < skip/2; ++x2) {
							VecAddUf((mvd_f1 + mov * x2)->co, mvs_f1->co);
							++mvs_f1;
						}
					}

					/* Main face verts */
					for(k = 0; k < 4; ++k) {
						int movx, movy;

						if(k == 0) { movx = -1; movy = -(slo2 - 2); }
						else if(k == 1) { movx = slo2 - 2; movy = -1; }
						else if(k == 2) { movx = 1; movy = slo2 - 2; }
						else if(k == 3) { movx = -(slo2 - 2); movy = 1; }

						for(y2 = 1; y2 < skip/2; ++y2) {
							for(x2 = 1; x2 < skip/2; ++x2) {
								VecAddUf((mvd_f1 + movy * y2 + movx * x2)->co, mvs_f1->co);
								++mvs_f1;
							}
						}
					}
							
					mvd_f1 += skip;
				}
				mvd_f1 += (skip - 1) * (slo2 - 2) - 1;
			}
			mvd_f1 -= (skip - 1) * (slo2 - 2) - 1 + skip;
			mvd_f1 += (slo2 - 2) * (skip/2-1) + skip/2-1 + 1;
		}

		/* update face center verts */
		VecAddUf(mvd_f2->co, mvs_f2->co);

		mvd_f2 += 1;
		mvs_f2 += 1;

		/* update face edge verts */
		for(j = 0; j < end; ++j) {
			MVert *restore;

			/* Super-face edge cross */
			for(k = 0; k < skip-1; ++k) {
				VecAddUf(mvd_f2->co, mvs_e1->co);
				mvd_f2++;
				mvs_e1++;
			}
			for(x = 1; x < sll; ++x) {
				VecAddUf(mvd_f2->co, mvs_f2->co);
				mvd_f2++;
				mvs_f2++;

				for(k = 0; k < skip-1; ++k) {
					VecAddUf(mvd_f2->co, mvs_e1->co);
					mvd_f2++;
					mvs_e1++;
				}
			}

			restore = mvs_e1;
			for(y = 0; y < sll - 1; ++y) {
				for(x = 0; x < sll; ++x) {
					for(k = 0; k < skip - 1; ++k) {
						VecAddUf(mvd_f3[(skip-1)+(y*skip) + (x*skip+k)*(slo2-2)].co,
							 mvs_e1->co);
						++mvs_e1;
					}
					mvs_e1 += skip-1;
				}
			}
			
			mvs_e1 = restore + skip - 1;
			for(y = 0; y < sll - 1; ++y) {
				for(x = 0; x < sll; ++x) {
					for(k = 0; k < skip - 1; ++k) {
						VecAddUf(mvd_f3[(slo2-2)*(skip-1)+(x*skip)+k + y*skip*(slo2-2)].co,
							 mvs_e1->co);
						++mvs_e1;
					}
					mvs_e1 += skip - 1;
				}
			}

			mvd_f3 += (slo2-2)*(slo2-2);
			mvs_e1 -= skip - 1;
		}

		/* update base (2) face verts */
		for(j = 0; j < end; ++j) {
			mvd_f2 += (slo2 - 1) * (skip - 1);
			for(y = 0; y < sll - 1; ++y) {
				for(x = 0; x < sll - 1; ++x) {
					VecAddUf(mvd_f2->co, mvs_f2->co);
					mvd_f2 += skip;
					++mvs_f2;
				}
				mvd_f2 += (slo2 - 1) * (skip - 1);
			}
			mvd_f2 -= (skip - 1);
		}
	}

	/* edges */
	mvd_e1 = mvd + totvert - me->totvert - me->totedge * (shi2-2);
	mvs_e2 = mvs + totvert - me->totvert - me->totedge * (slo1-2);
	for(i = 0; i < me->totedge; ++i) {
		for(j = 0; j < skip - 1; ++j) {
			VecAddUf(mvd_e1->co, mvs_e1->co);
			mvd_e1++;
			mvs_e1++;
		}
		for(j = 0; j < slo1 - 2; j++) {
			VecAddUf(mvd_e1->co, mvs_e2->co);
			mvd_e1++;
			mvs_e2++;
			
			for(k = 0; k < skip - 1; ++k) {
				VecAddUf(mvd_e1->co, mvs_e1->co);
				mvd_e1++;
				mvs_e1++;
			}
		}
	}

	final->needsFree = 1;
	final->release(final);
	mrdm->needsFree = 1;
	*MultiresDM_get_flags(mrdm) |= MULTIRES_DM_UPDATE_ALWAYS;
	mrdm->release(mrdm);
}

void multiresModifier_subdivide(MultiresModifierData *mmd, Object *ob, int updateblock, int simple)
{
	DerivedMesh *final = NULL;
	int totsubvert, totsubface, totsubedge;
	Mesh *me = get_mesh(ob);
	MDisps *mdisps;
	int i;

	if(mmd->totlvl == multires_max_levels) {
		// TODO
		return;
	}

	multires_force_update(ob);

	++mmd->lvl;
	++mmd->totlvl;

	mdisps = CustomData_get_layer(&me->fdata, CD_MDISPS);
	if(!mdisps)
		mdisps = CustomData_add_layer(&me->fdata, CD_MDISPS, CD_DEFAULT, NULL, me->totface);

	if(mdisps->disps && !updateblock) {
		DerivedMesh *orig, *mrdm;
		MultiresModifierData mmd_sub;

		orig = CDDM_from_mesh(me, NULL);
		mmd_sub.lvl = mmd_sub.totlvl = mmd->totlvl - 1;
		mrdm = multires_dm_create_from_derived(&mmd_sub, orig, me, 0, 0);
		totsubvert = mrdm->getNumVerts(mrdm);
		totsubedge = mrdm->getNumEdges(mrdm);
		totsubface = mrdm->getNumFaces(mrdm);
		orig->needsFree = 1;
		orig->release(orig);
		
		final = multires_subdisp_pre(mrdm, 1, simple);
		mrdm->needsFree = 1;
		mrdm->release(mrdm);
	}

	for(i = 0; i < me->totface; ++i) {
		//const int totdisp = (me->mface[i].v4 ? multires_quad_tot[totlvl] : multires_tri_tot[totlvl]);
		const int totdisp = multires_quad_tot[mmd->totlvl - 1];
		float (*disps)[3] = MEM_callocN(sizeof(float) * 3 * totdisp, "multires disps");

		if(mdisps[i].disps)
			MEM_freeN(mdisps[i].disps);

		mdisps[i].disps = disps;
		mdisps[i].totdisp = totdisp;
	}


	if(final && !updateblock) {
		DerivedMesh *orig;

		orig = CDDM_from_mesh(me, NULL);

		multires_subdisp(orig, me, final, mmd->totlvl - 1, mmd->totlvl, totsubvert, totsubedge, totsubface, 0);

		orig->needsFree = 1;
		orig->release(orig);
	}
}

void multiresModifier_setLevel(void *mmd_v, void *ob_v)
{
	MultiresModifierData *mmd = mmd_v;
	Object *ob = ob_v;
	Mesh *me = get_mesh(ob);

	if(me && mmd) {
		// TODO
	}
}

void multires_displacer_init(MultiresDisplacer *d, DerivedMesh *dm,
			     const int face_index, const int invert)
{
	float inv[3][3];

	d->face = MultiresDM_get_mesh(dm)->mface + face_index;
	/* Get the multires grid from customdata and calculate the TS matrix */
	d->grid = (MDisps*)dm->getFaceDataArray(dm, CD_MDISPS);
	if(d->grid)
		d->grid += face_index;
	calc_face_ts_mat_dm(d->mat, MultiresDM_get_orco(dm), d->face);
	if(invert) {
		Mat3Inv(inv, d->mat);
		Mat3CpyMat3(d->mat, inv);
	}

	calc_face_ts_partial(d->mat_center, d->mat_target, d->mat_norms, MultiresDM_get_orco(dm), d->face);
	d->mat_norms = MultiresDM_get_vertnorm(dm);

	d->spacing = pow(2, MultiresDM_get_totlvl(dm) - MultiresDM_get_lvl(dm));
	d->sidetot = multires_side_tot[MultiresDM_get_totlvl(dm) - 1];
	d->invert = invert;
}

void multires_displacer_weight(MultiresDisplacer *d, const float w)
{
	d->weight = w;
}

void multires_displacer_anchor(MultiresDisplacer *d, const int type, const int side_index)
{
	d->sidendx = side_index;
	d->x = d->y = d->sidetot / 2;
	d->type = type;

	if(type == 2) {
		if(side_index == 0)
			d->y -= d->spacing;
		else if(side_index == 1)
			d->x += d->spacing;
		else if(side_index == 2)
			d->y += d->spacing;
		else if(side_index == 3)
			d->x -= d->spacing;
	}
	else if(type == 3) {
		if(side_index == 0) {
			d->x -= d->spacing;
			d->y -= d->spacing;
		}
		else if(side_index == 1) {
			d->x += d->spacing;
			d->y -= d->spacing;
		}
		else if(side_index == 2) {
			d->x += d->spacing;
			d->y += d->spacing;
	}
		else if(side_index == 3) {
			d->x -= d->spacing;
			d->y += d->spacing;
		}
	}

	d->ax = d->x;
	d->ay = d->y;
}

void multires_displacer_anchor_edge(MultiresDisplacer *d, int v1, int v2, int x)
{
	const int mov = d->spacing * x;

	d->type = 4;

	if(v1 == d->face->v1) {
		d->x = 0;
		d->y = 0;
		if(v2 == d->face->v2)
			d->x += mov;
		else
			d->y += mov;
	}
	else if(v1 == d->face->v2) {
		d->x = d->sidetot - 1;
		d->y = 0;
		if(v2 == d->face->v1)
			d->x -= mov;
		else
			d->y += mov;
	}
	else if(v1 == d->face->v3) {
		d->x = d->sidetot - 1;
		d->y = d->sidetot - 1;
		if(v2 == d->face->v2)
			d->y -= mov;
		else
			d->x -= mov;
	}
	else if(v1 == d->face->v4) {
		d->x = 0;
		d->y = d->sidetot - 1;
		if(v2 == d->face->v3)
			d->x += mov;
		else
			d->y -= mov;
	}
}

void multires_displacer_anchor_vert(MultiresDisplacer *d, const int v)
{
	const int e = d->sidetot - 1;

	d->type = 5;

	d->x = d->y = 0;
	if(v == d->face->v2)
		d->x = e;
	else if(v == d->face->v3)
		d->x = d->y = e;
	else if(v == d->face->v4)
		d->y = e;
}

void multires_displacer_jump(MultiresDisplacer *d)
{
	if(d->sidendx == 0) {
		d->x -= d->spacing;
		d->y = d->ay;
	}
	else if(d->sidendx == 1) {
		d->x = d->ax;
		d->y -= d->spacing;
	}
	else if(d->sidendx == 2) {
		d->x += d->spacing;
		d->y = d->ay;
	}
	else if(d->sidendx == 3) {
		d->x = d->ax;
		d->y += d->spacing;
	}
}

void multires_displace(MultiresDisplacer *d, float co[3])
{
	float disp[3];
	float *data;

	if(!d->grid || !d->grid->disps) return;

	data = d->grid->disps[d->y * d->sidetot + d->x];

	if(d->invert)
		VecSubf(disp, co, d->subco->co);
	else
		VecCopyf(disp, data);

	{
		float norm[3];
		float mat[3][3], inv[3][3];

		norm[0] = d->subco->no[0] / 32767.0f;
		norm[1] = d->subco->no[1] / 32767.0f;
		norm[2] = d->subco->no[2] / 32767.0f;

		calc_ts_mat(mat, d->mat_center, d->mat_target, norm);
		if(d->invert) {
			Mat3Inv(inv, mat);
			Mat3CpyMat3(mat, inv);
		}
			
		Mat3MulVecfl(mat, disp);
	}

	if(d->invert) {
		VecCopyf(data, disp);
		
	}
	else {
		if(d->type == 4 || d->type == 5)
			VecMulf(disp, d->weight);
		VecAddf(co, co, disp);
	}

	if(d->type == 2) {
		if(d->sidendx == 0)
			d->y -= d->spacing;
		else if(d->sidendx == 1)
			d->x += d->spacing;
		else if(d->sidendx == 2)
			d->y += d->spacing;
		else if(d->sidendx == 3)
			d->x -= d->spacing;
	}
	else if(d->type == 3) {
		if(d->sidendx == 0)
			d->y -= d->spacing;
		else if(d->sidendx == 1)
			d->x += d->spacing;
		else if(d->sidendx == 2)
			d->y += d->spacing;
		else if(d->sidendx == 3)
			d->x -= d->spacing;
	}
}

/* Returns 0 on success, 1 if the src's totvert doesn't match */
int multiresModifier_reshape(MultiresModifierData *mmd, Object *dst, Object *src)
{
	Mesh *src_me = get_mesh(src);
	DerivedMesh *mrdm = dst->derivedFinal;

	if(mrdm && mrdm->getNumVerts(mrdm) == src_me->totvert) {
		MVert *mvert = CDDM_get_verts(mrdm);
		int i;

		for(i = 0; i < src_me->totvert; ++i)
			VecCopyf(mvert[i].co, src_me->mvert[i].co);
		mrdm->needsFree = 1;
		mrdm->release(mrdm);
		dst->derivedFinal = NULL;

		return 0;
	}

	return 1;
}

static void multiresModifier_disp_run(DerivedMesh *dm, MVert *subco, int invert)
{
	const int lvl = MultiresDM_get_lvl(dm);
	const int gridFaces = multires_side_tot[lvl - 2] - 1;
	const int edgeSize = multires_side_tot[lvl - 1] - 1;
	MVert *mvert = CDDM_get_verts(dm);
	MEdge *medge = MultiresDM_get_mesh(dm)->medge;
	MFace *mface = MultiresDM_get_mesh(dm)->mface;
	ListBase *map = MultiresDM_get_vert_face_map(dm);
	MultiresDisplacer d;
	int i, S, x, y;

	if(subco)
		d.subco = subco;

	for(i = 0; i < MultiresDM_get_mesh(dm)->totface; ++i) {
		const int numVerts = mface[i].v4 ? 4 : 3;
			
		multires_displacer_init(&d, dm, i, invert);
		multires_displacer_anchor(&d, 1, 0);
		multires_displace(&d, mvert->co);
		++mvert;
		++d.subco;

		for(S = 0; S < numVerts; ++S) {
			multires_displacer_anchor(&d, 2, S);
			for(x = 1; x < gridFaces; ++x) {
				multires_displace(&d, mvert->co);
				++mvert;
				++d.subco;
			}
		}

		for(S = 0; S < numVerts; S++) {
			multires_displacer_anchor(&d, 3, S);
			for(y = 1; y < gridFaces; y++) {
				for(x = 1; x < gridFaces; x++) {
					multires_displace(&d, mvert->co);
					++mvert;
					++d.subco;
				}
				multires_displacer_jump(&d);
			}
		}
	}

	for(i = 0; i < MultiresDM_get_mesh(dm)->totedge; ++i) {
		const MEdge *e = &medge[i];
		for(x = 1; x < edgeSize; ++x) {
			IndexNode *n1, *n2;
			int numFaces = 0;
			for(n1 = map[e->v1].first; n1; n1 = n1->next) {
				for(n2 = map[e->v2].first; n2; n2 = n2->next) {
					if(n1->index == n2->index)
						++numFaces;
				}
			}
			multires_displacer_weight(&d, 1.0f / numFaces);
			/* TODO: Better to have these loops outside the x loop */
			for(n1 = map[e->v1].first; n1; n1 = n1->next) {
				for(n2 = map[e->v2].first; n2; n2 = n2->next) {
					if(n1->index == n2->index) {
						multires_displacer_init(&d, dm, n1->index, invert);
						multires_displacer_anchor_edge(&d, e->v1, e->v2, x);
						multires_displace(&d, mvert->co);
					}
				}
			}
			++mvert;
			++d.subco;
		}
	}
		
	for(i = 0; i < MultiresDM_get_mesh(dm)->totvert; ++i) {
		IndexNode *n;
		multires_displacer_weight(&d, 1.0f / BLI_countlist(&map[i]));
		for(n = map[i].first; n; n = n->next) {
			multires_displacer_init(&d, dm, n->index, invert);
			multires_displacer_anchor_vert(&d, i);
			multires_displace(&d, mvert->co);
		}
		++mvert;
		++d.subco;
	}

	if(!invert)
		CDDM_calc_normals(dm);
}

static void multiresModifier_update(DerivedMesh *dm)
{
	MDisps *mdisps;
	MVert *mvert;
	MEdge *medge;
	MFace *mface;
	int i;

	if(!(G.f & G_SCULPTMODE) && !(*MultiresDM_get_flags(dm) & MULTIRES_DM_UPDATE_ALWAYS)) return;

	mdisps = dm->getFaceDataArray(dm, CD_MDISPS);

	if(mdisps) {
		SubsurfModifierData smd;
		const int lvl = MultiresDM_get_lvl(dm);
		const int totlvl = MultiresDM_get_totlvl(dm);
		Mesh *me = MultiresDM_get_mesh(dm);
		DerivedMesh *orig, *subco_dm;
		
		mvert = CDDM_get_verts(dm);
		medge = MultiresDM_get_mesh(dm)->medge;
		mface = MultiresDM_get_mesh(dm)->mface;

		orig = CDDM_from_mesh(me, NULL);

		if(lvl < totlvl) {
			/* Propagate disps upwards */
			DerivedMesh *final;
			MVert *verts_new;
			MultiresModifierData mmd;
			MVert *cur_lvl_orig_verts = NULL;
			
			/* Regenerate the current level's vertex coordinates without sculpting */
			mmd.totlvl = totlvl;
			mmd.lvl = lvl;
			subco_dm = multires_dm_create_from_derived(&mmd, orig, me, 0, 0);
			*MultiresDM_get_flags(subco_dm) |= MULTIRES_DM_UPDATE_BLOCK;
			cur_lvl_orig_verts = CDDM_get_verts(subco_dm);

			/* Subtract the original vertex cos from the new vertex cos */
			verts_new = CDDM_get_verts(dm);
			for(i = 0; i < dm->getNumVerts(dm); ++i)
				VecSubf(verts_new[i].co, verts_new[i].co, cur_lvl_orig_verts[i].co);

			final = multires_subdisp_pre(dm, totlvl - lvl, 0);

			multires_subdisp(orig, me, final, lvl, totlvl, dm->getNumVerts(dm), dm->getNumEdges(dm),
					 dm->getNumFaces(dm), 1);
		}
		else {
			/* Regenerate the current level's vertex coordinates without displacements */
			memset(&smd, 0, sizeof(SubsurfModifierData));
			smd.levels = lvl - 1;
			subco_dm = subsurf_make_derived_from_derived_with_multires(orig, &smd, NULL, 0, NULL, 0, 0);

			multiresModifier_disp_run(dm, CDDM_get_verts(subco_dm), 1);
		}
		
		orig->release(orig);
		subco_dm->release(subco_dm);
	}
}

void multires_force_update(Object *ob)
{
	if(ob->derivedFinal) {
		ob->derivedFinal->needsFree =1;
		ob->derivedFinal->release(ob->derivedFinal);
		ob->derivedFinal = NULL;
	}
}

struct DerivedMesh *multires_dm_create_from_derived(MultiresModifierData *mmd, DerivedMesh *dm, Mesh *me,
						    int useRenderParams, int isFinalCalc)
{
	SubsurfModifierData smd;
	MultiresSubsurf ms = {me, mmd->totlvl, mmd->lvl};
	DerivedMesh *result;
	int i;

	memset(&smd, 0, sizeof(SubsurfModifierData));
	smd.levels = smd.renderLevels = mmd->lvl - 1;

	result = subsurf_make_derived_from_derived_with_multires(dm, &smd, &ms, useRenderParams, NULL, isFinalCalc, 0);
	for(i = 0; i < result->getNumVerts(result); ++i)
		MultiresDM_get_subco(result)[i] = CDDM_get_verts(result)[i];
	multiresModifier_disp_run(result, MultiresDM_get_subco(result), 0);
	MultiresDM_set_update(result, multiresModifier_update);

	return result;
}

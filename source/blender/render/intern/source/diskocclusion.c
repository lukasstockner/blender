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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Brecht Van Lommel.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_material_types.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_memarena.h"
#include "BLI_threads.h"

#include "BKE_global.h"
#include "BKE_scene.h"
#include "BKE_utildefines.h"

#include "RE_shader_ext.h"

/* local includes */
#include "cache.h"
#include "database.h"
#include "diskocclusion.h"
#include "environment.h"
#include "object.h"
#include "object_mesh.h"
#include "part.h"
#include "render_types.h"
#include "rendercore.h"
#include "shading.h"
#include "object_strand.h"
#include "zbuf.h"

/* ------------------------- Declarations --------------------------- */

#define INVALID_INDEX ((int)(~0))
#define INVPI 0.31830988618379069f
#define TOTCHILD 8

typedef struct OccFace {
	int obi;
	int facenr;
} OccFace;

typedef struct OccNode {
	float co[3], area;
	float sh[9], dco;
	float occlusion, rad[3];
	int childflag;
	union {
		//OccFace face;
		int face;
		struct OccNode *node;
	} child[TOTCHILD];
} OccNode;

typedef struct OcclusionTree {
	MemArena *arena;

	float (*co)[3];		/* temporary during build */

	OccFace *face;		/* instance and face indices */
	float *occlusion;	/* occlusion for faces */
	float (*rad)[3];	/* radiance for faces */
	
	OccNode *root;

	OccNode **stack[BLENDER_MAX_THREADS];
	int maxdepth;

	int totface;

	float error;
	float distfac;

	int dothreadedbuild;
	int totbuildthread;
	int doindirect;

	PixelCache **cache;
} OcclusionTree;

typedef struct OcclusionThread {
	Render *re;
	SurfaceCache *mesh;
	float (*faceao)[3];
	float (*faceenv)[3];
	float (*faceindirect)[3];
	int begin, end;
	int thread;
} OcclusionThread;

typedef struct OcclusionBuildThread {
	Render *re;
	OcclusionTree *tree;
	int begin, end, depth;
	OccNode *node;
} OcclusionBuildThread;

/* ------------------------- Shading --------------------------- */

static void occ_shade(Render *re, ShadeSample *ssamp, ObjectInstanceRen *obi, VlakRen *vlr, float *rad)
{
	ShadeInput *shi= ssamp->shi;
	ShadeResult *shr= ssamp->shr;
	float l, u, v, *v1, *v2, *v3;
	
	/* init */
	if(vlr->v4) {
		shi->geometry.u= u= 0.5f;
		shi->geometry.v= v= 0.5f;
	}
	else {
		shi->geometry.u= u= 1.0f/3.0f;
		shi->geometry.v= v= 1.0f/3.0f;
	}

	/* setup render coordinates */
	v1= vlr->v1->co;
	v2= vlr->v2->co;
	v3= vlr->v3->co;
	
	/* renderco */
	l= 1.0f-u-v;
	
	shi->geometry.co[0]= l*v3[0]+u*v1[0]+v*v2[0];
	shi->geometry.co[1]= l*v3[1]+u*v1[1]+v*v2[1];
	shi->geometry.co[2]= l*v3[2]+u*v1[2]+v*v2[2];
	
	shade_input_set_triangle_i(re, shi, obi, vlr, 0, 1, 2);

	/* set up view vector */
	copy_v3_v3(shi->geometry.view, shi->geometry.co);
	normalize_v3(shi->geometry.view);
	
	/* cache for shadow */
	shi->shading.samplenr++;
	
	shi->geometry.xs= 0; // TODO
	shi->geometry.ys= 0;
	
	shade_input_set_normals(shi);

	/* no normal flip */
	if(shi->geometry.flippednor)
		shade_input_flip_normals(shi);

	madd_v3_v3fl(shi->geometry.co, shi->geometry.vn, 0.0001f); /* ugly.. */

	/* not a pretty solution, but fixes common cases */
	if(shi->primitive.obr->ob && shi->primitive.obr->ob->transflag & OB_NEG_SCALE) {
		negate_v3(shi->geometry.vn);
		negate_v3(shi->geometry.vno);
	}

	/* init material vars */
	// note, keep this synced with render_types.h
	memcpy(&shi->material.r, &shi->material.mat->r, 23*sizeof(float));
	shi->material.har= shi->material.mat->har;
	
	/* render */
	shade_input_set_shade_texco(re, shi);
	shade_material_loop(re, shi, shr); /* todo: nodes */
	
	copy_v3_v3(rad, shr->combined);
}

static void occ_build_shade(Render *re, OcclusionTree *tree)
{
	ShadeSample ssamp;
	ObjectInstanceRen *obi;
	VlakRen *vlr;
	int a;

	/* setup shade sample with correct passes */
	memset(&ssamp, 0, sizeof(ShadeSample));
	ssamp.shi[0].shading.lay= re->db.lay;
	ssamp.shi[0].shading.passflag= SCE_PASS_DIFFUSE|SCE_PASS_RGBA;
	ssamp.shi[0].shading.combinedflag= ~(SCE_PASS_SPEC);
	ssamp.tot= 1;

	for(a=0; a<tree->totface; a++) {
		obi= &re->db.objectinstance[tree->face[a].obi];
		vlr= render_object_vlak_get(obi->obr, tree->face[a].facenr);

		occ_shade(re, &ssamp, obi, vlr, tree->rad[a]);
	}
}

/* ------------------------------ Building --------------------------------- */

static void occ_face(Render *re, const OccFace *face, float *co, float *normal, float *area)
{
	ObjectInstanceRen *obi;
	VlakRen *vlr;
	float v1[3], v2[3], v3[3], v4[3];

	obi= &re->db.objectinstance[face->obi];
	vlr= render_object_vlak_get(obi->obr, face->facenr);
	
	if(co) {
		if(vlr->v4)
			interp_v3_v3v3(co, vlr->v1->co, vlr->v3->co, 0.5f);
		else
			cent_tri_v3(co, vlr->v1->co, vlr->v2->co, vlr->v3->co);

		if(obi->flag & R_TRANSFORMED)
			mul_m4_v3(obi->mat, co);
	}
	
	if(normal) {
		negate_v3_v3(normal, vlr->n);

		if(obi->flag & R_TRANSFORMED)
			mul_m3_v3(obi->nmat, normal);
	}

	if(area) {
		copy_v3_v3(v1, vlr->v1->co);
		copy_v3_v3(v2, vlr->v2->co);
		copy_v3_v3(v3, vlr->v3->co);
		if(vlr->v4) copy_v3_v3(v4, vlr->v4->co);

		if(obi->flag & R_TRANSFORMED) {
			mul_m4_v3(obi->mat, v1);
			mul_m4_v3(obi->mat, v2);
			mul_m4_v3(obi->mat, v3);
			if(vlr->v4) mul_m4_v3(obi->mat, v4);
		}

		/* todo: correct area for instances */
		if(vlr->v4)
			*area= area_quad_v3(v1, v2, v3, v4);
		else
			*area= area_tri_v3(v1, v2, v3);
	}
}

static void occ_sum_occlusion(Render *re, OcclusionTree *tree, OccNode *node)
{
	OccNode *child;
	float occ, area, totarea, rad[3];
	int a, b, indirect= tree->doindirect;

	occ= 0.0f;
	totarea= 0.0f;
	if(indirect) zero_v3(rad);

	for(b=0; b<TOTCHILD; b++) {
		if(node->childflag & (1<<b)) {
			a= node->child[b].face;
			occ_face(re, &tree->face[a], 0, 0, &area);
			occ += area*tree->occlusion[a];
			if(indirect) madd_v3_v3fl(rad, tree->rad[a], area);
			totarea += area;
		}
		else if(node->child[b].node) {
			child= node->child[b].node;
			occ_sum_occlusion(re, tree, child);

			occ += child->area*child->occlusion;
			if(indirect) madd_v3_v3fl(rad, child->rad, child->area);
			totarea += child->area;
		}
	}

	if(totarea != 0.0f) {
		occ /= totarea;
		if(indirect) mul_v3_fl(rad, 1.0f/totarea);
	}
	
	node->occlusion= occ;
	if(indirect) copy_v3_v3(node->rad, rad);
}

static int occ_find_bbox_axis(OcclusionTree *tree, int begin, int end, float *min, float *max)
{
	float len, maxlen= -1.0f;
	int a, axis = 0;

	INIT_MINMAX(min, max);

	for(a=begin; a<end; a++)
		DO_MINMAX(tree->co[a], min, max)

	for(a=0; a<3; a++) {
		len= max[a] - min[a];

		if(len > maxlen) {
			maxlen= len;
			axis= a;
		}
	}

	return axis;
}

static void occ_node_from_face(Render *re, OccFace *face, OccNode *node)
{
	float n[3];

	occ_face(re, face, node->co, n, &node->area);
	node->dco= 0.0f;
	vec_fac_to_sh(node->sh, n, node->area);
}

static void occ_build_dco(Render *re, OcclusionTree *tree, OccNode *node, float *co, float *dco)
{
	OccNode *child;
	float dist, d[3], nco[3];
	int b;

	for(b=0; b<TOTCHILD; b++) {
		if(node->childflag & (1<<b)) {
			occ_face(re, tree->face+node->child[b].face, nco, 0, 0);
		}
		else if(node->child[b].node) {
			child= node->child[b].node;
			occ_build_dco(re, tree, child, co, dco);
			copy_v3_v3(nco, child->co);
		}

		sub_v3_v3v3(d, nco, co);
		dist= dot_v3v3(d, d);
		if(dist > *dco)
			*dco= dist;
	}
}

static void occ_build_split(OcclusionTree *tree, int begin, int end, int *split)
{
	float min[3], max[3], mid;
	int axis, a, enda;

	/* split in middle of boundbox. this seems faster than median split
	 * on complex scenes, possibly since it avoids two distant faces to
	 * be in the same node better? */
	axis= occ_find_bbox_axis(tree, begin, end, min, max);
	mid= 0.5f*(min[axis]+max[axis]);

	a= begin;
	enda= end;
	while(a<enda) {
		if(tree->co[a][axis] > mid) {
			enda--;
			SWAP(OccFace, tree->face[a], tree->face[enda]);
			SWAP(float, tree->co[a][0], tree->co[enda][0]);
			SWAP(float, tree->co[a][1], tree->co[enda][1]);
			SWAP(float, tree->co[a][2], tree->co[enda][2]);
		}
		else
			a++;
	}

	*split= enda;
}

static void occ_build_8_split(OcclusionTree *tree, int begin, int end, int *offset, int *count)
{
	/* split faces into eight groups */
	int b, splitx, splity[2], splitz[4];

	occ_build_split(tree, begin, end, &splitx);

	/* force split if none found, to deal with degenerate geometry */
	if(splitx == begin || splitx == end)
		splitx= (begin+end)/2;

	occ_build_split(tree, begin, splitx, &splity[0]);
	occ_build_split(tree, splitx, end, &splity[1]);

	occ_build_split(tree, begin, splity[0], &splitz[0]);
	occ_build_split(tree, splity[0], splitx, &splitz[1]);
	occ_build_split(tree, splitx, splity[1], &splitz[2]);
	occ_build_split(tree, splity[1], end, &splitz[3]);

	offset[0]= begin;
	offset[1]= splitz[0];
	offset[2]= splity[0];
	offset[3]= splitz[1];
	offset[4]= splitx;
	offset[5]= splitz[2];
	offset[6]= splity[1];
	offset[7]= splitz[3];

	for(b=0; b<7; b++)
		count[b]= offset[b+1] - offset[b];
	count[7]= end - offset[7];
}

static void occ_build_recursive(Render *re, OcclusionTree *tree, OccNode *node, int begin, int end, int depth);

static void *exec_occ_build(void *data)
{
	OcclusionBuildThread *othread= (OcclusionBuildThread*)data;
	Render *re= othread->re;

	occ_build_recursive(re, othread->tree, othread->node, othread->begin, othread->end, othread->depth);

	return 0;
}

static void occ_build_recursive(Render *re, OcclusionTree *tree, OccNode *node, int begin, int end, int depth)
{
	ListBase threads;
	OcclusionBuildThread othreads[BLENDER_MAX_THREADS];
	OccNode *child, tmpnode;
	OccFace *face;
	int a, b, totthread=0, offset[TOTCHILD], count[TOTCHILD];

	/* add a new node */
	node->occlusion= 1.0f;

	/* leaf node with only children */
	if(end - begin <= TOTCHILD) {
		for(a=begin, b=0; a<end; a++, b++) {
			face= &tree->face[a];
			node->child[b].face= a;
			node->childflag |= (1<<b);
		}
	}
	else {
		/* order faces */
		occ_build_8_split(tree, begin, end, offset, count);

		if(depth == 1 && tree->dothreadedbuild)
			BLI_init_threads(&threads, exec_occ_build, tree->totbuildthread);

		for(b=0; b<TOTCHILD; b++) {
			if(count[b] == 0) {
				node->child[b].node= NULL;
			}
			else if(count[b] == 1) {
				face= &tree->face[offset[b]];
				node->child[b].face= offset[b];
				node->childflag |= (1<<b);
			}
			else {
				if(tree->dothreadedbuild)
					BLI_lock_thread(LOCK_CUSTOM1);

				child= BLI_memarena_alloc(tree->arena, sizeof(OccNode));
				node->child[b].node= child;

				/* keep track of maximum depth for stack */
				if(depth+1 > tree->maxdepth)
					tree->maxdepth= depth+1;

				if(tree->dothreadedbuild)
					BLI_unlock_thread(LOCK_CUSTOM1);

				if(depth == 1 && tree->dothreadedbuild) {
					othreads[totthread].tree= tree;
					othreads[totthread].node= child;
					othreads[totthread].begin= offset[b];
					othreads[totthread].end= offset[b]+count[b];
					othreads[totthread].depth= depth+1;
					othreads[totthread].re= re;
					BLI_insert_thread(&threads, &othreads[totthread]);
					totthread++;
				}
				else
					occ_build_recursive(re, tree, child, offset[b], offset[b]+count[b], depth+1);
			}
		}

		if(depth == 1 && tree->dothreadedbuild)
			BLI_end_threads(&threads);
	}

	/* combine area, position and sh */
	for(b=0; b<TOTCHILD; b++) {
		if(node->childflag & (1<<b)) {
			child= &tmpnode;
			occ_node_from_face(re, tree->face+node->child[b].face, &tmpnode);
		}
		else {
			child= node->child[b].node;
		}

		if(child) {
			node->area += child->area;
			add_sh_shsh(node->sh, node->sh, child->sh);
			madd_v3_v3v3fl(node->co, node->co, child->co, child->area);
		}
	}

	if(node->area != 0.0f)
		mul_v3_fl(node->co, 1.0f/node->area);

	/* compute maximum distance from center */
	node->dco= 0.0f;
	occ_build_dco(re, tree, node, node->co, &node->dco);
}

static void occ_build_sh_normalize(OccNode *node)
{
	/* normalize spherical harmonics to not include area, so
	 * we can clamp the dot product and then mutliply by area */
	int b;

	if(node->area != 0.0f)
		mul_sh_fl(node->sh, 1.0f/node->area);

	for(b=0; b<TOTCHILD; b++) {
		if(node->childflag & (1<<b));
		else if(node->child[b].node)
			occ_build_sh_normalize(node->child[b].node);
	}
}

static OcclusionTree *occ_tree_build(Render *re)
{
	OcclusionTree *tree;
	ObjectInstanceRen *obi;
	ObjectRen *obr;
	Material *ma;
	VlakRen *vlr= NULL;
	int a, b, c, totface;

	/* count */
	totface= 0;
	for(obi=re->db.instancetable.first; obi; obi=obi->next) {
		obr= obi->obr;
		for(a=0; a<obr->totvlak; a++) {
			if((a & 255)==0) vlr= obr->vlaknodes[a>>8].vlak;
			else vlr++;

			ma= vlr->mat;

			if((ma->shade_flag & MA_APPROX_OCCLUSION) && (ma->material_type == MA_TYPE_SURFACE))
				totface++;
		}
	}

	if(totface == 0)
		return NULL;
	
	tree= MEM_callocN(sizeof(OcclusionTree), "OcclusionTree");
	tree->totface= totface;

	/* parameters */
	tree->error= get_render_aosss_error(&re->params.r, re->db.wrld.ao_approx_error);
	tree->distfac= (re->db.wrld.aomode & WO_LIGHT_DIST)? re->db.wrld.aodistfac: 0.0f;
	tree->doindirect= (re->db.wrld.ao_indirect_energy > 0.0f && re->db.wrld.ao_indirect_bounces > 0);

	/* allocation */
	tree->arena= BLI_memarena_new(0x8000 * sizeof(OccNode));
	BLI_memarena_use_calloc(tree->arena);

	if(re->db.wrld.aomode & WO_LIGHT_CACHE)
		tree->cache= MEM_callocN(sizeof(PixelCache*)*BLENDER_MAX_THREADS, "PixelCache*");

	tree->face= MEM_callocN(sizeof(OccFace)*totface, "OcclusionFace");
	tree->co= MEM_callocN(sizeof(float)*3*totface, "OcclusionCo");
	tree->occlusion= MEM_callocN(sizeof(float)*totface, "OcclusionOcclusion");

	if(tree->doindirect)
		tree->rad= MEM_callocN(sizeof(float)*3*totface, "OcclusionRad");

	/* make array of face pointers */
	for(b=0, c=0, obi=re->db.instancetable.first; obi; obi=obi->next, c++) {
		obr= obi->obr;
		for(a=0; a<obr->totvlak; a++) {
			if((a & 255)==0) vlr= obr->vlaknodes[a>>8].vlak;
			else vlr++;

			ma= vlr->mat;

			if((ma->shade_flag & MA_APPROX_OCCLUSION) && (ma->material_type == MA_TYPE_SURFACE)) {
				tree->face[b].obi= c;
				tree->face[b].facenr= a;
				tree->occlusion[b]= 1.0f;
				occ_face(re, &tree->face[b], tree->co[b], NULL, NULL); 
				b++;
			}
		}
	}

	/* threads */
	tree->totbuildthread= (re->params.r.threads > 1 && totface > 10000)? 8: 1;
	tree->dothreadedbuild= (tree->totbuildthread > 1);

	/* recurse */
	tree->root= BLI_memarena_alloc(tree->arena, sizeof(OccNode));
	tree->maxdepth= 1;
	occ_build_recursive(re, tree, tree->root, 0, totface, 1);

	if(tree->doindirect) {
		occ_build_shade(re, tree);
		occ_sum_occlusion(re, tree, tree->root);
	}
	
	MEM_freeN(tree->co);
	tree->co= NULL;

	occ_build_sh_normalize(tree->root);

	for(a=0; a<BLENDER_MAX_THREADS; a++)
		tree->stack[a]= MEM_callocN(sizeof(OccNode)*TOTCHILD*(tree->maxdepth+1), "OccStack");

	return tree;
}

static void disk_occlusion_free_tree(OcclusionTree *tree)
{
	int a;

	if(tree) {
		if(tree->arena) BLI_memarena_free(tree->arena);
		for(a=0; a<BLENDER_MAX_THREADS; a++)
			if(tree->stack[a])
				MEM_freeN(tree->stack[a]);
		if(tree->occlusion) MEM_freeN(tree->occlusion);
		if(tree->cache) MEM_freeN(tree->cache);
		if(tree->face) MEM_freeN(tree->face);
		if(tree->rad) MEM_freeN(tree->rad);
		MEM_freeN(tree);
	}
}

/* ------------------------- Traversal --------------------------- */

static float occ_solid_angle(OccNode *node, float *v, float d2, float invd2, float *receivenormal)
{
	float dotreceive, dotemit;
	float ev[3];

	ev[0]= -v[0]*invd2;
	ev[1]= -v[1]*invd2;
	ev[2]= -v[2]*invd2;
	dotemit= diffuse_shv3(node->sh, ev);
	dotreceive= dot_v3v3(receivenormal, v)*invd2;

	CLAMP(dotemit, 0.0f, 1.0f);
	CLAMP(dotreceive, 0.0f, 1.0f);
	
	return ((node->area*dotemit*dotreceive)/(d2 + node->area*INVPI))*INVPI;
}

static float occ_form_factor(Render *re, OccFace *face, float *p, float *n)
{
	ObjectInstanceRen *obi;
	VlakRen *vlr;
	float v1[3], v2[3], v3[3], v4[3];

	obi= &re->db.objectinstance[face->obi];
	vlr= render_object_vlak_get(obi->obr, face->facenr);

	copy_v3_v3(v1, vlr->v1->co);
	copy_v3_v3(v2, vlr->v2->co);
	copy_v3_v3(v3, vlr->v3->co);

	if(obi->flag & R_TRANSFORMED) {
		mul_m4_v3(obi->mat, v1);
		mul_m4_v3(obi->mat, v2);
		mul_m4_v3(obi->mat, v3);
	}

	if(vlr->v4) {
		copy_v3_v3(v4, vlr->v4->co);
		if(obi->flag & R_TRANSFORMED)
			mul_m4_v3(obi->mat, v4);

		return form_factor_hemi_poly(p, n, v1, v2, v3, v4);
	}

	return form_factor_hemi_poly(p, n, v1, v2, v3, NULL);
}

static void occ_lookup(Render *re, OcclusionTree *tree, int thread, OccFace *exclude, float *pp, float *pn, float *occ, float rad[3], float bentn[3])
{
	OccNode *node, **stack;
	OccFace *face;
	float resultocc, resultrad[3], v[3], p[3], n[3], co[3], invd2;
	float distfac, fac, error, d2, weight, emitarea;
	int b, f, totstack;

	/* init variables */
	copy_v3_v3(p, pp);
	copy_v3_v3(n, pn);
	madd_v3_v3v3fl(p, p, n, 1e-4f);

	if(bentn)
		copy_v3_v3(bentn, n);
	
	error= tree->error;
	distfac= tree->distfac;

	resultocc= 0.0f;
	zero_v3(resultrad);

	/* init stack */
	stack= tree->stack[thread];
	stack[0]= tree->root;
	totstack= 1;

	while(totstack) {
		/* pop point off the stack */
		node= stack[--totstack];

		sub_v3_v3v3(v, node->co, p);
		d2= dot_v3v3(v, v) + 1e-16f;
		emitarea= MAX2(node->area, node->dco);

		if(d2*error > emitarea) {
			if(distfac != 0.0f) {
				fac= 1.0f/(1.0f + distfac*d2);
				if(fac < 0.01f)
					continue;
			}
			else
				fac= 1.0f;

			/* accumulate occlusion from spherical harmonics */
			invd2 = 1.0f/sqrtf(d2);
			weight= occ_solid_angle(node, v, d2, invd2, n);

			if(rad)
				madd_v3_v3fl(resultrad, node->rad, weight*fac);

			weight *= node->occlusion;

			if(bentn) {
				bentn[0] -= weight*invd2*v[0];
				bentn[1] -= weight*invd2*v[1];
				bentn[2] -= weight*invd2*v[2];
			}

			resultocc += weight*fac;
		}
		else {
			/* traverse into children */
			for(b=0; b<TOTCHILD; b++) {
				if(node->childflag & (1<<b)) {
					f= node->child[b].face;
					face= &tree->face[f];

					/* accumulate occlusion with face form factor */
					if(!exclude || !(face->obi == exclude->obi && face->facenr == exclude->facenr)) {
						if(bentn || distfac != 0.0f) {
							occ_face(re, face, co, NULL, NULL); 
							sub_v3_v3v3(v, co, p);
							d2= dot_v3v3(v, v) + 1e-16f;

							fac= (distfac == 0.0f)? 1.0f: 1.0f/(1.0f + distfac*d2);
							if(fac < 0.01f)
								continue;
						}
						else
							fac= 1.0f;

						weight= occ_form_factor(re, face, p, n);

						if(rad)
							madd_v3_v3fl(resultrad, tree->rad[f], weight*fac);

						weight *= tree->occlusion[f];

						if(bentn) {
							invd2= 1.0f/sqrtf(d2);
							bentn[0] -= weight*invd2*v[0];
							bentn[1] -= weight*invd2*v[1];
							bentn[2] -= weight*invd2*v[2];
						}

						resultocc += weight*fac;
					}
				}
				else if(node->child[b].node) {
					/* push child on the stack */
					stack[totstack++]= node->child[b].node;
				}
			}
		}
	}

	if(occ) *occ= resultocc;
	if(rad) copy_v3_v3(rad, resultrad);
	/*if(rad && exclude) {
		int a;
		for(a=0; a<tree->totface; a++)
			if((tree->face[a].obi == exclude->obi && tree->face[a].facenr == exclude->facenr))
				copy_v3_v3(rad, tree->rad[a]);
	}*/
	if(bentn) normalize_v3(bentn);
}

static void occ_compute_bounces(Render *re, OcclusionTree *tree, int totbounce)
{
	float (*rad)[3], (*sum)[3], (*tmp)[3], co[3], n[3], occ;
	int bounce, i;

	rad= MEM_callocN(sizeof(float)*3*tree->totface, "OcclusionBounceRad");
	sum= MEM_dupallocN(tree->rad);

	for(bounce=1; bounce<totbounce; bounce++) {
		for(i=0; i<tree->totface; i++) {
			occ_face(re, &tree->face[i], co, n, NULL);
			madd_v3_v3fl(co, n, 1e-8f);

			occ_lookup(re, tree, 0, &tree->face[i], co, n, &occ, rad[i], NULL);
			rad[i][0]= MAX2(rad[i][0], 0.0f);
			rad[i][1]= MAX2(rad[i][1], 0.0f);
			rad[i][2]= MAX2(rad[i][2], 0.0f);
			add_v3_v3(sum[i], rad[i]);

			if(re->cb.test_break(re->cb.tbh))
				break;
		}

		if(re->cb.test_break(re->cb.tbh))
			break;

		tmp= tree->rad;
		tree->rad= rad;
		rad= tmp;

		occ_sum_occlusion(re, tree, tree->root);
	}

	MEM_freeN(rad);
	MEM_freeN(tree->rad);
	tree->rad= sum;

	if(!re->cb.test_break(re->cb.tbh))
		occ_sum_occlusion(re, tree, tree->root);
}

static void occ_compute_passes(Render *re, OcclusionTree *tree, int totpass)
{
	float *occ, co[3], n[3];
	int pass, i;
	
	occ= MEM_callocN(sizeof(float)*tree->totface, "OcclusionPassOcc");

	for(pass=0; pass<totpass; pass++) {
		for(i=0; i<tree->totface; i++) {
			occ_face(re, &tree->face[i], co, n, NULL);
			negate_v3(n);
			madd_v3_v3v3fl(co, co, n, 1e-8f);

			occ_lookup(re, tree, 0, &tree->face[i], co, n, &occ[i], NULL, NULL);
			if(re->cb.test_break(re->cb.tbh))
				break;
		}

		if(re->cb.test_break(re->cb.tbh))
			break;

		for(i=0; i<tree->totface; i++) {
			tree->occlusion[i] -= occ[i]; //MAX2(1.0f-occ[i], 0.0f);
			if(tree->occlusion[i] < 0.0f)
				tree->occlusion[i]= 0.0f;
		}

		occ_sum_occlusion(re, tree, tree->root);
	}

	MEM_freeN(occ);
}

static void sample_occ_tree(Render *re, OcclusionTree *tree, OccFace *exclude, float *co, float *n, int thread, int onlyshadow, float *ao, float *env, float *indirect)
{
	float nn[3], bn[3], occ, occlusion, correction, rad[3];
	int envcolor;

	envcolor= re->db.wrld.aocolor;
	if(onlyshadow)
		envcolor= WO_ENV_LIGHT_WHITE;

	copy_v3_v3(nn, n);
	negate_v3(nn);

	occ_lookup(re, tree, thread, exclude, co, nn, &occ, (tree->doindirect)? rad: NULL, (env && envcolor)? bn: NULL);

	correction= re->db.wrld.ao_approx_correction;

	occlusion= (1.0f-correction)*(1.0f-occ);
	CLAMP(occlusion, 0.0f, 1.0f);
	if(correction != 0.0f)
		occlusion += correction*exp(-occ);

	if(env) {
		/* sky shading using bent normal */
		if(ELEM(envcolor, WO_ENV_LIGHT_SKY_COLOR, WO_ENV_LIGHT_SKY_TEX)) {
			environment_no_tex_shade(re, env, bn);
			mul_v3_fl(env, occlusion);
		}
		else {
			env[0]= occlusion;
			env[1]= occlusion;
			env[2]= occlusion;
		}
	}

	if(ao) {
		ao[0]= occlusion;
		ao[1]= occlusion;
		ao[2]= occlusion;
	}

	if(tree->doindirect) copy_v3_v3(indirect, rad);
	else zero_v3(indirect);
}

/* ------------------------- External Functions --------------------------- */

static void *exec_surface_cache_sample(void *data)
{
	OcclusionThread *othread= (OcclusionThread*)data;
	Render *re= othread->re;
	SurfaceCache *mesh= othread->mesh;
	float ao[3], env[3], indirect[3], co[3], n[3], *co1, *co2, *co3, *co4;
	int a, *face;

	for(a=othread->begin; a<othread->end; a++) {
		face= mesh->face[a];
		co1= mesh->co[face[0]];
		co2= mesh->co[face[1]];
		co3= mesh->co[face[2]];

		if(face[3]) {
			co4= mesh->co[face[3]];

			interp_v3_v3v3(co, co1, co3, 0.5f);
			normal_quad_v3( n,co1, co2, co3, co4);
		}
		else {
			cent_tri_v3(co, co1, co2, co3);
			normal_tri_v3( n,co1, co2, co3);
		}
		negate_v3(n);

		sample_occ_tree(re, re->db.occlusiontree, NULL, co, n, othread->thread, 0, ao, env, indirect);
		copy_v3_v3(othread->faceao[a], ao);
		copy_v3_v3(othread->faceenv[a], env);
		copy_v3_v3(othread->faceindirect[a], indirect);
	}

	return 0;
}

void disk_occlusion_create(Render *re)
{
	OcclusionThread othreads[BLENDER_MAX_THREADS];
	OcclusionTree *tree;
	SurfaceCache *mesh;
	ListBase threads;
	float ao[3], env[3], indirect[3], (*faceao)[3], (*faceenv)[3], (*faceindirect)[3];
	int a, totface, totthread, *face, *count;

	re->cb.i.infostr= "Occlusion preprocessing";
	re->cb.stats_draw(re->cb.sdh, &re->cb.i);
	
	re->db.occlusiontree= tree= occ_tree_build(re);
	
	if(tree) {
		if(re->db.wrld.ao_approx_passes > 0)
			occ_compute_passes(re, tree, re->db.wrld.ao_approx_passes);
		if(tree->doindirect && (re->db.wrld.mode & WO_INDIRECT_LIGHT))
			occ_compute_bounces(re, tree, re->db.wrld.ao_indirect_bounces);

		for(mesh=re->db.surfacecache.first; mesh; mesh=mesh->next) {
			if(!mesh->face || !mesh->co || !mesh->ao)
				continue;

			count= MEM_callocN(sizeof(int)*mesh->totvert, "OcclusionCount");
			faceao= MEM_callocN(sizeof(float)*3*mesh->totface, "StrandSurfFaceAO");
			faceenv= MEM_callocN(sizeof(float)*3*mesh->totface, "StrandSurfFaceEnv");
			faceindirect= MEM_callocN(sizeof(float)*3*mesh->totface, "StrandSurfFaceIndirect");

			totthread= (mesh->totface > 10000)? re->params.r.threads: 1;
			totface= mesh->totface/totthread;
			for(a=0; a<totthread; a++) {
				othreads[a].re= re;
				othreads[a].faceao= faceao;
				othreads[a].faceenv= faceenv;
				othreads[a].faceindirect= faceindirect;
				othreads[a].thread= a;
				othreads[a].mesh= mesh;
				othreads[a].begin= a*totface;
				othreads[a].end= (a == totthread-1)? mesh->totface: (a+1)*totface;
			}

			if(totthread == 1) {
				exec_surface_cache_sample(&othreads[0]);
			}
			else {
				BLI_init_threads(&threads, exec_surface_cache_sample, totthread);

				for(a=0; a<totthread; a++)
					BLI_insert_thread(&threads, &othreads[a]);

				BLI_end_threads(&threads);
			}

			for(a=0; a<mesh->totface; a++) {
				face= mesh->face[a];

                copy_v3_v3(ao, faceao[a]);
                copy_v3_v3(env, faceenv[a]);
                copy_v3_v3(indirect, faceindirect[a]);
  
                add_v3_v3(mesh->ao[face[0]], ao);
                add_v3_v3(mesh->env[face[0]], env);
                add_v3_v3(mesh->indirect[face[0]], indirect);
                count[face[0]]++;
                add_v3_v3(mesh->ao[face[1]], ao);
                add_v3_v3(mesh->env[face[1]], env);
                add_v3_v3(mesh->indirect[face[1]], indirect);
                count[face[1]]++;
                add_v3_v3(mesh->ao[face[2]], ao);
                add_v3_v3(mesh->env[face[2]], env);
                add_v3_v3(mesh->indirect[face[2]], indirect);
                count[face[2]]++;

                if(face[3]) {
                    add_v3_v3(mesh->ao[face[3]], ao);
                    add_v3_v3(mesh->env[face[3]], env);
                    add_v3_v3(mesh->indirect[face[3]], indirect);
                    count[face[3]]++;
                }
            }

            for(a=0; a<mesh->totvert; a++) {
                if(count[a]) {
                    mul_v3_fl(mesh->ao[a], 1.0f/count[a]);
                    mul_v3_fl(mesh->env[a], 1.0f/count[a]);
                    mul_v3_fl(mesh->indirect[a], 1.0f/count[a]);
                }
            }

            MEM_freeN(count);
            MEM_freeN(faceao);
            MEM_freeN(faceenv);
            MEM_freeN(faceindirect);
		}
	}
}

void disk_occlusion_free(RenderDB *rdb)
{
	if(rdb->occlusiontree) {
		disk_occlusion_free_tree(rdb->occlusiontree);
		rdb->occlusiontree = NULL;
	}
}

void disk_occlusion_sample_direct(Render *re, ShadeInput *shi)
{
	OcclusionTree *tree= re->db.occlusiontree;
	OccFace exclude;
	float *vn;
	int onlyshadow;

	onlyshadow= (shi->material.mat->mode & MA_ONLYSHADOW);
	exclude.obi= shi->primitive.obi - re->db.objectinstance;
	exclude.facenr= shi->primitive.vlr->index;

	if(re->db.wrld.ao_shading_method == WO_LIGHT_SHADE_NONE)
		vn= shi->geometry.vno;
	else
		vn= shi->geometry.vn;

	sample_occ_tree(re, tree, &exclude, shi->geometry.co, vn, shi->shading.thread, onlyshadow, shi->shading.ao, shi->shading.env, shi->shading.indirect);
}

void disk_occlusion_sample(Render *re, ShadeInput *shi)
{
	OcclusionTree *tree= re->db.occlusiontree;
	PixelCache *cache;

	if(tree) {
		if(shi->primitive.strand) {
			StrandRen *strand= shi->primitive.strand;
			surface_cache_sample(strand->buffer->surface, shi);
		}
		/* try to get result from the cache if possible */
		else if((shi->shading.depth > 0) ||
			    ((shi->material.mat->mode & MA_TRANSP) && (shi->material.mat->mode & MA_ZTRANSP)) ||
                !(tree->cache && tree->cache[shi->shading.thread] && pixel_cache_sample(tree->cache[shi->shading.thread], shi))) {

			/* no luck, let's sample the occlusion */
			disk_occlusion_sample_direct(re, shi);

			/* fill result into cache, each time */
			if(tree->cache && tree->cache[shi->shading.thread]) {
				cache= tree->cache[shi->shading.thread];
				pixel_cache_insert_sample(cache, shi);
			}
		}
	}
	else {
		shi->shading.ao[0]= 1.0f;
		shi->shading.ao[1]= 1.0f;
		shi->shading.ao[2]= 1.0f;

		zero_v3(shi->shading.env);
		zero_v3(shi->shading.indirect);
	}
}

void disk_occlusion_cache_create(Render *re, RenderPart *pa, ShadeSample *ssamp)
{
	OcclusionTree *tree= re->db.occlusiontree;

	if(tree->cache)
		tree->cache[pa->thread]= pixel_cache_create(re, pa, ssamp);
}

void disk_occlusion_cache_free(Render *re, RenderPart *pa)
{
	OcclusionTree *tree= re->db.occlusiontree;

	if(tree->cache)
		pixel_cache_free(tree->cache[pa->thread]);
}


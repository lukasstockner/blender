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
 * Contributor(s): Blender Foundation.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "DNA_material_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_memarena.h"
#include "BLI_rand.h"
#include "BLI_threads.h"

#include "BKE_DerivedMesh.h"
#include "BKE_global.h"

#include "cache.h"
#include "diskocclusion.h"
#include "object_strand.h"
#include "part.h"
#include "raytrace.h"
#include "rendercore.h"
#include "render_types.h"
#include "shading.h"

/******************************* Pixel Cache *********************************/

#define CACHE_STEP 3

static PixelCacheSample *find_sample(PixelCache *cache, int x, int y)
{
	x -= cache->x;
	y -= cache->y;

	x /= cache->step;
	y /= cache->step;
	x *= cache->step;
	y *= cache->step;

	if(x < 0 || x >= cache->w || y < 0 || y >= cache->h)
		return NULL;
	else
		return &cache->sample[y*cache->w + x];
}

int pixel_cache_sample(PixelCache *cache, ShadeInput *shi)
{
	PixelCacheSample *samples[4], *sample;
	float *co= shi->geometry.co;
	float *n= shi->geometry.vn;
	int x= shi->geometry.xs;
	int y= shi->geometry.ys;
	float *ao= shi->shading.ao;
	float *env= shi->shading.env;
	float *indirect= shi->shading.indirect;
	float wn[4], wz[4], wb[4], tx, ty, w, totw, mino, maxo;
	float d[3], dist2;
	int i, x1, y1, x2, y2;

	/* first try to find a sample in the same pixel */
	if(cache->sample && cache->step) {
		sample= &cache->sample[(y-cache->y)*cache->w + (x-cache->x)];
		if(sample->filled) {
			sub_v3_v3v3(d, sample->co, co);
			dist2= dot_v3v3(d, d);
			if(dist2 < 0.5f*sample->dist2 && dot_v3v3(sample->n, n) > 0.98f) {
				copy_v3_v3(ao, sample->ao);
				copy_v3_v3(env, sample->env);
				copy_v3_v3(indirect, sample->indirect);
				return 1;
			}
		}
	}
	else {
		return 0;
	}

	/* try to interpolate between 4 neighbouring pixels */
	samples[0]= find_sample(cache, x, y);
	samples[1]= find_sample(cache, x+cache->step, y);
	samples[2]= find_sample(cache, x, y+cache->step);
	samples[3]= find_sample(cache, x+cache->step, y+cache->step);

	for(i=0; i<4; i++)
		if(!samples[i] || !samples[i]->filled) {
			return 0;
		}

	/* require intensities not being too different */
	mino= MIN4(samples[0]->intensity, samples[1]->intensity, samples[2]->intensity, samples[3]->intensity);
	maxo= MAX4(samples[0]->intensity, samples[1]->intensity, samples[2]->intensity, samples[3]->intensity);

	if(maxo - mino > 0.05f) {
		return 0;
	}

	/* compute weighted interpolation between samples */
	zero_v3(ao);
	zero_v3(env);
	zero_v3(indirect);
	totw= 0.0f;

	x1= samples[0]->x;
	y1= samples[0]->y;
	x2= samples[3]->x;
	y2= samples[3]->y;

	tx= (float)(x2 - x)/(float)(x2 - x1);
	ty= (float)(y2 - y)/(float)(y2 - y1);

	wb[3]= (1.0f-tx)*(1.0f-ty);
	wb[2]= (tx)*(1.0f-ty);
	wb[1]= (1.0f-tx)*(ty);
	wb[0]= tx*ty;

	for(i=0; i<4; i++) {
		sub_v3_v3v3(d, samples[i]->co, co);
		dist2= dot_v3v3(d, d);

		wz[i]= 1.0f; //(samples[i]->dist2/(1e-4f + dist2));
		wn[i]= pow(dot_v3v3(samples[i]->n, n), 32.0f);

		w= wb[i]*wn[i]*wz[i];

		totw += w;
		madd_v3_v3fl(ao, samples[i]->ao, w);
		madd_v3_v3fl(env, samples[i]->env, w);
		madd_v3_v3fl(indirect, samples[i]->indirect, w);
	}

	if(totw >= 0.9f) {
		totw= 1.0f/totw;
		mul_v3_fl(ao, totw);
		mul_v3_fl(env, totw);
		mul_v3_fl(indirect, totw);
		return 1;
	}

	return 0;
}

PixelCache *pixel_cache_create(Render *re, RenderPart *pa, ShadeSample *ssamp)
{
	PixStr ps, **rd= NULL;
	PixelCache *cache;
	PixelCacheSample *sample;
	ShadeInput *shi;
	int *ro=NULL, *rp=NULL, *rz=NULL;
	int x, y, step = CACHE_STEP, offs;

	cache= MEM_callocN(sizeof(PixelCache), "PixelCache");
	cache->w= pa->rectx;
	cache->h= pa->recty;
	cache->x= pa->disprect.xmin;
	cache->y= pa->disprect.ymin;
	cache->step= step;
	cache->sample= MEM_callocN(sizeof(PixelCacheSample)*cache->w*cache->h, "PixelCacheSample");
	sample= cache->sample;

	if(re->params.osa) {
		rd= pa->rectdaps;
	}
	else {
		/* fake pixel struct for non-osa */
		ps.next= NULL;
		ps.mask= 0xFFFF;

		ro= pa->recto;
		rp= pa->rectp;
		rz= pa->rectz;
	}

	/* compute a sample at every step pixels */
	offs= 0;

	for(y=pa->disprect.ymin; y<pa->disprect.ymax; y++) {
		for(x=pa->disprect.xmin; x<pa->disprect.xmax; x++, sample++, rd++, ro++, rp++, rz++, offs++) {
			PixelRow row[MAX_PIXEL_ROW];
			int totrow;

			if(!(((x - pa->disprect.xmin + step) % step) == 0 || x == pa->disprect.xmax-1))
				continue;
			if(!(((y - pa->disprect.ymin + step) % step) == 0 || y == pa->disprect.ymax-1))
				continue;

			/* XXX test */
			totrow= pixel_row_fill(row, re, pa, offs);
			shade_samples_from_pixel(re, ssamp, &row[0], x, y);

			shi= ssamp->shi;
			if(shi->primitive.vlr) {
				disk_occlusion_sample_direct(re, shi);

				copy_v3_v3(sample->co, shi->geometry.co);
				copy_v3_v3(sample->n, shi->geometry.vno);
				copy_v3_v3(sample->ao, shi->shading.ao);
				copy_v3_v3(sample->env, shi->shading.env);
				copy_v3_v3(sample->indirect, shi->shading.indirect);
				sample->intensity= MAX3(sample->ao[0], sample->ao[1], sample->ao[2]);
				sample->intensity= MAX2(sample->intensity, MAX3(sample->env[0], sample->env[1], sample->env[2]));
				sample->intensity= MAX2(sample->intensity, MAX3(sample->indirect[0], sample->indirect[1], sample->indirect[2]));
				sample->dist2= dot_v3v3(shi->geometry.dxco, shi->geometry.dxco) + dot_v3v3(shi->geometry.dyco, shi->geometry.dyco);
				sample->x= shi->geometry.xs;
				sample->y= shi->geometry.ys;
				sample->filled= 1;
			}

			if(re->cb.test_break(re->cb.tbh))
				break;
		}
	}

	return cache;
}

void pixel_cache_free(PixelCache *cache)
{
	if(cache->sample)
		MEM_freeN(cache->sample);

	MEM_freeN(cache);
}

void pixel_cache_insert_sample(PixelCache *cache, ShadeInput *shi)
{
	PixelCacheSample *sample;

	if(!(cache->sample && cache->step))
		return;

	sample= &cache->sample[(shi->geometry.ys-cache->y)*cache->w + (shi->geometry.xs-cache->x)];
	copy_v3_v3(sample->co, shi->geometry.co);
	copy_v3_v3(sample->n, shi->geometry.vno);
	copy_v3_v3(sample->ao, shi->shading.ao);
	copy_v3_v3(sample->env, shi->shading.env);
	copy_v3_v3(sample->indirect, shi->shading.indirect);
	sample->intensity= MAX3(sample->ao[0], sample->ao[1], sample->ao[2]);
	sample->intensity= MAX2(sample->intensity, MAX3(sample->env[0], sample->env[1], sample->env[2]));
	sample->intensity= MAX2(sample->intensity, MAX3(sample->indirect[0], sample->indirect[1], sample->indirect[2]));
	sample->dist2= dot_v3v3(shi->geometry.dxco, shi->geometry.dxco) + dot_v3v3(shi->geometry.dyco, shi->geometry.dyco);
	sample->filled= 1;
}

/******************************* Surface Cache ********************************/

SurfaceCache *surface_cache_create(Render *re, ObjectRen *obr, DerivedMesh *dm, float mat[][4], int timeoffset)
{
	SurfaceCache *cache;
	MFace *mface;
	MVert *mvert;
	float (*co)[3];
	int a, totvert, totface;

	totvert= dm->getNumVerts(dm);
	totface= dm->getNumFaces(dm);

	for(cache=re->db.surfacecache.first; cache; cache=cache->next)
		if(cache->obr.ob == obr->ob && cache->obr.par == obr->par
			&& cache->obr.index == obr->index && cache->totvert==totvert && cache->totface==totface)
			break;

	if(!cache) {
		cache= MEM_callocN(sizeof(SurfaceCache), "SurfaceCache");
		cache->obr= *obr;
		cache->totvert= totvert;
		cache->totface= totface;
		cache->face= MEM_callocN(sizeof(int)*4*cache->totface, "StrandSurfFaces");
		cache->ao= MEM_callocN(sizeof(float)*3*cache->totvert, "StrandSurfAO");
		cache->env= MEM_callocN(sizeof(float)*3*cache->totvert, "StrandSurfEnv");
		cache->indirect= MEM_callocN(sizeof(float)*3*cache->totvert, "StrandSurfIndirect");
		BLI_addtail(&re->db.surfacecache, cache);
	}

	if(timeoffset == -1 && !cache->prevco)
		cache->prevco= co= MEM_callocN(sizeof(float)*3*cache->totvert, "StrandSurfCo");
	else if(timeoffset == 0 && !cache->co)
		cache->co= co= MEM_callocN(sizeof(float)*3*cache->totvert, "StrandSurfCo");
	else if(timeoffset == 1 && !cache->nextco)
		cache->nextco= co= MEM_callocN(sizeof(float)*3*cache->totvert, "StrandSurfCo");
	else
		return cache;

	mvert= dm->getVertArray(dm);
	for(a=0; a<cache->totvert; a++, mvert++) {
		copy_v3_v3(co[a], mvert->co);
		mul_m4_v3(mat, co[a]);
	}

	mface= dm->getFaceArray(dm);
	for(a=0; a<cache->totface; a++, mface++) {
		cache->face[a][0]= mface->v1;
		cache->face[a][1]= mface->v2;
		cache->face[a][2]= mface->v3;
		cache->face[a][3]= mface->v4;
	}

	return cache;
}

void surface_cache_free(RenderDB *rdb)
{
	SurfaceCache *cache;

	for(cache=rdb->surfacecache.first; cache; cache=cache->next) {
		if(cache->co) MEM_freeN(cache->co);
		if(cache->prevco) MEM_freeN(cache->prevco);
		if(cache->nextco) MEM_freeN(cache->nextco);
		if(cache->ao) MEM_freeN(cache->ao);
		if(cache->env) MEM_freeN(cache->env);
		if(cache->indirect) MEM_freeN(cache->indirect);
		if(cache->face) MEM_freeN(cache->face);
	}

	BLI_freelistN(&rdb->surfacecache);
}

void surface_cache_sample(SurfaceCache *cache, ShadeInput *shi)
{
	StrandRen *strand= shi->primitive.strand;
	int *face, *index = render_strand_get_face(shi->primitive.obr, strand, 0);
	float w[4], *co1, *co2, *co3, *co4;

	if(cache && cache->face && cache->co && cache->ao && index) {
		face= cache->face[*index];

		co1= cache->co[face[0]];
		co2= cache->co[face[1]];
		co3= cache->co[face[2]];
		co4= (face[3])? cache->co[face[3]]: NULL;

		interp_weights_face_v3(w, co1, co2, co3, co4, strand->vert->co);

		zero_v3(shi->shading.ao);
		zero_v3(shi->shading.env);
		zero_v3(shi->shading.indirect);

		madd_v3_v3fl(shi->shading.ao, cache->ao[face[0]], w[0]);
		madd_v3_v3fl(shi->shading.env, cache->env[face[0]], w[0]);
		madd_v3_v3fl(shi->shading.indirect, cache->indirect[face[0]], w[0]);
		madd_v3_v3fl(shi->shading.ao, cache->ao[face[1]], w[1]);
		madd_v3_v3fl(shi->shading.env, cache->env[face[1]], w[1]);
		madd_v3_v3fl(shi->shading.indirect, cache->indirect[face[1]], w[1]);
		madd_v3_v3fl(shi->shading.ao, cache->ao[face[2]], w[2]);
		madd_v3_v3fl(shi->shading.env, cache->env[face[2]], w[2]);
		madd_v3_v3fl(shi->shading.indirect, cache->indirect[face[2]], w[2]);
		if(face[3]) {
			madd_v3_v3fl(shi->shading.ao, cache->ao[face[3]], w[3]);
			madd_v3_v3fl(shi->shading.env, cache->env[face[3]], w[3]);
			madd_v3_v3fl(shi->shading.indirect, cache->indirect[face[3]], w[3]);
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

/******************************* Irradiance Cache ********************************/

/* Initial implementation based on Pixie (Copyright © 1999 - 2010, Okan Arikan) */

/* Relevant papers:
   [1] A Ray Tracing Solution for Diffuse Interreflection.
       Ward, Rubinstein, Clear. 1988.
   [2] Irradiance Gradients.
       Ward, Heckbert. 1992.
   [3] Approximate Global Illumination System for Computer Generated Films.
       Tabellion, Lamorlette. 2004.
   [4] Making Radiance and Irradiance Caching Practical: Adaptive Caching
       and Neighbor Clamping. Křivánek, Bouatouch, Pattanaik, Žára. 2006. */

#define EPSILON					1e-6f
#define MAX_PIXEL_DIST			10.0f
#define MAX_ERROR_K				1.0f
#define WEIGHT_NORMAL_DENOM		65.823047821929777f		/* 1/(1 - cos(10°)) */
#define SINGULAR_VALUE_EPSILON	1e-4f
#define MAX_CACHE_DIMENSION		((3+3+1)*9)

/* Data Structures */

typedef struct IrrCacheSample {
	/* cache sample in a node */
	struct IrrCacheSample *next;

	float P[3];						/* position */
	float N[3];						/* normal */
	float dP;						/* radius */
	int read;						/* read from file */
} IrrCacheSample;

typedef struct IrrCacheNode {
	/* node in the cache tree */
	float center[3];					/* center of node */
	float side;							/* max side length */
	IrrCacheSample *samples;			/* samples in the node */
	struct IrrCacheNode *children[8];	/* child nodes */
} IrrCacheNode;

struct IrrCache {
	/* irradiance cache */
	IrrCacheNode root;			/* root node of the tree */
	int maxdepth;				/* maximum tree dist */

	IrrCacheNode **stack;		/* stack for traversal */
	int stacksize;				/* stack size */

	MemArena *arena;			/* memory arena for nodes and samples */
	int thread;					/* thread owning the cache */

	int totsample;
	int totnode;
	int totlookup;
	int totpost;

	/* options */
	int neighbour_clamp;
	int lsq_reconstruction;
	int locked;
	int dimension;
	int use_SH;

	/* test: a stable global coordinate system may help */
};

typedef struct Lsq4DFit {
	float AtA[4][4];
	float AtB[MAX_CACHE_DIMENSION][4];
} Lsq4DFit;

void lsq_4D_add(Lsq4DFit *lsq, float a[4], float *b, int dimension, float weight)
{
	float (*AtA)[4]= lsq->AtA;
	float (*AtB)[4]= lsq->AtB;
	int i, j;

	/* build AtA and AtB directly from rows of A and
	   corresponding right hand side b */
	for(i=0; i<4; i++)
		for(j=0; j<4; j++)
			AtA[i][j] += weight*a[i]*a[j];
	
	for(i=0; i<dimension; i++)
		for(j=0; j<4; j++)
			AtB[i][j] += weight*a[j]*b[i];
}

void TNT_svd(float m[][4], float *w, float u[][4]);

void svd_invert_m4_m4(float R[4][4], float M[4][4])
{
	float V[4][4], W[4], Wm[4][4], U[4][4];

	copy_m4_m4(V, M);
	TNT_svd(V, W, U);

	zero_m4(Wm);
	Wm[0][0]= (W[0] < SINGULAR_VALUE_EPSILON)? 0.0f: 1.0f/W[0];
	Wm[1][1]= (W[1] < SINGULAR_VALUE_EPSILON)? 0.0f: 1.0f/W[1];
	Wm[2][2]= (W[2] < SINGULAR_VALUE_EPSILON)? 0.0f: 1.0f/W[2];
	Wm[3][3]= (W[3] < SINGULAR_VALUE_EPSILON)? 0.0f: 1.0f/W[3];

	transpose_m4(V);

	mul_serie_m4(R, U, Wm, V, 0, 0, 0, 0, 0);
}

void lsq_4D_solve(Lsq4DFit *lsq, float *solution, int dimension)
{
	float AtAinv[4][4], x[4];
	int i;

	svd_invert_m4_m4(AtAinv, lsq->AtA);

	for(i=0; i<dimension; i++) {
		mul_v4_m4v4(x, AtAinv, lsq->AtB[i]);
		solution[i]= x[3];
	}
}

/* Create and Free */

static IrrCache *irr_cache_new(Render *re, int thread)
{
	IrrCache *cache;
	float bb[2][3], viewbb[2][3];

	cache= MEM_callocN(sizeof(IrrCache), "IrrCache");
	cache->thread= thread;
	cache->maxdepth= 1;

	cache->arena= BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE);
	BLI_memarena_use_calloc(cache->arena);

	/* initialize root node with bounds */
	render_instances_bound(&re->db, viewbb);
	INIT_MINMAX(bb[0], bb[1]);
	box_minmax_bounds_m4(bb[0], bb[1], viewbb, re->cam.viewinv);
	mid_v3_v3v3(cache->root.center, bb[0], bb[1]);
	cache->root.side= MAX3(bb[1][0]-bb[0][0], bb[1][1]-bb[0][1], bb[1][2]-bb[0][2]);

	/* allocate stack */
	cache->stacksize= 10;
	cache->stack= MEM_mallocN(sizeof(IrrCacheNode*)*cache->stacksize*8, "IrrCache stack");

	/* options */
	cache->lsq_reconstruction= 1;
	cache->neighbour_clamp= 1;

	if(re->db.wrld.mode & WO_AMB_OCC)
		cache->dimension++;
	if(re->db.wrld.mode & WO_ENV_LIGHT)
		cache->dimension += 3;
	if(re->db.wrld.mode & WO_INDIRECT_LIGHT)
		cache->dimension += 3;

	if(re->db.wrld.ao_bump_method == WO_LIGHT_BUMP_APPROX) {
		cache->use_SH= 1;
		cache->dimension *= 9;
	}

	return cache;
}

static void irr_cache_delete(IrrCache *cache)
{
	if(G.f & G_DEBUG)
		printf("irr cache stats: %d samples, %d lookups, post %d, %.4f.\n", cache->totsample, cache->totlookup, cache->totpost, (float)cache->totsample/(float)cache->totlookup);

	BLI_memarena_free(cache->arena);
	MEM_freeN(cache->stack);
	MEM_freeN(cache);
}

void irr_cache_check_stack(IrrCache *cache)
{
	/* increase stack size as more nodes are added */
	if(cache->maxdepth > cache->stacksize) {
		cache->stacksize= cache->maxdepth + 5;
		if(cache->stack)
			MEM_freeN(cache->stack);
		cache->stack= MEM_mallocN(sizeof(IrrCacheNode*)*cache->stacksize*8, "IrrCache stack");
	}
}

static int irr_cache_node_point_inside(IrrCacheNode *node, float scale, float add, float P[3])
{
	float side= node->side*scale + add;

	return (((node->center[0] + side) > P[0]) &&
	        ((node->center[1] + side) > P[1]) &&
	        ((node->center[2] + side) > P[2]) &&
	        ((node->center[0] - side) < P[0]) &&
	        ((node->center[1] - side) < P[1]) &&
	        ((node->center[2] - side) < P[2]));
}

static float *irr_sample_C(IrrCacheSample *sample)
{
	return (float*)((char*)sample + sizeof(IrrCacheSample));
}

static void irr_sample_set(IrrCacheSample *sample, int use_SH, float *ao, float *env, float *indirect)
{
	float *C= irr_sample_C(sample);
	int offset= 0;

	if(!use_SH) {
		/* copy regular values */
		if(ao)
			C[offset++]= *ao;
		if(env) {
			C[offset++]= env[0];
			C[offset++]= env[1];
			C[offset++]= env[2];
		}
		if(indirect) {
			C[offset++]= indirect[0];
			C[offset++]= indirect[1];
			C[offset++]= indirect[2];
		}
	}
	else {
		/* copy spherical harmonics */
		if(ao)
			copy_sh_sh(&C[9*offset++], ao);
		if(env) {
			copy_sh_sh(&C[9*offset++], env);
			copy_sh_sh(&C[9*offset++], env+9);
			copy_sh_sh(&C[9*offset++], env+18);
		}
		if(indirect) {
			copy_sh_sh(&C[9*offset++], indirect);
			copy_sh_sh(&C[9*offset++], indirect+9);
			copy_sh_sh(&C[9*offset++], indirect+18);
		}
	}
}

static void irr_sample_get(float *C, int use_SH, float N[3], float *ao, float env[3], float indirect[3])
{
	int offset= 0;

	if(!use_SH) {
		/* simple copy from C array to ao/env/indirect */
		if(ao)
			*ao= C[offset++];
		if(env) {
			env[0]= C[offset++];
			env[1]= C[offset++];
			env[2]= C[offset++];
		}
		if(indirect) {
			indirect[0]= C[offset++];
			indirect[1]= C[offset++];
			indirect[2]= C[offset++];
		}
	}
	else {
		/* evaluate spherical harmonics using normal */
		if(ao)
			*ao= eval_shv3(&C[9*offset++], N);
		if(env) {
			env[0]= eval_shv3(&C[9*offset++], N);
			env[1]= eval_shv3(&C[9*offset++], N);
			env[2]= eval_shv3(&C[9*offset++], N);
		}
		if(indirect) {
			indirect[0]= eval_shv3(&C[9*offset++], N);
			indirect[1]= eval_shv3(&C[9*offset++], N);
			indirect[2]= eval_shv3(&C[9*offset++], N);
		}
	}
}

/* Neighbour Clamping [Křivánek 2006] */

static void irr_cache_clamp_sample(IrrCache *cache, IrrCacheSample *nsample)
{
	IrrCacheSample *sample;
	IrrCacheNode *node, **stack, **stacknode;
	float P[3];
	int i;

	copy_v3_v3(P, nsample->P);

	stack= cache->stack;
	stacknode= stack;

	*stacknode++= &cache->root;
	while(stacknode > stack) {
		node= *(--stacknode);

		/* sum the values in this level */
		for(sample=node->samples; sample; sample=sample->next) {
			float l;

			/* avoid issues with coincident points */
			l= len_squared_v3v3(sample->P, P);
			l= (l > EPSILON)? sqrtf(l): EPSILON;

			nsample->dP= minf(nsample->dP, sample->dP + l);
			sample->dP= minf(sample->dP, nsample->dP + l);
		}

		/* check the children */
		for(i=0; i<8; i++) {
			IrrCacheNode *tnode= node->children[i];

			if(tnode && irr_cache_node_point_inside(tnode, 4.0f, 0.0f, P))
				*stacknode++= tnode;
		}
	}
}

/* Insert Sample */

static void irr_cache_insert(IrrCache *cache, IrrCacheSample *sample)
{
	IrrCacheNode *node;
	float dist, P[3];
	int i, j, depth;

	copy_v3_v3(P, sample->P);

	/* error multiplier */
	dist = sample->dP/MAX_ERROR_K;
	
	/* insert the new sample into the cache */
	node= &cache->root;
	depth= 0;
	while(node->side > (2*dist)) {
		depth++;

		j= 0;
		for(i=0; i<3; i++)
			if(P[i] > node->center[i])
				j |= 1 << i;

		if(node->children[j] == NULL) {
			IrrCacheNode *nnode= BLI_memarena_alloc(cache->arena, sizeof(IrrCacheNode));

			for(i=0; i<3; i++) {
				float fac= (P[i] > node->center[i])? 0.25f: -0.25f;
				nnode->center[i]= node->center[i] + fac*node->side;
			}

			nnode->side= node->side*0.5f;
			node->children[j]= nnode;

			cache->totnode++;
		}

		node= node->children[j];
	}

	sample->next= node->samples;
	node->samples= sample;

	cache->maxdepth= MAX2(depth, cache->maxdepth);
	irr_cache_check_stack(cache);

	cache->totsample++;
}

/* Add a Sample */

static void irr_cache_add(Render *re, ShadeInput *shi, IrrCache *cache, float *ao, float env[3], float indirect[3], float P[3], float dPdu[3], float dPdv[3], float N[3], float bumpN[3])
{
	IrrCacheSample *sample;
	float Rmean, sh_ao[9], sh_env[27], sh_indirect[27];
	int use_SH= cache->use_SH;
	
	/* do raytracing */
	if(!use_SH)
		ray_ao_env_indirect(re, shi, ao, env, indirect, &Rmean, use_SH);
	else
		ray_ao_env_indirect(re, shi, (ao)? sh_ao: NULL, (env)? sh_env: NULL, (indirect)? sh_indirect: NULL, &Rmean, use_SH);

	/* save sample to cache? */
	if(!1) ///*(MAX_ERROR != 0) &&*/ (*ao > EPSILON)) { // XXX?
		return;

	sample= BLI_memarena_alloc(cache->arena, sizeof(IrrCacheSample) + sizeof(float)*cache->dimension);

	// XXX too large? try setting pixel dist to 0!
	/* compute the radius of validity [Tabellion 2004] */
	if(re->db.wrld.aomode & WO_LIGHT_CACHE_FILE)
		Rmean= Rmean*0.5f; /* XXX pixel dist is not reusable .. */
	else
		Rmean= minf(Rmean*0.5f, MAX_PIXEL_DIST*(len_v3(dPdu) + len_v3(dPdv))*0.5f);
	
	/* record the data */
	copy_v3_v3(sample->P, P);
	copy_v3_v3(sample->N, N);
	sample->dP= Rmean;

	/* copy color values */
	if(!use_SH) {
		irr_sample_set(sample, use_SH, ao, env, indirect);
	}
	else {
		irr_sample_set(sample, use_SH, (ao)? sh_ao: NULL, (env)? sh_env: NULL, (indirect)? sh_indirect: NULL);
		irr_sample_get(irr_sample_C(sample), use_SH, bumpN, ao, env, indirect);
	}

	/* neighbour clamping trick */
	if(cache->neighbour_clamp) {
		irr_cache_clamp_sample(cache, sample);
		Rmean= sample->dP; /* copy dP back so we get the right place in the octree */
	}

	/* insert into tree */
	irr_cache_insert(cache, sample);
}

/* Lookup */

int irr_cache_lookup(Render *re, ShadeInput *shi, IrrCache *cache, float *ao, float env[3], float indirect[3], float cP[3], float cdPdu[3], float cdPdv[3], float cN[3], float cbumpN[3], int preprocess)
{
	IrrCacheSample *sample;
	IrrCacheNode *node, **stack, **stacknode;
	float accum[MAX_CACHE_DIMENSION], P[3], dPdu[3], dPdv[3], N[3], bumpN[3], totw;
	float discard_weight, maxdist, distfac;
	int i, added= 0, totfound= 0, use_lsq, dimension;
	Lsq4DFit lsq;

	/* XXX check how often this is called! */
	/* XXX can identical samples end up in the cache now? */

	/* a small value for discard-smoothing of irradiance */
	discard_weight= (preprocess)? 0.1f: 0.0f;

	/* transform the lookup point to the correct coordinate system */
	copy_v3_v3(P, cP);
	copy_v3_v3(dPdu, cdPdu);
	copy_v3_v3(dPdv, cdPdv);

	/* normals depend on how we do bump mapping */
	if(re->db.wrld.ao_bump_method == WO_LIGHT_BUMP_NONE) {
		copy_v3_v3(N, cN);
		cbumpN= NULL;
	}
	else if(re->db.wrld.ao_bump_method == WO_LIGHT_BUMP_APPROX) {
		copy_v3_v3(N, cN);
		copy_v3_v3(bumpN, cbumpN);
	}
	else {
		copy_v3_v3(N, cbumpN);
		cbumpN= NULL;
	}

	negate_v3(N);
	if(cbumpN) negate_v3(bumpN); /* blender normal is negated */

	mul_m4_v3(re->cam.viewinv, P);
	mul_m3_v3(re->cam.viewninv, dPdu);
	mul_m3_v3(re->cam.viewninv, dPdv);
	mul_m3_v3(re->cam.viewninv, N);
	if(cbumpN)
		mul_m3_v3(re->cam.viewninv, bumpN);

	//print_m3("nm", re->cam.viewnmat);
	
	totw= 0.0f;

	/* setup tree traversal */
	stack= cache->stack;
	stacknode= stack;
	*stacknode++= &cache->root;

	use_lsq= cache->lsq_reconstruction;
	dimension= cache->dimension;

	if(use_lsq)
		memset(&lsq, 0, sizeof(lsq));
	else
		memset(accum, 0, sizeof(accum));

	/* the motivation for this factor is that in preprocess we only require
	   one sample for lookup not to fail, whereas for least squares
	   reconstruction we need more samples for a proper reconstruction. it's
	   quite arbitrary though and it would be good to have an actual
	   guarantee that we have enough samples for reconstruction */
	distfac= (preprocess)? 1.0f: 2.0f;

	maxdist= sqrtf(len_v3(dPdu)*len_v3(dPdv))*0.5f;
	maxdist *= MAX_PIXEL_DIST*distfac;

	while(stacknode > stack) {
		node= *(--stacknode);

		/* sum the values in this level */
		for(sample=node->samples; sample; sample=sample->next) {
			float D[3], a, e1, e2, w, dist;

			/* D vector from sample to query point */
			sub_v3_v3v3(D, sample->P, P);

			/* ignore sample in the front */
			a= dot_v3v3(D, sample->N);
			if((a*a/(dot_v3v3(D, D) + EPSILON)) > 0.1f)
				continue;

			dist= len_v3(D);

			if(dist > sample->dP*distfac)
				continue;

#if 0
			{
				/* error metric following [Tabellion 2004] */
				//float area= sqrtf(len_v3(dPdu)*len_v3(dPdv));
				float area= (len_v3(dPdu) + len_v3(dPdv))*0.5f;
				e1= sqrtf(dot_v3v3(D, D))/maxf(minf(sample->dP*0.5f, 10.0f*area), 1.5f*area);
			}
			e1= len_v3(D)/sample->dP;
#endif

			/* positional error */
			e1= dist/maxdist;

			/* directional error */
			e2= maxf(1.0f - dot_v3v3(N, sample->N), 0.0f);
			e2= sqrtf(e2*WEIGHT_NORMAL_DENOM);

			/* compute the weight */
			w= 1.0f - MAX_ERROR_K*maxf(e1, e2);

			if(w > BLI_thread_frand(cache->thread)*discard_weight) {
				if(!preprocess) {
					float *C= irr_sample_C(sample);

					if(use_lsq) {
						float a[4]= {D[0], D[1], D[2], 1.0f};
						lsq_4D_add(&lsq, a, C, dimension, w);
					}
					else {
						for(i=0; i<dimension; i++)
							accum[i] += w*C[i];
					}
				}

				totw += w;
				totfound++;
			}
		}

		/* check the children */
		for(i=0; i<8; i++) {
			IrrCacheNode *tnode= node->children[i];

			if(tnode && irr_cache_node_point_inside(tnode, 1.0f, maxdist, P))
				*stacknode++= tnode;
		}
	}

	/* do we have anything ? */
	if(totw > EPSILON && totfound >= 1) {
		if(!preprocess) {
			if(use_lsq) {
				lsq_4D_solve(&lsq, accum, dimension);
			}
			else {
				float invw= 1.0/totw;

				for(i=0; i<dimension; i++)
					accum[i] *= invw;
			}

			irr_sample_get(accum, cache->use_SH, bumpN, ao, env, indirect);
		}
	}
	else if(!cache->locked) {
		/* create a new sample */
		irr_cache_add(re, shi, cache, ao, env, indirect, P, dPdu, dPdv, N, bumpN);
		added= 1;

		if(!preprocess)
			cache->totpost++;
	}

	if(!preprocess)
		cache->totlookup++;

	/* make sure we don't have NaNs */
	// assert(dot_v3v3(C, C) >= 0);

	return added;
}

void irr_cache_create(Render *re, RenderPart *pa, RenderLayer *rl, ShadeSample *ssamp)
{
	RenderResult *rr= pa->result;
	IrrCache *cache;
	int crop, x, y, seed, step;
	
	if(!((re->db.wrld.aomode & WO_LIGHT_CACHE) && (re->db.wrld.mode & (WO_AMB_OCC|WO_ENV_LIGHT|WO_INDIRECT_LIGHT))))
		return;
	if((re->params.r.mode & R_RAYTRACE) == 0)
		return;

	//radio_cache_create(re, pa->thread);

	BLI_lock_thread(LOCK_RCACHE);
	if((re->db.wrld.aomode & WO_LIGHT_CACHE_FILE) && (cache=irr_cache_read(re, pa->thread))) {
		re->db.irrcache[pa->thread]= cache;
	}
	else {
		cache= irr_cache_new(re, pa->thread);
		re->db.irrcache[pa->thread]= cache;
	}
	BLI_unlock_thread(LOCK_RCACHE);

	if(cache->locked)
		return;

	seed= pa->rectx*pa->disprect.ymin;

	crop= 0;
	if(pa->crop)
		crop= 1;

	//step= MAX2(pa->disprect.ymax - pa->disprect.ymin + 2*crop, pa->disprect.xmax - pa->disprect.xmin + 2*crop);
	step= 1;

	while(step > 0) {
		rr->renrect.ymin= 0;
		rr->renrect.ymax= -2*crop;
		rr->renlay= rl;

		for(y=pa->disprect.ymin+crop; y<pa->disprect.ymax-crop; y+=step, rr->renrect.ymax++) {
			for(x=pa->disprect.xmin+crop; x<pa->disprect.xmax-crop; x+=step) {
				int lx = (x - pa->disprect.xmin);
				int ly = (y - pa->disprect.ymin);
				int od = lx + ly*(pa->disprect.xmax - pa->disprect.xmin);
				PixelRow row[MAX_PIXEL_ROW];
				int a, b, totrow;

				BLI_thread_srandom(pa->thread, seed++);

				/* create shade pixel row, sorted front to back */
				totrow= pixel_row_fill(row, re, pa, od);

				for(a=0; a<totrow; a++) {
					shade_samples_from_pixel(re, ssamp, &row[a], lx+pa->disprect.xmin, ly+pa->disprect.ymin);

					for(b=0; b<ssamp->tot; b++) {
						ShadeInput *shi= &ssamp->shi[b];
						ShadeGeometry *geom= &shi->geometry;
						float *ao= (re->db.wrld.mode & WO_AMB_OCC)? shi->shading.ao: NULL;
						float *env= (re->db.wrld.mode & WO_ENV_LIGHT)? shi->shading.env: NULL;
						float *indirect= (re->db.wrld.mode & WO_INDIRECT_LIGHT)? shi->shading.indirect: NULL;
						int added;

						added= irr_cache_lookup(re, shi, cache,
							ao, env, indirect,
							geom->co, geom->dxco, geom->dyco, geom->vno, geom->vn, 1);
						
						if(added) {
							if(indirect)
								add_v3_v3(rl->rectf + od*4, indirect);
							if(env)
								add_v3_v3(rl->rectf + od*4, env);
							if(ao)
								rl->rectf[od*4] += *ao;
						}
					}
				}

				if(re->cb.test_break(re->cb.tbh)) break;
			}
		}

		step /= 2;
	}

	memset(rl->rectf, 0, sizeof(float)*4*rl->rectx*rl->recty);
}

void irr_cache_free(Render *re, RenderPart *pa)
{
	IrrCache *cache= re->db.irrcache[pa->thread];

	if(cache) {
		if((re->db.wrld.aomode & WO_LIGHT_CACHE_FILE) && !re->cb.test_break(re->cb.tbh))
			if(!cache->locked)
				irr_cache_merge(re, cache);

		irr_cache_delete(cache);
		re->db.irrcache[pa->thread]= NULL;
	}

	radio_cache_free(&re->db, pa->thread);
}

#define CACHE_FILENAME "/tmp/cache.irr"

void irr_cache_write(Render *re, IrrCache *cache)
{
	FILE *f;
	IrrCacheNode *node, **stack, **stacknode;
	IrrCacheSample *sample;
	int i;
	
	f= fopen(CACHE_FILENAME, "w");

	/* write cache */
	fwrite(cache, sizeof(IrrCache), 1, f);

	/* write nodes */
	stack= cache->stack;
	stacknode= stack;

	*stacknode++= &cache->root;
	while(stacknode > stack) {
		node= *(--stacknode);

		/* XXX perhaps we should only write samples with a decent
		   minimum radius, or alternatively increase the radius */

		/* write samples */
		for(sample=node->samples; sample; sample=sample->next)
			fwrite(sample, sizeof(IrrCacheSample) + sizeof(float)*cache->dimension, 1, f);

		/* push children on stack */
		for(i=0; i<8; i++) {
			IrrCacheNode *tnode= node->children[i];

			if(tnode) {
				fwrite(tnode, sizeof(IrrCacheNode), 1, f);
				*stacknode++= tnode;
			}
		}
	}

	fclose(f);
}

IrrCache *irr_cache_read(Render *re, int thread)
{
	FILE *f;
	IrrCache *cache;
	IrrCacheNode *node, **stack, **stacknode;
	IrrCacheSample *sample, **sample_p;
	int i;

	f= fopen(CACHE_FILENAME, "r");
	if(!f)
		return NULL;

	/* read cache */
	cache= MEM_callocN(sizeof(IrrCache), "IrrCache");

	if(fread(cache, sizeof(IrrCache), 1, f) != 1) {
		MEM_freeN(cache);
		fclose(f);
		return NULL;
	}

	cache->thread= thread;

	cache->arena= BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE);
	BLI_memarena_use_calloc(cache->arena);

	cache->stack= NULL;
	cache->stacksize= 0;
	irr_cache_check_stack(cache);

	//cache->locked= 1;

	/* read nodes */
	stack= cache->stack;
	stacknode= stack;

	*stacknode++= &cache->root;
	while(stacknode > stack) {
		node= *(--stacknode);

		/* read samples */
		sample_p= &node->samples;
		for(sample=node->samples; sample; sample=sample->next) {
			int samplesize= sizeof(IrrCacheSample) + sizeof(float)*cache->dimension;
			*sample_p= sample= BLI_memarena_alloc(cache->arena, samplesize);
			if(fread(sample, samplesize, 1, f) != 1) {
				fclose(f);
				irr_cache_delete(cache);
				return NULL;
			}

			sample->read= 1;
			sample_p= &sample->next;
		}

		/* push children on stack */
		for(i=0; i<8; i++) {
			IrrCacheNode *tnode= node->children[i];

			if(tnode) {
				node->children[i]= tnode= BLI_memarena_alloc(cache->arena, sizeof(IrrCacheNode));
				if(fread(tnode, sizeof(IrrCacheNode), 1, f) != 1) {
					fclose(f);
					irr_cache_delete(cache);
					return NULL;
				}

				*stacknode++= tnode;
			}
		}
	}

	fclose(f);

	return cache;
}

void irr_cache_merge(Render *re, IrrCache *from)
{
	IrrCache *to;
	IrrCacheNode *node, **stack, **stacknode;
	IrrCacheSample *sample, *newsample;
	int i;

	BLI_lock_thread(LOCK_RCACHE);
	to= irr_cache_read(re, from->thread);

	if(!to) {
		irr_cache_write(re, from);
		BLI_unlock_thread(LOCK_RCACHE);
		return;
	}

	stack= from->stack;
	stacknode= stack;

	*stacknode++= &from->root;
	while(stacknode > stack) {
		node= *(--stacknode);

		/* insert samples */
		for(sample=node->samples; sample; sample=sample->next) {
			if(!sample->read) {
				int samplesize= sizeof(IrrCacheSample) + sizeof(float)*from->dimension;
				newsample= BLI_memarena_alloc(to->arena, samplesize);
				memcpy(newsample, sample, samplesize);
				irr_cache_insert(to, newsample);
			}
		}

		/* push children on stack */
		for(i=0; i<8; i++) {
			IrrCacheNode *tnode= node->children[i];

			if(tnode)
				*stacknode++= tnode;
		}
	}

	printf("final %d %d (+= %d)\n", to->totsample, to->totnode, from->totsample);

	irr_cache_write(re, to);
	irr_cache_delete(to);

	BLI_unlock_thread(LOCK_RCACHE);
}

/****************************** Radiosity Cache ******************************/

#if 0
#define RADIO_CACHE_MAX_CHILD	8

/* Data Structures */

typedef struct RadioCacheSample {
	/* cache sample in a node */
	struct RadioCacheSample *next;

	float P[3];
	float C[3];
} RadioCacheSample;

typedef struct RadioCacheNode {
	/* node in the cache tree */
	float center[3];					/* center of node */
	float side;							/* max side length */
	RadioCacheSample *samples;			/* samples in the node */
	struct RadioCacheNode *children[8];	/* child nodes */
	int totsample;
} RadioCacheNode;

typedef struct RadioCache {
	/* radioadiance cache */
	RadioCacheNode root;			/* root node of the tree */
	int maxdepth;				/* maximum tree dist */

	RadioCacheNode **stack;		/* stack for traversal */
	int stacksize;				/* stack size */

	MemArena *arena;			/* memory arena for nodes and samples */
	int thread;					/* thread owning the cache */

	int totsample;
	int totlookup;
} RadioCache;

/* Create and Free */

void radio_cache_create(Render *re, int thread)
{
	RadioCache *cache;
	float bb[2][3];

	cache= MEM_callocN(sizeof(RadioCache), "RadioCache");
	cache->thread= thread;
	cache->maxdepth= 1;

	cache->arena= BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE);
	BLI_memarena_use_calloc(cache->arena);

	/* initialize root node with bounds */
	render_instances_bound(&re->db, bb);
	mid_v3_v3v3(cache->root.center, bb[0], bb[1]);
	cache->root.side= MAX3(bb[1][0]-bb[0][0], bb[1][1]-bb[0][1], bb[1][2]-bb[0][2]);

	/* allocate stack */
	cache->stacksize= 10;
	cache->stack= MEM_mallocN(sizeof(RadioCacheNode*)*cache->stacksize*8, "RadioCache stack");

	re->db.radiocache[thread]= cache;
}

void radio_cache_free(RenderDB *rdb, int thread)
{
	RadioCache *cache= rdb->radiocache[thread];

	if(cache) {
		printf("radio cache %d/%d, efficiency %f\n", cache->totsample, cache->totlookup, (float)cache->totsample/(float)cache->totlookup);

		BLI_memarena_free(cache->arena);
		MEM_freeN(cache->stack);
		MEM_freeN(cache);
		
		rdb->radiocache[thread]= NULL;
	}
}

void radio_cache_check_stack(RadioCache *cache)
{
	/* increase stack size as more nodes are added */
	if(cache->maxdepth > cache->stacksize) {
		cache->stacksize= cache->maxdepth + 5;
		MEM_freeN(cache->stack);
		cache->stack= MEM_mallocN(sizeof(RadioCacheNode*)*cache->stacksize*8, "RadioCache stack");
	}
}

static int radio_cache_node_point_inside(RadioCacheNode *node, float scale, float add, float P[3])
{
	float side= node->side*scale + add;

	return (((node->center[0] + side) > P[0]) &&
	        ((node->center[1] + side) > P[1]) &&
	        ((node->center[2] + side) > P[2]) &&
	        ((node->center[0] - side) < P[0]) &&
	        ((node->center[1] - side) < P[1]) &&
	        ((node->center[2] - side) < P[2]));
}

void radio_cache_add(Render *re, ShadeInput *shi, float C[3])
{
	RadioCache *cache= re->db.radiocache[shi->shading.thread];
	RadioCacheSample *sample;
	RadioCacheNode *node;
	float P[3];
	int i, j, depth;

	if(!cache)
		return;

	sample= BLI_memarena_alloc(cache->arena, sizeof(RadioCacheSample));

	/* record the data */
	copy_v3_v3(P, shi->geometry.co);
	copy_v3_v3(sample->P, P);
	copy_v3_v3(sample->C, C);

	/* insert the new sample into the cache */
	node= &cache->root;
	depth= 0;
	while(node->totsample > RADIO_CACHE_MAX_CHILD) {
		depth++;

		j= 0;
		for(i=0; i<3; i++)
			if(P[i] > node->center[i])
				j |= 1 << i;

		if(node->children[j] == NULL) {
			RadioCacheNode *nnode= BLI_memarena_alloc(cache->arena, sizeof(RadioCacheNode));

			for(i=0; i<3; i++) {
				float fac= (P[i] > node->center[i])? 0.25f: -0.25f;
				nnode->center[i]= node->center[i] + fac*node->side;
			}

			nnode->side= node->side*0.5f;
			node->children[j]= nnode;
		}

		node= node->children[j];
	}

	sample->next= node->samples;
	node->samples= sample;
	node->totsample++;

	cache->maxdepth= MAX2(depth, cache->maxdepth);
	radio_cache_check_stack(cache);

	cache->totsample++;
}

/* Lookup */

int radio_cache_lookup(Render *re, ShadeInput *shi, float C[3], float raylength)
{
	RadioCache *cache= re->db.radiocache[shi->shading.thread];
	RadioCacheSample *sample;
	RadioCacheNode *node, **stack, **stacknode;
	float P[3], accum[3], totw, maxdist;
	int i, totfound= 0;

	if(!cache)
		return 0;

	cache->totlookup++;

	/* transform the lookup point to the correct coordinate system */
	copy_v3_v3(P, shi->geometry.co);
	
	/* setup tree traversal */
	stack= cache->stack;
	stacknode= stack;
	*stacknode++= &cache->root;

	zero_v3(accum);
	totw= 0.0f;

	raylength= maxf(0.01, raylength);
	maxdist= 1000.0f*(raylength*raylength);

	while(stacknode > stack) {
		node= *(--stacknode);

		/* sum the values in this level */
		for(sample=node->samples; sample; sample=sample->next) {
			float e1, w, dist;

			/* positional error */
			dist= len_v3v3(P, sample->P);
			e1= dist/maxdist;
			w= 1.0f - e1;

			if(w > 0.0f) {
				madd_v3_v3fl(accum, sample->C, w);
				totw += w;
				totfound++;
			}
		}

		/* check the children */
		for(i=0; i<8; i++) {
			RadioCacheNode *tnode= node->children[i];

			if(tnode && radio_cache_node_point_inside(tnode, 1.0f, maxdist, P))
				*stacknode++= tnode;
		}
	}

	/* do we have anything ? */
	if(totw > EPSILON && totfound >= 1) {
		mul_v3_v3fl(C, accum, 1.0f/totw);
		return 1;
	}

	return 0;
}
#endif

#include "BLI_ghash.h"
#include "object_mesh.h"

typedef struct RadioCache {
	GHash *hash;
	MemArena *arena;
	int thread;
} RadioCache;

void radio_cache_create(Render *re, int thread)
{
	RadioCache *cache;
	
	cache= MEM_callocN(sizeof(RadioCache), "RadioCache");
	re->db.radiocache[thread]= cache;

	cache->hash= BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp);
	cache->arena= BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE);
}

void radio_cache_free(RenderDB *rdb, int thread)
{
	RadioCache *cache= rdb->radiocache[thread];

	if(cache) {
		BLI_memarena_free(cache->arena);
		BLI_ghash_free(cache->hash, NULL, NULL);
	}
}

#if 0
static float face_projected_area(RenderCamera *cam, VlakRen *vlr)
{
	float area, hoco[4][4], zco[4][3];
	int a;

	/* instances .. */
	for(a=0; a<3; a++) {
		camera_matrix_co_to_hoco(cam->winmat, hoco[a], ((VertRen**)&(vlr->v1))[a]->co);
		camera_hoco_to_zco(cam, zco[a], hoco[a]);
	}

	area= area_tri_v3(zco[0], zco[1], zco[2]);

	return area;
}
#endif

int radio_cache_lookup(Render *re, ShadeInput *shi, float color[3], float raylength)
{
	RadioCache *cache= re->db.radiocache[shi->shading.thread];

	if(cache) {
		float *store= BLI_ghash_lookup(cache->hash, shi->primitive.vlr);

		if(store) {
			copy_v3_v3(color, store);
			return 1;
		}
	}

	return 0;
}

void radio_cache_add(Render *re, ShadeInput *shi, float color[3])
{
	RadioCache *cache= re->db.radiocache[shi->shading.thread];

	if(cache) {
		float *store= BLI_memarena_alloc(cache->arena, sizeof(float)*3);

		copy_v3_v3(store, color);
		BLI_ghash_insert(cache->hash, shi->primitive.vlr, store);
	}
}


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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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
				//printf("success A\n");
				return 1;
			}
		}
	}
	else {
		//printf("fail A\n");
		return 0;
	}

	/* try to interpolate between 4 neighbouring pixels */
	samples[0]= find_sample(cache, x, y);
	samples[1]= find_sample(cache, x+cache->step, y);
	samples[2]= find_sample(cache, x, y+cache->step);
	samples[3]= find_sample(cache, x+cache->step, y+cache->step);

	for(i=0; i<4; i++)
		if(!samples[i] || !samples[i]->filled) {
			//printf("fail B\n");
			return 0;
		}

	/* require intensities not being too different */
	mino= MIN4(samples[0]->intensity, samples[1]->intensity, samples[2]->intensity, samples[3]->intensity);
	maxo= MAX4(samples[0]->intensity, samples[1]->intensity, samples[2]->intensity, samples[3]->intensity);

	if(maxo - mino > 0.05f) {
		//printf("fail B\n");
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
		//printf("success B\n");
		return 1;
	}

	//printf("fail C\n");
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
#define INFINITY				1e30f
#define CACHE_DIMENSION			(1+3+3)
#define MAX_PIXEL_DIST			10.0f
#define MAX_ERROR_K				1.0f
#define WEIGHT_NORMAL_DENOM		65.823047821929777f		/* 1/(1 - cos(10°)) */
#define HORIZON_CUTOFF			0.17364817766693041f	/* cos(80°) */
#define LSQ_RECONSTRUCTION

/* Data Structures */

typedef struct IrrCacheSample {
	/* cache sample in a node */
	struct IrrCacheSample *next;

	float P[3];						/* position */
	float N[3];						/* normal */
	float dP;						/* radius */
	float C[CACHE_DIMENSION];		/* irradiance */
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

	/* test: a stable global coordinate system may help */
	int totsample;
	int totlookup;
	int totpost;
};

typedef struct Lsq4DFit {
	float AtA[4][4];
	float AtB[CACHE_DIMENSION][4];
} Lsq4DFit;

void lsq_4D_add(Lsq4DFit *lsq, float a[4], float b[CACHE_DIMENSION], float weight)
{
	float (*AtA)[4]= lsq->AtA;
	float (*AtB)[4]= lsq->AtB;
	int i, j;

	/* build AtA and AtB directly from rows of A and
	   corresponding right hand side b */
	for(i=0; i<4; i++)
		for(j=0; j<4; j++)
			AtA[i][j] += weight*a[i]*a[j];
	
	for(i=0; i<CACHE_DIMENSION; i++)
		for(j=0; j<4; j++)
			AtB[i][j] += weight*a[j]*b[i];
}

void TNT_svd(float m[][4], float *w, float u[][4]);

int svd_invert_m4_m4(float R[4][4], float M[4][4])
{
	float V[4][4], W[4], Wm[4][4], U[4][4];

	copy_m4_m4(V, M);
	TNT_svd(V, W, U);

	zero_m4(Wm);
	Wm[0][0]= (W[0] < 1e-6f)? 0.0f: 1.0f/W[0];
	Wm[1][1]= (W[1] < 1e-6f)? 0.0f: 1.0f/W[1];
	Wm[2][2]= (W[2] < 1e-6f)? 0.0f: 1.0f/W[2];
	Wm[3][3]= (W[3] < 1e-6f)? 0.0f: 1.0f/W[3];

	transpose_m4(V);

	mul_serie_m4(R, U, Wm, V, 0, 0, 0, 0, 0);

	return 1;
}

void lsq_4D_solve(Lsq4DFit *lsq, float solution[CACHE_DIMENSION])
{
	float AtAinv[4][4], x[4];
	int i;

	svd_invert_m4_m4(AtAinv, lsq->AtA);

	for(i=0; i<CACHE_DIMENSION; i++) {
		mul_v4_m4v4(x, AtAinv, lsq->AtB[i]);
		solution[i]= x[3];
	}
}

/* Create and Free */

static IrrCache *irr_cache_new(Render *re, int thread)
{
	IrrCache *cache;
	float bb[2][3];

	cache= MEM_callocN(sizeof(IrrCache), "IrrCache");
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
	cache->stack= MEM_mallocN(sizeof(IrrCacheNode*)*cache->stacksize*8, "IrrCache stack");

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

static void irr_sample_set(float C[CACHE_DIMENSION], float *ao, float env[3], float indirect[3])
{
	if(ao) C[0]= *ao;
	if(env) copy_v3_v3(C+1, env);
	if(indirect) copy_v3_v3(C+4, indirect);
}

static void irr_sample_get(float C[CACHE_DIMENSION], float *ao, float env[3], float indirect[3])
{
	if(ao) *ao= C[0];
	if(env) copy_v3_v3(env, C+1);
	if(indirect) copy_v3_v3(indirect, C+4);
}

/* Add a Sample */

static void irr_cache_add(Render *re, ShadeInput *shi, IrrCache *cache, float *ao, float env[3], float indirect[3], float P[3], float dPdu[3], float dPdv[3], float N[3])
{
	IrrCacheSample *sample;
	IrrCacheNode *node;
	float Rmean;
	int i, j, depth;
	
	ray_ao_env_indirect(re, shi, ao, env, indirect, &Rmean);

	/* save sample to cache? */
	if(1) { ///*(MAX_ERROR != 0) &&*/ (*ao > EPSILON)) { // XXX?
		sample= BLI_memarena_alloc(cache->arena, sizeof(IrrCacheSample));

		// XXX too large? try setting pixel dist to 0!
		/* compute the radius of validity [Tabellion 2004] */
		Rmean= minf(Rmean*0.5f, MAX_PIXEL_DIST*(len_v3(dPdu) + len_v3(dPdv))*0.5f);
		
		/* record the data */
		copy_v3_v3(sample->P, P);
		copy_v3_v3(sample->N, N);
		sample->dP= Rmean;
		irr_sample_set(sample->C, ao, env, indirect);

		/* error multiplier */
		Rmean /= MAX_ERROR_K;
		
		/* insert the new sample into the cache */
		node= &cache->root;
		depth= 0;
		while(node->side > (2*Rmean)) {
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
			}

			node= node->children[j];
		}

		sample->next= node->samples;
		node->samples= sample;

		cache->maxdepth= MAX2(depth, cache->maxdepth);
		irr_cache_check_stack(cache);

		cache->totsample++;
	}
}

/* Lookup */

int irr_cache_lookup(Render *re, ShadeInput *shi, IrrCache *cache, float *ao, float env[3], float indirect[3], float cP[3], float dPdu[3], float dPdv[3], float cN[3], int preprocess)
{
	IrrCacheSample *sample;
	IrrCacheNode *node, **stack, **stacknode;
	float accum[CACHE_DIMENSION], P[3], N[3], totw;
	float discard_weight, maxdist;
	int i, added= 0, totfound= 0;
#ifdef LSQ_RECONSTRUCTION
	Lsq4DFit lsq;
#endif

	/* XXX check how often this is called! */

	/* a small value for discard-smoothing of irradiance */
	discard_weight= (preprocess)? 0.1f: 0.0f;

	/* transform the lookup point to the correct coordinate system */
	copy_v3_v3(P, cP);
	copy_v3_v3(N, cN);
	negate_v3(N); /* blender normal is negated */
	
	totw= 0.0f;

	/* setup tree traversal */
	stack= cache->stack;
	stacknode= stack;
	*stacknode++= &cache->root;

#ifdef LSQ_RECONSTRUCTION
	memset(&lsq, 0, sizeof(lsq));
#else
	memset(accum, 0, sizeof(accum));
#endif

	maxdist= sqrtf(len_v3(dPdu)*len_v3(dPdv)); //*0.5f);
	maxdist *= (preprocess? MAX_PIXEL_DIST/2: MAX_PIXEL_DIST);

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

			if(preprocess && !(dist < sample->dP))
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
#ifdef LSQ_RECONSTRUCTION
					float a[4]= {D[0], D[1], D[2], 1.0f};

					lsq_4D_add(&lsq, a, sample->C, w);
#else
					for(i=0; i<CACHE_DIMENSION; i++)
						accum[i] += w*sample->C[i];
#endif
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
#ifdef LSQ_RECONSTRUCTION
			lsq_4D_solve(&lsq, accum);
#else
			float invw= 1.0/totw;

			for(i=0; i<CACHE_DIMENSION; i++)
				accum[i] *= invw;
#endif

			irr_sample_get(accum, ao, env, indirect);
		}
	}
	else {
		/* create a new sample */
		irr_cache_add(re, shi, cache, ao, env, indirect, P, dPdu, dPdv, N);
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
	int offs, crop, x, y, seed;
	
	if(!((re->db.wrld.aomode & WO_AOCACHE) && (re->db.wrld.mode & (WO_AMB_OCC|WO_ENV_LIGHT|WO_INDIRECT_LIGHT))))
		return;

	cache= irr_cache_new(re, pa->thread);
	re->db.irrcache[pa->thread]= cache;

	seed= pa->rectx*pa->disprect.ymin;

	offs= 0;
	crop= 0;
	if(pa->crop) {
		crop= 1;
		offs= pa->rectx + 1;
	}

	rr->renrect.ymin= 0;
	rr->renrect.ymax= -2*crop;
	rr->renlay= rl;

	for(y=pa->disprect.ymin+crop; y<pa->disprect.ymax-crop; y++, rr->renrect.ymax++) {
		for(x=pa->disprect.xmin+crop; x<pa->disprect.xmax-crop; x++) {
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
						geom->co, geom->dxco, geom->dyco, geom->vn, 1);
					
					if(added) {
						if(indirect)
							copy_v3_v3(rl->rectf + od*4, indirect);
						else if(env)
							copy_v3_v3(rl->rectf + od*4, env);
						else if(ao)
							rl->rectf[od*4]= *ao;
					}
				}
			}

			offs++;
		}

		offs+= 2*crop;

		if(re->cb.test_break(re->cb.tbh)) break;
	}

	memset(rl->rectf, 0, sizeof(float)*4*rl->rectx*rl->recty);
}

void irr_cache_free(Render *re, RenderPart *pa)
{
	IrrCache *cache= re->db.irrcache[pa->thread];

	if(cache) {
		irr_cache_delete(cache);
		re->db.irrcache[pa->thread]= NULL;
	}
}


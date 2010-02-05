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

#include "DNA_material_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_listbase.h"
#include "BLI_math.h"

#include "BKE_DerivedMesh.h"

#include "cache.h"
#include "diskocclusion.h"
#include "object_strand.h"
#include "part.h"
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
	else
		return 0;

	/* try to interpolate between 4 neighbouring pixels */
	samples[0]= find_sample(cache, x, y);
	samples[1]= find_sample(cache, x+cache->step, y);
	samples[2]= find_sample(cache, x, y+cache->step);
	samples[3]= find_sample(cache, x+cache->step, y+cache->step);

	for(i=0; i<4; i++)
		if(!samples[i] || !samples[i]->filled)
			return 0;

	/* require intensities not being too different */
	mino= MIN4(samples[0]->intensity, samples[1]->intensity, samples[2]->intensity, samples[3]->intensity);
	maxo= MAX4(samples[0]->intensity, samples[1]->intensity, samples[2]->intensity, samples[3]->intensity);

	if(maxo - mino > 0.05f)
		return 0;

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
	PixStr ps;
	PixelCache *cache;
	PixelCacheSample *sample;
	ShadeInput *shi;
	void **rd=NULL;
	int *ro=NULL, *rp=NULL, *rz=NULL;
	int x, y, step = CACHE_STEP;

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
	for(y=pa->disprect.ymin; y<pa->disprect.ymax; y++) {
		for(x=pa->disprect.xmin; x<pa->disprect.xmax; x++, sample++, rd++, ro++, rp++, rz++) {
			if(!(((x - pa->disprect.xmin + step) % step) == 0 || x == pa->disprect.xmax-1))
				continue;
			if(!(((y - pa->disprect.ymin + step) % step) == 0 || y == pa->disprect.ymax-1))
				continue;

			if(re->params.osa) {
				if(!*rd) continue;

				shade_samples_from_ps(re, ssamp, (PixStr *)(*rd), x, y);
			}
			else {
				if(!*rp) continue;

				ps.obi= *ro;
				ps.facenr= *rp;
				ps.z= *rz;
				shade_samples_from_ps(re, ssamp, &ps, x, y);
			}

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


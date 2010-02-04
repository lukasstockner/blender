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

#include <math.h>
#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_rand.h"

#include "sampler.h"
#include "render_types.h"

struct QMCSampler {
	struct QMCSampler *next, *prev;

	int type;
	int tot;
	int used;
	int thread;
	double *samp2d;
	double offs[BLENDER_MAX_THREADS][2];
};

/********************************* QMC sampling ******************************/

static void halton_sample(double *ht_invprimes, double *ht_nums, double *v)
{
	// incremental halton sequence generator, from:
	// "Instant Radiosity", Keller A.
	unsigned int i;
	
	for(i = 0; i < 2; i++) {
		double r = fabs((1.0 - ht_nums[i]) - 1e-10);
		
		if(ht_invprimes[i] >= r) {
			double lasth;
			double h = ht_invprimes[i];
			
			do {
				lasth = h;
				h *= ht_invprimes[i];
			} while (h >= r);
			
			ht_nums[i] += ((lasth + h) - 1.0);
		}
		else
			ht_nums[i] += ht_invprimes[i];
		
		v[i] = (float)ht_nums[i];
	}
}

/* Generate Hammersley points in [0,1)^2
 * From Lucille renderer */
static void hammersley_create(double *out, int n)
{
	double p, t;
	int k, kk;

	for(k = 0; k < n; k++) {
		t = 0;
		for(p = 0.5, kk = k; kk; p *= 0.5, kk >>= 1)
			if(kk & 1)		/* kk mod 2 = 1		*/
				t += p;
	
		out[2 * k + 0] = (double)k / (double)n;
		out[2 * k + 1] = t;
	}
}

static QMCSampler *qmc_sampler_init(int type, int tot)
{	
	QMCSampler *qsa = MEM_callocN(sizeof(QMCSampler), "qmc sampler");
	qsa->samp2d = MEM_callocN(2*sizeof(double)*tot, "qmc sample table");

	qsa->tot = tot;
	qsa->type = type;
	
	if(qsa->type==SAMP_TYPE_HAMMERSLEY) 
		hammersley_create(qsa->samp2d, qsa->tot);
		
	return qsa;
}

static void qmc_sampler_init_pixel(QMCSampler *qsa, int thread)
{
	if(qsa->type==SAMP_TYPE_HAMMERSLEY) {
		/* hammersley sequence is fixed, already created in QMCSampler init.
		 * per pixel, gets a random offset. We create separate offsets per thread, for write-safety */
		qsa->offs[thread][0] = 0.5 * BLI_thread_frand(thread);
		qsa->offs[thread][1] = 0.5 * BLI_thread_frand(thread);
	}
	else { /* SAMP_TYPE_HALTON */
		
		/* generate a new randomised halton sequence per pixel
		 * to alleviate qmc artifacts and make it reproducable 
		 * between threads/frames */
		double ht_invprimes[2], ht_nums[2];
		double r[2];
		int i;
	
		ht_nums[0] = BLI_thread_frand(thread);
		ht_nums[1] = BLI_thread_frand(thread);
		ht_invprimes[0] = 0.5;
		ht_invprimes[1] = 1.0/3.0;
		
		for(i=0; i< qsa->tot; i++) {
			halton_sample(ht_invprimes, ht_nums, r);
			qsa->samp2d[2*i+0] = r[0];
			qsa->samp2d[2*i+1] = r[1];
		}
	}
}

static void qmc_sampler_free(QMCSampler *qsa)
{
	MEM_freeN(qsa->samp2d);
	MEM_freeN(qsa);
}

void sampler_get_double_2d(double s[2], QMCSampler *qsa, int num)
{
	if(qsa->type == SAMP_TYPE_HAMMERSLEY) {
		int thread= qsa->thread;

		s[0] = fmod(qsa->samp2d[2*num+0] + qsa->offs[thread][0], 1.0f);
		s[1] = fmod(qsa->samp2d[2*num+1] + qsa->offs[thread][1], 1.0f);
	}
	else { /* SAMP_TYPE_HALTON */
		s[0] = qsa->samp2d[2*num+0];
		s[1] = qsa->samp2d[2*num+1];
	}
}

void sampler_get_float_2d(float s[2], QMCSampler *qsa, int num)
{
	double d[2];

	sampler_get_double_2d(d, qsa, num);
	s[0]= (float)d[0];
	s[1]= (float)d[1];
}

/******************************** Global Init/Free **************************/

/* called from convertBlenderScene.c */
void samplers_init(Render *re)
{
	re->sample.qmcsamplers= MEM_callocN(sizeof(ListBase)*BLENDER_MAX_THREADS, "QMCListBase");
}

void samplers_free(Render *re)
{
	QMCSampler *qsa, *next;
	int a;

	if(re->sample.qmcsamplers) {
		for(a=0; a<BLENDER_MAX_THREADS; a++) {
			for(qsa=re->sample.qmcsamplers[a].first; qsa; qsa=next) {
				next= qsa->next;
				qmc_sampler_free(qsa);
			}

			re->sample.qmcsamplers[a].first= re->sample.qmcsamplers[a].last= NULL;
		}

		MEM_freeN(re->sample.qmcsamplers);
		re->sample.qmcsamplers= NULL;
	}
}

/************************ Acquire/Release per Thread *************************/

QMCSampler *sampler_acquire(Render *re, int thread, int type, int tot)
{
	QMCSampler *qsa;

	/* create qmc samplers as needed, since recursion makes it hard to
	 * predict how many are needed */

	for(qsa=re->sample.qmcsamplers[thread].first; qsa; qsa=qsa->next) {
		if(qsa->type == type && qsa->tot == tot && !qsa->used) {
			qsa->used= 1;
			qsa->thread= thread;
			qmc_sampler_init_pixel(qsa, thread);
			return qsa;
		}
	}

	qsa= qmc_sampler_init(type, tot);
	qsa->used= 1;
	qsa->thread= thread;
	BLI_addtail(&re->sample.qmcsamplers[thread], qsa);
	qmc_sampler_init_pixel(qsa, thread);

	return qsa;
}

void sampler_release(Render *re, QMCSampler *qsa)
{
	qsa->used= 0;
	qsa->thread= 0;
}

/******************************* Projection Utilities ***********************/

/* uniform hemisphere sampling */
void sample_project_hemi(float vec[3], float s[2])
{
	float phi, sqr;
	
	phi = s[0]*2.0f*(float)M_PI;
	sqr = sqrtf(1.0f - s[1]*s[1]);

	vec[0] = cosf(phi)*sqr;
	vec[1] = sinf(phi)*sqr;
	vec[2] = s[1];
}

/* cosine weighted hemisphere sampling */
void sample_project_hemi_cosine_weighted(float vec[3], float s[2])
{
	float phi, sqr;
	
	phi = s[0]*2.0f*(float)M_PI;	
	sqr = sqrtf(1.0f - s[1]);

	vec[0] = cosf(phi)*sqr;
	vec[1] = sinf(phi)*sqr;
	vec[2] = sqrtf(s[1]);
}

/* disc of radius 'radius', centred on 0,0 */
void sample_project_disc(float vec[3], float radius, float s[2])
{
	float phi, sqr;

	phi = s[0]*2.0f*(float)M_PI;
	sqr = sqrtf(s[1]);

	vec[0] = cosf(phi)*sqr*radius*0.5f;
	vec[1] = sinf(phi)*sqr*radius*0.5f;
	vec[2] = 0.0f;
}

/* rect of edge lengths sizex, sizey, centred on 0.0,0.0 i.e. ranging from -sizex/2 to +sizey/2 */
void sample_project_rect(float vec[3], float sizex, float sizey, float s[2])
{
	vec[0] = (s[0] - 0.5f) * sizex;
	vec[1] = (s[1] - 0.5f) * sizey;
	vec[2] = 0.0f;
}

/* phong weighted disc using 'blur' for exponent, centred on 0,0 */
void sample_project_phong(float vec[3], float blur, float s[2])
{
	float phi, pz, sqr;

	phi = s[0]*2.0f*(float)M_PI;
	pz = powf(s[1], blur);
	sqr = sqrtf(1.0f-pz*pz);

	vec[0] = cosf(phi)*sqr;
	vec[1] = sinf(phi)*sqr;
	vec[2] = 0.0f;
}


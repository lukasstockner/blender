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
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_node_types.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"

#include "BKE_global.h"
#include "BKE_node.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "intern/openexr/openexr_multi.h"

#include "RE_pipeline.h"
#include "RE_shader_ext.h"

#include "part.h"
#include "pixelfilter.h"
#include "render_types.h"
#include "result.h"
#include "shading.h"

/***************************** Shade Result **********************************/

void shade_result_init(ShadeResult *shr, int tot)
{
	int a;

	memset(shr, 0, sizeof(ShadeResult)*tot);

	for(a=0; a<tot; a++)
		shr[a].z= PASS_Z_MAX;
}

static void copy_minimum_speed(float *fp, float *speed)
{
	if((ABS(speed[0]) + ABS(speed[1]))< (ABS(fp[0]) + ABS(fp[1]))) {
		fp[0]= speed[0];
		fp[1]= speed[1];
	}
	if((ABS(speed[2]) + ABS(speed[3]))< (ABS(fp[2]) + ABS(fp[3]))) {
		fp[2]= speed[2];
		fp[3]= speed[3];
	}
}

int shade_result_accumulate(ShadeResult *samp_shr, ShadeSample *ssamp, int tot, int passflag)
{
	int a, sample, retval = tot;

	for(a=0; a < tot; a++, samp_shr++) {
		ShadeInput *shi= ssamp->shi;
		ShadeResult *shr= ssamp->shr;
		
 		for(sample=0; sample<ssamp->tot; sample++, shi++, shr++) {
		
			if(shi->shading.mask & (1<<a)) {
				float fac= (1.0f - samp_shr->combined[3])*shr->combined[3];
				int first= (samp_shr->z == PASS_Z_MAX);
				
				pxf_add_alpha_under(samp_shr->combined, shr->combined);
				
				// transp method samp_shr->z= MIN2(samp_shr->z, shr->z);
				samp_shr->z= shr->z;

				/* optim... */
				if(passflag & ~(SCE_PASS_VECTOR|SCE_PASS_COMBINED)) {
					if(passflag & SCE_PASS_RGBA)
						pxf_add_alpha_under(samp_shr->col, shr->col);
					
					if(passflag & SCE_PASS_NORMAL)
						madd_v3_v3fl(samp_shr->nor, shr->nor, fac);

					if(passflag & SCE_PASS_EMIT)
						madd_v3_v3fl(samp_shr->emit, shr->emit, fac);

					if(passflag & SCE_PASS_DIFFUSE)
						madd_v3_v3fl(samp_shr->diff, shr->diff, fac);
					
					if(passflag & SCE_PASS_SPEC)
						madd_v3_v3fl(samp_shr->spec, shr->spec, fac);

					if(passflag & SCE_PASS_SHADOW)
						madd_v3_v3fl(samp_shr->shad, shr->shad, fac);

					if(passflag & SCE_PASS_AO)
						madd_v3_v3fl(samp_shr->ao, shr->ao, fac);

					if(passflag & SCE_PASS_ENVIRONMENT)
						madd_v3_v3fl(samp_shr->env, shr->env, fac);

					if(passflag & SCE_PASS_INDIRECT)
						madd_v3_v3fl(samp_shr->indirect, shr->indirect, fac);

					if(passflag & SCE_PASS_REFLECT)
						madd_v3_v3fl(samp_shr->refl, shr->refl, fac);
					
					if(passflag & SCE_PASS_REFRACT)
						madd_v3_v3fl(samp_shr->refr, shr->refr, fac);
					
					if(passflag & SCE_PASS_MIST)
						samp_shr->mist= samp_shr->mist+fac*shr->mist;

					if(first) {
						if(passflag & SCE_PASS_INDEXOB)
							samp_shr->indexob= shr->indexob;

						if(passflag & SCE_PASS_UV)
							copy_v3_v3(samp_shr->uv, shr->uv);
					}
				}

				/* TODO this is not tested well with transparency yet */
				if(passflag & SCE_PASS_VECTOR) {
					if(first || fac > 0.95f)
						copy_v4_v4(samp_shr->winspeed, shr->winspeed);
					else
						copy_minimum_speed(samp_shr->winspeed, shr->winspeed);
				}
			}
		}
		
		if(samp_shr->combined[3]>0.999f) retval--;
	}

	return retval;
}

static void interpolate_vec1(float *v1, float *v2, float t, float negt, float *v)
{
	v[0]= negt*v1[0] + t*v2[0];
}

static void interpolate_vec3(float *v1, float *v2, float t, float negt, float *v)
{
	v[0]= negt*v1[0] + t*v2[0];
	v[1]= negt*v1[1] + t*v2[1];
	v[2]= negt*v1[2] + t*v2[2];
}

static void interpolate_vec4(float *v1, float *v2, float t, float negt, float *v)
{
	v[0]= negt*v1[0] + t*v2[0];
	v[1]= negt*v1[1] + t*v2[1];
	v[2]= negt*v1[2] + t*v2[2];
	v[3]= negt*v1[3] + t*v2[3];
}

void shade_result_interpolate(ShadeResult *shr, ShadeResult *shr1, ShadeResult *shr2, float t, int passflag)
{
	float negt= 1.0f - t;

	interpolate_vec4(shr1->combined, shr2->combined, t, negt, shr->combined);

	if(passflag & SCE_PASS_VECTOR) {
		interpolate_vec4(shr1->winspeed, shr2->winspeed, t, negt, shr->winspeed);
	}
	/* optim... */
	if(passflag & ~(SCE_PASS_VECTOR|SCE_PASS_COMBINED)) {
		if(passflag & SCE_PASS_Z)
			interpolate_vec1(&shr1->z, &shr2->z, t, negt, &shr->z);
		if(passflag & SCE_PASS_RGBA)
			interpolate_vec4(shr1->col, shr2->col, t, negt, shr->col);
		if(passflag & SCE_PASS_NORMAL) {
			interpolate_vec3(shr1->nor, shr2->nor, t, negt, shr->nor);
			normalize_v3(shr->nor);
		}
		if(passflag & SCE_PASS_EMIT)
			interpolate_vec3(shr1->emit, shr2->emit, t, negt, shr->emit);
		if(passflag & SCE_PASS_DIFFUSE)
			interpolate_vec3(shr1->diff, shr2->diff, t, negt, shr->diff);
		if(passflag & SCE_PASS_SPEC)
			interpolate_vec3(shr1->spec, shr2->spec, t, negt, shr->spec);
		if(passflag & SCE_PASS_SHADOW)
			interpolate_vec3(shr1->shad, shr2->shad, t, negt, shr->shad);
		if(passflag & SCE_PASS_AO)
			interpolate_vec3(shr1->ao, shr2->ao, t, negt, shr->ao);
		if(passflag & SCE_PASS_ENVIRONMENT)
			interpolate_vec3(shr1->env, shr2->env, t, negt, shr->env);
		if(passflag & SCE_PASS_INDIRECT)
			interpolate_vec3(shr1->indirect, shr2->indirect, t, negt, shr->indirect);
		if(passflag & SCE_PASS_REFLECT)
			interpolate_vec3(shr1->refl, shr2->refl, t, negt, shr->refl);
		if(passflag & SCE_PASS_REFRACT)
			interpolate_vec3(shr1->refr, shr2->refr, t, negt, shr->refr);
		if(passflag & SCE_PASS_MIST)
			interpolate_vec1(&shr1->mist, &shr2->mist, t, negt, &shr->mist);
	}
}

/* merge multiple shaderesults into one ~ corresponds to simple box filter */
static void shade_result_merge(ShadeResult *shr, RenderLayer *rl, int tot)
{
	RenderPass *rpass;
	float weight= 1.0f/((float)tot);
	int delta= sizeof(ShadeResult)/4;
	
	for(rpass= rl->passes.first; rpass; rpass= rpass->next) {
		float *col= NULL;
		int pixsize= 3;
		
		switch(rpass->passtype) {
			case SCE_PASS_Z: {
				/* z provided by last sample in pixel */
				int b;

				for(b=1; b<tot; b++)
					if(shr[b].z != PASS_Z_MAX)
						shr->z= shr[b].z;
				break;
			}
			case SCE_PASS_RGBA:
				col= shr->col;
				pixsize= 4;
				break;
			case SCE_PASS_EMIT:
				col= shr->emit;
				break;
			case SCE_PASS_DIFFUSE:
				col= shr->diff;
				break;
			case SCE_PASS_SPEC:
				col= shr->spec;
				break;
			case SCE_PASS_SHADOW:
				col= shr->shad;
				break;
			case SCE_PASS_AO:
				col= shr->ao;
				break;
			case SCE_PASS_ENVIRONMENT:
				col= shr->env;
				break;
			case SCE_PASS_INDIRECT:
				col= shr->indirect;
				break;
			case SCE_PASS_REFLECT:
				col= shr->refl;
				break;
			case SCE_PASS_REFRACT:
				col= shr->refr;
				break;
			case SCE_PASS_NORMAL:
				col= shr->nor;
				break;
			case SCE_PASS_MIST:
				col= &shr->mist;
				pixsize= 1;
				break;
			case SCE_PASS_UV:
				col= shr->uv;
				break;
			case SCE_PASS_INDEXOB:
				/* indexob provided by first sample in pixel,
				   automatically set to this so nothing to do */
				break;
			case SCE_PASS_VECTOR: {
				/* sample with minimum speed in pixel */
				ShadeResult *shr_t= shr+1;
				float winspeed[4];
				int b;

				winspeed[0]= winspeed[1]= winspeed[2]= winspeed[3]= PASS_VECTOR_MAX;

				for(b=1; b<tot; b++, shr_t++)
					if(shr_t->combined[3] > 0.0f)
						copy_minimum_speed(winspeed, shr_t->winspeed);

				if(winspeed[0] == PASS_VECTOR_MAX)
					zero_v4(winspeed);
				
				copy_v4_v4(shr->winspeed, winspeed);
				break;
			}
		}

		if(col) {
			float *fp= col+delta;
			int samp, b;
			
			for(samp= 1; samp<tot; samp++, fp+=delta)
				for(b=0; b<pixsize; b++)
					col[b]+= fp[b];

			for(b=0; b<pixsize; b++)
				col[b]*= weight;
		}
	}
}

/* if a == -1 copies, else adds filtered */
static void shade_result_to_layer(Render *re, RenderLayer *rl, int offset, int mask, int a, int tot, ShadeResult *shr)
{
	RenderPass *rpass;

	/* the rules here are a bit strange, but follows existing code
	   for compatibility still, though not entirely, ztra/solid were
	   not using the same rules but are now unified */
	
	for(rpass= rl->passes.first; rpass; rpass= rpass->next) {
		float *fp, *col= NULL;
		int pixsize= 3;
		
		switch(rpass->passtype) {
			case SCE_PASS_Z:
				/* z provided by last sample in pixel */
				fp= rpass->rect + offset;
				if(shr->z != PASS_Z_MAX)
					*fp= shr->z;
				break;
			case SCE_PASS_RGBA:
				col= shr->col;
				pixsize= 4;
				break;
			case SCE_PASS_EMIT:
				col= shr->emit;
				break;
			case SCE_PASS_DIFFUSE:
				col= shr->diff;
				break;
			case SCE_PASS_SPEC:
				col= shr->spec;
				break;
			case SCE_PASS_SHADOW:
				col= shr->shad;
				break;
			case SCE_PASS_AO:
				col= shr->ao;
				break;
			case SCE_PASS_ENVIRONMENT:
				col= shr->env;
				break;
			case SCE_PASS_INDIRECT:
				col= shr->indirect;
				break;
			case SCE_PASS_REFLECT:
				col= shr->refl;
				break;
			case SCE_PASS_REFRACT:
				col= shr->refr;
				break;
			case SCE_PASS_NORMAL:
				col= shr->nor;
				break;
			case SCE_PASS_MIST:
				col= &shr->mist;
				pixsize= 1;
				break;
			case SCE_PASS_UV:
				if(mask) {
					/* box filter only, gauss will screwup UV too much */
					float fac= (float)pxf_mask_count(&re->sample, mask)/tot;
					float *fp= rpass->rect + 3*offset;
					madd_v3_v3fl(fp, shr->uv, fac);
				}
				else
					col= shr->uv;
				break;
			case SCE_PASS_INDEXOB:
				fp= rpass->rect + offset;
				/* obindex provided by first sample in pixel */
				if(*fp == 0.0f)
					*fp= shr->indexob;
				break;
			case SCE_PASS_VECTOR:
				if(mask) {
					/* sample with minimum speed in pixel */
					if(a == 0) {
						ShadeResult *shr_t= shr - a;
						float *fp= rpass->rect + 4*offset;
						float winspeed[4];
						int b;

						winspeed[0]= winspeed[1]= winspeed[2]= winspeed[3]= PASS_VECTOR_MAX;

						for(b=0; b<tot; b++, shr_t++)
							if(shr_t->combined[3] > 0.0f)
								copy_minimum_speed(winspeed, shr_t->winspeed);

						if(winspeed[0] == PASS_VECTOR_MAX)
							zero_v4(winspeed);
						
						copy_v4_v4(fp, winspeed);
					}
				}
				else {
					col= shr->winspeed;
					pixsize= 4;
				}
				break;
			case SCE_PASS_RAYHITS:
				col= shr->rayhits;
				pixsize= 4;
				break;
		}

		if(col) {
			float *fp= rpass->rect + pixsize*offset;

			if(mask) /* add pass filtered */
				pxf_add_filtered_pixsize(&re->sample, mask, col, fp, rl->rectx, pixsize);
			else /* copy unfiltered */
				memcpy(fp, col, sizeof(float)*pixsize);
		}
	}
}

void shade_result_to_part(Render *re, RenderPart *pa, RenderLayer *rl, int offs, ShadeResult *shr)
{
	RenderResult *rr= pa->result;
	int a, passflag= rl->passflag & ~(SCE_PASS_COMBINED);
	int osa= (re->params.osa)? re->params.osa: 1;
	/* previously solid was filtered but transp not... */
	int filter_passes = 1;

	if(pa->fullresult.first || osa == 1) {
		/* full sample or non osa filling - requires only simple copy */
		for(a=0; a<osa; a++) {
			RenderLayer *rl= pa->rlpp[a];
			float alpha= shr[a].combined[3];

			if(alpha > 0.0f) {
				copy_v4_v4(rl->rectf + 4*offs, shr[a].combined);

				if(passflag)
					shade_result_to_layer(re, rl, offs, 0, a, osa, &shr[a]);
			}
		}
	}
	else {
		/* osa without full samples */
		float alpha= 0.0f;

		/* add combined color filtered in render layer */
		for(a=0; a<osa; a++) {
			pxf_add_filtered(&re->sample, 1<<a, shr[a].combined, rl->rectf + 4*offs, rr->rectx);
			alpha+= shr[a].combined[3];
		}
		
		if(passflag && alpha > 0.0f) {
			if(filter_passes) {
				/* add each shade result individually filtered */
				for(a=0; a<osa; a++)
					shade_result_to_layer(re, rl, offs, (1<<a), a, osa, &shr[a]);
			}
			else {
				/* average all in one, and then copy (box filter) */
				shade_result_merge(shr, rl, osa);
				shade_result_to_layer(re, rl, offs, 0, 0, 1, shr);
			}
		}
	}
}

/***************************** Render Result *********************************/

void RE_FreeRenderResult(RenderResult *res)
{
	if(res==NULL) return;

	BLI_rw_mutex_lock(&res->mutex, THREAD_LOCK_WRITE);

	while(res->layers.first) {
		RenderLayer *rl= res->layers.first;
		
		if(rl->rectf) MEM_freeN(rl->rectf);
		/* acolrect and scolrect are optionally allocated in shade_tile, only free here since it can be used for drawing */
		if(rl->acolrect) MEM_freeN(rl->acolrect);
		if(rl->scolrect) MEM_freeN(rl->scolrect);
		
		while(rl->passes.first) {
			RenderPass *rpass= rl->passes.first;
			if(rpass->rect) MEM_freeN(rpass->rect);
			BLI_remlink(&rl->passes, rpass);
			MEM_freeN(rpass);
		}
		BLI_remlink(&res->layers, rl);
		MEM_freeN(rl);
	}
	
	if(res->rect32)
		MEM_freeN(res->rect32);
	if(res->rectz)
		MEM_freeN(res->rectz);
	if(res->rectf)
		MEM_freeN(res->rectf);
	if(res->text)
		MEM_freeN(res->text);

	BLI_rw_mutex_unlock(&res->mutex);
	BLI_rw_mutex_end(&res->mutex);
	
	MEM_freeN(res);
}

/* version that's compatible with fullsample buffers */
void render_result_free(ListBase *lb, RenderResult *rr)
{
	RenderResult *rrnext;
	
	for(; rr; rr= rrnext) {
		rrnext= rr->next;
		
		if(lb && lb->first)
			BLI_remlink(lb, rr);
		
		RE_FreeRenderResult(rr);
	}
}


/* all layers except the active one get temporally pushed away */
void push_render_result(Render *re)
{
	BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);

	/* officially pushed result should be NULL... error can happen with do_seq */
	RE_FreeRenderResult(re->pushedresult);
	
	re->pushedresult= re->result;
	re->result= NULL;

	BLI_rw_mutex_unlock(&re->resultmutex);
}

/* if scemode is R_SINGLE_LAYER, at end of rendering, merge the both render results */
void pop_render_result(Render *re)
{
	
	if(re->result==NULL) {
		printf("pop render result error; no current result!\n");
		return;
	}
	if(re->pushedresult) {
		BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);

		if(re->pushedresult->rectx==re->result->rectx && re->pushedresult->recty==re->result->recty) {
			/* find which layer in pushedresult should be replaced */
			SceneRenderLayer *srl;
			RenderLayer *rlpush;
			RenderLayer *rl= re->result->layers.first;
			int nr;
			
			/* render result should be empty after this */
			BLI_remlink(&re->result->layers, rl);
			
			/* reconstruct render result layers */
			for(nr=0, srl= re->db.scene->r.layers.first; srl; srl= srl->next, nr++) {
				if(nr==re->params.r.actlay)
					BLI_addtail(&re->result->layers, rl);
				else {
					rlpush= RE_GetRenderLayer(re->pushedresult, srl->name);
					if(rlpush) {
						BLI_remlink(&re->pushedresult->layers, rlpush);
						BLI_addtail(&re->result->layers, rlpush);
					}
				}
			}
		}
		
		RE_FreeRenderResult(re->pushedresult);
		re->pushedresult= NULL;

		BLI_rw_mutex_unlock(&re->resultmutex);
	}
}

/* NOTE: OpenEXR only supports 32 chars for layer+pass names
   In blender we now use max 10 chars for pass, max 20 for layer */
static char *get_pass_name(int passtype, int channel)
{
	
	if(passtype == SCE_PASS_COMBINED) {
		if(channel==-1) return "Combined";
		if(channel==0) return "Combined.R";
		if(channel==1) return "Combined.G";
		if(channel==2) return "Combined.B";
		return "Combined.A";
	}
	if(passtype == SCE_PASS_Z) {
		if(channel==-1) return "Depth";
		return "Depth.Z";
	}
	if(passtype == SCE_PASS_VECTOR) {
		if(channel==-1) return "Vector";
		if(channel==0) return "Vector.X";
		if(channel==1) return "Vector.Y";
		if(channel==2) return "Vector.Z";
		return "Vector.W";
	}
	if(passtype == SCE_PASS_NORMAL) {
		if(channel==-1) return "Normal";
		if(channel==0) return "Normal.X";
		if(channel==1) return "Normal.Y";
		return "Normal.Z";
	}
	if(passtype == SCE_PASS_UV) {
		if(channel==-1) return "UV";
		if(channel==0) return "UV.U";
		if(channel==1) return "UV.V";
		return "UV.A";
	}
	if(passtype == SCE_PASS_RGBA) {
		if(channel==-1) return "Color";
		if(channel==0) return "Color.R";
		if(channel==1) return "Color.G";
		if(channel==2) return "Color.B";
		return "Color.A";
	}
	if(passtype == SCE_PASS_EMIT) {
		if(channel==-1) return "Emit";
		if(channel==0) return "Emit.R";
		if(channel==1) return "Emit.G";
		return "Emit.B";
	}
	if(passtype == SCE_PASS_DIFFUSE) {
		if(channel==-1) return "Diffuse";
		if(channel==0) return "Diffuse.R";
		if(channel==1) return "Diffuse.G";
		return "Diffuse.B";
	}
	if(passtype == SCE_PASS_SPEC) {
		if(channel==-1) return "Spec";
		if(channel==0) return "Spec.R";
		if(channel==1) return "Spec.G";
		return "Spec.B";
	}
	if(passtype == SCE_PASS_SHADOW) {
		if(channel==-1) return "Shadow";
		if(channel==0) return "Shadow.R";
		if(channel==1) return "Shadow.G";
		return "Shadow.B";
	}
	if(passtype == SCE_PASS_AO) {
		if(channel==-1) return "AO";
		if(channel==0) return "AO.R";
		if(channel==1) return "AO.G";
		return "AO.B";
	}
	if(passtype == SCE_PASS_ENVIRONMENT) {
		if(channel==-1) return "Env";
		if(channel==0) return "Env.R";
		if(channel==1) return "Env.G";
		return "Env.B";
	}
	if(passtype == SCE_PASS_INDIRECT) {
		if(channel==-1) return "Indirect";
		if(channel==0) return "Indirect.R";
		if(channel==1) return "Indirect.G";
		return "Indirect.B";
	}
	if(passtype == SCE_PASS_REFLECT) {
		if(channel==-1) return "Reflect";
		if(channel==0) return "Reflect.R";
		if(channel==1) return "Reflect.G";
		return "Reflect.B";
	}
	if(passtype == SCE_PASS_REFRACT) {
		if(channel==-1) return "Refract";
		if(channel==0) return "Refract.R";
		if(channel==1) return "Refract.G";
		return "Refract.B";
	}
	if(passtype == SCE_PASS_INDEXOB) {
		if(channel==-1) return "IndexOB";
		return "IndexOB.X";
	}
	if(passtype == SCE_PASS_MIST) {
		if(channel==-1) return "Mist";
		return "Mist.Z";
	}
	if(passtype == SCE_PASS_RAYHITS)
	{
		if(channel==-1) return "Rayhits";
		if(channel==0) return "Rayhits.R";
		if(channel==1) return "Rayhits.G";
		return "Rayhits.B";
	}
	return "Unknown";
}

static int passtype_from_name(char *str)
{
	
	if(strcmp(str, "Combined")==0)
		return SCE_PASS_COMBINED;

	if(strcmp(str, "Depth")==0)
		return SCE_PASS_Z;

	if(strcmp(str, "Vector")==0)
		return SCE_PASS_VECTOR;

	if(strcmp(str, "Normal")==0)
		return SCE_PASS_NORMAL;

	if(strcmp(str, "UV")==0)
		return SCE_PASS_UV;

	if(strcmp(str, "Color")==0)
		return SCE_PASS_RGBA;

	if(strcmp(str, "Emit")==0)
		return SCE_PASS_EMIT;

	if(strcmp(str, "Diffuse")==0)
		return SCE_PASS_DIFFUSE;

	if(strcmp(str, "Spec")==0)
		return SCE_PASS_SPEC;

	if(strcmp(str, "Shadow")==0)
		return SCE_PASS_SHADOW;
	
	if(strcmp(str, "AO")==0)
		return SCE_PASS_AO;

	if(strcmp(str, "Env")==0)
		return SCE_PASS_ENVIRONMENT;

	if(strcmp(str, "Indirect")==0)
		return SCE_PASS_INDIRECT;

	if(strcmp(str, "Reflect")==0)
		return SCE_PASS_REFLECT;

	if(strcmp(str, "Refract")==0)
		return SCE_PASS_REFRACT;

	if(strcmp(str, "IndexOB")==0)
		return SCE_PASS_INDEXOB;

	if(strcmp(str, "Mist")==0)
		return SCE_PASS_MIST;
	
	if(strcmp(str, "RayHits")==0)
		return SCE_PASS_RAYHITS;
	return 0;
}

void render_unique_exr_name(Render *re, char *str, int sample)
{
	char di[FILE_MAX], name[FILE_MAXFILE+MAX_ID_NAME+100], fi[FILE_MAXFILE];
	
	BLI_strncpy(di, G.sce, FILE_MAX);
	BLI_splitdirstring(di, fi);
	
	if(sample==0)
		sprintf(name, "%s_%s.exr", fi, re->db.scene->id.name+2);
	else
		sprintf(name, "%s_%s%d.exr", fi, re->db.scene->id.name+2, sample);

	BLI_make_file_string("/", str, btempdir, name);
}

static void render_layer_add_pass(RenderResult *rr, RenderLayer *rl, int channels, int passtype)
{
	char *typestr= get_pass_name(passtype, 0);
	RenderPass *rpass= MEM_callocN(sizeof(RenderPass), typestr);
	int rectsize= rr->rectx*rr->recty*channels;
	
	BLI_addtail(&rl->passes, rpass);
	rpass->passtype= passtype;
	rpass->channels= channels;
	rpass->rectx= rl->rectx;
	rpass->recty= rl->recty;
	
	if(rr->exrhandle) {
		int a;
		for(a=0; a<channels; a++)
			IMB_exr_add_channel(rr->exrhandle, rl->name, get_pass_name(passtype, a), 0, 0, NULL);
	}
	else {
		float *rect;
		int x;
		
		rpass->rect= MEM_mapallocN(sizeof(float)*rectsize, typestr);
		
		if(passtype==SCE_PASS_Z) {
			rect= rpass->rect;
			for(x= rectsize-1; x>=0; x--)
				rect[x]= PASS_Z_MAX;
		}
	}
}

float *RE_RenderLayerGetPass(RenderLayer *rl, int passtype)
{
	RenderPass *rpass;
	
	for(rpass=rl->passes.first; rpass; rpass= rpass->next)
		if(rpass->passtype== passtype)
			return rpass->rect;
	return NULL;
}

RenderLayer *RE_GetRenderLayer(RenderResult *rr, const char *name)
{
	RenderLayer *rl;
	
	if(rr==NULL) return NULL;
	
	for(rl= rr->layers.first; rl; rl= rl->next)
		if(strncmp(rl->name, name, RE_MAXNAME)==0)
			return rl;
	return NULL;
}

/* called by main render as well for parts */
/* will read info from Render *re to define layers */
/* called in threads */
/* re->cam.winx,winy is coordinate space of entire image, partrct the part within */
RenderResult *render_result_create(Render *re, rcti *partrct, int crop, int savebuffers)
{
	RenderResult *rr;
	RenderLayer *rl;
	SceneRenderLayer *srl;
	int rectx, recty, nr;
	
	rectx= partrct->xmax - partrct->xmin;
	recty= partrct->ymax - partrct->ymin;
	
	if(rectx<=0 || recty<=0)
		return NULL;
	
	rr= MEM_callocN(sizeof(RenderResult), "new render result");
	rr->rectx= rectx;
	rr->recty= recty;
	rr->renrect.xmin= 0; rr->renrect.xmax= rectx-2*crop;
	/* crop is one or two extra pixels rendered for filtering, is used for merging and display too */
	rr->crop= crop;
	
	/* tilerect is relative coordinates within render disprect. do not subtract crop yet */
	rr->tilerect.xmin= partrct->xmin - re->disprect.xmin;
	rr->tilerect.xmax= partrct->xmax - re->disprect.xmax;
	rr->tilerect.ymin= partrct->ymin - re->disprect.ymin;
	rr->tilerect.ymax= partrct->ymax - re->disprect.ymax;
	
	if(savebuffers) {
		rr->exrhandle= IMB_exr_get_handle();
	}
	
	/* check renderdata for amount of layers */
	for(nr=0, srl= re->params.r.layers.first; srl; srl= srl->next, nr++) {
		
		if((re->params.r.scemode & R_SINGLE_LAYER) && nr!=re->params.r.actlay)
			continue;
		if(srl->layflag & SCE_LAY_DISABLE)
			continue;
		
		rl= MEM_callocN(sizeof(RenderLayer), "new render layer");
		BLI_addtail(&rr->layers, rl);
		
		strcpy(rl->name, srl->name);
		rl->lay= srl->lay;
		rl->lay_zmask= srl->lay_zmask;
		rl->layflag= srl->layflag;
		rl->passflag= srl->passflag; // for debugging: srl->passflag|SCE_PASS_RAYHITS;
		rl->pass_xor= srl->pass_xor;
		rl->light_override= srl->light_override;
		rl->mat_override= srl->mat_override;
		rl->rectx= rectx;
		rl->recty= recty;
		
		if(rr->exrhandle) {
			IMB_exr_add_channel(rr->exrhandle, rl->name, "Combined.R", 0, 0, NULL);
			IMB_exr_add_channel(rr->exrhandle, rl->name, "Combined.G", 0, 0, NULL);
			IMB_exr_add_channel(rr->exrhandle, rl->name, "Combined.B", 0, 0, NULL);
			IMB_exr_add_channel(rr->exrhandle, rl->name, "Combined.A", 0, 0, NULL);
		}
		else
			rl->rectf= MEM_mapallocN(rectx*recty*sizeof(float)*4, "Combined rgba");
		
		if(srl->passflag  & SCE_PASS_Z)
			render_layer_add_pass(rr, rl, 1, SCE_PASS_Z);
		if(srl->passflag  & SCE_PASS_VECTOR)
			render_layer_add_pass(rr, rl, 4, SCE_PASS_VECTOR);
		if(srl->passflag  & SCE_PASS_NORMAL)
			render_layer_add_pass(rr, rl, 3, SCE_PASS_NORMAL);
		if(srl->passflag  & SCE_PASS_UV) 
			render_layer_add_pass(rr, rl, 3, SCE_PASS_UV);
		if(srl->passflag  & SCE_PASS_RGBA)
			render_layer_add_pass(rr, rl, 4, SCE_PASS_RGBA);
		if(srl->passflag  & SCE_PASS_EMIT)
			render_layer_add_pass(rr, rl, 3, SCE_PASS_EMIT);
		if(srl->passflag  & SCE_PASS_DIFFUSE)
			render_layer_add_pass(rr, rl, 3, SCE_PASS_DIFFUSE);
		if(srl->passflag  & SCE_PASS_SPEC)
			render_layer_add_pass(rr, rl, 3, SCE_PASS_SPEC);
		if(srl->passflag  & SCE_PASS_AO)
			render_layer_add_pass(rr, rl, 3, SCE_PASS_AO);
		if(srl->passflag  & SCE_PASS_ENVIRONMENT)
			render_layer_add_pass(rr, rl, 3, SCE_PASS_ENVIRONMENT);
		if(srl->passflag  & SCE_PASS_INDIRECT)
			render_layer_add_pass(rr, rl, 3, SCE_PASS_INDIRECT);
		if(srl->passflag  & SCE_PASS_SHADOW)
			render_layer_add_pass(rr, rl, 3, SCE_PASS_SHADOW);
		if(srl->passflag  & SCE_PASS_REFLECT)
			render_layer_add_pass(rr, rl, 3, SCE_PASS_REFLECT);
		if(srl->passflag  & SCE_PASS_REFRACT)
			render_layer_add_pass(rr, rl, 3, SCE_PASS_REFRACT);
		if(srl->passflag  & SCE_PASS_INDEXOB)
			render_layer_add_pass(rr, rl, 1, SCE_PASS_INDEXOB);
		if(srl->passflag  & SCE_PASS_MIST)
			render_layer_add_pass(rr, rl, 1, SCE_PASS_MIST);
		if(rl->passflag & SCE_PASS_RAYHITS)
			render_layer_add_pass(rr, rl, 4, SCE_PASS_RAYHITS);
		
	}
	/* sss, previewrender and envmap don't do layers, so we make a default one */
	if(rr->layers.first==NULL) {
		rl= MEM_callocN(sizeof(RenderLayer), "new render layer");
		BLI_addtail(&rr->layers, rl);
		
		rl->rectx= rectx;
		rl->recty= recty;

		/* duplicate code... */
		if(rr->exrhandle) {
			IMB_exr_add_channel(rr->exrhandle, rl->name, "Combined.R", 0, 0, NULL);
			IMB_exr_add_channel(rr->exrhandle, rl->name, "Combined.G", 0, 0, NULL);
			IMB_exr_add_channel(rr->exrhandle, rl->name, "Combined.B", 0, 0, NULL);
			IMB_exr_add_channel(rr->exrhandle, rl->name, "Combined.A", 0, 0, NULL);
		}
		else
			rl->rectf= MEM_mapallocN(rectx*recty*sizeof(float)*4, "Combined rgba");
		
		/* note, this has to be in sync with scene.c */
		rl->lay= (1<<20) -1;
		rl->layflag= 0x7FFF;	/* solid ztra halo strand */
		rl->passflag= SCE_PASS_COMBINED;
		
		re->params.r.actlay= 0;
	}
	
	/* border render; calculate offset for use in compositor. compo is centralized coords */
	rr->xof= re->disprect.xmin + (re->disprect.xmax - re->disprect.xmin)/2 - re->cam.winx/2;
	rr->yof= re->disprect.ymin + (re->disprect.ymax - re->disprect.ymin)/2 - re->cam.winy/2;

	BLI_rw_mutex_init(&rr->mutex);
	
	return rr;
}

static void do_merge_tile(RenderResult *rr, RenderResult *rrpart, float *target, float *tile, int pixsize)
{
	int y, ofs, copylen, tilex, tiley;
	
	copylen= tilex= rrpart->rectx;
	tiley= rrpart->recty;
	
	if(rrpart->crop) {	/* filters add pixel extra */
		tile+= pixsize*(rrpart->crop + rrpart->crop*tilex);
		
		copylen= tilex - 2*rrpart->crop;
		tiley -= 2*rrpart->crop;
		
		ofs= (rrpart->tilerect.ymin + rrpart->crop)*rr->rectx + (rrpart->tilerect.xmin+rrpart->crop);
		target+= pixsize*ofs;
	}
	else {
		ofs= (rrpart->tilerect.ymin*rr->rectx + rrpart->tilerect.xmin);
		target+= pixsize*ofs;
	}

	copylen *= sizeof(float)*pixsize;
	tilex *= pixsize;
	ofs= pixsize*rr->rectx;

	for(y=0; y<tiley; y++) {
		memcpy(target, tile, copylen);
		target+= ofs;
		tile+= tilex;
	}
}

/* used when rendering to a full buffer, or when reading the exr part-layer-pass file */
/* no test happens here if it fits... we also assume layers are in sync */
/* is used within threads */
static void render_result_merge(RenderResult *rr, RenderResult *rrpart)
{
	RenderLayer *rl, *rlp;
	RenderPass *rpass, *rpassp;
	
	for(rl= rr->layers.first, rlp= rrpart->layers.first; rl && rlp; rl= rl->next, rlp= rlp->next) {
		
		/* combined */
		if(rl->rectf && rlp->rectf)
			do_merge_tile(rr, rrpart, rl->rectf, rlp->rectf, 4);
		
		/* passes are allocated in sync */
		for(rpass= rl->passes.first, rpassp= rlp->passes.first; rpass && rpassp; rpass= rpass->next, rpassp= rpassp->next) {
			do_merge_tile(rr, rrpart, rpass->rect, rpassp->rect, rpass->channels);
		}
	}
}


static void save_render_result_tile(RenderResult *rr, RenderResult *rrpart)
{
	RenderLayer *rlp;
	RenderPass *rpassp;
	int offs, partx, party;
	
	BLI_lock_thread(LOCK_IMAGE);
	
	for(rlp= rrpart->layers.first; rlp; rlp= rlp->next) {
		
		if(rrpart->crop) {	/* filters add pixel extra */
			offs= (rrpart->crop + rrpart->crop*rrpart->rectx);
		}
		else {
			offs= 0;
		}
		
		/* combined */
		if(rlp->rectf) {
			int a, xstride= 4;
			for(a=0; a<xstride; a++)
				IMB_exr_set_channel(rr->exrhandle, rlp->name, get_pass_name(SCE_PASS_COMBINED, a), 
								xstride, xstride*rrpart->rectx, rlp->rectf+a + xstride*offs);
		}
		
		/* passes are allocated in sync */
		for(rpassp= rlp->passes.first; rpassp; rpassp= rpassp->next) {
			int a, xstride= rpassp->channels;
			for(a=0; a<xstride; a++)
				IMB_exr_set_channel(rr->exrhandle, rlp->name, get_pass_name(rpassp->passtype, a), 
									xstride, xstride*rrpart->rectx, rpassp->rect+a + xstride*offs);
		}
		
	}

	party= rrpart->tilerect.ymin + rrpart->crop;
	partx= rrpart->tilerect.xmin + rrpart->crop;
	IMB_exrtile_write_channels(rr->exrhandle, partx, party, 0);

	BLI_unlock_thread(LOCK_IMAGE);
}

static void save_empty_result_tiles(Render *re)
{
	RenderPart *pa;
	RenderResult *rr;
	
	for(rr= re->result; rr; rr= rr->next) {
		IMB_exrtile_clear_channels(rr->exrhandle);
		
		for(pa= re->parts.first; pa; pa= pa->next) {
			if(pa->ready==0) {
				int party= pa->disprect.ymin - re->disprect.ymin + pa->crop;
				int partx= pa->disprect.xmin - re->disprect.xmin + pa->crop;
				IMB_exrtile_write_channels(rr->exrhandle, partx, party, 0);
			}
		}
	}
}


/* for passes read from files, these have names stored */
static char *make_pass_name(RenderPass *rpass, int chan)
{
	static char name[16];
	int len;
	
	BLI_strncpy(name, rpass->name, EXR_PASS_MAXNAME);
	len= strlen(name);
	name[len]= '.';
	name[len+1]= rpass->chan_id[chan];
	name[len+2]= 0;

	return name;
}

/* filename already made absolute */
/* called from within UI, saves both rendered result as a file-read result */
void RE_WriteRenderResult(RenderResult *rr, char *filename, int compress)
{
	RenderLayer *rl;
	RenderPass *rpass;
	void *exrhandle= IMB_exr_get_handle();

	BLI_make_existing_file(filename);
	
	/* composite result */
	if(rr->rectf) {
		IMB_exr_add_channel(exrhandle, "Composite", "Combined.R", 4, 4*rr->rectx, rr->rectf);
		IMB_exr_add_channel(exrhandle, "Composite", "Combined.G", 4, 4*rr->rectx, rr->rectf+1);
		IMB_exr_add_channel(exrhandle, "Composite", "Combined.B", 4, 4*rr->rectx, rr->rectf+2);
		IMB_exr_add_channel(exrhandle, "Composite", "Combined.A", 4, 4*rr->rectx, rr->rectf+3);
	}
	
	/* add layers/passes and assign channels */
	for(rl= rr->layers.first; rl; rl= rl->next) {
		
		/* combined */
		if(rl->rectf) {
			int a, xstride= 4;
			for(a=0; a<xstride; a++)
				IMB_exr_add_channel(exrhandle, rl->name, get_pass_name(SCE_PASS_COMBINED, a), 
									xstride, xstride*rr->rectx, rl->rectf+a);
		}
		
		/* passes are allocated in sync */
		for(rpass= rl->passes.first; rpass; rpass= rpass->next) {
			int a, xstride= rpass->channels;
			for(a=0; a<xstride; a++) {
				if(rpass->passtype)
					IMB_exr_add_channel(exrhandle, rl->name, get_pass_name(rpass->passtype, a), 
										xstride, xstride*rr->rectx, rpass->rect+a);
				else
					IMB_exr_add_channel(exrhandle, rl->name, make_pass_name(rpass, a), 
										xstride, xstride*rr->rectx, rpass->rect+a);
			}
		}
	}
	
	IMB_exr_begin_write(exrhandle, filename, rr->rectx, rr->recty, compress);
	
	IMB_exr_write_channels(exrhandle);
	IMB_exr_close(exrhandle);
}

/* callbacks for RE_MultilayerConvert */
static void *ml_addlayer_cb(void *base, char *str)
{
	RenderResult *rr= base;
	RenderLayer *rl;
	
	rl= MEM_callocN(sizeof(RenderLayer), "new render layer");
	BLI_addtail(&rr->layers, rl);
	
	BLI_strncpy(rl->name, str, EXR_LAY_MAXNAME);
	return rl;
}
static void ml_addpass_cb(void *base, void *lay, char *str, float *rect, int totchan, char *chan_id)
{
	RenderLayer *rl= lay;	
	RenderPass *rpass= MEM_callocN(sizeof(RenderPass), "loaded pass");
	int a;
	
	BLI_addtail(&rl->passes, rpass);
	rpass->channels= totchan;

	rpass->passtype= passtype_from_name(str);
	if(rpass->passtype==0) printf("unknown pass %s\n", str);
	rl->passflag |= rpass->passtype;
	
	BLI_strncpy(rpass->name, str, EXR_PASS_MAXNAME);
	/* channel id chars */
	for(a=0; a<totchan; a++)
		rpass->chan_id[a]= chan_id[a];
	
	rpass->rect= rect;
}

/* from imbuf, if a handle was returned we convert this to render result */
RenderResult *RE_MultilayerConvert(void *exrhandle, int rectx, int recty)
{
	RenderResult *rr= MEM_callocN(sizeof(RenderResult), "loaded render result");
	RenderLayer *rl;
	RenderPass *rpass;
	
	rr->rectx= rectx;
	rr->recty= recty;
	
	IMB_exr_multilayer_convert(exrhandle, rr, ml_addlayer_cb, ml_addpass_cb);

	for(rl=rr->layers.first; rl; rl=rl->next) {
		rl->rectx= rectx;
		rl->recty= recty;

		for(rpass=rl->passes.first; rpass; rpass=rpass->next) {
			rpass->rectx= rectx;
			rpass->recty= recty;
		}
	}
	
	return rr;
}

/* called in end of render, to add names to passes... for UI only */
void renderresult_add_names(RenderResult *rr)
{
	RenderLayer *rl;
	RenderPass *rpass;
	
	for(rl= rr->layers.first; rl; rl= rl->next)
		for(rpass= rl->passes.first; rpass; rpass= rpass->next)
			strcpy(rpass->name, get_pass_name(rpass->passtype, -1));
}

/* called for reading temp files, and for external engines */
int render_result_read_from_file(char *filename, RenderResult *rr)
{
	RenderLayer *rl;
	RenderPass *rpass;
	void *exrhandle= IMB_exr_get_handle();
	int rectx, recty;

	if(IMB_exr_begin_read(exrhandle, filename, &rectx, &recty)==0) {
		IMB_exr_close(exrhandle);
		return 0;
	}
	
	if(rr == NULL || rectx!=rr->rectx || recty!=rr->recty) {
		printf("error in reading render result\n");
		IMB_exr_close(exrhandle);
		return 0;
	}
	else {
		for(rl= rr->layers.first; rl; rl= rl->next) {
			
			/* combined */
			if(rl->rectf) {
				int a, xstride= 4;
				for(a=0; a<xstride; a++)
					IMB_exr_set_channel(exrhandle, rl->name, get_pass_name(SCE_PASS_COMBINED, a), 
										xstride, xstride*rectx, rl->rectf+a);
			}
			
			/* passes are allocated in sync */
			for(rpass= rl->passes.first; rpass; rpass= rpass->next) {
				int a, xstride= rpass->channels;
				for(a=0; a<xstride; a++)
					IMB_exr_set_channel(exrhandle, rl->name, get_pass_name(rpass->passtype, a), 
										xstride, xstride*rectx, rpass->rect+a);
			}
			
		}
		IMB_exr_read_channels(exrhandle);
		renderresult_add_names(rr);
	}
	
	IMB_exr_close(exrhandle);

	return 1;
}

/* only for temp buffer files, makes exact copy of render result */
void render_result_read(Render *re, int sample)
{
	char str[FILE_MAX];

	BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);

	RE_FreeRenderResult(re->result);
	re->result= render_result_create(re, &re->disprect, 0, RR_USEMEM);

	render_unique_exr_name(re, str, sample);
	printf("read exr tmp file: %s\n", str);

	if(!render_result_read_from_file(str, re->result))
		printf("cannot read: %s\n", str);

	BLI_rw_mutex_unlock(&re->resultmutex);
}

/* reads all buffers, calls optional composite, merges in first result->rectf */
void do_merge_fullsample(Render *re, bNodeTree *ntree, ListBase *list)
{
	float *rectf, filt[3][3];
	int sample;
	
	/* we accumulate in here */
	rectf= MEM_mapallocN(re->rectx*re->recty*sizeof(float)*4, "fullsample rgba");
	
	for(sample=0; sample<re->params.r.osa; sample++) {
		RenderResult rres;
		int x, y;
		
		/* set all involved renders on the samplebuffers (first was done by render itself) */
		/* also function below assumes this */
		if(sample) {
			Render *re1;
			
			tag_scenes_for_render(re);
			for(re1= list->first; re1; re1= re1->next) {
				if(re1->db.scene->id.flag & LIB_DOIT)
					if(re1->params.r.scemode & R_FULL_SAMPLE)
						render_result_read(re1, sample);
			}
		}

		/* composite */
		if(ntree) {
			ntreeCompositTagRender(re->db.scene);
			ntreeCompositTagAnimated(ntree);
			
			ntreeCompositExecTree(ntree, &re->params.r, G.background==0);
		}
		
		/* ensure we get either composited result or the active layer */
		RE_AcquireResultImage(re, &rres);
		
		/* accumulate with filter, and clip */
		pxf_mask_table(&re->sample, (1<<sample), filt);

		for(y=0; y<re->recty; y++) {
			float *rf= rectf + 4*y*re->rectx;
			float *col= rres.rectf + 4*y*re->rectx;
				
			for(x=0; x<re->rectx; x++, rf+=4, col+=4) {
				/* clamping to 1.0 is needed for correct AA */
				if(col[0]<0.0f) col[0]=0.0f; else if(col[0] > 1.0f) col[0]= 1.0f;
				if(col[1]<0.0f) col[1]=0.0f; else if(col[1] > 1.0f) col[1]= 1.0f;
				if(col[2]<0.0f) col[2]=0.0f; else if(col[2] > 1.0f) col[2]= 1.0f;
				
				pxf_add_filtered_table(filt, col, rf, re->rectx, re->recty, x, y);
			}
		}
		
		RE_ReleaseResultImage(re);

		/* show stuff */
		if(sample!=re->params.osa-1) {
			/* weak... the display callback wants an active renderlayer pointer... */
			re->result->renlay= render_get_active_layer(re, re->result);
			re->cb.display_draw(re->cb.ddh, re->result, NULL);
		}
		
		if(re->cb.test_break(re->cb.tbh))
			break;
	}
	
	BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);
	if(re->result->rectf) 
		MEM_freeN(re->result->rectf);
	re->result->rectf= rectf;
	BLI_rw_mutex_unlock(&re->resultmutex);
}

void render_result_merge_part(Render *re, RenderResult *result)
{
	/* we do actually write pixels, but don't allocate/deallocate anything,
	   so it is safe with other threads reading at the same time */
	BLI_rw_mutex_lock(&re->result->mutex, THREAD_LOCK_READ);
	
	/* merge too on break! */
	if(re->result->exrhandle) {
		RenderResult *rr, *rrpart;
		
		for(rr= re->result, rrpart= result; rr && rrpart; rr= rr->next, rrpart= rrpart->next)
			save_render_result_tile(rr, rrpart);
		
	}
	else {
		/* on break, don't merge in result for preview renders, looks nicer */
		if(re->cb.test_break(re->cb.tbh) && (re->params.r.scemode & R_PREVIEWBUTS));
		else render_result_merge(re->result, result);
	}

	BLI_rw_mutex_unlock(&re->result->mutex);
}

void render_result_exr_write(Render *re)
{
	RenderResult *rr;
	char str[FILE_MAX];
	
	for(rr= re->result; rr; rr= rr->next) {
		render_unique_exr_name(re, str, rr->sample_nr);
	
		printf("write exr tmp file, %dx%d, %s\n", rr->rectx, rr->recty, str);
		IMB_exrtile_begin_write(rr->exrhandle, str, 0, rr->rectx, rr->recty, re->partx, re->party);
	}
}
	
void render_result_exr_read(Render *re)
{
	/* save buffers, read back results */
	RenderResult *rr;

	BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);
	save_empty_result_tiles(re);
	
	for(rr= re->result; rr; rr= rr->next) {
		IMB_exr_close(rr->exrhandle);
		rr->exrhandle= NULL;
	}
	
	render_result_free(&re->fullresult, re->result);
	re->result= NULL;

	BLI_rw_mutex_unlock(&re->resultmutex);
	
	render_result_read(re, 0);
}

void render_result_border_merge(Render *re)
{
	if((re->params.r.mode & R_CROP)==0) {
		RenderResult *rres;
		
		BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);

		/* sub-rect for merge call later on */
		re->result->tilerect= re->disprect;
		
		/* this copying sequence could become function? */
		/* weak is: it chances disprect from border */
		re->disprect.xmin= re->disprect.ymin= 0;
		re->disprect.xmax= re->cam.winx;
		re->disprect.ymax= re->cam.winy;
		re->rectx= re->cam.winx;
		re->recty= re->cam.winy;
		
		rres= render_result_create(re, &re->disprect, 0, RR_USEMEM);
		
		render_result_merge(rres, re->result);
		RE_FreeRenderResult(re->result);
		re->result= rres;
		
		/* weak... the display callback wants an active renderlayer pointer... */
		re->result->renlay= render_get_active_layer(re, re->result);
		
		BLI_rw_mutex_unlock(&re->resultmutex);

		re->cb.display_init(re->cb.dih, re->result);
		re->cb.display_draw(re->cb.ddh, re->result, NULL);
	}
	else {
		/* set offset (again) for use in compositor, disprect was manipulated. */
		re->result->xof= 0;
		re->result->yof= 0;
	}
}

/* make osa new results for samples */
RenderResult *render_result_full_sample_create(Render *re)
{
	int a;
	
	for(a=0; a<re->params.osa; a++) {
		RenderResult *rr= render_result_create(re, &re->disprect, 0, 1);
		BLI_addtail(&re->fullresult, rr);
		rr->sample_nr= a;
	}
	
	return re->fullresult.first;
}

/******************************** Utilities *********************************/

int get_sample_layers(Render *re, RenderPart *pa, RenderLayer *rl, RenderLayer **rlpp)
{
	
	if(pa->fullresult.first) {
		int sample, nr= BLI_findindex(&pa->result->layers, rl);
		
		for(sample=0; sample<re->params.osa; sample++) {
			RenderResult *rr= BLI_findlink(&pa->fullresult, sample);
		
			rlpp[sample]= BLI_findlink(&rr->layers, nr);
		}		

		return re->params.osa;
	}
	else {
		rlpp[0]= rl;

		return 1;
	}
}

/********************************* Exported **********************************/

/* if you want to know exactly what has been done */
RenderResult *RE_AcquireResultRead(Render *re)
{
	if(re) {
		BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_READ);
		return re->result;
	}

	return NULL;
}

RenderResult *RE_AcquireResultWrite(Render *re)
{
	if(re) {
		BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_WRITE);
		return re->result;
	}

	return NULL;
}

void RE_SwapResult(Render *re, RenderResult **rr)
{
	/* for keeping render buffers */
	if(re) {
		SWAP(RenderResult*, re->result, *rr);
	}
}

void RE_ReleaseResult(Render *re)
{
	if(re)
		BLI_rw_mutex_unlock(&re->resultmutex);
}

/* fill provided result struct with what's currently active or done */
void RE_AcquireResultImage(Render *re, RenderResult *rr)
{
	memset(rr, 0, sizeof(RenderResult));

	if(re) {
		BLI_rw_mutex_lock(&re->resultmutex, THREAD_LOCK_READ);

		if(re->result) {
			RenderLayer *rl;
			
			rr->rectx= re->result->rectx;
			rr->recty= re->result->recty;
			
			rr->rectf= re->result->rectf;
			rr->rectz= re->result->rectz;
			rr->rect32= re->result->rect32;
			
			/* active layer */
			rl= render_get_active_layer(re, re->result);

			if(rl) {
				if(rr->rectf==NULL)
					rr->rectf= rl->rectf;
				if(rr->rectz==NULL)
					rr->rectz= RE_RenderLayerGetPass(rl, SCE_PASS_Z);	
			}

			rr->layers= re->result->layers;
		}
	}
}

void RE_ReleaseResultImage(Render *re)
{
	if(re)
		BLI_rw_mutex_unlock(&re->resultmutex);
}

/* caller is responsible for allocating rect in correct size! */
void RE_ResultGet32(Render *re, unsigned int *rect)
{
	RenderResult rres;
	
	RE_AcquireResultImage(re, &rres);

	if(rres.rect32) 
		memcpy(rect, rres.rect32, sizeof(int)*rres.rectx*rres.recty);
	else if(rres.rectf) {
		float *fp= rres.rectf;
		int tot= rres.rectx*rres.recty;
		char *cp= (char *)rect;
		
		if (re->params.r.color_mgt_flag & R_COLOR_MANAGEMENT) {
			/* Finally convert back to sRGB rendered image */ 
			for(;tot>0; tot--, cp+=4, fp+=4) {
				cp[0] = FTOCHAR(linearrgb_to_srgb(fp[0]));
				cp[1] = FTOCHAR(linearrgb_to_srgb(fp[1]));
				cp[2] = FTOCHAR(linearrgb_to_srgb(fp[2]));
				cp[3] = FTOCHAR(fp[3]);
			}
		}
		else {
			/* Color management is off : no conversion necessary */
			for(;tot>0; tot--, cp+=4, fp+=4) {
				cp[0] = FTOCHAR(fp[0]);
				cp[1] = FTOCHAR(fp[1]);
				cp[2] = FTOCHAR(fp[2]);
				cp[3] = FTOCHAR(fp[3]);
			}
		}
	}
	else
		/* else fill with black */
		memset(rect, 0, sizeof(int)*re->rectx*re->recty);

	RE_ReleaseResultImage(re);
}


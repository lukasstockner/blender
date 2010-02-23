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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2006 Blender Foundation
 * All rights reserved.
 *
 * Contributors: Hos, Robert Wenzlaff.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdio.h>
#include <float.h>
#include <math.h>
#include <string.h>

#include "BLI_math.h"

#include "BKE_colortools.h"
#include "BKE_material.h"
#include "BKE_node.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"

#include "DNA_group_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"

/* local include */
#include "cache.h"
#include "database.h"
#include "diskocclusion.h"
#include "lamp.h"
#include "material.h"
#include "object_mesh.h"
#include "pixelfilter.h"
#include "raytrace.h"
#include "render_types.h"
#include "sampler.h"
#include "shading.h"
#include "shadowbuf.h"
#include "sss.h"
#include "texture_stack.h"

/**************************** Transparent Shadows ****************************/

/* called from ray.c */
void shade_color(Render *re, ShadeInput *shi, ShadeResult *shr)
{
	ShadeMaterial *smat= &shi->material;

	mat_shading_begin(re, shi, smat);

	mat_color(shr->diff, smat);
	shr->alpha= mat_alpha(smat);

	mat_shading_end(re, smat);
}

/*********************************** Ramps ***********************************/

/* ramp for at end of shade */
static void shade_surface_result_ramps(Render *re, ShadeInput *shi, ShadeResult *shr)
{
	Material *ma= shi->material.mat;
	float col[4], fac;
	float *diff= shr->diff;
	float *spec= shr->spec;

	if(ma->ramp_col && ma->rampin_col==MA_RAMP_IN_RESULT) {
		fac= rgb_to_grayscale(diff);
		do_colorband(ma->ramp_col, fac, col);
		
		/* blending method */
		fac= col[3]*ma->rampfac_col;
		ramp_blend(ma->rampblend_col, diff, diff+1, diff+2, fac, col);
	}

	if(ma->ramp_spec && ma->rampin_spec==MA_RAMP_IN_RESULT) {
		fac= rgb_to_grayscale(spec);
		do_colorband(ma->ramp_spec, fac, col);
		
		/* blending method */
		fac= col[3]*ma->rampfac_spec;
		ramp_blend(ma->rampblend_spec, spec, spec+1, spec+2, fac, col);
	}
}

/***************************** Ambient Occlusion *****************************/

/* pure AO, check for raytrace and world should have been done */
/* preprocess, textures were not done, don't use shi->material.amb for that reason */
void ambient_occlusion(Render *re, ShadeInput *shi)
{
	if((re->db.wrld.ao_gather_method == WO_AOGATHER_APPROX) && shi->material.mat->amb!=0.0f)
		disk_occlusion_sample(re, shi);
	else if((re->params.r.mode & R_RAYTRACE) && shi->material.mat->amb!=0.0f) {
		int thread= shi->shading.thread;
		float *ao= (re->db.wrld.mode & WO_AMB_OCC)? shi->shading.ao: NULL;
		float *env= (re->db.wrld.mode & WO_ENV_LIGHT)? shi->shading.env: NULL;
		float *indirect= (re->db.wrld.mode & WO_INDIRECT_LIGHT)? shi->shading.indirect: NULL;
			
		if(re->db.irrcache[thread]) {
			ShadeGeometry *geom= &shi->geometry;

			irr_cache_lookup(re, shi, re->db.irrcache[thread],
				ao, env, indirect,
				geom->co, geom->dxco, geom->dyco, geom->vn, 0);
		}
		else
			ray_ao_env_indirect(re, shi, ao, env, indirect, NULL);

		if(ao)
			ao[1]= ao[2]= ao[0];
	}
	else
		shi->shading.ao[0]= shi->shading.ao[1]= shi->shading.ao[2]= 1.0f;
}

/* wrld mode was checked for */
static void ambient_occlusion_apply(Render *re, ShadeInput *shi, ShadeResult *shr)
{
	float f= re->db.wrld.aoenergy;
	float tmp[3], tmpspec[3], color[3];

	copy_v3_v3(shr->ao, shi->shading.ao);

	if(!(shi->shading.combinedflag & SCE_PASS_AO))
		return;

	if(f == 0.0f)
		return;

	if(re->db.wrld.aomix==WO_AOADD) {
		mat_color(color, &shi->material);

		shr->diff[0] += shi->shading.ao[0]*color[0]*f;
		shr->diff[1] += shi->shading.ao[1]*color[1]*f;
		shr->diff[2] += shi->shading.ao[2]*color[2]*f;
	}
	else if(re->db.wrld.aomix==WO_AOMUL) {
		mul_v3_v3v3(tmp, shr->diff, shi->shading.ao);
		mul_v3_v3v3(tmpspec, shr->spec, shi->shading.ao);

		if(f == 1.0f) {
			copy_v3_v3(shr->diff, tmp);
			copy_v3_v3(shr->spec, tmpspec);
		}
		else {
			interp_v3_v3v3(shr->diff, shr->diff, tmp, f);
			interp_v3_v3v3(shr->spec, shr->spec, tmpspec, f);
		}
	}
}

void environment_lighting_apply(Render *re, ShadeInput *shi, ShadeResult *shr)
{
	float color[3], f= re->db.wrld.ao_env_energy*shi->material.amb;

	if(f == 0.0f)
		return;
	
	mat_color(color, &shi->material);

	shr->env[0]= shi->shading.env[0]*color[0]*f;
	shr->env[1]= shi->shading.env[1]*color[1]*f;
	shr->env[2]= shi->shading.env[2]*color[2]*f;

	if(shi->shading.combinedflag & SCE_PASS_ENVIRONMENT)
		add_v3_v3(shr->diff, shr->env);
}

static void indirect_lighting_apply(Render *re, ShadeInput *shi, ShadeResult *shr)
{
	float color[3], f= re->db.wrld.ao_indirect_energy;

	if(f == 0.0f)
		return;

	mat_color(color, &shi->material);

	shr->indirect[0]= shi->shading.indirect[0]*color[0]*f;
	shr->indirect[1]= shi->shading.indirect[1]*color[1]*f;
	shr->indirect[2]= shi->shading.indirect[2]*color[2]*f;

	if(shi->shading.combinedflag & SCE_PASS_INDIRECT)
		add_v3_v3(shr->diff, shr->indirect);
}

static void shade_compute_ao(Render *re, ShadeInput *shi, ShadeResult *shr)
{
	int passflag= shi->shading.passflag;

	if(re->db.wrld.mode & (WO_AMB_OCC|WO_ENV_LIGHT|WO_INDIRECT_LIGHT)) {
		if(((passflag & SCE_PASS_COMBINED) && (shi->shading.combinedflag & (SCE_PASS_AO|SCE_PASS_ENVIRONMENT|SCE_PASS_INDIRECT)))
			|| (passflag & (SCE_PASS_AO|SCE_PASS_ENVIRONMENT|SCE_PASS_INDIRECT))) {
			/* AO was calculated for scanline already */
			if(shi->shading.depth)
				ambient_occlusion(re, shi);
		}
	}
}

/********************************* Shading ***********************************/

static float shade_phong_correction(Render *re, LampRen *lar, ShadeInput *shi, float *lv)
{
	Material *ma= shi->material.mat;
	float phongcorr= 1.0f;

	/* phong threshold to prevent backfacing faces having artefacts on ray shadow (terminator problem) */
	/* this complex construction screams for a nicer implementation! (ton) */
	if(re->params.r.mode & R_SHADOW) {
		if((ma->mode & MA_SHADOW) && !shi->geometry.tangentvn) {
			float inp= dot_v3v3(shi->geometry.vn, lv);

			if(lar->type==LA_HEMI || lar->type==LA_AREA);
			else if((ma->mode & MA_RAYBIAS) && (lar->mode & LA_SHAD_RAY) && (shi->primitive.vlr->flag & R_SMOOTH)) {
				float thresh= shi->primitive.obr->ob->smoothresh;
				if(inp>thresh)
					phongcorr= (inp-thresh)/(inp*(1.0f-thresh));
				else
					phongcorr= 0.0f;
			}
			else if(ma->sbias!=0.0f && ((lar->mode & LA_SHAD_RAY) || lar->shb)) {
				if(inp>ma->sbias)
					phongcorr= (inp-ma->sbias)/(inp*(1.0f-ma->sbias));
				else
					phongcorr= 0.0f;
			}
		}
	}
	
	return phongcorr;
}

/********************************** Only Shadow ******************************/

static void shade_surface_only_shadow(Render *re, ShadeInput *shi, ShadeResult *shr)
{
	if(re->params.r.mode & R_SHADOW) {
		ListBase *lights;
		LampRen *lar;
		GroupObject *go;
		float accum, alpha= mat_alpha(&shi->material);
		
		accum= 0.0f;
		
		lights= lamps_get(re, shi);
		for(go=lights->first; go; go= go->next) {
			lar= go->lampren;

			if(lar && !lamp_skip(re, lar, shi) && (lar->shb || (lar->mode & LA_SHAD_RAY))) {
				float lv[3], lashdw[3], lainf[3];

				if(!lamp_sample(lv, lainf, lashdw, re, lar, shi, shi->geometry.co, NULL))
					continue;

				if(dot_v3v3(shi->geometry.vn, lv) <= 0.0f)
					continue;

				lainf[0] *= (1.0f - lashdw[0]);
				lainf[1] *= (1.0f - lashdw[1]);
				lainf[2] *= (1.0f - lashdw[2]);

				accum += rgb_to_grayscale(lainf);
			}
		}

		shr->alpha= alpha*accum;
	}
	
	/* quite disputable this...  also note it doesn't mirror-raytrace */	
	if((re->db.wrld.mode & (WO_AMB_OCC|WO_ENV_LIGHT)) && shi->material.amb!=0.0f) {
		float f;
		
		if(re->db.wrld.mode & WO_AMB_OCC) {
			f= re->db.wrld.aoenergy*shi->material.amb;

			if(re->db.wrld.aomix==WO_AOADD)
				shr->alpha += f*(1.0f - rgb_to_grayscale(shi->shading.ao));
			else
				shr->alpha= (1.0f - f)*shr->alpha + f*(1.0f - (1.0f - shr->alpha)*rgb_to_grayscale(shi->shading.ao));
		}

		if(re->db.wrld.mode & WO_ENV_LIGHT) {
			f= re->db.wrld.ao_env_energy*shi->material.amb;
			shr->alpha += f*(1.0f - rgb_to_grayscale(shi->shading.env));
		}
	}
}

/**************************** Color & Alpha Pass *****************************/

static void shade_color_alpha(Render *re, ShadeInput *shi, ShadeResult *shr)
{
	Material *ma= shi->material.mat;
	int passflag= shi->shading.passflag;
	int pre_sss= ((ma->sss_flag & MA_DIFF_SSS) && !sss_pass_done(re, ma));

	/* SSS hack */
	if(pre_sss) {
		if(ma->sss_texfac == 0.0f) {
			shi->material.r= 1.0f;
			shi->material.g= 1.0f;
			shi->material.b= 1.0f;
			shi->material.alpha= 1.0f;
		}
		else {
			shi->material.r= pow(shi->material.r, ma->sss_texfac);
			shi->material.g= pow(shi->material.g, ma->sss_texfac);
			shi->material.b= pow(shi->material.b, ma->sss_texfac);
			shi->material.alpha= pow(shi->material.alpha, ma->sss_texfac);
		}
	}

	/* alpha pass */
	shr->alpha= mat_alpha(&shi->material);

	/* color pass */
	if((passflag & SCE_PASS_RGBA) || (ma->sss_flag & MA_DIFF_SSS)) {
		mat_color(shr->col, &shi->material);

		mul_v3_fl(shr->col, shr->alpha);
		shr->col[3]= shr->alpha;
	}
}

/**************************** Direct Lighting ********************************/

static void shade_surface_emission(ShadeInput *shi, ShadeResult *shr)
{
	mat_emit(shr->emit, &shi->material, &shi->geometry, shi->shading.thread);
}

void shade_jittered_coords(Render *re, ShadeInput *shi, int max, float jitco[RE_MAX_OSA][3], int *totjitco)
{
	/* magic numbers for reordering sample positions to give better
	 * results with adaptive sample, when it usually only takes 4 samples */
	int order8[8] = {0, 1, 5, 6, 2, 3, 4, 7};
	int order11[11] = {1, 3, 8, 10, 0, 2, 4, 5, 6, 7, 9};
	int order16[16] = {1, 3, 9, 12, 0, 6, 7, 8, 13, 2, 4, 5, 10, 11, 14, 15};
	int count = pxf_mask_count(&re->sample, shi->shading.mask);

	/* for better antialising shadow samples are distributed over the subpixel
	 * sample coordinates, this only works for raytracing depth 0 though */
	if(!shi->primitive.strand && shi->shading.depth == 0 && count > 1 && count <= max) {
		float xs, ys, zs, view[3];
		int samp, ordsamp, tot= 0;

		for(samp=0; samp<re->params.osa; samp++) {
			if(re->params.osa == 8) ordsamp = order8[samp];
			else if(re->params.osa == 11) ordsamp = order11[samp];
			else if(re->params.osa == 16) ordsamp = order16[samp];
			else ordsamp = samp;

			if(shi->shading.mask & (1<<ordsamp)) {
				float ofs[2];

				/* zbuffer has this inverse corrected, ensures xs,ys are inside pixel */
				pxf_sample_offset(&re->sample, ordsamp, ofs);
				xs= (float)shi->geometry.scanco[0] + ofs[0];
				ys= (float)shi->geometry.scanco[1] + ofs[1];
				zs= shi->geometry.scanco[2];

				shade_input_calc_viewco(re, shi, xs, ys, zs, view, NULL, jitco[tot], NULL, NULL);
				tot++;
			}
		}

		*totjitco= tot;
	}
	else {
		copy_v3_v3(jitco[0], shi->geometry.co);
		*totjitco= 1;
	}
}

static void shade_lamp_accumulate(Render *re, LampRen *lar, ShadeInput *shi, ShadeResult *shr, float lv[3], float lainf[3], float lashdw[3], int passflag)
{
	Material *ma= shi->material.mat;
	float diff[3] = {0.0f, 0.0f, 0.0f};
	float spec[3] = {0.0f, 0.0f, 0.0f};

	/* if fully shadowed, try to avoid shading */
	if(is_zero_v3(lashdw) && !((lar->mode & LA_ONLYSHADOW) || (passflag & SCE_PASS_SHADOW)))
		return;

	/* phong correction */
	if((re->params.r.mode & R_SHADOW) && (ma->mode & MA_SHADOW))
		mul_v3_fl(lashdw, shade_phong_correction(re, lar, shi, lv));

	/* diffuse */
	if(!(lar->mode & LA_NO_DIFF) && (passflag & (SCE_PASS_COMBINED|SCE_PASS_DIFFUSE|SCE_PASS_SHADOW))) {
		mat_bsdf_f(diff, &shi->material, &shi->geometry, shi->shading.thread, lv, BSDF_DIFFUSE);

		if(lar->mode & LA_ONLYSHADOW) {
			shr->onlyshadow[0] += diff[0]*lainf[0]*(1.0f - lashdw[0]);
			shr->onlyshadow[1] += diff[1]*lainf[1]*(1.0f - lashdw[1]);
			shr->onlyshadow[2] += diff[2]*lainf[2]*(1.0f - lashdw[2]);
		}
		else {
			shr->diff[0] += diff[0]*lainf[0]*lashdw[0];
			shr->diff[1] += diff[1]*lainf[1]*lashdw[1];
			shr->diff[2] += diff[2]*lainf[2]*lashdw[2];
		}
	}

	/* specular */
	if(!(lar->mode & LA_NO_SPEC) && (passflag & (SCE_PASS_COMBINED|SCE_PASS_SPEC|SCE_PASS_SHADOW))) {
		mat_bsdf_f(spec, &shi->material, &shi->geometry, shi->shading.thread, lv, BSDF_SPECULAR);

		if(lar->mode & LA_ONLYSHADOW) {
			shr->onlyshadow[0] += spec[0]*lainf[0]*(1.0f - lashdw[0]);
			shr->onlyshadow[1] += spec[1]*lainf[1]*(1.0f - lashdw[1]);
			shr->onlyshadow[2] += spec[2]*lainf[2]*(1.0f - lashdw[2]);
		}
		else {
			shr->spec[0] += spec[0]*lainf[0]*lashdw[0];
			shr->spec[1] += spec[1]*lainf[1]*lashdw[1];
			shr->spec[2] += spec[2]*lainf[2]*lashdw[2];
		}
	}

	/* accumulate */
	if(passflag & SCE_PASS_SHADOW) {
		/* add unshadowed in shadow pass, for division in the end */
		shr->shad[0] += (diff[0] + spec[0])*lainf[0];
		shr->shad[1] += (diff[1] + spec[1])*lainf[1];
		shr->shad[2] += (diff[2] + spec[2])*lainf[2];
	}
}

static int shade_full_osa(Render *re, ShadeInput *shi)
{
	return (re->params.r.mode & R_OSA) && (re->params.osa > 0) && (shi->primitive.vlr->flag & R_FULL_OSA);
}

static int shade_lamp_tot_samples(Render *re, LampRen *lar, ShadeInput *shi)
{
	int tot= lar->ray_totsamp;

	/* XXX temporary hack */
	if(shi->shading.depth)
		return 1;

	if(tot <= 1)
		return 1;
	else if(shade_full_osa(re, shi))
		return tot/re->params.osa + 1;
	else
		return tot;
}

static void shade_lamp_multi(Render *re, LampRen *lar, ShadeInput *shi, ShadeResult *shr, int passflag)
{
	/* full shading + shadow multisampling of lamp */
	QMCSampler *qsa;
	float jitco[RE_MAX_OSA][3], fac= 1.0f;
	int sample, totsample, totjitco= 0;

	totsample= shade_lamp_tot_samples(re, lar, shi);
	shade_jittered_coords(re, shi, totsample, jitco, &totjitco);

	if(lar->ray_samp_method==LA_SAMP_HAMMERSLEY) {
		/* fix number of samples */
		qsa= sampler_acquire(re, shi->shading.thread, SAMP_TYPE_HAMMERSLEY, totsample);
		fac= 1.0f/totsample;

		for(sample=0; sample<totsample; sample++) {
			float lv[3], lainf[3], lashdw[3], r[2];

			/* get lamp vector, influence and shadow */
			sampler_get_float_2d(r, qsa, sample);
			if(!lamp_sample(lv, lainf, lashdw, re, lar, shi, jitco[sample%totjitco], r))
				continue;

			/* weighting for multisample */
			mul_v3_fl(lainf, fac);

			shade_lamp_accumulate(re, lar, shi, shr, lv, lainf, lashdw, passflag);
		}
	}
	else {
		float adapt_thresh= lar->adapt_thresh;
		int min_adapt_samples= 4;
		float avg= 0.0f;
		float pre_diff[3], pre_spec[3], pre_onlyshadow[3], pre_shad[3];

		copy_v3_v3(pre_diff, shr->diff);
		copy_v3_v3(pre_spec, shr->spec);
		copy_v3_v3(pre_onlyshadow, shr->onlyshadow);
		copy_v3_v3(pre_shad, shr->shad);

		/* adaptive number of samples */
		qsa = sampler_acquire(re, shi->shading.thread, SAMP_TYPE_HALTON, totsample);
		fac= 1.0f/totsample;

		for(sample=0; sample<totsample; sample++) {
			float lv[3], lainf[3], lashdw[3], r[2];

			/* get lamp vector, influence and shadow */
			sampler_get_float_2d(r, qsa, sample);
			if(!lamp_sample(lv, lainf, lashdw, re, lar, shi, jitco[sample%totjitco], r))
				continue;

			shade_lamp_accumulate(re, lar, shi, shr, lv, lainf, lashdw, passflag);
			avg += rgb_to_grayscale(lashdw);

			/* adaptive sampling - consider samples below threshold as in shadow (or vice versa) and exit early */
			if((totsample > min_adapt_samples) && (adapt_thresh > 0.0) && (sample > totsample/3)) {
				/* XXX if(isec->mode==RE_RAY_SHADOW_TRA) {
					if((shadfac[3] / sample > (1.0-adapt_thresh)) || (shadfac[3] / sample < adapt_thresh))
						break;
					else if(adaptive_sample_variance(sample, shadfac, colsq, adapt_thresh))
						break;
				}
				else*/ if ((avg/sample > (1.0-adapt_thresh)) || (avg/sample < adapt_thresh))
					break;
			}
		}

		/* XXX evil hack */
		sub_v3_v3v3(shr->diff, shr->diff, pre_diff);
		madd_v3_v3v3fl(shr->diff, pre_diff, shr->diff, 1.0f/sample);

		sub_v3_v3v3(shr->spec, shr->spec, pre_spec);
		madd_v3_v3v3fl(shr->spec, pre_spec, shr->spec, 1.0f/sample);

		sub_v3_v3v3(shr->onlyshadow, shr->onlyshadow, pre_onlyshadow);
		madd_v3_v3v3fl(shr->onlyshadow, pre_onlyshadow, shr->onlyshadow, 1.0f/sample);

		sub_v3_v3v3(shr->shad, shr->shad, pre_shad);
		madd_v3_v3v3fl(shr->shad, pre_shad, shr->shad, 1.0f/sample);
	}

	sampler_release(re, qsa);
}

static void shade_lamp_multi_shadow(Render *re, LampRen *lar, ShadeInput *shi, ShadeResult *shr, int passflag)
{
	/* only multisample shadow, we do this for ray shadows at depth 0, to
	   get antialiasing, shadow buffers do this automatically with soft */
	QMCSampler *qsa;
	float jitco[RE_MAX_OSA][3], fac= 1.0f;
	float accuminf[3], accumshdw[3], accumlv[3];
	int sample, totsample, totjitco= 0;

	totsample= (re->params.osa > 4)? re->params.osa: 5;

	qsa= sampler_acquire(re, shi->shading.thread, SAMP_TYPE_HAMMERSLEY, totsample);
	shade_jittered_coords(re, shi, totsample, jitco, &totjitco);
	fac= 1.0f/totsample;

	zero_v3(accuminf);
	zero_v3(accumshdw);
	zero_v3(accumlv);

	for(sample=0; sample<totsample; sample++) {
		float lv[3], lainf[3], lashdw[3], r[2];

		/* get lamp vector, influence and shadow */
		sampler_get_float_2d(r, qsa, sample);
		if(!lamp_sample(lv, lainf, lashdw, re, lar, shi, jitco[sample%totjitco], r))
			continue;

		madd_v3_v3fl(accuminf, lainf, fac);
		madd_v3_v3fl(accumshdw, lashdw, fac);
		add_v3_v3(accumlv, lv);
	}

	normalize_v3(accumlv);

	shade_lamp_accumulate(re, lar, shi, shr, accumlv, accuminf, accumshdw, passflag);

	sampler_release(re, qsa);
}

static void shade_lamp_single(Render *re, LampRen *lar, ShadeInput *shi, ShadeResult *shr, int passflag)
{
	/* simple single shadow + shading evaluation */
	float lv[3], lainf[3], lashdw[3];

	if(!lamp_sample(lv, lainf, lashdw, re, lar, shi, shi->geometry.co, NULL))
		return;

	shade_lamp_accumulate(re, lar, shi, shr, lv, lainf, lashdw, passflag);
}

void shade_surface_direct(Render *re, ShadeInput *shi, ShadeResult *shr)
{
	GroupObject *go;
	ListBase *lights;
	LampRen *lar;
	int passflag= shi->shading.passflag;

	/* direct specular & diffuse */
	if(!(passflag & (SCE_PASS_COMBINED|SCE_PASS_DIFFUSE|SCE_PASS_SPEC|SCE_PASS_SHADOW)))
		return;

	lights= lamps_get(re, shi);

	zero_v3(shr->onlyshadow);

	/* accumulates in shr->diff and shr->spec, and unshadowed in shr->shad */
	for(go=lights->first; go; go= go->next) {
		lar= go->lampren;

		if(lar && !lamp_skip(re, lar, shi)) {
			/* pick type of evualation */
			if(lar->mode & LA_SHAD_RAY) {
				if(lar->ray_totsamp > 1)
					shade_lamp_multi(re, lar, shi, shr, passflag);
				else if(shi->shading.depth == 0 && !shade_full_osa(re, shi))
					shade_lamp_multi_shadow(re, lar, shi, shr, passflag);
				else
					shade_lamp_single(re, lar, shi, shr, passflag);
			}
			else {
				if(lar->type == LA_AREA)
					shade_lamp_multi(re, lar, shi, shr, passflag);
				else
					shade_lamp_single(re, lar, shi, shr, passflag);
			}
		}
	}

	/* only shadow apply, do in grayscale to avoid getting ugly discolorations */
	if(!is_zero_v3(shr->onlyshadow)) {
		float intensity = rgb_to_grayscale(shr->diff) + rgb_to_grayscale(shr->spec);

		if(intensity > 0.0f) {
			float shadow = rgb_to_grayscale(shr->onlyshadow);
			float factor = maxf((intensity - shadow)/intensity, 0.0f);

			mul_v3_fl(shr->diff, factor);
			mul_v3_fl(shr->spec, factor);
		}
	}

	/* prevent only shadow lamps from producing negative colors.*/
	if(shr->spec[0] < 0) shr->spec[0] = 0;
	if(shr->spec[1] < 0) shr->spec[1] = 0;
	if(shr->spec[2] < 0) shr->spec[2] = 0;

	if(shr->diff[0] < 0) shr->diff[0] = 0;
	if(shr->diff[1] < 0) shr->diff[1] = 0;
	if(shr->diff[2] < 0) shr->diff[2] = 0;

	/* we now have unshadowed result in shr->shad, turn it into shadow by
	   dividing shadowed diffuse and specular by unshadowed. it's not possible
	   to clearly do the separation for compositing ... */
	if(passflag & SCE_PASS_SHADOW) {
		if(shr->shad[0]!=0.0f) shr->shad[0]= (shr->diff[0] + shr->spec[0])/shr->shad[0];
		if(shr->shad[1]!=0.0f) shr->shad[1]= (shr->diff[1] + shr->spec[1])/shr->shad[1];
		if(shr->shad[2]!=0.0f) shr->shad[2]= (shr->diff[2] + shr->spec[2])/shr->shad[2];
	}
}

static void shade_surface_indirect(Render *re, ShadeInput *shi, ShadeResult *shr, int backside)
{
	int passflag= shi->shading.passflag;

	shade_compute_ao(re, shi, shr); /* .ao */

	/* add AO in combined? */
	if((re->params.r.mode & R_RAYTRACE) || re->db.wrld.ao_gather_method == WO_AOGATHER_APPROX) {
		if(re->db.wrld.mode & WO_AMB_OCC)
			ambient_occlusion_apply(re, shi, shr);

		if(re->db.wrld.mode & WO_ENV_LIGHT)
			environment_lighting_apply(re, shi, shr);

		if(re->db.wrld.mode & WO_INDIRECT_LIGHT)
			indirect_lighting_apply(re, shi, shr);
	}
		
	/* ambient light */
	madd_v3_v3fl(shr->diff, &re->db.wrld.ambr, shi->material.amb);

	/* refcol is for envmap only */
	if(shi->material.refcol[0]!=0.0f) {
		float result[3];
		float *refcol= shi->material.refcol;
		float *mircol= &shi->material.mirr;

		// TODO NSHAD: how to integrate this?
		
		result[0]= mircol[0]*refcol[1] + (1.0f - mircol[0]*refcol[0])*shr->diff[0];
		result[1]= mircol[1]*refcol[2] + (1.0f - mircol[1]*refcol[0])*shr->diff[1];
		result[2]= mircol[2]*refcol[3] + (1.0f - mircol[2]*refcol[0])*shr->diff[2];
		
		if(passflag & SCE_PASS_REFLECT)
			sub_v3_v3v3(shr->refl, result, shr->diff);
		
		if(shi->shading.combinedflag & SCE_PASS_REFLECT)
			add_v3_v3(shr->spec, result);
	}

	/* depth >= 1 when ray-shading */
	if(!backside && shi->shading.depth==0) {
		if(re->params.r.mode & R_RAYTRACE) {
			if(shi->material.ray_mirror!=0.0f || ((shi->material.mat->mode & MA_TRANSP) && (shi->material.mat->mode & MA_RAYTRANSP) && shr->alpha!=1.0f)) {
				/* ray trace works on combined, but gives pass info */
				ray_trace_specular(re, shi, shr);
			}
		}
	}
}

static void shade_surface_sss(Render *re, ShadeInput *shi, ShadeResult *shr)
{
	Material *ma= shi->material.mat;
	int passflag= shi->shading.passflag;

	if((ma->sss_flag & MA_DIFF_SSS) && (passflag & (SCE_PASS_COMBINED|SCE_PASS_DIFFUSE))) {
		float sss[3], col[3], invalpha, texfac= ma->sss_texfac;

		if(!sss_sample(re, ma, shi->geometry.co, sss)) {
			/* preprocess stage */
			copy_v3_v3(shr->sss, shr->diff);
			shr->sss[3]= shr->alpha; // TODO NSHAD solve SSS + alpha
		}
		else {
			/* rendering stage */
			invalpha= (shr->col[3] > FLT_EPSILON)? 1.0f/shr->col[3]: 1.0f;

			if(texfac==0.0f) {
				copy_v3_v3(col, shr->col);
				mul_v3_fl(col, invalpha);
			}
			else if(texfac==1.0f) {
				col[0]= col[1]= col[2]= 1.0f;
				mul_v3_fl(col, invalpha);
			}
			else {
				copy_v3_v3(col, shr->col);
				mul_v3_fl(col, invalpha);
				col[0]= pow(col[0], 1.0f-texfac);
				col[1]= pow(col[1], 1.0f-texfac);
				col[2]= pow(col[2], 1.0f-texfac);
			}

			shr->diff[0]= sss[0]*col[0];
			shr->diff[1]= sss[1]*col[1];
			shr->diff[2]= sss[2]*col[2];
		}
	}
}

static void shade_modulate_object_color(Render *re, ShadeInput *shi, ShadeResult *shr)
{
	Material *ma= shi->material.mat;

	if((ma->shade_flag & MA_OBCOLOR) && shi->primitive.obr->ob) {
		float obcol[4];

		copy_v4_v4(obcol, shi->primitive.obr->ob->col);
		CLAMP(obcol[3], 0.0f, 1.0f);

		shr->combined[0] *= obcol[0];
		shr->combined[1] *= obcol[1];
		shr->combined[2] *= obcol[2];
		shr->alpha *= obcol[3];
	}
}

void shade_surface(Render *re, ShadeInput *shi, ShadeResult *shr, int backside)
{
	Material *ma= shi->material.mat;
	
	memset(shr, 0, sizeof(ShadeResult));
	
	/* only shadow is in a separate loop */
	if(ma->mode & MA_ONLYSHADOW) {
		shade_surface_only_shadow(re, shi, shr);
		return;
	}
	
	/* preprocess */
	mat_shading_begin(re, shi, &shi->material);

	/* color and alpha pass */
	shade_color_alpha(re, shi, shr);

	/* shadeless is simply color copy */
	if(ma->mode & MA_SHLESS) {
		mat_color(shr->combined, &shi->material);
		mat_shading_end(re, &shi->material);
		return;
	}

	/* emission */
	shade_surface_emission(shi, shr);

	/* direct lights */
	shade_surface_direct(re, shi, shr); /* .diff, .shad, .spec */

	/* indirect light from environment and other surfaces */
	shade_surface_indirect(re, shi, shr, backside); /* .diff, .spec, .refl, .refr */

	/* subsurface scattering */
	shade_surface_sss(re, shi, shr);

	/* result ramps */
	shade_surface_result_ramps(re, shi, shr);

	/* add diffuse + specular into combined */
	add_v3_v3v3(shr->combined, shr->emit, shr->diff);

	if(shi->shading.combinedflag & SCE_PASS_SPEC)
		add_v3_v3(shr->combined, shr->spec);

	/* modulate by the object color */
	shade_modulate_object_color(re, shi, shr);

	shr->combined[3]= shr->alpha;

	mat_shading_end(re, &shi->material);
}


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
#include "database.h"
#include "diskocclusion.h"
#include "lamp.h"
#include "material.h"
#include "object_mesh.h"
#include "raytrace.h"
#include "render_types.h"
#include "rendercore.h"
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
	else if((re->params.r.mode & R_RAYTRACE) && shi->material.mat->amb!=0.0f)
		ray_ao(re, shi, shi->shading.ao, shi->shading.env);
	else
		shi->shading.ao[0]= shi->shading.ao[1]= shi->shading.ao[2]= 1.0f;
}

/* wrld mode was checked for */
static void ambient_occlusion_apply(Render *re, ShadeInput *shi, ShadeResult *shr)
{
	float f= re->db.wrld.aoenergy;
	float tmp[3], tmpspec[3];

	if(f == 0.0f)
		return;

	if(re->db.wrld.aomix==WO_AOADD) {
		shr->diff[0] += shi->shading.ao[0]*shi->material.r*shi->material.refl*f;
		shr->diff[1] += shi->shading.ao[1]*shi->material.g*shi->material.refl*f;
		shr->diff[2] += shi->shading.ao[2]*shi->material.b*shi->material.refl*f;
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

static void environment_lighting_apply(Render *re, ShadeInput *shi, ShadeResult *shr)
{
	float f= re->db.wrld.ao_env_energy*shi->material.amb;

	if(f == 0.0f)
		return;
	
	shr->diff[0] += shi->shading.env[0]*shi->material.r*shi->material.refl*f;
	shr->diff[1] += shi->shading.env[1]*shi->material.g*shi->material.refl*f;
	shr->diff[2] += shi->shading.env[2]*shi->material.b*shi->material.refl*f;
}

static void indirect_lighting_apply(Render *re, ShadeInput *shi, ShadeResult *shr)
{
	float f= re->db.wrld.ao_indirect_energy;

	if(f == 0.0f)
		return;

	shr->diff[0] += shi->shading.indirect[0]*shi->material.r*shi->material.refl*f;
	shr->diff[1] += shi->shading.indirect[1]*shi->material.g*shi->material.refl*f;
	shr->diff[2] += shi->shading.indirect[2]*shi->material.b*shi->material.refl*f;
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
			copy_v3_v3(shr->ao, shi->shading.ao);
			copy_v3_v3(shr->env, shi->shading.env); // XXX multiply
			copy_v3_v3(shr->indirect, shi->shading.indirect); // XXX multiply
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

/*********************************** Lamps ***********************************/

static int shade_lamp_influence(Render *re, LampRen *lar, ShadeInput *shi, float lv[4], float lainf[3], float lashdw[3])
{
	Material *ma= shi->material.mat;

	if(lar->type==LA_YF_PHOTON) return 0;
	if(lar->mode & LA_LAYER) if((lar->lay & shi->primitive.obi->lay)==0) return 0;
	if((lar->lay & shi->shading.lay)==0) return 0;

	/* lamp influence & shadow*/
	if(!lamp_influence(re, lar, shi, lainf, lv))
		return 0;
	
	if((re->params.r.mode & R_SHADOW) && (ma->mode & MA_SHADOW)) {
		float phongcorr;

		lamp_shadow(re, lar, shi, lv, lashdw);

		/* phong correction */
		phongcorr= shade_phong_correction(re, lar, shi, lv);
		mul_v3_fl(lashdw, phongcorr);
	}
	else
		lashdw[0]= lashdw[1]= lashdw[2]= 1.0f;

	return 1;
}

/********************************** Only Shadow ******************************/

static void shade_surface_only_shadow(Render *re, ShadeInput *shi, ShadeResult *shr)
{
	if(re->params.r.mode & R_SHADOW) {
		ListBase *lights;
		LampRen *lar;
		GroupObject *go;
		float inpr, lv[3];
		float *view, lashdw[3];
		float ir, accum, visifac, lampdist;
		float alpha= mat_alpha(&shi->material);
		
		view= shi->geometry.view;

		accum= ir= 0.0f;
		
		lights= lamps_get(re, shi);
		for(go=lights->first; go; go= go->next) {
			lar= go->lampren;

			if(lar==NULL) continue;
			if(lar->type==LA_YF_PHOTON) continue; /* yafray only */
			if(lar->mode & LA_LAYER) if((lar->lay & shi->primitive.obi->lay)==0) continue;
			if((lar->lay & shi->shading.lay)==0) continue;
			
			if(lar->shb || (lar->mode & LA_SHAD_RAY)) {
				visifac= lamp_visibility(lar, shi->geometry.co, shi->geometry.vn, lv, &lampdist);
				ir+= 1.0f;

				if(visifac <= 0.0f) {
					accum+= 1.0f;
					continue;
				}

				inpr= dot_v3v3(shi->geometry.vn, lv);
				if(inpr <= 0.0f) {
					accum+= 1.0f;
					continue;
				}				
				lamp_shadow(re, lar, shi, lv, lashdw);

				accum+= (1.0f-visifac) + (visifac)*rgb_to_grayscale(lashdw);
			}
		}
		if(ir>0.0f) {
			accum/= ir;
			shr->alpha= alpha*(1.0f-accum);
		}
		else shr->alpha= alpha;
	}
	
	/* quite disputable this...  also note it doesn't mirror-raytrace */	
	if((re->db.wrld.mode & (WO_AMB_OCC|WO_ENV_LIGHT|WO_INDIRECT_LIGHT)) && shi->material.amb!=0.0f) {
		float f;
		
		f= 1.0f - shi->shading.ao[0];
		f= re->db.wrld.aoenergy*f*shi->material.amb;
		
		if(re->db.wrld.aomix==WO_AOADD) {
			shr->alpha += f;
			shr->alpha *= f;
		}
		else if(re->db.wrld.aomix==WO_AOMUL) {
			shr->alpha *= f;
		}
	}
}

/*********************************** Shading *********************************/

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

static void shade_surface_emission(ShadeInput *shi, ShadeResult *shr)
{
	mat_emit(shr->emit, &shi->material, &shi->geometry, shi->shading.thread);
}

static void shade_surface_direct(Render *re, ShadeInput *shi, ShadeResult *shr)
{
	GroupObject *go;
	ListBase *lights;
	LampRen *lar;
	int passflag= shi->shading.passflag;

	/* direct specular & diffuse */
	if(!(passflag & (SCE_PASS_COMBINED|SCE_PASS_DIFFUSE|SCE_PASS_SPEC|SCE_PASS_SHADOW)))
		return;

	lights= lamps_get(re, shi);

	/* accumulates in shr->diff and shr->spec, and unshadowed in shr->shad */
	for(go=lights->first; go; go= go->next) {
		float lv[3], lainf[3], lashdw[3];
		float diff[3] = {0.0f, 0.0f, 0.0f};
		float spec[3] = {0.0f, 0.0f, 0.0f};

		lar= go->lampren;
		if(!lar)
			continue;

		/* get lamp vector, influence and shadow */
		if(!shade_lamp_influence(re, lar, shi, lv, lainf, lashdw))
			continue;
		
		/* if fully shadowed, try to avoid shading */
		if(is_zero_v3(lashdw) && !((lar->mode & LA_ONLYSHADOW) || (passflag & SCE_PASS_SHADOW)))
			continue;

		/* diffuse */
		if(!(lar->mode & LA_NO_DIFF) && (passflag & (SCE_PASS_COMBINED|SCE_PASS_DIFFUSE|SCE_PASS_SHADOW))) {
			mat_bsdf_f(diff, &shi->material, &shi->geometry, shi->shading.thread, lv, BSDF_DIFFUSE);

			if(lar->mode & LA_ONLYSHADOW) {
				shr->diff[0] -= diff[0]*lainf[0]*(1.0f - lashdw[0]);
				shr->diff[1] -= diff[1]*lainf[1]*(1.0f - lashdw[1]);
				shr->diff[2] -= diff[2]*lainf[2]*(1.0f - lashdw[2]);
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
				shr->spec[0] -= spec[0]*lainf[0]*(1.0f - lashdw[0]);
				shr->spec[1] -= spec[1]*lainf[1]*(1.0f - lashdw[1]);
				shr->spec[2] -= spec[2]*lainf[2]*(1.0f - lashdw[2]);
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
	float color[3];
	int passflag= shi->shading.passflag;

	mat_color(color, &shi->material);

	shade_compute_ao(re, shi, shr); /* .ao */

	/* add AO in combined? */
	if((re->params.r.mode & R_RAYTRACE) || re->db.wrld.ao_gather_method == WO_AOGATHER_APPROX) {
		if(re->db.wrld.mode & WO_AMB_OCC)
			if(shi->shading.combinedflag & SCE_PASS_AO)
				ambient_occlusion_apply(re, shi, shr);

		if(re->db.wrld.mode & WO_ENV_LIGHT)
			if(shi->shading.combinedflag & SCE_PASS_ENVIRONMENT)
				environment_lighting_apply(re, shi, shr);

		if(re->db.wrld.mode & WO_INDIRECT_LIGHT)
			if(shi->shading.combinedflag & SCE_PASS_INDIRECT)
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
				ray_trace(re, shi, shr);
				//ray_trace_mirror(re, shi, shr);
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


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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributors: Hos, Robert Wenzlaff.
 * Contributors: 2004/2005/2006 Blender Foundation, full recode
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/* system includes */
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <string.h>
#include <assert.h>

/* External modules: */
#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_jitter.h"
#include "BLI_math.h"
#include "BLI_rand.h"
#include "BLI_threads.h"

#include "BKE_utildefines.h"

#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_group_types.h"

#include "BKE_node.h"
#include "BKE_texture.h"

#include "PIL_time.h"

#include "RE_raytrace.h"

/* local includes */
#include "cache.h"
#include "camera.h"
#include "database.h"
#include "diskocclusion.h"
#include "environment.h"
#include "lamp.h"
#include "object.h"
#include "object_halo.h"
#include "object_mesh.h"
#include "object_strand.h"
#include "part.h"
#include "pixelfilter.h"
#include "render_types.h"
#include "rendercore.h"
#include "result.h"
#include "shading.h"
#include "shadowbuf.h"
#include "sss.h"
#include "sunsky.h"
#include "zbuf.h"

/******************************* Halo **************************************/

static int calchalo_z(HaloRen *har, int zz)
{
	if(har->type & HA_ONLYSKY) {
		if(zz < 0x7FFFFFF0) zz= - 0x7FFFFF;	/* edge render messes zvalues */
	}
	else {
		zz= (zz>>8);
	}
	return zz;
}

static void halo_pixelstruct(Render *re, HaloRen *har, RenderLayer **rlpp, int totsample, int od, float dist, float xn, float yn, PixStr *ps)
{
	float col[4], accol[4], fac;
	int amount, amountm, zz, flarec, sample, fullsample, mask=0;
	int osa= (re->params.osa)? re->params.osa: 1;
	
	fullsample= (totsample > 1);
	amount= 0;
	accol[0]=accol[1]=accol[2]=accol[3]= 0.0f;
	flarec= har->flarec;
	
	while(ps) {
		amountm= pxf_mask_count(&re->sample, ps->mask);
		amount+= amountm;
		
		zz= calchalo_z(har, ps->z);
		if((zz> har->zs) || (har->mat && (har->mat->mode & MA_HALO_SOFT))) {
			if(shadeHaloFloat(re, har, col, zz, dist, xn, yn, flarec)) {
				flarec= 0;

				if(fullsample) {
					for(sample=0; sample<totsample; sample++)
						if(ps->mask & (1 << sample))
							pxf_add_alpha_fac(rlpp[sample]->rectf + od*4, col, har->add);
				}
				else {
					fac= ((float)amountm)/(float)osa;
					accol[0]+= fac*col[0];
					accol[1]+= fac*col[1];
					accol[2]+= fac*col[2];
					accol[3]+= fac*col[3];
				}
			}
		}
		
		mask |= ps->mask;
		ps= ps->next;
	}

	/* now do the sky sub-pixels */
	amount= osa-amount;
	if(amount) {
		if(shadeHaloFloat(re, har, col, 0x7FFFFF, dist, xn, yn, flarec)) {
			if(!fullsample) {
				fac= ((float)amount)/(float)osa;
				accol[0]+= fac*col[0];
				accol[1]+= fac*col[1];
				accol[2]+= fac*col[2];
				accol[3]+= fac*col[3];
			}
		}
	}

	if(fullsample) {
		for(sample=0; sample<totsample; sample++)
			if(!(mask & (1 << sample)))
				pxf_add_alpha_fac(rlpp[sample]->rectf + od*4, col, har->add);
	}
	else {
		col[0]= accol[0];
		col[1]= accol[1];
		col[2]= accol[2];
		col[3]= accol[3];
		
		for(sample=0; sample<totsample; sample++)
			pxf_add_alpha_fac(rlpp[sample]->rectf + od*4, col, har->add);
	}
}

static void halo_tile(Render *re, RenderPart *pa, RenderLayer *rl)
{
	RenderLayer *rlpp[RE_MAX_OSA];
	HaloRen *har;
	rcti disprect= pa->disprect, testrect= pa->disprect;
	PixStr **rd= NULL;
	float dist, xsq, ysq, xn, yn;
	float col[4];
	int a, *rz, zz, y, sample, totsample, od;
	short minx, maxx, miny, maxy, x;
	unsigned int lay= rl->lay;

	/* we don't render halos in the cropped area, gives errors in flare counter */
	if(pa->crop) {
		testrect.xmin+= pa->crop;
		testrect.xmax-= pa->crop;
		testrect.ymin+= pa->crop;
		testrect.ymax-= pa->crop;
	}
	
	totsample= get_sample_layers(re, pa, rl, rlpp);

	for(a=0; a<re->db.tothalo; a++) {
		har= re->db.sortedhalos[a];

		/* layer test, clip halo with y */
		if((har->lay & lay)==0);
		else if(testrect.ymin > har->maxy);
		else if(testrect.ymax < har->miny);
		else {
			
			minx= floor(har->xs-har->rad);
			maxx= ceil(har->xs+har->rad);
			
			if(testrect.xmin > maxx);
			else if(testrect.xmax < minx);
			else {
				
				minx= MAX2(minx, testrect.xmin);
				maxx= MIN2(maxx, testrect.xmax);
			
				miny= MAX2(har->miny, testrect.ymin);
				maxy= MIN2(har->maxy, testrect.ymax);
			
				for(y=miny; y<maxy; y++) {
					int rectofs= (y-disprect.ymin)*pa->rectx + (minx - disprect.xmin);
					rz= pa->rectz + rectofs;
					od= rectofs;
					
					if(pa->rectdaps)
						rd= pa->rectdaps + rectofs;
					
					yn= (y-har->ys)*re->cam.ycor;
					ysq= yn*yn;
					
					for(x=minx; x<maxx; x++, rz++, od++) {
						xn= x- har->xs;
						xsq= xn*xn;
						dist= xsq+ysq;
						if(dist<har->radsq) {
							if(rd && *rd) {
								halo_pixelstruct(re, har, rlpp, totsample, od, dist, xn, yn, (PixStr *)*rd);
							}
							else {
								zz= calchalo_z(har, *rz);
								if((zz> har->zs) || (har->mat && (har->mat->mode & MA_HALO_SOFT))) {
									if(shadeHaloFloat(re, har, col, zz, dist, xn, yn, har->flarec)) {
										for(sample=0; sample<totsample; sample++)
											pxf_add_alpha_fac(rlpp[sample]->rectf + od*4, col, har->add);
									}
								}
							}
						}
						if(rd) rd++;
					}
				}
			}
		}
		if(re->cb.test_break(re->cb.tbh) ) break; 
	}
}

/******************************* Sky *************************************/

/* only do sky, is default in the solid layer (shade_tile) btw */
static void sky_tile(Render *re, RenderPart *pa, RenderLayer *rl)
{
	RenderLayer *rlpp[RE_MAX_OSA];
	int x, y, od=0, totsample;
	
	if(re->params.r.alphamode!=R_ADDSKY)
		return;
	
	totsample= get_sample_layers(re, pa, rl, rlpp);
	
	for(y=pa->disprect.ymin; y<pa->disprect.ymax; y++) {
		for(x=pa->disprect.xmin; x<pa->disprect.xmax; x++, od+=4) {
			float col[4];
			int sample, done= 0;
			
			for(sample= 0; sample<totsample; sample++) {
				float *pass= rlpp[sample]->rectf + od;
				
				if(pass[3]<1.0f) {
					
					if(done==0) {
						environment_shade_pixel(re, col, x, y, pa->thread);
						done= 1;
					}
					
					if(pass[3]==0.0f) {
						copy_v4_v4(pass, col);
					}
					else {
						pxf_add_alpha_under(pass, col);
					}
				}
			}			
		}
		
		if(y&1)
			if(re->cb.test_break(re->cb.tbh)) break; 
	}
}

/******************************* Atmosphere *************************************/

static void atm_tile(Render *re, RenderPart *pa, RenderLayer *rl)
{
	RenderPass *zpass;
	GroupObject *go;
	LampRen *lar;
	RenderLayer *rlpp[RE_MAX_OSA];
	int totsample;
	int x, y, od= 0;
	
	totsample= get_sample_layers(re, pa, rl, rlpp);

	/* check that z pass is enabled */
	if(pa->rectz==NULL) return;
	for(zpass= rl->passes.first; zpass; zpass= zpass->next)
		if(zpass->passtype==SCE_PASS_Z)
			break;
	
	if(zpass==NULL) return;

	/* check for at least one sun lamp that its atmosphere flag is is enabled */
	for(go=re->db.lights.first; go; go= go->next) {
		lar= go->lampren;
		if(lar->type==LA_SUN && lar->sunsky && (lar->sunsky->effect_type & LA_SUN_EFFECT_AP))
			break;
	}
	/* do nothign and return if there is no sun lamp */
	if(go==NULL)
		return;
	
	/* for each x,y and each sample, and each sun lamp*/
	for(y=pa->disprect.ymin; y<pa->disprect.ymax; y++) {
		for(x=pa->disprect.xmin; x<pa->disprect.xmax; x++, od++) {
			int sample;
			
			for(sample=0; sample<totsample; sample++) {
				float *zrect= RE_RenderLayerGetPass(rlpp[sample], SCE_PASS_Z) + od;
				float *rgbrect = rlpp[sample]->rectf + 4*od;
				float rgb[3] = {0};
				int done= 0;
				
				for(go=re->db.lights.first; go; go= go->next) {
				
					
					lar= go->lampren;
					if(lar->type==LA_SUN &&	lar->sunsky) {
						
						/* if it's sky continue and don't apply atmosphere effect on it */
						if(*zrect >= 9.9e10 || rgbrect[3]==0.0f) {
							continue;
						}
						
						if((lar->sunsky->effect_type & LA_SUN_EFFECT_AP)) {	
							float tmp_rgb[3];
							
							copy_v3_v3(tmp_rgb, rgbrect);
							if(rgbrect[3]!=1.0f) {	/* de-premul */
								float div= 1.0f/rgbrect[3];
								mul_v3_fl(tmp_rgb, div);
							}
							atmosphere_shade_pixel(re, lar->sunsky, tmp_rgb, x, y, *zrect);
							if(rgbrect[3]!=1.0f) {	/* premul */
								mul_v3_fl(tmp_rgb, rgbrect[3]);
							}
							
							if(done==0) {
								copy_v3_v3(rgb, tmp_rgb);
								done = 1;						
							}
							else{
								rgb[0] = 0.5f*rgb[0] + 0.5f*tmp_rgb[0];
								rgb[1] = 0.5f*rgb[1] + 0.5f*tmp_rgb[1];
								rgb[2] = 0.5f*rgb[2] + 0.5f*tmp_rgb[2];
							}
						}
					}
				}

				/* if at least for one sun lamp aerial perspective was applied*/
				if(done) {
					copy_v3_v3(rgbrect, rgb);
				}
			}
		}
	}
}

/******************************* Key Alpha *************************************/

static void convert_to_key_alpha(Render *re, RenderPart *pa, RenderLayer *rl)
{
	RenderLayer *rlpp[RE_MAX_OSA];
	int y, sample, totsample;
	
	totsample= get_sample_layers(re, pa, rl, rlpp);
	
	for(sample= 0; sample<totsample; sample++) {
		float *rectf= rlpp[sample]->rectf;
		
		for(y= pa->rectx*pa->recty; y>0; y--, rectf+=4) {
			if(rectf[3] >= 1.0f);
			else if(rectf[3] > 0.0f) {
				rectf[0] /= rectf[3];
				rectf[1] /= rectf[3];
				rectf[2] /= rectf[3];
			}
		}
	}
}

/******************************* Edge *************************************/

static void edge_enhance_add(Render *re, RenderPart *pa, float *rectf, float *arect)
{
	float addcol[4];
	int pix;
	
	if(arect==NULL)
		return;
	
	for(pix= pa->rectx*pa->recty; pix>0; pix--, arect++, rectf+=4) {
		if(*arect != 0.0f) {
			addcol[0]= *arect * re->params.r.edgeR;
			addcol[1]= *arect * re->params.r.edgeG;
			addcol[2]= *arect * re->params.r.edgeB;
			addcol[3]= *arect;
			pxf_add_alpha_over(rectf, addcol);
		}
	}
}

/* adds only alpha values */
static void edge_enhance_tile(Render *re, RenderPart *pa, float *rectf, int *rectz)
{
	/* use zbuffer to define edges, add it to the image */
	int y, x, col, *rz, *rz1, *rz2, *rz3;
	int zval1, zval2, zval3;
	float *rf;
	
	/* shift values in zbuffer 4 to the right (anti overflows), for filter we need multiplying with 12 max */
	rz= rectz;
	if(rz==NULL) return;
	
	for(y=0; y<pa->recty; y++)
		for(x=0; x<pa->rectx; x++, rz++) (*rz)>>= 4;
	
	rz1= rectz;
	rz2= rz1+pa->rectx;
	rz3= rz2+pa->rectx;
	
	rf= rectf+pa->rectx+1;
	
	for(y=0; y<pa->recty-2; y++) {
		for(x=0; x<pa->rectx-2; x++, rz1++, rz2++, rz3++, rf++) {
			
			/* prevent overflow with sky z values */
			zval1=   rz1[0] + 2*rz1[1] +   rz1[2];
			zval2=  2*rz2[0]           + 2*rz2[2];
			zval3=   rz3[0] + 2*rz3[1] +   rz3[2];
			
			col= ( 4*rz2[1] - (zval1 + zval2 + zval3)/3 );
			if(col<0) col= -col;
			
			col >>= 5;
			if(col > (1<<16)) col= (1<<16);
			else col= (re->params.r.edgeint*col)>>8;
			
			if(col>0) {
				float fcol;
				
				if(col>255) fcol= 1.0f;
				else fcol= (float)col/255.0f;
				
				if(re->params.osa)
					*rf+= fcol/(float)re->params.osa;
				else
					*rf= fcol;
			}
		}
		rz1+= 2;
		rz2+= 2;
		rz3+= 2;
		rf+= 2;
	}
	
	/* shift back zbuf values, we might need it still */
	rz= rectz;
	for(y=0; y<pa->recty; y++)
		for(x=0; x<pa->rectx; x++, rz++) (*rz)<<= 4;
	
}

/************************* Solid and ZTransp Shade ****************************/

static int pixel_row_compare(const void *a1, const void *a2)
{
	const PixelRow *r1 = a1, *r2 = a2;

	if(r1->z > r2->z) return 1;
	else if(r1->z < r2->z) return -1;
	return 0;
}

static void shade_strand_samples(Render *re, StrandShadeCache *cache, ShadeSample *ssamp, int x, int y, PixelRow *row, int addpassflag)
{
	StrandSegment sseg;
	StrandVert *svert;
	ObjectInstanceRen *obi;
	ObjectRen *obr;

	obi= re->db.objectinstance + row->obi;
	obr= obi->obr;

	sseg.obi= obi;
	sseg.strand= render_object_strand_get(obr, row->p-1);
	sseg.buffer= sseg.strand->buffer;

	svert= sseg.strand->vert + row->segment;
	sseg.v[0]= (row->segment > 0)? (svert-1): svert;
	sseg.v[1]= svert;
	sseg.v[2]= svert+1;
	sseg.v[3]= (row->segment < sseg.strand->totvert-2)? svert+2: svert+1;

	ssamp->tot= 1;
	strand_shade_segment(re, cache, &sseg, ssamp, row->v, row->u, addpassflag);
	ssamp->shi[0].shading.mask= row->mask;
}

static void unref_strand_samples(Render *re, StrandShadeCache *cache, PixelRow *row, int a, int tot)
{
	StrandVert *svert;
	ObjectInstanceRen *obi;
	ObjectRen *obr;
	StrandRen *strand;

	/* remove references to samples that are not being rendered, but we still
	 * need to remove them so that the reference count of strand vertex shade
	 * samples correctly drops to zero */
	for(; a<tot; a++) {
		if(row[a].segment != -1) {
			obi= re->db.objectinstance + row[a].obi;
			obr= obi->obr;
			strand= render_object_strand_get(obr, row[a].p-1);
			svert= strand->vert + row[a].segment;

			strand_shade_unref(cache, svert);
			strand_shade_unref(cache, svert+1);
		}
	}
}

static int pixel_row_shade_samples(Render *re, ShadeSample *ssamp, StrandShadeCache *cache, int x, int y, PixelRow *row, int addpassflag)
{
	int samp;

	if(row->segment != -1) {
		/* strand shading */
		shade_strand_samples(re, cache, ssamp, x, y, row, addpassflag);
		return 1;
	}
	else {
		/* vlak shading */
		shade_samples_from_pixel(re, ssamp, row, x, y);

		if(ssamp->tot) {
			shade_samples(re, ssamp);

			/* include lamphalos for ztra, since halo layer was added already */
			if((re->params.flag & R_LAMPHALO) && (ssamp->shi->shading.layflag & SCE_LAY_HALO))
				for(samp=0; samp<ssamp->tot; samp++)
					lamp_spothalo_render(re, &ssamp->shi[samp],
						ssamp->shr[samp].combined, ssamp->shr[samp].combined[3]);
			
			return 1;
		}

		return 0;
	}
}

/* merge samples from solid, ztransp and strands into a one PixelRow */
int pixel_row_fill(PixelRow *row, Render *re, RenderPart *pa, int offs)
{
	PixStr *ps= (pa->rectdaps)? *(pa->rectdaps + offs): NULL;
	APixstr *ap = (pa->apixbuf)? pa->apixbuf + offs: NULL;
	APixstrand *apstrand = (pa->apixbufstrand)? pa->apixbufstrand + offs: NULL;
	APixstr *apn;
	APixstrand *apnstrand;
	int a, b, totsample, tot= 0;
	int osa = (re->params.osa? re->params.osa: 1);

	tot= 0;

	/* add solid samples */
	for(; ps; ps=ps->next) {
		row[tot].z= ps->z;
		row[tot].mask=ps->mask;
		row[tot].obi= ps->obi;
		row[tot].p= ps->facenr;
		row[tot].segment= -1;
		tot++;
	}

	/* add ztransp samples */
	for(apn= ap; apn; apn=apn->next) {
		for(a=0; a<4; a++) {
			if(apn->p[a]) {
				row[tot].obi= apn->obi[a];
				row[tot].z= apn->z[a];
				row[tot].p= apn->p[a];
				row[tot].mask= apn->mask[a];
				row[tot].segment= -1;
				tot++;
				if(tot>=MAX_PIXEL_ROW) tot= MAX_PIXEL_ROW-1;
			}
			else break;
		}
	}

	/* add strand samples */
	for(apnstrand= apstrand; apnstrand; apnstrand=apnstrand->next) {
		for(a=0; a<4; a++) {
			if(apnstrand->p[a]) {
				row[tot].obi= apnstrand->obi[a];
				row[tot].z= apnstrand->z[a];
				row[tot].p= apnstrand->p[a];
				row[tot].mask= apnstrand->mask[a];
				row[tot].segment= apnstrand->seg[a];

				if(osa > 1) {
					totsample= 0;
					for(b=0; b<osa; b++)
						if(row[tot].mask & (1<<b))
							totsample++;
				}
				else
					totsample= 1;

				row[tot].u= apnstrand->u[a]/totsample;
				row[tot].v= apnstrand->v[a]/totsample;
				tot++;
				if(tot>=MAX_PIXEL_ROW) tot= MAX_PIXEL_ROW-1;
			}
		}
	}

	/* sort front to back */
	if(tot==2) {
		if(row[0].z > row[1].z)
			SWAP(PixelRow, row[0], row[1]);
	}
	else if(tot>2)
		qsort(row, tot, sizeof(PixelRow), pixel_row_compare);

	return tot;
}

static void pixel_row_shade_lamphalo(Render *re, ShadeResult *samp_shr, ShadeSample *ssamp, int tot, int x, int y)
{
	int a, mask= 0;

	for(a=0; a<tot; a++)
		if(samp_shr[a].z == PASS_Z_MAX)
			mask |= (1<<a);
	
	/* do lamp halo for pixel samples that were not filled in yet, others were
	   done as part of shading calculations. eventually we should do proper
	   interleaved surface/lamphalo shading... now it is not correct for ztransp */
	if(mask) {
		ShadeInput *shi= ssamp->shi;
		float col[4];

		/* weak shadeinput setup */
		memset(&shi->primitive, 0, sizeof(shi->primitive));
		camera_raster_to_view(&re->cam, shi->geometry.view, x, y);
		camera_raster_to_co(&re->cam, shi->geometry.co, x, y, 0x7FFFFFFF);
		
		zero_v4(col);
		lamp_spothalo_render(re, shi, col, 1.0f);

		for(a=0; a<tot; a++)
			if(samp_shr[a].z == PASS_Z_MAX)
				copy_v4_v4(samp_shr[a].combined, col);
	}
}

static void pixel_row_shade(Render *re, ShadeResult *samp_shr, ShadeSample *ssamp, PixelRow *row, int tot, int x, int y, StrandShadeCache *sscache, int passflag, int layflag)
{
	int a, osa = (re->params.osa? re->params.osa: 1);

	/* initialize samp_shr */
	shade_result_init(samp_shr, osa);

	/* shade entries in row front to back, and accumulate in samp_shr */
	for(a=0; a<tot; a++) {
		if(pixel_row_shade_samples(re, ssamp, sscache, x, y, &row[a], passflag)) {
			/* for each mask-sample we alpha-under colors */
			if(!shade_result_accumulate(samp_shr, ssamp, osa, passflag)) {
				/* stop early, sufficiently opaque */

				/* still need to remove cache references for strands */
				if(sscache)
					unref_strand_samples(re, sscache, row, a+1, tot);

				break;
			}
		}
	}

	if((re->params.flag & R_LAMPHALO) && (ssamp->shi->shading.layflag & SCE_LAY_HALO))
		pixel_row_shade_lamphalo(re, samp_shr, ssamp, osa, x, y);
}

static void zbuf_shade_all(Render *re, RenderPart *pa, RenderLayer *rl)
{
	RenderResult *rr= pa->result;
	StrandShadeCache *sscache= pa->sscache;
	ShadeSample ssamp;
	int seed, lamphalo;
	int x, y, crop, offs;
	int passflag, layflag;

	if(re->cb.test_break(re->cb.tbh))
		return;

	/* lamp halo? */
	lamphalo= (re->params.flag & R_LAMPHALO) && (rl->layflag & SCE_LAY_HALO);
	
	/* we set per pixel a fixed seed, for random AO and shadow samples */
	seed= pa->rectx*pa->disprect.ymin;
	
	/* general shader info, passes */
	shade_sample_initialize(re, &ssamp, pa, rl);
	passflag= rl->passflag & ~(SCE_PASS_COMBINED);
	layflag= rl->layflag;

	/* precompute shading data for this tile */
	if(re->params.r.mode & R_SHADOW)
		irregular_shadowbuf_create(re, pa, pa->apixbuf);

	if(re->db.occlusiontree)
		disk_occlusion_cache_create(re, pa, &ssamp);
	else
		irr_cache_create(re, pa, rl, &ssamp);

	/* filtered render, for now we assume only 1 filter size */
	offs= 0;
	crop= 0;
	seed= pa->rectx*pa->disprect.ymin;
	if(pa->crop) {
		crop= 1;
		offs= pa->rectx + 1;
	}

	/* initialize scanline updates for main thread,
	   scanline updates have to be 2 lines behind for crop */
	rr->renrect.ymin= 0;
	rr->renrect.ymax= -2*crop;
	rr->renlay= rl;

	/* render the tile */
	for(y=pa->disprect.ymin+crop; y<pa->disprect.ymax-crop; y++, rr->renrect.ymax++) {
		for(x=pa->disprect.xmin+crop; x<pa->disprect.xmax-crop; x++) {
			PixelRow row[MAX_PIXEL_ROW];
			int totrow;

			/* create shade pixel row, sorted front to back */
			totrow= pixel_row_fill(row, re, pa, offs);

			if(totrow || lamphalo) {
				ShadeResult samp_shr[RE_MAX_OSA];

				/* per pixel fixed seed */
				BLI_thread_srandom(pa->thread, seed++);

				/* shade and accumulate into samp_shr */
				pixel_row_shade(re, samp_shr, &ssamp, row, totrow, x, y, sscache, passflag, layflag);

				/* write samp_shr into part render layer */
				shade_result_to_part(re, pa, rl, offs, samp_shr);
			}

			offs++;
		}

		offs+= 2*crop;

		if(re->cb.test_break(re->cb.tbh)) break;
	}

	/* disable scanline updating */
	rr->renlay= NULL;

	/* free tile precomputed data */
	if(re->db.occlusiontree)
		disk_occlusion_cache_free(re, pa);
	else
		irr_cache_free(re, pa);

	if(re->params.r.mode & R_SHADOW)
		irregular_shadowbuf_free(re, pa);
}

static void zbuf_rasterize(Render *re, RenderPart *pa, RenderLayer *rl, ListBase *psmlist, float *edgerect)
{
	/* rasterization */
	if((rl->layflag & SCE_LAY_ZMASK) && (rl->layflag & SCE_LAY_NEG_ZMASK))
		pa->rectmask= MEM_mallocN(sizeof(int)*pa->rectx*pa->recty, "rectmask");

	if(rl->layflag & SCE_LAY_SOLID)
		zbuffer_solid(re, pa, rl, psmlist, edge_enhance_tile, edgerect);

	if(re->params.flag & R_ZTRA || re->db.totstrand)
		if(rl->layflag & (SCE_LAY_ZTRA|SCE_LAY_STRAND))
			zbuffer_alpha(re, pa, rl);

	if(pa->rectmask) {
		MEM_freeN(pa->rectmask);
		pa->rectmask= NULL;
	}

	/* shading */
	zbuf_shade_all(re, pa, rl);

	/* free */
	if(pa->apixbuf)
		MEM_freeN(pa->apixbuf);
	if(pa->apixbufstrand)
		MEM_freeN(pa->apixbufstrand);
	if(pa->sscache)
		strand_shade_cache_free(pa->sscache);
	free_alpha_pixel_structs(&pa->apsmbase);	

	pa->apixbuf= NULL;
	pa->apixbufstrand= NULL;
	pa->sscache= NULL;
	pa->apsmbase.first= pa->apsmbase.last= NULL;
}

/***************************** Main Rasterization Call ********************************/

/* main call for rasterization, now combined OSA and non-OSA */
/* supposed to be fully threadable! */
void render_rasterize_part(Render *re, RenderPart *pa)
{
	RenderResult *rr= pa->result;
	RenderLayer *rl;
	ListBase psmlist= {NULL, NULL};
	float *edgerect= NULL;
	
	/* allocate the necessary buffers, zbuffer code clears/inits rects */
	pa->recto= MEM_mallocN(sizeof(int)*pa->rectx*pa->recty, "recto");
	pa->rectp= MEM_mallocN(sizeof(int)*pa->rectx*pa->recty, "rectp");
	pa->rectz= MEM_mallocN(sizeof(int)*pa->rectx*pa->recty, "rectz");

	for(rl= rr->layers.first; rl; rl= rl->next) {
		pa->totsample= get_sample_layers(re, pa, rl, pa->rlpp);

		if(rl->layflag & SCE_LAY_EDGE) 
			if(re->params.r.mode & R_EDGE) 
				edgerect= MEM_callocN(sizeof(float)*pa->rectx*pa->recty, "rectedge");
	
		if(rl->layflag & (SCE_LAY_SOLID|SCE_LAY_ZTRA|SCE_LAY_STRAND))
			zbuf_rasterize(re, pa, rl, &psmlist, edgerect);

		/* halo is now after solid and ztra */
		if(re->params.flag & R_HALO)
			if(rl->layflag & SCE_LAY_HALO)
				halo_tile(re, pa, rl);

		/* free stuff within loop! */
		if(pa->rectdaps) {
			MEM_freeN(pa->rectdaps);
			pa->rectdaps= NULL;
		}
		free_pixel_structs(&psmlist);

		/* sun/sky */
		if(rl->layflag & SCE_LAY_SKY)
			atm_tile(re, pa, rl);
		
		/* sky before edge */
		if(rl->layflag & SCE_LAY_SKY)
			sky_tile(re, pa, rl);

		/* extra layers */
		if(rl->layflag & SCE_LAY_EDGE) 
			if(re->params.r.mode & R_EDGE)
				edge_enhance_add(re, pa, rl->rectf, edgerect);
		
		if(edgerect) {
			MEM_freeN(edgerect);
			edgerect= NULL;
		}

		/* de-premul alpha */
		if(re->params.r.alphamode & R_ALPHAKEY)
			convert_to_key_alpha(re, pa, rl);
	}
	
	/* free all */
	MEM_freeN(pa->recto); pa->recto= NULL;
	MEM_freeN(pa->rectp); pa->rectp= NULL;
	MEM_freeN(pa->rectz); pa->rectz= NULL;
	
	/* display active layer */
	rr->renrect.ymin=rr->renrect.ymax= 0;
	rr->renlay= render_get_active_layer(re, rr);
}

/******************************* SSS *************************************/

/* SSS preprocess tile render, fully threadable */
typedef struct ZBufSSSHandle {
	RenderPart *pa;
	ListBase psmlist;
	int totps;
} ZBufSSSHandle;

static void addps_sss(void *cb_handle, int obi, int facenr, int x, int y, int z)
{
	ZBufSSSHandle *handle = cb_handle;
	RenderPart *pa= handle->pa;

	/* extra border for filter gives double samples on part edges,
	   don't use those */
	if(x<pa->crop || x>=pa->rectx-pa->crop)
		return;
	if(y<pa->crop || y>=pa->recty-pa->crop)
		return;
	
#if 0
	if(pa->rectall) {
		void **rs= pa->rectall + pa->rectx*y + x;

		addps(&handle->psmlist, rs, obi, facenr, z, 0, 0);
		handle->totps++;
	}
#endif
	if(pa->rectz) {
		int *rz= pa->rectz + pa->rectx*y + x;
		int *rp= pa->rectp + pa->rectx*y + x;
		int *ro= pa->recto + pa->rectx*y + x;

		if(z < *rz) {
			if(*rp == 0)
				handle->totps++;
			*rz= z;
			*rp= facenr;
			*ro= obi;
		}
	}
	if(pa->rectbackz) {
		int *rz= pa->rectbackz + pa->rectx*y + x;
		int *rp= pa->rectbackp + pa->rectx*y + x;
		int *ro= pa->rectbacko + pa->rectx*y + x;

		if(z >= *rz) {
			if(*rp == 0)
				handle->totps++;
			*rz= z;
			*rp= facenr;
			*ro= obi;
		}
	}
}

static void shade_sample_sss(Render *re, ShadeSample *ssamp, Material *mat, ObjectInstanceRen *obi, VlakRen *vlr, int quad, float x, float y, float z, float *co, float *color, float *area)
{
	ShadeInput *shi= ssamp->shi;
	ShadeResult shr;
	float orthoarea, nor[3], alpha, sx, sy;

	/* cache for shadow */
	shi->shading.samplenr= re->sample.shadowsamplenr[shi->shading.thread]++;
	
	if(quad) 
		shade_input_set_triangle_i(re, shi, obi, vlr, 0, 2, 3);
	else
		shade_input_set_triangle_i(re, shi, obi, vlr, 0, 1, 2);

	/* center pixel */
	sx = x + 0.5f;
	sy = y + 0.5f;

	/* we estimate the area here using shi->geometry.dxco and shi->geometry.dyco. we need to
	   enabled shi->geometry.osatex these are filled. we compute two areas, one with
	   the normal pointed at the camera and one with the original normal, and
	   then clamp to avoid a too large contribution from a single pixel */
	shi->geometry.osatex= 1;
	
	render_vlak_get_normal(obi, vlr, shi->geometry.facenor);
	copy_v3_v3(nor, shi->geometry.facenor);
	camera_raster_to_view(&re->cam, shi->geometry.facenor, sx, sy);
	normalize_v3(shi->geometry.facenor);
	shade_input_set_viewco(re, shi, x, y, sx, sy, z);
	orthoarea= len_v3(shi->geometry.dxco)*len_v3(shi->geometry.dyco);

	copy_v3_v3(shi->geometry.facenor, nor);
	shade_input_set_viewco(re, shi, x, y, sx, sy, z);
	*area= len_v3(shi->geometry.dxco)*len_v3(shi->geometry.dyco);
	*area= MIN2(*area, 2.0f*orthoarea);

	shade_input_set_uv(shi);
	shade_input_set_normals(shi);

	/* we don't want flipped normals, they screw up back scattering */
	if(shi->geometry.flippednor)
		shade_input_flip_normals(shi);

	/* not a pretty solution, but fixes common cases */
	if(shi->primitive.obr->ob && shi->primitive.obr->ob->transflag & OB_NEG_SCALE) {
		negate_v3(shi->geometry.vn);
		negate_v3(shi->geometry.vno);
	}

	/* if nodetree, use the material that we are currently preprocessing
	   instead of the node material */
	if(shi->material.mat->nodetree && shi->material.mat->use_nodes)
		shi->material.mat= mat;

	/* init material vars */
	shade_input_init_material(re, shi);
	
	/* render */
	shade_input_set_shade_texco(re, shi);
	
	shade_samples_do_AO(re, ssamp);
	shade_material_loop(re, shi, &shr);
	
	copy_v3_v3(co, shi->geometry.co);
	copy_v3_v3(color, shr.sss);

	alpha= shr.sss[3]; // TODO NSHAD solve SSS + alpha
	*area *= alpha;
}

static void zbufshade_sss_free(RenderPart *pa)
{
#if 0
	MEM_freeN(pa->rectall); pa->rectall= NULL;
	free_pixel_structs(&handle.psmlist);
#else
	MEM_freeN(pa->rectz); pa->rectz= NULL;
	MEM_freeN(pa->rectp); pa->rectp= NULL;
	MEM_freeN(pa->recto); pa->recto= NULL;
	MEM_freeN(pa->rectbackz); pa->rectbackz= NULL;
	MEM_freeN(pa->rectbackp); pa->rectbackp= NULL;
	MEM_freeN(pa->rectbacko); pa->rectbacko= NULL;
#endif
}

void render_sss_bake_part(Render *re, RenderPart *pa)
{
	ShadeSample ssamp;
	ZBufSSSHandle handle;
	RenderResult *rr= pa->result;
	RenderLayer *rl;
	VlakRen *vlr;
	Material *mat= re->db.sss_mat;
	float (*co)[3], (*color)[3], *area, *fcol;
	int x, y, seed, quad, totpoint, display = !(re->params.r.scemode & R_PREVIEWBUTS);
	int *ro, *rz, *rp, *rbo, *rbz, *rbp, lay;
#if 0
	PixStr *ps;
	void **rs;
	int z;
#endif

	/* setup pixelstr list and buffer for zbuffering */
	handle.pa= pa;
	handle.totps= 0;

#if 0
	handle.psmlist.first= handle.psmlist.last= NULL;
	addpsmain(&handle.psmlist);

	pa->rectall= MEM_callocN(sizeof(void*)*pa->rectx*pa->recty+4, "rectall");
#else
	pa->recto= MEM_mallocN(sizeof(int)*pa->rectx*pa->recty, "recto");
	pa->rectp= MEM_mallocN(sizeof(int)*pa->rectx*pa->recty, "rectp");
	pa->rectz= MEM_mallocN(sizeof(int)*pa->rectx*pa->recty, "rectz");
	pa->rectbacko= MEM_mallocN(sizeof(int)*pa->rectx*pa->recty, "rectbacko");
	pa->rectbackp= MEM_mallocN(sizeof(int)*pa->rectx*pa->recty, "rectbackp");
	pa->rectbackz= MEM_mallocN(sizeof(int)*pa->rectx*pa->recty, "rectbackz");
#endif

	/* setup shade sample with correct passes */
	memset(&ssamp, 0, sizeof(ssamp));
	shade_sample_initialize(re, &ssamp, pa, rr->layers.first);
	ssamp.tot= 1;
	
	for(rl=rr->layers.first; rl; rl=rl->next) {
		ssamp.shi[0].shading.lay |= rl->lay;
		ssamp.shi[0].shading.layflag |= rl->layflag;
		ssamp.shi[0].shading.passflag |= rl->passflag;
		ssamp.shi[0].shading.combinedflag |= ~rl->pass_xor;
	}

	rl= rr->layers.first;
	ssamp.shi[0].shading.passflag |= SCE_PASS_RGBA|SCE_PASS_COMBINED;
	ssamp.shi[0].shading.combinedflag &= ~(SCE_PASS_SPEC);
	ssamp.shi[0].material.mat_override= NULL;
	ssamp.shi[0].material.light_override= NULL;
	lay= ssamp.shi[0].shading.lay;

	/* create the pixelstrs to be used later */
	zbuffer_sss(re, pa, lay, &handle, addps_sss);

	if(handle.totps==0) {
		zbufshade_sss_free(pa);
		return;
	}
	
	fcol= rl->rectf;

	co= MEM_mallocN(sizeof(float)*3*handle.totps, "SSSCo");
	color= MEM_mallocN(sizeof(float)*3*handle.totps, "SSSColor");
	area= MEM_mallocN(sizeof(float)*handle.totps, "SSSArea");

#if 0
	/* create ISB (does not work currently!) */
	if(re->params.r.mode & R_SHADOW)
		irregular_shadowbuf_create(re, pa, NULL);
#endif

	if(display) {
		/* initialize scanline updates for main thread */
		rr->renrect.ymin= 0;
		rr->renlay= rl;
	}
	
	seed= pa->rectx*pa->disprect.ymin;
#if 0
	rs= pa->rectall;
#else
	rz= pa->rectz;
	rp= pa->rectp;
	ro= pa->recto;
	rbz= pa->rectbackz;
	rbp= pa->rectbackp;
	rbo= pa->rectbacko;
#endif
	totpoint= 0;

	for(y=pa->disprect.ymin; y<pa->disprect.ymax; y++, rr->renrect.ymax++) {
		for(x=pa->disprect.xmin; x<pa->disprect.xmax; x++, fcol+=4) {
			/* per pixel fixed seed */
			BLI_thread_srandom(pa->thread, seed++);
			
#if 0
			if(rs) {
				/* for each sample in this pixel, shade it */
				for(ps=(PixStr*)*rs; ps; ps=ps->next) {
					ObjectInstanceRen *obi= &re->db.objectinstance[ps->obi];
					ObjectRen *obr= obi->obr;
					vlr= render_object_vlak_get(obr, (ps->facenr-1) & RE_QUAD_MASK);
					quad= (ps->facenr & RE_QUAD_OFFS);
					z= ps->z;

					shade_sample_sss(re, &ssamp, mat, obi, vlr, quad, x, y, z,
						co[totpoint], color[totpoint], &area[totpoint]);

					totpoint++;

					add_v3_v3v3(fcol, fcol, color);
					fcol[3]= 1.0f;
				}

				rs++;
			}
#else
			if(rp) {
				if(*rp != 0) {
					ObjectInstanceRen *obi= &re->db.objectinstance[*ro];
					ObjectRen *obr= obi->obr;

					/* shade front */
					vlr= render_object_vlak_get(obr, (*rp-1) & RE_QUAD_MASK);
					quad= ((*rp) & RE_QUAD_OFFS);

					shade_sample_sss(re, &ssamp, mat, obi, vlr, quad, x, y, *rz,
						co[totpoint], color[totpoint], &area[totpoint]);
					
					add_v3_v3v3(fcol, fcol, color[totpoint]);
					fcol[3]= 1.0f;
					totpoint++;
				}

				rp++; rz++; ro++;
			}

			if(rbp) {
				if(*rbp != 0 && !(*rbp == *(rp-1) && *rbo == *(ro-1))) {
					ObjectInstanceRen *obi= &re->db.objectinstance[*rbo];
					ObjectRen *obr= obi->obr;

					/* shade back */
					vlr= render_object_vlak_get(obr, (*rbp-1) & RE_QUAD_MASK);
					quad= ((*rbp) & RE_QUAD_OFFS);

					shade_sample_sss(re, &ssamp, mat, obi, vlr, quad, x, y, *rbz,
						co[totpoint], color[totpoint], &area[totpoint]);
					
					/* to indicate this is a back sample */
					area[totpoint]= -area[totpoint];

					add_v3_v3v3(fcol, fcol, color[totpoint]);
					fcol[3]= 1.0f;
					totpoint++;
				}

				rbz++; rbp++; rbo++;
			}
#endif
		}

		if(y&1)
			if(re->cb.test_break(re->cb.tbh)) break; 
	}

	/* note: after adding we do not free these arrays, sss keeps them */
	if(totpoint > 0) {
		sss_add_points(re, co, color, area, totpoint);
	}
	else {
		MEM_freeN(co);
		MEM_freeN(color);
		MEM_freeN(area);
	}
	
#if 0
	if(re->params.r.mode & R_SHADOW)
		irregular_shadowbuf_free(re, pa);
#endif
		
	if(display) {
		/* display active layer */
		rr->renrect.ymin=rr->renrect.ymax= 0;
		rr->renlay= render_get_active_layer(re, rr);
	}
	
	zbufshade_sss_free(pa);
}


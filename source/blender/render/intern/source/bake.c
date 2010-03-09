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

#include <assert.h>
#include <float.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_image_types.h"
#include "DNA_material_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_math.h"

#include "PIL_time.h"

#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_utildefines.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "RE_raytrace.h"
#include "RE_shader_ext.h"

#include "database.h"
#include "object_mesh.h"
#include "render_types.h"
#include "rendercore.h"
#include "shading.h"
#include "zbuf.h"

/* ************************* bake ************************ */

typedef struct BakeShade {
	Render *re;
	ShadeSample ssamp;
	ObjectInstanceRen *obi;
	VlakRen *vlr;
	
	ZSpan *zspan;
	Image *ima;
	ImBuf *ibuf;
	
	int rectx, recty, quad, type, vdone, ready;

	float dir[3];
	Object *actob;
	
	unsigned int *rect;
	float *rect_float;
	
	int usemask;
	char *rect_mask; /* bake pixel mask */

	float dxco[3], dyco[3];

	short *do_update;
} BakeShade;

/* bake uses a char mask to know what has been baked */
#define BAKE_MASK_NULL		0
#define BAKE_MASK_MARGIN	1
#define BAKE_MASK_BAKED		2
static void bake_mask_filter_extend( char *mask, int width, int height )
{
	char *row1, *row2, *row3;
	int rowlen, x, y;
	char *temprect;
	
	rowlen= width;
	
	/* make a copy, to prevent flooding */
	temprect= MEM_dupallocN(mask);
	
	for(y=1; y<=height; y++) {
		/* setup rows */
		row1= (char *)(temprect + (y-2)*rowlen);
		row2= row1 + rowlen;
		row3= row2 + rowlen;
		if(y==1)
			row1= row2;
		else if(y==height)
			row3= row2;
		
		for(x=0; x<rowlen; x++) {
			if (mask[((y-1)*rowlen)+x]==0) {
				if (*row1 || *row2 || *row3 || *(row1+1) || *(row3+1) ) {
					mask[((y-1)*rowlen)+x] = BAKE_MASK_MARGIN;
				} else if((x!=rowlen-1) && (*(row1+2) || *(row2+2) || *(row3+2)) ) {
					mask[((y-1)*rowlen)+x] = BAKE_MASK_MARGIN;
				}
			}
			
			if(x!=0) {
				row1++; row2++; row3++;
			}
		}
	}
	MEM_freeN(temprect);
}

static void bake_mask_clear( ImBuf *ibuf, char *mask, char val )
{
	int x,y;
	if (ibuf->rect_float) {
		for(x=0; x<ibuf->x; x++) {
			for(y=0; y<ibuf->y; y++) {
				if (mask[ibuf->x*y + x] == val) {
					float *col= ibuf->rect_float + 4*(ibuf->x*y + x);
					col[0] = col[1] = col[2] = col[3] = 0.0f;
				}
			}
		}
		
	} else {
		/* char buffer */
		for(x=0; x<ibuf->x; x++) {
			for(y=0; y<ibuf->y; y++) {
				if (mask[ibuf->x*y + x] == val) {
					char *col= (char *)(ibuf->rect + ibuf->x*y + x);
					col[0] = col[1] = col[2] = col[3] = 0;
				}
			}
		}
	}
}

static void bake_set_shade_input(Render *re, ObjectInstanceRen *obi, VlakRen *vlr, ShadeInput *shi, int quad, int isect, int x, int y, float u, float v)
{
	/* regular scanconvert */
	if(quad) 
		shade_input_set_triangle_i(re, shi, obi, vlr, 0, 2, 3);
	else
		shade_input_set_triangle_i(re, shi, obi, vlr, 0, 1, 2);
		
	/* cache for shadow */
	shi->shading.samplenr= re->sample.shadowsamplenr[shi->shading.thread]++;

	shi->shading.mask= 0xFFFF; /* all samples */
	
	shi->geometry.u= -u;
	shi->geometry.v= -v;
	shi->geometry.xs= x;
	shi->geometry.ys= y;
	
	shade_input_set_uv(shi);
	shade_input_set_normals(shi);

	/* no normal flip */
	if(shi->geometry.flippednor)
		shade_input_flip_normals(shi);

	/* set up view vector to look right at the surface (note that the normal
	 * is negated in the renderer so it does not need to be done here) */
	shi->geometry.view[0]= shi->geometry.vn[0];
	shi->geometry.view[1]= shi->geometry.vn[1];
	shi->geometry.view[2]= shi->geometry.vn[2];

	shade_input_init_material(re, shi);
}

static void bake_shade(void *handle, Object *ob, ShadeInput *shi, int quad, int x, int y, float u, float v, float *tvn, float *ttang)
{
	BakeShade *bs= handle;
	Render *re= bs->re;
	ShadeResult shr;
	VlakRen *vlr= shi->primitive.vlr;
	
	if(bs->type==RE_BAKE_AO) {
		ambient_occlusion(re, shi);

		if(re->params.r.bake_flag & R_BAKE_NORMALIZE) {
			copy_v3_v3(shr.combined, shi->shading.ao);
		}
		else {
			zero_v3(shr.combined);
			environment_lighting_apply(re, shi, &shr);
		}
	}
	else {
		if (bs->type==RE_BAKE_SHADOW) /* Why do shadows set the color anyhow?, ignore material color for baking */
			shi->material.r = shi->material.g = shi->material.b = 1.0f;
	
		shade_input_set_shade_texco(re, shi);
		
		if(ELEM3(bs->type, RE_BAKE_NORMALS, RE_BAKE_TEXTURE, RE_BAKE_SHADOW))
			shi->shading.combinedflag &= ~(SCE_PASS_AO|SCE_PASS_ENVIRONMENT|SCE_PASS_INDIRECT);
		
		if(shi->material.mat->nodetree && shi->material.mat->use_nodes) {
			ntreeShaderExecTree(shi->material.mat->nodetree, re, shi, &shr);
			shi->material.mat= vlr->mat;		/* shi->material.mat is being set in nodetree */
		}
		else
			shade_material_loop(re, shi, &shr);
		
		if(bs->type==RE_BAKE_NORMALS) {
			float nor[3];

			copy_v3_v3(nor, shi->geometry.vn);

			if(re->params.r.bake_normal_space == R_BAKE_SPACE_CAMERA);
			else if(re->params.r.bake_normal_space == R_BAKE_SPACE_TANGENT) {
				float mat[3][3], imat[3][3];

				/* bitangent */
				if(tvn && ttang) {
					copy_v3_v3(mat[0], ttang);
					cross_v3_v3v3(mat[1], tvn, ttang);
					copy_v3_v3(mat[2], tvn);
				}
				else {
					copy_v3_v3(mat[0], shi->texture.nmaptang);
					cross_v3_v3v3(mat[1], shi->geometry.vn, shi->texture.nmaptang);
					copy_v3_v3(mat[2], shi->geometry.vn);
				}

				invert_m3_m3(imat, mat);
				mul_m3_v3(imat, nor);
			}
			else if(re->params.r.bake_normal_space == R_BAKE_SPACE_OBJECT)
				mul_mat3_m4_v3(ob->imat, nor); /* ob->imat includes viewinv! */
			else if(re->params.r.bake_normal_space == R_BAKE_SPACE_WORLD)
				mul_mat3_m4_v3(re->cam.viewinv, nor);

			normalize_v3(nor); /* in case object has scaling */

			shr.combined[0]= nor[0]/2.0f + 0.5f;
			shr.combined[1]= 0.5f - nor[1]/2.0f;
			shr.combined[2]= nor[2]/2.0f + 0.5f;
		}
		else if(bs->type==RE_BAKE_TEXTURE) {
			shr.combined[0]= shi->material.r;
			shr.combined[1]= shi->material.g;
			shr.combined[2]= shi->material.b;
			shr.alpha = shi->material.alpha;
		}
		else if(bs->type==RE_BAKE_SHADOW) {
			copy_v3_v3(shr.combined, shr.shad);
			shr.alpha = shi->material.alpha;
		}
	}
	
	if(bs->rect_float) {
		float *col= bs->rect_float + 4*(bs->rectx*y + x);
		copy_v3_v3(col, shr.combined);
		if (bs->type==RE_BAKE_ALL || bs->type==RE_BAKE_TEXTURE) {
			col[3]= shr.alpha;
		} else {
			col[3]= 1.0;
		}
	}
	else {
		char *col= (char *)(bs->rect + bs->rectx*y + x);
		col[0]= FTOCHAR(shr.combined[0]);
		col[1]= FTOCHAR(shr.combined[1]);
		col[2]= FTOCHAR(shr.combined[2]);
		
		
		if (bs->type==RE_BAKE_ALL || bs->type==RE_BAKE_TEXTURE) {
			col[3]= FTOCHAR(shr.alpha);
		} else {
			col[3]= 255;
		}
	}
	
	if (bs->rect_mask) {
		bs->rect_mask[bs->rectx*y + x] = BAKE_MASK_BAKED;
	}
}

static void bake_displacement(void *handle, ShadeInput *shi, float dist, int x, int y)
{
	BakeShade *bs= handle;
	Render *re= bs->re;
	float disp;
	
	if(re->params.r.bake_flag & R_BAKE_NORMALIZE && re->params.r.bake_maxdist) {
		disp = (dist+re->params.r.bake_maxdist) / (re->params.r.bake_maxdist*2); /* alter the range from [-bake_maxdist, bake_maxdist] to [0, 1]*/
	} else {
		disp = 0.5 + dist; /* alter the range from [-0.5,0.5] to [0,1]*/
	}
	
	if(bs->rect_float) {
		float *col= bs->rect_float + 4*(bs->rectx*y + x);
		col[0] = col[1] = col[2] = disp;
		col[3]= 1.0f;
	} else {	
		char *col= (char *)(bs->rect + bs->rectx*y + x);
		col[0]= FTOCHAR(disp);
		col[1]= FTOCHAR(disp);
		col[2]= FTOCHAR(disp);
		col[3]= 255;
	}
	if (bs->rect_mask) {
		bs->rect_mask[bs->rectx*y + x] = BAKE_MASK_BAKED;
	}
}

#if 0
static int bake_check_intersect(Isect *is, int ob, RayFace *face)
{
	BakeShade *bs = (BakeShade*)is->userdata;
	Render *re= bs->re;
	
	/* no direction checking for now, doesn't always improve the result
	 * (dot_v3v3(shi->geometry.facenor, bs->dir) > 0.0f); */

	return (re->objectinstance[ob & ~RE_RAY_TRANSFORM_OFFS].obr->ob != bs->actob);
}
#endif

static int bake_intersect_tree(Render *re, RayObject* raytree, Isect* isect, float *start, float *dir, float sign, float *hitco, float *dist)
{
	//TODO, validate against blender 2.4x, results may have changed.
	float maxdist;
	int hit;

	/* might be useful to make a user setting for maxsize*/
	if(re->params.r.bake_maxdist > 0.0f)
		maxdist= re->params.r.bake_maxdist;
	else
		maxdist= FLT_MAX + re->params.r.bake_biasdist;
	
	/* 'dir' is always normalized */
	madd_v3_v3v3fl(isect->start, start, dir, -re->params.r.bake_biasdist);

	isect->vec[0] = dir[0]*maxdist*sign;
	isect->vec[1] = dir[1]*maxdist*sign;
	isect->vec[2] = dir[2]*maxdist*sign;

	isect->labda = maxdist;

	/* TODO, 2.4x had this...
	hit = RE_ray_tree_intersect_check(R.raytree, isect, bake_check_intersect);
	...the active object may NOT be ignored in some cases.
	*/

	hit = RE_rayobject_raycast(raytree, isect);
	if(hit) {
		hitco[0] = isect->start[0] + isect->labda*isect->vec[0];
		hitco[1] = isect->start[1] + isect->labda*isect->vec[1];
		hitco[2] = isect->start[2] + isect->labda*isect->vec[2];

		*dist= len_v3v3(start, hitco);
	}

	return hit;
}

static void bake_set_vlr_dxyco(BakeShade *bs, float *uv1, float *uv2, float *uv3)
{
	VlakRen *vlr= bs->vlr;
	float A, d[3], *v1, *v2, *v3;

	if(bs->quad) {
		v1= vlr->v1->co;
		v2= vlr->v3->co;
		v3= vlr->v4->co;
	}
	else {
		v1= vlr->v1->co;
		v2= vlr->v2->co;
		v3= vlr->v3->co;
	}

	/* formula derived from barycentric coordinates:
	 * (uvArea1*v1 + uvArea2*v2 + uvArea3*v3)/uvArea
	 * then taking u and v partial derivatives to get dxco and dyco */
	A= (uv2[0] - uv1[0])*(uv3[1] - uv1[1]) - (uv3[0] - uv1[0])*(uv2[1] - uv1[1]);

	if(fabs(A) > FLT_EPSILON) {
		A= 0.5f/A;

		d[0]= uv2[1] - uv3[1];
		d[1]= uv3[1] - uv1[1];
		d[2]= uv1[1] - uv2[1];
		mul_v3_fl(d, A);
		interp_v3_v3v3v3(bs->dxco, v1, v2, v3, d);

		d[0]= uv3[0] - uv2[0];
		d[1]= uv1[0] - uv3[0];
		d[2]= uv2[0] - uv1[0];
		mul_v3_fl(d, A);
		interp_v3_v3v3v3(bs->dyco, v1, v2, v3, d);
	}
	else {
		zero_v3(bs->dxco);
		zero_v3(bs->dyco);
	}

	if(bs->obi->flag & R_TRANSFORMED) {
		mul_m3_v3(bs->obi->nmat, bs->dxco);
		mul_m3_v3(bs->obi->nmat, bs->dyco);
	}
}

static void do_bake_shade(void *handle, int x, int y, float u, float v)
{
	BakeShade *bs= handle;
	Render *re= bs->re;
	VlakRen *vlr= bs->vlr;
	ObjectInstanceRen *obi= bs->obi;
	Object *ob= obi->obr->ob;
	float l, *v1, *v2, *v3, tvn[3], ttang[3];
	int quad;
	ShadeSample *ssamp= &bs->ssamp;
	ShadeInput *shi= ssamp->shi;
	
	/* fast threadsafe break test */
	if(re->cb.test_break(re->cb.tbh))
		return;
	
	/* setup render coordinates */
	if(bs->quad) {
		v1= vlr->v1->co;
		v2= vlr->v3->co;
		v3= vlr->v4->co;
	}
	else {
		v1= vlr->v1->co;
		v2= vlr->v2->co;
		v3= vlr->v3->co;
	}
	
	/* renderco */
	l= 1.0f-u-v;
	
	shi->geometry.co[0]= l*v3[0]+u*v1[0]+v*v2[0];
	shi->geometry.co[1]= l*v3[1]+u*v1[1]+v*v2[1];
	shi->geometry.co[2]= l*v3[2]+u*v1[2]+v*v2[2];
	
	if(obi->flag & R_TRANSFORMED)
		mul_m4_v3(obi->mat, shi->geometry.co);
	
	copy_v3_v3(shi->geometry.dxco, bs->dxco);
	copy_v3_v3(shi->geometry.dyco, bs->dyco);

	quad= bs->quad;
	bake_set_shade_input(re, obi, vlr, shi, quad, 0, x, y, u, v);

	if(bs->type==RE_BAKE_NORMALS && re->params.r.bake_normal_space==R_BAKE_SPACE_TANGENT) {
		shade_input_set_shade_texco(re, shi);
		copy_v3_v3(tvn, shi->geometry.vn);
		copy_v3_v3(ttang, shi->texture.nmaptang);
	}

	/* if we are doing selected to active baking, find point on other face */
	if(bs->actob) {
		Isect isec, minisec;
		float co[3], minco[3], dist, mindist=0.0f;
		int hit, sign, dir=1;
		
		/* intersect with ray going forward and backward*/
		hit= 0;
		memset(&minisec, 0, sizeof(minisec));
		minco[0]= minco[1]= minco[2]= 0.0f;
		
		copy_v3_v3(bs->dir, shi->geometry.vn);
		
		for(sign=-1; sign<=1; sign+=2) {
			memset(&isec, 0, sizeof(isec));
			isec.mode= RE_RAY_MIRROR;

			isec.orig.ob   = obi;
			isec.orig.face = vlr;
			isec.userdata= bs;
			
			if(bake_intersect_tree(re, re->db.raytree, &isec, shi->geometry.co, shi->geometry.vn, sign, co, &dist)) {
				if(!hit || len_v3v3(shi->geometry.co, co) < len_v3v3(shi->geometry.co, minco)) {
					minisec= isec;
					mindist= dist;
					copy_v3_v3(minco, co);
					hit= 1;
					dir = sign;
				}
			}
		}

		if (bs->type==RE_BAKE_DISPLACEMENT) {
			if(hit)
				bake_displacement(handle, shi, (dir==-1)? mindist:-mindist, x, y);
			else
				bake_displacement(handle, shi, 0.0f, x, y);
			return;
		}

		/* if hit, we shade from the new point, otherwise from point one starting face */
		if(hit) {
			obi= (ObjectInstanceRen*)minisec.hit.ob;
			vlr= (VlakRen*)minisec.hit.face;
			quad= (minisec.isect == 2);
			copy_v3_v3(shi->geometry.co, minco);
			
			u= -minisec.u;
			v= -minisec.v;
			bake_set_shade_input(re, obi, vlr, shi, quad, 1, x, y, u, v);
		}
	}

	if(bs->type==RE_BAKE_NORMALS && re->params.r.bake_normal_space==R_BAKE_SPACE_TANGENT)
		bake_shade(handle, ob, shi, quad, x, y, u, v, tvn, ttang);
	else
		bake_shade(handle, ob, shi, quad, x, y, u, v, 0, 0);
}

static int get_next_bake_face(Render *re, BakeShade *bs)
{
	ObjectRen *obr;
	VlakRen *vlr;
	MTFace *tface;
	static int v= 0, vdone= 0;
	static ObjectInstanceRen *obi= NULL;
	
	if(bs==NULL) {
		vlr= NULL;
		v= vdone= 0;
		obi= re->db.instancetable.first;
		return 0;
	}

	BLI_lock_thread(LOCK_CUSTOM1);	

	for(; obi; obi=obi->next, v=0) {
		obr= obi->obr;

		for(; v<obr->totvlak; v++) {
			vlr= render_object_vlak_get(obr, v);

			if((bs->actob && bs->actob == obr->ob) || (!bs->actob && (obr->ob->flag & SELECT))) {
				tface= render_vlak_get_tface(obr, vlr, obr->bakemtface, NULL, 0);

				if(tface && tface->tpage) {
					Image *ima= tface->tpage;
					ImBuf *ibuf= BKE_image_get_ibuf(ima, NULL);
					float vec[4]= {0.0f, 0.0f, 0.0f, 0.0f};
					
					if(ibuf==NULL)
						continue;
					
					if(ibuf->rect==NULL && ibuf->rect_float==NULL)
						continue;
					
					if(ibuf->rect_float && !(ibuf->channels==0 || ibuf->channels==4))
						continue;
					
					/* find the image for the first time? */
					if(ima->id.flag & LIB_DOIT) {
						ima->id.flag &= ~LIB_DOIT;
						
						/* we either fill in float or char, this ensures things go fine */
						if(ibuf->rect_float)
							imb_freerectImBuf(ibuf);
						/* clear image */
						if(re->params.r.bake_flag & R_BAKE_CLEAR)
							IMB_rectfill(ibuf, vec);
					
						/* might be read by UI to set active image for display */
						re->bakebuf= ima;
					}				
					
					bs->obi= obi;
					bs->vlr= vlr;
					
					bs->vdone++;	/* only for error message if nothing was rendered */
					v++;
					
					BLI_unlock_thread(LOCK_CUSTOM1);
					return 1;
				}
			}
		}
	}
	
	BLI_unlock_thread(LOCK_CUSTOM1);
	return 0;
}

/* already have tested for tface and ima and zspan */
static void shade_tface(BakeShade *bs)
{
	Render *re= bs->re;
	VlakRen *vlr= bs->vlr;
	ObjectInstanceRen *obi= bs->obi;
	ObjectRen *obr= obi->obr;
	MTFace *tface= render_vlak_get_tface(obr, vlr, obr->bakemtface, NULL, 0);
	Image *ima= tface->tpage;
	float vec[4][2];
	int a, i1, i2, i3;
	
	/* check valid zspan */
	if(ima!=bs->ima) {
		bs->ima= ima;
		bs->ibuf= BKE_image_get_ibuf(ima, NULL);
		/* note, these calls only free/fill contents of zspan struct, not zspan itself */
		zbuf_free_span(bs->zspan);
		zbuf_alloc_span(bs->zspan, bs->ibuf->x, bs->ibuf->y, re->cam.clipcrop);
	}				
	
	bs->rectx= bs->ibuf->x;
	bs->recty= bs->ibuf->y;
	bs->rect= bs->ibuf->rect;
	bs->rect_float= bs->ibuf->rect_float;
	bs->quad= 0;
	
	if (bs->usemask) {
		if (bs->ibuf->userdata==NULL) {
			BLI_lock_thread(LOCK_CUSTOM1);
			if (bs->ibuf->userdata==NULL) { /* since the thread was locked, its possible another thread alloced the value */
				bs->ibuf->userdata = (void *)MEM_callocN(sizeof(char)*bs->rectx*bs->recty, "BakeMask");
				bs->rect_mask= (char *)bs->ibuf->userdata;
			}
			BLI_unlock_thread(LOCK_CUSTOM1);
		} else {
			bs->rect_mask= (char *)bs->ibuf->userdata;
		}
	}
	
	/* get pixel level vertex coordinates */
	for(a=0; a<4; a++) {
		/* Note, workaround for pixel aligned UVs which are common and can screw up our intersection tests
		 * where a pixel gets inbetween 2 faces or the middle of a quad,
		 * camera aligned quads also have this problem but they are less common.
		 * Add a small offset to the UVs, fixes bug #18685 - Campbell */
		vec[a][0]= tface->uv[a][0]*(float)bs->rectx - (0.5f + 0.001);
		vec[a][1]= tface->uv[a][1]*(float)bs->recty - (0.5f + 0.002);
	}
	
	/* UV indices have to be corrected for possible quad->tria splits */
	i1= 0; i2= 1; i3= 2;
	vlr_set_uv_indices(vlr, &i1, &i2, &i3);
	bake_set_vlr_dxyco(bs, vec[i1], vec[i2], vec[i3]);
	zspan_scanconvert(bs->zspan, bs, vec[i1], vec[i2], vec[i3], do_bake_shade);
	
	if(vlr->v4) {
		bs->quad= 1;
		bake_set_vlr_dxyco(bs, vec[0], vec[2], vec[3]);
		zspan_scanconvert(bs->zspan, bs, vec[0], vec[2], vec[3], do_bake_shade);
	}
}

static void *do_bake_thread(void *bs_v)
{
	BakeShade *bs= bs_v;
	Render *re= bs->re;
	
	while(get_next_bake_face(re, bs)) {
		shade_tface(bs);
		
		/* fast threadsafe break test */
		if(re->cb.test_break(re->cb.tbh))
			break;

		/* access is not threadsafe but since its just true/false probably ok
		 * only used for interactive baking */
		if(bs->do_update)
			*bs->do_update= TRUE;
	}
	bs->ready= 1;
	
	return NULL;
}

/* using object selection tags, the faces with UV maps get baked */
/* render should have been setup */
/* returns 0 if nothing was handled */
int RE_bake_shade_all_selected(Render *re, int type, Object *actob, short *do_update)
{
	BakeShade *handles;
	ListBase threads;
	Image *ima;
	int a, vdone=0, usemask=0;
	
	re->bakebuf= NULL;
	
	/* initialize static vars */
	get_next_bake_face(re, NULL);
	
	/* do we need a mask? */
	if (re->params.r.bake_filter && (re->params.r.bake_flag & R_BAKE_CLEAR)==0)
		usemask = 1;
	
	/* baker uses this flag to detect if image was initialized */
	for(ima= G.main->image.first; ima; ima= ima->id.next) {
		ImBuf *ibuf= BKE_image_get_ibuf(ima, NULL);
		ima->id.flag |= LIB_DOIT;
		if (ibuf)
			ibuf->userdata = NULL; /* use for masking if needed */
	}
	
	BLI_init_threads(&threads, do_bake_thread, re->params.r.threads);

	handles= MEM_callocN(sizeof(BakeShade)*re->params.r.threads, "BakeShade");

	/* get the threads running */
	for(a=0; a<re->params.r.threads; a++) {
		/* set defaults in handles */
		handles[a].re= re;
		handles[a].ssamp.shi[0].shading.lay= re->db.scene->lay;
		
		if (type==RE_BAKE_SHADOW) {
			handles[a].ssamp.shi[0].shading.passflag= SCE_PASS_SHADOW;
		} else {
			handles[a].ssamp.shi[0].shading.passflag= SCE_PASS_COMBINED;
		}
		handles[a].ssamp.shi[0].shading.combinedflag= ~(SCE_PASS_SPEC);
		handles[a].ssamp.shi[0].shading.thread= a;
		handles[a].ssamp.tot= 1;
		
		handles[a].type= type;
		handles[a].actob= actob;
		handles[a].zspan= MEM_callocN(sizeof(ZSpan), "zspan for bake");
		
		handles[a].usemask = usemask;

		handles[a].do_update = do_update; /* use to tell the view to update */
		
		BLI_insert_thread(&threads, &handles[a]);
	}
	
	/* wait for everything to be done */
	a= 0;
	while(a!=re->params.r.threads) {
		
		PIL_sleep_ms(50);

		for(a=0; a<re->params.r.threads; a++)
			if(handles[a].ready==0)
				break;
	}
	
	/* filter and refresh images */
	for(ima= G.main->image.first; ima; ima= ima->id.next) {
		if((ima->id.flag & LIB_DOIT)==0) {
			ImBuf *ibuf= BKE_image_get_ibuf(ima, NULL);
			if (re->params.r.bake_filter) {
				if (usemask) {
					/* extend the mask +2 pixels from the image,
					 * this is so colors dont blend in from outside */
					char *temprect;
					
					for(a=0; a<re->params.r.bake_filter; a++)
						bake_mask_filter_extend((char *)ibuf->userdata, ibuf->x, ibuf->y);
					
					temprect = MEM_dupallocN(ibuf->userdata);
					
					/* expand twice to clear this many pixels, so they blend back in */
					bake_mask_filter_extend(temprect, ibuf->x, ibuf->y);
					bake_mask_filter_extend(temprect, ibuf->x, ibuf->y);
					
					/* clear all pixels in the margin*/
					bake_mask_clear(ibuf, temprect, BAKE_MASK_MARGIN);
					MEM_freeN(temprect);
				}
				
				for(a=0; a<re->params.r.bake_filter; a++) {
					/*the mask, ibuf->userdata - can be null, in this case only zero alpha is used */
					IMB_filter_extend(ibuf, (char *)ibuf->userdata);
				}
				
				if (ibuf->userdata) {
					MEM_freeN(ibuf->userdata);
					ibuf->userdata= NULL;
				}
			}
			ibuf->userflags |= IB_BITMAPDIRTY;
			if (ibuf->rect_float) IMB_rect_from_float(ibuf);
		}
	}
	
	/* calculate return value */
 	for(a=0; a<re->params.r.threads; a++) {
		vdone+= handles[a].vdone;
		
		zbuf_free_span(handles[a].zspan);
		MEM_freeN(handles[a].zspan);
 	}

	MEM_freeN(handles);
	
	BLI_end_threads(&threads);

	return vdone;
}

struct Image *RE_bake_shade_get_image(Render *re)
{
	return re->bakebuf;
}


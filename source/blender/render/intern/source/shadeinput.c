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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2006 Blender Foundation
 * All rights reserved.
 *
 * Contributors: Hos, Robert Wenzlaff.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdio.h>
#include <math.h>
#include <string.h>

#include "BLI_blenlib.h"
#include "BLI_math.h"

#include "DNA_curve_types.h"
#include "DNA_group_types.h"
#include "DNA_lamp_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"

#include "BKE_colortools.h"
#include "BKE_node.h"
#include "BKE_utildefines.h"

/* local include */
#include "camera.h"
#include "database.h"
#include "environment.h"
#include "part.h"
#include "pixelfilter.h"
#include "raycounter.h"
#include "render_types.h"
#include "rendercore.h"
#include "shading.h"
#include "shadowbuf.h"
#include "object.h"
#include "object_mesh.h"
#include "object_strand.h"
#include "texture_stack.h"
#include "volumetric.h"
#include "zbuf.h"

/* Shade Sample order:

- shade_samples_from_pixel()
	- for each sample
		- shade_input_set_triangle()  <- if prev sample-face is same, use shade_input_copy_triangle()
		- if vlr
			- shade_input_set_viewco()    <- not for ray or bake
			- shade_input_set_uv()        <- not for ray or bake
			- shade_input_set_normals()
- shade_samples()
	- for each sample
		- shade_input_set_shade_texco()
		- shade_input_do_shade()
- OSA: distribute sample result with filter masking

*/

/* initialise material variables in shadeinput, 
 * doing inverse gamma correction where applicable */
void shade_input_init_material(Render *re, ShadeInput *shi)
{
	/* note, keep this synced with render_types.h */
	memcpy(&shi->material.r, &shi->material.mat->r, 21*sizeof(float));
	shi->material.har= shi->material.mat->har;
}

/* **************************************************************************** */
/*                    ShadeInput                                                */
/* **************************************************************************** */


void vlr_set_uv_indices(VlakRen *vlr, int *i1, int *i2, int *i3)
{
	/* to prevent storing new tfaces or vcols, we check a split runtime */
	/* 		4---3		4---3 */
	/*		|\ 1|	or  |1 /| */
	/*		|0\ |		|/ 0| */
	/*		1---2		1---2 	0 = orig face, 1 = new face */
	
	/* Update vert nums to point to correct verts of original face */
	if(vlr->flag & R_DIVIDE_24) {  
		if(vlr->flag & R_FACE_SPLIT) {
			(*i1)++; (*i2)++; (*i3)++;
		}
		else {
			(*i3)++;
		}
	}
	else if(vlr->flag & R_FACE_SPLIT) {
		(*i2)++; (*i3)++; 
	}
}

/* copy data from face to ShadeInput, general case */
/* indices 0 1 2 3 only */
void shade_input_set_triangle_i(Render *re, ShadeInput *shi, ObjectInstanceRen *obi, VlakRen *vlr, short i1, short i2, short i3)
{
	VertRen **vpp= &vlr->v1;
	ShadeGeometry *geom= &shi->geometry;
	ShadePrimitive *prim= &shi->primitive;
	ShadeMaterial *mat= &shi->material;
	
	prim->vlr= vlr;
	prim->obi= obi;
	prim->obr= obi->obr;

	prim->v1= vpp[i1];
	prim->v2= vpp[i2];
	prim->v3= vpp[i3];
	
	prim->i1= i1;
	prim->i2= i2;
	prim->i3= i3;
	
	/* note, mat->mat is set in node shaders */
	mat->mat= (mat->mat_override)? mat->mat_override: vlr->mat;
	mat->mode= mat->mat->mode_l;		/* or-ed result for all nodes */

	geom->osatex= (mat->mat->texco & TEXCO_OSA);

	/* facenormal copy, can get flipped */
	geom->flippednor= render_vlak_get_normal(obi, vlr, geom->facenor, (i3 == 3));

	/* copy of original pre-flipped normal, for geometry->front/back node output */
	copy_v3_v3(geom->orignor, geom->facenor);
	if(geom->flippednor)
		mul_v3_fl(geom->orignor, -1.0f);
}

/* note, facenr declared volatile due to over-eager -O2 optimizations
 * on cygwin (particularly -frerun-cse-after-loop)
 */

/* full osa case: copy static info */
static void shade_input_copy_triangle(ShadeInput *shi, ShadeInput *from)
{
	shi->geometry= from->geometry;
	shi->primitive= from->primitive;
	shi->material.mode= from->material.mode;
	shi->material.mat= from->material.mat;
}

/* copy data from strand to shadeinput */
void shade_input_set_strand(Render *re, ShadeInput *shi, StrandRen *strand, StrandPoint *spoint)
{
	ShadeGeometry *geom= &shi->geometry;
	ShadeMaterial *mat= &shi->material;

	/* note, mat->mat is set in node shaders */
	mat->mat= mat->mat_override? mat->mat_override: strand->buffer->ma;
	
	geom->osatex= (mat->mat->texco & TEXCO_OSA);
	mat->mode= mat->mat->mode_l;		/* or-ed result for all nodes */

	/* shade_input_set_viewco equivalent */
	copy_v3_v3(geom->co, spoint->co);
	copy_v3_v3(geom->view, geom->co);
	normalize_v3(geom->view);

	geom->xs= (int)spoint->x;
	geom->ys= (int)spoint->y;

	if(geom->osatex || (re->params.r.mode & R_SHADOW)) {
		copy_v3_v3(geom->dxco, spoint->dtco);
		copy_v3_v3(geom->dyco, spoint->dsco);
	}

	/* dxview, dyview, not supported */

	/* facenormal, simply viewco flipped */
	copy_v3_v3(geom->facenor, spoint->nor);
	copy_v3_v3(geom->orignor, geom->facenor);

	/* shade_input_set_normals equivalent */
	if(mat->mat->mode & MA_TANGENT_STR) {
		copy_v3_v3(geom->vn, spoint->tan);
	}
	else {
		float cross[3];

		cross_v3_v3v3(cross, spoint->co, spoint->tan);
		cross_v3_v3v3(geom->vn, cross, spoint->tan);
		normalize_v3(geom->vn);

		if(dot_v3v3(geom->vn, geom->view) < 0.0f)
			negate_v3(geom->vn);
	}

	copy_v3_v3(geom->vno, geom->vn);

	geom->tangentvn= (mat->mat->mode & MA_TANGENT_STR) != 0;
}

void shade_input_set_strand_texco(Render *re, ShadeInput *shi, StrandRen *strand, StrandVert *svert, StrandPoint *spoint)
{
	StrandBuffer *strandbuf= strand->buffer;
	ObjectRen *obr= strandbuf->obr;
	StrandVert *sv;
	ShadeGeometry *geom= &shi->geometry;
	ShadeTexco *tex= &shi->texture;
	ShadeMaterial *mat= &shi->material;
	int mode= mat->mode;		/* or-ed result for all nodes */
	short texco= mat->mat->texco;

	if((mat->mat->texco & TEXCO_REFL)) {
		/* tex->dxview, tex->dyview, not supported */
	}

	if(geom->osatex && (texco & (TEXCO_NORM|TEXCO_REFL))) {
		/* not supported */
	}

	if(mode & (MA_TANGENT_V|MA_NORMAP_TANG)) {
		copy_v3_v3(geom->tang, spoint->tan);
		copy_v3_v3(tex->nmaptang, spoint->tan);
	}

	if(mode & MA_STR_SURFDIFF) {
		float *surfnor= render_strand_get_surfnor(obr, strand, 0);

		if(surfnor)
			copy_v3_v3(geom->surfnor, surfnor);
		else
			copy_v3_v3(geom->surfnor, geom->vn);

		if(mat->mat->strand_surfnor > 0.0f) {
			geom->surfdist= 0.0f;
			for(sv=strand->vert; sv!=svert; sv++)
				geom->surfdist+=len_v3v3(sv->co, (sv+1)->co);
			geom->surfdist += spoint->t*len_v3v3(sv->co, (sv+1)->co);
		}
	}

	if(re->params.r.mode & R_SPEED) {
		float *speed;
		
		speed= render_strand_get_winspeed(shi->primitive.obi, strand, 0);
		if(speed)
			copy_v4_v4(tex->winspeed, speed);
		else
			zero_v4(tex->winspeed);
	}

	/* shade_input_set_shade_texco equivalent */
	if(texco & NEED_UV) {
		if(texco & TEXCO_ORCO) {
			copy_v3_v3(tex->lo, render_strand_get_orco(obr, strand, 0));
			/* no geom->osatex, orco derivatives are zero */
		}

		if(texco & TEXCO_GLOB) {
			copy_v3_v3(tex->gl, geom->co);
			mul_m4_v3(re->cam.viewinv, tex->gl);
			
			if(geom->osatex) {
				copy_v3_v3(tex->dxgl, geom->dxco);
				mul_mat3_m4_v3(re->cam.viewinv, geom->dxco);
				copy_v3_v3(tex->dygl, geom->dyco);
				mul_mat3_m4_v3(re->cam.viewinv, geom->dyco);
			}
		}

		if(texco & TEXCO_STRAND) {
			tex->strandco= spoint->strandco;

			if(geom->osatex) {
				tex->dxstrand= spoint->dtstrandco;
				tex->dystrand= 0.0f;
			}
		}

		if((texco & TEXCO_UV) || (mode & (MA_VERTEXCOL|MA_VERTEXCOLP|MA_FACETEXTURE)))  {
			MCol *mcol;
			float *uv;
			char *name;
			int i;

			tex->totuv= 0;
			tex->totcol= 0;
			tex->actuv= obr->actmtface;
			tex->actcol= obr->actmcol;

			if(mode & (MA_VERTEXCOL|MA_VERTEXCOLP)) {
				for (i=0; (mcol=render_strand_get_mcol(obr, strand, i, &name, 0)); i++) {
					ShadeInputCol *scol= &tex->col[i];
					char *cp= (char*)mcol;
					
					tex->totcol++;
					scol->name= name;

					scol->col[0]= cp[3]/255.0f;
					scol->col[1]= cp[2]/255.0f;
					scol->col[2]= cp[1]/255.0f;
				}

				if(tex->totcol) {
					mat->vcol[0]= tex->col[tex->actcol].col[0];
					mat->vcol[1]= tex->col[tex->actcol].col[1];
					mat->vcol[2]= tex->col[tex->actcol].col[2];
				}
				else {
					mat->vcol[0]= 0.0f;
					mat->vcol[1]= 0.0f;
					mat->vcol[2]= 0.0f;
				}
			}

			for (i=0; (uv=render_strand_get_uv(obr, strand, i, &name, 0)); i++) {
				ShadeInputUV *suv= &tex->uv[i];

				tex->totuv++;
				suv->name= name;

				if(strandbuf->overrideuv == i) {
					suv->uv[0]= -1.0f;
					suv->uv[1]= spoint->strandco;
					suv->uv[2]= 0.0f;
				}
				else {
					suv->uv[0]= -1.0f + 2.0f*uv[0];
					suv->uv[1]= -1.0f + 2.0f*uv[1];
					suv->uv[2]= 0.0f;	/* texture.c assumes there are 3 coords */
				}

				if(geom->osatex) {
					suv->dxuv[0]= 0.0f;
					suv->dxuv[1]= 0.0f;
					suv->dyuv[0]= 0.0f;
					suv->dyuv[1]= 0.0f;
				}

				if((mode & MA_FACETEXTURE) && i==obr->actmtface) {
					if((mode & (MA_VERTEXCOL|MA_VERTEXCOLP))==0) {
						mat->vcol[0]= 1.0f;
						mat->vcol[1]= 1.0f;
						mat->vcol[2]= 1.0f;
					}
				}
			}

			if(tex->totuv == 0) {
				ShadeInputUV *suv= &tex->uv[0];

				suv->uv[0]= 0.0f;
				suv->uv[1]= spoint->strandco;
				suv->uv[2]= 0.0f;	/* texture.c assumes there are 3 coords */
				
				if(mode & MA_FACETEXTURE) {
					/* no tface? set at 1.0f */
					mat->vcol[0]= 1.0f;
					mat->vcol[1]= 1.0f;
					mat->vcol[2]= 1.0f;
				}
			}

		}

		if(texco & TEXCO_NORM)
			negate_v3_v3(tex->orn, geom->vn);

		if(texco & TEXCO_REFL) {
			/* mirror reflection color textures (and envmap) */
			shade_input_calc_reflection(shi);    /* wrong location for normal maps! XXXXXXXXXXXXXX */
		}

		if(texco & TEXCO_STRESS) {
			/* not supported */
		}

		if(texco & TEXCO_TANGENT) {
			if((mode & MA_TANGENT_V)==0) {
				/* just prevent surprises */
				geom->tang[0]= geom->tang[1]= geom->tang[2]= 0.0f;
				tex->nmaptang[0]= tex->nmaptang[1]= tex->nmaptang[2]= 0.0f;
			}
		}
	}

	/* this only avalailable for scanline renders */
	if(shi->shading.depth==0) {
		if(texco & TEXCO_WINDOW) {
			tex->winco[0]= -1.0f + 2.0f*spoint->x/(float)re->cam.winx;
			tex->winco[1]= -1.0f + 2.0f*spoint->y/(float)re->cam.winy;
			tex->winco[2]= 0.0f;

			/* not supported */
			if(geom->osatex) {
				tex->dxwin[0]= 0.0f;
				tex->dywin[1]= 0.0f;
				tex->dxwin[0]= 0.0f;
				tex->dywin[1]= 0.0f;
			}
		}

		if(texco & TEXCO_STICKY) {
			/* not supported */
		}
	}
	
	if(re->params.r.color_mgt_flag & R_COLOR_MANAGEMENT)
		if(mode & (MA_VERTEXCOL|MA_VERTEXCOLP|MA_FACETEXTURE))
			srgb_to_linearrgb_v3_v3(mat->vcol, mat->vcol);
}

/* from scanline pixel coordinates to 3d coordinates, requires set_triangle */
void shade_input_calc_viewco(Render *re, ShadeInput *shi, float x, float y, float z, float *view, float *dxyview, float *co, float *dxco, float *dyco)
{

	if(shi->material.mat->material_type == MA_TYPE_WIRE) {
		/* returns not normalized, so is in viewplane coords */
		camera_raster_to_view(&re->cam, view, x, y);
		normalize_v3(view);

		/* wire cannot use normal for calculating shi->geometry.co, so we
		 * reconstruct the coordinate less accurate using z-value */
		camera_raster_to_co(&re->cam, co, x, y, z);

		/* TODO dxco/dyco/dxyview ? */
	}
	else {
		/* for non-wire, intersect with the triangle to get the exact coord */
		ShadePrimitive *prim= &shi->primitive;
		float plane[4]; /* plane equation a*x + b*y + c*z = d */
		float v1[3];

		/* compute plane equation */
		copy_v3_v3(v1, prim->v1->co);
		if(prim->obi->flag & R_TRANSFORMED)
			mul_m4_v3(prim->obi->mat, v1);
		
		copy_v3_v3(plane, shi->geometry.facenor);
		plane[3]= v1[0]*plane[0] + v1[1]*plane[1] + v1[2]*plane[2];

		/* reconstruct */
		camera_raster_plane_to_co(&re->cam, co, dxco, dyco, view, dxyview, x, y, plane);
	}
	
	/* set camera coords - for scanline, it's always 0.0,0.0,0.0 (render is in camera space)
	 * however for raytrace it can be different - the position of the last intersection */
	/* TODO this is not correct for ortho! */
	shi->geometry.camera_co[0] = shi->geometry.camera_co[1] = shi->geometry.camera_co[2] = 0.0f;
}

/* from scanline pixel coordinates to 3d coordinates, requires set_triangle */
void shade_input_set_viewco(Render *re, ShadeInput *shi, float x, float y, float xs, float ys, float z)
{
	ShadeGeometry *geom= &shi->geometry;
	float *dxyview= NULL, *dxco= NULL, *dyco= NULL;
	
	/* currently in use for dithering (soft shadow), node preview, irregular shad */
	geom->xs= (int)xs;
	geom->ys= (int)ys;

	/* original scanline coordinate without jitter */
	geom->scanco[0]= x;
	geom->scanco[1]= y;
	geom->scanco[2]= z;

	/* check if we need derivatives */
	if(geom->osatex || (re->params.r.mode & R_SHADOW)) {
		dxco= geom->dxco;
		dyco= geom->dyco;

		if((shi->material.mat->texco & TEXCO_REFL))
			dxyview= &geom->dxview;
	}

	shade_input_calc_viewco(re, shi, xs, ys, z, geom->view, dxyview, geom->co, dxco, dyco);
}

/* calculate U and V, for scanline (silly render face u and v are in range -1 to 0) */
void shade_input_set_uv(ShadeInput *shi)
{
	ShadePrimitive *prim= &shi->primitive;
	ShadeGeometry *geom= &shi->geometry;
	VlakRen *vlr= prim->vlr;
	
	if((vlr->flag & R_SMOOTH) || (shi->material.mat->texco & NEED_UV) || (shi->shading.passflag & SCE_PASS_UV)) {
		float v1[3], v2[3], v3[3];

		copy_v3_v3(v1, prim->v1->co);
		copy_v3_v3(v2, prim->v2->co);
		copy_v3_v3(v3, prim->v3->co);

		if(prim->obi->flag & R_TRANSFORMED) {
			mul_m4_v3(prim->obi->mat, v1);
			mul_m4_v3(prim->obi->mat, v2);
			mul_m4_v3(prim->obi->mat, v3);
		}

		/* exception case for wire render of edge */
		if(vlr->v2==vlr->v3) {
			float lend, lenc;
			
			lend= len_v3v3(v2, v1);
			lenc= len_v3v3(geom->co, v1);
			
			if(lend==0.0f) {
				geom->u=geom->v= 0.0f;
			}
			else {
				geom->u= - (1.0f - lenc/lend);
				geom->v= 0.0f;
			}
			
			if(geom->osatex) {
				geom->dx_u=  0.0f;
				geom->dx_v=  0.0f;
				geom->dy_u=  0.0f;
				geom->dy_v=  0.0f;
			}
		}
		else {
			/* most of this could become re-used for faces */
			float detsh, t00, t10, t01, t11, xn, yn, zn;
			int axis1, axis2;

			/* find most stable axis to project */
			xn= fabs(geom->facenor[0]);
			yn= fabs(geom->facenor[1]);
			zn= fabs(geom->facenor[2]);

			if(zn>=xn && zn>=yn) { axis1= 0; axis2= 1; }
			else if(yn>=xn && yn>=zn) { axis1= 0; axis2= 2; }
			else { axis1= 1; axis2= 2; }

			/* compute u,v and derivatives */
			t00= v3[axis1]-v1[axis1]; t01= v3[axis2]-v1[axis2];
			t10= v3[axis1]-v2[axis1]; t11= v3[axis2]-v2[axis2];

			detsh= 1.0f/(t00*t11-t10*t01);
			t00*= detsh; t01*=detsh; 
			t10*=detsh; t11*=detsh;

			geom->u= (geom->co[axis1]-v3[axis1])*t11-(geom->co[axis2]-v3[axis2])*t10;
			geom->v= (geom->co[axis2]-v3[axis2])*t00-(geom->co[axis1]-v3[axis1])*t01;
			if(geom->osatex) {
				geom->dx_u=  geom->dxco[axis1]*t11- geom->dxco[axis2]*t10;
				geom->dx_v=  geom->dxco[axis2]*t00- geom->dxco[axis1]*t01;
				geom->dy_u=  geom->dyco[axis1]*t11- geom->dyco[axis2]*t10;
				geom->dy_v=  geom->dyco[axis2]*t00- geom->dyco[axis1]*t01;
			}

			/* u and v are in range -1 to 0, we allow a little bit extra but not too much, screws up speedvectors */
			CLAMP(geom->u, -2.0f, 1.0f);
			CLAMP(geom->v, -2.0f, 1.0f);
		}
	}	
}

void shade_input_set_normals(ShadeInput *shi)
{
	ShadeGeometry *geom= &shi->geometry;
	ShadePrimitive *prim= &shi->primitive;
	ObjectInstanceRen *obi= prim->obi;
	VlakRen *vlr= prim->vlr;
	float u= geom->u, v= geom->v;
	float l= 1.0f+u+v;

	/* calculate vertexnormals */
	if(vlr->flag & R_SMOOTH) {
		copy_v3_v3(prim->n1, prim->v1->n);
		copy_v3_v3(prim->n2, prim->v2->n);
		copy_v3_v3(prim->n3, prim->v3->n);

		if(obi->flag & R_TRANSFORMED) {
			mul_m3_v3(obi->nmat, prim->n1);
			mul_m3_v3(obi->nmat, prim->n2);
			mul_m3_v3(obi->nmat, prim->n3);
		}

		if(!(vlr->flag & (R_NOPUNOFLIP|R_TANGENT))) {
			if(dot_v3v3(geom->facenor, prim->n1) < 0.0f)
				negate_v3(prim->n1);
			if(dot_v3v3(geom->facenor, prim->n2) < 0.0f)
				negate_v3(prim->n2);
			if(dot_v3v3(geom->facenor, prim->n3) < 0.0f)
				negate_v3(prim->n3);
		}
	}
	
	/* calculate vertexnormals */
	if(prim->vlr->flag & R_SMOOTH) {
		float *n1= prim->n1, *n2= prim->n2, *n3= prim->n3;
		
		geom->vn[0]= l*n3[0]-u*n1[0]-v*n2[0];
		geom->vn[1]= l*n3[1]-u*n1[1]-v*n2[1];
		geom->vn[2]= l*n3[2]-u*n1[2]-v*n2[2];
		
		normalize_v3(geom->vn);
	}
	else
		copy_v3_v3(geom->vn, geom->facenor);

	/* used in nodes */
	copy_v3_v3(geom->vno, geom->vn);

	geom->tangentvn= (prim->vlr->flag & R_TANGENT);
}

/* use by raytrace, sss, bake to flip into the right direction */
void shade_input_flip_normals(ShadeInput *shi)
{
	ShadeGeometry *geom= &shi->geometry;

	negate_v3(geom->facenor);
	negate_v3(geom->vn);
	negate_v3(geom->vno);

	geom->flippednor= !geom->flippednor;
}

static void shade_input_vlr_texco_normal(ShadeGeometry *geom, ShadePrimitive *prim)
{
	/* normal already set as default, just do derivatives here */

	if(geom->osatex) {
		if(prim->vlr->flag & R_SMOOTH) {
			float *n1= prim->n1, *n2= prim->n2, *n3= prim->n3;
			float dl;
			
			dl= geom->dx_u + geom->dx_v;
			geom->dxno[0]= dl*n3[0] - geom->dx_u*n1[0] - geom->dx_v*n2[0];
			geom->dxno[1]= dl*n3[1] - geom->dx_u*n1[1] - geom->dx_v*n2[1];
			geom->dxno[2]= dl*n3[2] - geom->dx_u*n1[2] - geom->dx_v*n2[2];

			dl= geom->dy_u + geom->dy_v;
			geom->dyno[0]= dl*n3[0] - geom->dy_u*n1[0] - geom->dy_v*n2[0];
			geom->dyno[1]= dl*n3[1] - geom->dy_u*n1[1] - geom->dy_v*n2[1];
			geom->dyno[2]= dl*n3[2] - geom->dy_u*n1[2] - geom->dy_v*n2[2];
		}
		else {
			/* constant normal over face, zero derivatives */
			zero_v3(geom->dxno);
			zero_v3(geom->dyno);
		}
	}
}

static void shade_input_vlr_texco_window(RenderCamera *cam, ShadeTexco *tex, ShadeGeometry *geom)
{
	float x= geom->xs;
	float y= geom->ys;
	
	tex->winco[0]= -1.0f + 2.0f*x/(float)cam->winx;
	tex->winco[1]= -1.0f + 2.0f*y/(float)cam->winy;
	tex->winco[2]= 0.0f;

	if(geom->osatex) {
		tex->dxwin[0]= 2.0f/(float)cam->winx;
		tex->dywin[1]= 2.0f/(float)cam->winy;
		tex->dxwin[1]= tex->dxwin[2]= 0.0f;
		tex->dywin[0]= tex->dywin[2]= 0.0f;
	}
}

static void shade_input_vlr_texco_sticky(Render *re, ShadeTexco *tex, ShadeGeometry *geom, ShadePrimitive *prim)
{
	ObjectInstanceRen *obi= prim->obi;
	ObjectRen *obr= prim->obr;
	VertRen *v1= prim->v1, *v2= prim->v2, *v3= prim->v3;
	float x= geom->xs;
	float y= geom->ys;
	float *s1, *s2, *s3;
	
	s1= render_vert_get_sticky(obr, v1, 0);
	s2= render_vert_get_sticky(obr, v2, 0);
	s3= render_vert_get_sticky(obr, v3, 0);
	
	if(s1 && s2 && s3) {
		float obwinmat[4][4], winmat[4][4], ho1[4], ho2[4], ho3[4];
		float Zmulx, Zmuly;
		float hox, hoy, l, dl, u, v;
		float s00, s01, s10, s11, detsh;
		
		/* old globals, localized now */
		Zmulx=  ((float)re->cam.winx)/2.0f; Zmuly=  ((float)re->cam.winy)/2.0f;

		camera_window_matrix(&re->cam, winmat);

		if(prim->obi->flag & R_TRANSFORMED)
			mul_m4_m4m4(obwinmat, obi->mat, winmat);
		else
			copy_m4_m4(obwinmat, winmat);

		camera_matrix_co_to_hoco(obwinmat, ho1, v1->co);
		camera_matrix_co_to_hoco(obwinmat, ho2, v2->co);
		camera_matrix_co_to_hoco(obwinmat, ho3, v3->co);
		
		s00= ho3[0]/ho3[3] - ho1[0]/ho1[3];
		s01= ho3[1]/ho3[3] - ho1[1]/ho1[3];
		s10= ho3[0]/ho3[3] - ho2[0]/ho2[3];
		s11= ho3[1]/ho3[3] - ho2[1]/ho2[3];
		
		detsh= s00*s11-s10*s01;
		s00/= detsh; s01/=detsh; 
		s10/=detsh; s11/=detsh;
		
		/* recalc u and v again */
		hox= x/Zmulx -1.0f;
		hoy= y/Zmuly -1.0f;
		u= (hox - ho3[0]/ho3[3])*s11 - (hoy - ho3[1]/ho3[3])*s10;
		v= (hoy - ho3[1]/ho3[3])*s00 - (hox - ho3[0]/ho3[3])*s01;
		l= 1.0f+u+v;
		
		tex->sticky[0]= l*s3[0]-u*s1[0]-v*s2[0];
		tex->sticky[1]= l*s3[1]-u*s1[1]-v*s2[1];
		tex->sticky[2]= 0.0f;
		
		if(geom->osatex) {
			float dxuv[2], dyuv[2];
			dxuv[0]=  s11/Zmulx;
			dxuv[1]=  - s01/Zmulx;
			dyuv[0]=  - s10/Zmuly;
			dyuv[1]=  s00/Zmuly;
			
			dl= dxuv[0] + dxuv[1];
			tex->dxsticky[0]= dl*s3[0] - dxuv[0]*s1[0] - dxuv[1]*s2[0];
			tex->dxsticky[1]= dl*s3[1] - dxuv[0]*s1[1] - dxuv[1]*s2[1];
			dl= dyuv[0] + dyuv[1];
			tex->dysticky[0]= dl*s3[0] - dyuv[0]*s1[0] - dyuv[1]*s2[0];
			tex->dysticky[1]= dl*s3[1] - dyuv[0]*s1[1] - dyuv[1]*s2[1];
		}
	}
}

static void shade_input_vlr_texco_stress(ShadeTexco *tex, ShadeGeometry *geom, ShadePrimitive *prim)
{
	ObjectRen *obr= prim->obr;
	VertRen *v1= prim->v1, *v2= prim->v2, *v3= prim->v3;
	float *s1, *s2, *s3;
	float u= geom->u, v= geom->v;
	float l= 1.0f+u+v;
	
	s1= render_vert_get_stress(obr, v1, 0);
	s2= render_vert_get_stress(obr, v2, 0);
	s3= render_vert_get_stress(obr, v3, 0);

	if(s1 && s2 && s3) {
		tex->stress= l*s3[0] - u*s1[0] - v*s2[0];
		if(tex->stress<1.0f) tex->stress-= 1.0f;
		else tex->stress= (tex->stress-1.0f)/tex->stress;
	}
	else tex->stress= 0.0f;
}

static void shade_input_vlr_texco_orco(ShadeTexco *tex, ShadeGeometry *geom, ShadePrimitive *prim)
{
	ObjectInstanceRen *obi= prim->obi;
	ObjectRen *obr= prim->obr;
	VertRen *v1= prim->v1, *v2= prim->v2, *v3= prim->v3;
	float u= geom->u, v= geom->v;
	float l= 1.0f+u+v, dl;

	if(render_vert_get_orco(obr, v1, 0)) {
		float *o1, *o2, *o3;
		
		o1= render_vert_get_orco(obr, v1, 0);
		o2= render_vert_get_orco(obr, v2, 0);
		o3= render_vert_get_orco(obr, v3, 0);
		
		tex->lo[0]= l*o3[0]-u*o1[0]-v*o2[0];
		tex->lo[1]= l*o3[1]-u*o1[1]-v*o2[1];
		tex->lo[2]= l*o3[2]-u*o1[2]-v*o2[2];
		
		if(geom->osatex) {
			dl= geom->dx_u+geom->dx_v;
			tex->dxlo[0]= dl*o3[0]-geom->dx_u*o1[0]-geom->dx_v*o2[0];
			tex->dxlo[1]= dl*o3[1]-geom->dx_u*o1[1]-geom->dx_v*o2[1];
			tex->dxlo[2]= dl*o3[2]-geom->dx_u*o1[2]-geom->dx_v*o2[2];
			dl= geom->dy_u+geom->dy_v;
			tex->dylo[0]= dl*o3[0]-geom->dy_u*o1[0]-geom->dy_v*o2[0];
			tex->dylo[1]= dl*o3[1]-geom->dy_u*o1[1]-geom->dy_v*o2[1];
			tex->dylo[2]= dl*o3[2]-geom->dy_u*o1[2]-geom->dy_v*o2[2];
		}
	}

	copy_v3_v3(tex->duplilo, obi->dupliorco);
}

static void shade_input_vlr_texco_tangent(Render *re, ShadeTexco *tex, ShadeGeometry *geom, ShadePrimitive *prim, int mode)
{
	ObjectRen *obr= prim->obr;
	ObjectInstanceRen *obi= prim->obi;
	VertRen *v1= prim->v1, *v2= prim->v2, *v3= prim->v3;
	float u= geom->u, v= geom->v;
	float l= 1.0f+u+v;
	float *tangent, *s1, *s2, *s3;
	float tl, tu, tv;

	if(prim->vlr->flag & R_SMOOTH) {
		tl= l;
		tu= u;
		tv= v;
	}
	else {
		/* qdn: flat faces have tangents too,
		   could pick either one, using average here */
		tl= 1.0f/3.0f;
		tu= -1.0f/3.0f;
		tv= -1.0f/3.0f;
	}

	geom->tang[0]= geom->tang[1]= geom->tang[2]= 0.0f;
	tex->nmaptang[0]= tex->nmaptang[1]= tex->nmaptang[2]= 0.0f;

	if(mode & MA_TANGENT_V) {
		s1 = render_vert_get_tangent(obr, v1, 0);
		s2 = render_vert_get_tangent(obr, v2, 0);
		s3 = render_vert_get_tangent(obr, v3, 0);

		if(s1 && s2 && s3) {
			geom->tang[0]= (tl*s3[0] - tu*s1[0] - tv*s2[0]);
			geom->tang[1]= (tl*s3[1] - tu*s1[1] - tv*s2[1]);
			geom->tang[2]= (tl*s3[2] - tu*s1[2] - tv*s2[2]);

			if(obi->flag & R_TRANSFORMED)
				mul_m3_v3(obi->nmat, geom->tang);

			normalize_v3(geom->tang);
			copy_v3_v3(tex->nmaptang, geom->tang);
		}
	}

	if(mode & MA_NORMAP_TANG || re->params.flag & R_NEED_TANGENT) {
		tangent= render_vlak_get_nmap_tangent(obr, prim->vlr, 0);

		if(tangent) {
			int j1= prim->i1, j2= prim->i2, j3= prim->i3;

			vlr_set_uv_indices(prim->vlr, &j1, &j2, &j3);

			s1= &tangent[j1*3];
			s2= &tangent[j2*3];
			s3= &tangent[j3*3];

			tex->nmaptang[0]= (tl*s3[0] - tu*s1[0] - tv*s2[0]);
			tex->nmaptang[1]= (tl*s3[1] - tu*s1[1] - tv*s2[1]);
			tex->nmaptang[2]= (tl*s3[2] - tu*s1[2] - tv*s2[2]);

			if(obi->flag & R_TRANSFORMED)
				mul_m3_v3(obi->nmat, tex->nmaptang);

			normalize_v3(tex->nmaptang);
		}
	}
}

static void shade_input_vlr_texco_speed(ShadeTexco *tex, ShadeGeometry *geom, ShadePrimitive *prim)
{
	ObjectInstanceRen *obi= prim->obi;
	VertRen *v1= prim->v1, *v2= prim->v2, *v3= prim->v3;
	float *s1, *s2, *s3;
	float u= geom->u, v= geom->v;
	float l= 1.0f+u+v;
	
	s1= render_vert_get_winspeed(obi, v1, 0);
	s2= render_vert_get_winspeed(obi, v2, 0);
	s3= render_vert_get_winspeed(obi, v3, 0);
	if(s1 && s2 && s3) {
		tex->winspeed[0]= (l*s3[0] - u*s1[0] - v*s2[0]);
		tex->winspeed[1]= (l*s3[1] - u*s1[1] - v*s2[1]);
		tex->winspeed[2]= (l*s3[2] - u*s1[2] - v*s2[2]);
		tex->winspeed[3]= (l*s3[3] - u*s1[3] - v*s2[3]);
	}
	else {
		tex->winspeed[0]= tex->winspeed[1]= tex->winspeed[2]= tex->winspeed[3]= 0.0f;
	}
}

static void shade_input_vlr_texco_surface(ShadeGeometry *geom, ShadePrimitive *prim)
{
	ObjectRen *obr= prim->obr;
	ObjectInstanceRen *obi= prim->obi;
	float *surfnor= render_vlak_get_surfnor(obr, prim->vlr, 0);

	if(surfnor) {
		copy_v3_v3(geom->surfnor, surfnor);
		if(obi->flag & R_TRANSFORMED)
			mul_m3_v3(obi->nmat, geom->surfnor);
	}
	else
		copy_v3_v3(geom->surfnor, geom->vn);

	geom->surfdist= 0.0f;
}

static void shade_input_vlr_texco_global(RenderCamera *cam, ShadeTexco *tex, ShadeGeometry *geom)
{
	copy_v3_v3(tex->gl, geom->co);
	mul_m4_v3(cam->viewinv, tex->gl);

	if(geom->osatex) {
		copy_v3_v3(tex->dxgl, geom->dxco);
		mul_mat3_m4_v3(cam->viewinv, geom->dxco);

		copy_v3_v3(tex->dygl, geom->dyco);
		mul_mat3_m4_v3(cam->viewinv, geom->dyco);
	}
}

static void shade_input_vlr_texco_strand(ShadeTexco *tex, ShadeGeometry *geom, ShadePrimitive *prim)
{
	VertRen *v1= prim->v1, *v2= prim->v2, *v3= prim->v3;
	float u= geom->u, v= geom->v, l= 1.0f+u+v, dl;

	tex->strandco= (l*v3->accum - u*v1->accum - v*v2->accum);

	if(geom->osatex) {
		dl= geom->dx_u+geom->dx_v;
		tex->dxstrand= dl*v3->accum-geom->dx_u*v1->accum-geom->dx_v*v2->accum;

		dl= geom->dy_u+geom->dy_v;
		tex->dystrand= dl*v3->accum-geom->dy_u*v1->accum-geom->dy_v*v2->accum;
	}
}

static void shade_input_vlr_texco_uvcol(Render *re, ShadeInput *shi, ShadeTexco *tex, ShadeGeometry *geom, ShadePrimitive *prim)
{
	ObjectInstanceRen *obi= prim->obi;
	ObjectRen *obr= prim->obr;
	VlakRen *vlr= prim->vlr;
	MTFace *tface;
	MCol *mcol;
	ShadeMaterial *mat= &shi->material;
	char *name;
	int i, j1=prim->i1, j2=prim->i2, j3=prim->i3;
	int mode= shi->material.mode;
	float u= geom->u, v= geom->v, l= 1.0f+u+v, dl;

	/* uv and vcols are not copied on split, so set them according vlr divide flag */
	vlr_set_uv_indices(vlr, &j1, &j2, &j3);

	tex->totuv= 0;
	tex->totcol= 0;
	tex->actuv= obr->actmtface;
	tex->actcol= obr->actmcol;

	if(mode & (MA_VERTEXCOL|MA_VERTEXCOLP)) {
		for (i=0; (mcol=render_vlak_get_mcol(obr, vlr, i, &name, 0)); i++) {
			ShadeInputCol *scol= &tex->col[i];
			char *cp1, *cp2, *cp3;
			
			tex->totcol++;
			scol->name= name;

			cp1= (char *)(mcol+j1);
			cp2= (char *)(mcol+j2);
			cp3= (char *)(mcol+j3);
			
			scol->col[0]= (l*((float)cp3[3]) - u*((float)cp1[3]) - v*((float)cp2[3]))/255.0f;
			scol->col[1]= (l*((float)cp3[2]) - u*((float)cp1[2]) - v*((float)cp2[2]))/255.0f;
			scol->col[2]= (l*((float)cp3[1]) - u*((float)cp1[1]) - v*((float)cp2[1]))/255.0f;
		}

		if(tex->totcol) {
			mat->vcol[0]= tex->col[tex->actcol].col[0];
			mat->vcol[1]= tex->col[tex->actcol].col[1];
			mat->vcol[2]= tex->col[tex->actcol].col[2];
			mat->vcol[3]= 1.0f;
		}
		else {
			mat->vcol[0]= 0.0f;
			mat->vcol[1]= 0.0f;
			mat->vcol[2]= 0.0f;
			mat->vcol[3]= 1.0f;
		}
	}

	for (i=0; (tface=render_vlak_get_tface(obr, vlr, i, &name, 0)); i++) {
		ShadeInputUV *suv= &tex->uv[i];
		float *uv1, *uv2, *uv3;

		tex->totuv++;
		suv->name= name;
		
		uv1= tface->uv[j1];
		uv2= tface->uv[j2];
		uv3= tface->uv[j3];
		
		suv->uv[0]= -1.0f + 2.0f*(l*uv3[0]-u*uv1[0]-v*uv2[0]);
		suv->uv[1]= -1.0f + 2.0f*(l*uv3[1]-u*uv1[1]-v*uv2[1]);
		suv->uv[2]= 0.0f;	/* texture.c assumes there are 3 coords */

		if(geom->osatex) {
			float duv[2];
			
			dl= geom->dx_u+geom->dx_v;
			duv[0]= geom->dx_u; 
			duv[1]= geom->dx_v;
			
			suv->dxuv[0]= 2.0f*(dl*uv3[0]-duv[0]*uv1[0]-duv[1]*uv2[0]);
			suv->dxuv[1]= 2.0f*(dl*uv3[1]-duv[0]*uv1[1]-duv[1]*uv2[1]);
			
			dl= geom->dy_u+geom->dy_v;
			duv[0]= geom->dy_u; 
			duv[1]= geom->dy_v;
			
			suv->dyuv[0]= 2.0f*(dl*uv3[0]-duv[0]*uv1[0]-duv[1]*uv2[0]);
			suv->dyuv[1]= 2.0f*(dl*uv3[1]-duv[0]*uv1[1]-duv[1]*uv2[1]);
		}

		if((mode & MA_FACETEXTURE) && i==obr->actmtface) {
			if((mode & (MA_VERTEXCOL|MA_VERTEXCOLP))==0) {
				mat->vcol[0]= 1.0f;
				mat->vcol[1]= 1.0f;
				mat->vcol[2]= 1.0f;
				mat->vcol[3]= 1.0f;
			}
			if(tface && tface->tpage)
				do_realtime_texture(&re->params, shi, tface->tpage);
		}
	}

	tex->dupliuv[0]= -1.0f + 2.0f*obi->dupliuv[0];
	tex->dupliuv[1]= -1.0f + 2.0f*obi->dupliuv[1];
	tex->dupliuv[2]= 0.0f;

	if(tex->totuv == 0) {
		ShadeInputUV *suv= &tex->uv[0];

		suv->uv[0]= 2.0f*(u+.5f);
		suv->uv[1]= 2.0f*(v+.5f);
		suv->uv[2]= 0.0f;	/* texture.c assumes there are 3 coords */
		
		if(mode & MA_FACETEXTURE) {
			/* no tface? set at 1.0f */
			mat->vcol[0]= 1.0f;
			mat->vcol[1]= 1.0f;
			mat->vcol[2]= 1.0f;
			mat->vcol[3]= 1.0f;
		}
	}
}

void shade_input_set_shade_texco(Render *re, ShadeInput *shi)
{
	ShadePrimitive *prim= &shi->primitive;
	ShadeTexco *tex= &shi->texture;
	ShadeGeometry *geom= &shi->geometry;
	ShadeMaterial *mat= &shi->material;
	int mode= shi->material.mode;		/* or-ed result for all nodes */
	short texco= shi->material.mat->texco;

	/* normal */
	if((texco & (TEXCO_NORM|TEXCO_REFL)))
		shade_input_vlr_texco_normal(geom, prim);

	/* tangents */
	if(mode & (MA_TANGENT_V|MA_NORMAP_TANG) || re->params.flag & R_NEED_TANGENT)
		shade_input_vlr_texco_tangent(re, tex, geom, prim, mode);

	/* surface normal */
	if(mode & MA_STR_SURFDIFF)
		shade_input_vlr_texco_surface(geom, prim);
	
	/* speed */
	if(re->params.r.mode & R_SPEED)
		shade_input_vlr_texco_speed(tex, geom, prim);

	/* pass option forces UV calc */
	if(shi->shading.passflag & SCE_PASS_UV)
		texco |= (NEED_UV|TEXCO_UV);
	
	/* texture coordinates. tex->dxuv tex->dyuv have been set */
	if(texco & NEED_UV) {
		
		if(texco & TEXCO_ORCO)
			shade_input_vlr_texco_orco(tex, geom, prim);
		
		if(texco & TEXCO_GLOB)
			shade_input_vlr_texco_global(&re->cam, tex, geom);
		
		if(texco & TEXCO_STRAND)
			shade_input_vlr_texco_strand(tex, geom, prim);
				
		if((texco & TEXCO_UV) || (mode & (MA_VERTEXCOL|MA_VERTEXCOLP|MA_FACETEXTURE)))
			shade_input_vlr_texco_uvcol(re, shi, tex, geom, prim);
		
		if(texco & TEXCO_NORM)
			negate_v3_v3(tex->orn, geom->vn);
		
		if(texco & TEXCO_REFL) {
			/* mirror reflection color textures (and envmap) */
			shade_input_calc_reflection(shi);	/* wrong location for normal maps! XXXXXXXXXXXXXX */
		}
		
		if(texco & TEXCO_STRESS)
			shade_input_vlr_texco_stress(tex, geom, prim);
		
		if(texco & TEXCO_TANGENT) {
			if((mode & MA_TANGENT_V)==0) {
				/* just prevent surprises */
				zero_v3(geom->tang);
				zero_v3(tex->nmaptang);
			}
		}
	}
	
	/* this only avalailable for scanline renders */
	if(shi->shading.depth==0) {
		if(texco & TEXCO_WINDOW)
			shade_input_vlr_texco_window(&re->cam, tex, geom);

		if(texco & TEXCO_STICKY)
			shade_input_vlr_texco_sticky(re, tex, geom, prim);
	}
	else {
		/* Note! For raytracing winco is not set, important because this means
		   all shader input's need to have their variables set to zero else
		   un-initialized values are used */
	}

	if(re->params.r.color_mgt_flag & R_COLOR_MANAGEMENT)
		if(mode & (MA_VERTEXCOL|MA_VERTEXCOLP|MA_FACETEXTURE))
			srgb_to_linearrgb_v3_v3(mat->vcol, mat->vcol);
}

/* --------------------------------------------- */
/* also called from texture.c */
void shade_input_calc_reflection(ShadeInput *shi)
{
	ShadeGeometry *geom= &shi->geometry;
	ShadeTexco *texco= &shi->texture;

	reflect_v3_v3v3(texco->ref, geom->view, geom->vn);

	if(geom->osatex) {
		float dview[3], dno[3], ref[3];

		/* dxref */
		if(shi->primitive.vlr->flag & R_SMOOTH)
			add_v3_v3v3(dno, geom->vn, geom->dxno);
		else
			copy_v3_v3(dno, geom->vn);

		dview[0]= geom->view[0] + geom->dxview;
		dview[1]= geom->view[1];
		dview[2]= geom->view[2];

		reflect_v3_v3v3(ref, dview, dno);
		sub_v3_v3v3(texco->dxref, texco->ref, ref);

		/* dyref */
		if(shi->primitive.vlr->flag & R_SMOOTH)
			add_v3_v3v3(dno, geom->vn, geom->dyno);
		else
			copy_v3_v3(dno, geom->vn);

		dview[0]= geom->view[0];
		dview[1]= geom->view[1] + geom->dyview;
		dview[2]= geom->view[2];

		reflect_v3_v3v3(ref, dview, dno);
		sub_v3_v3v3(texco->dyref, texco->ref, ref);
	}
}

/******************************* Initialization ******************************/

/* initialize per part, not per pixel! */
static void shade_input_initialize(Render *re, ShadeInput *shi, RenderPart *pa, RenderLayer *rl, int sample)
{
	
	memset(shi, 0, sizeof(ShadeInput));
	
	shi->shading.sample= sample;
	shi->shading.thread= pa->thread;
	shi->shading.pa= pa;
	shi->shading.do_preview= (re->params.r.scemode & R_MATNODE_PREVIEW) != 0;
	shi->shading.lay= rl->lay;
	shi->shading.layflag= rl->layflag;
	shi->shading.passflag= rl->passflag;
	shi->shading.combinedflag= ~rl->pass_xor;
	shi->material.mat_override= rl->mat_override;
	shi->material.light_override= rl->light_override;
//	shi->material.rl= rl;
	/* note shi.depth==0  means first hit, not raytracing */
}

/* initialize per part, not per pixel! */
void shade_sample_initialize(Render *re, ShadeSample *ssamp, RenderPart *pa, RenderLayer *rl)
{
	int a, tot;
	
	tot= re->params.osa==0?1:re->params.osa;
	
	for(a=0; a<tot; a++) {
		shade_input_initialize(re, &ssamp->shi[a], pa, rl, a);
		memset(&ssamp->shr[a], 0, sizeof(ShadeResult));
	}
}

/**************************** ShadeInput from Pixstr *************************/

static void shade_input_from_pixel(Render *re, ShadeInput *shi, int x, int y, int z, short samp, unsigned short mask)
{
	float xs, ys, ofs[2];

	if(shi->primitive.vlr->flag & R_FULL_OSA) {
		/* zbuffer has this inverse corrected, ensures xs,ys are inside pixel */
		pxf_sample_offset(&re->sample, samp, ofs);
	}
	else {
		/* averaged offset for mask */
		pxf_mask_offset(&re->sample, mask, ofs);
	}

	xs= (float)x + ofs[0];
	ys= (float)y + ofs[1];

	shi->shading.mask= mask;
	shi->shading.samplenr= re->sample.shadowsamplenr[shi->shading.thread]++;
	shade_input_set_viewco(re, shi, x, y, xs, ys, (float)z);
	shade_input_set_uv(shi);
	shade_input_set_normals(shi);
}

static int shade_inputs_from_pixel(Render *re, ShadeInput *shi, PixelRow *row, int x, int y)
{
	ShadePrimitive *prim= &shi->primitive;
	unsigned short mask= row->mask;
	int tot= 0;

	if(row->p > 0) {
		prim->obi= part_get_instance(shi->shading.pa, &re->db.objectinstance[row->obi]);
		prim->obr= prim->obi->obr;
		prim->facenr= (row->p-1) & RE_QUAD_MASK;

		if(prim->facenr < prim->obr->totvlak) {
			VlakRen *vlr= render_object_vlak_get(prim->obr, prim->facenr);
			
			if(row->p & RE_QUAD_OFFS)
				shade_input_set_triangle_i(re, shi, prim->obi, vlr, 0, 2, 3);
			else
				shade_input_set_triangle_i(re, shi, prim->obi, vlr, 0, 1, 2);
		}
		else {
			prim->vlr= NULL;
			return 0;
		}
	}
	else {
		prim->vlr= NULL;
		return 0;
	}

	/* full osa is only set for OSA renders */
	if(prim->vlr->flag & R_FULL_OSA) {
		short samp;
		
		for(samp=0; samp<re->params.osa; samp++) {
			if(mask & (1<<samp)) {
				if(tot)
					shade_input_copy_triangle(shi, shi-1);
				
				shade_input_from_pixel(re, shi, x, y, row->z, samp, (1<<samp));
				shi++;
				tot++;
			}
		}
	}
	else {
		shade_input_from_pixel(re, shi, x, y, row->z, 0, mask);
		shi++;
		tot++;
	}

	return tot;
}

void shade_samples_from_pixel(Render *re, ShadeSample *ssamp, PixelRow *row, int x, int y)
{
	ssamp->tot= shade_inputs_from_pixel(re, ssamp->shi, row, x, y);
}

/********************************** Shading **********************************/

/* shades samples, returns true if anything happened */
void shade_samples(Render *re, ShadeSample *ssamp)
{
	ShadeInput *shi= ssamp->shi;
	ShadeResult *shr= ssamp->shr;
	int samp;

	if(!ssamp->tot)
		return;
	
	/* if shade (all shadepinputs have same passflag) */
	if(shi->shading.passflag & ~(SCE_PASS_Z|SCE_PASS_INDEXOB)) {
		for(samp=0; samp<ssamp->tot; samp++, shi++, shr++) {
			shade_input_set_shade_texco(re, shi);
			shade_input_do_shade(re, shi, shr);
		}
	}
	else if(shi->shading.passflag & SCE_PASS_Z) {
		for(samp=0; samp<ssamp->tot; samp++, shi++, shr++)
			shr->z= -shi->geometry.co[2];
	}
}

/* also used as callback for nodes */
/* delivers a fully filled in ShadeResult, for all passes */
void shade_material_loop(Render *re, ShadeInput *shi, ShadeResult *shr)
{
	shade_surface(re, shi, shr, 0); /* clears shr */
	
	if(shi->material.translucency!=0.0f) {
		ShadeResult shr_t;
		float fac= shi->material.translucency;
		
		shade_input_init_material(re, shi);

		copy_v3_v3(shi->geometry.vn, shi->geometry.vno);
		negate_v3(shi->geometry.vn);
		negate_v3(shi->geometry.facenor);
		shi->shading.depth++;	/* hack to get real shadow now */

		shade_surface(re, shi, &shr_t, 1);

		shi->shading.depth--;
		negate_v3(shi->geometry.vn);
		negate_v3(shi->geometry.facenor);

		/* a couple of passes */
		madd_v3_v3fl(shr->combined, shr_t.combined, fac);
		if(shi->shading.passflag & SCE_PASS_SPEC)
			madd_v3_v3fl(shr->spec, shr_t.spec, fac);
		if(shi->shading.passflag & SCE_PASS_DIFFUSE)
			madd_v3_v3fl(shr->diff, shr_t.diff, fac);
		if(shi->shading.passflag & SCE_PASS_SHADOW)
			madd_v3_v3fl(shr->shad, shr_t.shad, fac);
	}

	/* depth >= 1 when ray-shading */
	if(shi->shading.depth==0) {
		/* disable adding of sky for raytransp */
		if((shi->material.mat->mode & MA_TRANSP) && (shi->material.mat->mode & MA_RAYTRANSP))
			if((shi->shading.layflag & SCE_LAY_SKY) && (re->params.r.alphamode==R_ADDSKY))
				shr->alpha= 1.0f;
	}	

	if(re->params.r.mode & R_RAYTRACE) {
		if (re->db.render_volumes_inside.first)
			shade_volume_inside(re, shi, shr);
	}
}

static void shade_mist(Render *re, ShadeInput *shi, ShadeResult *shr)
{
	float mist= 1.0f;

	if((shi->shading.passflag & SCE_PASS_MIST) || ((re->db.wrld.mode & WO_MIST) && (shi->material.mat->mode & MA_NOMIST)==0))  {
		if(re->cam.type == R_CAM_ORTHO)
			shr->mist= environment_mist_factor(re, -shi->geometry.co[2], shi->geometry.co);
		else
			shr->mist= environment_mist_factor(re, len_v3(shi->geometry.co), shi->geometry.co);
	}
	else shr->mist= 0.0f;

	if((re->db.wrld.mode & WO_MIST) && (shi->material.mat->mode & MA_NOMIST)==0 ) {
		mist= shr->mist;

		if(mist != 1.0f)
			if(shi->material.mat->material_type!= MA_TYPE_VOLUME)
				mul_v3_fl(shr->combined, mist);
	}

	shr->combined[3]= shr->alpha*mist;
}

/* do a shade, finish up some passes, apply mist */
void shade_input_do_shade(Render *re, ShadeInput *shi, ShadeResult *shr)
{
	/* ------  main shading loop -------- */
#ifdef RE_RAYCOUNTER
	memset(&shi->shading.raycounter, 0, sizeof(shi->shading.raycounter));
#endif
	
	if(shi->material.mat->nodetree && shi->material.mat->use_nodes) {
		ntreeShaderExecTree(shi->material.mat->nodetree, re, shi, shr);
	}
	else {
		/* copy all relevant material vars, note, keep this synced with render_types.h */
		shade_input_init_material(re, shi);
		
		if (shi->material.mat->material_type == MA_TYPE_VOLUME) {
			if(re->params.r.mode & R_RAYTRACE) {
				shade_volume_outside(re, shi, shr);
				shr->combined[3]= shr->alpha;
			}
		} else { /* MA_TYPE_SURFACE, MA_TYPE_WIRE */
			shade_material_loop(re, shi, shr);
		}
	}
	
	/* vector, normal, indexob pass */
	if(shi->shading.passflag & (SCE_PASS_VECTOR|SCE_PASS_NORMAL|SCE_PASS_INDEXOB)) {
		copy_v4_v4(shr->winspeed, shi->texture.winspeed);
		copy_v3_v3(shr->nor, shi->geometry.vn);
		shr->indexob= shi->primitive.obr->ob->index;
	}

	/* uv pass */
	if(shi->shading.passflag & SCE_PASS_UV) {
		if(shi->texture.totuv) {
			shr->uv[0]= 0.5f + 0.5f*shi->texture.uv[shi->texture.actuv].uv[0];
			shr->uv[1]= 0.5f + 0.5f*shi->texture.uv[shi->texture.actuv].uv[1];
			shr->uv[2]= 1.0f;
		}
	}
	
	/* mist pass */
	shade_mist(re, shi, shr);
	
	/* z pass */
	shr->z= -shi->geometry.co[2];
	
	/* RAYHITS */
/*
	if(1 || shi->shading.passflag & SCE_PASS_RAYHITS)
	{
		shr->rayhits[0] = (float)shi->shading.raycounter.faces.test;
		shr->rayhits[1] = (float)shi->shading.raycounter.bb.hit;
		shr->rayhits[2] = 0.0;
		shr->rayhits[3] = 1.0;
	}
 */
	RE_RC_MERGE(&re_rc_counter[shi->shading.thread], &shi->shading.raycounter);
}


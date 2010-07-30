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

#include "DNA_camera_types.h"
#include "DNA_curve_types.h"
#include "DNA_group_types.h"
#include "DNA_lamp_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"

#include "BKE_colortools.h"
#include "BKE_group.h"
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
	memcpy(&shi->r, &shi->mat->r, 21*sizeof(float));
	shi->har= shi->mat->har;
	shi->sss_scale= shi->mat->sss_scale;
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

static int shade_input_override_material(ShadeInput *shi)
{
	if(shi->mat_override)
		if(!shi->except_override || !object_in_group(shi->obr->ob, shi->except_override))
			return 1;
	
	return 0;
}

/* copy data from face to ShadeInput, general case */
/* indices 0 1 2 3 only */
void shade_input_set_triangle_i(Render *re, ShadeInput *shi, ObjectInstanceRen *obi, VlakRen *vlr, short i1, short i2, short i3)
{
	VertRen **vpp= &vlr->v1;
	
	shi->vlr= vlr;
	shi->obi= obi;
	shi->obr= obi->obr;

	shi->v1= vpp[i1];
	shi->v2= vpp[i2];
	shi->v3= vpp[i3];
	
	shi->i1= i1;
	shi->i2= i2;
	shi->i3= i3;
	
	/* note, mat->mat is set in node shaders */
	if(shade_input_override_material(shi))
		shi->mat= shi->mat_override;
	else
		shi->mat= vlr->mat;
	shi->mode= shi->mat->mode_l;		/* or-ed result for all nodes */

	shi->osatex= (shi->mat->texco & TEXCO_OSA);
}

/* copy data from strand to shadeinput */
void shade_input_set_strand(Render *re, ShadeInput *shi, StrandRen *strand, StrandPoint *spoint)
{
	/* note, mat->mat is set in node shaders */
	if(shade_input_override_material(shi))
		shi->mat= shi->mat_override;
	else
		shi->mat= strand->buffer->ma;

	shi->osatex= (shi->mat->texco & TEXCO_OSA);
	shi->mode= shi->mat->mode_l;		/* or-ed result for all nodes */

	/* shade_input_set_viewco equivalent */
	copy_v3_v3(shi->co, spoint->co);
	copy_v3_v3(shi->view, shi->co);
	normalize_v3(shi->view);

	shi->xs= (int)spoint->x;
	shi->ys= (int)spoint->y;

	if(shi->osatex || (re->r.mode & R_SHADOW)) {
		copy_v3_v3(shi->dxco, spoint->dtco);
		copy_v3_v3(shi->dyco, spoint->dsco);
	}

	/* dxview, dyview, not supported */

	/* facenormal, simply viewco flipped */
	copy_v3_v3(shi->facenor, spoint->nor);

	/* shade_input_set_normals equivalent */
	if(shi->mat->mode & MA_TANGENT_STR) {
		copy_v3_v3(shi->vn, spoint->tan);
	}
	else {
		float cross[3];

		cross_v3_v3v3(cross, spoint->co, spoint->tan);
		cross_v3_v3v3(shi->vn, cross, spoint->tan);
		normalize_v3(shi->vn);

		if(dot_v3v3(shi->vn, shi->view) < 0.0f)
			negate_v3(shi->vn);
	}

	copy_v3_v3(shi->vno, shi->vn);

	shi->tangentvn= (shi->mat->mode & MA_TANGENT_STR) != 0;
}

void shade_input_set_strand_texco(Render *re, ShadeInput *shi, StrandRen *strand, StrandVert *svert, StrandPoint *spoint)
{
	StrandBuffer *strandbuf= strand->buffer;
	ObjectRen *obr= strandbuf->obr;
	StrandVert *sv;
	int mode= shi->mode;		/* or-ed result for all nodes */
	short texco= shi->mat->texco;

	if((shi->mat->texco & TEXCO_REFL)) {
		/* shi->dxview, shi->dyview, not supported */
	}

	if(shi->osatex && (texco & (TEXCO_NORM|TEXCO_REFL))) {
		/* not supported */
	}

	if(mode & (MA_TANGENT_V|MA_NORMAP_TANG)) {
		copy_v3_v3(shi->tang, spoint->tan);
		copy_v3_v3(shi->nmaptang, spoint->tan);
	}

	if(mode & MA_STR_SURFDIFF) {
		float *surfnor= render_strand_get_surfnor(obr, strand, 0);

		if(surfnor)
			copy_v3_v3(shi->surfnor, surfnor);
		else
			copy_v3_v3(shi->surfnor, shi->vn);

		if(shi->mat->strand_surfnor > 0.0f) {
			shi->surfdist= 0.0f;
			for(sv=strand->vert; sv!=svert; sv++)
				shi->surfdist+=len_v3v3(sv->co, (sv+1)->co);
			shi->surfdist += spoint->t*len_v3v3(sv->co, (sv+1)->co);
		}
	}

	if(re->r.mode & R_SPEED) {
		float *speed;
		
		speed= render_strand_get_winspeed(shi->obi, strand, 0);
		if(speed)
			copy_v4_v4(shi->winspeed, speed);
		else
			zero_v4(shi->winspeed);
	}

	/* shade_input_set_shade_texco equivalent */
	if(texco & NEED_UV) {
		if(texco & TEXCO_ORCO) {
			copy_v3_v3(shi->lo, render_strand_get_orco(obr, strand, 0));
			/* no shi->osatex, orco derivatives are zero */
		}

		if(texco & TEXCO_GLOB) {
			copy_v3_v3(shi->gl, shi->co);
			mul_m4_v3(re->cam.viewinv, shi->gl);
			
			if(shi->osatex) {
				copy_v3_v3(shi->dxgl, shi->dxco);
				mul_mat3_m4_v3(re->cam.viewinv, shi->dxco);
				copy_v3_v3(shi->dygl, shi->dyco);
				mul_mat3_m4_v3(re->cam.viewinv, shi->dyco);
			}
		}

		if(texco & TEXCO_STRAND) {
			shi->strandco= spoint->strandco;

			if(shi->osatex) {
				shi->dxstrand= spoint->dtstrandco;
				shi->dystrand= 0.0f;
			}
		}

		if((texco & TEXCO_UV) || (mode & (MA_VERTEXCOL|MA_VERTEXCOLP|MA_FACETEXTURE)))  {
			MCol *mcol;
			float *uv;
			char *name;
			int i;

			shi->totuv= 0;
			shi->totcol= 0;
			shi->actuv= obr->actmtface;
			shi->actcol= obr->actmcol;

			if(mode & (MA_VERTEXCOL|MA_VERTEXCOLP)) {
				for (i=0; (mcol=render_strand_get_mcol(obr, strand, i, &name, 0)); i++) {
					ShadeInputCol *scol= &shi->col[i];
					char *cp= (char*)mcol;
					
					shi->totcol++;
					scol->name= name;

					scol->col[0]= cp[3]/255.0f;
					scol->col[1]= cp[2]/255.0f;
					scol->col[2]= cp[1]/255.0f;
				}

				if(shi->totcol) {
					shi->vcol[0]= shi->col[shi->actcol].col[0];
					shi->vcol[1]= shi->col[shi->actcol].col[1];
					shi->vcol[2]= shi->col[shi->actcol].col[2];
				}
				else {
					shi->vcol[0]= 0.0f;
					shi->vcol[1]= 0.0f;
					shi->vcol[2]= 0.0f;
				}
			}

			for (i=0; (uv=render_strand_get_uv(obr, strand, i, &name, 0)); i++) {
				ShadeInputUV *suv= &shi->uv[i];

				shi->totuv++;
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

				if(shi->osatex) {
					suv->dxuv[0]= 0.0f;
					suv->dxuv[1]= 0.0f;
					suv->dyuv[0]= 0.0f;
					suv->dyuv[1]= 0.0f;
				}

				if((mode & MA_FACETEXTURE) && i==obr->actmtface) {
					if((mode & (MA_VERTEXCOL|MA_VERTEXCOLP))==0) {
						shi->vcol[0]= 1.0f;
						shi->vcol[1]= 1.0f;
						shi->vcol[2]= 1.0f;
					}
				}
			}

			if(shi->totuv == 0) {
				ShadeInputUV *suv= &shi->uv[0];

				suv->uv[0]= 0.0f;
				suv->uv[1]= spoint->strandco;
				suv->uv[2]= 0.0f;	/* texture.c assumes there are 3 coords */
				
				if(mode & MA_FACETEXTURE) {
					/* no tface? set at 1.0f */
					shi->vcol[0]= 1.0f;
					shi->vcol[1]= 1.0f;
					shi->vcol[2]= 1.0f;
				}
			}

		}

		if(texco & TEXCO_NORM)
			negate_v3_v3(shi->orn, shi->vn);

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
				shi->tang[0]= shi->tang[1]= shi->tang[2]= 0.0f;
				shi->nmaptang[0]= shi->nmaptang[1]= shi->nmaptang[2]= 0.0f;
			}
		}
	}

	/* this only avalailable for scanline renders */
	if(shi->depth==0) {
		if(texco & TEXCO_WINDOW) {
			shi->winco[0]= -1.0f + 2.0f*spoint->x/(float)re->cam.winx;
			shi->winco[1]= -1.0f + 2.0f*spoint->y/(float)re->cam.winy;
			shi->winco[2]= 0.0f;

			/* not supported */
			if(shi->osatex) {
				shi->dxwin[0]= 0.0f;
				shi->dywin[1]= 0.0f;
				shi->dxwin[0]= 0.0f;
				shi->dywin[1]= 0.0f;
			}
		}

		if(texco & TEXCO_STICKY) {
			/* not supported */
		}
	}
	
	if(re->r.color_mgt_flag & R_COLOR_MANAGEMENT)
		if(mode & (MA_VERTEXCOL|MA_VERTEXCOLP|MA_FACETEXTURE))
			srgb_to_linearrgb_v3_v3(shi->vcol, shi->vcol);
}

/* from scanline pixel coordinates to 3d coordinates, requires set_triangle */
void shade_input_calc_viewco(Render *re, ShadeInput *shi, float x, float y, float z, float *view, float *dxyview, float *co, float *dxco, float *dyco)
{

	if(shi->mat->material_type == MA_TYPE_WIRE) {
		/* returns not normalized, so is in viewplane coords */
		camera_raster_to_view(&re->cam, view, x, y);
		normalize_v3(view);

		/* wire cannot use normal for calculating shi->co, so we
		 * reconstruct the coordinate less accurate using z-value */
		camera_raster_to_co(&re->cam, co, x, y, z);

		/* TODO dxco/dyco/dxyview ? */
	}
	else {
		/* for non-wire, intersect with the triangle to get the exact coord */
		float plane[4]; /* plane equation a*x + b*y + c*z = d */
		float v1[3];

		/* compute plane equation */
		copy_v3_v3(v1, shi->v1->co);
		if(shi->obi->flag & R_TRANSFORMED)
			mul_m4_v3(shi->obi->mat, v1);
		
		copy_v3_v3(plane, shi->facenor);
		plane[3]= v1[0]*plane[0] + v1[1]*plane[1] + v1[2]*plane[2];

		/* reconstruct */
		camera_raster_plane_to_co(&re->cam, co, dxco, dyco, view, dxyview, x, y, plane);
	}
	
	/* set camera coords - for scanline, it's always 0.0,0.0,0.0 (render is in camera space)
	 * however for raytrace it can be different - the position of the last intersection */
	/* TODO this is not correct for ortho! */
	shi->camera_co[0] = shi->camera_co[1] = shi->camera_co[2] = 0.0f;
}

/* from scanline pixel coordinates to 3d coordinates, requires set_triangle */
void shade_input_set_viewco(Render *re, ShadeInput *shi, float x, float y, float xs, float ys, float z)
{
	float *dxyview= NULL, *dxco= NULL, *dyco= NULL;
	
	/* currently in use for dithering (soft shadow), node preview, irregular shad */
	shi->xs= (int)xs;
	shi->ys= (int)ys;

	/* original scanline coordinate without jitter */
	shi->scanco[0]= x;
	shi->scanco[1]= y;
	shi->scanco[2]= z;

	/* always compute derivatives */
	dxco= shi->dxco;
	dyco= shi->dyco;

	if((shi->mat->texco & TEXCO_REFL))
		dxyview= &shi->dxview;

	shade_input_calc_viewco(re, shi, xs, ys, z, shi->view, dxyview, shi->co, dxco, dyco);
}

/* calculate U and V, for scanline (silly render face u and v are in range -1 to 0) */
void shade_input_set_uv(ShadeInput *shi)
{
	VlakRen *vlr= shi->vlr;
	
	if((vlr->flag & R_SMOOTH) || (shi->mat->texco & NEED_UV) || (shi->passflag & SCE_PASS_UV)) {
		float v1[3], v2[3], v3[3], u, v, dx_u, dx_v, dy_u, dy_v;

		copy_v3_v3(v1, shi->v1->co);
		copy_v3_v3(v2, shi->v2->co);
		copy_v3_v3(v3, shi->v3->co);

		if(shi->obi->flag & R_TRANSFORMED) {
			mul_m4_v3(shi->obi->mat, v1);
			mul_m4_v3(shi->obi->mat, v2);
			mul_m4_v3(shi->obi->mat, v3);
		}

		/* exception case for wire render of edge */
		if(vlr->v2==vlr->v3) {
			float lend, lenc;
			
			lend= len_v3v3(v2, v1);
			lenc= len_v3v3(shi->co, v1);
			
			if(lend==0.0f) {
				zero_v3(shi->uvw);
			}
			else {
				shi->uvw[0]= (1.0f - lenc/lend);
				shi->uvw[1]= 1.0f - shi->uvw[0];
				shi->uvw[2]= 0.0f;
			}
			
			if(shi->osatex) {
				zero_v3(shi->duvw_dx);
				zero_v3(shi->duvw_dy);
			}
		}
		else {
			/* most of this could become re-used for faces */
			float detsh, t00, t10, t01, t11, xn, yn, zn;
			int axis1, axis2;

			/* find most stable axis to project */
			xn= fabs(shi->facenor[0]);
			yn= fabs(shi->facenor[1]);
			zn= fabs(shi->facenor[2]);

			if(zn>=xn && zn>=yn) { axis1= 0; axis2= 1; }
			else if(yn>=xn && yn>=zn) { axis1= 0; axis2= 2; }
			else { axis1= 1; axis2= 2; }

			/* compute u,v and derivatives */
			t00= v3[axis1]-v1[axis1]; t01= v3[axis2]-v1[axis2];
			t10= v3[axis1]-v2[axis1]; t11= v3[axis2]-v2[axis2];

			detsh= 1.0f/(t00*t11-t10*t01);
			t00*= detsh; t01*=detsh; 
			t10*=detsh; t11*=detsh;

			u= (shi->co[axis1]-v3[axis1])*t11-(shi->co[axis2]-v3[axis2])*t10;
			v= (shi->co[axis2]-v3[axis2])*t00-(shi->co[axis1]-v3[axis1])*t01;

			/* u and v are in range -1 to 0, we allow a little bit extra but not too much, screws up speedvectors */
			CLAMP(u, -2.0f, 1.0f);
			CLAMP(v, -2.0f, 1.0f);

			shi->uvw[0]= -u;
			shi->uvw[1]= -v;
			shi->uvw[2]= 1.0f + u + v;

			if(shi->osatex) {
				dx_u=  shi->dxco[axis1]*t11- shi->dxco[axis2]*t10;
				dx_v=  shi->dxco[axis2]*t00- shi->dxco[axis1]*t01;
				dy_u=  shi->dyco[axis1]*t11- shi->dyco[axis2]*t10;
				dy_v=  shi->dyco[axis2]*t00- shi->dyco[axis1]*t01;

				shi->duvw_dx[0]= -dx_u;
				shi->duvw_dx[1]= -dx_v;
				shi->duvw_dx[2]= dx_u + dx_v;

				shi->duvw_dy[0]= -dy_u;
				shi->duvw_dy[1]= -dy_v;
				shi->duvw_dy[2]= dy_u + dy_v;
			}
		}
	}	
}

void shade_input_set_normals(ShadeInput *shi)
{
	ObjectInstanceRen *obi= shi->obi;
	VlakRen *vlr= shi->vlr;

	/* calculate vertexnormals */
	if(vlr->flag & R_SMOOTH) {
		copy_v3_v3(shi->n1, shi->v1->n);
		copy_v3_v3(shi->n2, shi->v2->n);
		copy_v3_v3(shi->n3, shi->v3->n);

		if(obi->flag & R_TRANSFORMED) {
			mul_m3_v3(obi->nmat, shi->n1);
			mul_m3_v3(obi->nmat, shi->n2);
			mul_m3_v3(obi->nmat, shi->n3);
		}
	}
	
	/* calculate vertexnormals */
	if(shi->vlr->flag & R_SMOOTH) {
		interp_v3_v3v3v3(shi->vn, shi->n1, shi->n2, shi->n3, shi->uvw);
		normalize_v3(shi->vn);
	}
	else
		copy_v3_v3(shi->vn, shi->facenor);

	/* used in nodes */
	copy_v3_v3(shi->vno, shi->vn);

	shi->tangentvn= (shi->vlr->flag & R_TANGENT);

	/* flip normals to viewing direction */
	if(!shi->tangentvn)
		if(dot_v3v3(shi->facenor, shi->view) < 0.0f)
			shade_input_flip_normals(shi);
}

/* use by raytrace, sss, bake to flip into the right direction */
void shade_input_flip_normals(ShadeInput *shi)
{
	negate_v3(shi->facenor);
	negate_v3(shi->vn);
	negate_v3(shi->vno);

	shi->flippednor= !shi->flippednor;
}

static void shade_input_vlr_texco_normal(ShadeInput *shi)
{
	/* normal already set as default, just do derivatives here */

	if(shi->osatex) {
		if(shi->vlr->flag & R_SMOOTH) {
			interp_v3_v3v3v3(shi->dxno, shi->n1, shi->n2, shi->n3, shi->duvw_dx);
			interp_v3_v3v3v3(shi->dyno, shi->n1, shi->n2, shi->n3, shi->duvw_dy);
		}
		else {
			/* constant normal over face, zero derivatives */
			zero_v3(shi->dxno);
			zero_v3(shi->dyno);
		}
	}
}

static void shade_input_vlr_texco_window(RenderCamera *cam, ShadeInput *shi)
{
	float x= shi->xs;
	float y= shi->ys;
	
	shi->winco[0]= -1.0f + 2.0f*x/(float)cam->winx;
	shi->winco[1]= -1.0f + 2.0f*y/(float)cam->winy;
	shi->winco[2]= 0.0f;

	if(shi->osatex) {
		shi->dxwin[0]= 2.0f/(float)cam->winx;
		shi->dywin[1]= 2.0f/(float)cam->winy;
		shi->dxwin[1]= shi->dxwin[2]= 0.0f;
		shi->dywin[0]= shi->dywin[2]= 0.0f;
	}
}

static void shade_input_vlr_texco_sticky(Render *re, ShadeInput *shi)
{
	ObjectInstanceRen *obi= shi->obi;
	ObjectRen *obr= shi->obr;
	VertRen *v1= shi->v1, *v2= shi->v2, *v3= shi->v3;
	float x= shi->xs;
	float y= shi->ys;
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

		if(shi->obi->flag & R_TRANSFORMED)
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
		
		shi->sticky[0]= l*s3[0]-u*s1[0]-v*s2[0];
		shi->sticky[1]= l*s3[1]-u*s1[1]-v*s2[1];
		shi->sticky[2]= 0.0f;
		
		if(shi->osatex) {
			float dxuv[2], dyuv[2];
			dxuv[0]=  s11/Zmulx;
			dxuv[1]=  - s01/Zmulx;
			dyuv[0]=  - s10/Zmuly;
			dyuv[1]=  s00/Zmuly;
			
			dl= dxuv[0] + dxuv[1];
			shi->dxsticky[0]= dl*s3[0] - dxuv[0]*s1[0] - dxuv[1]*s2[0];
			shi->dxsticky[1]= dl*s3[1] - dxuv[0]*s1[1] - dxuv[1]*s2[1];
			dl= dyuv[0] + dyuv[1];
			shi->dysticky[0]= dl*s3[0] - dyuv[0]*s1[0] - dyuv[1]*s2[0];
			shi->dysticky[1]= dl*s3[1] - dyuv[0]*s1[1] - dyuv[1]*s2[1];
		}
	}
}

static void shade_input_vlr_texco_stress(ShadeInput *shi)
{
	ObjectRen *obr= shi->obr;
	VertRen *v1= shi->v1, *v2= shi->v2, *v3= shi->v3;
	float *s1, *s2, *s3;
	float *uvw= shi->uvw;
	
	s1= render_vert_get_stress(obr, v1, 0);
	s2= render_vert_get_stress(obr, v2, 0);
	s3= render_vert_get_stress(obr, v3, 0);

	if(s1 && s2 && s3) {
		shi->stress= s1[0]*uvw[0] + s2[0]*uvw[1] + s3[0]*uvw[2];

		if(shi->stress<1.0f) shi->stress-= 1.0f;
		else shi->stress= (shi->stress-1.0f)/shi->stress;
	}
	else shi->stress= 0.0f;
}

static void shade_input_vlr_texco_orco(ShadeInput *shi)
{
	ObjectInstanceRen *obi= shi->obi;
	ObjectRen *obr= shi->obr;
	VertRen *v1= shi->v1, *v2= shi->v2, *v3= shi->v3;

	if(render_vert_get_orco(obr, v1, 0)) {
		float *o1, *o2, *o3;
		
		o1= render_vert_get_orco(obr, v1, 0);
		o2= render_vert_get_orco(obr, v2, 0);
		o3= render_vert_get_orco(obr, v3, 0);

		interp_v3_v3v3v3(shi->lo, o1, o2, o3, shi->uvw);
		
		if(shi->osatex) {
			interp_v3_v3v3v3(shi->dxlo, o1, o2, o3, shi->duvw_dx);
			interp_v3_v3v3v3(shi->dylo, o1, o2, o3, shi->duvw_dy);
		}
	}

	copy_v3_v3(shi->duplilo, obi->dupliorco);
}

static void shade_input_vlr_texco_tangent(Render *re, ShadeInput *shi, int mode)
{
	ObjectRen *obr= shi->obr;
	ObjectInstanceRen *obi= shi->obi;
	VertRen *v1= shi->v1, *v2= shi->v2, *v3= shi->v3;
	float *tangent, *s1, *s2, *s3;
	float uvw[3];

	if(shi->vlr->flag & R_SMOOTH) {
		copy_v3_v3(uvw, shi->uvw);
	}
	else {
		/* qdn: flat faces have tangents too,
		   could pick either one, using average here */
		uvw[0]= 1.0f/3.0f;
		uvw[1]= 1.0f/3.0f;
		uvw[2]= 1.0f/3.0f;
	}

	shi->tang[0]= shi->tang[1]= shi->tang[2]= 0.0f;
	shi->nmaptang[0]= shi->nmaptang[1]= shi->nmaptang[2]= 0.0f;

	if(mode & MA_TANGENT_V) {
		s1 = render_vert_get_tangent(obr, v1, 0);
		s2 = render_vert_get_tangent(obr, v2, 0);
		s3 = render_vert_get_tangent(obr, v3, 0);

		if(s1 && s2 && s3) {
			interp_v3_v3v3v3(shi->tang, s1, s2, s3, uvw);

			if(obi->flag & R_TRANSFORMED)
				mul_m3_v3(obi->nmat, shi->tang);

			normalize_v3(shi->tang);
			copy_v3_v3(shi->nmaptang, shi->tang);
		}
	}

	if(mode & MA_NORMAP_TANG || re->flag & R_NEED_TANGENT) {
		tangent= render_vlak_get_nmap_tangent(obr, shi->vlr, 0);

		if(tangent) {
			int j1= shi->i1, j2= shi->i2, j3= shi->i3;

			vlr_set_uv_indices(shi->vlr, &j1, &j2, &j3);

			s1= &tangent[j1*3];
			s2= &tangent[j2*3];
			s3= &tangent[j3*3];

			interp_v3_v3v3v3(shi->nmaptang, s1, s2, s3, uvw);

			if(obi->flag & R_TRANSFORMED)
				mul_m3_v3(obi->nmat, shi->nmaptang);

			normalize_v3(shi->nmaptang);
		}
	}
}

static void shade_input_vlr_texco_speed(ShadeInput *shi)
{
	ObjectInstanceRen *obi= shi->obi;
	VertRen *v1= shi->v1, *v2= shi->v2, *v3= shi->v3;
	float *s1, *s2, *s3;
	
	s1= render_vert_get_winspeed(obi, v1, 0);
	s2= render_vert_get_winspeed(obi, v2, 0);
	s3= render_vert_get_winspeed(obi, v3, 0);

	if(s1 && s2 && s3)
		interp_v4_v4v4v4(shi->winspeed, s1, s2, s3, shi->uvw);
	else
		zero_v4(shi->winspeed);
}

static void shade_input_vlr_texco_surface(ShadeInput *shi)
{
	ObjectRen *obr= shi->obr;
	ObjectInstanceRen *obi= shi->obi;
	float *surfnor= render_vlak_get_surfnor(obr, shi->vlr, 0);

	if(surfnor) {
		copy_v3_v3(shi->surfnor, surfnor);
		if(obi->flag & R_TRANSFORMED)
			mul_m3_v3(obi->nmat, shi->surfnor);
	}
	else
		copy_v3_v3(shi->surfnor, shi->vn);

	shi->surfdist= 0.0f;
}

static void shade_input_vlr_texco_global(RenderCamera *cam, ShadeInput *shi)
{
	copy_v3_v3(shi->gl, shi->co);
	mul_m4_v3(cam->viewinv, shi->gl);

	if(shi->osatex) {
		copy_v3_v3(shi->dxgl, shi->dxco);
		mul_mat3_m4_v3(cam->viewinv, shi->dxco);

		copy_v3_v3(shi->dygl, shi->dyco);
		mul_mat3_m4_v3(cam->viewinv, shi->dyco);
	}
}

static void shade_input_vlr_texco_strand(ShadeInput *shi)
{
	ObjectRen *obr= shi->obr;
	VertRen *v1= shi->v1, *v2= shi->v2, *v3= shi->v3;
	float sco1, sco2, sco3;
	float *uvw= shi->uvw, *duvw_dx= shi->duvw_dx, *duvw_dy= shi->duvw_dy;

	if(!render_vert_get_strandco(obr, v1, 0)) {
		shi->strandco= 0.0f;
		shi->dxstrand= 0.0f;
		shi->dystrand= 0.0f;
		return;
	}

	sco1= *render_vert_get_strandco(obr, v1, 0);
	sco2= *render_vert_get_strandco(obr, v2, 0);
	sco3= *render_vert_get_strandco(obr, v3, 0);

	shi->strandco= uvw[0]*sco1 + uvw[1]*sco2 + uvw[2]*sco3;

	if(shi->osatex) {
		shi->dxstrand= duvw_dx[0]*sco1 + duvw_dx[1]*sco2 + duvw_dx[2]*sco3;
		shi->dystrand= duvw_dy[0]*sco1 + duvw_dy[1]*sco2 + duvw_dy[2]*sco3;
	}
}

static void shade_input_vlr_texco_uvcol(Render *re, ShadeInput *shi)
{
	ObjectInstanceRen *obi= shi->obi;
	ObjectRen *obr= shi->obr;
	VlakRen *vlr= shi->vlr;
	MTFace *tface;
	MCol *mcol;
	char *name;
	int i, j1=shi->i1, j2=shi->i2, j3=shi->i3;
	int mode= shi->mode;
	float *uvw= shi->uvw, *duvw_dx= shi->duvw_dx, *duvw_dy= shi->duvw_dy;

	/* uv and vcols are not copied on split, so set them according vlr divide flag */
	vlr_set_uv_indices(vlr, &j1, &j2, &j3);

	shi->totuv= 0;
	shi->totcol= 0;
	shi->actuv= obr->actmtface;
	shi->actcol= obr->actmcol;

	if(mode & (MA_VERTEXCOL|MA_VERTEXCOLP)) {
		for (i=0; (mcol=render_vlak_get_mcol(obr, vlr, i, &name, 0)); i++) {
			ShadeInputCol *scol= &shi->col[i];
			char *cp1, *cp2, *cp3;
			
			shi->totcol++;
			scol->name= name;

			cp1= (char *)(mcol+j1);
			cp2= (char *)(mcol+j2);
			cp3= (char *)(mcol+j3);
			
			scol->col[0]= (uvw[0]*cp1[3] + uvw[1]*cp2[3] + uvw[2]*cp3[3])*(1.0f/255.0f);
			scol->col[1]= (uvw[0]*cp1[2] + uvw[1]*cp2[2] + uvw[2]*cp3[2])*(1.0f/255.0f);
			scol->col[2]= (uvw[0]*cp1[1] + uvw[1]*cp2[1] + uvw[2]*cp3[1])*(1.0f/255.0f);
		}

		if(shi->totcol) {
			shi->vcol[0]= shi->col[shi->actcol].col[0];
			shi->vcol[1]= shi->col[shi->actcol].col[1];
			shi->vcol[2]= shi->col[shi->actcol].col[2];
			shi->vcol[3]= 1.0f;
		}
		else {
			shi->vcol[0]= 0.0f;
			shi->vcol[1]= 0.0f;
			shi->vcol[2]= 0.0f;
			shi->vcol[3]= 1.0f;
		}
	}

	for (i=0; (tface=render_vlak_get_tface(obr, vlr, i, &name, 0)); i++) {
		ShadeInputUV *suv= &shi->uv[i];
		float *uv1, *uv2, *uv3;

		shi->totuv++;
		suv->name= name;
		
		uv1= tface->uv[j1];
		uv2= tface->uv[j2];
		uv3= tface->uv[j3];

		interp_v2_v2v2v2(suv->uv, uv1, uv2, uv3, uvw);
		
		suv->uv[0]= -1.0f + 2.0f*suv->uv[0];
		suv->uv[1]= -1.0f + 2.0f*suv->uv[1];
		suv->uv[2]= 0.0f;	/* texture.c assumes there are 3 coords */

		if(shi->osatex) {
			interp_v2_v2v2v2(suv->dxuv, uv1, uv2, uv3, duvw_dx);
			interp_v2_v2v2v2(suv->dyuv, uv1, uv2, uv3, duvw_dy);

			mul_v2_fl(suv->dxuv, 2.0f);
			mul_v2_fl(suv->dyuv, 2.0f);
		}

		if((mode & MA_FACETEXTURE) && i==obr->actmtface) {
			if((mode & (MA_VERTEXCOL|MA_VERTEXCOLP))==0) {
				shi->vcol[0]= 1.0f;
				shi->vcol[1]= 1.0f;
				shi->vcol[2]= 1.0f;
				shi->vcol[3]= 1.0f;
			}
			if(tface && tface->tpage)
				do_realtime_texture(re, shi, tface->tpage);
		}
	}

	shi->dupliuv[0]= -1.0f + 2.0f*obi->dupliuv[0];
	shi->dupliuv[1]= -1.0f + 2.0f*obi->dupliuv[1];
	shi->dupliuv[2]= 0.0f;

	if(shi->totuv == 0) {
		ShadeInputUV *suv= &shi->uv[0];

		suv->uv[0]= 2.0f*(0.5f - uvw[0]);
		suv->uv[1]= 2.0f*(0.5f - uvw[1]);
		suv->uv[2]= 0.0f;	/* texture.c assumes there are 3 coords */
		
		if(mode & MA_FACETEXTURE) {
			/* no tface? set at 1.0f */
			shi->vcol[0]= 1.0f;
			shi->vcol[1]= 1.0f;
			shi->vcol[2]= 1.0f;
			shi->vcol[3]= 1.0f;
		}
	}
}

void shade_input_set_shade_texco(Render *re, ShadeInput *shi)
{
	int mode= shi->mode;		/* or-ed result for all nodes */
	short texco= shi->mat->texco;

	/* normal */
	if((texco & (TEXCO_NORM|TEXCO_REFL)))
		shade_input_vlr_texco_normal(shi);

	/* tangents */
	if(mode & (MA_TANGENT_V|MA_NORMAP_TANG) || re->flag & R_NEED_TANGENT)
		shade_input_vlr_texco_tangent(re, shi, mode);

	/* surface normal */
	if(mode & MA_STR_SURFDIFF)
		shade_input_vlr_texco_surface(shi);
	
	/* speed */
	if(re->r.mode & R_SPEED)
		shade_input_vlr_texco_speed(shi);

	/* pass option forces UV calc */
	if(shi->passflag & SCE_PASS_UV)
		texco |= (NEED_UV|TEXCO_UV);
	
	/* texture coordinates. shi->dxuv shi->dyuv have been set */
	if(texco & NEED_UV) {
		
		if(texco & TEXCO_ORCO)
			shade_input_vlr_texco_orco(shi);
		
		if(texco & TEXCO_GLOB)
			shade_input_vlr_texco_global(&re->cam, shi);
		
		if(texco & TEXCO_STRAND)
			shade_input_vlr_texco_strand(shi);
				
		if((texco & TEXCO_UV) || (mode & (MA_VERTEXCOL|MA_VERTEXCOLP|MA_FACETEXTURE)))
			shade_input_vlr_texco_uvcol(re, shi);
		
		if(texco & TEXCO_NORM)
			negate_v3_v3(shi->orn, shi->vn);
		
		if(texco & TEXCO_REFL) {
			/* mirror reflection color textures (and envmap) */
			shade_input_calc_reflection(shi);	/* wrong location for normal maps! XXXXXXXXXXXXXX */
		}
		
		if(texco & TEXCO_STRESS)
			shade_input_vlr_texco_stress(shi);
		
		if(texco & TEXCO_TANGENT) {
			if((mode & MA_TANGENT_V)==0) {
				/* just prevent surprises */
				zero_v3(shi->tang);
				zero_v3(shi->nmaptang);
			}
		}
	}
	
	/* this only avalailable for scanline renders */
	if(shi->depth==0) {
		if(texco & TEXCO_WINDOW)
			shade_input_vlr_texco_window(&re->cam, shi);

		if(texco & TEXCO_STICKY)
			shade_input_vlr_texco_sticky(re, shi);
	}
	else {
		/* Note! For raytracing winco is not set, important because this means
		   all shader input's need to have their variables set to zero else
		   un-initialized values are used */
	}

	if(re->r.color_mgt_flag & R_COLOR_MANAGEMENT)
		if(mode & (MA_VERTEXCOL|MA_VERTEXCOLP|MA_FACETEXTURE))
			srgb_to_linearrgb_v3_v3(shi->vcol, shi->vcol);
}

/* --------------------------------------------- */
/* also called from texture.c */
void shade_input_calc_reflection(ShadeInput *shi)
{
	reflect_v3_v3v3(shi->ref, shi->view, shi->vn);

	if(shi->osatex) {
		float dview[3], dno[3], ref[3];

		/* dxref */
		if(shi->vlr->flag & R_SMOOTH)
			add_v3_v3v3(dno, shi->vn, shi->dxno);
		else
			copy_v3_v3(dno, shi->vn);

		dview[0]= shi->view[0] + shi->dxview;
		dview[1]= shi->view[1];
		dview[2]= shi->view[2];

		reflect_v3_v3v3(ref, dview, dno);
		sub_v3_v3v3(shi->dxref, shi->ref, ref);

		/* dyref */
		if(shi->vlr->flag & R_SMOOTH)
			add_v3_v3v3(dno, shi->vn, shi->dyno);
		else
			copy_v3_v3(dno, shi->vn);

		dview[0]= shi->view[0];
		dview[1]= shi->view[1] + shi->dyview;
		dview[2]= shi->view[2];

		reflect_v3_v3v3(ref, dview, dno);
		sub_v3_v3v3(shi->dyref, shi->ref, ref);
	}
}

/******************************* Initialization ******************************/

/* initialize per part, not per pixel! */
static void shade_input_initialize(Render *re, ShadeInput *shi, RenderPart *pa, RenderLayer *rl, int sample)
{
	
	memset(shi, 0, sizeof(ShadeInput));
	
	shi->sample= sample;
	shi->thread= pa->thread;
	shi->pa= pa;
	shi->do_preview= (re->r.scemode & R_MATNODE_PREVIEW) != 0;
	shi->lay= rl->lay;
	shi->layflag= rl->layflag;
	shi->passflag= rl->passflag;
	shi->combinedflag= ~rl->pass_xor;
	shi->mat_override= rl->mat_override;
	shi->light_override= rl->light_override;
	shi->except_override= rl->except_override;
//	shi->rl= rl;
	/* note shi.depth==0  means first hit, not raytracing */
}

/* initialize per part, not per pixel! */
void shade_sample_initialize(Render *re, ShadeSample *ssamp, RenderPart *pa, RenderLayer *rl)
{
	int a, tot;
	
	tot= re->osa==0?1:re->osa;
	
	for(a=0; a<tot; a++) {
		shade_input_initialize(re, &ssamp->shi[a], pa, rl, a);
		memset(&ssamp->shr[a], 0, sizeof(ShadeResult));
	}
}

/**************************** ShadeInput from Pixstr *************************/

static void shade_input_from_pixel(Render *re, ShadeInput *shi, int x, int y, int z, short samp, unsigned short mask)
{
	float xs, ys, ofs[2];

	if(shi->vlr->flag & R_FULL_OSA) {
		/* zbuffer has this inverse corrected, ensures xs,ys are inside pixel */
		pxf_sample_offset(&re->sample, samp, ofs);
	}
	else {
		/* averaged offset for mask */
		pxf_mask_offset(&re->sample, mask, ofs);
	}

	xs= (float)x + ofs[0];
	ys= (float)y + ofs[1];

	shi->mask= mask;
	shi->samplenr= re->sample.shadowsamplenr[shi->thread]++;

	/* normal is needed for shade_input_set_viewco */
	shi->flippednor= 0;
	render_vlak_get_normal(shi->obi, shi->vlr, shi->facenor, (shi->i3 == 3));

	shade_input_set_viewco(re, shi, x, y, xs, ys, (float)z);
	shade_input_set_uv(shi);
	shade_input_set_normals(shi);
}

static int shade_inputs_from_pixel(Render *re, ShadeInput *shi, PixelRow *row, int x, int y)
{
	unsigned short mask= row->mask;
	int tot= 0;

	if(row->p > 0) {
		shi->obi= part_get_instance(shi->pa, &re->db.objectinstance[row->obi]);
		shi->obr= shi->obi->obr;
		shi->facenr= (row->p-1) & RE_QUAD_MASK;

		if(shi->facenr < shi->obr->totvlak) {
			VlakRen *vlr= render_object_vlak_get(shi->obr, shi->facenr);
			
			if(row->p & RE_QUAD_OFFS)
				shade_input_set_triangle_i(re, shi, shi->obi, vlr, 0, 2, 3);
			else
				shade_input_set_triangle_i(re, shi, shi->obi, vlr, 0, 1, 2);
		}
		else {
			shi->vlr= NULL;
			return 0;
		}
	}
	else {
		shi->vlr= NULL;
		return 0;
	}

	/* full osa is only set for OSA renders */
	if(shi->vlr->flag & R_FULL_OSA) {
		short samp;
		
		for(samp=0; samp<re->osa; samp++) {
			if(mask & (1<<samp)) {
				if(tot) {
					int shi_sample= shi->sample;
					*shi= *(shi-1);
					shi->sample= shi_sample;
				}
				
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
	if(shi->passflag & ~(SCE_PASS_Z|SCE_PASS_INDEXOB)) {
		for(samp=0; samp<ssamp->tot; samp++, shi++, shr++) {
			shade_input_set_shade_texco(re, shi);
			shade_input_do_shade(re, shi, shr);
		}
	}
	else if(shi->passflag & SCE_PASS_Z) {
		for(samp=0; samp<ssamp->tot; samp++, shi++, shr++)
			shr->z= -shi->co[2];
	}
}

/* also used as callback for nodes */
/* delivers a fully filled in ShadeResult, for all passes */
void shade_material_loop(Render *re, ShadeInput *shi, ShadeResult *shr)
{
	shade_surface(re, shi, shr, 0); /* clears shr */
	
	if(shi->translucency!=0.0f) {
		ShadeResult shr_t;
		float fac= shi->translucency;
		
		shade_input_init_material(re, shi);

		copy_v3_v3(shi->vn, shi->vno);
		negate_v3(shi->vn);
		negate_v3(shi->facenor);
		shi->depth++;	/* hack to get real shadow now */

		shade_surface(re, shi, &shr_t, 1);

		shi->depth--;
		negate_v3(shi->vn);
		negate_v3(shi->facenor);

		/* a couple of passes */
		madd_v3_v3fl(shr->combined, shr_t.combined, fac);
		if(shi->passflag & SCE_PASS_SPEC)
			madd_v3_v3fl(shr->spec, shr_t.spec, fac);
		if(shi->passflag & SCE_PASS_DIFFUSE)
			madd_v3_v3fl(shr->diff, shr_t.diff, fac);
		if(shi->passflag & SCE_PASS_SHADOW)
			madd_v3_v3fl(shr->shad, shr_t.shad, fac);
	}

	/* depth >= 1 when ray-shading */
	if(shi->depth==0) {
		/* disable adding of sky for raytransp */
		if((shi->mat->mode & MA_TRANSP) && (shi->mat->mode & MA_RAYTRANSP))
			if((shi->layflag & SCE_LAY_SKY) && (re->r.alphamode==R_ADDSKY))
				shr->alpha= 1.0f;
	}	

	if(re->r.mode & R_RAYTRACE) {
		if (re->db.render_volumes_inside.first)
			shade_volume_inside(re, shi, shr);
	}
}

static void shade_mist(Render *re, ShadeInput *shi, ShadeResult *shr)
{
	float mist= 1.0f;

	if((shi->passflag & SCE_PASS_MIST) || ((re->db.wrld.mode & WO_MIST) && (shi->mat->mode & MA_NOMIST)==0))  {
		if(re->cam.type == CAM_ORTHO)
			shr->mist= environment_mist_factor(re, -shi->co[2], shi->co);
		else
			shr->mist= environment_mist_factor(re, len_v3(shi->co), shi->co);
	}
	else shr->mist= 0.0f;

	if((re->db.wrld.mode & WO_MIST) && (shi->mat->mode & MA_NOMIST)==0 ) {
		mist= shr->mist;

		if(mist != 1.0f)
			if(shi->mat->material_type!= MA_TYPE_VOLUME)
				mul_v3_fl(shr->combined, mist);
	}

	shr->combined[3]= shr->alpha*mist;
}

/* do a shade, finish up some passes, apply mist */
void shade_input_do_shade(Render *re, ShadeInput *shi, ShadeResult *shr)
{
	/* ------  main shading loop -------- */
#ifdef RE_RAYCOUNTER
	memset(&shi->raycounter, 0, sizeof(shi->raycounter));
#endif
	
	if(shi->mat->nodetree && shi->mat->use_nodes) {
		ntreeShaderExecTree(shi->mat->nodetree, re, shi, shr);
	}
	else {
		/* copy all relevant material vars, note, keep this synced with render_types.h */
		shade_input_init_material(re, shi);
		
		if (shi->mat->material_type == MA_TYPE_VOLUME) {
			if(re->r.mode & R_RAYTRACE) {
				shade_volume_outside(re, shi, shr);
				shr->combined[3]= shr->alpha;
			}
		} else { /* MA_TYPE_SURFACE, MA_TYPE_WIRE */
			shade_material_loop(re, shi, shr);
		}
	}
	
	/* vector, normal, indexob pass */
	if(shi->passflag & (SCE_PASS_VECTOR|SCE_PASS_NORMAL|SCE_PASS_INDEXOB|SCE_PASS_INDEXMA)) {
		copy_v4_v4(shr->winspeed, shi->winspeed);
		copy_v3_v3(shr->nor, shi->vn);
		shr->indexob= shi->obi->index;

		/*nodetree sets indexma itself*/
		if (!(shi->mat->nodetree && shi->mat->use_nodes))
			shr->indexma= shi->index;
	}

	/* uv pass */
	if(shi->passflag & SCE_PASS_UV) {
		if(shi->totuv) {
			shr->uv[0]= 0.5f + 0.5f*shi->uv[shi->actuv].uv[0];
			shr->uv[1]= 0.5f + 0.5f*shi->uv[shi->actuv].uv[1];
			shr->uv[2]= 1.0f;
		}
	}
	
	/* mist pass */
	shade_mist(re, shi, shr);
	
	/* z pass */
	shr->z= -shi->co[2];
	
	/* RAYHITS */
#if 0
	if(1 || shi->passflag & SCE_PASS_RAYHITS)
	{
		shr->nor[0] = (float)shi->raycounter.faces.test/100.0f;
		shr->nor[1] = (float)shi->raycounter.simd_bb.test/100.0f;
		shr->nor[2] = (float)shi->raycounter.simd_bb.hit/100.0f;
	}
#endif
	RE_RC_MERGE(&re_rc_counter[shi->thread], &shi->raycounter);
}


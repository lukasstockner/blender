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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): 2004-2006, Blender Foundation, full recode
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_rand.h"

#include "DNA_texture_types.h"
#include "DNA_object_types.h"
#include "DNA_lamp_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_material_types.h"
#include "DNA_image_types.h"
#include "DNA_node_types.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "BKE_colortools.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_material.h"
#include "BKE_node.h"
#include "BKE_plugin_types.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"

#include "envmap.h"
#include "pointdensity.h"
#include "render_types.h"
#include "rendercore.h"
#include "shading.h"
#include "texture.h"
#include "voxeldata.h"

/************************ Utilities **************************/

/* this allows colorbanded textures to control normals as well */
static void tex_normal_derivate(Tex *tex, TexResult *texres)
{
	if(tex->flag & TEX_COLORBAND) {
		float col[4];

		if(do_colorband(tex->coba, texres->tin, col)) {
			float fac0, fac1, fac2, fac3;
			
			fac0= (col[0]+col[1]+col[2]);
			do_colorband(tex->coba, texres->nor[0], col);
			fac1= (col[0]+col[1]+col[2]);
			do_colorband(tex->coba, texres->nor[1], col);
			fac2= (col[0]+col[1]+col[2]);
			do_colorband(tex->coba, texres->nor[2], col);
			fac3= (col[0]+col[1]+col[2]);
			
			texres->nor[0]= 0.3333*(fac0 - fac1);
			texres->nor[1]= 0.3333*(fac0 - fac2);
			texres->nor[2]= 0.3333*(fac0 - fac3);
			
			return;
		}
	}

	texres->nor[0]= texres->tin - texres->nor[0];
	texres->nor[1]= texres->tin - texres->nor[1];
	texres->nor[2]= texres->tin - texres->nor[2];
}

void tex_brightness_contrast(Tex *tex, TexResult *texres)
{
	texres->tin= (texres->tin - 0.5)*tex->contrast + tex->bright - 0.5;

	if(texres->tin<0.0)
		texres->tin= 0.0;
	else if(texres->tin>1.0)
		texres->tin= 1.0;
}

void tex_brightness_contrast_rgb(Tex *tex, TexResult *texres)
{
	texres->tr= tex->rfac*((texres->tr - 0.5)*tex->contrast + tex->bright - 0.5);
	if(texres->tr<0.0) texres->tr= 0.0;
	texres->tg= tex->gfac*((texres->tg - 0.5)*tex->contrast + tex->bright - 0.5);
	if(texres->tg<0.0) texres->tg= 0.0;
	texres->tb= tex->bfac*((texres->tb - 0.5)*tex->contrast + tex->bright - 0.5);
	if(texres->tb<0.0) texres->tb= 0.0;
}

/******************************* Blend ***********************************/

static int tex_blend_sample(Tex *tex, float *texvec, TexResult *texres)
{
	float x, y, t;

	if(tex->flag & TEX_FLIPBLEND) {
		x= texvec[1];
		y= texvec[0];
	}
	else {
		x= texvec[0];
		y= texvec[1];
	}

	if(tex->stype==TEX_LIN) {	/* lin */
		texres->tin= (1.0+x)/2.0;
	}
	else if(tex->stype==TEX_QUAD) {	/* quad */
		texres->tin= (1.0+x)/2.0;
		if(texres->tin<0.0) texres->tin= 0.0;
		else texres->tin*= texres->tin;
	}
	else if(tex->stype==TEX_EASE) {	/* ease */
		texres->tin= (1.0+x)/2.0;
		if(texres->tin<=.0) texres->tin= 0.0;
		else if(texres->tin>=1.0) texres->tin= 1.0;
		else {
			t= texres->tin*texres->tin;
			texres->tin= (3.0*t-2.0*t*texres->tin);
		}
	}
	else if(tex->stype==TEX_DIAG) { /* diag */
		texres->tin= (2.0+x+y)/4.0;
	}
	else if(tex->stype==TEX_RAD) { /* radial */
		texres->tin= (atan2(y,x) / (2*M_PI) + 0.5);
	}
	else {  /* sphere TEX_SPHERE */
		texres->tin= 1.0-sqrt(x*x+	y*y+texvec[2]*texvec[2]);
		if(texres->tin<0.0) texres->tin= 0.0;
		if(tex->stype==TEX_HALO) texres->tin*= texres->tin;  /* halo */
	}

	tex_brightness_contrast(tex, texres);

	return TEX_INT;
}

/******************************* Clouds ****************************************/
/* newnoise: all noisebased types now have different noisebases to choose from */

static int tex_clouds_sample(Tex *tex, float *texvec, TexResult *texres)
{
	int rv = TEX_INT;
	
	texres->tin = BLI_gTurbulence(tex->noisesize, texvec[0], texvec[1], texvec[2], tex->noisedepth, (tex->noisetype!=TEX_NOISESOFT), tex->noisebasis);

	if(texres->nor!=NULL) {
		// calculate bumpnormal
		texres->nor[0] = BLI_gTurbulence(tex->noisesize, texvec[0] + tex->nabla, texvec[1], texvec[2], tex->noisedepth,  (tex->noisetype!=TEX_NOISESOFT), tex->noisebasis);
		texres->nor[1] = BLI_gTurbulence(tex->noisesize, texvec[0], texvec[1] + tex->nabla, texvec[2], tex->noisedepth,  (tex->noisetype!=TEX_NOISESOFT), tex->noisebasis);
		texres->nor[2] = BLI_gTurbulence(tex->noisesize, texvec[0], texvec[1], texvec[2] + tex->nabla, tex->noisedepth,  (tex->noisetype!=TEX_NOISESOFT), tex->noisebasis);
		
		tex_normal_derivate(tex, texres);
		rv |= TEX_NOR;
	}

	if(tex->stype==TEX_COLOR) {
		// in this case, int. value should really be computed from color,
		// and bumpnormal from that, would be too slow, looks ok as is
		texres->tr = texres->tin;
		texres->tg = BLI_gTurbulence(tex->noisesize, texvec[1], texvec[0], texvec[2], tex->noisedepth, (tex->noisetype!=TEX_NOISESOFT), tex->noisebasis);
		texres->tb = BLI_gTurbulence(tex->noisesize, texvec[1], texvec[2], texvec[0], tex->noisedepth, (tex->noisetype!=TEX_NOISESOFT), tex->noisebasis);
		tex_brightness_contrast_rgb(tex, texres);
		texres->ta = 1.0;
		return (rv | TEX_RGB);
	}

	tex_brightness_contrast(tex, texres);

	return rv;

}

/**************************** Wood ******************************/

/* creates a sine wave */
static float tex_sin(float a)
{
	return 0.5 + 0.5*sin(a);
}

/* creates a saw wave */
static float tex_saw(float a)
{
	const float b = 2*M_PI;
	
	int n = (int)(a / b);
	a -= n*b;
	if(a < 0) a += b;
	return a / b;
}

/* creates a triangle wave */
static float tex_tri(float a)
{
	const float b = 2*M_PI;
	const float rmax = 1.0;
	
	a = rmax - 2.0*fabs(floor((a*(1.0/b))+0.5) - (a*(1.0/b)));
	
	return a;
}

/* computes basic wood intensity value at x,y,z */
static float wood_int(Tex *tex, float x, float y, float z)
{
	float wi=0;						
	short wf = tex->noisebasis2;	/* wave form:	TEX_SIN=0,  TEX_SAW=1,  TEX_TRI=2						 */
	short wt = tex->stype;			/* wood type:	TEX_BAND=0, TEX_RING=1, TEX_BANDNOISE=2, TEX_RINGNOISE=3 */

	float (*waveform[3])(float);	/* create array of pointers to waveform functions */
	waveform[0] = tex_sin;			/* assign address of tex_sin() function to pointer array */
	waveform[1] = tex_saw;
	waveform[2] = tex_tri;
	
	if((wf>TEX_TRI) || (wf<TEX_SIN)) wf=0; /* check to be sure noisebasis2 is initialized ahead of time */
		
	if(wt==TEX_BAND) {
		wi = waveform[wf]((x + y + z)*10.0);
	}
	else if(wt==TEX_RING) {
		wi = waveform[wf](sqrt(x*x + y*y + z*z)*20.0);
	}
	else if(wt==TEX_BANDNOISE) {
		wi = tex->turbul*BLI_gNoise(tex->noisesize, x, y, z, (tex->noisetype!=TEX_NOISESOFT), tex->noisebasis);
		wi = waveform[wf]((x + y + z)*10.0 + wi);
	}
	else if(wt==TEX_RINGNOISE) {
		wi = tex->turbul*BLI_gNoise(tex->noisesize, x, y, z, (tex->noisetype!=TEX_NOISESOFT), tex->noisebasis);
		wi = waveform[wf](sqrt(x*x + y*y + z*z)*20.0 + wi);
	}
	
	return wi;
}

static int tex_wood_sample(Tex *tex, float *texvec, TexResult *texres)
{
	int rv=TEX_INT;

	texres->tin = wood_int(tex, texvec[0], texvec[1], texvec[2]);
	if(texres->nor!=NULL) {
		/* calculate bumpnormal */
		texres->nor[0] = wood_int(tex, texvec[0] + tex->nabla, texvec[1], texvec[2]);
		texres->nor[1] = wood_int(tex, texvec[0], texvec[1] + tex->nabla, texvec[2]);
		texres->nor[2] = wood_int(tex, texvec[0], texvec[1], texvec[2] + tex->nabla);
		
		tex_normal_derivate(tex, texres);
		rv |= TEX_NOR;
	}

	tex_brightness_contrast(tex, texres);

	return rv;
}

/**************************** Marble ******************************/

/* computes basic marble intensity at x,y,z */
static float marble_int(Tex *tex, float x, float y, float z)
{
	float n, mi;
	short wf = tex->noisebasis2;	/* wave form:	TEX_SIN=0,  TEX_SAW=1,  TEX_TRI=2						*/
	short mt = tex->stype;			/* marble type:	TEX_SOFT=0,	TEX_SHARP=1,TEX_SHAPER=2 					*/

	float (*waveform[3])(float);	/* create array of pointers to waveform functions */
	waveform[0] = tex_sin;			/* assign address of tex_sin() function to pointer array */
	waveform[1] = tex_saw;
	waveform[2] = tex_tri;

	if((wf>TEX_TRI) || (wf<TEX_SIN)) wf=0; /* check to be sure noisebasis2 isn't initialized ahead of time */

	n = 5.0 * (x + y + z);

	mi = n + tex->turbul * BLI_gTurbulence(tex->noisesize, x, y, z, tex->noisedepth, (tex->noisetype!=TEX_NOISESOFT),  tex->noisebasis);

	if(mt>=TEX_SOFT) {  /* TEX_SOFT always true */
		mi = waveform[wf](mi);
		if(mt==TEX_SHARP) {
			mi = sqrt(mi);
		} 
		else if(mt==TEX_SHARPER) {
			mi = sqrt(sqrt(mi));
		}
	}

	return mi;
}

static int tex_marble_sample(Tex *tex, float *texvec, TexResult *texres)
{
	int rv=TEX_INT;

	texres->tin = marble_int(tex, texvec[0], texvec[1], texvec[2]);

	if(texres->nor!=NULL) {
		/* calculate bumpnormal */
		texres->nor[0] = marble_int(tex, texvec[0] + tex->nabla, texvec[1], texvec[2]);
		texres->nor[1] = marble_int(tex, texvec[0], texvec[1] + tex->nabla, texvec[2]);
		texres->nor[2] = marble_int(tex, texvec[0], texvec[1], texvec[2] + tex->nabla);

		tex_normal_derivate(tex, texres);

		rv |= TEX_NOR;
	}

	tex_brightness_contrast(tex, texres);

	return rv;
}

/**************************** Magic ******************************/

static int tex_magic_sample(Tex *tex, float *texvec, TexResult *texres)
{
	float x, y, z, turb=1.0;
	int n;

	n= tex->noisedepth;
	turb= tex->turbul/5.0;

	x=  sin( ( texvec[0]+texvec[1]+texvec[2])*5.0 );
	y=  cos( (-texvec[0]+texvec[1]-texvec[2])*5.0 );
	z= -cos( (-texvec[0]-texvec[1]+texvec[2])*5.0 );
	if(n>0) {
		x*= turb;
		y*= turb;
		z*= turb;
		y= -cos(x-y+z);
		y*= turb;
		if(n>1) {
			x= cos(x-y-z);
			x*= turb;
			if(n>2) {
				z= sin(-x-y-z);
				z*= turb;
				if(n>3) {
					x= -cos(-x+y-z);
					x*= turb;
					if(n>4) {
						y= -sin(-x+y+z);
						y*= turb;
						if(n>5) {
							y= -cos(-x+y+z);
							y*= turb;
							if(n>6) {
								x= cos(x+y+z);
								x*= turb;
								if(n>7) {
									z= sin(x+y-z);
									z*= turb;
									if(n>8) {
										x= -cos(-x-y+z);
										x*= turb;
										if(n>9) {
											y= -sin(x-y+z);
											y*= turb;
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}

	if(turb!=0.0) {
		turb*= 2.0;
		x/= turb; 
		y/= turb; 
		z/= turb;
	}
	texres->tr= 0.5-x;
	texres->tg= 0.5-y;
	texres->tb= 0.5-z;

	texres->tin= 0.3333*(texres->tr+texres->tg+texres->tb);

	tex_brightness_contrast_rgb(tex, texres);
	texres->ta= 1.0;

	return TEX_RGB;
}

/**************************** Stucci ******************************/

/* newnoise: tex_sample_stucci also modified to use different noisebasis */
static int tex_stucci_sample(Tex *tex, float *texvec, TexResult *texres)
{
	float nor[3], b2, ofs;
	int retval= TEX_INT;

	b2= BLI_gNoise(tex->noisesize, texvec[0], texvec[1], texvec[2], (tex->noisetype!=TEX_NOISESOFT), tex->noisebasis);

	ofs= tex->turbul/200.0;

	if(tex->stype) ofs*=(b2*b2);
	nor[0] = BLI_gNoise(tex->noisesize, texvec[0]+ofs, texvec[1], texvec[2], (tex->noisetype!=TEX_NOISESOFT), tex->noisebasis);
	nor[1] = BLI_gNoise(tex->noisesize, texvec[0], texvec[1]+ofs, texvec[2], (tex->noisetype!=TEX_NOISESOFT), tex->noisebasis);	
	nor[2] = BLI_gNoise(tex->noisesize, texvec[0], texvec[1], texvec[2]+ofs, (tex->noisetype!=TEX_NOISESOFT), tex->noisebasis);

	texres->tin= nor[2];

	if(texres->nor) { 

		copy_v3_v3(texres->nor, nor);
		tex_normal_derivate(tex, texres);

		if(tex->stype==TEX_WALLOUT) {
			texres->nor[0]= -texres->nor[0];
			texres->nor[1]= -texres->nor[1];
			texres->nor[2]= -texres->nor[2];
		}

		retval |= TEX_NOR;
	}

	if(tex->stype==TEX_WALLOUT) 
		texres->tin= 1.0f-texres->tin;

	if(texres->tin<0.0f)
		texres->tin= 0.0f;

	return retval;
}

/******************************* Musgrave ******************************/
/* newnoise: musgrave terrain noise types                              */

static float mg_mFractalOrfBmTex(Tex *tex, float *texvec, TexResult *texres)
{
	int rv = TEX_INT;
	float (*mgravefunc)(float, float, float, float, float, float, int);

	if(tex->stype==TEX_MFRACTAL)
		mgravefunc = mg_MultiFractal;
	else
		mgravefunc = mg_fBm;

	texres->tin = tex->ns_outscale*mgravefunc(texvec[0], texvec[1], texvec[2], tex->mg_H, tex->mg_lacunarity, tex->mg_octaves, tex->noisebasis);

	if(texres->nor!=NULL) {
		float offs= tex->nabla/tex->noisesize;	// also scaling of texvec

		/* calculate bumpnormal */
		texres->nor[0] = tex->ns_outscale*mgravefunc(texvec[0] + offs, texvec[1], texvec[2], tex->mg_H, tex->mg_lacunarity, tex->mg_octaves, tex->noisebasis);
		texres->nor[1] = tex->ns_outscale*mgravefunc(texvec[0], texvec[1] + offs, texvec[2], tex->mg_H, tex->mg_lacunarity, tex->mg_octaves, tex->noisebasis);
		texres->nor[2] = tex->ns_outscale*mgravefunc(texvec[0], texvec[1], texvec[2] + offs, tex->mg_H, tex->mg_lacunarity, tex->mg_octaves, tex->noisebasis);

		tex_normal_derivate(tex, texres);
		rv |= TEX_NOR;
	}

	tex_brightness_contrast(tex, texres);

	return rv;

}

static float mg_ridgedOrHybridMFTex(Tex *tex, float *texvec, TexResult *texres)
{
	int rv = TEX_INT;
	float (*mgravefunc)(float, float, float, float, float, float, float, float, int);

	if(tex->stype==TEX_RIDGEDMF)
		mgravefunc = mg_RidgedMultiFractal;
	else
		mgravefunc = mg_HybridMultiFractal;

	texres->tin = tex->ns_outscale*mgravefunc(texvec[0], texvec[1], texvec[2], tex->mg_H, tex->mg_lacunarity, tex->mg_octaves, tex->mg_offset, tex->mg_gain, tex->noisebasis);

	if(texres->nor!=NULL) {
		float offs= tex->nabla/tex->noisesize;	// also scaling of texvec

		/* calculate bumpnormal */
		texres->nor[0] = tex->ns_outscale*mgravefunc(texvec[0] + offs, texvec[1], texvec[2], tex->mg_H, tex->mg_lacunarity, tex->mg_octaves, tex->mg_offset, tex->mg_gain, tex->noisebasis);
		texres->nor[1] = tex->ns_outscale*mgravefunc(texvec[0], texvec[1] + offs, texvec[2], tex->mg_H, tex->mg_lacunarity, tex->mg_octaves, tex->mg_offset, tex->mg_gain, tex->noisebasis);
		texres->nor[2] = tex->ns_outscale*mgravefunc(texvec[0], texvec[1], texvec[2] + offs, tex->mg_H, tex->mg_lacunarity, tex->mg_octaves, tex->mg_offset, tex->mg_gain, tex->noisebasis);

		tex_normal_derivate(tex, texres);
		rv |= TEX_NOR;
	}

	tex_brightness_contrast(tex, texres);

	return rv;
}


static float mg_HTerrainTex(Tex *tex, float *texvec, TexResult *texres)
{
	int rv = TEX_INT;

	texres->tin = tex->ns_outscale*mg_HeteroTerrain(texvec[0], texvec[1], texvec[2], tex->mg_H, tex->mg_lacunarity, tex->mg_octaves, tex->mg_offset, tex->noisebasis);

	if(texres->nor!=NULL) {
		float offs= tex->nabla/tex->noisesize;	// also scaling of texvec

		/* calculate bumpnormal */
		texres->nor[0] = tex->ns_outscale*mg_HeteroTerrain(texvec[0] + offs, texvec[1], texvec[2], tex->mg_H, tex->mg_lacunarity, tex->mg_octaves, tex->mg_offset, tex->noisebasis);
		texres->nor[1] = tex->ns_outscale*mg_HeteroTerrain(texvec[0], texvec[1] + offs, texvec[2], tex->mg_H, tex->mg_lacunarity, tex->mg_octaves, tex->mg_offset, tex->noisebasis);
		texres->nor[2] = tex->ns_outscale*mg_HeteroTerrain(texvec[0], texvec[1], texvec[2] + offs, tex->mg_H, tex->mg_lacunarity, tex->mg_octaves, tex->mg_offset, tex->noisebasis);

		tex_normal_derivate(tex, texres);
		rv |= TEX_NOR;
	}

	tex_brightness_contrast(tex, texres);

	return rv;
}

static int tex_musgrave_sample(Tex *tex, float *texvec, TexResult *texres)
{
	float tmpvec[3];

	/* ton: added this, for Blender convention reason. 
	 * artificer: added the use of tmpvec to avoid scaling texvec
	 */
	copy_v3_v3(tmpvec, texvec);
	mul_v3_fl(tmpvec, 1.0/tex->noisesize);

	switch(tex->stype) {
		case TEX_MFRACTAL:
		case TEX_FBM:
			return mg_mFractalOrfBmTex(tex, tmpvec, texres);
		case TEX_RIDGEDMF:
		case TEX_HYBRIDMF:
			return mg_ridgedOrHybridMFTex(tex, tmpvec, texres);
		case TEX_HTERRAIN:
			return mg_HTerrainTex(tex, tmpvec, texres);
		default:
			return 0;
	}
}

/************************ Distored Noise ***********************/

static float mg_distNoiseTex(Tex *tex, float *texvec, TexResult *texres)
{
	int rv = TEX_INT;

	texres->tin = mg_VLNoise(texvec[0], texvec[1], texvec[2], tex->dist_amount, tex->noisebasis, tex->noisebasis2);

	if(texres->nor!=NULL) {
		float offs= tex->nabla/tex->noisesize;	// also scaling of texvec

		/* calculate bumpnormal */
		texres->nor[0] = mg_VLNoise(texvec[0] + offs, texvec[1], texvec[2], tex->dist_amount, tex->noisebasis, tex->noisebasis2);
		texres->nor[1] = mg_VLNoise(texvec[0], texvec[1] + offs, texvec[2], tex->dist_amount, tex->noisebasis, tex->noisebasis2);
		texres->nor[2] = mg_VLNoise(texvec[0], texvec[1], texvec[2] + offs, tex->dist_amount, tex->noisebasis, tex->noisebasis2);

		tex_normal_derivate(tex, texres);
		rv |= TEX_NOR;
	}

	tex_brightness_contrast(tex, texres);

	return rv;
}

static int tex_distnoise_sample(Tex *tex, float *texvec, TexResult *texres)
{
	float tmpvec[3];

	/* ton: added this, for Blender convention reason. 
	 * artificer: added the use of tmpvec to avoid scaling texvec
	 */
	copy_v3_v3(tmpvec, texvec);
	mul_v3_fl(tmpvec, 1.0/tex->noisesize);

	return mg_distNoiseTex(tex, tmpvec, texres);
}

/****************************** Voronoi *******************************/
/* newnoise: Voronoi texture type,  probably the slowest, especially  */
/* with minkovsky, bumpmapping, could be done another way             */

static int tex_voronoi_sample(Tex *tex, float *texvec, TexResult *texres)
{
	int rv = TEX_INT;
	float da[4], pa[12];	/* distance and point coordinate arrays of 4 nearest neighbours */
	float aw1 = fabs(tex->vn_w1);
	float aw2 = fabs(tex->vn_w2);
	float aw3 = fabs(tex->vn_w3);
	float aw4 = fabs(tex->vn_w4);
	float sc = (aw1 + aw2 + aw3 + aw4);
	float tmpvec[3];
	if(sc!=0.f) sc =  tex->ns_outscale/sc;

	/* ton: added this, for Blender convention reason.
	 * artificer: added the use of tmpvec to avoid scaling texvec
	 */
	copy_v3_v3(tmpvec, texvec);
	mul_v3_fl(tmpvec, 1.0/tex->noisesize);

	voronoi(tmpvec[0], tmpvec[1], tmpvec[2], da, pa, tex->vn_mexp, tex->vn_distm);
	texres->tin = sc * fabs(tex->vn_w1*da[0] + tex->vn_w2*da[1] + tex->vn_w3*da[2] + tex->vn_w4*da[3]);

	if(tex->vn_coltype) {
		float ca[3];	/* cell color */
		cellNoiseV(pa[0], pa[1], pa[2], ca);
		texres->tr = aw1*ca[0];
		texres->tg = aw1*ca[1];
		texres->tb = aw1*ca[2];
		cellNoiseV(pa[3], pa[4], pa[5], ca);
		texres->tr += aw2*ca[0];
		texres->tg += aw2*ca[1];
		texres->tb += aw2*ca[2];
		cellNoiseV(pa[6], pa[7], pa[8], ca);
		texres->tr += aw3*ca[0];
		texres->tg += aw3*ca[1];
		texres->tb += aw3*ca[2];
		cellNoiseV(pa[9], pa[10], pa[11], ca);
		texres->tr += aw4*ca[0];
		texres->tg += aw4*ca[1];
		texres->tb += aw4*ca[2];
		if(tex->vn_coltype>=2) {
			float t1 = (da[1]-da[0])*10;
			if(t1>1) t1=1;
			if(tex->vn_coltype==3) t1*=texres->tin; else t1*=sc;
			texres->tr *= t1;
			texres->tg *= t1;
			texres->tb *= t1;
		}
		else {
			texres->tr *= sc;
			texres->tg *= sc;
			texres->tb *= sc;
		}
	}

	if(texres->nor!=NULL) {
		float offs= tex->nabla/tex->noisesize;	// also scaling of tmpvec

		/* calculate bumpnormal */
		voronoi(tmpvec[0] + offs, tmpvec[1], tmpvec[2], da, pa, tex->vn_mexp,  tex->vn_distm);
		texres->nor[0] = sc * fabs(tex->vn_w1*da[0] + tex->vn_w2*da[1] + tex->vn_w3*da[2] + tex->vn_w4*da[3]);
		voronoi(tmpvec[0], tmpvec[1] + offs, tmpvec[2], da, pa, tex->vn_mexp,  tex->vn_distm);
		texres->nor[1] = sc * fabs(tex->vn_w1*da[0] + tex->vn_w2*da[1] + tex->vn_w3*da[2] + tex->vn_w4*da[3]);
		voronoi(tmpvec[0], tmpvec[1], tmpvec[2] + offs, da, pa, tex->vn_mexp,  tex->vn_distm);
		texres->nor[2] = sc * fabs(tex->vn_w1*da[0] + tex->vn_w2*da[1] + tex->vn_w3*da[2] + tex->vn_w4*da[3]);

		tex_normal_derivate(tex, texres);
		rv |= TEX_NOR;
	}

	if(tex->vn_coltype) {
		tex_brightness_contrast_rgb(tex, texres);
		texres->ta = 1.0;
		return (rv | TEX_RGB);
	}

	tex_brightness_contrast(tex, texres);

	return rv;
}

/****************************** Noise *******************************/

static int tex_noise_sample(Tex *tex, TexResult *texres)
{
	float div=3.0;
	int val, ran, loop;

	ran= BLI_rand();
	val= (ran & 3);

	loop= tex->noisedepth;
	while(loop--) {
		ran= (ran>>2);
		val*= (ran & 3);
		div*= 3.0;
	}

	texres->tin= ((float)val)/div;;

	tex_brightness_contrast(tex, texres);
	return TEX_INT;
}

/****************************** Plugin *******************************/

static void tex_plugin_init(Render *re, Tex *tex)
{
	int cfra= re->params.r.cfra;

	if(tex->plugin && tex->plugin->doit)
		if(tex->plugin->cfra)
			*(tex->plugin->cfra)= (float)cfra; //frame_to_float(re->db.scene, cfra); // XXX old animsys - timing stuff to be fixed 
}

static int tex_plugin_sample(Tex *tex, float *texvec, float *dxt, float *dyt, int osatex, TexResult *texres)
{
	PluginTex *pit;
	int rgbnor=0;
	float result[ 8 ];

	texres->tin= 0.0;

	pit= tex->plugin;
	if(pit && pit->doit) {
		if(texres->nor) {
			if(pit->version < 6) {
				copy_v3_v3(pit->result+5, texres->nor);
			} else {
				copy_v3_v3(result+5, texres->nor);
			}
		}
		if(pit->version < 6) {
			if(osatex) rgbnor= ((TexDoitold)pit->doit)(tex->stype, 
					pit->data, texvec, dxt, dyt);
			else rgbnor= ((TexDoitold)pit->doit)(tex->stype, 
					pit->data, texvec, 0, 0);
		} else {
			if(osatex) rgbnor= ((TexDoit)pit->doit)(tex->stype, 
					pit->data, texvec, dxt, dyt, result);
			else rgbnor= ((TexDoit)pit->doit)(tex->stype, 
					pit->data, texvec, 0, 0, result);
		}

		if(pit->version < 6) {
			texres->tin = pit->result[0];
		} else {
			texres->tin = result[0];
		}

		if(rgbnor & TEX_NOR) {
			if(texres->nor) {
				if(pit->version < 6) {
					copy_v3_v3(texres->nor, pit->result+5);
				} else {
					copy_v3_v3(texres->nor, result+5);
				}
			}
		}

		if(rgbnor & TEX_RGB) {
			if(pit->version < 6) {
				texres->tr = pit->result[1];
				texres->tg = pit->result[2];
				texres->tb = pit->result[3];
				texres->ta = pit->result[4];
			} else {
				texres->tr = result[1];
				texres->tg = result[2];
				texres->tb = result[3];
				texres->ta = result[4];
			}

			tex_brightness_contrast_rgb(tex, texres);
		}

		tex_brightness_contrast(tex, texres);
	}

	return rgbnor;
}

/*************************** Image ***************************/

static int tex_image_sample(RenderParams *rpm, Tex *tex, float *texvec, float *dxt, float *dyt, int osatex, TexResult *texres)
{
	int retval;

	if(osatex)
		retval= imagewraposa(rpm, tex, tex->ima, NULL, texvec, dxt, dyt, texres);
	else
		retval= imagewrap(rpm, tex, tex->ima, NULL, texvec, texres); 

	tag_image_time(tex->ima); /* tag image as having being used */

	return retval;
}

#if 0
static int tex_uvimage_sample(RenderParams *rpm, Tex *tex, float *texvec, float *dxt, float *dyt, int osatex, TexResult *texres, short thread)
{
	static Tex imatex[BLENDER_MAX_THREADS];
	static int firsttime= 1;

	Tex *uvtex;
	int a;

	if(rpm->r.scemode & R_NO_TEX) return;

	if(firsttime) {
		/* threadsafe init, hacky .. */
		BLI_lock_thread(LOCK_IMAGE);
		if(firsttime) {
			for(a=0; a<BLENDER_MAX_THREADS; a++) {
				memset(&imatex[a], 0, sizeof(Tex));
				default_tex(&imatex[a]);
				imatex[a].type= TEX_IMAGE;
			}

			firsttime= 0;
		}
		BLI_unlock_thread(LOCK_IMAGE);
	}
	
	uvtex= &imatex[thread];
	uvtex->iuser.ok= ima->ok;

	return tex_image_sample(rpm, uvtex, texvec, dxt, dyt, osatex, texres);
}
#endif

/**************************** Nodes ******************************/

static int tex_nodes_sample(RenderParams *rpm, Tex *tex, float *texvec, float *dxt, float *dyt, int osatex, TexResult *texres, short thread, short which_output)
{
	bNodeTree *nodes = tex->nodetree;
	int retval = TEX_INT;

	retval = ntreeTexExecTree(nodes, texres, texvec, (osatex)? dxt: NULL, (osatex)? dyt: NULL, osatex,
		thread, tex, which_output, rpm->r.cfra, (rpm->r.scemode & R_TEXNODE_PREVIEW) != 0, NULL, NULL);

	if(texres->nor) retval |= TEX_NOR;
	retval |= TEX_RGB;

	return retval;
}

/*************************** Sample ******************************/

int tex_sample(RenderParams *rpm, Tex *tex, TexCoord *texco, TexResult *texres, short thread, short which_output)
{
	float *texvec= texco->co;
	float *dxt= texco->dx;
	float *dyt= texco->dy;
	int osatex= texco->osatex;
	int retval;

	texres->talpha= 0;	/* is set when image texture returns alpha (considered premul) */

	switch(tex->type) {
		case TEX_CLOUDS:
			retval= tex_clouds_sample(tex, texvec, texres);
			break;
		case TEX_WOOD:
			retval= tex_wood_sample(tex, texvec, texres); 
			break;
		case TEX_MARBLE:
			retval= tex_marble_sample(tex, texvec, texres); 
			break;
		case TEX_MAGIC:
			retval= tex_magic_sample(tex, texvec, texres); 
			break;
		case TEX_BLEND:
			retval= tex_blend_sample(tex, texvec, texres);
			break;
		case TEX_STUCCI:
			retval= tex_stucci_sample(tex, texvec, texres); 
			break;
		case TEX_NOISE:
			retval= tex_noise_sample(tex, texres); 
			break;
		case TEX_IMAGE:
			retval= tex_image_sample(rpm, tex, texvec, dxt, dyt, osatex, texres); 
			break;
		case TEX_PLUGIN:
			retval= tex_plugin_sample(tex, texvec, dxt, dyt, osatex, texres);
			break;
		case TEX_ENVMAP:
			retval= tex_envmap_sample(rpm, tex, texvec, dxt, dyt, osatex, texres);
			break;
		case TEX_MUSGRAVE:
			retval= tex_musgrave_sample(tex, texvec, texres);
			break;
			/* newnoise: voronoi type */
		case TEX_VORONOI:
			retval= tex_voronoi_sample(tex, texvec, texres);
			break;
		case TEX_DISTNOISE:
			retval= tex_distnoise_sample(tex, texvec, texres);
			break;
		case TEX_POINTDENSITY:
			retval= tex_pointdensity_sample(rpm, tex, texvec, texres);
			break;
		case TEX_VOXELDATA:
			retval= tex_voxeldata_sample(tex, texvec, texres);  
			break;
		case TEX_NODES:
			retval= tex_nodes_sample(rpm, tex, texvec, dxt, dyt, osatex, texres, thread, which_output);
			break;
		default:
			texres->tin= 0.0f;
			retval= 0;
			break;
	}

	if(tex->flag & TEX_COLORBAND) {
		float col[4];

		if(do_colorband(tex->coba, texres->tin, col)) {
			texres->talpha= 1;
			texres->tr= col[0];
			texres->tg= col[1];
			texres->tb= col[2];
			texres->ta= col[3];
			retval |= TEX_RGB;
		}
	}

	return retval;
}

/*************************** Init/End ***************************/

void tex_init(Render *re, Tex *tex)
{
	int cfra= re->params.r.cfra;
	
	switch(tex->type) {
		case TEX_PLUGIN:
			tex_plugin_init(re, tex);
			break;
		case TEX_ENVMAP:
			tex_envmap_init(re, tex);
			break;
		case TEX_VOXELDATA:
			tex_voxeldata_init(re, tex);
			break;
		case TEX_POINTDENSITY:
			tex_pointdensity_init(re, tex);
			break;
		case TEX_NODES:
			ntreeBeginExecTree(tex->nodetree); /* has internal flag to detect it only does it once */
			break;
	}

	/* imap test */
	if(tex->ima && ELEM(tex->ima->source, IMA_SRC_MOVIE, IMA_SRC_SEQUENCE))
		BKE_image_user_calc_frame(&tex->iuser, cfra, (re)? re->params.flag & R_SEC_FIELD: 0);
}

void tex_free(Render *re, Tex *tex)
{
	switch(tex->type) {
		case TEX_VOXELDATA:
			tex_voxeldata_free(re, tex);
			break;
		case TEX_POINTDENSITY:
			tex_pointdensity_free(re, tex);
			break;
		case TEX_NODES:
			ntreeEndExecTree(tex->nodetree);
			break;
	}
}

/************************ List Init/End ********************************/

void tex_list_init(Render *re, ListBase *lb)
{
	Tex *tex;
	
	/* TODO should be doing only textures used in this render */
	for(tex= lb->first; tex; tex=tex->id.next)
		if(tex->id.us)
			tex_init(re, tex);
}

void tex_list_free(Render *re, ListBase *lb)
{
	Tex *tex;

	for(tex= lb->first; tex; tex=tex->id.next)
		if(tex->id.us)
			tex_free(re, tex);
}


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
 * Contributors: 2004/2005/2006 Blender Foundation, full recode
 *
 * ***** END GPL LICENSE BLOCK *****
 */


#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_jitter.h"
#include "BLI_math.h"
#include "BLI_rand.h"

/* own includes */
#include "pixelfilter.h"
#include "render_types.h"
#include "result.h"

/* some defines replacing magic numbers to make code more understandable,
   note however that these can't just be increased without breaking things */
#define FILTER_PIXEL_WIDTH	3
#define FILTER_PIXEL_AREA	(FILTER_PIXEL_WIDTH*FILTER_PIXEL_WIDTH)
#define FILTER_PIXEL_OFFSET	(-FILTER_PIXEL_WIDTH/2)
#define FILTER_MASK_BYTES	(RE_MAX_OSA/8)
#define	FILTER_MASK_BITS	(RE_MAX_OSA)

typedef struct SampleTables
{
	float centLut[16];
	float *fmask[FILTER_MASK_BYTES][FILTER_PIXEL_AREA];
	char cmask[256], *centmask;
	
} SampleTables;

/****************************** Filters *********************************/

static float filt_quadratic(float x)
{
    if (x <  0.0f) x = -x;
    if (x < 0.5f) return 0.75f-(x*x);
    if (x < 1.5f) return 0.50f*(x-1.5f)*(x-1.5f);
    return 0.0f;
}


static float filt_cubic(float x)
{
	float x2= x*x;
	
    if (x <  0.0f) x = -x;
	
    if (x < 1.0f) return 0.5*x*x2 - x2 + 2.0f/3.0f;
    if (x < 2.0f) return (2.0-x)*(2.0-x)*(2.0-x)/6.0f;
    return 0.0f;
}


static float filt_catrom(float x)
{
	float x2= x*x;
	
    if (x <  0.0f) x = -x;
    if (x < 1.0f) return  1.5f*x2*x - 2.5f*x2  + 1.0f;
    if (x < 2.0f) return -0.5f*x2*x + 2.5*x2 - 4.0f*x + 2.0f;
    return 0.0f;
}

static float filt_mitchell(float x)	/* Mitchell & Netravali's two-param cubic */
{
	float b = 1.0f/3.0f, c = 1.0f/3.0f;
	float p0 = (  6.0 -  2.0*b         ) / 6.0;
	float p2 = (-18.0 + 12.0*b +  6.0*c) / 6.0;
	float p3 = ( 12.0 -  9.0*b -  6.0*c) / 6.0;
	float q0 = (	   8.0*b + 24.0*c) / 6.0;
	float q1 = (      - 12.0*b - 48.0*c) / 6.0;
	float q2 = (         6.0*b + 30.0*c) / 6.0;
	float q3 = (       -     b -  6.0*c) / 6.0;

	if (x<-2.0) return 0.0;
	if (x<-1.0) return (q0-x*(q1-x*(q2-x*q3)));
	if (x< 0.0) return (p0+x*x*(p2-x*p3));
	if (x< 1.0) return (p0+x*x*(p2+x*p3));
	if (x< 2.0) return (q0+x*(q1+x*(q2+x*q3)));
	return 0.0;
}

/* x ranges from -1 to 1 */
float RE_filter_value(int type, float x, float gaussfac)
{
	x= ABS(x);
	
	switch(type) {
		case R_FILTER_BOX:
			if(x>1.0) return 0.0f;
			return 1.0;
			
		case R_FILTER_TENT:
			if(x>1.0) return 0.0f;
			return 1.0f-x;
			
		case R_FILTER_GAUSS:
			x*= gaussfac;
			return (1.0/exp(x*x) - 1.0/exp(gaussfac*gaussfac*2.25));
			
		case R_FILTER_MITCH:
			return filt_mitchell(x*gaussfac);
			
		case R_FILTER_QUAD:
			return filt_quadratic(x*gaussfac);
			
		case R_FILTER_CUBIC:
			return filt_cubic(x*gaussfac);
			
		case R_FILTER_CATROM:
			return filt_catrom(x*gaussfac);
	}
	return 0.0f;
}

static float calc_weight(Render *re, float *weight, int i, int j)
{
	float x, y, dist, totw= 0.0;
	int a;

	for(a=0; a<re->params.osa; a++) {
		x= re->sample.jit[a][0] + i;
		y= re->sample.jit[a][1] + j;
		dist= sqrt(x*x+y*y);

		weight[a]= 0.0;

		/* Weighting choices */
		switch(re->params.r.filtertype) {
		case R_FILTER_BOX:
			if(i==0 && j==0) weight[a]= 1.0;
			break;
			
		case R_FILTER_TENT:
			if(dist < re->params.r.gauss)
				weight[a]= re->params.r.gauss - dist;
			break;
			
		case R_FILTER_GAUSS:
		case R_FILTER_MITCH:
		case R_FILTER_QUAD:
		case R_FILTER_CUBIC:
		case R_FILTER_CATROM:
			weight[a]= RE_filter_value(re->params.r.filtertype, dist, re->params.r.gauss);
			break;
		}
		
		totw+= weight[a];

	}
	return totw;
}

/**************************** Table Lookups ********************************/

static int count_bits_u16(SampleTables *st, unsigned short value)
{
	return (st->cmask[value & 255] + st->cmask[value>>8]);
}

int pxf_mask_count(RenderSampleData *rsd, unsigned short mask)
{
	/* number of samples in this mask */
	return (rsd->table)? count_bits_u16(rsd->table, mask): 0;
}

void pxf_mask_offset(RenderSampleData *rsd, unsigned short mask, float ofs[2])
{
	if(rsd->table) {
		/* averaged offset of samples in mask inside pixel */
		SampleTables *st= rsd->table;
		short b= st->centmask[mask];

		ofs[0]= st->centLut[b&15] + 0.5f;
		ofs[1]= st->centLut[b>>4] + 0.5f;
	}
	else {
		ofs[0]= 0.5f;
		ofs[1]= 0.5f;
	}
}

void pxf_sample_offset(RenderSampleData *rsd, int sample, float ofs[2])
{
	/* offset of sample within pixel */
	ofs[0]= rsd->jit[sample][0] + 0.5f;
	ofs[1]= rsd->jit[sample][1] + 0.5f;
}

float (*pxf_sample_offset_table(Render *re))[2]
{
	/* current jitter table to lookup sample offset */
	if(re->params.osa)
		return re->sample.jit;
	else if(re->cb.i.curblur)
		return &re->sample.mblur_jit[re->cb.i.curblur-1];
	else
		return NULL;
}

static float pxf_mask_weight(SampleTables *st, int a, int mask)
{
	/* weights are split up into lower 8 and upper 8 bits/samples,
	   to keep the lookup tables sufficiently small */
	return (st->fmask[0][a][mask & 255] + st->fmask[1][a][mask >> 8]);
}

/************************* Init/Free ****************************/

static void pxf_init_jit(RenderSampleData *rsd, int osa, int mblur_samples)
{
	static float jit[32][2];	/* simple caching */
	static float mblur_jit[32][2];  /* simple caching */
	static int lastjit= 0;
	static int last_mblur_jit= 0;
	
	/* XXX not thread safe */
	if(lastjit!=osa || last_mblur_jit != mblur_samples) {
		memset(jit, 0, sizeof(jit));
		BLI_initjit(jit[0], osa);

		memset(mblur_jit, 0, sizeof(mblur_jit));
		BLI_initjit(mblur_jit[0], mblur_samples);
	}
	
	lastjit= osa;
	memcpy(rsd->jit, jit, sizeof(jit));

	last_mblur_jit= mblur_samples;
	memcpy(rsd->mblur_jit, mblur_jit, sizeof(mblur_jit));
}

/* based on settings in render, it makes the lookup tables */
static void pxf_init_table(Render *re)
{
	RenderSampleData *rsd= &re->sample;
	SampleTables *st;
	float flweight[RE_MAX_OSA], *fm;
	float *fpx[FILTER_MASK_BYTES], *fpy[FILTER_MASK_BYTES];
	float weight[RE_MAX_OSA], totw, val;
	int i, j, a, b, c, osa= re->params.osa;
	
	st= rsd->table= MEM_callocN(sizeof(SampleTables), "sample tables");

	/* cmask: map byte to number of enabled bits in that byte */
	for(a=0; a<256; a++) {
		st->cmask[a]= 0;

		for(b=0; b<8; b++)
			if(a & (1<<b))
				st->cmask[a]++;
	}

	/* fmask weight for a'th 8 samples on pixel offset b */
	
	for(a=0; a<FILTER_PIXEL_AREA;a++)
		for(c=0; c<FILTER_MASK_BYTES; c++)
			st->fmask[c][a]= MEM_callocN(256*sizeof(float), "initfilt");

	/* calculate totw */
	totw= 0.0;
	for(j= 0; j<FILTER_PIXEL_WIDTH; j++)
		for(i= 0; i<FILTER_PIXEL_WIDTH; i++)
			totw+= calc_weight(re, weight, i+FILTER_PIXEL_OFFSET, j+FILTER_PIXEL_OFFSET);

	for(j= 0; j<FILTER_PIXEL_WIDTH; j++) {
		for(i= 0; i<FILTER_PIXEL_WIDTH; i++) {
			/* calculate using jit, with offset the weights */

			memset(weight, 0, sizeof(weight));
			calc_weight(re, weight, i+FILTER_PIXEL_OFFSET, j+FILTER_PIXEL_OFFSET);

			for(a=0; a<RE_MAX_OSA; a++)
				flweight[a]= weight[a]*(1.0/totw);

			for(c=0; c<FILTER_MASK_BYTES; c++) {
				fm= st->fmask[c][FILTER_PIXEL_WIDTH*j + i];

				for(a=0; a<256; a++)
					for(b=0; b<8; b++)
						if(a & (1<<b))
							fm[a]+= flweight[b + 8*c];
			}
		}
	}

	/* centmask: the correct subpixel offset per mask */

	for(c=0; c<FILTER_MASK_BYTES; c++) {
		fpx[c]= MEM_callocN(256*sizeof(float), "initgauss4");
		fpy[c]= MEM_callocN(256*sizeof(float), "initgauss4");

		for(a=0; a<256; a++) {
			for(b=0; b<8; b++) {
				if(a & (1<<b)) {
					fpx[c][a]+= rsd->jit[b + 8*c][0];
					fpy[c][a]+= rsd->jit[b + 8*c][1];
				}
			}
		}
	}

	st->centmask= MEM_mallocN((1<<osa), "Initfilt3");
	
	for(a=0; a<RE_MAX_OSA; a++)
		st->centLut[a]= -0.45+((float)a)/RE_MAX_OSA;

	for(a= (1<<osa)-1; a>0; a--) {
		val= count_bits_u16(st, a);
		i= 8+(15.9*(fpy[0][a & 255]+fpy[1][a>>8])/val);
		CLAMP(i, 0, 15);
		j= 8+(15.9*(fpx[0][a & 255]+fpx[1][a>>8])/val);
		CLAMP(j, 0, 15);
		i= j + (i<<4);
		st->centmask[a]= i;
	}

	for(c=0; c<FILTER_MASK_BYTES; c++) {
		MEM_freeN(fpx[c]);
		MEM_freeN(fpy[c]);
	}
}

static void pxf_free_table(RenderSampleData *rsd)
{
	int a, b;
	
	if(rsd->table) {
		for(a=0; a<FILTER_PIXEL_AREA; a++)
			for(b=0; b<FILTER_MASK_BYTES; b++)
				MEM_freeN(rsd->table->fmask[b][a]);
		
		MEM_freeN(rsd->table->centmask);
		MEM_freeN(rsd->table);
		rsd->table= NULL;
	}
}

void pxf_init(Render *re)
{
	pxf_free(re);

	/* needed for mblur too */
	pxf_init_jit(&re->sample, re->params.osa, re->params.r.mblur_samples);

	if(re->params.osa)
		pxf_init_table(re);
	else /* just prevents cpu cycles for larger render and copying */
		re->params.r.filtertype= 0;
}

void pxf_free(Render *re)
{
	pxf_free_table(&re->sample);
}

/************************ Pixel Blending ***************************/

/* ------------------------------------------------------------------------- */
/* Debug/behaviour defines                                                   */
/* if defined: alpha blending with floats clips color, as with shorts        */
/* #define RE_FLOAT_COLOR_CLIPPING  */
/* if defined: alpha values are clipped                                      */
/* For now, we just keep alpha clipping. We run into thresholding and        */
/* blending difficulties otherwise. Be careful here.                         */
#define RE_ALPHA_CLIPPING

/* Threshold for a 'full' pixel: pixels with alpha above this level are      */
/* considered opaque This is the decimal value for 0xFFF0 / 0xFFFF           */
#define RE_FULL_COLOR_FLOAT 0.9998
/* Threshold for an 'empty' pixel: pixels with alpha above this level are    */
/* considered completely transparent. This is the decimal value              */
/* for 0x000F / 0xFFFF                                                       */
#define RE_EMPTY_COLOR_FLOAT 0.0002

/* ------------------------------------------------------------------------- */

void pxf_add_alpha_over(float dest[4], float source[4])
{
    /* d = s + (1-alpha_s)d*/
    float mul;
    
	mul= 1.0 - source[3];

	dest[0]= (mul*dest[0]) + source[0];
	dest[1]= (mul*dest[1]) + source[1];
	dest[2]= (mul*dest[2]) + source[2];
	dest[3]= (mul*dest[3]) + source[3];

}

void pxf_add_alpha_under(float dest[4], float source[4])
{
    float mul;

	mul= 1.0 - dest[3];

	dest[0]+= (mul*source[0]);
	dest[1]+= (mul*source[1]);
	dest[2]+= (mul*source[2]);
	dest[3]+= (mul*source[3]);
} 

void pxf_add_alpha_fac(float dest[4], float source[4], char addfac)
{
    float m; /* weiging factor of destination */
    float c; /* intermediate color           */

    /* Addfac is a number between 0 and 1: rescale */
    /* final target is to diminish the influence of dest when addfac rises */
    m = 1.0 - ( source[3] * ((255.0 - addfac) / 255.0));

    /* blend colors*/
    c= (m * dest[0]) + source[0];
#ifdef RE_FLOAT_COLOR_CLIPPING
    if(c >= RE_FULL_COLOR_FLOAT) dest[0] = RE_FULL_COLOR_FLOAT; 
    else 
#endif
        dest[0]= c;
   
    c= (m * dest[1]) + source[1];
#ifdef RE_FLOAT_COLOR_CLIPPING
    if(c >= RE_FULL_COLOR_FLOAT) dest[1] = RE_FULL_COLOR_FLOAT; 
    else 
#endif
        dest[1]= c;
    
    c= (m * dest[2]) + source[2];
#ifdef RE_FLOAT_COLOR_CLIPPING
    if(c >= RE_FULL_COLOR_FLOAT) dest[2] = RE_FULL_COLOR_FLOAT; 
    else 
#endif
        dest[2]= c;

	c= (m * dest[3]) + source[3];
#ifdef RE_ALPHA_CLIPPING
	if(c >= RE_FULL_COLOR_FLOAT) dest[3] = RE_FULL_COLOR_FLOAT; 
	else 
#endif
       dest[3]= c;

}

void pxf_add_alpha_over_mask(RenderSampleData *rsd, float *dest, float *source, unsigned short dmask, unsigned short smask)
{
	unsigned short shared= dmask & smask;
	float mul= 1.0 - source[3];
	
	if(shared) {	/* overlapping masks */
		
		/* masks differ, we make a mixture of 'add' and 'over' */
		if(shared!=dmask) {
			float shared_bits= (float)pxf_mask_count(rsd, shared);		/* alpha over */
			float tot_bits= (float)pxf_mask_count(rsd, smask|dmask);		/* alpha add */
			
			float add= (tot_bits - shared_bits)/tot_bits;		/* add level */
			mul= add + (1.0f-add)*mul;
		}
	}
	else if(dmask && smask) {
		/* works for premul only, of course */
		dest[0]+= source[0];
		dest[1]+= source[1];
		dest[2]+= source[2];
		dest[3]+= source[3];
		
		return;
 	}

	dest[0]= (mul*dest[0]) + source[0];
	dest[1]= (mul*dest[1]) + source[1];
	dest[2]= (mul*dest[2]) + source[2];
	dest[3]= (mul*dest[3]) + source[3];
}

/***************************** Filtered Blending ****************************/

/* filtered adding to scanlines */
void pxf_add_filtered(RenderSampleData *rsd, unsigned short mask, float *col, float *rowbuf, int row_w)
{
	/* calc the value of mask */
	SampleTables *st= rsd->table;
	float *rb1, *rb2, *rb3;
	float val, r, g, b, al;
	unsigned int a;
	int j;
	
	r= col[0];
	g= col[1];
	b= col[2];
	al= col[3];
	
	rb2= rowbuf-4;
	rb3= rb2-4*row_w;
	rb1= rb2+4*row_w;
	
	for(j=2; j>=0; j--) {
		
		a= j;
		
		val= pxf_mask_weight(st, a, mask);
		if(val!=0.0) {
			rb1[0]+= val*r;
			rb1[1]+= val*g;
			rb1[2]+= val*b;
			rb1[3]+= val*al;
		}
		a+=3;
		
		val= pxf_mask_weight(st, a, mask);
		if(val!=0.0) {
			rb2[0]+= val*r;
			rb2[1]+= val*g;
			rb2[2]+= val*b;
			rb2[3]+= val*al;
		}
		a+=3;
		
		val= pxf_mask_weight(st, a, mask);
		if(val!=0.0) {
			rb3[0]+= val*r;
			rb3[1]+= val*g;
			rb3[2]+= val*b;
			rb3[3]+= val*al;
		}
		
		rb1+= 4;
		rb2+= 4;
		rb3+= 4;
	}
}


void pxf_add_filtered_pixsize(RenderSampleData *rsd, unsigned short mask, float *in, float *rowbuf, int row_w, int pixsize)
{
	/* calc the value of mask */
	SampleTables *st= rsd->table;
	float *rb1, *rb2, *rb3;
	float val;
	unsigned int a;
	int i, j;
	
	rb2= rowbuf-pixsize;
	rb3= rb2-pixsize*row_w;
	rb1= rb2+pixsize*row_w;
	
	for(j=2; j>=0; j--) {
		
		a= j;
		
		val= pxf_mask_weight(st, a, mask);
		if(val!=0.0) {
			for(i= 0; i<pixsize; i++)
				rb1[i]+= val*in[i];
		}
		a+=3;
		
		val= pxf_mask_weight(st, a, mask);
		if(val!=0.0) {
			for(i= 0; i<pixsize; i++)
				rb2[i]+= val*in[i];
		}
		a+=3;
		
		val= pxf_mask_weight(st, a, mask);
		if(val!=0.0) {
			for(i= 0; i<pixsize; i++)
				rb3[i]+= val*in[i];
		}
		
		rb1+= pixsize;
		rb2+= pixsize;
		rb3+= pixsize;
	}
}

/* FSA Accumulation Helpers
 *
 * index ordering, scanline based:
 *
 *  ---    ---   ---  
 * | 2,0 | 2,1 | 2,2 |
 *  ---    ---   ---  
 * | 1,0 | 1,1 | 1,2 |
 *  ---    ---   ---  
 * | 0,0 | 0,1 | 0,2 |
 *  ---    ---   ---  
*/

void pxf_mask_table(RenderSampleData *rsd, unsigned short mask, float filt[3][3])
{
	SampleTables *st= rsd->table;
	int a, j;
	
	for(j=2; j>=0; j--) {
		a= j;
		filt[2][2-j]= pxf_mask_weight(st, a, mask);

		a+=3;
		filt[1][2-j]= pxf_mask_weight(st, a, mask);
		
		a+=3;
		filt[0][2-j]= pxf_mask_weight(st, a, mask);
	}
}

void pxf_add_filtered_table(float filt[][3], float *col, float *rowbuf, int row_w, int col_h, int x, int y)
{
	float *fpoin[3][3];
	float val, r, g, b, al, lfilt[3][3];
	
	r= col[0];
	g= col[1];
	b= col[2];
	al= col[3];
	
	memcpy(lfilt, filt, sizeof(lfilt));
	
	fpoin[0][1]= rowbuf-4*row_w;
	fpoin[1][1]= rowbuf;
	fpoin[2][1]= rowbuf+4*row_w;
	
	fpoin[0][0]= fpoin[0][1] - 4;
	fpoin[1][0]= fpoin[1][1] - 4;
	fpoin[2][0]= fpoin[2][1] - 4;
	
	fpoin[0][2]= fpoin[0][1] + 4;
	fpoin[1][2]= fpoin[1][1] + 4;
	fpoin[2][2]= fpoin[2][1] + 4;
	
	if(y==0) {
		fpoin[0][0]= fpoin[1][0];
		fpoin[0][1]= fpoin[1][1];
		fpoin[0][2]= fpoin[1][2];
		/* filter needs the opposite value yes! */
		lfilt[0][0]= filt[2][0];
		lfilt[0][1]= filt[2][1];
		lfilt[0][2]= filt[2][2];
	}
	else if(y==col_h-1) {
		fpoin[2][0]= fpoin[1][0];
		fpoin[2][1]= fpoin[1][1];
		fpoin[2][2]= fpoin[1][2];
		
		lfilt[2][0]= filt[0][0];
		lfilt[2][1]= filt[0][1];
		lfilt[2][2]= filt[0][2];
	}
	
	if(x==0) {
		fpoin[2][0]= fpoin[2][1];
		fpoin[1][0]= fpoin[1][1];
		fpoin[0][0]= fpoin[0][1];
		
		lfilt[2][0]= filt[2][2];
		lfilt[1][0]= filt[1][2];
		lfilt[0][0]= filt[0][2];
	}
	else if(x==row_w-1) {
		fpoin[2][2]= fpoin[2][1];
		fpoin[1][2]= fpoin[1][1];
		fpoin[0][2]= fpoin[0][1];
		
		lfilt[2][2]= filt[2][0];
		lfilt[1][2]= filt[1][0];
		lfilt[0][2]= filt[0][0];
	}
	
	/* loop unroll */
#define MASKFILT(i, j) \
	val= lfilt[i][j]; \
	\
	if(val!=0.0f) { \
		float *fp= fpoin[i][j]; \
		fp[0]+= val*r; \
		fp[1]+= val*g; \
		fp[2]+= val*b; \
		fp[3]+= val*al; \
	}
	
	MASKFILT(0, 0)
	MASKFILT(0, 1)
	MASKFILT(0, 2)
	MASKFILT(1, 0)
	MASKFILT(1, 1)
	MASKFILT(1, 2)
	MASKFILT(2, 0)
	MASKFILT(2, 1)
	MASKFILT(2, 2)
}


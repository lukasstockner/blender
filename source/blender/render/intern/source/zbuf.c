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
 * Contributors: Hos, RPW
 *               2004-2006 Blender Foundation, full recode
 *
 * ***** END GPL LICENSE BLOCK *****
 */


/*---------------------------------------------------------------------------*/
/* Common includes                                                           */
/*---------------------------------------------------------------------------*/

#include <math.h>
#include <float.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_jitter.h"
#include "BLI_math.h"
#include "BLI_threads.h"

#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_node_types.h"

#include "BKE_global.h"
#include "BKE_material.h"
#include "BKE_utildefines.h"

#include "RE_render_ext.h"

/* local includes */
#include "camera.h"
#include "database.h"
#include "lamp.h"
#include "object.h"
#include "object_mesh.h"
#include "object_strand.h"
#include "part.h"
#include "pixelfilter.h"
#include "render_types.h"
#include "rendercore.h"
#include "shading.h"
#include "shadowbuf.h"
#include "sss.h"
#include "zbuf.h"

/* ****************** Spans ******************************* */

/* each zbuffer has coordinates transformed to local rect coordinates, so we can simply clip */
void zbuf_alloc_span(ZSpan *zspan, int rectx, int recty, float clipcrop)
{
	memset(zspan, 0, sizeof(ZSpan));
	
	zspan->rectx= rectx;
	zspan->recty= recty;
	
	zspan->span1= MEM_mallocN(recty*sizeof(float), "zspan");
	zspan->span2= MEM_mallocN(recty*sizeof(float), "zspan");

	zspan->clipcrop= clipcrop;
}

void zbuf_free_span(ZSpan *zspan)
{
	if(zspan) {
		if(zspan->span1) MEM_freeN(zspan->span1);
		if(zspan->span2) MEM_freeN(zspan->span2);
		zspan->span1= zspan->span2= NULL;
	}
}

/* reset range for clipping */
void zbuf_init_span(ZSpan *zspan)
{
	zspan->miny1= zspan->miny2= zspan->recty+1;
	zspan->maxy1= zspan->maxy2= -1;
	zspan->minp1= zspan->maxp1= zspan->minp2= zspan->maxp2= NULL;
}

void zbuf_add_to_span(ZSpan *zspan, float *v1, float *v2)
{
	float *minv, *maxv, *span;
	float xx1, dx0, xs0;
	int y, my0, my2;
	
	if(v1[1]<v2[1]) {
		minv= v1; maxv= v2;
	}
	else {
		minv= v2; maxv= v1;
	}
	
	my0= ceilf(minv[1]);
	my2= floorf(maxv[1]);
	
	if(my2<0 || my0>= zspan->recty) return;
	
	/* clip top */
	if(my2>=zspan->recty) my2= zspan->recty-1;
	/* clip bottom */
	if(my0<0) my0= 0;
	
	if(my0>my2) return;
	/* if(my0>my2) should still fill in, that way we get spans that skip nicely */
	
	xx1= maxv[1]-minv[1];
	if(xx1>FLT_EPSILON) {
		dx0= (minv[0]-maxv[0])/xx1;
		xs0= dx0*(minv[1]-my2) + minv[0];
	}
	else {
		dx0= 0.0f;
		xs0= MIN2(minv[0],maxv[0]);
	}
	
	/* empty span */
	if(zspan->maxp1 == NULL) {
		span= zspan->span1;
	}
	else {	/* does it complete left span? */
		if( maxv == zspan->minp1 || minv==zspan->maxp1) {
			span= zspan->span1;
		}
		else {
			span= zspan->span2;
		}
	}

	if(span==zspan->span1) {
//		printf("left span my0 %d my2 %d\n", my0, my2);
		if(zspan->minp1==NULL || zspan->minp1[1] > minv[1] ) {
			zspan->minp1= minv;
		}
		if(zspan->maxp1==NULL || zspan->maxp1[1] < maxv[1] ) {
			zspan->maxp1= maxv;
		}
		if(my0<zspan->miny1) zspan->miny1= my0;
		if(my2>zspan->maxy1) zspan->maxy1= my2;
	}
	else {
//		printf("right span my0 %d my2 %d\n", my0, my2);
		if(zspan->minp2==NULL || zspan->minp2[1] > minv[1] ) {
			zspan->minp2= minv;
		}
		if(zspan->maxp2==NULL || zspan->maxp2[1] < maxv[1] ) {
			zspan->maxp2= maxv;
		}
		if(my0<zspan->miny2) zspan->miny2= my0;
		if(my2>zspan->maxy2) zspan->maxy2= my2;
	}

	for(y=my2; y>=my0; y--, xs0+= dx0) {
		/* xs0 is the xcoord! */
		span[y]= xs0;
	}
}

/*-----------------------------------------------------------*/ 
/* Functions                                                 */
/*-----------------------------------------------------------*/ 

static void fillrect(int *rect, int x, int y, int val)
{
	if(val == 0) {
		memset(rect, 0, sizeof(int)*x*y);
	}
	else {
		int a, len;

		len= x*y;
		for(a=0; a<len; a++)
			rect[a]= val;
	}
}

/* based on Liang&Barsky, for clipping of pyramidical volume */
static short cliptestf(float p, float q, float *u1, float *u2)
{
	float r;
	
	if(p<0.0) {
		if(q<p) return 0;
		else if(q<0.0) {
			r= q/p;
			if(r>*u2) return 0;
			else if(r>*u1) *u1=r;
		}
	}
	else {
		if(p>0.0) {
			if(q<0.0) return 0;
			else if(q<p) {
				r= q/p;
				if(r<*u1) return 0;
				else if(r<*u2) *u2=r;
			}
		}
		else if(q<0.0) return 0;
	}
	return 1;
}

/* *************  ACCUMULATION ZBUF ************ */


static APixstr *addpsmainA(ListBase *lb)
{
	APixstrMain *psm;

	psm= MEM_mallocN(sizeof(APixstrMain), "addpsmainA");
	BLI_addtail(lb, psm);
	psm->ps= MEM_callocN(4096*sizeof(APixstr),"pixstr");

	return psm->ps;
}

void free_alpha_pixel_structs(ListBase *lb)
{
	APixstrMain *psm, *psmnext;

	for(psm= lb->first; psm; psm= psmnext) {
		psmnext= psm->next;
		if(psm->ps)
			MEM_freeN(psm->ps);
		MEM_freeN(psm);
	}
}

static APixstr *addpsA(ZSpan *zspan)
{
	/* make new PS */
	if(zspan->apsmcounter==0) {
		zspan->curpstr= addpsmainA(zspan->apsmbase);
		zspan->apsmcounter= 4095;
	}
	else {
		zspan->curpstr++;
		zspan->apsmcounter--;
	}
	return zspan->curpstr;
}

static void zbuffillAc4(ZSpan *zspan, int obi, int zvlnr, float *v1, float *v2, float *v3, float *v4)
{
	APixstr *ap, *apofs, *apn;
	double zxd, zyd, zy0, zverg;
	float x0,y0,z0;
	float x1,y1,z1,x2,y2,z2,xx1;
	float *span1, *span2;
	int *rz, *rm, x, y;
	int sn1, sn2, rectx, *rectzofs, *rectmaskofs, my0, my2, mask;
	
	/* init */
	zbuf_init_span(zspan);
	
	/* set spans */
	zbuf_add_to_span(zspan, v1, v2);
	zbuf_add_to_span(zspan, v2, v3);
	if(v4) {
		zbuf_add_to_span(zspan, v3, v4);
		zbuf_add_to_span(zspan, v4, v1);
	}
	else
		zbuf_add_to_span(zspan, v3, v1);
	
	/* clipped */
	if(zspan->minp2==NULL || zspan->maxp2==NULL) return;

	if(zspan->miny1 < zspan->miny2) my0= zspan->miny2; else my0= zspan->miny1;
	if(zspan->maxy1 > zspan->maxy2) my2= zspan->maxy2; else my2= zspan->maxy1;
	
	if(my2<my0) return;
	
	/* ZBUF DX DY, in floats still */
	x1= v1[0]- v2[0];
	x2= v2[0]- v3[0];
	y1= v1[1]- v2[1];
	y2= v2[1]- v3[1];
	z1= v1[2]- v2[2];
	z2= v2[2]- v3[2];
	x0= y1*z2-z1*y2;
	y0= z1*x2-x1*z2;
	z0= x1*y2-y1*x2;
	
	if(z0==0.0) return;
	
	xx1= (x0*v1[0] + y0*v1[1])/z0 + v1[2];
	
	zxd= -(double)x0/(double)z0;
	zyd= -(double)y0/(double)z0;
	zy0= ((double)my2)*zyd + (double)xx1;
	
	/* start-offset in rect */
	rectx= zspan->rectx;
	rectzofs= (int *)(zspan->arectz+rectx*(my2));
	rectmaskofs= (int *)(zspan->rectmask+rectx*(my2));
	apofs= (zspan->apixbuf+ rectx*(my2));
	mask= zspan->mask;

	/* correct span */
	sn1= (my0 + my2)/2;
	if(zspan->span1[sn1] < zspan->span2[sn1]) {
		span1= zspan->span1+my2;
		span2= zspan->span2+my2;
	}
	else {
		span1= zspan->span2+my2;
		span2= zspan->span1+my2;
	}
	
	for(y=my2; y>=my0; y--, span1--, span2--) {
		
		sn1= floorf(*span1);
		sn2= floorf(*span2);
		sn1++; 
		
		if(sn2>=rectx) sn2= rectx-1;
		if(sn1<0) sn1= 0;
		
		if(sn2>=sn1) {
			int intzverg;
			
			zverg= (double)sn1*zxd + zy0;
			rz= rectzofs+sn1;
			rm= rectmaskofs+sn1;
			ap= apofs+sn1;
			x= sn2-sn1;
			
			zverg-= zspan->polygon_offset;
			
			while(x>=0) {
				intzverg= (int)CLAMPIS(zverg, INT_MIN, INT_MAX);

				if( intzverg < *rz) {
					if(!zspan->rectmask || intzverg > *rm) {
						
						apn= ap;
						while(apn) {
							if(apn->p[0]==0) {apn->obi[0]= obi; apn->p[0]= zvlnr; apn->z[0]= intzverg; apn->mask[0]= mask; break; }
							if(apn->p[0]==zvlnr && apn->obi[0]==obi) {apn->mask[0]|= mask; break; }
							if(apn->p[1]==0) {apn->obi[1]= obi; apn->p[1]= zvlnr; apn->z[1]= intzverg; apn->mask[1]= mask; break; }
							if(apn->p[1]==zvlnr && apn->obi[1]==obi) {apn->mask[1]|= mask; break; }
							if(apn->p[2]==0) {apn->obi[2]= obi; apn->p[2]= zvlnr; apn->z[2]= intzverg; apn->mask[2]= mask; break; }
							if(apn->p[2]==zvlnr && apn->obi[2]==obi) {apn->mask[2]|= mask; break; }
							if(apn->p[3]==0) {apn->obi[3]= obi; apn->p[3]= zvlnr; apn->z[3]= intzverg; apn->mask[3]= mask; break; }
							if(apn->p[3]==zvlnr && apn->obi[3]==obi) {apn->mask[3]|= mask; break; }
							if(apn->next==NULL) apn->next= addpsA(zspan);
							apn= apn->next;
						}				
					}
				}
				zverg+= zxd;
				rz++; 
				rm++;
				ap++; 
				x--;
			}
		}
		
		zy0-=zyd;
		rectzofs-= rectx;
		rectmaskofs-= rectx;
		apofs-= rectx;
	}
}



static void zbuflineAc(ZSpan *zspan, int obi, int zvlnr, float *vec1, float *vec2)
{
	APixstr *ap, *apn;
	int *rectz, *rectmask;
	int start, end, x, y, oldx, oldy, ofs;
	int dz, vergz, mask, maxtest=0;
	float dx, dy;
	float v1[3], v2[3];
	
	dx= vec2[0]-vec1[0];
	dy= vec2[1]-vec1[1];
	
	mask= zspan->mask;
	
	if(fabs(dx) > fabs(dy)) {

		/* all lines from left to right */
		if(vec1[0]<vec2[0]) {
			copy_v3_v3(v1, vec1);
			copy_v3_v3(v2, vec2);
		}
		else {
			copy_v3_v3(v2, vec1);
			copy_v3_v3(v1, vec2);
			dx= -dx; dy= -dy;
		}

		start= floorf(v1[0]);
		end= start+floorf(dx);
		if(end>=zspan->rectx) end= zspan->rectx-1;
		
		oldy= floorf(v1[1]);
		dy/= dx;
		
		vergz= v1[2];
		vergz-= zspan->polygon_offset;
		dz= (v2[2]-v1[2])/dx;
		if(vergz>0x50000000 && dz>0) maxtest= 1;		// prevent overflow
		
		rectz= (int *)(zspan->arectz+zspan->rectx*(oldy) +start);
		rectmask= (int *)(zspan->rectmask+zspan->rectx*(oldy) +start);
		ap= (zspan->apixbuf+ zspan->rectx*(oldy) +start);

		if(dy<0) ofs= -zspan->rectx;
		else ofs= zspan->rectx;
		
		for(x= start; x<=end; x++, rectz++, rectmask++, ap++) {
			
			y= floorf(v1[1]);
			if(y!=oldy) {
				oldy= y;
				rectz+= ofs;
				rectmask+= ofs;
				ap+= ofs;
			}
			
			if(x>=0 && y>=0 && y<zspan->recty) {
				if(vergz<*rectz) {
					if(!zspan->rectmask || vergz>*rectmask) {
					
						apn= ap;
						while(apn) {	/* loop unrolled */
							if(apn->p[0]==0) {apn->obi[0]= obi; apn->p[0]= zvlnr; apn->z[0]= vergz; apn->mask[0]= mask; break; }
							if(apn->p[0]==zvlnr && apn->obi[0]==obi) {apn->mask[0]|= mask; break; }
							if(apn->p[1]==0) {apn->obi[1]= obi; apn->p[1]= zvlnr; apn->z[1]= vergz; apn->mask[1]= mask; break; }
							if(apn->p[1]==zvlnr && apn->obi[1]==obi) {apn->mask[1]|= mask; break; }
							if(apn->p[2]==0) {apn->obi[2]= obi; apn->p[2]= zvlnr; apn->z[2]= vergz; apn->mask[2]= mask; break; }
							if(apn->p[2]==zvlnr && apn->obi[2]==obi) {apn->mask[2]|= mask; break; }
							if(apn->p[3]==0) {apn->obi[3]= obi; apn->p[3]= zvlnr; apn->z[3]= vergz; apn->mask[3]= mask; break; }
							if(apn->p[3]==zvlnr && apn->obi[3]==obi) {apn->mask[3]|= mask; break; }
							if(apn->next==0) apn->next= addpsA(zspan);
							apn= apn->next;
						}				
					}
				}
			}
			
			v1[1]+= dy;
			if(maxtest && (vergz > 0x7FFFFFF0 - dz)) vergz= 0x7FFFFFF0;
			else vergz+= dz;
		}
	}
	else {
	
		/* all lines from top to bottom */
		if(vec1[1]<vec2[1]) {
			copy_v3_v3(v1, vec1);
			copy_v3_v3(v2, vec2);
		}
		else {
			copy_v3_v3(v2, vec1);
			copy_v3_v3(v1, vec2);
			dx= -dx; dy= -dy;
		}

		start= floorf(v1[1]);
		end= start+floorf(dy);
		
		if(start>=zspan->recty || end<0) return;
		
		if(end>=zspan->recty) end= zspan->recty-1;
		
		oldx= floorf(v1[0]);
		dx/= dy;
		
		vergz= v1[2];
		vergz-= zspan->polygon_offset;
		dz= (v2[2]-v1[2])/dy;
		if(vergz>0x50000000 && dz>0) maxtest= 1;		// prevent overflow

		rectz= (int *)( zspan->arectz+ (start)*zspan->rectx+ oldx );
		rectmask= (int *)( zspan->rectmask+ (start)*zspan->rectx+ oldx );
		ap= (zspan->apixbuf+ zspan->rectx*(start) +oldx);
				
		if(dx<0) ofs= -1;
		else ofs= 1;

		for(y= start; y<=end; y++, rectz+=zspan->rectx, rectmask+=zspan->rectx, ap+=zspan->rectx) {
			
			x= floorf(v1[0]);
			if(x!=oldx) {
				oldx= x;
				rectz+= ofs;
				rectmask+= ofs;
				ap+= ofs;
			}
			
			if(x>=0 && y>=0 && x<zspan->rectx) {
				if(vergz<*rectz) {
					if(!zspan->rectmask || vergz>*rectmask) {
						
						apn= ap;
						while(apn) {	/* loop unrolled */
							if(apn->p[0]==0) {apn->obi[0]= obi; apn->p[0]= zvlnr; apn->z[0]= vergz; apn->mask[0]= mask; break; }
							if(apn->p[0]==zvlnr) {apn->mask[0]|= mask; break; }
							if(apn->p[1]==0) {apn->obi[1]= obi; apn->p[1]= zvlnr; apn->z[1]= vergz; apn->mask[1]= mask; break; }
							if(apn->p[1]==zvlnr) {apn->mask[1]|= mask; break; }
							if(apn->p[2]==0) {apn->obi[2]= obi; apn->p[2]= zvlnr; apn->z[2]= vergz; apn->mask[2]= mask; break; }
							if(apn->p[2]==zvlnr) {apn->mask[2]|= mask; break; }
							if(apn->p[3]==0) {apn->obi[3]= obi; apn->p[3]= zvlnr; apn->z[3]= vergz; apn->mask[3]= mask; break; }
							if(apn->p[3]==zvlnr) {apn->mask[3]|= mask; break; }
							if(apn->next==0) apn->next= addpsA(zspan);
							apn= apn->next;
						}	
					}
				}
			}
			
			v1[0]+= dx;
			if(maxtest && (vergz > 0x7FFFFFF0 - dz)) vergz= 0x7FFFFFF0;
			else vergz+= dz;
		}
	}
}

/* *************  NORMAL ZBUFFER ************ */

static void zbufline(ZSpan *zspan, int obi, int zvlnr, float *vec1, float *vec2)
{
	int *rectz, *rectp, *recto, *rectmask;
	int start, end, x, y, oldx, oldy, ofs;
	int dz, vergz, maxtest= 0;
	float dx, dy;
	float v1[3], v2[3];
	
	dx= vec2[0]-vec1[0];
	dy= vec2[1]-vec1[1];
	
	if(fabs(dx) > fabs(dy)) {

		/* all lines from left to right */
		if(vec1[0]<vec2[0]) {
			copy_v3_v3(v1, vec1);
			copy_v3_v3(v2, vec2);
		}
		else {
			copy_v3_v3(v2, vec1);
			copy_v3_v3(v1, vec2);
			dx= -dx; dy= -dy;
		}

		start= floorf(v1[0]);
		end= start+floorf(dx);
		if(end>=zspan->rectx) end= zspan->rectx-1;
		
		oldy= floorf(v1[1]);
		dy/= dx;
		
		vergz= floorf(v1[2]);
		dz= floorf((v2[2]-v1[2])/dx);
		if(vergz>0x50000000 && dz>0) maxtest= 1;		// prevent overflow
		
		rectz= zspan->rectz + oldy*zspan->rectx+ start;
		rectp= zspan->rectp + oldy*zspan->rectx+ start;
		recto= zspan->recto + oldy*zspan->rectx+ start;
		rectmask= zspan->rectmask + oldy*zspan->rectx+ start;
		
		if(dy<0) ofs= -zspan->rectx;
		else ofs= zspan->rectx;
		
		for(x= start; x<=end; x++, rectz++, rectp++, recto++, rectmask++) {
			
			y= floorf(v1[1]);
			if(y!=oldy) {
				oldy= y;
				rectz+= ofs;
				rectp+= ofs;
				recto+= ofs;
				rectmask+= ofs;
			}
			
			if(x>=0 && y>=0 && y<zspan->recty) {
				if(vergz<*rectz) {
					if(!zspan->rectmask || vergz>*rectmask) {
						*recto= obi;
						*rectz= vergz;
						*rectp= zvlnr;
					}
				}
			}
			
			v1[1]+= dy;
			
			if(maxtest && (vergz > 0x7FFFFFF0 - dz)) vergz= 0x7FFFFFF0;
			else vergz+= dz;
		}
	}
	else {
		/* all lines from top to bottom */
		if(vec1[1]<vec2[1]) {
			copy_v3_v3(v1, vec1);
			copy_v3_v3(v2, vec2);
		}
		else {
			copy_v3_v3(v2, vec1);
			copy_v3_v3(v1, vec2);
			dx= -dx; dy= -dy;
		}

		start= floorf(v1[1]);
		end= start+floorf(dy);
		
		if(end>=zspan->recty) end= zspan->recty-1;
		
		oldx= floorf(v1[0]);
		dx/= dy;
		
		vergz= floorf(v1[2]);
		dz= floorf((v2[2]-v1[2])/dy);
		if(vergz>0x50000000 && dz>0) maxtest= 1;		// prevent overflow
		
		rectz= zspan->rectz + start*zspan->rectx+ oldx;
		rectp= zspan->rectp + start*zspan->rectx+ oldx;
		recto= zspan->recto + start*zspan->rectx+ oldx;
		rectmask= zspan->rectmask + start*zspan->rectx+ oldx;
		
		if(dx<0) ofs= -1;
		else ofs= 1;

		for(y= start; y<=end; y++, rectz+=zspan->rectx, rectp+=zspan->rectx, recto+=zspan->rectx, rectmask+=zspan->rectx) {
			
			x= floorf(v1[0]);
			if(x!=oldx) {
				oldx= x;
				rectz+= ofs;
				rectp+= ofs;
				recto+= ofs;
				rectmask+= ofs;
			}
			
			if(x>=0 && y>=0 && x<zspan->rectx) {
				if(vergz<*rectz) {
					if(!zspan->rectmask || vergz>*rectmask) {
						*rectz= vergz;
						*rectp= zvlnr;
						*recto= obi;
					}
				}
			}
			
			v1[0]+= dx;
			if(maxtest && (vergz > 0x7FFFFFF0 - dz)) vergz= 0x7FFFFFF0;
			else vergz+= dz;
		}
	}
}

static void zbufline_onlyZ(ZSpan *zspan, int obi, int zvlnr, float *vec1, float *vec2)
{
	int *rectz, *rectz1= NULL;
	int start, end, x, y, oldx, oldy, ofs;
	int dz, vergz, maxtest= 0;
	float dx, dy;
	float v1[3], v2[3];
	
	dx= vec2[0]-vec1[0];
	dy= vec2[1]-vec1[1];
	
	if(fabs(dx) > fabs(dy)) {
		
		/* all lines from left to right */
		if(vec1[0]<vec2[0]) {
			copy_v3_v3(v1, vec1);
			copy_v3_v3(v2, vec2);
		}
		else {
			copy_v3_v3(v2, vec1);
			copy_v3_v3(v1, vec2);
			dx= -dx; dy= -dy;
		}
		
		start= floorf(v1[0]);
		end= start+floorf(dx);
		if(end>=zspan->rectx) end= zspan->rectx-1;
		
		oldy= floorf(v1[1]);
		dy/= dx;
		
		vergz= floorf(v1[2]);
		dz= floorf((v2[2]-v1[2])/dx);
		if(vergz>0x50000000 && dz>0) maxtest= 1;		// prevent overflow
		
		rectz= zspan->rectz + oldy*zspan->rectx+ start;
		if(zspan->rectz1)
			rectz1= zspan->rectz1 + oldy*zspan->rectx+ start;
		
		if(dy<0) ofs= -zspan->rectx;
		else ofs= zspan->rectx;
		
		for(x= start; x<=end; x++, rectz++) {
			
			y= floorf(v1[1]);
			if(y!=oldy) {
				oldy= y;
				rectz+= ofs;
				if(rectz1) rectz1+= ofs;
			}
			
			if(x>=0 && y>=0 && y<zspan->recty) {
				if(vergz < *rectz) {
					if(rectz1) *rectz1= *rectz;
					*rectz= vergz;
				}
				else if(rectz1 && vergz < *rectz1)
					*rectz1= vergz;
			}
			
			v1[1]+= dy;
			
			if(maxtest && (vergz > 0x7FFFFFF0 - dz)) vergz= 0x7FFFFFF0;
			else vergz+= dz;
			
			if(rectz1) rectz1++;
		}
	}
	else {
		/* all lines from top to bottom */
		if(vec1[1]<vec2[1]) {
			copy_v3_v3(v1, vec1);
			copy_v3_v3(v2, vec2);
		}
		else {
			copy_v3_v3(v2, vec1);
			copy_v3_v3(v1, vec2);
			dx= -dx; dy= -dy;
		}
		
		start= floorf(v1[1]);
		end= start+floorf(dy);
		
		if(end>=zspan->recty) end= zspan->recty-1;
		
		oldx= floorf(v1[0]);
		dx/= dy;
		
		vergz= floorf(v1[2]);
		dz= floorf((v2[2]-v1[2])/dy);
		if(vergz>0x50000000 && dz>0) maxtest= 1;		// prevent overflow
		
		rectz= zspan->rectz + start*zspan->rectx+ oldx;
		if(zspan->rectz1)
			rectz1= zspan->rectz1 + start*zspan->rectx+ oldx;
		
		if(dx<0) ofs= -1;
		else ofs= 1;
		
		for(y= start; y<=end; y++, rectz+=zspan->rectx) {
			
			x= floorf(v1[0]);
			if(x!=oldx) {
				oldx= x;
				rectz+= ofs;
				if(rectz1) rectz1+= ofs;
			}
			
			if(x>=0 && y>=0 && x<zspan->rectx) {
				if(vergz < *rectz) {
					if(rectz1) *rectz1= *rectz;
					*rectz= vergz;
				}
				else if(rectz1 && vergz < *rectz1)
					*rectz1= vergz;
			}
			
			v1[0]+= dx;
			if(maxtest && (vergz > 0x7FFFFFF0 - dz)) vergz= 0x7FFFFFF0;
			else vergz+= dz;
			
			if(rectz1)
				rectz1+=zspan->rectx;
		}
	}
}


static int clipline(float *v1, float *v2)	/* return 0: do not draw */
{
	float dz,dw, u1=0.0, u2=1.0;
	float dx, dy, v13;
	
	dz= v2[2]-v1[2];
	dw= v2[3]-v1[3];
	
	/* this 1.01 is for clipping x and y just a tinsy larger. that way it is
		filled in with zbufwire correctly when rendering in parts. otherwise
		you see line endings at edges... */
	
	if(cliptestf(-dz-dw, v1[3]+v1[2], &u1,&u2)) {
		if(cliptestf(dz-dw, v1[3]-v1[2], &u1,&u2)) {
			
			dx= v2[0]-v1[0];
			dz= 1.01*(v2[3]-v1[3]);
			v13= 1.01*v1[3];
			
			if(cliptestf(-dx-dz, v1[0]+v13, &u1,&u2)) {
				if(cliptestf(dx-dz, v13-v1[0], &u1,&u2)) {
					
					dy= v2[1]-v1[1];
					
					if(cliptestf(-dy-dz, v1[1]+v13, &u1,&u2)) {
						if(cliptestf(dy-dz, v13-v1[1], &u1,&u2)) {
							
							if(u2<1.0) {
								v2[0]= v1[0]+u2*dx;
								v2[1]= v1[1]+u2*dy;
								v2[2]= v1[2]+u2*dz;
								v2[3]= v1[3]+u2*dw;
							}
							if(u1>0.0) {
								v1[0]= v1[0]+u1*dx;
								v1[1]= v1[1]+u1*dy;
								v1[2]= v1[2]+u1*dz;
								v1[3]= v1[3]+u1*dw;
							}
							return 1;
						}
					}
				}
			}
		}
	}
	
	return 0;
}

void hoco_to_zco(ZSpan *zspan, float *zco, float *hoco)
{
	float div;
	
	div= 1.0f/hoco[3];
	zco[0]= zspan->zmulx*(1.0+hoco[0]*div) + zspan->zofsx;
	zco[1]= zspan->zmuly*(1.0+hoco[1]*div) + zspan->zofsy;
	zco[2]= 0x7FFFFFFF *(hoco[2]*div);
}

void zbufclipwire(ZSpan *zspan, int obi, int zvlnr, int ec, float *ho1, float *ho2, float *ho3, float *ho4, int c1, int c2, int c3, int c4)
{
	float vez[20];
	int and, or;

	/* edgecode: 1= draw */
	if(ec==0) return;

	if(ho4) {
		and= (c1 & c2 & c3 & c4);
		or= (c1 | c2 | c3 | c4);
	}
	else {
		and= (c1 & c2 & c3);
		or= (c1 | c2 | c3);
	}
	
	if(or) {	/* not in the middle */
		if(and) {	/* out completely */
			return;
		}
		else {	/* clipping */

			if(ec & ME_V1V2) {
				copy_v4_v4(vez, ho1);
				copy_v4_v4(vez+4, ho2);
				if( clipline(vez, vez+4)) {
					hoco_to_zco(zspan, vez, vez);
					hoco_to_zco(zspan, vez+4, vez+4);
					zspan->zbuflinefunc(zspan, obi, zvlnr, vez, vez+4);
				}
			}
			if(ec & ME_V2V3) {
				copy_v4_v4(vez, ho2);
				copy_v4_v4(vez+4, ho3);
				if( clipline(vez, vez+4)) {
					hoco_to_zco(zspan, vez, vez);
					hoco_to_zco(zspan, vez+4, vez+4);
					zspan->zbuflinefunc(zspan, obi, zvlnr, vez, vez+4);
				}
			}
			if(ho4) {
				if(ec & ME_V3V4) {
					copy_v4_v4(vez, ho3);
					copy_v4_v4(vez+4, ho4);
					if( clipline(vez, vez+4)) {
						hoco_to_zco(zspan, vez, vez);
						hoco_to_zco(zspan, vez+4, vez+4);
						zspan->zbuflinefunc(zspan, obi, zvlnr, vez, vez+4);
					}
				}
				if(ec & ME_V4V1) {
					copy_v4_v4(vez, ho4);
					copy_v4_v4(vez+4, ho1);
					if( clipline(vez, vez+4)) {
						hoco_to_zco(zspan, vez, vez);
						hoco_to_zco(zspan, vez+4, vez+4);
						zspan->zbuflinefunc(zspan, obi, zvlnr, vez, vez+4);
					}
				}
			}
			else {
				if(ec & ME_V3V1) {
					copy_v4_v4(vez, ho3);
					copy_v4_v4(vez+4, ho1);
					if( clipline(vez, vez+4)) {
						hoco_to_zco(zspan, vez, vez);
						hoco_to_zco(zspan, vez+4, vez+4);
						zspan->zbuflinefunc(zspan, obi, zvlnr, vez, vez+4);
					}
				}
			}
			
			return;
		}
	}

	hoco_to_zco(zspan, vez, ho1);
	hoco_to_zco(zspan, vez+4, ho2);
	hoco_to_zco(zspan, vez+8, ho3);
	if(ho4) {
		hoco_to_zco(zspan, vez+12, ho4);

		if(ec & ME_V3V4)  zspan->zbuflinefunc(zspan, obi, zvlnr, vez+8, vez+12);
		if(ec & ME_V4V1)  zspan->zbuflinefunc(zspan, obi, zvlnr, vez+12, vez);
	}
	else {
		if(ec & ME_V3V1)  zspan->zbuflinefunc(zspan, obi, zvlnr, vez+8, vez);
	}

	if(ec & ME_V1V2)  zspan->zbuflinefunc(zspan, obi, zvlnr, vez, vez+4);
	if(ec & ME_V2V3)  zspan->zbuflinefunc(zspan, obi, zvlnr, vez+4, vez+8);

}

void zbufsinglewire(ZSpan *zspan, int obi, int zvlnr, float *ho1, float *ho2)
{
	float f1[4], f2[4];
	int c1, c2;

	c1= camera_hoco_test_clip(ho1);
	c2= camera_hoco_test_clip(ho2);

	if(c1 | c2) {	/* not in the middle */
		if(!(c1 & c2)) {	/* not out completely */
			copy_v4_v4(f1, ho1);
			copy_v4_v4(f2, ho2);

			if(clipline(f1, f2)) {
				hoco_to_zco(zspan, f1, f1);
				hoco_to_zco(zspan, f2, f2);
				zspan->zbuflinefunc(zspan, obi, zvlnr, f1, f2);
			}
		}
	}
	else {
		hoco_to_zco(zspan, f1, ho1);
		hoco_to_zco(zspan, f2, ho2);

		zspan->zbuflinefunc(zspan, obi, zvlnr, f1, f2);
	}
}

/**
 * Fill the z buffer, but invert z order, and add the face index to
 * the corresponing face buffer.
 *
 * This is one of the z buffer fill functions called in zbufclip() and
 * zbufwireclip(). 
 *
 * @param v1 [4 floats, world coordinates] first vertex
 * @param v2 [4 floats, world coordinates] second vertex
 * @param v3 [4 floats, world coordinates] third vertex
 */
static void zbuffillGLinv4(ZSpan *zspan, int obi, int zvlnr, float *v1, float *v2, float *v3, float *v4) 
{
	double zxd, zyd, zy0, zverg;
	float x0,y0,z0;
	float x1,y1,z1,x2,y2,z2,xx1;
	float *span1, *span2;
	int *rectoofs, *ro;
	int *rectpofs, *rp;
	int *rectmaskofs, *rm;
	int *rz, x, y;
	int sn1, sn2, rectx, *rectzofs, my0, my2;
	
	/* init */
	zbuf_init_span(zspan);
	
	/* set spans */
	zbuf_add_to_span(zspan, v1, v2);
	zbuf_add_to_span(zspan, v2, v3);
	if(v4) {
		zbuf_add_to_span(zspan, v3, v4);
		zbuf_add_to_span(zspan, v4, v1);
	}
	else 
		zbuf_add_to_span(zspan, v3, v1);
	
	/* clipped */
	if(zspan->minp2==NULL || zspan->maxp2==NULL) return;
	
	if(zspan->miny1 < zspan->miny2) my0= zspan->miny2; else my0= zspan->miny1;
	if(zspan->maxy1 > zspan->maxy2) my2= zspan->maxy2; else my2= zspan->maxy1;
	
	//	printf("my %d %d\n", my0, my2);
	if(my2<my0) return;
	
	
	/* ZBUF DX DY, in floats still */
	x1= v1[0]- v2[0];
	x2= v2[0]- v3[0];
	y1= v1[1]- v2[1];
	y2= v2[1]- v3[1];
	z1= v1[2]- v2[2];
	z2= v2[2]- v3[2];
	x0= y1*z2-z1*y2;
	y0= z1*x2-x1*z2;
	z0= x1*y2-y1*x2;
	
	if(z0==0.0) return;
	
	xx1= (x0*v1[0] + y0*v1[1])/z0 + v1[2];
	
	zxd= -(double)x0/(double)z0;
	zyd= -(double)y0/(double)z0;
	zy0= ((double)my2)*zyd + (double)xx1;
	
	/* start-offset in rect */
	rectx= zspan->rectx;
	rectzofs= (zspan->rectz+rectx*my2);
	rectpofs= (zspan->rectp+rectx*my2);
	rectoofs= (zspan->recto+rectx*my2);
	rectmaskofs= (zspan->rectmask+rectx*my2);
	
	/* correct span */
	sn1= (my0 + my2)/2;
	if(zspan->span1[sn1] < zspan->span2[sn1]) {
		span1= zspan->span1+my2;
		span2= zspan->span2+my2;
	}
	else {
		span1= zspan->span2+my2;
		span2= zspan->span1+my2;
	}
	
	for(y=my2; y>=my0; y--, span1--, span2--) {
		
		sn1= floorf(*span1);
		sn2= floorf(*span2);
		sn1++; 
		
		if(sn2>=rectx) sn2= rectx-1;
		if(sn1<0) sn1= 0;
		
		if(sn2>=sn1) {
			int intzverg;
			
			zverg= (double)sn1*zxd + zy0;
			rz= rectzofs+sn1;
			rp= rectpofs+sn1;
			ro= rectoofs+sn1;
			rm= rectmaskofs+sn1;
			x= sn2-sn1;
			
			while(x>=0) {
				intzverg= (int)CLAMPIS(zverg, INT_MIN, INT_MAX);

				if( intzverg > *rz || *rz==0x7FFFFFFF) {
					if(!zspan->rectmask || intzverg > *rm) {
						*ro= obi;
						*rz= intzverg;
						*rp= zvlnr;
					}
				}
				zverg+= zxd;
				rz++; 
				rp++; 
				ro++;
				rm++;
				x--;
			}
		}
		
		zy0-=zyd;
		rectzofs-= rectx;
		rectpofs-= rectx;
		rectoofs-= rectx;
		rectmaskofs-= rectx;
	}
}

/* uses spanbuffers */

static void zbuffillGL4(ZSpan *zspan, int obi, int zvlnr, float *v1, float *v2, float *v3, float *v4)
{
	double zxd, zyd, zy0, zverg;
	float x0,y0,z0;
	float x1,y1,z1,x2,y2,z2,xx1;
	float *span1, *span2;
	int *rectoofs, *ro;
	int *rectpofs, *rp;
	int *rectmaskofs, *rm;
	int *rz, x, y;
	int sn1, sn2, rectx, *rectzofs, my0, my2;
	
	/* init */
	zbuf_init_span(zspan);
	
	/* set spans */
	zbuf_add_to_span(zspan, v1, v2);
	zbuf_add_to_span(zspan, v2, v3);
	if(v4) {
		zbuf_add_to_span(zspan, v3, v4);
		zbuf_add_to_span(zspan, v4, v1);
	}
	else 
		zbuf_add_to_span(zspan, v3, v1);
		
	/* clipped */
	if(zspan->minp2==NULL || zspan->maxp2==NULL) return;
	
	if(zspan->miny1 < zspan->miny2) my0= zspan->miny2; else my0= zspan->miny1;
	if(zspan->maxy1 > zspan->maxy2) my2= zspan->maxy2; else my2= zspan->maxy1;
	
//	printf("my %d %d\n", my0, my2);
	if(my2<my0) return;
	
	
	/* ZBUF DX DY, in floats still */
	x1= v1[0]- v2[0];
	x2= v2[0]- v3[0];
	y1= v1[1]- v2[1];
	y2= v2[1]- v3[1];
	z1= v1[2]- v2[2];
	z2= v2[2]- v3[2];
	x0= y1*z2-z1*y2;
	y0= z1*x2-x1*z2;
	z0= x1*y2-y1*x2;
	
	if(z0==0.0) return;

	xx1= (x0*v1[0] + y0*v1[1])/z0 + v1[2];

	zxd= -(double)x0/(double)z0;
	zyd= -(double)y0/(double)z0;
	zy0= ((double)my2)*zyd + (double)xx1;

	/* start-offset in rect */
	rectx= zspan->rectx;
	rectzofs= (zspan->rectz+rectx*my2);
	rectpofs= (zspan->rectp+rectx*my2);
	rectoofs= (zspan->recto+rectx*my2);
	rectmaskofs= (zspan->rectmask+rectx*my2);

	/* correct span */
	sn1= (my0 + my2)/2;
	if(zspan->span1[sn1] < zspan->span2[sn1]) {
		span1= zspan->span1+my2;
		span2= zspan->span2+my2;
	}
	else {
		span1= zspan->span2+my2;
		span2= zspan->span1+my2;
	}
	
	for(y=my2; y>=my0; y--, span1--, span2--) {
		
		sn1= floorf(*span1);
		sn2= floorf(*span2);
		sn1++; 
		
		if(sn2>=rectx) sn2= rectx-1;
		if(sn1<0) sn1= 0;
		
		if(sn2>=sn1) {
			int intzverg;
			
			zverg= (double)sn1*zxd + zy0;
			rz= rectzofs+sn1;
			rp= rectpofs+sn1;
			ro= rectoofs+sn1;
			rm= rectmaskofs+sn1;
			x= sn2-sn1;
			
			while(x>=0) {
				intzverg= (int)CLAMPIS(zverg, INT_MIN, INT_MAX);
				
				if(intzverg < *rz) {
					if(!zspan->rectmask || intzverg > *rm) {
						*rz= intzverg;
						*rp= zvlnr;
						*ro= obi;
					}
				}
				zverg+= zxd;
				rz++; 
				rp++; 
				ro++; 
				rm++;
				x--;
			}
		}
		
		zy0-=zyd;
		rectzofs-= rectx;
		rectpofs-= rectx;
		rectoofs-= rectx;
		rectmaskofs-= rectx;
	}
}

/**
 * Fill the z buffer. The face buffer is not operated on!
 *
 * This is one of the z buffer fill functions called in zbufclip() and
 * zbufwireclip(). 
 *
 * @param v1 [4 floats, world coordinates] first vertex
 * @param v2 [4 floats, world coordinates] second vertex
 * @param v3 [4 floats, world coordinates] third vertex
 */

/* now: filling two Z values, the closest and 2nd closest */
static void zbuffillGL_onlyZ(ZSpan *zspan, int obi, int zvlnr, float *v1, float *v2, float *v3, float *v4) 
{
	double zxd, zyd, zy0, zverg;
	float x0,y0,z0;
	float x1,y1,z1,x2,y2,z2,xx1;
	float *span1, *span2;
	int *rz, *rz1, x, y;
	int sn1, sn2, rectx, *rectzofs, *rectzofs1= NULL, my0, my2;
	
	/* init */
	zbuf_init_span(zspan);
	
	/* set spans */
	zbuf_add_to_span(zspan, v1, v2);
	zbuf_add_to_span(zspan, v2, v3);
	if(v4) {
		zbuf_add_to_span(zspan, v3, v4);
		zbuf_add_to_span(zspan, v4, v1);
	}
	else 
		zbuf_add_to_span(zspan, v3, v1);
	
	/* clipped */
	if(zspan->minp2==NULL || zspan->maxp2==NULL) return;
	
	if(zspan->miny1 < zspan->miny2) my0= zspan->miny2; else my0= zspan->miny1;
	if(zspan->maxy1 > zspan->maxy2) my2= zspan->maxy2; else my2= zspan->maxy1;
	
	//	printf("my %d %d\n", my0, my2);
	if(my2<my0) return;
	
	
	/* ZBUF DX DY, in floats still */
	x1= v1[0]- v2[0];
	x2= v2[0]- v3[0];
	y1= v1[1]- v2[1];
	y2= v2[1]- v3[1];
	z1= v1[2]- v2[2];
	z2= v2[2]- v3[2];
	x0= y1*z2-z1*y2;
	y0= z1*x2-x1*z2;
	z0= x1*y2-y1*x2;
	
	if(z0==0.0) return;
	
	xx1= (x0*v1[0] + y0*v1[1])/z0 + v1[2];
	
	zxd= -(double)x0/(double)z0;
	zyd= -(double)y0/(double)z0;
	zy0= ((double)my2)*zyd + (double)xx1;
	
	/* start-offset in rect */
	rectx= zspan->rectx;
	rectzofs= (zspan->rectz+rectx*my2);
	if(zspan->rectz1)
		rectzofs1= (zspan->rectz1+rectx*my2);
	
	/* correct span */
	sn1= (my0 + my2)/2;
	if(zspan->span1[sn1] < zspan->span2[sn1]) {
		span1= zspan->span1+my2;
		span2= zspan->span2+my2;
	}
	else {
		span1= zspan->span2+my2;
		span2= zspan->span1+my2;
	}
	
	for(y=my2; y>=my0; y--, span1--, span2--) {
		
		sn1= floorf(*span1);
		sn2= floorf(*span2);
		sn1++; 
		
		if(sn2>=rectx) sn2= rectx-1;
		if(sn1<0) sn1= 0;
		
		if(sn2>=sn1) {
			zverg= (double)sn1*zxd + zy0;
			rz= rectzofs+sn1;
			rz1= rectzofs1+sn1;
			x= sn2-sn1;
			
			while(x>=0) {
				int zvergi= (int)CLAMPIS(zverg, INT_MIN, INT_MAX);

				/* option: maintain two depth values, closest and 2nd closest */
				if(zvergi < *rz) {
					if(rectzofs1) *rz1= *rz;
					*rz= zvergi;
				}
				else if(rectzofs1 && zvergi < *rz1)
					*rz1= zvergi;

				zverg+= zxd;
				
				rz++; 
				rz1++;
				x--;
			}
		}
		
		zy0-=zyd;
		rectzofs-= rectx;
		if(rectzofs1) rectzofs1-= rectx;
	}
}

/* 2d scanconvert for tria, calls func for each x,y coordinate and gives UV barycentrics */
void zspan_scanconvert_strand(ZSpan *zspan, void *handle, float *v1, float *v2, float *v3, void (*func)(void *, int, int, float, float, float) )
{
	float x0, y0, x1, y1, x2, y2, z0, z1, z2, z;
	float u, v, uxd, uyd, vxd, vyd, uy0, vy0, zxd, zyd, zy0, xx1;
	float *span1, *span2;
	int x, y, sn1, sn2, rectx= zspan->rectx, my0, my2;
	
	/* init */
	zbuf_init_span(zspan);
	
	/* set spans */
	zbuf_add_to_span(zspan, v1, v2);
	zbuf_add_to_span(zspan, v2, v3);
	zbuf_add_to_span(zspan, v3, v1);
	
	/* clipped */
	if(zspan->minp2==NULL || zspan->maxp2==NULL) return;
	
	if(zspan->miny1 < zspan->miny2) my0= zspan->miny2; else my0= zspan->miny1;
	if(zspan->maxy1 > zspan->maxy2) my2= zspan->maxy2; else my2= zspan->maxy1;
	
	//	printf("my %d %d\n", my0, my2);
	if(my2<my0) return;
	
	/* ZBUF DX DY, in floats still */
	x1= v1[0]- v2[0];
	x2= v2[0]- v3[0];
	y1= v1[1]- v2[1];
	y2= v2[1]- v3[1];
	z1= v1[2]- v2[2];
	z2= v2[2]- v3[2];

	x0= y1*z2-z1*y2;
	y0= z1*x2-x1*z2;
	z0= x1*y2-y1*x2;
	
	if(z0==0.0f) return;

	xx1= (x0*v1[0] + y0*v1[1])/z0 + v1[2];
	zxd= -(double)x0/(double)z0;
	zyd= -(double)y0/(double)z0;
	zy0= ((double)my2)*zyd + (double)xx1;
	
	z1= 1.0f; // (u1 - u2)
	z2= 0.0f; // (u2 - u3)

	x0= y1*z2-z1*y2;
	y0= z1*x2-x1*z2;
	
	xx1= (x0*v1[0] + y0*v1[1])/z0 + 1.0f;	
	uxd= -(double)x0/(double)z0;
	uyd= -(double)y0/(double)z0;
	uy0= ((double)my2)*uyd + (double)xx1;

	z1= -1.0f; // (v1 - v2)
	z2= 1.0f;  // (v2 - v3)
	
	x0= y1*z2-z1*y2;
	y0= z1*x2-x1*z2;
	
	xx1= (x0*v1[0] + y0*v1[1])/z0;
	vxd= -(double)x0/(double)z0;
	vyd= -(double)y0/(double)z0;
	vy0= ((double)my2)*vyd + (double)xx1;
	
	/* correct span */
	sn1= (my0 + my2)/2;
	if(zspan->span1[sn1] < zspan->span2[sn1]) {
		span1= zspan->span1+my2;
		span2= zspan->span2+my2;
	}
	else {
		span1= zspan->span2+my2;
		span2= zspan->span1+my2;
	}
	
	for(y=my2; y>=my0; y--, span1--, span2--) {
		
		sn1= floorf(*span1);
		sn2= floorf(*span2);
		sn1++; 
		
		if(sn2>=rectx) sn2= rectx-1;
		if(sn1<0) sn1= 0;
		
		u= (double)sn1*uxd + uy0;
		v= (double)sn1*vxd + vy0;
		z= (double)sn1*zxd + zy0;
		
		for(x= sn1; x<=sn2; x++, u+=uxd, v+=vxd, z+=zxd)
			func(handle, x, y, u, v, z);
		
		uy0 -= uyd;
		vy0 -= vyd;
		zy0 -= zyd;
	}
}

/* scanconvert for strand triangles, calls func for each x,y coordinate and gives UV barycentrics and z */

void zspan_scanconvert(ZSpan *zspan, void *handle, float *v1, float *v2, float *v3, void (*func)(void *, int, int, float, float) )
{
	float x0, y0, x1, y1, x2, y2, z0, z1, z2;
	float u, v, uxd, uyd, vxd, vyd, uy0, vy0, xx1;
	float *span1, *span2;
	int x, y, sn1, sn2, rectx= zspan->rectx, my0, my2;
	
	/* init */
	zbuf_init_span(zspan);
	
	/* set spans */
	zbuf_add_to_span(zspan, v1, v2);
	zbuf_add_to_span(zspan, v2, v3);
	zbuf_add_to_span(zspan, v3, v1);
	
	/* clipped */
	if(zspan->minp2==NULL || zspan->maxp2==NULL) return;
	
	if(zspan->miny1 < zspan->miny2) my0= zspan->miny2; else my0= zspan->miny1;
	if(zspan->maxy1 > zspan->maxy2) my2= zspan->maxy2; else my2= zspan->maxy1;
	
	//	printf("my %d %d\n", my0, my2);
	if(my2<my0) return;
	
	/* ZBUF DX DY, in floats still */
	x1= v1[0]- v2[0];
	x2= v2[0]- v3[0];
	y1= v1[1]- v2[1];
	y2= v2[1]- v3[1];
	
	z1= 1.0f; // (u1 - u2)
	z2= 0.0f; // (u2 - u3)
	
	x0= y1*z2-z1*y2;
	y0= z1*x2-x1*z2;
	z0= x1*y2-y1*x2;
	
	if(z0==0.0f) return;
	
	xx1= (x0*v1[0] + y0*v1[1])/z0 + 1.0f;	
	uxd= -(double)x0/(double)z0;
	uyd= -(double)y0/(double)z0;
	uy0= ((double)my2)*uyd + (double)xx1;

	z1= -1.0f; // (v1 - v2)
	z2= 1.0f;  // (v2 - v3)
	
	x0= y1*z2-z1*y2;
	y0= z1*x2-x1*z2;
	
	xx1= (x0*v1[0] + y0*v1[1])/z0;
	vxd= -(double)x0/(double)z0;
	vyd= -(double)y0/(double)z0;
	vy0= ((double)my2)*vyd + (double)xx1;
	
	/* correct span */
	sn1= (my0 + my2)/2;
	if(zspan->span1[sn1] < zspan->span2[sn1]) {
		span1= zspan->span1+my2;
		span2= zspan->span2+my2;
	}
	else {
		span1= zspan->span2+my2;
		span2= zspan->span1+my2;
	}
	
	for(y=my2; y>=my0; y--, span1--, span2--) {
		
		sn1= floorf(*span1);
		sn2= floorf(*span2);
		sn1++; 
		
		if(sn2>=rectx) sn2= rectx-1;
		if(sn1<0) sn1= 0;
		
		u= (double)sn1*uxd + uy0;
		v= (double)sn1*vxd + vy0;
		
		for(x= sn1; x<=sn2; x++, u+=uxd, v+=vxd)
			func(handle, x, y, u, v);
		
		uy0 -= uyd;
		vy0 -= vyd;
	}
}



/**
 * (clip pyramid)
 * Sets labda: flag, and parametrize the clipping of vertices in
 * viewspace coordinates. labda = -1 means no clipping, labda in [0,
	 * 1] means a clipping.
 * Note: uses globals.
 * @param v1 start coordinate s
 * @param v2 target coordinate t
 * @param b1 
 * @param b2 
 * @param b3
 * @param a index for coordinate (x, y, or z)
 */

static void clippyra(float *labda, float *v1, float *v2, int *b2, int *b3, int a, float clipcrop)
{
	float da,dw,u1=0.0,u2=1.0;
	float v13;
	
	labda[0]= -1.0;
	labda[1]= -1.0;

	da= v2[a]-v1[a];
	/* prob; we clip slightly larger, osa renders add 2 pixels on edges, should become variable? */
	/* or better; increase r.winx/y size, but thats quite a complex one. do it later */
	if(a==2) {
		dw= (v2[3]-v1[3]);
		v13= v1[3];
	}
	else {
		dw= clipcrop*(v2[3]-v1[3]);
		v13= clipcrop*v1[3];
	}	
	/* according the original article by Liang&Barsky, for clipping of
	 * homogenous coordinates with viewplane, the value of "0" is used instead of "-w" .
	 * This differs from the other clipping cases (like left or top) and I considered
	 * it to be not so 'homogenic'. But later it has proven to be an error,
	 * who would have thought that of L&B!
	 */

	if(cliptestf(-da-dw, v13+v1[a], &u1,&u2)) {
		if(cliptestf(da-dw, v13-v1[a], &u1,&u2)) {
			*b3=1;
			if(u2<1.0) {
				labda[1]= u2;
				*b2=1;
			}
			else labda[1]=1.0;  /* u2 */
			if(u1>0.0) {
				labda[0]= u1;
				*b2=1;
			} else labda[0]=0.0;
		}
	}
}

/**
 * (make vertex pyramide clip)
 * Checks labda and uses this to make decision about clipping the line
 * segment from v1 to v2. labda is the factor by which the vector is
 * cut. ( calculate s + l * ( t - s )). The result is appended to the
 * vertex list of this face.
 * 
 * 
 * @param v1 start coordinate s
 * @param v2 target coordinate t
 * @param b1 
 * @param b2 
 * @param clve vertex vector.
 */

static void makevertpyra(float *vez, float *labda, float **trias, float *v1, float *v2, int *b1, int *clve)
{
	float l1, l2, *adr;

	l1= labda[0];
	l2= labda[1];

	if(l1!= -1.0) {
		if(l1!= 0.0) {
			adr= vez+4*(*clve);
			trias[*b1]=adr;
			(*clve)++;
			adr[0]= v1[0]+l1*(v2[0]-v1[0]);
			adr[1]= v1[1]+l1*(v2[1]-v1[1]);
			adr[2]= v1[2]+l1*(v2[2]-v1[2]);
			adr[3]= v1[3]+l1*(v2[3]-v1[3]);
		} 
		else trias[*b1]= v1;
		
		(*b1)++;
	}
	if(l2!= -1.0) {
		if(l2!= 1.0) {
			adr= vez+4*(*clve);
			trias[*b1]=adr;
			(*clve)++;
			adr[0]= v1[0]+l2*(v2[0]-v1[0]);
			adr[1]= v1[1]+l2*(v2[1]-v1[1]);
			adr[2]= v1[2]+l2*(v2[2]-v1[2]);
			adr[3]= v1[3]+l2*(v2[3]-v1[3]);
			(*b1)++;
		}
	}
}

/* ------------------------------------------------------------------------- */

#define ZBUF_PROJECT_CACHE_SIZE 256

typedef struct ZbufProjectCache {
	int index, clip;
	float ho[4];
} ZbufProjectCache;

static void zbuf_project_cache_clear(ZbufProjectCache *cache, int size)
{
	int i;

	if(size > ZBUF_PROJECT_CACHE_SIZE)
		size= ZBUF_PROJECT_CACHE_SIZE;

	memset(cache, 0, sizeof(ZbufProjectCache)*size);
	for(i=0; i<size; i++)
		cache[i].index= -1;
}

static int zbuf_shadow_project(ZbufProjectCache *cache, int index, float winmat[][4], float *co, float *ho)
{
	int clipflag, cindex= index & 255;

	if(cache[cindex].index == index) {
		copy_v4_v4(ho, cache[cindex].ho);
		return cache[cindex].clip;
	}
	else {
		camera_matrix_co_to_hoco(winmat, ho, co);
		clipflag= camera_hoco_test_clip(ho);

		copy_v4_v4(cache[cindex].ho, ho);
		cache[cindex].clip= clipflag;
		cache[cindex].index= index;

		return clipflag;
	}
}

static int zbuf_part_project(ZbufProjectCache *cache, int index, float winmat[][4], float *bounds, float *co, float *ho)
{
	float wco;
	int clipflag= 0, cindex= index & 255;

	if(cache[cindex].index == index) {
		copy_v4_v4(ho, cache[cindex].ho);
		return cache[cindex].clip;
	}
	else {
		camera_matrix_co_to_hoco(winmat, ho, co);

		wco= ho[3];
		if(ho[0] > bounds[1]*wco) clipflag |= 1;
		else if(ho[0]< bounds[0]*wco) clipflag |= 2;
		if(ho[1] > bounds[3]*wco) clipflag |= 4;
		else if(ho[1]< bounds[2]*wco) clipflag |= 8;

		copy_v4_v4(cache[cindex].ho, ho);
		cache[cindex].clip= clipflag;
		cache[cindex].index= index;

		return clipflag;
	}
}

/* do zbuffering and clip, f1 f2 f3 are hocos, c1 c2 c3 are clipping flags */

void zbufclip(ZSpan *zspan, int obi, int zvlnr, float *f1, float *f2, float *f3, int c1, int c2, int c3)
{
	float *vlzp[32][3], labda[3][2];
	float vez[400], *trias[40];
	
	if(c1 | c2 | c3) {	/* not in middle */
		if(c1 & c2 & c3) {	/* completely out */
			return;
		} else {	/* clipping */
			int arg, v, b, clipflag[3], b1, b2, b3, c4, clve=3, clvlo, clvl=1;

			vez[0]= f1[0]; vez[1]= f1[1]; vez[2]= f1[2]; vez[3]= f1[3];
			vez[4]= f2[0]; vez[5]= f2[1]; vez[6]= f2[2]; vez[7]= f2[3];
			vez[8]= f3[0]; vez[9]= f3[1]; vez[10]= f3[2];vez[11]= f3[3];

			vlzp[0][0]= vez;
			vlzp[0][1]= vez+4;
			vlzp[0][2]= vez+8;

			clipflag[0]= ( (c1 & 48) | (c2 & 48) | (c3 & 48) );
			if(clipflag[0]==0) {	/* othwerwise it needs to be calculated again, after the first (z) clip */
				clipflag[1]= ( (c1 & 3) | (c2 & 3) | (c3 & 3) );
				clipflag[2]= ( (c1 & 12) | (c2 & 12) | (c3 & 12) );
			}
			else clipflag[1]=clipflag[2]= 0;
			
			for(b=0;b<3;b++) {
				
				if(clipflag[b]) {
				
					clvlo= clvl;
					
					for(v=0; v<clvlo; v++) {
					
						if(vlzp[v][0]!=NULL) {	/* face is still there */
							b2= b3 =0;	/* clip flags */

							if(b==0) arg= 2;
							else if (b==1) arg= 0;
							else arg= 1;
							
							clippyra(labda[0], vlzp[v][0],vlzp[v][1], &b2,&b3, arg, zspan->clipcrop);
							clippyra(labda[1], vlzp[v][1],vlzp[v][2], &b2,&b3, arg, zspan->clipcrop);
							clippyra(labda[2], vlzp[v][2],vlzp[v][0], &b2,&b3, arg, zspan->clipcrop);

							if(b2==0 && b3==1) {
								/* completely 'in', but we copy because of last for() loop in this section */;
								vlzp[clvl][0]= vlzp[v][0];
								vlzp[clvl][1]= vlzp[v][1];
								vlzp[clvl][2]= vlzp[v][2];
								vlzp[v][0]= NULL;
								clvl++;
							} else if(b3==0) {
								vlzp[v][0]= NULL;
								/* completely 'out' */;
							} else {
								b1=0;
								makevertpyra(vez, labda[0], trias, vlzp[v][0],vlzp[v][1], &b1,&clve);
								makevertpyra(vez, labda[1], trias, vlzp[v][1],vlzp[v][2], &b1,&clve);
								makevertpyra(vez, labda[2], trias, vlzp[v][2],vlzp[v][0], &b1,&clve);

								/* after front clip done: now set clip flags */
								if(b==0) {
									clipflag[1]= clipflag[2]= 0;
									f1= vez;
									for(b3=0; b3<clve; b3++) {
										c4= camera_hoco_test_clip(f1);
										clipflag[1] |= (c4 & 3);
										clipflag[2] |= (c4 & 12);
										f1+= 4;
									}
								}
								
								vlzp[v][0]= NULL;
								if(b1>2) {
									for(b3=3; b3<=b1; b3++) {
										vlzp[clvl][0]= trias[0];
										vlzp[clvl][1]= trias[b3-2];
										vlzp[clvl][2]= trias[b3-1];
										clvl++;
									}
								}
							}
						}
					}
				}
			}

            /* warning, this should never happen! */
			if(clve>38 || clvl>31) printf("clip overflow: clve clvl %d %d\n",clve,clvl);

            /* perspective division */
			f1=vez;
			for(c1=0;c1<clve;c1++) {
				hoco_to_zco(zspan, f1, f1);
				f1+=4;
			}
			for(b=1;b<clvl;b++) {
				if(vlzp[b][0]) {
					zspan->zbuffunc(zspan, obi, zvlnr, vlzp[b][0],vlzp[b][1],vlzp[b][2], NULL);
				}
			}
			return;
		}
	}

	/* perspective division: HCS to ZCS */
	hoco_to_zco(zspan, vez, f1);
	hoco_to_zco(zspan, vez+4, f2);
	hoco_to_zco(zspan, vez+8, f3);
	zspan->zbuffunc(zspan, obi, zvlnr, vez,vez+4,vez+8, NULL);
}

void zbufclip4(ZSpan *zspan, int obi, int zvlnr, float *f1, float *f2, float *f3, float *f4, int c1, int c2, int c3, int c4)
{
	float vez[16];
	
	if(c1 | c2 | c3 | c4) {	/* not in middle */
		if(c1 & c2 & c3 & c4) {	/* completely out */
			return;
		} else {	/* clipping */
			zbufclip(zspan, obi, zvlnr, f1, f2, f3, c1, c2, c3);
			zbufclip(zspan, obi, zvlnr, f1, f3, f4, c1, c3, c4);
		}
		return;
	}

	/* perspective division: HCS to ZCS */
	hoco_to_zco(zspan, vez, f1);
	hoco_to_zco(zspan, vez+4, f2);
	hoco_to_zco(zspan, vez+8, f3);
	hoco_to_zco(zspan, vez+12, f4);

	zspan->zbuffunc(zspan, obi, zvlnr, vez, vez+4, vez+8, vez+12);
}

/* ************** ZMASK ******************************** */

#define EXTEND_PIXEL(a)	if(temprectp[a]) {z+= rectz[a]; tot++;}

/* changes the zbuffer to be ready for z-masking: applies an extend-filter, and then clears */
static void zmask_rect(int *rectz, int *rectp, int xs, int ys, int neg)
{
	int len=0, x, y;
	int *temprectp;
	int row1, row2, row3, *curp, *curz;
	
	temprectp= MEM_dupallocN(rectp);
	
	/* extend: if pixel is not filled in, we check surrounding pixels and average z value  */
	
	for(y=1; y<=ys; y++) {
		/* setup row indices */
		row1= (y-2)*xs;
		row2= row1 + xs;
		row3= row2 + xs;
		if(y==1)
			row1= row2;
		else if(y==ys)
			row3= row2;
		
		curp= rectp + (y-1)*xs;
		curz= rectz + (y-1)*xs;
		
		for(x=0; x<xs; x++, curp++, curz++) {
			if(curp[0]==0) {
				int tot= 0;
				float z= 0.0f;
				
				EXTEND_PIXEL(row1);
				EXTEND_PIXEL(row2);
				EXTEND_PIXEL(row3);
				EXTEND_PIXEL(row1 + 1);
				EXTEND_PIXEL(row3 + 1);
				if(x!=xs-1) {
					EXTEND_PIXEL(row1 + 2);
					EXTEND_PIXEL(row2 + 2);
					EXTEND_PIXEL(row3 + 2);
				}					
				if(tot) {
					len++;
					curz[0]= (int)(z/(float)tot);
					curp[0]= -1;	/* env */
				}
			}
			
			if(x!=0) {
				row1++; row2++; row3++;
			}
		}
	}

	MEM_freeN(temprectp);
	
	if(neg); /* z values for negative are already correct */
	else {
		/* clear not filled z values */
		for(len= xs*ys -1; len>=0; len--) {
			if(rectp[len]==0) {
				rectz[len] = -0x7FFFFFFF;
				rectp[len]= -1;	/* env code */
			}	
		}
	}
}




/* ***************** ZBUFFER MAIN ROUTINES **************** */

static void zbuffer_fill_solid(Render *re, RenderPart *pa, RenderLayer *rl, void(*fillfunc)(Render *re, RenderPart*, ZSpan*, int, void*), void *data)
{
	ZbufProjectCache cache[ZBUF_PROJECT_CACHE_SIZE];
	ZSpan zspans[RE_MAX_OSA], *zspan;
	VlakRen *vlr= NULL;
	VertRen *v1, *v2, *v3, *v4;
	Material *ma=0;
	ObjectInstanceRen *obi;
	ObjectRen *obr;
	float obwinmat[4][4], winmat[4][4], bounds[4];
	float ho1[4], ho2[4], ho3[4], ho4[4]={0}, (*jit)[2];
	unsigned int lay= rl->lay, lay_zmask= rl->lay_zmask;
	int i, v, zvlnr, zsample, samples, c1, c2, c3, c4=0;
	short nofill=0, env=0, wire=0, zmaskpass=0;
	short all_z= (rl->layflag & SCE_LAY_ALL_Z) && !(rl->layflag & SCE_LAY_ZMASK);
	short neg_zmask= (rl->layflag & SCE_LAY_ZMASK) && (rl->layflag & SCE_LAY_NEG_ZMASK);

	camera_window_matrix(&re->cam, winmat);
	camera_window_rect_bounds(re->cam.winx, re->cam.winy, &pa->disprect, bounds);
	
	samples= (re->params.osa? re->params.osa: 1);
	samples= MIN2(4, samples-pa->sample);

	for(zsample=0; zsample<samples; zsample++) {
		zspan= &zspans[zsample];

		zbuf_alloc_span(zspan, pa->rectx, pa->recty, re->cam.clipcrop);
		
		/* needed for transform from hoco to zbuffer co */
		zspan->zmulx= ((float)re->cam.winx)/2.0;
		zspan->zmuly= ((float)re->cam.winy)/2.0;

		jit= pxf_sample_offset_table(re);
		if(jit && re->params.osa) {
			zspan->zofsx= -pa->disprect.xmin - jit[pa->sample+zsample][0];
			zspan->zofsy= -pa->disprect.ymin - jit[pa->sample+zsample][1];
		}
		else if(jit) {
			zspan->zofsx= -pa->disprect.xmin - jit[0][0];
			zspan->zofsy= -pa->disprect.ymin - jit[0][1];
		}
		else {
			zspan->zofsx= -pa->disprect.xmin;
			zspan->zofsy= -pa->disprect.ymin;
		}

		/* to center the sample position */
		zspan->zofsx -= 0.5f;
		zspan->zofsy -= 0.5f;
		
		/* the buffers */
		if(zsample == samples-1) {
			zspan->rectp= pa->rectp;
			zspan->recto= pa->recto;

			if(neg_zmask)
				zspan->rectz= pa->rectmask;
			else
				zspan->rectz= pa->rectz;
		}
		else {
			zspan->recto= MEM_mallocN(sizeof(int)*pa->rectx*pa->recty, "recto");
			zspan->rectp= MEM_mallocN(sizeof(int)*pa->rectx*pa->recty, "rectp");
			zspan->rectz= MEM_mallocN(sizeof(int)*pa->rectx*pa->recty, "rectz");
		}

		fillrect(zspan->rectz, pa->rectx, pa->recty, 0x7FFFFFFF);
		fillrect(zspan->rectp, pa->rectx, pa->recty, 0);
		fillrect(zspan->recto, pa->rectx, pa->recty, 0);
	}

	/* in case zmask we fill Z for objects in lay_zmask first, then clear Z, and then do normal zbuffering */
	if(rl->layflag & SCE_LAY_ZMASK)
		zmaskpass= 1;
	
	for(; zmaskpass >=0; zmaskpass--) {
		ma= NULL;

		/* filling methods */
		for(zsample=0; zsample<samples; zsample++) {
			zspan= &zspans[zsample];

			if(zmaskpass && neg_zmask)
				zspan->zbuffunc= zbuffillGLinv4;
			else
				zspan->zbuffunc= zbuffillGL4;
			zspan->zbuflinefunc= zbufline;
		}

		/* regular zbuffering loop, does all sample buffers */
		for(i=0; i < re->db.totinstance; i++) {
			obi= part_get_instance(pa, &re->db.objectinstance[i]);
			obr= obi->obr;

			if(obi->flag & R_HIDDEN)
				continue;

			/* continue happens in 2 different ways... zmaskpass only does lay_zmask stuff */
			if(zmaskpass) {
				if((obi->lay & lay_zmask)==0)
					continue;
			}
			else if(!all_z && !(obi->lay & (lay|lay_zmask)))
				continue;
			
			if(obi->flag & R_TRANSFORMED)
				mul_m4_m4m4(obwinmat, obi->mat, winmat);
			else
				copy_m4_m4(obwinmat, winmat);

			if(box_clip_bounds_m4(obi->obr->boundbox, bounds, obwinmat))
				continue;

			zbuf_project_cache_clear(cache, obr->totvert);

			for(v=0; v<obr->totvlak; v++) {
				if((v & 255)==0) vlr= obr->vlaknodes[v>>8].vlak;
				else vlr++;

				/* the cases: visible for render, only z values, zmask, nothing */
				if(obi->lay & lay) {
					if(vlr->mat!=ma) {
						ma= vlr->mat;
						nofill= (ma->mode & MA_ONLYCAST) || ((ma->mode & MA_TRANSP) && (ma->mode & MA_ZTRANSP));
						env= (ma->mode & MA_ENV);
						wire= (ma->material_type == MA_TYPE_WIRE);
						
						for(zsample=0; zsample<samples; zsample++) {
							if(ma->mode & MA_ZINV || (zmaskpass && neg_zmask))
								zspans[zsample].zbuffunc= zbuffillGLinv4;
							else
								zspans[zsample].zbuffunc= zbuffillGL4;
						}
					}
				}
				else if(all_z || (obi->lay & lay_zmask)) {
					env= 1;
					nofill= 0;
					ma= NULL; 
				}
				else {
					nofill= 1;
					ma= NULL;	/* otherwise nofill can hang */
				}

				if(nofill==0) {
					unsigned short partclip;
					
					v1= vlr->v1;
					v2= vlr->v2;
					v3= vlr->v3;
					v4= vlr->v4;

					c1= zbuf_part_project(cache, v1->index, obwinmat, bounds, v1->co, ho1);
					c2= zbuf_part_project(cache, v2->index, obwinmat, bounds, v2->co, ho2);
					c3= zbuf_part_project(cache, v3->index, obwinmat, bounds, v3->co, ho3);

					/* partclipping doesn't need viewplane clipping */
					partclip= c1 & c2 & c3;
					if(v4) {
						c4= zbuf_part_project(cache, v4->index, obwinmat, bounds, v4->co, ho4);
						partclip &= c4;
					}

					if(partclip==0) {
						
						if(env) zvlnr= -1;
						else zvlnr= v+1;

						c1= camera_hoco_test_clip(ho1);
						c2= camera_hoco_test_clip(ho2);
						c3= camera_hoco_test_clip(ho3);
						if(v4)
							c4= camera_hoco_test_clip(ho4);

						for(zsample=0; zsample<samples; zsample++) {
							zspan= &zspans[zsample];

							if(wire) {
								if(v4)
									zbufclipwire(zspan, i, zvlnr, vlr->ec, ho1, ho2, ho3, ho4, c1, c2, c3, c4);
								else
									zbufclipwire(zspan, i, zvlnr, vlr->ec, ho1, ho2, ho3, 0, c1, c2, c3, 0);
							}
							else {
								/* strands allow to be filled in as quad */
								if(v4 && (vlr->flag & R_STRAND)) {
									zbufclip4(zspan, i, zvlnr, ho1, ho2, ho3, ho4, c1, c2, c3, c4);
								}
								else {
									zbufclip(zspan, i, zvlnr, ho1, ho2, ho3, c1, c2, c3);
									if(v4)
										zbufclip(zspan, i, (env)? zvlnr: zvlnr+RE_QUAD_OFFS, ho1, ho3, ho4, c1, c3, c4);
								}
							}
						}
					}
				}
			}
		}
		
		/* clear all z to close value, so it works as mask for next passes (ztra+strand) */
		if(zmaskpass) {
			for(zsample=0; zsample<samples; zsample++) {
				zspan= &zspans[zsample];

				if(neg_zmask) {
					zspan->rectmask= zspan->rectz;
					if(zsample == samples-1)
						zspan->rectz= pa->rectz;
					else
						zspan->rectz= MEM_mallocN(sizeof(int)*pa->rectx*pa->recty, "rectz");
					fillrect(zspan->rectz, pa->rectx, pa->recty, 0x7FFFFFFF);

					zmask_rect(zspan->rectmask, zspan->rectp, pa->rectx, pa->recty, 1);
				}
				else
					zmask_rect(zspan->rectz, zspan->rectp, pa->rectx, pa->recty, 0);
			}
		}
	}

	for(zsample=0; zsample<samples; zsample++) {
		zspan= &zspans[zsample];

		if(fillfunc)
			fillfunc(re, pa, zspan, pa->sample+zsample, data);

		if(zsample != samples-1) {
			MEM_freeN(zspan->rectz);
			MEM_freeN(zspan->rectp);
			MEM_freeN(zspan->recto);
			if(zspan->rectmask)
				MEM_freeN(zspan->rectmask);
		}

		zbuf_free_span(zspan);
	}
}

void zbuffer_shadow(Render *re, float winmat[][4], LampRen *lar, int *rectz, int size, float jitx, float jity)
{
	ZbufProjectCache cache[ZBUF_PROJECT_CACHE_SIZE];
	ZSpan zspan;
	ObjectInstanceRen *obi;
	ObjectRen *obr;
	VlakRen *vlr= NULL;
	Material *ma= NULL;
	StrandSegment sseg;
	StrandRen *strand= NULL;
	StrandVert *svert;
	StrandBound *sbound;
	float obwinmat[4][4], ho1[4], ho2[4], ho3[4], ho4[4];
	int a, b, c, i, c1, c2, c3, c4, ok=1, lay= -1;

	if(lar->mode & (LA_LAYER|LA_LAYER_SHADOW)) lay= lar->lay;

	/* 1.0f for clipping in clippyra()... bad stuff actually */
	zbuf_alloc_span(&zspan, size, size, 1.0f);
	zspan.zmulx=  ((float)size)/2.0;
	zspan.zmuly=  ((float)size)/2.0;
	/* -0.5f to center the sample position */
	zspan.zofsx= jitx - 0.5f;
	zspan.zofsy= jity - 0.5f;
	
	/* the buffers */
	zspan.rectz= rectz;
	fillrect(rectz, size, size, 0x7FFFFFFE);
	if(lar->buftype==LA_SHADBUF_HALFWAY) {
		zspan.rectz1= MEM_mallocN(size*size*sizeof(int), "seconday z buffer");
		fillrect(zspan.rectz1, size, size, 0x7FFFFFFE);
	}
	
	/* filling methods */
	zspan.zbuflinefunc= zbufline_onlyZ;
	zspan.zbuffunc= zbuffillGL_onlyZ;

	for(i=0, obi=re->db.instancetable.first; obi; i++, obi=obi->next) {
		obr= obi->obr;

		if(obr->ob==re->db.excludeob)
			continue;
		else if(!(obi->lay & lay))
			continue;
		else if(obi->flag & R_HIDDEN)
			continue;

		if(obi->flag & R_TRANSFORMED)
			mul_m4_m4m4(obwinmat, obi->mat, winmat);
		else
			copy_m4_m4(obwinmat, winmat);

		if(box_clip_bounds_m4(obi->obr->boundbox, NULL, obwinmat))
			continue;

		zbuf_project_cache_clear(cache, obr->totvert);

		/* faces */
		for(a=0; a<obr->totvlak; a++) {

			if((a & 255)==0) vlr= obr->vlaknodes[a>>8].vlak;
			else vlr++;

			/* note, these conditions are copied in shadowbuf_autoclip() */
			if(vlr->mat!= ma) {
				ma= vlr->mat;
				ok= 1;
				if((ma->mode & MA_SHADBUF)==0) ok= 0;
			}

			if(ok) {
				c1= zbuf_shadow_project(cache, vlr->v1->index, obwinmat, vlr->v1->co, ho1);
				c2= zbuf_shadow_project(cache, vlr->v2->index, obwinmat, vlr->v2->co, ho2);
				c3= zbuf_shadow_project(cache, vlr->v3->index, obwinmat, vlr->v3->co, ho3);

				if((ma->material_type == MA_TYPE_WIRE) || (vlr->flag & R_STRAND)) {
					if(vlr->v4) {
						c4= zbuf_shadow_project(cache, vlr->v4->index, obwinmat, vlr->v4->co, ho4);
						zbufclipwire(&zspan, 0, a+1, vlr->ec, ho1, ho2, ho3, ho4, c1, c2, c3, c4);
					}
					else
						zbufclipwire(&zspan, 0, a+1, vlr->ec, ho1, ho2, ho3, 0, c1, c2, c3, 0);
				}
				else {
					if(vlr->v4) {
						c4= zbuf_shadow_project(cache, vlr->v4->index, obwinmat, vlr->v4->co, ho4);
						zbufclip4(&zspan, 0, 0, ho1, ho2, ho3, ho4, c1, c2, c3, c4);
					}
					else
						zbufclip(&zspan, 0, 0, ho1, ho2, ho3, c1, c2, c3);
				}
			}

			if((a & 255)==255 && re->cb.test_break(re->cb.tbh)) 
				break;
		}

		/* strands */
		if(obr->strandbuf) {
			/* for each bounding box containing a number of strands */
			sbound= obr->strandbuf->bound;
			for(c=0; c<obr->strandbuf->totbound; c++, sbound++) {
				if(box_clip_bounds_m4(sbound->boundbox, NULL, obwinmat))
					continue;

				/* for each strand in this bounding box */
				for(a=sbound->start; a<sbound->end; a++) {
					strand= render_object_strand_get(obr, a);

					sseg.obi= obi;
					sseg.buffer= strand->buffer;
					sseg.sqadaptcos= sseg.buffer->adaptcos;
					sseg.sqadaptcos *= sseg.sqadaptcos;
					sseg.strand= strand;
					svert= strand->vert;

					/* note, these conditions are copied in shadowbuf_autoclip() */
					if(sseg.buffer->ma!= ma) {
						ma= sseg.buffer->ma;
						ok= 1;
						if((ma->mode & MA_SHADBUF)==0) ok= 0;
					}

					if(ok && (sseg.buffer->lay & lay)) {
						zbuf_project_cache_clear(cache, strand->totvert);

						for(b=0; b<strand->totvert-1; b++, svert++) {
							sseg.v[0]= (b > 0)? (svert-1): svert;
							sseg.v[1]= svert;
							sseg.v[2]= svert+1;
							sseg.v[3]= (b < strand->totvert-2)? svert+2: svert+1;

							c1= zbuf_shadow_project(cache, sseg.v[0]-strand->vert, obwinmat, sseg.v[0]->co, ho1);
							c2= zbuf_shadow_project(cache, sseg.v[1]-strand->vert, obwinmat, sseg.v[1]->co, ho2);
							c3= zbuf_shadow_project(cache, sseg.v[2]-strand->vert, obwinmat, sseg.v[2]->co, ho3);
							c4= zbuf_shadow_project(cache, sseg.v[3]-strand->vert, obwinmat, sseg.v[3]->co, ho4);

							if(!(c1 & c2 & c3 & c4))
								render_strand_segment(re, winmat, NULL, &zspan, 1, &sseg);
						}
					}

					if((a & 255)==255 && re->cb.test_break(re->cb.tbh)) 
						break;
				}
			}
		}

		if(re->cb.test_break(re->cb.tbh)) 
			break;
	}
	
	/* merge buffers */
	if(lar->buftype==LA_SHADBUF_HALFWAY) {
		for(a=size*size -1; a>=0; a--)
			rectz[a]= (rectz[a]>>1) + (zspan.rectz1[a]>>1);
		
		MEM_freeN(zspan.rectz1);
	}
	
	zbuf_free_span(&zspan);
}

static void zbuffill_sss(ZSpan *zspan, int obi, int zvlnr, float *v1, float *v2, float *v3, float *v4)
{
	double zxd, zyd, zy0, z;
	float x0, y0, x1, y1, x2, y2, z0, z1, z2, xx1, *span1, *span2;
	int x, y, sn1, sn2, rectx= zspan->rectx, my0, my2;
	/* init */
	zbuf_init_span(zspan);
	
	/* set spans */
	zbuf_add_to_span(zspan, v1, v2);
	zbuf_add_to_span(zspan, v2, v3);
	if(v4) {
		zbuf_add_to_span(zspan, v3, v4);
		zbuf_add_to_span(zspan, v4, v1);
	}
	else 
		zbuf_add_to_span(zspan, v3, v1);
	
	/* clipped */
	if(zspan->minp2==NULL || zspan->maxp2==NULL) return;
	
	if(zspan->miny1 < zspan->miny2) my0= zspan->miny2; else my0= zspan->miny1;
	if(zspan->maxy1 > zspan->maxy2) my2= zspan->maxy2; else my2= zspan->maxy1;
	
	if(my2<my0) return;
	
	/* ZBUF DX DY, in floats still */
	x1= v1[0]- v2[0];
	x2= v2[0]- v3[0];
	y1= v1[1]- v2[1];
	y2= v2[1]- v3[1];
	z1= v1[2]- v2[2];
	z2= v2[2]- v3[2];
	
	x0= y1*z2-z1*y2;
	y0= z1*x2-x1*z2;
	z0= x1*y2-y1*x2;
	
	if(z0==0.0f) return;
	
	xx1= (x0*v1[0] + y0*v1[1])/z0 + v1[2];
	zxd= -(double)x0/(double)z0;
	zyd= -(double)y0/(double)z0;
	zy0= ((double)my2)*zyd + (double)xx1;
	
	/* correct span */
	sn1= (my0 + my2)/2;
	if(zspan->span1[sn1] < zspan->span2[sn1]) {
		span1= zspan->span1+my2;
		span2= zspan->span2+my2;
	}
	else {
		span1= zspan->span2+my2;
		span2= zspan->span1+my2;
	}
	
	for(y=my2; y>=my0; y--, span1--, span2--) {
		sn1= floorf(*span1);
		sn2= floorf(*span2);
		sn1++; 
		
		if(sn2>=rectx) sn2= rectx-1;
		if(sn1<0) sn1= 0;
		
		z= (double)sn1*zxd + zy0;
		
		for(x= sn1; x<=sn2; x++, z+=zxd)
			zspan->sss_func(zspan->sss_handle, obi, zvlnr, x, y, z);
		
		zy0 -= zyd;
	}
}

void zbuffer_sss(Render *re, RenderPart *pa, unsigned int lay, void *handle, void (*func)(void*, int, int, int, int, int))
{
	ZbufProjectCache cache[ZBUF_PROJECT_CACHE_SIZE];
	ZSpan zspan;
	ObjectInstanceRen *obi;
	ObjectRen *obr;
	VlakRen *vlr= NULL;
	VertRen *v1, *v2, *v3, *v4;
	Material *ma=0, *sss_ma= re->db.sss_mat;
	float obwinmat[4][4], winmat[4][4], bounds[4];
	float ho1[4], ho2[4], ho3[4], ho4[4]={0};
	int i, v, zvlnr, c1, c2, c3, c4=0;
	short nofill=0, env=0, wire=0;
	
	camera_window_matrix(&re->cam, winmat);
	camera_window_rect_bounds(re->cam.winx, re->cam.winy, &pa->disprect, bounds);

	zbuf_alloc_span(&zspan, pa->rectx, pa->recty, re->cam.clipcrop);

	zspan.sss_handle= handle;
	zspan.sss_func= func;
	
	/* needed for transform from hoco to zbuffer co */
	zspan.zmulx=  ((float)re->cam.winx)/2.0;
	zspan.zmuly=  ((float)re->cam.winy)/2.0;
	
	/* -0.5f to center the sample position */
	zspan.zofsx= -pa->disprect.xmin - 0.5f;
	zspan.zofsy= -pa->disprect.ymin - 0.5f;
	
	/* filling methods */
	zspan.zbuffunc= zbuffill_sss;

	/* fill front and back zbuffer */
	if(pa->rectz) {
		fillrect(pa->recto, pa->rectx, pa->recty, 0); 
		fillrect(pa->rectp, pa->rectx, pa->recty, 0); 
		fillrect(pa->rectz, pa->rectx, pa->recty, 0x7FFFFFFF);
	}
	if(pa->rectbackz) {
		fillrect(pa->rectbacko, pa->rectx, pa->recty, 0); 
		fillrect(pa->rectbackp, pa->rectx, pa->recty, 0); 
		fillrect(pa->rectbackz, pa->rectx, pa->recty, -0x7FFFFFFF);
	}

	for(i=0, obi=re->db.instancetable.first; obi; i++, obi=obi->next) {
		obr= obi->obr;

		if(!(obi->lay & lay))
			continue;

		if(obi->flag & R_TRANSFORMED)
			mul_m4_m4m4(obwinmat, obi->mat, winmat);
		else
			copy_m4_m4(obwinmat, winmat);

		if(box_clip_bounds_m4(obi->obr->boundbox, bounds, obwinmat))
			continue;

		zbuf_project_cache_clear(cache, obr->totvert);

		for(v=0; v<obr->totvlak; v++) {
			if((v & 255)==0) vlr= obr->vlaknodes[v>>8].vlak;
			else vlr++;
			
			if(material_in_material(vlr->mat, sss_ma)) {
				/* three cases, visible for render, only z values and nothing */
				if(obi->lay & lay) {
					if(vlr->mat!=ma) {
						ma= vlr->mat;
						nofill= ma->mode & MA_ONLYCAST;
						env= (ma->mode & MA_ENV);
						wire= (ma->material_type == MA_TYPE_WIRE);
					}
				}
				else {
					nofill= 1;
					ma= NULL;	/* otherwise nofill can hang */
				}
				
				if(nofill==0 && wire==0 && env==0) {
					unsigned short partclip;
					
					v1= vlr->v1;
					v2= vlr->v2;
					v3= vlr->v3;
					v4= vlr->v4;

					c1= zbuf_part_project(cache, v1->index, obwinmat, bounds, v1->co, ho1);
					c2= zbuf_part_project(cache, v2->index, obwinmat, bounds, v2->co, ho2);
					c3= zbuf_part_project(cache, v3->index, obwinmat, bounds, v3->co, ho3);

					/* partclipping doesn't need viewplane clipping */
					partclip= c1 & c2 & c3;
					if(v4) {
						c4= zbuf_part_project(cache, v4->index, obwinmat, bounds, v4->co, ho4);
						partclip &= c4;
					}

					if(partclip==0) {
						c1= camera_hoco_test_clip(ho1);
						c2= camera_hoco_test_clip(ho2);
						c3= camera_hoco_test_clip(ho3);

						zvlnr= v+1;
						zbufclip(&zspan, i, zvlnr, ho1, ho2, ho3, c1, c2, c3);
						if(v4) {
							c4= camera_hoco_test_clip(ho4);
							zbufclip(&zspan, i, zvlnr+RE_QUAD_OFFS, ho1, ho3, ho4, c1, c3, c4);
						}
					}
				}
			}
		}
	}
		
	zbuf_free_span(&zspan);
}

/* ******************** ABUF ************************* */

/**
 * Copy results from the solid face z buffering to the transparent
 * buffer.
 */
static void copyto_abufz(Render *re, RenderPart *pa, int *arectz, int *rectmask, int sample)
{
	PixStr *ps, **rd;
	int x, y, *rza, *rma;
	
	if(re->params.osa==0) {
		if(!pa->rectz)
			fillrect(arectz, pa->rectx, pa->recty, 0x7FFFFFFE);
		else
			memcpy(arectz, pa->rectz, sizeof(int)*pa->rectx*pa->recty);

		if(rectmask && pa->rectmask)
			memcpy(rectmask, pa->rectmask, sizeof(int)*pa->rectx*pa->recty);

		return;
	}
	else if(!pa->rectdaps) {
		fillrect(arectz, pa->rectx, pa->recty, 0x7FFFFFFE);
		return;
	}
	
	rza= arectz;
	rma= rectmask;
	rd= pa->rectdaps;

	sample= (1<<sample);
	
	for(y=0; y<pa->recty; y++) {
		for(x=0; x<pa->rectx; x++) {
			
			*rza= 0x7FFFFFFF;
			if(rectmask) *rma= 0x7FFFFFFF;
			if(*rd) {	
				/* when there's a sky pixstruct, fill in sky-Z, otherwise solid Z */
				for(ps= (PixStr *)(*rd); ps; ps= ps->next) {
					if(sample & ps->mask) {
						*rza= ps->z;
						if(rectmask) *rma= ps->maskz;
						break;
					}
				}
			}
			
			rd++; rza++, rma++;
		}
	}
}


/* ------------------------------------------------------------------------ */

/**
 * Do accumulation z buffering.
 */

static int zbuffer_abuf(Render *re, RenderPart *pa, APixstr *apixbuf, ListBase *apsmbase, unsigned int lay, int negzmask, float winmat[][4], int winx, int winy, int samples, float (*jit)[2], float clipcrop, int shadow)
{
	ZbufProjectCache cache[ZBUF_PROJECT_CACHE_SIZE];
	ZSpan zspans[16], *zspan;	/* MAX_OSA */
	Material *ma=NULL;
	ObjectInstanceRen *obi;
	ObjectRen *obr;
	VlakRen *vlr=NULL;
	VertRen *v1, *v2, *v3, *v4;
	float vec[3], hoco[4], mul, zval, fval;
	float obwinmat[4][4], bounds[4], ho1[4], ho2[4], ho3[4], ho4[4]={0};
	int i, v, zvlnr, c1, c2, c3, c4=0, dofill= 0;
	int zsample, polygon_offset;

	camera_window_rect_bounds(winx, winy, &pa->disprect, bounds);

	for(zsample=0; zsample<samples; zsample++) {
		zspan= &zspans[zsample];

		zbuf_alloc_span(zspan, pa->rectx, pa->recty, clipcrop);
		
		/* needed for transform from hoco to zbuffer co */
		zspan->zmulx=  ((float)winx)/2.0;
		zspan->zmuly=  ((float)winy)/2.0;
		
		/* the buffers */
		zspan->arectz= MEM_mallocN(sizeof(int)*pa->rectx*pa->recty, "Arectz");
		zspan->apixbuf= apixbuf;
		zspan->apsmbase= apsmbase;
		
		if(negzmask)
			zspan->rectmask= MEM_mallocN(sizeof(int)*pa->rectx*pa->recty, "Arectmask");

		/* filling methods */
		zspan->zbuffunc= zbuffillAc4;
		zspan->zbuflinefunc= zbuflineAc;

		copyto_abufz(re, pa, zspan->arectz, zspan->rectmask, zsample);	/* init zbuffer */
		zspan->mask= 1<<zsample;

		if(jit && samples > 1) {
			zspan->zofsx= -pa->disprect.xmin + jit[zsample][0];
			zspan->zofsy= -pa->disprect.ymin + jit[zsample][1];
		}
		else if(jit) {
			zspan->zofsx= -pa->disprect.xmin + jit[0][0];
			zspan->zofsy= -pa->disprect.ymin + jit[0][1];
		}
		else {
			zspan->zofsx= -pa->disprect.xmin;
			zspan->zofsy= -pa->disprect.ymin;
		}

		/* to center the sample position */
		zspan->zofsx -= 0.5f;
		zspan->zofsy -= 0.5f;
	}
	
	/* we use this to test if nothing was filled in */
	zvlnr= 0;

	for(i=0; i < re->db.totinstance; i++) {
		obi= part_get_instance(pa, &re->db.objectinstance[i]);
		obr= obi->obr;

		if(!(obi->lay & lay))
			continue;
		else if(obi->flag & R_HIDDEN)
			continue;

		if(obi->flag & R_TRANSFORMED)
			mul_m4_m4m4(obwinmat, obi->mat, winmat);
		else
			copy_m4_m4(obwinmat, winmat);

		if(box_clip_bounds_m4(obi->obr->boundbox, bounds, obwinmat))
			continue;

		zbuf_project_cache_clear(cache, obr->totvert);

		for(v=0; v<obr->totvlak; v++) {
			if((v & 255)==0)
				vlr= obr->vlaknodes[v>>8].vlak;
			else vlr++;
			
			if(vlr->mat!=ma) {
				ma= vlr->mat;
				dofill= shadow || (((ma->mode & MA_TRANSP) && (ma->mode & MA_ZTRANSP)) && !(ma->mode & MA_ONLYCAST));
			}
			
			if(dofill) {
				unsigned short partclip;
				
				v1= vlr->v1;
				v2= vlr->v2;
				v3= vlr->v3;
				v4= vlr->v4;

				c1= zbuf_part_project(cache, v1->index, obwinmat, bounds, v1->co, ho1);
				c2= zbuf_part_project(cache, v2->index, obwinmat, bounds, v2->co, ho2);
				c3= zbuf_part_project(cache, v3->index, obwinmat, bounds, v3->co, ho3);

				/* partclipping doesn't need viewplane clipping */
				partclip= c1 & c2 & c3;
				if(v4) {
					c4= zbuf_part_project(cache, v4->index, obwinmat, bounds, v4->co, ho4);
					partclip &= c4;
				}

				if(partclip==0) {
					/* a little advantage for transp rendering (a z offset) */
					if(!shadow && ma->zoffs != 0.0) {
						mul= 0x7FFFFFFF;
						zval= mul*(1.0+ho1[2]/ho1[3]);

						copy_v3_v3(vec, v1->co);
						/* z is negative, otherwise its being clipped */ 
						vec[2]-= ma->zoffs;
						camera_matrix_co_to_hoco(obwinmat, hoco, vec);
						fval= mul*(1.0+hoco[2]/hoco[3]);

						polygon_offset= (int) fabs(zval - fval );
					}
					else polygon_offset= 0;
					
					zvlnr= v+1;

					c1= camera_hoco_test_clip(ho1);
					c2= camera_hoco_test_clip(ho2);
					c3= camera_hoco_test_clip(ho3);
					if(v4)
						c4= camera_hoco_test_clip(ho4);

					for(zsample=0; zsample<samples; zsample++) {
						zspan= &zspans[zsample];
						zspan->polygon_offset= polygon_offset;
			
						if(ma->material_type == MA_TYPE_WIRE) {
							if(v4)
								zbufclipwire(zspan, i, zvlnr, vlr->ec, ho1, ho2, ho3, ho4, c1, c2, c3, c4);
							else
								zbufclipwire(zspan, i, zvlnr, vlr->ec, ho1, ho2, ho3, 0, c1, c2, c3, 0);
						}
						else {
							if(v4 && (vlr->flag & R_STRAND)) {
								zbufclip4(zspan, i, zvlnr, ho1, ho2, ho3, ho4, c1, c2, c3, c4);
							}
							else {
								zbufclip(zspan, i, zvlnr, ho1, ho2, ho3, c1, c2, c3);
								if(v4)
									zbufclip(zspan, i, zvlnr+RE_QUAD_OFFS, ho1, ho3, ho4, c1, c3, c4);
							}
						}
					}
				}
				if((v & 255)==255) 
					if(re->cb.test_break(re->cb.tbh)) 
						break; 
			}
		}

		if(re->cb.test_break(re->cb.tbh)) break;
	}
	
	for(zsample=0; zsample<samples; zsample++) {
		zspan= &zspans[zsample];
		MEM_freeN(zspan->arectz);
		if(zspan->rectmask)
			MEM_freeN(zspan->rectmask);
		zbuf_free_span(zspan);
	}
	
	return zvlnr;
}

static int zbuffer_abuf_render(Render *re, RenderPart *pa, APixstr *apixbuf, APixstrand *apixbufstrand, ListBase *apsmbase, RenderLayer *rl, StrandShadeCache *sscache)
{
	float winmat[4][4], (*jit)[2];
	int samples, negzmask, doztra= 0;

	samples= (re->params.osa)? re->params.osa: 1;
	negzmask= ((rl->layflag & SCE_LAY_ZMASK) && (rl->layflag & SCE_LAY_NEG_ZMASK));

	jit= pxf_sample_offset_table(re);
	
	camera_window_matrix(&re->cam, winmat);

	if(rl->layflag & SCE_LAY_ZTRA)
		doztra+= zbuffer_abuf(re, pa, apixbuf, apsmbase, rl->lay, negzmask, winmat, re->cam.winx, re->cam.winy, samples, jit, re->cam.clipcrop, 0);
	if((rl->layflag & SCE_LAY_STRAND) && apixbufstrand)
		doztra+= zbuffer_strands_abuf(re, pa, apixbufstrand, apsmbase, rl->lay, negzmask, winmat, re->cam.winx, re->cam.winy, samples, jit, re->cam.clipcrop, 0, sscache);

	return doztra;
}

void zbuffer_abuf_shadow(Render *re, LampRen *lar, float winmat[][4], APixstr *apixbuf, APixstrand *apixbufstrand, ListBase *apsmbase, int size, int samples, float (*jit)[2])
{
	RenderPart pa;
	int lay= -1;

	if(lar->mode & LA_LAYER) lay= lar->lay;

	memset(&pa, 0, sizeof(RenderPart));
	pa.rectx= size;
	pa.recty= size;
	pa.disprect.xmin= 0;
	pa.disprect.ymin= 0;
	pa.disprect.xmax= size;
	pa.disprect.ymax= size;

	zbuffer_abuf(re, &pa, apixbuf, apsmbase, lay, 0, winmat, size, size, samples, jit, 1.0f, 1);
	if(apixbufstrand)
		zbuffer_strands_abuf(re, &pa, apixbufstrand, apsmbase, lay, 0, winmat, size, size, samples, jit, 1.0f, 1, NULL);
}

int zbuffer_alpha(Render *re, RenderPart *pa, RenderLayer *rl)
{
	int doztra;

	if(re->cb.test_break(re->cb.tbh))
		return 0;

	pa->apixbuf= MEM_callocN(pa->rectx*pa->recty*sizeof(APixstr), "apixbuf");
	if(re->db.totstrand && (rl->layflag & SCE_LAY_STRAND)) {
		pa->apixbufstrand= MEM_callocN(pa->rectx*pa->recty*sizeof(APixstrand), "apixbufstrand");
		pa->sscache= strand_shade_cache_create();
	}

	/* fill the Apixbuf */
	doztra= zbuffer_abuf_render(re, pa, pa->apixbuf, pa->apixbufstrand, &pa->apsmbase, rl, pa->sscache);

	if(doztra == 0) {
		/* nothing filled in */
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

	return doztra;
}

/**************************** Solid Rasterization *************************/

typedef struct ZbufSolidData {
	RenderLayer *rl;
	ListBase *psmlist;
	void (*edgefunc)(struct Render *re, struct RenderPart *pa, float *rectf, int *rectz);
	float *edgerect;
} ZbufSolidData;

static PixStrMain *addpsmain(ListBase *lb)
{
	PixStrMain *psm;
	
	psm= (PixStrMain *)MEM_mallocN(sizeof(PixStrMain),"pixstrMain");
	BLI_addtail(lb, psm);
	
	psm->ps= (PixStr *)MEM_mallocN(4096*sizeof(PixStr),"pixstr");
	psm->counter= 0;
	
	return psm;
}

void free_pixel_structs(ListBase *lb)
{
	PixStrMain *psm, *psmnext;
	
	for(psm= lb->first; psm; psm= psmnext) {
		psmnext= psm->next;
		if(psm->ps)
			MEM_freeN(psm->ps);
		MEM_freeN(psm);
	}
	lb->first= lb->last= NULL;
}

static void addps(ListBase *lb, PixStr **rd, int obi, int facenr, int z, int maskz, unsigned short mask)
{
	PixStrMain *psm;
	PixStr *ps, *last= NULL;
	
	if(*rd) {	
		ps= (PixStr *)(*rd);
		
		while(ps) {
			if( ps->obi == obi && ps->facenr == facenr ) {
				ps->mask |= mask;
				return;
			}
			last= ps;
			ps= ps->next;
		}
	}
	
	/* make new PS (pixel struct) */
	psm= lb->last;
	
	if(psm->counter==4095)
		psm= addpsmain(lb);
	
	ps= psm->ps + psm->counter++;
	
	if(last) last->next= ps;
	else *rd= (void*)ps;
	
	ps->next= NULL;
	ps->obi= obi;
	ps->facenr= facenr;
	ps->z= z;
	ps->maskz= maskz;
	ps->mask = mask;
	ps->shadfac= 0;
}

#if 0
static unsigned short make_solid_mask(PixStr *ps)
{ 
	unsigned short mask;

	mask= ps->mask;
	for(ps= ps->next; ps; ps= ps->next)
		mask |= ps->mask;

	return mask;
}
#endif

static void make_pixelstructs(Render *re, RenderPart *pa, ZSpan *zspan, int sample, void *data)
{
	ZbufSolidData *sdata= (ZbufSolidData*)data;
	ListBase *lb= sdata->psmlist;
	PixStr **rd= pa->rectdaps;
	int *ro= zspan->recto;
	int *rp= zspan->rectp;
	int *rz= zspan->rectz;
	int *rm= zspan->rectmask;
	int x, y;
	int mask= (re->params.osa)? 1<<sample: 0xFFFF;

	for(y=0; y<pa->recty; y++) {
		for(x=0; x<pa->rectx; x++, rd++, rp++, ro++, rz++, rm++) {
			if(*rp) {
				addps(lb, rd, *ro, *rp, *rz, (zspan->rectmask)? *rm: 0, mask);
			}
		}
	}

	if(sdata->edgerect)
		sdata->edgefunc(re, pa, sdata->edgerect, zspan->rectz);
}

void zbuffer_solid(Render *re, RenderPart *pa, RenderLayer *rl, ListBase *psmlist,
	void (*edgefunc)(struct Render *re, struct RenderPart *pa, float *rectf, int *rectz),
	float *edgerect)
{
	int osa= (re->params.osa)? re->params.osa: 1;

	if(re->cb.test_break(re->cb.tbh))
		return;

	/* initialize pixelstructs */
	addpsmain(psmlist);
	pa->rectdaps= MEM_callocN(sizeof(void*)*pa->rectx*pa->recty+4, "zbufDArectd");
	
	/* always fill visibility */
	for(pa->sample=0; pa->sample<osa; pa->sample+=4) {
		ZbufSolidData sdata;

		sdata.rl= rl;
		sdata.psmlist= psmlist;
		sdata.edgerect= edgerect;
		sdata.edgefunc= edgefunc;
		zbuffer_fill_solid(re, pa, rl, make_pixelstructs, &sdata);
		if(re->cb.test_break(re->cb.tbh)) break; 
	}
}


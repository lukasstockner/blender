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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Contributor(s): Blender Foundation.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <math.h>
#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"

#include "camera.h"
#include "part.h"
#include "render_types.h"

/******************************* Create/Free *********************************/

void parts_create(Render *re)
{
	int nr, xd, yd, partx, party, xparts, yparts;
	int xminb, xmaxb, yminb, ymaxb;
	
	parts_free(re);
	
	/* this is render info for caller, is not reset when parts are freed! */
	re->cb.i.totpart= 0;
	re->cb.i.curpart= 0;
	re->cb.i.partsdone= 0;
	
	/* just for readable code.. */
	xminb= re->disprect.xmin;
	yminb= re->disprect.ymin;
	xmaxb= re->disprect.xmax;
	ymaxb= re->disprect.ymax;
	
	xparts= re->params.r.xparts;
	yparts= re->params.r.yparts;
	
	/* mininum part size, but for exr tile saving it was checked already */
	if(!(re->params.r.scemode & (R_EXR_TILE_FILE|R_FULL_SAMPLE))) {
		if(re->cam.type == R_CAM_PANO) {
			if(ceil(re->rectx/(float)xparts) < 8) 
				xparts= 1 + re->rectx/8;
		}
		else
			if(ceil(re->rectx/(float)xparts) < 64) 
				xparts= 1 + re->rectx/64;
		
		if(ceil(re->recty/(float)yparts) < 64) 
			yparts= 1 + re->recty/64;
	}
	
	/* part size */
	partx= ceil(re->rectx/(float)xparts);
	party= ceil(re->recty/(float)yparts);
	
	re->xparts= xparts;
	re->yparts= yparts;
	re->partx= partx;
	re->party= party;
	
	/* calculate rotation factor of 1 pixel */
	if(re->cam.type == R_CAM_PANO)
		re->cam.panophi= panorama_pixel_rot(re);
	
	for(nr=0; nr<xparts*yparts; nr++) {
		rcti disprect;
		int rectx, recty;
		
		xd= (nr % xparts);
		yd= (nr-xd)/xparts;
		
		disprect.xmin= xminb+ xd*partx;
		disprect.ymin= yminb+ yd*party;
		
		/* ensure we cover the entire picture, so last parts go to end */
		if(xd<xparts-1) {
			disprect.xmax= disprect.xmin + partx;
			if(disprect.xmax > xmaxb)
				disprect.xmax = xmaxb;
		}
		else disprect.xmax= xmaxb;
		
		if(yd<yparts-1) {
			disprect.ymax= disprect.ymin + party;
			if(disprect.ymax > ymaxb)
				disprect.ymax = ymaxb;
		}
		else disprect.ymax= ymaxb;
		
		rectx= disprect.xmax - disprect.xmin;
		recty= disprect.ymax - disprect.ymin;
		
		/* so, now can we add this part? */
		if(rectx>0 && recty>0) {
			RenderPart *pa= MEM_callocN(sizeof(RenderPart), "new part");

			pa->re= re;
			
			/* Non-box filters need 2 pixels extra to work */
			if((re->params.r.filtertype || (re->params.r.mode & R_EDGE))) {
				pa->crop= 2;
				disprect.xmin -= pa->crop;
				disprect.ymin -= pa->crop;
				disprect.xmax += pa->crop;
				disprect.ymax += pa->crop;
				rectx+= 2*pa->crop;
				recty+= 2*pa->crop;
			}
			pa->disprect= disprect;
			pa->rectx= rectx;
			pa->recty= recty;

			BLI_addtail(&re->parts, pa);
			re->cb.i.totpart++;
		}
	}
}

void parts_free(Render *re)
{
	RenderPart *part;
	
	for(part=re->parts.first; part; part=part->next) {
		if(part->rectp) MEM_freeN(part->rectp);
		if(part->rectz) MEM_freeN(part->rectz);
	}

	BLI_freelistN(&re->parts);
}

/******************************** Find Next **********************************/

/* for panorama, we render per Y slice with, and update
   camera parameters when we go the next slice */
int parts_find_next_slice(Render *re, int *slice, int *minx, rctf *viewplane)
{
	RenderPart *pa, *best= NULL;
	int found= 0;

	*minx= re->cam.winx;

	if(re->cam.type == R_CAM_PANO) {
		/* most left part of the non-rendering parts */
		for(pa= re->parts.first; pa; pa= pa->next) {
			if(pa->ready==0 && pa->nr==0) {
				if(pa->disprect.xmin < *minx) {
					found= 1;
					best= pa;
					*minx= pa->disprect.xmin;
				}
			}
		}

		if(found)
			panorama_set_camera_params(re, &best->disprect, viewplane);
	}
	else {
		/* for regular render, just one 'slice' */
		found= (*slice == 0);
	}
	
	(*slice)++;

	return found;
}

RenderPart *parts_find_next(Render *re, int minx)
{
	RenderPart *pa, *best= NULL;
	int centx=re->cam.winx/2, centy=re->cam.winy/2, tot=1;
	int mindist, distx, disty;
	
	/* find center of rendered parts, image center counts for 1 too */
	for(pa= re->parts.first; pa; pa= pa->next) {
		if(pa->ready) {
			centx+= (pa->disprect.xmin+pa->disprect.xmax)/2;
			centy+= (pa->disprect.ymin+pa->disprect.ymax)/2;
			tot++;
		}
	}

	centx/=tot;
	centy/=tot;
	
	/* closest of the non-rendering parts */
	mindist= re->cam.winx*re->cam.winy;
	for(pa= re->parts.first; pa; pa= pa->next) {
		if(pa->ready==0 && pa->nr==0) {
			distx= centx - (pa->disprect.xmin+pa->disprect.xmax)/2;
			disty= centy - (pa->disprect.ymin+pa->disprect.ymax)/2;
			distx= (int)sqrt(distx*distx + disty*disty);
			if(distx<mindist) {
				if(re->cam.type == R_CAM_PANO) {
					if(pa->disprect.xmin==minx) {
						best= pa;
						mindist= distx;
					}
				}
				else {
					best= pa;
					mindist= distx;
				}
			}
		}
	}

	return best;
}


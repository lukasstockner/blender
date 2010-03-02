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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributors: 2004/2005/2006 Blender Foundation, full recode
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_rand.h"

#include "DNA_camera_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h"
#include "DNA_texture_types.h"

#include "BKE_DerivedMesh.h"

#include "camera.h"
#include "database.h"
#include "object.h"
#include "object_halo.h"
#include "pixelfilter.h"
#include "render_types.h"
#include "shading.h"
#include "zbuf.h"

#include "RE_render_ext.h"

/****************************************************************************/

HaloRen *render_object_halo_get(ObjectRen *obr, int nr)
{
	HaloRen *har;
	int a;

	a= render_object_chunk_get((void**)&obr->bloha, &obr->blohalen, nr, sizeof(void*));
	har= obr->bloha[a];

	if(har == NULL) {
		har= MEM_callocN(256*sizeof(HaloRen), "findOrAddHalo");
		obr->bloha[a]= har;
	}

	return har + (nr & 255);
}

/****************************************************************************/

HaloRen *halo_init(Render *re, ObjectRen *obr, Material *ma,   float *vec,   float *vec1, 
				  float *orco,   float hasize,   float vectsize, int seed)
{
	HaloRen *har;
	MTex *mtex;
	float tin, tr, tg, tb, ta;
	float xn, yn, zn, texvec[3], hoco[4], hoco1[4];

	if(hasize==0.0) return NULL;

	camera_halo_co_to_hoco(&re->cam, hoco, vec);;
	if(hoco[3]==0.0) return NULL;
	if(vec1) {
		camera_halo_co_to_hoco(&re->cam, hoco1, vec1);
		if(hoco1[3]==0.0) return NULL;
	}

	har= render_object_halo_get(obr, obr->tothalo++);
	copy_v3_v3(har->co, vec);
	har->hasize= hasize;

	/* actual projectvert is done in function halos_project() because of parts/border/pano */
	/* we do it here for sorting of halos */
	zn= hoco[3];
	har->xs= 0.5*re->cam.winx*(hoco[0]/zn);
	har->ys= 0.5*re->cam.winy*(hoco[1]/zn);
	har->zs= 0x7FFFFF*(hoco[2]/zn);
	
	har->zBufDist = 0x7FFFFFFF*(hoco[2]/zn); 
	
	/* halovect */
	if(vec1) {

		har->type |= HA_VECT;

		xn=  har->xs - 0.5*re->cam.winx*(hoco1[0]/hoco1[3]);
		yn=  har->ys - 0.5*re->cam.winy*(hoco1[1]/hoco1[3]);
		if(xn==0.0 || (xn==0.0 && yn==0.0)) zn= 0.0;
		else zn= atan2(yn, xn);

		har->sin= sin(zn);
		har->cos= cos(zn);
		zn= len_v3v3(vec1, vec);

		har->hasize= vectsize*zn + (1.0-vectsize)*hasize;
		
		sub_v3_v3v3(har->no, vec, vec1);
		normalize_v3(har->no);
	}

	if(ma->mode & MA_HALO_XALPHA) har->type |= HA_XALPHA;

	har->alfa= ma->alpha;
	har->r= ma->r;
	har->g= ma->g;
	har->b= ma->b;
	har->add= (255.0*ma->add);
	har->mat= ma;
	har->hard= ma->har;
	har->seed= seed % 256;

	if(ma->mode & MA_STAR) har->starpoints= ma->starc;
	if(ma->mode & MA_HALO_LINES) har->linec= ma->linec;
	if(ma->mode & MA_HALO_RINGS) har->ringc= ma->ringc;
	if(ma->mode & MA_HALO_FLARE) har->flarec= ma->flarec;


	if(ma->mtex[0]) {

		if( (ma->mode & MA_HALOTEX) ) har->tex= 1;
		else {

			mtex= ma->mtex[0];
			copy_v3_v3(texvec, vec);

			if(mtex->texco & TEXCO_NORM) {
				;
			}
			else if(mtex->texco & TEXCO_OBJECT) {
				/* texvec[0]+= imatbase->ivec[0]; */
				/* texvec[1]+= imatbase->ivec[1]; */
				/* texvec[2]+= imatbase->ivec[2]; */
				/* mul_m3_v3(imatbase->imat, texvec); */
			}
			else {
				if(orco) {
					copy_v3_v3(texvec, orco);
				}
			}

			externtex(mtex, texvec, &tin, &tr, &tg, &tb, &ta);

			yn= tin*mtex->colfac;
			zn= tin*mtex->alphafac;

			if(mtex->mapto & MAP_COL) {
				zn= 1.0-yn;
				har->r= (yn*tr+ zn*ma->r);
				har->g= (yn*tg+ zn*ma->g);
				har->b= (yn*tb+ zn*ma->b);
			}
			if(mtex->texco & TEXCO_UV) {
				har->alfa= tin;
			}
			if(mtex->mapto & MAP_ALPHA)
				har->alfa= tin;
		}
	}

	return har;
}

/****************************************************************************/

HaloRen *halo_init_particle(Render *re, ObjectRen *obr, DerivedMesh *dm, Material *ma,   float *vec,   float *vec1, 
				  float *orco, float *uvco, float hasize, float vectsize, int seed)
{
	HaloRen *har;
	MTex *mtex;
	float tin, tr, tg, tb, ta;
	float xn, yn, zn, texvec[3], hoco[4], hoco1[4], in[3],tex[3],out[3];
	int i;

	if(hasize==0.0) return NULL;

	camera_halo_co_to_hoco(&re->cam, hoco, vec);;
	if(hoco[3]==0.0) return NULL;
	if(vec1) {
		camera_halo_co_to_hoco(&re->cam, hoco1, vec1);
		if(hoco1[3]==0.0) return NULL;
	}

	har= render_object_halo_get(obr, obr->tothalo++);
	copy_v3_v3(har->co, vec);
	har->hasize= hasize;

	/* actual projectvert is done in function halos_project() because of parts/border/pano */
	/* we do it here for sorting of halos */
	zn= hoco[3];
	har->xs= 0.5*re->cam.winx*(hoco[0]/zn);
	har->ys= 0.5*re->cam.winy*(hoco[1]/zn);
	har->zs= 0x7FFFFF*(hoco[2]/zn);
	
	har->zBufDist = 0x7FFFFFFF*(hoco[2]/zn); 
	
	/* halovect */
	if(vec1) {

		har->type |= HA_VECT;

		xn=  har->xs - 0.5*re->cam.winx*(hoco1[0]/hoco1[3]);
		yn=  har->ys - 0.5*re->cam.winy*(hoco1[1]/hoco1[3]);
		if(xn==0.0 || (xn==0.0 && yn==0.0)) zn= 0.0;
		else zn= atan2(yn, xn);

		har->sin= sin(zn);
		har->cos= cos(zn);
		zn= len_v3v3(vec1, vec)*0.5;

		har->hasize= vectsize*zn + (1.0-vectsize)*hasize;
		
		sub_v3_v3v3(har->no, vec, vec1);
		normalize_v3(har->no);
	}

	if(ma->mode & MA_HALO_XALPHA) har->type |= HA_XALPHA;

	har->alfa= ma->alpha;
	har->r= ma->r;
	har->g= ma->g;
	har->b= ma->b;
	har->add= (255.0*ma->add);
	har->mat= ma;
	har->hard= ma->har;
	har->seed= seed % 256;

	if(ma->mode & MA_STAR) har->starpoints= ma->starc;
	if(ma->mode & MA_HALO_LINES) har->linec= ma->linec;
	if(ma->mode & MA_HALO_RINGS) har->ringc= ma->ringc;
	if(ma->mode & MA_HALO_FLARE) har->flarec= ma->flarec;

	if((ma->mode & MA_HALOTEX) && ma->mtex[0]){
		har->tex= 1;
		i=1;
	}
	
	for(i=0; i<MAX_MTEX; i++)
		if(ma->mtex[i] && (ma->septex & (1<<i))==0) {
			mtex= ma->mtex[i];
			copy_v3_v3(texvec, vec);

			if(mtex->texco & TEXCO_NORM) {
				;
			}
			else if(mtex->texco & TEXCO_OBJECT) {
				if(mtex->object){
					float imat[4][4];
					/* imat should really be cached somewhere before this */
					invert_m4_m4(imat,mtex->object->obmat);
					mul_m4_v3(imat,texvec);
				}
				/* texvec[0]+= imatbase->ivec[0]; */
				/* texvec[1]+= imatbase->ivec[1]; */
				/* texvec[2]+= imatbase->ivec[2]; */
				/* mul_m3_v3(imatbase->imat, texvec); */
			}
			else if(mtex->texco & TEXCO_GLOB){
				copy_v3_v3(texvec,vec);
			}
			else if(mtex->texco & TEXCO_UV && uvco){
				int uv_index=CustomData_get_named_layer_index(&dm->faceData,CD_MTFACE,mtex->uvname);
				if(uv_index<0)
					uv_index=CustomData_get_active_layer_index(&dm->faceData,CD_MTFACE);

				uv_index-=CustomData_get_layer_index(&dm->faceData,CD_MTFACE);

				texvec[0]=2.0f*uvco[2*uv_index]-1.0f;
				texvec[1]=2.0f*uvco[2*uv_index+1]-1.0f;
				texvec[2]=0.0f;
			}
			else if(orco) {
				copy_v3_v3(texvec, orco);
			}

			externtex(mtex, texvec, &tin, &tr, &tg, &tb, &ta);

			//yn= tin*mtex->colfac;
			//zn= tin*mtex->alphafac;
			if(mtex->mapto & MAP_COL) {
				tex[0]=tr;
				tex[1]=tg;
				tex[2]=tb;
				out[0]=har->r;
				out[1]=har->g;
				out[2]=har->b;

				texture_rgb_blend(in,tex,out,tin,mtex->colfac,mtex->blendtype);
			//	zn= 1.0-yn;
				//har->r= (yn*tr+ zn*ma->r);
				//har->g= (yn*tg+ zn*ma->g);
				//har->b= (yn*tb+ zn*ma->b);
				har->r= in[0];
				har->g= in[1];
				har->b= in[2];
			}
			if(mtex->mapto & MAP_ALPHA)
				har->alfa = texture_value_blend(mtex->def_var,har->alfa,tin,mtex->alphafac,mtex->blendtype);
			if(mtex->mapto & MAP_HAR)
				har->hard = 1.0+126.0*texture_value_blend(mtex->def_var,((float)har->hard)/127.0,tin,mtex->hardfac,mtex->blendtype);
			if(mtex->mapto & MAP_RAYMIRR)
				har->hasize = 100.0*texture_value_blend(mtex->def_var,har->hasize/100.0,tin,mtex->raymirrfac,mtex->blendtype);
			/* now what on earth is this good for?? */
			//if(mtex->texco & 16) {
			//	har->alfa= tin;
			//}
		}

	return har;
}

/***************************** Halo Sorting **********************************/
/* only the first n are sorted in case we have stars, those are at the end   */

static int verghalo(const void *a1, const void *a2)
{
	const HaloRen *har1= *(const HaloRen**)a1;
	const HaloRen *har2= *(const HaloRen**)a2;
	
	if(har1->zs < har2->zs) return 1;
	else if(har1->zs > har2->zs) return -1;
	return 0;
}

void halos_sort(RenderDB *rdb, int totsort)
{
	ObjectRen *obr;
	HaloRen *har= NULL, **haso;
	int a;

	if(rdb->tothalo==0) return;

	rdb->sortedhalos= MEM_callocN(sizeof(HaloRen*)*rdb->tothalo, "sorthalos");
	haso= rdb->sortedhalos;

	for(obr=rdb->objecttable.first; obr; obr=obr->next) {
		for(a=0; a<obr->tothalo; a++) {
			if((a & 255)==0) har= obr->bloha[a>>8];
			else har++;

			*(haso++)= har;
		}
	}

	qsort(rdb->sortedhalos, totsort, sizeof(HaloRen*), verghalo);
}

/**************************** Halo Projection ********************************/
/* project & clip halos using camera. regular objects do this on the fly.    */

/* ugly function for halos in panorama */
static int panotestclip(int do_pano, int xparts, float *v)
{
	/* to be used for halos en infos */
	float abs4;
	short c=0;

	if(do_pano==0) return camera_hoco_test_clip(v);

	abs4= fabs(v[3]);

	if(v[2]< -abs4) c=16;		/* this used to be " if(v[2]<0) ", see clippz() */
	else if(v[2]> abs4) c+= 32;

	if( v[1]>abs4) c+=4;
	else if( v[1]< -abs4) c+=8;

	abs4*= xparts;
	if( v[0]>abs4) c+=2;
	else if( v[0]< -abs4) c+=1;

	return c;
}

void halos_project(RenderDB *rdb, RenderCamera *cam, float xoffs, int xparts)
{
	ObjectRen *obr;
	HaloRen *har = NULL;
	float zn, vec[3], hoco[4];
	int a;

	if(cam->type == R_CAM_PANO) {
		float panophi= xoffs;
		
		cam->panosi= sin(panophi);
		cam->panoco= cos(panophi);
	}

	for(obr=rdb->objecttable.first; obr; obr=obr->next) {
		/* calculate view coordinates (and zbuffer value) */
		for(a=0; a<obr->tothalo; a++) {
			if((a & 255)==0) har= obr->bloha[a>>8];
			else har++;

			if(cam->type == R_CAM_PANO) {
				vec[0]= cam->panoco*har->co[0] - cam->panosi*har->co[2];
				vec[1]= har->co[1];
				vec[2]= cam->panosi*har->co[0] + cam->panoco*har->co[2];
			}
			else {
				copy_v3_v3(vec, har->co);
			}

			camera_halo_co_to_hoco(cam, hoco, vec);
			
			/* we clip halos less critical, but not for the Z */
			hoco[0]*= 0.5;
			hoco[1]*= 0.5;
			
			if( panotestclip(cam->type == R_CAM_PANO, xparts, hoco) ) {
				har->miny= har->maxy= -10000;	/* that way render clips it */
			}
			else if(hoco[3]<0.0) {
				har->miny= har->maxy= -10000;	/* render clips it */
			}
			else /* do the projection...*/
			{
				/* bring back hocos */
				hoco[0]*= 2.0;
				hoco[1]*= 2.0;
				
				zn= hoco[3];
				har->xs= 0.5*cam->winx*(1.0+hoco[0]/zn); /* the 0.5 negates the previous 2...*/
				har->ys= 0.5*cam->winy*(1.0+hoco[1]/zn);
			
				/* this should be the zbuffer coordinate */
				har->zs= 0x7FFFFF*(hoco[2]/zn);
				/* taking this from the face clip functions? seems ok... */
				har->zBufDist = 0x7FFFFFFF*(hoco[2]/zn);
				
				vec[0]+= har->hasize;
				camera_halo_co_to_hoco(cam, hoco, vec);;
				vec[0]-= har->hasize;
				zn= hoco[3];
				har->rad= fabs(har->xs- 0.5*cam->winx*(1.0+hoco[0]/zn));
			
				/* this clip is not really OK, to prevent stars to become too large */
				if(har->type & HA_ONLYSKY) {
					if(har->rad>3.0) har->rad= 3.0;
				}
			
				har->radsq= har->rad*har->rad;
			
				har->miny= har->ys - har->rad/cam->ycor;
				har->maxy= har->ys + har->rad/cam->ycor;
			
				/* the Zd value is still not really correct for pano */
			
				vec[2]-= har->hasize;	/* z negative, otherwise it's clipped */
				camera_halo_co_to_hoco(cam, hoco, vec);;
				zn= hoco[3];
				zn= fabs( (float)har->zs - 0x7FFFFF*(hoco[2]/zn));
				har->zd= CLAMPIS(zn, 0, INT_MAX);
			
			}
		}
	}
}



/******************************** Stars **************************************/
/* note that this code is used by the 3d view for drawing as well ..         */

static HaloRen *initstar(Render *re, ObjectRen *obr, float *vec, float hasize)
{
	HaloRen *har;
	float hoco[4];
	
	camera_halo_co_to_hoco(&re->cam, hoco, vec);
	
	har= render_object_halo_get(obr, obr->tothalo++);
	
	/* projectvert is done in function zbufvlaggen again, because of parts */
	copy_v3_v3(har->co, vec);
	har->hasize= hasize;
	
	har->zd= 0.0;
	
	return har;
}

/* there must be a 'fixed' amount of stars generated between near and far,
 * all stars must by preference lie on the far and solely differ in clarity
 * and color. */

void RE_make_stars(Render *re, Scene *scenev3d, void (*initfunc)(void),
				   void (*vertexfunc)(float*),  void (*termfunc)(void))
{
	extern unsigned char hash[512];
	ObjectRen *obr= NULL;
	World *wrld= NULL;
	HaloRen *har;
	Scene *scene;
	Camera *camera;
	double dblrand, hlfrand;
	float vec[4], fx, fy, fz;
	float fac, starmindist, clipend;
	float mat[4][4], stargrid, maxrand, maxjit, force, alpha;
	int x, y, z, sx, sy, sz, ex, ey, ez, done = 0;
	
	if(initfunc) {
		scene= scenev3d;
		wrld= scene->world;
	}
	else {
		scene= re->db.scene;
		wrld= &(re->db.wrld);
	}
	
	stargrid = wrld->stardist;			/* distance between stars */
	maxrand = 2.0;						/* amount a star can be shifted (in grid units) */
	maxjit = (wrld->starcolnoise);		/* amount a color is being shifted */
	
	/* size of stars */
	force = ( wrld->starsize );
	
	/* minimal free space (starting at camera) */
	starmindist= wrld->starmindist;
	
	if (stargrid <= 0.10) return;
	
	if (re) re->params.flag |= R_HALO;
	else stargrid *= 1.0;				/* then it draws fewer */
	
	if(re) invert_m4_m4(mat, re->cam.viewmat);
	else unit_m4(mat);
	
	/* BOUNDING BOX CALCULATION
		* bbox goes from z = loc_near_var | loc_far_var,
		* x = -z | +z,
		* y = -z | +z
		*/
	
	if(scene->camera==NULL)
		return;
	camera = scene->camera->data;
	clipend = camera->clipend;
	
	/* convert to grid coordinates */
	
	sx = ((mat[3][0] - clipend) / stargrid) - maxrand;
	sy = ((mat[3][1] - clipend) / stargrid) - maxrand;
	sz = ((mat[3][2] - clipend) / stargrid) - maxrand;
	
	ex = ((mat[3][0] + clipend) / stargrid) + maxrand;
	ey = ((mat[3][1] + clipend) / stargrid) + maxrand;
	ez = ((mat[3][2] + clipend) / stargrid) + maxrand;
	
	dblrand = maxrand * stargrid;
	hlfrand = 2.0 * dblrand;
	
	if (initfunc) {
		initfunc();	
	}

	if(re) /* add render object for stars */
		obr= render_object_create(&re->db, NULL, NULL, 0, 0, 0);
	
	for (x = sx, fx = sx * stargrid; x <= ex; x++, fx += stargrid) {
		for (y = sy, fy = sy * stargrid; y <= ey ; y++, fy += stargrid) {
			for (z = sz, fz = sz * stargrid; z <= ez; z++, fz += stargrid) {

				BLI_srand((hash[z & 0xff] << 24) + (hash[y & 0xff] << 16) + (hash[x & 0xff] << 8));
				vec[0] = fx + (hlfrand * BLI_drand()) - dblrand;
				vec[1] = fy + (hlfrand * BLI_drand()) - dblrand;
				vec[2] = fz + (hlfrand * BLI_drand()) - dblrand;
				vec[3] = 1.0;
				
				if (vertexfunc) {
					if(done & 1) vertexfunc(vec);
					done++;
				}
				else {
					mul_m4_v3(re->cam.viewmat, vec);
					
					/* in vec are global coordinates
					* calculate distance to camera
					* and using that, define the alpha
					*/
					
					{
						float tx, ty, tz;
						
						tx = vec[0];
						ty = vec[1];
						tz = vec[2];
						
						alpha = sqrt(tx * tx + ty * ty + tz * tz);
						
						if (alpha >= clipend) alpha = 0.0;
						else if (alpha <= starmindist) alpha = 0.0;
						else if (alpha <= 2.0 * starmindist) {
							alpha = (alpha - starmindist) / starmindist;
						} else {
							alpha -= 2.0 * starmindist;
							alpha /= (clipend - 2.0 * starmindist);
							alpha = 1.0 - alpha;
						}
					}
					
					
					if (alpha != 0.0) {
						fac = force * BLI_drand();
						
						har = initstar(re, obr, vec, fac);
						
						if (har) {
							har->alfa = sqrt(sqrt(alpha));
							har->add= 255;
							har->r = har->g = har->b = 1.0;
							if (maxjit) {
								har->r += ((maxjit * BLI_drand()) ) - maxjit;
								har->g += ((maxjit * BLI_drand()) ) - maxjit;
								har->b += ((maxjit * BLI_drand()) ) - maxjit;
							}
							har->hard = 32;
							har->lay= -1;
							har->type |= HA_ONLYSKY;
							done++;
						}
					}
				}
			}
			/* do not call blender_test_break() here, since it is used in UI as well, confusing the callback system */
			/* main cause is G.afbreek of course, a global again... (ton) */
		}
	}

	if(termfunc) termfunc();

	if(obr)
		re->db.tothalo += obr->tothalo;
}

/********************************* Flare **********************************/

static void renderhalo_post(Render *re, RenderResult *rr, float *rectf, HaloRen *har)	/* postprocess version */
{
	float dist, xsq, ysq, xn, yn, colf[4], *rectft, *rtf;
	float haloxs, haloys;
	int minx, maxx, miny, maxy, x, y;

	/* calculate the disprect mapped coordinate for halo. note: rectx is disprect corrected */
	haloxs= har->xs - re->disprect.xmin;
	haloys= har->ys - re->disprect.ymin;
	
	har->miny= miny= haloys - har->rad/re->cam.ycor;
	har->maxy= maxy= haloys + har->rad/re->cam.ycor;
	
	if(maxy<0);
	else if(rr->recty<miny);
	else {
		minx= floor(haloxs-har->rad);
		maxx= ceil(haloxs+har->rad);
			
		if(maxx<0);
		else if(rr->rectx<minx);
		else {
		
			if(minx<0) minx= 0;
			if(maxx>=rr->rectx) maxx= rr->rectx-1;
			if(miny<0) miny= 0;
			if(maxy>rr->recty) maxy= rr->recty;
	
			rectft= rectf+ 4*rr->rectx*miny;

			for(y=miny; y<maxy; y++) {
	
				rtf= rectft+4*minx;
				
				yn= (y - haloys)*re->cam.ycor;
				ysq= yn*yn;
				
				for(x=minx; x<=maxx; x++) {
					xn= x - haloxs;
					xsq= xn*xn;
					dist= xsq+ysq;
					if(dist<har->radsq) {
						
						if(shadeHaloFloat(re, har, colf, 0x7FFFFF, dist, xn, yn, har->flarec))
							pxf_add_alpha_fac(rtf, colf, har->add);
					}
					rtf+=4;
				}
	
				rectft+= 4*rr->rectx;
				
				if(re->cb.test_break(re->cb.tbh)) break; 
			}
		}
	}
} 

static void renderflare(Render *re, RenderResult *rr, float *rectf, HaloRen *har)
{
	extern float hashvectf[];
	HaloRen fla;
	Material *ma;
	float *rc, rad, alfa, visifac, vec[3];
	int b, type;
	
	fla= *har;
	fla.linec= fla.ringc= fla.flarec= 0;
	
	rad= har->rad;
	alfa= har->alfa;
	
	visifac= re->cam.ycor*(har->pixels);
	/* all radials added / r^3  == 1.0f! */
	visifac /= (har->rad*har->rad*har->rad);
	visifac*= visifac;

	ma= har->mat;
	
	/* first halo: just do */
	
	har->rad= rad*ma->flaresize*visifac;
	har->radsq= har->rad*har->rad;
	har->zs= fla.zs= 0;
	
	har->alfa= alfa*visifac;

	renderhalo_post(re, rr, rectf, har);
	
	/* next halo's: the flares */
	rc= hashvectf + ma->seed2;
	
	for(b=1; b<har->flarec; b++) {
		
		fla.r= fabs(rc[0]);
		fla.g= fabs(rc[1]);
		fla.b= fabs(rc[2]);
		fla.alfa= ma->flareboost*fabs(alfa*visifac*rc[3]);
		fla.hard= 20.0f + fabs(70*rc[7]);
		fla.tex= 0;
		
		type= (int)(fabs(3.9*rc[6]));

		fla.rad= ma->subsize*sqrt(fabs(2.0f*har->rad*rc[4]));
		
		if(type==3) {
			fla.rad*= 3.0f;
			fla.rad+= re->rectx/10;
		}
		
		fla.radsq= fla.rad*fla.rad;
		
		vec[0]= 1.4*rc[5]*(har->xs-re->cam.winx/2);
		vec[1]= 1.4*rc[5]*(har->ys-re->cam.winy/2);
		vec[2]= 32.0f*sqrt(vec[0]*vec[0] + vec[1]*vec[1] + 1.0f);
		
		fla.xs= re->cam.winx/2 + vec[0] + (1.2+rc[8])*re->rectx*vec[0]/vec[2];
		fla.ys= re->cam.winy/2 + vec[1] + (1.2+rc[8])*re->rectx*vec[1]/vec[2];

		if(re->params.flag & R_SEC_FIELD) {
			if(re->params.r.mode & R_ODDFIELD) fla.ys += 0.5;
			else fla.ys -= 0.5;
		}
		if(type & 1) fla.type= HA_FLARECIRC;
		else fla.type= 0;
		renderhalo_post(re, rr, rectf, &fla);

		fla.alfa*= 0.5;
		if(type & 2) fla.type= HA_FLARECIRC;
		else fla.type= 0;
		renderhalo_post(re, rr, rectf, &fla);
		
		rc+= 7;
	}
}

/* needs recode... integrate this better! */
void halos_render_flare(Render *re)
{
	RenderResult *rr= re->result;
	RenderLayer *rl;
	HaloRen *har;
	RenderCamera cam;
	int a, do_draw=0;
	
	/* for now, we get the first renderlayer in list with halos set */
	for(rl= rr->layers.first; rl; rl= rl->next)
		if(rl->layflag & SCE_LAY_HALO)
			break;

	if(rl==NULL || rl->rectf==NULL)
		return;
	
	cam= re->cam;
	if(re->cam.type == R_CAM_PANO)
		re->cam.type= R_CAM_PERSP;
	
	halos_project(&re->db, &re->cam, 0, re->xparts);
	
	for(a=0; a<re->db.tothalo; a++) {
		har= re->db.sortedhalos[a];
		
		if(har->flarec) {
			do_draw= 1;
			renderflare(re, rr, rl->rectf, har);
		}
	}

	if(do_draw) {
		/* weak... the display callback wants an active renderlayer pointer... */
		rr->renlay= rl;
		re->cb.display_draw(re->cb.ddh, rr, NULL);
	}
	
	re->cam= cam;	
}


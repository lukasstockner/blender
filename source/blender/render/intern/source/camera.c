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

#include <stdlib.h>
#include <stdio.h>

#include "DNA_camera_types.h"
#include "DNA_lamp_types.h"

#include "BLI_math.h"

#include "camera.h"
#include "database.h"
#include "object_halo.h"
#include "render_types.h"
#include "rendercore.h"
#include "zbuf.h"

/* Blender Camera
 *
 * Coordinates:
 * - co: 3d coordinates in view space.
 * - hoco: homogenous coordinates in view space.
 * - raster: pixel coordinates (+ z-buffer).
 * - zco: pixel coordinates (+ zbuffer) in a part.
 *
 * All vertex, lamp, .. coordinates are transformed to camera space
 * when the database is created. This means the camera location is
 * at (0, 0, 0), and the (0, 0, 1) z-axis points right into the scene.
 */

/************************ Coordinate Conversion ******************************/

/* x and y are current pixels in rect to be rendered */
/* do not normalize! */
void camera_raster_to_view(RenderCamera *cam, float *view, float x, float y)
{
	switch(cam->type) {
		case R_CAM_ORTHO: {
			view[0]= 0.0f;
			view[1]= 0.0f;
			view[2]= -ABS(cam->clipsta);
			break;
		}
		case R_CAM_PERSP: {
			/* move x and y to real viewplane coords */
			x= (x/(float)cam->winx);
			y= (y/(float)cam->winy);

			view[0]= cam->viewplane.xmin + x*(cam->viewplane.xmax - cam->viewplane.xmin);
			view[1]= cam->viewplane.ymin + y*(cam->viewplane.ymax - cam->viewplane.ymin);
			view[2]= -ABS(cam->clipsta);
			break;
		}
		case R_CAM_PANO: {
			float u, v;

			x-= cam->panodxp;
			
			/* move x and y to real viewplane coords */
			x= (x/(float)cam->winx);
			y= (y/(float)cam->winy);

			view[0]= cam->viewplane.xmin + x*(cam->viewplane.xmax - cam->viewplane.xmin);
			view[1]= cam->viewplane.ymin + y*(cam->viewplane.ymax - cam->viewplane.ymin);
			view[2]= -ABS(cam->clipsta);
			
			u= view[0] + cam->panodxv;
			v= view[2];

			view[0]= cam->panoco*u + cam->panosi*v;
			view[2]= -cam->panosi*u + cam->panoco*v;
			break;
		}
	}
}

void camera_raster_to_ray(RenderCamera *cam, float start[3], float vec[3], float x, float y)
{
	camera_raster_to_view(cam, vec, x, y);
	normalize_v3(vec);

	switch(cam->type) {
		case R_CAM_ORTHO: {
			/* copy from shade_input_calc_viewco */
			float fx= 2.0f/(cam->winx*cam->winmat[0][0]);
			float fy= 2.0f/(cam->winy*cam->winmat[1][1]);
			
			start[0]= (x - 0.5f*cam->winx)*fx - cam->winmat[3][0]/cam->winmat[0][0];
			start[1]= (y - 0.5f*cam->winy)*fy - cam->winmat[3][1]/cam->winmat[1][1];
			start[2]= 0.0f;
			break;
		}
		case R_CAM_PERSP:
		case R_CAM_PANO: {
			start[0]= start[1]= start[2]= 0.0f;
			break;
		}
	}
}

void camera_raster_to_co(RenderCamera *cam, float co[3], float x, float y, int z)
{
	switch(cam->type) {
		case R_CAM_ORTHO: {
			/* x and y 3d coordinate can be derived from pixel coord and winmat */
			float fx= 2.0f/(cam->winx*cam->winmat[0][0]);
			float fy= 2.0f/(cam->winy*cam->winmat[1][1]);
			float zco;
			
			co[0]= (x - 0.5f*cam->winx)*fx - cam->winmat[3][0]/cam->winmat[0][0];
			co[1]= (y - 0.5f*cam->winy)*fy - cam->winmat[3][1]/cam->winmat[1][1];
			
			zco= ((float)z)/2147483647.0f;
			co[2]= cam->winmat[3][2]/(cam->winmat[2][3]*zco - cam->winmat[2][2]);
			break;
		}
		case R_CAM_PERSP:
		case R_CAM_PANO: {
			float view[3], fac, zco;

			camera_raster_to_view(cam, view, x, y);
			
			/* inverse of zbuf calc: zbuf = MAXZ*hoco_z/hoco_w */
			zco= ((float)z)/2147483647.0f;
			co[2]= cam->winmat[3][2]/(cam->winmat[2][3]*zco - cam->winmat[2][2]);

			fac= co[2]/view[2];
			co[0]= fac*view[0];
			co[1]= fac*view[1];
			break;
		}
	}
}

void camera_raster_plane_to_co(RenderCamera *cam, float co[3], float dxco[3], float dyco[3], float view[3], float dxyview[2], float x, float y, float plane[4])
{
	float fac;

	/* returns not normalized, so is in viewplane coords */
	camera_raster_to_view(cam, view, x, y);
	
	/* ortho viewplane cannot intersect using view vector originating in (0,0,0) */
	switch(cam->type) {
		case R_CAM_ORTHO: {
			/* x and y 3d coordinate can be derived from pixel coord and winmat */
			float fx= 2.0f/(cam->winx*cam->winmat[0][0]);
			float fy= 2.0f/(cam->winy*cam->winmat[1][1]);
			
			co[0]= (x - 0.5f*cam->winx)*fx - cam->winmat[3][0]/cam->winmat[0][0];
			co[1]= (y - 0.5f*cam->winy)*fy - cam->winmat[3][1]/cam->winmat[1][1];
			
			/* using a*x + b*y + c*z = d equation, (a b c) is normal */
			if(plane[2]!=0.0f)
				co[2]= (plane[3] - plane[0]*co[0] - plane[1]*co[1])/plane[2];
			else
				co[2]= 0.0f;
			
			if(dxco && dyco) {
				dxco[0]= fx;
				dxco[1]= 0.0f;
				if(plane[2]!=0.0f)
					dxco[2]= (plane[0]*fx)/plane[2];
				else 
					dxco[2]= 0.0f;
				
				dyco[0]= 0.0f;
				dyco[1]= fy;
				if(plane[2]!=0.0f)
					dyco[2]= (plane[1]*fy)/plane[2];
				else 
					dyco[2]= 0.0f;
				
				if(dxyview) {
					if(co[2]!=0.0f) fac= 1.0f/co[2]; else fac= 0.0f;
					dxyview[0]= -cam->viewdx*fac;
					dxyview[1]= -cam->viewdy*fac;
				}
			}
			break;
		}
		case R_CAM_PERSP:
		case R_CAM_PANO: {
			float div;
			
			div= plane[0]*view[0] + plane[1]*view[1] + plane[2]*view[2];
			if (div!=0.0f) fac= plane[3]/div;
			else fac= 0.0f;
			
			co[0]= fac*view[0];
			co[1]= fac*view[1];
			co[2]= fac*view[2];
			
			/* pixel dx/dy for render coord */
			if(dxco && dyco) {
				float u= plane[3]/(div - cam->viewdx*plane[0]);
				float v= plane[3]/(div - cam->viewdy*plane[1]);
				
				dxco[0]= co[0]- (view[0]-cam->viewdx)*u;
				dxco[1]= co[1]- (view[1])*u;
				dxco[2]= co[2]- (view[2])*u;
				
				dyco[0]= co[0]- (view[0])*v;
				dyco[1]= co[1]- (view[1]-cam->viewdy)*v;
				dyco[2]= co[2]- (view[2])*v;
				
				if(dxyview) {
					if(fac!=0.0f) fac= 1.0f/fac;
					dxyview[0]= -cam->viewdx*fac;
					dxyview[1]= -cam->viewdy*fac;
				}
			}
			break;
		}
	}

	/* cannot normalize earlier, code above needs it at viewplane level */
	normalize_v3(view);
}

void camera_halo_co_to_hoco(RenderCamera *cam, float hoco[4], float co[3])
{
	/* calculate homogenous coordinates. */
	float (*winmat)[4]= cam->winmat;
	float x, y, z;

	x= co[0]; 
	y= co[1]; 
	z= co[2];

	hoco[0]= x*winmat[0][0]						+ z*winmat[2][0] + winmat[3][0];
	hoco[1]= 					y*winmat[1][1]	+ z*winmat[2][1] + winmat[3][1];
	hoco[2]=									  z*winmat[2][2] + winmat[3][2];
	hoco[3]=									  z*winmat[2][3] + winmat[3][3];
}

void camera_matrix_co_to_hoco(float winmat[][4], float hoco[4], float co[3])
{
	/* calculate homogenous coordinates */
	float x, y, z;

	x= co[0]; 
	y= co[1]; 
	z= co[2];

	hoco[0]= x*winmat[0][0] + y*winmat[1][0] + z*winmat[2][0] + winmat[3][0];
	hoco[1]= x*winmat[0][1] + y*winmat[1][1] + z*winmat[2][1] + winmat[3][1];
	hoco[2]= x*winmat[0][2] + y*winmat[1][2] + z*winmat[2][2] + winmat[3][2];
	hoco[3]= x*winmat[0][3] + y*winmat[1][3] + z*winmat[2][3] + winmat[3][3];
}

void camera_window_matrix(RenderCamera *cam, float winmat[][4])
{
	float panomat[4][4];

	if(cam->type == R_CAM_PANO) {
		unit_m4(panomat);
		panomat[0][0]= cam->panoco;
		panomat[0][2]= cam->panosi;
		panomat[2][0]= -cam->panosi;
		panomat[2][2]= cam->panoco;

		mul_m4_m4m4(winmat, panomat, cam->winmat);
	}
	else
		copy_m4_m4(winmat, cam->winmat);
}

void camera_window_rect_bounds(int winx, int winy, rcti *rect, float bounds[4])
{
	bounds[0]= (2*rect->xmin - winx-1)/(float)winx;
	bounds[1]= (2*rect->xmax - winx+1)/(float)winx;
	bounds[2]= (2*rect->ymin - winy-1)/(float)winy;
	bounds[3]= (2*rect->ymax - winy+1)/(float)winy;
}

int camera_hoco_test_clip(float hoco[4])
{
	/* WATCH IT: this function should do the same as cliptestf,
	   otherwise troubles in zbufclip()*/
	float abs4;
	short c=0;
	
	/* if we set clip flags, the clipping should be at least larger than
	   epsilon.  prevents issues with vertices lying exact on borders */
	abs4= fabsf(hoco[3]) + FLT_EPSILON;
	
	if(hoco[2] < -abs4) c=16;			/* this used to be " if(hoco[2]<0) ", see clippz() */
	else if(hoco[2] > abs4) c+= 32;
	
	if(hoco[0] > abs4) c+=2;
	else if(hoco[0] < -abs4) c+=1;
	
	if(hoco[1] > abs4) c+=4;
	else if(hoco[1] < -abs4) c+=8;
	
	return c;
}

void camera_hoco_to_zco(RenderCamera *cam, float zco[3], float hoco[4])
{
	float div= 1.0f/hoco[3];

	zco[0]= cam->winx*0.5f*(1.0 + hoco[0]*div);
	zco[1]= cam->winy*0.5f*(1.0 + hoco[1]*div);
	zco[2]= (hoco[2]*div);

}

/********************************* Panorama **********************************/

/* calculus for how much 1 pixel rendered should rotate the 3d geometry */
/* is not that simple, needs to be corrected for errors of larger viewplane sizes */
/* called in initrender.c, parts_create() and convertblender.c, for speedvectors */
float panorama_pixel_rot(Render *re)
{
	RenderCamera *cam= &re->cam;
	float psize, phi, xfac;
	
	/* size of 1 pixel mapped to viewplane coords */
	psize= (cam->viewplane.xmax-cam->viewplane.xmin)/(float)cam->winx;
	/* angle of a pixel */
	phi= atan(psize/cam->clipsta);
	
	/* correction factor for viewplane shifting, first calculate how much the viewplane angle is */
	xfac= ((cam->viewplane.xmax-cam->viewplane.xmin))/(float)re->xparts;
	xfac= atan(0.5f*xfac/cam->clipsta); 
	/* and how much the same viewplane angle is wrapped */
	psize= 0.5f*phi*((float)re->partx);
	
	/* the ratio applied to final per-pixel angle */
	phi*= xfac/psize;
	
	return phi;
}

/* call when all parts stopped rendering, to find the next Y slice */
/* if slice found, it rotates the dbase */
void panorama_set_camera_params(Render *re, rcti *disprect, rctf *viewplane)
{
	RenderCamera *cam= &re->cam;
	float phi= panorama_pixel_rot(re);

	cam->panodxp= (cam->winx - (disprect->xmin + disprect->xmax) )/2;
	cam->panodxv= ((viewplane->xmax-viewplane->xmin)*cam->panodxp)/(float)cam->winx;
	
	/* shift viewplane */
	cam->viewplane.xmin = viewplane->xmin + cam->panodxv;
	cam->viewplane.xmax = viewplane->xmax + cam->panodxv;

	RE_SetWindow(re, &cam->viewplane, cam->clipsta, cam->clipend, 1);
	
	/* TODO halos are projected according to panorama slice, should remove
	   this and do projection on the fly for halos just like it happens for
	   other primitives */
	cam->panosi= sin(cam->panodxp*phi);
	cam->panoco= cos(cam->panodxp*phi);
	halos_project(&re->db, &re->cam, cam->panodxp*phi, re->xparts);
}

/******************************* External API ********************************
 * TODO this API and it's usage could be cleaned up, it's not clear now who  *
 * is setting the camera when.                                               */

/* call this after InitState() */
/* per render, there's one persistant viewplane. Parts will set their own viewplanes */
void RE_SetCamera(Render *re, Object *camera)
{
	RenderCamera *cam= &re->cam;
	Camera *bcam=NULL;
	rctf viewplane;
	float pixsize, clipsta, clipend;
	float lens, shiftx=0.0, shifty=0.0, winside;
	
	/* question mark */
	cam->ycor= ( (float)re->params.r.yasp)/( (float)re->params.r.xasp);
	if(re->params.r.mode & R_FIELDS)
		cam->ycor *= 2.0f;
	
	cam->type= R_CAM_PERSP;
	
	if(camera->type==OB_CAMERA) {
		bcam= camera->data;
		
		if(bcam->type == CAM_ORTHO) cam->type= R_CAM_ORTHO;
		else if(bcam->flag & CAM_PANORAMA) cam->type= R_CAM_PANO;
		
		/* solve this too... all time depending stuff is in convertblender.c?
		 * Need to update the camera early because it's used for projection matrices
		 * and other stuff BEFORE the animation update loop is done 
		 * */
#if 0 // XXX old animation system
		if(bcam->ipo) {
			calc_ipo(bcam->ipo, frame_to_float(re->scene, re->params.r.cfra));
			execute_ipo(&bcam->id, bcam->ipo);
		}
#endif // XXX old animation system
		lens= bcam->lens;
		shiftx=bcam->shiftx;
		shifty=bcam->shifty;

		clipsta= bcam->clipsta;
		clipend= bcam->clipend;
	}
	else if(camera->type==OB_LAMP) {
		Lamp *la= camera->data;
		float fac= cos( M_PI*la->spotsize/360.0 );
		float phi= acos(fac);
		
		lens= 16.0*fac/sin(phi);
		if(lens==0.0f)
			lens= 35.0;
		clipsta= la->clipsta;
		clipend= la->clipend;
	}
	else {	/* envmap exception... */
		lens= cam->lens;
		if(lens==0.0f)
			lens= 16.0;
		
		clipsta= cam->clipsta;
		clipend= cam->clipend;
		if(clipsta==0.0f || clipend==0.0f) {
			clipsta= 0.1f;
			clipend= 1000.0f;
		}
	}

	/* ortho only with camera available */
	if(cam && (cam->type == R_CAM_ORTHO)) {
		if( (re->params.r.xasp*cam->winx) >= (re->params.r.yasp*cam->winy) ) {
			cam->viewfac= cam->winx;
		}
		else {
			cam->viewfac= cam->ycor*cam->winy;
		}
		/* ortho_scale == 1.0 means exact 1 to 1 mapping */
		pixsize= bcam->ortho_scale/cam->viewfac;
	}
	else {
		if( (re->params.r.xasp*cam->winx) >= (re->params.r.yasp*cam->winy) ) {
			cam->viewfac= (cam->winx*lens)/32.0;
		}
		else {
			cam->viewfac= cam->ycor*(cam->winy*lens)/32.0;
		}
		
		pixsize= clipsta/cam->viewfac;
	}
	
	/* viewplane fully centered, zbuffer fills in jittered between -.5 and +.5 */
	winside= MAX2(cam->winx, cam->winy);
	viewplane.xmin= -0.5f*(float)cam->winx + shiftx*winside; 
	viewplane.ymin= -0.5f*cam->ycor*(float)cam->winy + shifty*winside;
	viewplane.xmax=  0.5f*(float)cam->winx + shiftx*winside; 
	viewplane.ymax=  0.5f*cam->ycor*(float)cam->winy + shifty*winside; 

	if(re->params.flag & R_SEC_FIELD) {
		if(re->params.r.mode & R_ODDFIELD) {
			viewplane.ymin-= .5*cam->ycor;
			viewplane.ymax-= .5*cam->ycor;
		}
		else {
			viewplane.ymin+= .5*cam->ycor;
			viewplane.ymax+= .5*cam->ycor;
		}
	}
	/* the window matrix is used for clipping, and not changed during OSA steps */
	/* using an offset of +0.5 here would give clip errors on edges */
	viewplane.xmin= pixsize*(viewplane.xmin);
	viewplane.xmax= pixsize*(viewplane.xmax);
	viewplane.ymin= pixsize*(viewplane.ymin);
	viewplane.ymax= pixsize*(viewplane.ymax);
	
	cam->viewdx= pixsize;
	cam->viewdy= cam->ycor*pixsize;

	if(cam->type == R_CAM_ORTHO)
		RE_SetOrtho(re, &viewplane, clipsta, clipend);
	else 
		RE_SetWindow(re, &viewplane, clipsta, clipend, cam->type == R_CAM_PANO);

}

void RE_SetPixelSize(Render *re, float pixsize)
{
	RenderCamera *cam= &re->cam;

	cam->viewdx= pixsize;
	cam->viewdy= cam->ycor*pixsize;
}

void RE_GetCameraWindow(struct Render *re, struct Object *camera, int frame, float mat[][4])
{
	re->params.r.cfra= frame;
	RE_SetCamera(re, camera);
	copy_m4_m4(mat, re->cam.winmat);
}

void RE_SetWindow(Render *re, rctf *viewplane, float clipsta, float clipend, int pano)
{
	RenderCamera *cam= &re->cam;
	
	cam->viewplane= *viewplane;
	cam->clipsta= clipsta;
	cam->clipend= clipend;
	cam->type= (pano)? R_CAM_PANO: R_CAM_PERSP;

	perspective_m4( cam->winmat,cam->viewplane.xmin, cam->viewplane.xmax, cam->viewplane.ymin, cam->viewplane.ymax, cam->clipsta, cam->clipend);
	
}

void RE_SetOrtho(Render *re, rctf *viewplane, float clipsta, float clipend)
{
	RenderCamera *cam= &re->cam;
	
	cam->viewplane= *viewplane;
	cam->clipsta= clipsta;
	cam->clipend= clipend;
	cam->type= R_CAM_ORTHO;

	orthographic_m4( cam->winmat,cam->viewplane.xmin, cam->viewplane.xmax, cam->viewplane.ymin, cam->viewplane.ymax, cam->clipsta, cam->clipend);
}

void RE_SetView(Render *re, float mat[][4])
{
	RenderCamera *cam= &re->cam;

	/* re->ok flag? */
	copy_m4_m4(cam->viewmat, mat);
	invert_m4_m4(cam->viewinv, cam->viewmat);

	copy_m3_m4(cam->viewnmat, cam->viewinv);
	transpose_m3(cam->viewnmat);
	copy_m3_m4(cam->viewninv, cam->viewmat);
	transpose_m3(cam->viewninv);
}


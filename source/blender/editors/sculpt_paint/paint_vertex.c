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
 * The Original Code is: all of this file.
 *
 * Contributor(s): Nicholas Bishop
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <math.h>
#include <string.h>

#ifdef WIN32
#include <io.h>
#else
#include <unistd.h>
#endif   

#include "MEM_guardedalloc.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_ghash.h"

#include "DNA_armature_types.h"
#include "DNA_mesh_types.h"
#include "DNA_particle_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "BKE_armature.h"
#include "BKE_action.h"
#include "BKE_brush.h"
#include "BKE_DerivedMesh.h"
#include "BKE_cloth.h"
#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_depsgraph.h"
#include "BKE_deform.h"
#include "BKE_displist.h"
#include "BKE_dmgrid.h"
#include "BKE_global.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_multires.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_utildefines.h"

#include "WM_api.h"
#include "WM_types.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "ED_armature.h"
#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_view3d.h"

#include "paint_intern.h"

/* polling - retrieve whether cursor should be set or operator should be done */


/* Returns true if vertex paint mode is active */
int vertex_paint_mode_poll(bContext *C)
{
	Object *ob = CTX_data_active_object(C);

	return ob && ob->mode == OB_MODE_VERTEX_PAINT;
}

int vertex_paint_poll(bContext *C)
{
	if(vertex_paint_mode_poll(C) && 
	   paint_brush(&CTX_data_tool_settings(C)->vpaint->paint)) {
		ScrArea *sa= CTX_wm_area(C);
		if(sa->spacetype==SPACE_VIEW3D) {
			ARegion *ar= CTX_wm_region(C);
			if(ar->regiontype==RGN_TYPE_WINDOW)
				return 1;
		}
	}
	return 0;
}

int weight_paint_mode_poll(bContext *C)
{
	Object *ob = CTX_data_active_object(C);

	return ob && ob->mode == OB_MODE_WEIGHT_PAINT;
}

int weight_paint_poll(bContext *C)
{
	Object *ob = CTX_data_active_object(C);

	if(ob && ob->mode & OB_MODE_WEIGHT_PAINT &&
	   paint_brush(&CTX_data_tool_settings(C)->wpaint->paint)) {
		ScrArea *sa= CTX_wm_area(C);
		if(sa->spacetype==SPACE_VIEW3D) {
			ARegion *ar= CTX_wm_region(C);
			if(ar->regiontype==RGN_TYPE_WINDOW)
				return 1;
		}
	}
	return 0;
}

static VPaint *new_vpaint(int wpaint)
{
	VPaint *vp= MEM_callocN(sizeof(VPaint), "VPaint");
	
	vp->flag= VP_AREA+VP_SPRAY;
	
	if(wpaint)
		vp->flag= VP_AREA;

	return vp;
}

static int *get_indexarray(Mesh *me)
{
	return MEM_mallocN(sizeof(int)*(me->totface+1), "vertexpaint");
}


/* in contradiction to cpack drawing colors, the MCOL colors (vpaint colors) are per byte! 
   so not endian sensitive. Mcol = ABGR!!! so be cautious with cpack calls */

unsigned int rgba_to_mcol(float r, float g, float b, float a)
{
	int ir, ig, ib, ia;
	unsigned int col;
	char *cp;
	
	ir= floor(255.0*r);
	if(ir<0) ir= 0; else if(ir>255) ir= 255;
	ig= floor(255.0*g);
	if(ig<0) ig= 0; else if(ig>255) ig= 255;
	ib= floor(255.0*b);
	if(ib<0) ib= 0; else if(ib>255) ib= 255;
	ia= floor(255.0*a);
	if(ia<0) ia= 0; else if(ia>255) ia= 255;
	
	cp= (char *)&col;
	cp[0]= ia;
	cp[1]= ib;
	cp[2]= ig;
	cp[3]= ir;
	
	return col;
	
}

unsigned int vpaint_get_current_col(VPaint *vp)
{
	Brush *brush = paint_brush(&vp->paint);
	return rgba_to_mcol(brush->rgb[0], brush->rgb[1], brush->rgb[2], 1.0f);
}

static void do_shared_vertexcol(Mesh *me)
{
	/* if no mcol: do not do */
	/* if tface: only the involved faces, otherwise all */
	MFace *mface;
	MTFace *tface;
	int a;
	short *scolmain, *scol;
	char *mcol;
	
	if(me->mcol==0 || me->totvert==0 || me->totface==0) return;
	
	scolmain= MEM_callocN(4*sizeof(short)*me->totvert, "colmain");
	
	tface= me->mtface;
	mface= me->mface;
	mcol= (char *)me->mcol;
	for(a=me->totface; a>0; a--, mface++, mcol+=16) {
		if((tface && tface->mode & TF_SHAREDCOL) || (me->editflag & ME_EDIT_PAINT_MASK)==0) {
			scol= scolmain+4*mface->v1;
			scol[0]++; scol[1]+= mcol[1]; scol[2]+= mcol[2]; scol[3]+= mcol[3];
			scol= scolmain+4*mface->v2;
			scol[0]++; scol[1]+= mcol[5]; scol[2]+= mcol[6]; scol[3]+= mcol[7];
			scol= scolmain+4*mface->v3;
			scol[0]++; scol[1]+= mcol[9]; scol[2]+= mcol[10]; scol[3]+= mcol[11];
			if(mface->v4) {
				scol= scolmain+4*mface->v4;
				scol[0]++; scol[1]+= mcol[13]; scol[2]+= mcol[14]; scol[3]+= mcol[15];
			}
		}
		if(tface) tface++;
	}
	
	a= me->totvert;
	scol= scolmain;
	while(a--) {
		if(scol[0]>1) {
			scol[1]/= scol[0];
			scol[2]/= scol[0];
			scol[3]/= scol[0];
		}
		scol+= 4;
	}
	
	tface= me->mtface;
	mface= me->mface;
	mcol= (char *)me->mcol;
	for(a=me->totface; a>0; a--, mface++, mcol+=16) {
		if((tface && tface->mode & TF_SHAREDCOL) || (me->editflag & ME_EDIT_PAINT_MASK)==0) {
			scol= scolmain+4*mface->v1;
			mcol[1]= scol[1]; mcol[2]= scol[2]; mcol[3]= scol[3];
			scol= scolmain+4*mface->v2;
			mcol[5]= scol[1]; mcol[6]= scol[2]; mcol[7]= scol[3];
			scol= scolmain+4*mface->v3;
			mcol[9]= scol[1]; mcol[10]= scol[2]; mcol[11]= scol[3];
			if(mface->v4) {
				scol= scolmain+4*mface->v4;
				mcol[13]= scol[1]; mcol[14]= scol[2]; mcol[15]= scol[3];
			}
		}
		if(tface) tface++;
	}

	MEM_freeN(scolmain);
}

static void make_vertexcol(Object *ob)	/* single ob */
{
	Mesh *me;
	if(!ob || ob->id.lib) return;
	me= get_mesh(ob);
	if(me==0) return;
	if(me->edit_mesh) return;

	/* copies from shadedisplist to mcol */
	if(!me->mcol) {
		CustomData_add_layer(&me->fdata, CD_MCOL, CD_CALLOC, NULL, me->totface);
		mesh_update_customdata_pointers(me);
	}

	//if(shade)
	//	shadeMeshMCol(scene, ob, me);
	//else

	memset(me->mcol, 255, 4*sizeof(MCol)*me->totface);
}

static void copy_vpaint_prev(VPaint *vp, unsigned int *mcol, int tot)
{
	if(vp->vpaint_prev) {
		MEM_freeN(vp->vpaint_prev);
		vp->vpaint_prev= NULL;
	}
	vp->tot= tot;	
	
	if(mcol==NULL || tot==0) return;
	
	vp->vpaint_prev= MEM_mallocN(4*sizeof(int)*tot, "vpaint_prev");
	memcpy(vp->vpaint_prev, mcol, 4*sizeof(int)*tot);
	
}

static void copy_wpaint_prev (VPaint *wp, MDeformVert *dverts, int dcount)
{
	if (wp->wpaint_prev) {
		free_dverts(wp->wpaint_prev, wp->tot);
		wp->wpaint_prev= NULL;
	}
	
	if(dverts && dcount) {
		
		wp->wpaint_prev = MEM_mallocN (sizeof(MDeformVert)*dcount, "wpaint prev");
		wp->tot = dcount;
		copy_dverts (wp->wpaint_prev, dverts, dcount);
	}
}


void vpaint_fill(Object *ob, unsigned int paintcol)
{
	Mesh *me;
	MFace *mf;
	unsigned int *mcol;
	int i, selected;

	me= get_mesh(ob);
	if(me==0 || me->totface==0) return;

	if(!me->mcol)
		make_vertexcol(ob);

	selected= (me->editflag & ME_EDIT_PAINT_MASK);

	mf = me->mface;
	mcol = (unsigned int*)me->mcol;
	for (i = 0; i < me->totface; i++, mf++, mcol+=4) {
		if (!selected || mf->flag & ME_FACE_SEL) {
			mcol[0] = paintcol;
			mcol[1] = paintcol;
			mcol[2] = paintcol;
			mcol[3] = paintcol;
		}
	}
	
	DAG_id_flush_update(&me->id, OB_RECALC_DATA);
}


/* fills in the selected faces with the current weight and vertex group */
void wpaint_fill(VPaint *wp, Object *ob, float paintweight)
{
	Mesh *me;
	MFace *mface;
	MDeformWeight *dw, *uw;
	int *indexar;
	int index, vgroup;
	unsigned int faceverts[5]={0,0,0,0,0};
	unsigned char i;
	int vgroup_mirror= -1;
	int selected;
	
	me= ob->data;
	if(me==0 || me->totface==0 || me->dvert==0 || !me->mface) return;
	
	selected= (me->editflag & ME_EDIT_PAINT_MASK);

	indexar= get_indexarray(me);

	if(selected) {
		for(index=0, mface=me->mface; index<me->totface; index++, mface++) {
			if((mface->flag & ME_FACE_SEL)==0)
				indexar[index]= 0;
			else
				indexar[index]= index+1;
		}
	}
	else {
		for(index=0; index<me->totface; index++)
			indexar[index]= index+1;
	}
	
	vgroup= ob->actdef-1;
	
	/* directly copied from weight_paint, should probaby split into a separate function */
	/* if mirror painting, find the other group */		
	if(me->editflag & ME_EDIT_MIRROR_X) {
		bDeformGroup *defgroup= BLI_findlink(&ob->defbase, ob->actdef-1);
		if(defgroup) {
			bDeformGroup *curdef;
			int actdef= 0;
			char name[32];

			BLI_strncpy(name, defgroup->name, 32);
			bone_flip_name(name, 0);		/* 0 = don't strip off number extensions */
			
			for (curdef = ob->defbase.first; curdef; curdef=curdef->next, actdef++)
				if (!strcmp(curdef->name, name))
					break;
			if(curdef==NULL) {
				int olddef= ob->actdef;	/* tsk, ED_vgroup_add sets the active defgroup */
				curdef= ED_vgroup_add_name (ob, name);
				ob->actdef= olddef;
			}
			
			if(curdef && curdef!=defgroup)
				vgroup_mirror= actdef;
		}
	}
	/* end copy from weight_paint*/
	
	copy_wpaint_prev(wp, me->dvert, me->totvert);
	
	for(index=0; index<me->totface; index++) {
		if(indexar[index] && indexar[index]<=me->totface) {
			mface= me->mface + (indexar[index]-1);
			/* just so we can loop through the verts */
			faceverts[0]= mface->v1;
			faceverts[1]= mface->v2;
			faceverts[2]= mface->v3;
			faceverts[3]= mface->v4;
			for (i=0; i<3 || faceverts[i]; i++) {
				if(!((me->dvert+faceverts[i])->flag)) {
					dw= defvert_verify_index(me->dvert+faceverts[i], vgroup);
					if(dw) {
						uw= defvert_verify_index(wp->wpaint_prev+faceverts[i], vgroup);
						uw->weight= dw->weight; /* set the undo weight */
						dw->weight= paintweight;
						
						if(me->editflag & ME_EDIT_MIRROR_X) {	/* x mirror painting */
							int j= mesh_get_x_mirror_vert(ob, faceverts[i]);
							if(j>=0) {
								/* copy, not paint again */
								if(vgroup_mirror != -1) {
									dw= defvert_verify_index(me->dvert+j, vgroup_mirror);
									uw= defvert_verify_index(wp->wpaint_prev+j, vgroup_mirror);
								} else {
									dw= defvert_verify_index(me->dvert+j, vgroup);
									uw= defvert_verify_index(wp->wpaint_prev+j, vgroup);
								}
								uw->weight= dw->weight; /* set the undo weight */
								dw->weight= paintweight;
							}
						}
					}
					(me->dvert+faceverts[i])->flag= 1;
				}
			}
		}
	}
	
	index=0;
	while (index<me->totvert) {
		(me->dvert+index)->flag= 0;
		index++;
	}
	
	MEM_freeN(indexar);
	copy_wpaint_prev(wp, NULL, 0);

	DAG_id_flush_update(&me->id, OB_RECALC_DATA);
}

/* XXX: should be re-implemented as a vertex/weight paint 'colour correct' operator
 
void vpaint_dogamma(Scene *scene)
{
	VPaint *vp= scene->toolsettings->vpaint;
	Mesh *me;
	Object *ob;
	float igam, fac;
	int a, temp;
	unsigned char *cp, gamtab[256];

	ob= OBACT;
	me= get_mesh(ob);

	if(!(ob->mode & OB_MODE_VERTEX_PAINT)) return;
	if(me==0 || me->mcol==0 || me->totface==0) return;

	igam= 1.0/vp->gamma;
	for(a=0; a<256; a++) {
		
		fac= ((float)a)/255.0;
		fac= vp->mul*pow( fac, igam);
		
		temp= 255.9*fac;
		
		if(temp<=0) gamtab[a]= 0;
		else if(temp>=255) gamtab[a]= 255;
		else gamtab[a]= temp;
	}

	a= 4*me->totface;
	cp= (unsigned char *)me->mcol;
	while(a--) {
		
		cp[1]= gamtab[ cp[1] ];
		cp[2]= gamtab[ cp[2] ];
		cp[3]= gamtab[ cp[3] ];
		
		cp+= 4;
	}
}
 */

/*
static void vpaint_blend(VPaint *vp, unsigned int *col, unsigned int *colorig, unsigned int paintcol, int alpha)
{
	Brush *brush = paint_brush(&vp->paint);

	if(brush->vertexpaint_tool==VP_MIX || brush->vertexpaint_tool==VP_BLUR) *col= mcol_blend( *col, paintcol, alpha);
	else if(brush->vertexpaint_tool==VP_ADD) *col= mcol_add( *col, paintcol, alpha);
	else if(brush->vertexpaint_tool==VP_SUB) *col= mcol_sub( *col, paintcol, alpha);
	else if(brush->vertexpaint_tool==VP_MUL) *col= mcol_mul( *col, paintcol, alpha);
	else if(brush->vertexpaint_tool==VP_LIGHTEN) *col= mcol_lighten( *col, paintcol, alpha);
	else if(brush->vertexpaint_tool==VP_DARKEN) *col= mcol_darken( *col, paintcol, alpha);
	
	// if no spray, clip color adding with colorig & orig alpha
	if((vp->flag & VP_SPRAY)==0) {
		unsigned int testcol=0, a;
		char *cp, *ct, *co;
		
		alpha= (int)(255.0*brush_alpha(brush));
		
		if(brush->vertexpaint_tool==VP_MIX || brush->vertexpaint_tool==VP_BLUR) testcol= mcol_blend( *colorig, paintcol, alpha);
		else if(brush->vertexpaint_tool==VP_ADD) testcol= mcol_add( *colorig, paintcol, alpha);
		else if(brush->vertexpaint_tool==VP_SUB) testcol= mcol_sub( *colorig, paintcol, alpha);
		else if(brush->vertexpaint_tool==VP_MUL) testcol= mcol_mul( *colorig, paintcol, alpha);
		else if(brush->vertexpaint_tool==VP_LIGHTEN)  testcol= mcol_lighten( *colorig, paintcol, alpha);
		else if(brush->vertexpaint_tool==VP_DARKEN)   testcol= mcol_darken( *colorig, paintcol, alpha);
		
		cp= (char *)col;
		ct= (char *)&testcol;
		co= (char *)colorig;
		
		for(a=0; a<4; a++) {
			if( ct[a]<co[a] ) {
				if( cp[a]<ct[a] ) cp[a]= ct[a];
				else if( cp[a]>co[a] ) cp[a]= co[a];
			}
			else {
				if( cp[a]<co[a] ) cp[a]= co[a];
				else if( cp[a]>ct[a] ) cp[a]= ct[a];
			}
		}
	}
}
*/

static int sample_backbuf_area(ViewContext *vc, int *indexar, int totface, int x, int y, float size)
{
	struct ImBuf *ibuf;
	int a, tot=0, index;
	
	/* brecht: disabled this because it obviously failes for
	   brushes with size > 64, why is this here? */
	/*if(size>64.0) size= 64.0;*/
	
	ibuf= view3d_read_backbuf(vc, x-size, y-size, x+size, y+size);
	if(ibuf) {
		unsigned int *rt= ibuf->rect;

		memset(indexar, 0, sizeof(int)*(totface+1));
		
		size= ibuf->x*ibuf->y;
		while(size--) {
				
			if(*rt) {
				index= WM_framebuffer_to_index(*rt);
				if(index>0 && index<=totface)
					indexar[index] = 1;
			}
		
			rt++;
		}
		
		for(a=1; a<=totface; a++) {
			if(indexar[a]) indexar[tot++]= a;
		}

		IMB_freeImBuf(ibuf);
	}
	
	return tot;
}

static float calc_vp_alpha_dl(VPaint *vp, ViewContext *vc, float vpimat[][3], float *vert_nor, float *mval, float pressure)
{
	Brush *brush = paint_brush(&vp->paint);
	float fac, fac_2, size, dx, dy;
	float alpha;
	short vertco[2];
	const int radius= brush_size(brush);

	project_short_noclip(vc->ar, vert_nor, vertco);
	dx= mval[0]-vertco[0];
	dy= mval[1]-vertco[1];
	
	if (brush_use_size_pressure(brush))
		size = pressure * radius;
	else
		size = radius;
	
	fac_2= dx*dx + dy*dy;
	if(fac_2 > size*size) return 0.f;
	fac = sqrtf(fac_2);
	
	alpha= brush_alpha(brush) * brush_curve_strength_clamp(brush, fac, size);
	
	if (brush_use_alpha_pressure(brush))
		alpha *= pressure;
		
	if(vp->flag & VP_NORMALS) {
		float *no= vert_nor+3;
		
		/* transpose ! */
		fac= vpimat[2][0]*no[0]+vpimat[2][1]*no[1]+vpimat[2][2]*no[2];
		if(fac>0.0) {
			dx= vpimat[0][0]*no[0]+vpimat[0][1]*no[1]+vpimat[0][2]*no[2];
			dy= vpimat[1][0]*no[0]+vpimat[1][1]*no[1]+vpimat[1][2]*no[2];
			
			alpha*= fac/sqrtf(dx*dx + dy*dy + fac*fac);
		}
		else return 0.f;
	}
	
	return alpha;
}

static void wpaint_blend(VPaint *wp, MDeformWeight *dw, MDeformWeight *uw, float alpha, float paintval, int flip)
{
	Brush *brush = paint_brush(&wp->paint);
	int tool = brush->vertexpaint_tool;
	
	if(dw==NULL || uw==NULL) return;
	
	if (flip) {
		switch(tool) {
			case IMB_BLEND_MIX:
				paintval = 1.f - paintval; break;
			case IMB_BLEND_ADD:
				tool= IMB_BLEND_SUB; break;
			case IMB_BLEND_SUB:
				tool= IMB_BLEND_ADD; break;
			case IMB_BLEND_LIGHTEN:
				tool= IMB_BLEND_DARKEN; break;
			case IMB_BLEND_DARKEN:
				tool= IMB_BLEND_LIGHTEN; break;
		}
	}
	
	if(tool==IMB_BLEND_MIX || tool==VERTEX_PAINT_BLUR)
		dw->weight = paintval*alpha + dw->weight*(1.0-alpha);
	else if(tool==IMB_BLEND_ADD)
		dw->weight += paintval*alpha;
	else if(tool==IMB_BLEND_SUB) 
		dw->weight -= paintval*alpha;
	else if(tool==IMB_BLEND_MUL) 
		/* first mul, then blend the fac */
		dw->weight = ((1.0-alpha) + alpha*paintval)*dw->weight;
	else if(tool==IMB_BLEND_LIGHTEN) {
		if (dw->weight < paintval)
			dw->weight = paintval*alpha + dw->weight*(1.0-alpha);
	} else if(tool==IMB_BLEND_DARKEN) {
		if (dw->weight > paintval)
			dw->weight = paintval*alpha + dw->weight*(1.0-alpha);
	}
	CLAMP(dw->weight, 0.0f, 1.0f);
	
	/* if no spray, clip result with orig weight & orig alpha */
	if((wp->flag & VP_SPRAY)==0) {
		float testw=0.0f;
		
		alpha= brush_alpha(brush);
		if(tool==IMB_BLEND_MIX || tool==VERTEX_PAINT_BLUR)
			testw = paintval*alpha + uw->weight*(1.0-alpha);
		else if(tool==IMB_BLEND_ADD)
			testw = uw->weight + paintval*alpha;
		else if(tool==IMB_BLEND_SUB) 
			testw = uw->weight - paintval*alpha;
		else if(tool==IMB_BLEND_MUL) 
			/* first mul, then blend the fac */
			testw = ((1.0-alpha) + alpha*paintval)*uw->weight;		
		else if(tool==IMB_BLEND_LIGHTEN) {
			if (uw->weight < paintval)
				testw = paintval*alpha + uw->weight*(1.0-alpha);
			else
				testw = uw->weight;
		} else if(tool==IMB_BLEND_DARKEN) {
			if (uw->weight > paintval)
				testw = paintval*alpha + uw->weight*(1.0-alpha);
			else
				testw = uw->weight;
		}
		CLAMP(testw, 0.0f, 1.0f);
		
		if( testw<uw->weight ) {
			if(dw->weight < testw) dw->weight= testw;
			else if(dw->weight > uw->weight) dw->weight= uw->weight;
		}
		else {
			if(dw->weight > testw) dw->weight= testw;
			else if(dw->weight < uw->weight) dw->weight= uw->weight;
		}
	}
	
}

/* ----------------------------------------------------- */

/* used for 3d view, on active object, assumes me->dvert exists */
/* if mode==1: */
/*     samples cursor location, and gives menu with vertex groups to activate */
/* else */
/*     sets wp->weight to the closest weight value to vertex */
/*     note: we cant sample frontbuf, weight colors are interpolated too unpredictable */
void sample_wpaint(Scene *scene, ARegion *ar, View3D *v3d, int mode)
{
	ViewContext vc;
	ToolSettings *ts= scene->toolsettings;
	Object *ob= OBACT;
	Mesh *me= get_mesh(ob);
	int index;
	short mval[2] = {0, 0}, sco[2];
	int vgroup= ob->actdef-1;

	if (!me) return;
	
//	getmouseco_areawin(mval);
	index= view3d_sample_backbuf(&vc, mval[0], mval[1]);
	
	if(index && index<=me->totface) {
		MFace *mface;
		
		mface= ((MFace *)me->mface) + index-1;
		
		if(mode==1) {	/* sampe which groups are in here */
			MDeformVert *dv;
			int a, totgroup;
			
			totgroup= BLI_countlist(&ob->defbase);
			if(totgroup) {
				int totmenu=0;
				int *groups=MEM_callocN(totgroup*sizeof(int), "groups");
				
				dv= me->dvert+mface->v1;
				for(a=0; a<dv->totweight; a++) {
					if (dv->dw[a].def_nr<totgroup)
						groups[dv->dw[a].def_nr]= 1;
				}
				dv= me->dvert+mface->v2;
				for(a=0; a<dv->totweight; a++) {
					if (dv->dw[a].def_nr<totgroup)
						groups[dv->dw[a].def_nr]= 1;
				}
				dv= me->dvert+mface->v3;
				for(a=0; a<dv->totweight; a++) {
					if (dv->dw[a].def_nr<totgroup)
						groups[dv->dw[a].def_nr]= 1;
				}
				if(mface->v4) {
					dv= me->dvert+mface->v4;
					for(a=0; a<dv->totweight; a++) {
						if (dv->dw[a].def_nr<totgroup)
							groups[dv->dw[a].def_nr]= 1;
					}
				}
				for(a=0; a<totgroup; a++)
					if(groups[a]) totmenu++;
				
				if(totmenu==0) {
					//notice("No Vertex Group Selected");
				}
				else {
					bDeformGroup *dg;
					short val;
					char item[40], *str= MEM_mallocN(40*totmenu+40, "menu");
					
					strcpy(str, "Vertex Groups %t");
					for(a=0, dg=ob->defbase.first; dg && a<totgroup; a++, dg= dg->next) {
						if(groups[a]) {
							sprintf(item, "|%s %%x%d", dg->name, a);
							strcat(str, item);
						}
					}
					
					val= 0; // XXX pupmenu(str);
					if(val>=0) {
						ob->actdef= val+1;
						DAG_id_flush_update(&me->id, OB_RECALC_DATA);
					}
					MEM_freeN(str);
				}
				MEM_freeN(groups);
			}
//			else notice("No Vertex Groups in Object");
		}
		else {
			DerivedMesh *dm;
			float w1, w2, w3, w4, co[3], fac;
			
			dm = mesh_get_derived_final(scene, ob, CD_MASK_BAREMESH);
			if(dm->getVertCo==NULL) {
				//notice("Not supported yet");
			}
			else {
				/* calc 3 or 4 corner weights */
				dm->getVertCo(dm, mface->v1, co);
				project_short_noclip(ar, co, sco);
				w1= ((mval[0]-sco[0])*(mval[0]-sco[0]) + (mval[1]-sco[1])*(mval[1]-sco[1]));
				
				dm->getVertCo(dm, mface->v2, co);
				project_short_noclip(ar, co, sco);
				w2= ((mval[0]-sco[0])*(mval[0]-sco[0]) + (mval[1]-sco[1])*(mval[1]-sco[1]));
				
				dm->getVertCo(dm, mface->v3, co);
				project_short_noclip(ar, co, sco);
				w3= ((mval[0]-sco[0])*(mval[0]-sco[0]) + (mval[1]-sco[1])*(mval[1]-sco[1]));
				
				if(mface->v4) {
					dm->getVertCo(dm, mface->v4, co);
					project_short_noclip(ar, co, sco);
					w4= ((mval[0]-sco[0])*(mval[0]-sco[0]) + (mval[1]-sco[1])*(mval[1]-sco[1]));
				}
				else w4= 1.0e10;
				
				fac= MIN4(w1, w2, w3, w4);
				if(w1==fac) {
					ts->vgroup_weight= defvert_find_weight(me->dvert+mface->v1, vgroup);
				}
				else if(w2==fac) {
					ts->vgroup_weight= defvert_find_weight(me->dvert+mface->v2, vgroup);
				}
				else if(w3==fac) {
					ts->vgroup_weight= defvert_find_weight(me->dvert+mface->v3, vgroup);
				}
				else if(w4==fac) {
					if(mface->v4) {
						ts->vgroup_weight= defvert_find_weight(me->dvert+mface->v4, vgroup);
					}
				}
			}
			dm->release(dm);
		}		
		
	}
	
}

static void do_weight_paint_auto_normalize(MDeformVert *dvert, 
					   int paint_nr, char *map)
{
//	MDeformWeight *dw = dvert->dw;
	float sum=0.0f, fac=0.0f, paintw=0.0f;
	int i, tot=0;

	if (!map)
		return;

	for (i=0; i<dvert->totweight; i++) {
		if (dvert->dw[i].def_nr == paint_nr)
			paintw = dvert->dw[i].weight;

		if (map[dvert->dw[i].def_nr]) {
			tot += 1;
			if (dvert->dw[i].def_nr != paint_nr)
				sum += dvert->dw[i].weight;
		}
	}
	
	if (!tot || sum <= (1.0f - paintw))
		return;

	fac = sum / (1.0f - paintw);
	fac = fac==0.0f ? 1.0f : 1.0f / fac;

	for (i=0; i<dvert->totweight; i++) {
		if (map[dvert->dw[i].def_nr]) {
			if (dvert->dw[i].def_nr != paint_nr)
				dvert->dw[i].weight *= fac;
		}
	}
}

static void do_weight_paint_vertex(VPaint *wp, Object *ob, int index, 
				   float alpha, float paintweight, int flip, 
				   int vgroup_mirror, char *validmap)
{
	Mesh *me= ob->data;
	MDeformWeight *dw, *uw;
	int vgroup= ob->actdef-1;
	
	if(wp->flag & VP_ONLYVGROUP) {
		dw= defvert_find_index(me->dvert+index, vgroup);
		uw= defvert_find_index(wp->wpaint_prev+index, vgroup);
	}
	else {
		dw= defvert_verify_index(me->dvert+index, vgroup);
		uw= defvert_verify_index(wp->wpaint_prev+index, vgroup);
	}
	if(dw==NULL || uw==NULL)
		return;
	
	wpaint_blend(wp, dw, uw, alpha, paintweight, flip);
	do_weight_paint_auto_normalize(me->dvert+index, vgroup, validmap);

	if(me->editflag & ME_EDIT_MIRROR_X) {	/* x mirror painting */
		int j= mesh_get_x_mirror_vert(ob, index);
		if(j>=0) {
			/* copy, not paint again */
			if(vgroup_mirror != -1)
				uw= defvert_verify_index(me->dvert+j, vgroup_mirror);
			else
				uw= defvert_verify_index(me->dvert+j, vgroup);
				
			uw->weight= dw->weight;

			do_weight_paint_auto_normalize(me->dvert+j, vgroup, validmap);
		}
	}
}


/* *************** set wpaint operator ****************** */

static int set_wpaint(bContext *C, wmOperator *op)		/* toggle */
{		
	Object *ob= CTX_data_active_object(C);
	Scene *scene= CTX_data_scene(C);
	VPaint *wp= scene->toolsettings->wpaint;
	Mesh *me;
	
	me= get_mesh(ob);
	if(ob->id.lib || me==NULL) return OPERATOR_PASS_THROUGH;
	
	if(ob->mode & OB_MODE_WEIGHT_PAINT) ob->mode &= ~OB_MODE_WEIGHT_PAINT;
	else ob->mode |= OB_MODE_WEIGHT_PAINT;
	
	
	/* Weightpaint works by overriding colors in mesh,
		* so need to make sure we recalc on enter and
		* exit (exit needs doing regardless because we
				* should redeform).
		*/
	DAG_id_flush_update(&me->id, OB_RECALC_DATA);
	
	if(ob->mode & OB_MODE_WEIGHT_PAINT) {
		Object *par;
		
		if(wp==NULL)
			wp= scene->toolsettings->wpaint= new_vpaint(1);

		paint_init(&wp->paint, PAINT_CURSOR_WEIGHT_PAINT);
		paint_cursor_start(C, weight_paint_poll);
		
		mesh_octree_table(ob, NULL, NULL, 's');
		
		/* verify if active weight group is also active bone */
		par= modifiers_isDeformedByArmature(ob);
		if(par && (par->mode & OB_MODE_POSE)) {
			bArmature *arm= par->data;

			if(arm->act_bone)
				ED_vgroup_select_by_name(ob, arm->act_bone->name);
		}
	}
	else {
		mesh_octree_table(NULL, NULL, NULL, 'e');
		mesh_mirrtopo_table(NULL, 'e');
	}
	
	WM_event_add_notifier(C, NC_SCENE|ND_MODE, scene);
	
	return OPERATOR_FINISHED;
}

/* for switching to/from mode */
static int paint_poll_test(bContext *C)
{
	if(CTX_data_edit_object(C))
		return 0;
	if(CTX_data_active_object(C)==NULL)
		return 0;
	return 1;
}

void PAINT_OT_weight_paint_toggle(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name= "Weight Paint Mode";
	ot->idname= "PAINT_OT_weight_paint_toggle";
	
	/* api callbacks */
	ot->exec= set_wpaint;
	ot->poll= paint_poll_test;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
}

/* ************ paint radial controls *************/

static int vpaint_radial_control_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	Paint *p = paint_get_active(CTX_data_scene(C));
	Brush *brush = paint_brush(p);
	
	WM_paint_cursor_end(CTX_wm_manager(C), p->paint_cursor);
	p->paint_cursor = NULL;
	brush_radial_control_invoke(op, brush, 1);
	return WM_radial_control_invoke(C, op, event);
}

static int vpaint_radial_control_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	int ret = WM_radial_control_modal(C, op, event);
	if(ret != OPERATOR_RUNNING_MODAL)
		paint_cursor_start(C, vertex_paint_poll);
	return ret;
}

static int vpaint_radial_control_exec(bContext *C, wmOperator *op)
{
	Brush *brush = paint_brush(&CTX_data_scene(C)->toolsettings->vpaint->paint);
	int ret = brush_radial_control_exec(op, brush, 1);
	
	WM_event_add_notifier(C, NC_BRUSH|NA_EDITED, brush);
	
	return ret;
}

static int wpaint_radial_control_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	Paint *p = paint_get_active(CTX_data_scene(C));
	Brush *brush = paint_brush(p);
	
	WM_paint_cursor_end(CTX_wm_manager(C), p->paint_cursor);
	p->paint_cursor = NULL;
	brush_radial_control_invoke(op, brush, 1);
	return WM_radial_control_invoke(C, op, event);
}

static int wpaint_radial_control_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	int ret = WM_radial_control_modal(C, op, event);
	if(ret != OPERATOR_RUNNING_MODAL)
		paint_cursor_start(C, weight_paint_poll);
	return ret;
}

static int wpaint_radial_control_exec(bContext *C, wmOperator *op)
{
	Brush *brush = paint_brush(&CTX_data_scene(C)->toolsettings->wpaint->paint);
	int ret = brush_radial_control_exec(op, brush, 1);
	
	WM_event_add_notifier(C, NC_BRUSH|NA_EDITED, brush);
	
	return ret;
}

void PAINT_OT_weight_paint_radial_control(wmOperatorType *ot)
{
	WM_OT_radial_control_partial(ot);

	ot->name= "Weight Paint Radial Control";
	ot->idname= "PAINT_OT_weight_paint_radial_control";

	ot->invoke= wpaint_radial_control_invoke;
	ot->modal= wpaint_radial_control_modal;
	ot->exec= wpaint_radial_control_exec;
	ot->poll= weight_paint_poll;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO|OPTYPE_BLOCKING;
}

void PAINT_OT_vertex_paint_radial_control(wmOperatorType *ot)
{
	WM_OT_radial_control_partial(ot);

	ot->name= "Vertex Paint Radial Control";
	ot->idname= "PAINT_OT_vertex_paint_radial_control";

	ot->invoke= vpaint_radial_control_invoke;
	ot->modal= vpaint_radial_control_modal;
	ot->exec= vpaint_radial_control_exec;
	ot->poll= vertex_paint_poll;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO|OPTYPE_BLOCKING;
}

/* ************ weight paint operator ********** */

struct WPaintData {
	ViewContext vc;
	int *indexar;
	int vgroup_mirror;
	float *vertexcosnos;
	float wpimat[3][3];
	
	/*variables for auto normalize*/
	int auto_normalize;
	char *vgroup_validmap; /*stores if vgroups tie to deforming bones or not*/
};

static char *wpaint_make_validmap(Mesh *me, Object *ob)
{
	bDeformGroup *dg;
	ModifierData *md;
	char *validmap;
	bPose *pose;
	bPoseChannel *chan;
	ArmatureModifierData *amd;
	GHash *gh = BLI_ghash_new(BLI_ghashutil_strhash, BLI_ghashutil_strcmp, "wpaint_make_validmap gh");
	int i = 0, step1=1;

	/*add all names to a hash table*/
	for (dg=ob->defbase.first, i=0; dg; dg=dg->next, i++) {
		BLI_ghash_insert(gh, dg->name, NULL);
	}

	if (!i)
		return NULL;

	validmap = MEM_callocN(i, "wpaint valid map");

	/*now loop through the armature modifiers and identify deform bones*/
	for (md = ob->modifiers.first; md; md= !md->next && step1 ? (step1=0), modifiers_getVirtualModifierList(ob) : md->next) {
		if (!(md->mode & (eModifierMode_Realtime|eModifierMode_Virtual)))
			continue;

		if (md->type == eModifierType_Armature) 
		{
			amd = (ArmatureModifierData*) md;

			if(amd->object && amd->object->pose) {
				pose = amd->object->pose;
				
				for (chan=pose->chanbase.first; chan; chan=chan->next) {
					if (chan->bone->flag & BONE_NO_DEFORM)
						continue;

					if (BLI_ghash_haskey(gh, chan->name)) {
						BLI_ghash_remove(gh, chan->name, NULL, NULL);
						BLI_ghash_insert(gh, chan->name, SET_INT_IN_POINTER(1));
					}
				}
			}
		}
	}
	
	/*add all names to a hash table*/
	for (dg=ob->defbase.first, i=0; dg; dg=dg->next, i++) {
		if (BLI_ghash_lookup(gh, dg->name) != NULL) {
			validmap[i] = 1;
		}
	}

	BLI_ghash_free(gh, NULL, NULL);

	return validmap;
}

static int wpaint_stroke_test_start(bContext *C, wmOperator *op, wmEvent *event)
{
	Scene *scene= CTX_data_scene(C);
	struct PaintStroke *stroke = op->customdata;
	ToolSettings *ts= CTX_data_tool_settings(C);
	VPaint *wp= ts->wpaint;
	Object *ob= CTX_data_active_object(C);
	struct WPaintData *wpd;
	Mesh *me;
	float mat[4][4], imat[4][4];
	
	if(scene->obedit) return OPERATOR_CANCELLED;
	
	me= get_mesh(ob);
	if(me==NULL || me->totface==0) return OPERATOR_PASS_THROUGH;
	
	/* if nothing was added yet, we make dverts and a vertex deform group */
	if (!me->dvert)
		ED_vgroup_data_create(&me->id);
	
	/* make mode data storage */
	wpd= MEM_callocN(sizeof(struct WPaintData), "WPaintData");
	paint_stroke_set_mode_data(stroke, wpd);
	view3d_set_viewcontext(C, &wpd->vc);
	wpd->vgroup_mirror= -1;
	
	/*set up auto-normalize, and generate map for detecting which
	  vgroups affect deform bones*/
	wpd->auto_normalize = ts->auto_normalize;
	if (wpd->auto_normalize)
		wpd->vgroup_validmap = wpaint_make_validmap(me, ob);
	
	//	if(qual & LR_CTRLKEY) {
	//		sample_wpaint(scene, ar, v3d, 0);
	//		return;
	//	}
	//	if(qual & LR_SHIFTKEY) {
	//		sample_wpaint(scene, ar, v3d, 1);
	//		return;
	//	}
	
	/* ALLOCATIONS! no return after this line */
	/* painting on subsurfs should give correct points too, this returns me->totvert amount */
	wpd->vertexcosnos= mesh_get_mapped_verts_nors(scene, ob);
	wpd->indexar= get_indexarray(me);
	copy_wpaint_prev(wp, me->dvert, me->totvert);
	
	/* this happens on a Bone select, when no vgroup existed yet */
	if(ob->actdef<=0) {
		Object *modob;
		if((modob = modifiers_isDeformedByArmature(ob))) {
			Bone *actbone= ((bArmature *)modob->data)->act_bone;
			if(actbone) {
				bPoseChannel *pchan= get_pose_channel(modob->pose, actbone->name);

				if(pchan) {
					bDeformGroup *dg= defgroup_find_name(ob, pchan->name);
					if(dg==NULL)
						dg= ED_vgroup_add_name(ob, pchan->name);	/* sets actdef */
					else
						ob->actdef= 1 + defgroup_find_index(ob, dg);
				}
			}
		}
	}
	if(ob->defbase.first==NULL) {
		ED_vgroup_add(ob);
	}
	
	//	if(ob->lay & v3d->lay); else error("Active object is not in this layer");
	
	/* imat for normals */
	mul_m4_m4m4(mat, ob->obmat, wpd->vc.rv3d->viewmat);
	invert_m4_m4(imat, mat);
	copy_m3_m4(wpd->wpimat, imat);
	
	/* if mirror painting, find the other group */
	if(me->editflag & ME_EDIT_MIRROR_X) {
		bDeformGroup *defgroup= BLI_findlink(&ob->defbase, ob->actdef-1);
		if(defgroup) {
			bDeformGroup *curdef;
			int actdef= 0;
			char name[32];
			
			BLI_strncpy(name, defgroup->name, 32);
			bone_flip_name(name, 0);		/* 0 = don't strip off number extensions */
			
			for (curdef = ob->defbase.first; curdef; curdef=curdef->next, actdef++)
				if (!strcmp(curdef->name, name))
					break;
			if(curdef==NULL) {
				int olddef= ob->actdef;	/* tsk, ED_vgroup_add sets the active defgroup */
				curdef= ED_vgroup_add_name (ob, name);
				ob->actdef= olddef;
			}
			
			if(curdef && curdef!=defgroup)
				wpd->vgroup_mirror= actdef;
		}
	}

	return 1;
}

static void wpaint_stroke_update_step(bContext *C, struct PaintStroke *stroke, PointerRNA *itemptr)
{
	ToolSettings *ts= CTX_data_tool_settings(C);
	VPaint *wp= ts->wpaint;
	Brush *brush = paint_brush(&wp->paint);
	struct WPaintData *wpd= paint_stroke_mode_data(stroke);
	ViewContext *vc;
	Object *ob;
	Mesh *me;
	float mat[4][4];
	float paintweight= ts->vgroup_weight;
	int *indexar;
	int totindex, index, totw, flip;
	float alpha;
	float mval[2], pressure;
	
	/* cannot paint if there is no stroke data */
	if (wpd == NULL) {
		// XXX: force a redraw here, since even though we can't paint, 
		// at least view won't freeze until stroke ends
		ED_region_tag_redraw(CTX_wm_region(C));
		return;
	}
		
	vc= &wpd->vc;
	ob= vc->obact;
	me= ob->data;
	indexar= wpd->indexar;
	
	view3d_operator_needs_opengl(C);
		
	/* load projection matrix */
	mul_m4_m4m4(mat, ob->obmat, vc->rv3d->persmat);

	flip = RNA_boolean_get(itemptr, "flip");
	pressure = RNA_float_get(itemptr, "pressure");
	RNA_float_get_array(itemptr, "mouse", mval);
	mval[0]-= vc->ar->winrct.xmin;
	mval[1]-= vc->ar->winrct.ymin;
			
	swap_m4m4(wpd->vc.rv3d->persmat, mat);
			
	/* which faces are involved */
	if(wp->flag & VP_AREA) {
		totindex= sample_backbuf_area(vc, indexar, me->totface, mval[0], mval[1], brush_size(brush));
	}
	else {
		indexar[0]= view3d_sample_backbuf(vc, mval[0], mval[1]);
		if(indexar[0]) totindex= 1;
		else totindex= 0;
	}
			
	if(wp->flag & VP_COLINDEX) {
		for(index=0; index<totindex; index++) {
			if(indexar[index] && indexar[index]<=me->totface) {
				MFace *mface= ((MFace *)me->mface) + (indexar[index]-1);
						
				if(mface->mat_nr!=ob->actcol-1) {
					indexar[index]= 0;
				}
			}
		}
	}
			
	if((me->editflag & ME_EDIT_PAINT_MASK) && me->mface) {
		for(index=0; index<totindex; index++) {
			if(indexar[index] && indexar[index]<=me->totface) {
				MFace *mface= ((MFace *)me->mface) + (indexar[index]-1);
						
				if((mface->flag & ME_FACE_SEL)==0) {
					indexar[index]= 0;
				}
			}					
		}
	}
			
	/* make sure each vertex gets treated only once */
	/* and calculate filter weight */
	totw= 0;
	if(brush->vertexpaint_tool==VERTEX_PAINT_BLUR) 
		paintweight= 0.0f;
	else
		paintweight= ts->vgroup_weight;
			
	for(index=0; index<totindex; index++) {
		if(indexar[index] && indexar[index]<=me->totface) {
			MFace *mface= me->mface + (indexar[index]-1);
					
			(me->dvert+mface->v1)->flag= 1;
			(me->dvert+mface->v2)->flag= 1;
			(me->dvert+mface->v3)->flag= 1;
			if(mface->v4) (me->dvert+mface->v4)->flag= 1;
					
			if(brush->vertexpaint_tool==VERTEX_PAINT_BLUR) {
				MDeformWeight *dw, *(*dw_func)(MDeformVert *, int);
						
				if(wp->flag & VP_ONLYVGROUP)
					dw_func= (void *)defvert_find_index; /* uses a const, cast to quiet warning */
				else
					dw_func= defvert_verify_index;
						
				dw= dw_func(me->dvert+mface->v1, ob->actdef-1);
				if(dw) {paintweight+= dw->weight; totw++;}
				dw= dw_func(me->dvert+mface->v2, ob->actdef-1);
				if(dw) {paintweight+= dw->weight; totw++;}
				dw= dw_func(me->dvert+mface->v3, ob->actdef-1);
				if(dw) {paintweight+= dw->weight; totw++;}
				if(mface->v4) {
					dw= dw_func(me->dvert+mface->v4, ob->actdef-1);
					if(dw) {paintweight+= dw->weight; totw++;}
				}
			}
		}
	}
			
	if(brush->vertexpaint_tool==VERTEX_PAINT_BLUR) 
		paintweight/= (float)totw;
			
	for(index=0; index<totindex; index++) {
				
		if(indexar[index] && indexar[index]<=me->totface) {
			MFace *mface= me->mface + (indexar[index]-1);
					
			if((me->dvert+mface->v1)->flag) {
				alpha= calc_vp_alpha_dl(wp, vc, wpd->wpimat, wpd->vertexcosnos+6*mface->v1, mval, pressure);
				if(alpha) {
					do_weight_paint_vertex(wp, ob, mface->v1, 
						alpha, paintweight, flip, wpd->vgroup_mirror, 
						wpd->vgroup_validmap);
				}
				(me->dvert+mface->v1)->flag= 0;
			}
					
			if((me->dvert+mface->v2)->flag) {
				alpha= calc_vp_alpha_dl(wp, vc, wpd->wpimat, wpd->vertexcosnos+6*mface->v2, mval, pressure);
				if(alpha) {
					do_weight_paint_vertex(wp, ob, mface->v2, 
						alpha, paintweight, flip, wpd->vgroup_mirror, 
						wpd->vgroup_validmap);
				}
				(me->dvert+mface->v2)->flag= 0;
			}
					
			if((me->dvert+mface->v3)->flag) {
				alpha= calc_vp_alpha_dl(wp, vc, wpd->wpimat, wpd->vertexcosnos+6*mface->v3, mval, pressure);
				if(alpha) {
					do_weight_paint_vertex(wp, ob, mface->v3, 
						alpha, paintweight, flip, wpd->vgroup_mirror, 
						wpd->vgroup_validmap);
				}
				(me->dvert+mface->v3)->flag= 0;
			}
					
			if((me->dvert+mface->v4)->flag) {
				if(mface->v4) {
					alpha= calc_vp_alpha_dl(wp, vc, wpd->wpimat, wpd->vertexcosnos+6*mface->v4, mval, pressure);
					if(alpha) {
						do_weight_paint_vertex(wp, ob, mface->v4, 
							alpha, paintweight, flip, wpd->vgroup_mirror,
							wpd->vgroup_validmap);
					}
					(me->dvert+mface->v4)->flag= 0;
				}
			}
		}
	}
			
	swap_m4m4(vc->rv3d->persmat, mat);
			
	DAG_id_flush_update(ob->data, OB_RECALC_DATA);
	ED_region_tag_redraw(vc->ar);
}

static void wpaint_stroke_done(bContext *C, struct PaintStroke *stroke)
{
	ToolSettings *ts= CTX_data_tool_settings(C);
	Object *ob= CTX_data_active_object(C);
	struct WPaintData *wpd= paint_stroke_mode_data(stroke);
	
	if(wpd) {
		if(wpd->vertexcosnos)
			MEM_freeN(wpd->vertexcosnos);
		MEM_freeN(wpd->indexar);
		
		if (wpd->vgroup_validmap)
			MEM_freeN(wpd->vgroup_validmap);
		
		MEM_freeN(wpd);
	}
	
	/* frees prev buffer */
	copy_wpaint_prev(ts->wpaint, NULL, 0);
	
	/* and particles too */
	if(ob->particlesystem.first) {
		ParticleSystem *psys;
		int i;
		
		for(psys= ob->particlesystem.first; psys; psys= psys->next) {
			for(i=0; i<PSYS_TOT_VG; i++) {
				if(psys->vgroup[i]==ob->actdef) {
					psys->recalc |= PSYS_RECALC_RESET;
					break;
				}
			}
		}
	}
	
	DAG_id_flush_update(ob->data, OB_RECALC_DATA);	
}


static int wpaint_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	
	op->customdata = paint_stroke_new(C, NULL, wpaint_stroke_test_start,
					  wpaint_stroke_update_step, NULL, NULL,
					  wpaint_stroke_done);
	
	/* add modal handler */
	WM_event_add_modal_handler(C, op);

	op->type->modal(C, op, event);
	
	return OPERATOR_RUNNING_MODAL;
}

void PAINT_OT_weight_paint(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name= "Weight Paint";
	ot->idname= "PAINT_OT_weight_paint";
	
	/* api callbacks */
	ot->invoke= wpaint_invoke;
	ot->modal= paint_stroke_modal;
	/* ot->exec= vpaint_exec; <-- needs stroke property */
	ot->poll= weight_paint_poll;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO|OPTYPE_BLOCKING;

	RNA_def_collection_runtime(ot->srna, "stroke", &RNA_OperatorStrokeElement, "Stroke", "");
}

static int weight_paint_set_exec(bContext *C, wmOperator *op)
{
	struct Scene *scene= CTX_data_scene(C);
	Object *obact = CTX_data_active_object(C);

	wpaint_fill(scene->toolsettings->wpaint, obact, scene->toolsettings->vgroup_weight);
	ED_region_tag_redraw(CTX_wm_region(C)); // XXX - should redraw all 3D views
	return OPERATOR_FINISHED;
}

void PAINT_OT_weight_set(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Weight Set";
	ot->idname= "PAINT_OT_weight_set";

	/* api callbacks */
	ot->exec= weight_paint_set_exec;
	ot->poll= facemask_paint_poll;

	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ************ set / clear vertex paint mode ********** */


static int set_vpaint(bContext *C, wmOperator *op)		/* toggle */
{	
	Object *ob= CTX_data_active_object(C);
	Scene *scene= CTX_data_scene(C);
	VPaint *vp= scene->toolsettings->vpaint;
	Mesh *me;
	
	me= get_mesh(ob);
	
	if(me==NULL || object_data_is_libdata(ob)) {
		ob->mode &= ~OB_MODE_VERTEX_PAINT;
		return OPERATOR_PASS_THROUGH;
	}
	
	/* toggle: end vpaint */
	if(ob->mode & OB_MODE_VERTEX_PAINT) {
		free_paintsession(ob);

		ob->mode &= ~OB_MODE_VERTEX_PAINT;
	}
	else {
		ob->mode |= OB_MODE_VERTEX_PAINT;
		/* Turn off weight painting */
		if (ob->mode & OB_MODE_WEIGHT_PAINT)
			set_wpaint(C, op);
		
		if(vp==NULL)
			vp= scene->toolsettings->vpaint= new_vpaint(0);

		if(me->mcol==NULL)
			make_vertexcol(ob);

		create_paintsession(ob);
		
		paint_cursor_start(C, vertex_paint_poll);

		paint_init(&vp->paint, PAINT_CURSOR_VERTEX_PAINT);
	}
	
	/* create pbvh */
	if(ob->mode & OB_MODE_VERTEX_PAINT) {
		DerivedMesh *dm = mesh_get_derived_final(scene, ob, CD_MASK_BAREMESH|CD_MASK_MCOL);
		ob->paint->pbvh = dm->getPBVH(ob, dm);
	}
	
	WM_event_add_notifier(C, NC_SCENE|ND_MODE, scene);
	
	return OPERATOR_FINISHED;
}

void PAINT_OT_vertex_paint_toggle(wmOperatorType *ot)
{
	
	/* identifiers */
	ot->name= "Vertex Paint Mode";
	ot->idname= "PAINT_OT_vertex_paint_toggle";
	
	/* api callbacks */
	ot->exec= set_vpaint;
	ot->poll= paint_poll_test;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}



/* ********************** vertex paint operator ******************* */

void paint_raycast_cb(PBVHNode *node, void *data_v, float *tmin)
{
	if(BLI_pbvh_node_get_tmin(node) < *tmin) {
		PaintStrokeRaycastData *data = data_v;
		
		if(BLI_pbvh_node_raycast(data->ob->paint->pbvh, node, NULL,
					 data->ray_start, data->ray_normal,
					 &data->dist, NULL, NULL)) {
			data->hit |= 1;
			*tmin = data->dist;
		}
	}
}

int vpaint_stroke_get_location(bContext *C, struct PaintStroke *stroke, float out[3], float mouse[2])
{
	// XXX: sculpt_stroke_modifiers_check(C, ss);
	return paint_stroke_get_location(C, stroke, paint_raycast_cb, NULL, out, mouse, 0);		
}

static int vpaint_stroke_test_start(bContext *C, struct wmOperator *op, wmEvent *event)
{
	ToolSettings *ts= CTX_data_tool_settings(C);
	VPaint *vp= ts->vpaint;
	Object *ob= CTX_data_active_object(C);
	Scene *scene = CTX_data_scene(C);
	DerivedMesh *dm;
	Mesh *me;

	/* context checks could be a poll() */
	me= get_mesh(ob);
	if(me==NULL || me->totface==0) return OPERATOR_PASS_THROUGH;
	
	if(me->mcol==NULL) return OPERATOR_CANCELLED;
	
	/* for filtering */
	copy_vpaint_prev(vp, (unsigned int *)me->mcol, me->totface);
	
	dm = mesh_get_derived_final(scene, ob, CD_MASK_BAREMESH|CD_MASK_MCOL);
	ob->paint->pbvh = dm->getPBVH(ob, dm);

	return 1;
}

static void vpaint_blend(Brush *brush, float col[4], float alpha)
{
	int tool = brush->vertexpaint_tool;

	if(tool == IMB_BLEND_ADD_ALPHA &&
	   (brush->flag & BRUSH_DIR_IN))
		tool = IMB_BLEND_ERASE_ALPHA;

	IMB_blend_color_float(col, col, brush->rgb, alpha, tool);
}

static float tex_strength(Brush *brush, PaintStroke *stroke,
			  float co[3], float mask, float len,
			  float radius3d)
{
	ViewContext *vc = paint_stroke_view_context(stroke);
	float (*pmat)[4];
	float tex_mouse[2] = {0,0}; /* TODO */

	paint_stroke_projection_mat(stroke, &pmat);

	return brush_tex_strength(vc, pmat, brush, co, mask, len,
				  brush_size(brush), radius3d, 0,
				  tex_mouse);
}

/* apply paint at specified coordinate
   returns 1 if paint was applied, 0 otherwise */
static int vpaint_paint_coord(Brush *brush, PaintStroke *stroke,
			      float co[3], float col[4],
			      float center[3], float radius,
			      float radius_squared, float mask)
{
	float strength, dist, dist_squared;

	dist_squared = len_squared_v3v3(center, co);

	if(dist_squared < radius_squared) {
		dist = sqrtf(dist_squared);

		strength = brush->alpha *
			tex_strength(brush, stroke, co, mask, dist, radius);
		
		vpaint_blend(brush, col, strength);

		return 1;
	}

	return 0;
}

static void vpaint_nodes_grids(Brush *brush, PaintStroke *stroke,
			       DMGridData **grids, CustomData *vdata,
			       GridKey *gridkey,
			       int *grid_indices, int totgrid,
			       int gridsize, int active, float center[3],
			       float radius)
{
	float radius_squared = radius*radius;
	int i, x, y;

	for(i = 0; i < totgrid; ++i) {
		DMGridData *grid = grids[grid_indices[i]];

		for(y = 0; y < gridsize; ++y) {
			for(x = 0; x < gridsize; ++x) {
				DMGridData *elem = GRIDELEM_AT(grid,
							       y*gridsize+x,
							       gridkey);
				float *co = GRIDELEM_CO(elem, gridkey);
				float *gridcol = GRIDELEM_COLOR(elem, gridkey)[active];
				float mask = paint_mask_from_gridelem(elem,
								      gridkey,
								      vdata);

				vpaint_paint_coord(brush, stroke, co,
						   gridcol,
						   center, radius,
						   radius_squared,
						   mask);
			}
		}
	}
}

static void vpaint_nodes_faces(Brush *brush, PaintStroke *stroke,
			       MFace *mface, MVert *mvert,
			       CustomData *vdata, CustomData *fdata,
			       int *face_indices, int totface, float center[3],
			       float radius)
{
	float radius_squared = radius*radius;
	MCol *mcol;
	int pmask_totlayer, pmask_first_layer;
	int i, j;

	mcol = CustomData_get_layer(fdata, CD_MCOL);
	pmask_totlayer = CustomData_number_of_layers(vdata, CD_PAINTMASK);
	pmask_first_layer = CustomData_get_layer_index(vdata, CD_PAINTMASK);

	for(i = 0; i < totface; ++i) {
		int face_index = face_indices[i];
		MFace *f = mface + face_index;
		int S = f->v4 ? 4 : 3;

		for(j = 0; j < S; ++j) {
			int vndx = (&f->v1)[j];
			int cndx = face_index*4 + j;
			float *co = mvert[vndx].co;
			float fcol[4];
			float mask;

			fcol[0] = mcol[cndx].b / 255.0f;
			fcol[1] = mcol[cndx].g / 255.0f;
			fcol[2] = mcol[cndx].r / 255.0f;
			fcol[3] = mcol[cndx].a / 255.0f;

			mask = paint_mask_from_vertex(vdata, vndx,
						      pmask_totlayer,
						      pmask_first_layer);

			vpaint_paint_coord(brush, stroke, co, fcol, center,
					   radius, radius_squared, mask);

			mcol[cndx].b = fcol[0] * 255.0f;
			mcol[cndx].g = fcol[1] * 255.0f;
			mcol[cndx].r = fcol[2] * 255.0f;
			mcol[cndx].a = fcol[3] * 255.0f;
		}
	}
}

static void vpaint_nodes_grids_smooth(Brush *brush, PaintStroke *stroke,
				      DMGridData **grids, CustomData *vdata,
				      GridKey *gridkey,
				      int *grid_indices, int totgrid,
				      int gridsize, int active, float center[3],
				      float radius, float radius_squared)
{
	int i, j, x, y, x2, y2;

	/* TODO: this could be better optimized like sculpt,
	   just doing the simplest smooth for now */

	for(i = 0; i < totgrid; ++i) {
		DMGridData *grid = grids[grid_indices[i]], *act_elem, *elem;

		for(y = 0; y < gridsize; ++y) {
			for(x = 0; x < gridsize; ++x) {
				float avg_col[4] = {0, 0, 0, 0};
				float *act_col, strength, mask;
				float *co, dist_squared, dist;
				int totcol = 0;

				act_elem = GRIDELEM_AT(grid, y*gridsize + x, gridkey);

				co = GRIDELEM_CO(act_elem, gridkey);
				dist_squared = len_squared_v3v3(center, co);

				if(dist_squared > radius_squared)
					continue;

				dist = sqrtf(dist_squared);

				for(y2 = -1; y2 <= 1; y2+=2) {
					if(y + y2 < 0 || y + y2 >= gridsize)
						continue;

					for(x2 = -1; x2 <= 1; x2+=2) {
						if(x + x2 < 0 || x + x2 >= gridsize)
							continue;

						elem = GRIDELEM_AT(grid, (y+y2)*gridsize + (x+x2), gridkey);

						++totcol;
						for(j = 0; j < 4; ++j)
							avg_col[j] += GRIDELEM_COLOR(elem, gridkey)[active][j];
					}
				}

				mask = paint_mask_from_gridelem(act_elem,
								gridkey,
								vdata);
				strength = brush->alpha *
					tex_strength(brush, stroke,
						     co, mask,
						     dist, radius);
				act_col = GRIDELEM_COLOR(act_elem, gridkey)[active];
				for(j = 0; j < 4; ++j)
					act_col[j] = interpf(avg_col[j] / totcol, act_col[j], strength);
			}
		}
	}

	/* be sure to stitch grids after */
}

static void vpaint_nodes_faces_smooth(Brush *brush, PaintStroke *stroke,
				      PBVH *pbvh, PBVHNode *node,
				      MFace *mface,
				      CustomData *fdata, ListBase *fmap,
				      float center[3],
				      float radius, float radius_squared)
{
	PBVHVertexIter vd;
	PaintStrokeTest test;
	MCol *mcol;
	
	paint_stroke_test_init(&test, center, radius_squared);
	mcol = CustomData_get_layer(fdata, CD_MCOL);

	BLI_pbvh_vertex_iter_begin(pbvh, node, vd, PBVH_ITER_UNIQUE) {
		if(paint_stroke_test(&test, vd.co)) {
			IndexNode *n;
			float strength, avg_col[4] = {0, 0, 0, 0};
			int vndx = vd.vert_indices[vd.i], totcol, i, j;

			/* first find average color from neighboring faces */
			totcol = 0;
			for(n = fmap[vndx].first; n; n = n->next) {
				int fndx = n->index;
				MFace *f = &mface[fndx];
				int S = f->v4 ? 4 : 3;

				for(i = 0; i < S; ++i) {
					int cndx = fndx*4 + i;

					avg_col[0] += mcol[cndx].b / 255.0;
					avg_col[1] += mcol[cndx].g / 255.0;
					avg_col[2] += mcol[cndx].r / 255.0;
					avg_col[3] += mcol[cndx].a / 255.0;
					
					++totcol;
				}
			}

			for(i = 0; i < 4; ++i)
				avg_col[i] /= totcol;

			/* for all face corners matching vndx,
			   interp towards the averaged color */
			for(n = fmap[vndx].first; n; n = n->next) {
				int fndx = n->index;
				MFace *f = &mface[fndx];
				int S = f->v4 ? 4 : 3;

				for(i = 0; i < S; ++i) {
					int cndx = fndx*4 + i;

					if((&f->v1)[i] != vndx)
						continue;

					strength = brush->alpha *
						tex_strength(brush, stroke,
							     vd.co,
							     vd.mask_combined,
							     test.dist, radius);

					for(j = 0; j < 4; ++j) {
						unsigned char *c;
						float col;

						c = ((unsigned char*)(&mcol[cndx])) + j;

						col = *c / 255.0f;
						col = interpf(avg_col[3 - j], col, strength);
						*c = col * 255.0f;
					}
				}
			}
		}
	}
	BLI_pbvh_vertex_iter_end;
}

static int vpaint_find_gridkey_active_layer(CustomData *fdata, GridKey *gridkey)
{
	int active, i;

	active = CustomData_get_active_layer_index(fdata, CD_MCOL);
	
	if(active == -1)
		return -1;

	for(i = 0; i < gridkey->color; ++i) {
		if(!strcmp(gridkey->color_names[i],
			   fdata->layers[active].name))
			return i;
	}

	return -1;
}

static void vpaint_nodes(VPaint *vp, PaintStroke *stroke,
			 Scene *scene, Object *ob,
			 PBVHNode **nodes, int totnode,
			 float center[3], float radius)
{
	PBVH *pbvh = ob->paint->pbvh;
	Brush *brush = paint_brush(&vp->paint);
	int blur = brush->vertexpaint_tool == VERTEX_PAINT_BLUR;
	int n;

	for(n = 0; n < totnode; ++n) {
		CustomData *vdata = NULL;
		CustomData *fdata = NULL;

		BLI_pbvh_get_customdata(pbvh, &vdata, &fdata);

		if(BLI_pbvh_uses_grids(pbvh)) {
			DMGridData **grids;
			GridKey *gridkey;
			int *grid_indices, totgrid, gridsize;
			int active;
			
			BLI_pbvh_node_get_grids(pbvh, nodes[n], &grid_indices,
						&totgrid, NULL, &gridsize, &grids,
						NULL, &gridkey);

			active = vpaint_find_gridkey_active_layer(fdata,
								  gridkey);

			if(active != -1) {
				if(blur) {
					vpaint_nodes_grids_smooth(brush, stroke,
							   grids, vdata,
							   gridkey,
							   grid_indices,
							   totgrid,
							   gridsize,
							   active, center,
							   radius, radius*radius);
					BLI_pbvh_node_set_flags(nodes[n],
								SET_INT_IN_POINTER(PBVH_NeedsColorStitch));
				}
				else {
					vpaint_nodes_grids(brush, stroke,
							   grids, vdata,
							   gridkey,
							   grid_indices,
							   totgrid,
							   gridsize,
							   active, center,
							   radius);
				}
			}
		}
		else {
			MVert *mvert;
			MFace *mface;
			int *face_indices, totface;

			BLI_pbvh_node_get_verts(pbvh, nodes[n], NULL, &mvert);
			BLI_pbvh_node_get_faces(pbvh, nodes[n], &mface,
						&face_indices, NULL, &totface);

			if(blur) {
				DerivedMesh *dm;
				ListBase *fmap;

				dm = mesh_get_derived_final(scene, ob, CD_MASK_BAREMESH);
				fmap = dm->getFaceMap ? dm->getFaceMap(ob, dm) : NULL;

				vpaint_nodes_faces_smooth(brush, stroke,
							  pbvh, nodes[n],
							  mface, fdata,
							  fmap, center,
							  radius, radius*radius);
			}				
			else {
				vpaint_nodes_faces(brush, stroke,
						   mface, mvert,
						   vdata, fdata,
						   face_indices, totface,
						   center, radius);
			}
		}

		BLI_pbvh_node_set_flags(nodes[n],
			SET_INT_IN_POINTER(PBVH_UpdateColorBuffers|
					   PBVH_UpdateRedraw));
	}
}

typedef struct {
	int hit_index;
	int grid_hit_index;
	PBVHNode *node;
} VPaintColorOneFaceHitData;

void vpaint_color_one_face_raycast_cb(PBVHNode *node, void *data_v, float *tmin)
{
	if (BLI_pbvh_node_get_tmin(node) < *tmin) {
		PaintStrokeRaycastData *data = data_v;
		PaintSession *ps = data->ob->paint;
		VPaintColorOneFaceHitData *mode_data = data->mode_data;

		if(BLI_pbvh_node_raycast(ps->pbvh, node, NULL,
					 data->ray_start, data->ray_normal,
					 &data->dist, &mode_data->hit_index,
					 &mode_data->grid_hit_index)) {
			data->hit |= 1;
			mode_data->node = node;
			*tmin = data->dist;
		}
	}
}

/* applies brush color to a single point in a multires grid */
static void vpaint_color_single_gridelem(Brush *brush, DMGridData **grids,
					 GridKey *gridkey, int *grid_indices,
					 int gridsize, int active,
					 int hit_index, int grid_hit_index)
{
	float *gridcol;

	gridcol = GRIDELEM_COLOR_AT(grids[grid_indices[hit_index]],
				    grid_hit_index, gridkey)[active];

	vpaint_blend(brush, gridcol, brush->alpha);
}

/* applies brush color to a single face */
static void vpaint_color_single_face(Brush *brush, MFace *mface,
				     CustomData *fdata, int *face_indices,
				     int hit_index)
{
	MCol *mcol;
	int i, S;

	mcol = CustomData_get_layer(fdata, CD_MCOL);
	mface += face_indices[hit_index];
	S = mface->v4 ? 4 : 3;

	for(i = 0; i < S; ++i) {
		int cndx = face_indices[hit_index]*4 + i;
		float fcol[4];

		fcol[0] = mcol[cndx].b / 255.0f;
		fcol[1] = mcol[cndx].g / 255.0f;
		fcol[2] = mcol[cndx].r / 255.0f;
		fcol[3] = mcol[cndx].a / 255.0f;

		vpaint_blend(brush, fcol, brush->alpha);

		mcol[cndx].b = fcol[0] * 255.0f;
		mcol[cndx].g = fcol[1] * 255.0f;
		mcol[cndx].r = fcol[2] * 255.0f;
		mcol[cndx].a = fcol[3] * 255.0f;
	}
}

static void vpaint_color_single_element(bContext *C, PaintStroke *stroke,
					PointerRNA *itemptr)
{
	VPaint *vp= CTX_data_tool_settings(C)->vpaint;
	ViewContext *vc = paint_stroke_view_context(stroke);
	Brush *brush = paint_brush(&vp->paint);
	float mouse[2], hit_loc[3];
	VPaintColorOneFaceHitData hit_data;

	RNA_float_get_array(itemptr, "mouse", mouse);

	if(paint_stroke_get_location(C, stroke,
				     vpaint_color_one_face_raycast_cb,
				     &hit_data, hit_loc, mouse, 0)) {
		PBVH *pbvh = vc->obact->paint->pbvh;
		CustomData *fdata;

		BLI_pbvh_get_customdata(pbvh, NULL, &fdata);

		if(BLI_pbvh_uses_grids(pbvh)) {
			DMGridData **grids;
			GridKey *gridkey;
			int *grid_indices, gridsize;
			int active;

			BLI_pbvh_node_get_grids(pbvh, hit_data.node,
						&grid_indices,
						NULL, NULL, &gridsize, &grids,
						NULL, &gridkey);

			active = vpaint_find_gridkey_active_layer(fdata,
								  gridkey);

			if(active != -1) {
				vpaint_color_single_gridelem(brush,
					grids,
					gridkey,
					grid_indices,
					gridsize, active,
					hit_data.hit_index,
					hit_data.grid_hit_index);
			}
		}
		else {
			MFace *mface;
			int *face_indices;

			BLI_pbvh_node_get_faces(pbvh, hit_data.node,
						&mface, &face_indices,
						NULL, NULL);

			vpaint_color_single_face(brush, mface, fdata,
				 face_indices,
				 hit_data.hit_index);
		}

		BLI_pbvh_node_set_flags(hit_data.node,
			SET_INT_IN_POINTER(PBVH_UpdateColorBuffers|
					   PBVH_UpdateRedraw));
	}
}

static void vpaint_stroke_update_step(bContext *C, PaintStroke *stroke,
					  PointerRNA *itemptr)
{
	VPaint *vp= CTX_data_tool_settings(C)->vpaint;
	Object *ob = CTX_data_active_object(C);

	paint_stroke_apply_brush(C, stroke, &vp->paint);

	if(paint_brush(&vp->paint)->vertexpaint_tool == VERTEX_PAINT_BLUR)
		multires_stitch_grids(ob);
	multires_mark_as_modified(ob);

	/* partial redraw */
	paint_tag_partial_redraw(C, ob);
}

static void vpaint_stroke_brush_action(bContext *C, PaintStroke *stroke)
{

	VPaint *vp= CTX_data_tool_settings(C)->vpaint;

	if(vp->flag & VP_AREA) {
		ViewContext *vc = paint_stroke_view_context(stroke);
		Scene *scene = CTX_data_scene(C);
		Object *ob = vc->obact;
		Brush *brush = paint_brush(&vp->paint);
		PBVHSearchSphereData search_data;
		PBVHNode **nodes;
		int totnode;
		float center[3], radius;

		paint_stroke_symmetry_location(stroke, center);

		search_data.center = center;
		
		radius = paint_stroke_radius(stroke);
		search_data.radius_squared = radius*radius;
		search_data.original = 0;

		BLI_pbvh_search_gather(ob->paint->pbvh, BLI_pbvh_search_sphere_cb,
				       &search_data, &nodes, &totnode);
		
		if(brush->flag & BRUSH_MASK) {
			paintmask_brush_apply(&vp->paint, stroke, ob, nodes, totnode,
					      center, brush->alpha, radius);
		}
		else {
			vpaint_nodes(vp, stroke, scene, ob, nodes, totnode,
				     center, radius);
		}

		if(nodes)
			MEM_freeN(nodes);
	}
	else {
		// TODO (itemptr) vpaint_color_single_element(C, stroke, itemptr);
		/* TODO: mask */
	}
}

static void vpaint_stroke_done(bContext *C, struct PaintStroke *stroke)
{
	ToolSettings *ts= CTX_data_tool_settings(C);

	/* frees prev buffer */
	copy_vpaint_prev(ts->vpaint, NULL, 0);
}

static int vpaint_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	op->customdata = paint_stroke_new(C,
					  vpaint_stroke_get_location,
					  vpaint_stroke_test_start,
					  vpaint_stroke_update_step,
					  NULL,
					  vpaint_stroke_brush_action,
					  vpaint_stroke_done);
	
	/* add modal handler */
	WM_event_add_modal_handler(C, op);

	op->type->modal(C, op, event);
	
	return OPERATOR_RUNNING_MODAL;
}

void PAINT_OT_vertex_paint(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Vertex Paint";
	ot->idname= "PAINT_OT_vertex_paint";
	
	/* api callbacks */
	ot->invoke= vpaint_invoke;
	ot->modal= paint_stroke_modal;
	/* ot->exec= vpaint_exec; <-- needs stroke property */
	ot->poll= vertex_paint_poll;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO|OPTYPE_BLOCKING;

	RNA_def_collection_runtime(ot->srna, "stroke", &RNA_OperatorStrokeElement, "Stroke", "");
}

/* ********************** weight from bones operator ******************* */

static int weight_from_bones_poll(bContext *C)
{
	Object *ob= CTX_data_active_object(C);

	return (ob && (ob->mode & OB_MODE_WEIGHT_PAINT) && modifiers_isDeformedByArmature(ob));
}

static int weight_from_bones_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Object *ob= CTX_data_active_object(C);
	Object *armob= modifiers_isDeformedByArmature(ob);
	Mesh *me= ob->data;
	int type= RNA_enum_get(op->ptr, "type");

	create_vgroups_from_armature(scene, ob, armob, type, (me->editflag & ME_EDIT_MIRROR_X));

	DAG_id_flush_update(&me->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_GEOM|ND_DATA, me);

	return OPERATOR_FINISHED;
}

void PAINT_OT_weight_from_bones(wmOperatorType *ot)
{
	static EnumPropertyItem type_items[]= {
		{ARM_GROUPS_AUTO, "AUTOMATIC", 0, "Automatic", "Automatic weights froms bones"},
		{ARM_GROUPS_ENVELOPE, "ENVELOPES", 0, "From Envelopes", "Weights from envelopes with user defined radius"},
		{0, NULL, 0, NULL, NULL}};

	/* identifiers */
	ot->name= "Weight from Bones";
	ot->idname= "PAINT_OT_weight_from_bones";
	
	/* api callbacks */
	ot->exec= weight_from_bones_exec;
	ot->invoke= WM_menu_invoke;
	ot->poll= weight_from_bones_poll;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	/* properties */
	ot->prop= RNA_def_enum(ot->srna, "type", type_items, 0, "Type", "Method to use for assigning weights.");
}


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
 * The Original Code is: all of this file.
 *
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 * Drawing routines for the Action window type
 */

/* System includes ----------------------------------------------------- */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "MEM_guardedalloc.h"

#include "BMF_Api.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

/* Types --------------------------------------------------------------- */
#include "DNA_listBase.h"
#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_curve_types.h"
#include "DNA_ipo_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_constraint_types.h"
#include "DNA_key_types.h"
#include "DNA_lamp_types.h"
#include "DNA_material_types.h"
#include "DNA_userdef_types.h"
#include "DNA_gpencil_types.h"

#include "BKE_action.h"
#include "BKE_depsgraph.h"
#include "BKE_ipo.h"
#include "BKE_key.h"
#include "BKE_material.h"
#include "BKE_object.h"
#include "BKE_global.h"
#include "BKE_utildefines.h"

/* Everything from source (BIF, BDR, BSE) ------------------------------ */ 

#include "BIF_editaction.h"
#include "BIF_editkey.h"
#include "BIF_editnla.h"
#include "BIF_interface.h"
#include "BIF_interface_icons.h"
#include "BIF_drawgpencil.h"
#include "BIF_gl.h"
#include "BIF_glutil.h"
#include "BIF_keyframing.h"
#include "BIF_language.h"
#include "BIF_resources.h"
#include "BIF_screen.h"
#include "BIF_mywindow.h"
#include "BIF_space.h"

#include "BDR_drawaction.h"
#include "BDR_editcurve.h"
#include "BDR_gpencil.h"

#include "BSE_drawnla.h"
#include "BSE_drawipo.h"
#include "BSE_drawview.h"
#include "BSE_editaction_types.h"
#include "BSE_editipo.h"
#include "BSE_headerbuttons.h"
#include "BSE_time.h"
#include "BSE_view.h"

/* 'old' stuff": defines and types, and own include -------------------- */

#include "blendef.h"
#include "interface.h"
#include "mydevice.h"

/********************************** Slider Stuff **************************** */

/* sliders for shapekeys */
static void meshactionbuts(SpaceAction *saction, Object *ob, Key *key)
{
	int           i;
	char          str[64];
	float	      x, y;
	uiBlock       *block;
	uiBut 		  *but;

#define XIC 20
#define YIC 20

	/* lets make the rvk sliders */

	/* reset the damn myortho2 or the sliders won't draw/redraw
	 * correctly *grumble*
	 */
	mywinset(curarea->win);
	myortho2(-0.375f, curarea->winx-0.375f, G.v2d->cur.ymin, G.v2d->cur.ymax);

    sprintf(str, "actionbuttonswin %d", curarea->win);
    block= uiNewBlock (&curarea->uiblocks, str, UI_EMBOSS, UI_HELV, curarea->win);

	x = NAMEWIDTH + 1;
    y = 0.0f;

	/* make the little 'open the sliders' widget */
	// should eventually be removed
    BIF_ThemeColor(TH_FACE); // this slot was open... (???... Aligorith)
	glRects(2, (short)y + 2*CHANNELHEIGHT - 2, ACTWIDTH - 2, (short)y + CHANNELHEIGHT + 2);
	glColor3ub(0, 0, 0);
	glRasterPos2f(4, y + CHANNELHEIGHT + 6);
	BMF_DrawString(G.font, "Sliders");

	uiBlockSetEmboss(block, UI_EMBOSSN);

	if (!(G.saction->flag & SACTION_SLIDERS)) {
		ACTWIDTH = NAMEWIDTH;
		but=uiDefIconButBitS(block, TOG, SACTION_SLIDERS, B_REDR, 
					  ICON_DISCLOSURE_TRI_RIGHT,
					  NAMEWIDTH - XIC - 5, (short)y + CHANNELHEIGHT,
					  XIC,YIC-2,
					  &(G.saction->flag), 0, 0, 0, 0, 
					  "Show action window sliders");
		/* no hilite, the winmatrix is not correct later on... */
		uiButSetFlag(but, UI_NO_HILITE);

	}
	else {
		but= uiDefIconButBitS(block, TOG, SACTION_SLIDERS, B_REDR, 
					  ICON_DISCLOSURE_TRI_DOWN,
					  NAMEWIDTH - XIC - 5, (short)y + CHANNELHEIGHT,
					  XIC,YIC-2,
					  &(G.saction->flag), 0, 0, 0, 0, 
					  "Hide action window sliders");
		/* no hilite, the winmatrix is not correct later on... */
		uiButSetFlag(but, UI_NO_HILITE);
		
		ACTWIDTH = NAMEWIDTH + SLIDERWIDTH;
		
		/* sliders are open so draw them */
		BIF_ThemeColor(TH_FACE); 
		
		glRects(NAMEWIDTH,  0,  NAMEWIDTH+SLIDERWIDTH,  curarea->winy);
		uiBlockSetEmboss(block, UI_EMBOSS);
		for (i=1; i < key->totkey; i++) {
			make_rvk_slider(block, ob, i, 
							(int)x, (int)y, SLIDERWIDTH-2, CHANNELHEIGHT-1, "Slider to control Shape Keys");
			
			y-=CHANNELHEIGHT+CHANNELSKIP;
			
			/* see sliderval array in editkey.c */
			if (i >= 255) break;
		}
	}
	uiDrawBlock(block);
}

static void icu_slider_func(void *voidicu, void *voidignore) 
{
	/* the callback for the icu sliders ... copies the
	 * value from the icu->curval into a bezier at the
	 * right frame on the right ipo curve (creating both the
	 * ipo curve and the bezier if needed).
	 */
	IpoCurve  *icu= voidicu;
	BezTriple *bezt=NULL;
	float cfra, icuval;

	cfra = frame_to_float(CFRA);
	if (G.saction->pin==0 && OBACT)
		cfra= get_action_frame(OBACT, cfra);
	
	/* if the ipocurve exists, try to get a bezier
	 * for this frame
	 */
	bezt = get_bezt_icu_time(icu, &cfra, &icuval);

	/* create the bezier triple if one doesn't exist,
	 * otherwise modify it's value
	 */
	if (bezt == NULL) {
		insert_vert_icu(icu, cfra, icu->curval, 0);
	}
	else {
		bezt->vec[1][1] = icu->curval;
	}

	/* make sure the Ipo's are properly processed and
	 * redraw as necessary
	 */
	sort_time_ipocurve(icu);
	testhandles_ipocurve(icu);
	
	/* nla-update (in case this affects anything) */
	synchronize_action_strips();
	
	/* do redraw pushes, and also the depsgraph flushes */
	if (OBACT->pose || ob_get_key(OBACT))
		DAG_object_flush_update(G.scene, OBACT, OB_RECALC);
	else
		DAG_object_flush_update(G.scene, OBACT, OB_RECALC_OB);
	
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWACTION, 0);
	allqueue(REDRAWNLA, 0);
	allqueue(REDRAWIPO, 0);
	allspace(REMAKEIPO, 0);
	allqueue(REDRAWBUTSALL, 0);
}

static void make_icu_slider(uiBlock *block, IpoCurve *icu,
					 int x, int y, int w, int h, char *tip)
{
	/* create a slider for the ipo-curve*/
	uiBut *but;
	
	if(icu == NULL) return;
	
	if (IS_EQ(icu->slide_max, icu->slide_min)) {
		if (IS_EQ(icu->ymax, icu->ymin)) {
			if (ELEM(icu->blocktype, ID_CO, ID_KE)) {
				/* hack for constraints and shapekeys (and maybe a few others) */
				icu->slide_min= 0.0;
				icu->slide_max= 1.0;
			}
			else {
				icu->slide_min= -100;
				icu->slide_max= 100;
			}
		}
		else {
			icu->slide_min= icu->ymin;
			icu->slide_max= icu->ymax;
		}
	}
	if (icu->slide_min >= icu->slide_max) {
		SWAP(float, icu->slide_min, icu->slide_max);
	}

	but=uiDefButF(block, NUMSLI, REDRAWVIEW3D, "",
				  x, y , w, h,
				  &(icu->curval), icu->slide_min, icu->slide_max, 
				  10, 2, tip);
	
	uiButSetFunc(but, icu_slider_func, icu, NULL);
	
	// no hilite, the winmatrix is not correct later on...
	uiButSetFlag(but, UI_NO_HILITE);
}

/* sliders for ipo-curves of active action-channel */
static void action_icu_buts(SpaceAction *saction)
{
	ListBase act_data = {NULL, NULL};
	bActListElem *ale;
	int filter;
	void *data;
	short datatype;
	
	char          str[64];
	float	        x, y;
	uiBlock       *block;

	/* lets make the action sliders */

	/* reset the damn myortho2 or the sliders won't draw/redraw
	 * correctly *grumble*
	 */
	mywinset(curarea->win);
	myortho2(-0.375f, curarea->winx-0.375f, G.v2d->cur.ymin, G.v2d->cur.ymax);
	
    sprintf(str, "actionbuttonswin %d", curarea->win);
    block= uiNewBlock (&curarea->uiblocks, str, 
                       UI_EMBOSS, UI_HELV, curarea->win);

	x = (float)NAMEWIDTH + 1;
    y = 0.0f;
	
	uiBlockSetEmboss(block, UI_EMBOSSN);

	if (G.saction->flag & SACTION_SLIDERS) {
		/* sliders are open so draw them */
		
		/* get editor data */
		data= get_action_context(&datatype);
		if (data == NULL) return;
		
		/* build list of channels to draw */
		filter= (ACTFILTER_FORDRAWING|ACTFILTER_VISIBLE|ACTFILTER_CHANNELS);
		actdata_filter(&act_data, filter, data, datatype);
		
		/* draw backdrop first */
		BIF_ThemeColor(TH_FACE); // change this color... it's ugly
		glRects(NAMEWIDTH,  (short)G.v2d->cur.ymin,  NAMEWIDTH+SLIDERWIDTH,  (short)G.v2d->cur.ymax);
		
		uiBlockSetEmboss(block, UI_EMBOSS);
		for (ale= act_data.first; ale; ale= ale->next) {
			const float yminc= y-CHANNELHEIGHT/2;
			const float ymaxc= y+CHANNELHEIGHT/2;
			
			/* check if visible */
			if ( IN_RANGE(yminc, G.v2d->cur.ymin, G.v2d->cur.ymax) ||
				 IN_RANGE(ymaxc, G.v2d->cur.ymin, G.v2d->cur.ymax) ) 
			{
				/* determine what needs to be drawn */
				switch (ale->type) {
					case ACTTYPE_CONCHAN: /* constraint channel */
					{
						bActionChannel *achan = (bActionChannel *)ale->owner;
						IpoCurve *icu = (IpoCurve *)ale->key_data;
						
						/* only show if owner is selected */
						if ((ale->ownertype == ACTTYPE_OBJECT) || SEL_ACHAN(achan)) {
							make_icu_slider(block, icu,
											(int)x, (int)y, SLIDERWIDTH-2, CHANNELHEIGHT-2, 
											"Slider to control current value of Constraint Influence");
						}
					}
						break;
					case ACTTYPE_ICU: /* ipo-curve channel */
					{
						bActionChannel *achan = (bActionChannel *)ale->owner;
						IpoCurve *icu = (IpoCurve *)ale->key_data;
						
						/* only show if owner is selected */
						if ((ale->ownertype == ACTTYPE_OBJECT) || SEL_ACHAN(achan)) {
							make_icu_slider(block, icu,
											(int)x, (int)y, SLIDERWIDTH-2, CHANNELHEIGHT-2, 
											"Slider to control current value of IPO-Curve");
						}
					}
						break;
					case ACTTYPE_SHAPEKEY: /* shapekey channel */
					{
						Object *ob= (Object *)ale->id;
						IpoCurve *icu= (IpoCurve *)ale->key_data;
						
						// TODO: only show if object is active 
						if (icu) {
							make_icu_slider(block, icu,
										(int)x, (int)y, SLIDERWIDTH-2, CHANNELHEIGHT-2, 
										"Slider to control ShapeKey");
						}
						else if (ob && ale->index) {
							make_rvk_slider(block, ob, ale->index, 
									(int)x, (int)y, SLIDERWIDTH-2, CHANNELHEIGHT-1, "Slider to control Shape Keys");
						}
					}
						break;
				}
			}
			
			/* adjust y-position for next one */
			y-=CHANNELHEIGHT+CHANNELSKIP;
		}
		
		/* free tempolary channels */
		BLI_freelistN(&act_data);
	}
	uiDrawBlock(block);
}

/********************************** Current Frame **************************** */

void draw_cfra_number(float cfra)
{
	float xscale, yscale, yspace, ypixels, x;
	short slen, time=0;
	char str[32];
	
	/* check if current spacetype allows drawing */
	switch (curarea->spacetype) {
		case SPACE_ACTION: /* action editor */
			if (G.saction->flag & SACTION_NODRAWCFRANUM)
				return;
			else if (G.saction->flag & SACTION_DRAWTIME)
				time= 1;
			break;
		case SPACE_NLA: /* nla editor */
			if (G.snla->flag & SNLA_NODRAWCFRANUM)
				return;
			else if (G.snla->flag & SNLA_DRAWTIME)
				time= 1;
			break;
		case SPACE_IPO: /* ipo editor */
			if (G.sipo->flag & SIPO_NODRAWCFRANUM)
				return;
			else if (G.sipo->flag & SIPO_DRAWTIME)
				time= 1;
			break;
			
		default: /* other spaces don't support this */
			return;
	}
	
	/* move ortho view to align with slider in bottom */
	glTranslatef(0.0f, G.v2d->cur.ymin, 0.0f);
	
	/* bad hacks in drawing markers... inverse correct that as well */
	yspace= G.v2d->cur.ymax - G.v2d->cur.ymin;
	ypixels= G.v2d->mask.ymax - G.v2d->mask.ymin;
	glTranslatef(0.0f, 5.0*yspace/ypixels, 0.0f);
	
	/* because the frame number text is subject to the same scaling as the contents of the view */
	view2d_getscale(G.v2d, &xscale, &yscale);
	glScalef(1.0/xscale, 1.0/yscale, 1.0);
	
	if (time) 
		sprintf(str, "   %.2f", FRA2TIME(CFRA));
	else 
		sprintf(str, "   %d", CFRA);
	slen= BIF_GetStringWidth(G.font, str, 0);
	
	/* get starting coordinates for drawing */
	x= cfra * xscale;
	
	/* draw green box around/behind text */
	BIF_ThemeColor(TH_CFRAME);
	BIF_ThemeColorShadeAlpha(TH_CFRAME, 0, -100);
	glRectf(x,  0,  x+slen,  15);
	
	/* draw current frame number - black text */
	BIF_ThemeColor(TH_TEXT);
	ui_rasterpos_safe(x-5, 3, 1.0);
	BIF_DrawString(G.fonts, str, 0);
	
	/* restore view transform */
	glScalef(xscale, yscale, 1.0);
	glTranslatef(0.0f, -G.v2d->cur.ymin, 0.0f);
	glTranslatef(0.0f, -5.0*yspace/ypixels, 0.0f);
}

void draw_cfra_action (void)
{
	Object *ob;
	float vec[2];
	
	/* Draw a light green line to indicate current frame */
	vec[0]= (float)(G.scene->r.cfra);
	vec[0]*= G.scene->r.framelen;
	
	vec[1]= G.v2d->cur.ymin;
	BIF_ThemeColor(TH_CFRAME);
	glLineWidth(2.0);
	
	glBegin(GL_LINE_STRIP);
	glVertex2fv(vec);
	vec[1]= G.v2d->cur.ymax;
	glVertex2fv(vec);
	glEnd();
	
	/* Draw dark green line if slow-parenting/time-offset is enabled */
	ob= (G.scene->basact) ? (G.scene->basact->object) : 0;
	if ((ob) && (ob->ipoflag & OB_OFFS_OB) && (give_timeoffset(ob)!=0.0)) {
		vec[0]-= give_timeoffset(ob); /* could avoid calling twice */
		
		BIF_ThemeColorShade(TH_CFRAME, -30);
		
		glBegin(GL_LINE_STRIP);
		glVertex2fv(vec);
		vec[1]= G.v2d->cur.ymin;
		glVertex2fv(vec);
		glEnd();
	}
	
	glLineWidth(1.0);
	
	/* Draw current frame number in a little box*/
	draw_cfra_number(vec[0]);
}

/********************************** Left-Hand Panel + Generics **************************** */

/* left hand part */
static void draw_channel_names(void) 
{
	ListBase act_data = {NULL, NULL};
	bActListElem *ale;
	int filter;
	void *data;
	short datatype;
	short ofsx = 0, ofsy = 0; 
	float x= 0.0f, y= 0.0f;
	
	/* determine what type of data we are operating on */
	data = get_action_context(&datatype);
	if (data == NULL) return;
	
	/* Clip to the scrollable area */
	if (curarea->winx>SCROLLB+10 && curarea->winy>SCROLLH+10) {
		if(G.v2d->scroll) {	
			ofsx= curarea->winrct.xmin;	
			ofsy= curarea->winrct.ymin;
			glViewport(ofsx,  ofsy+G.v2d->mask.ymin, NAMEWIDTH, 
					   (ofsy+G.v2d->mask.ymax) -
					   (ofsy+G.v2d->mask.ymin)); 
			glScissor(ofsx,	 ofsy+G.v2d->mask.ymin, NAMEWIDTH, 
					  (ofsy+G.v2d->mask.ymax) -
					  (ofsy+G.v2d->mask.ymin));
		}
	}
	
	/* prepare scaling for LHS panel */
	myortho2(0,	NAMEWIDTH, G.v2d->cur.ymin, G.v2d->cur.ymax);
	
	/* set default color back to black */
	glColor3ub(0x00, 0x00, 0x00);
	
	/* build list of channels to draw */
	filter= (ACTFILTER_FORDRAWING|ACTFILTER_VISIBLE|ACTFILTER_CHANNELS);
	actdata_filter(&act_data, filter, data, datatype);
	
	/* loop through channels, and set up drawing depending on their type  */
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	
	for (ale= act_data.first; ale; ale= ale->next) {
		const float yminc= y-CHANNELHEIGHT/2;
		const float ymaxc= y+CHANNELHEIGHT/2;
		
		/* check if visible */
		if ( IN_RANGE(yminc, G.v2d->cur.ymin, G.v2d->cur.ymax) ||
			 IN_RANGE(ymaxc, G.v2d->cur.ymin, G.v2d->cur.ymax) ) 
		{
			bActionGroup *grp = NULL;
			short indent= 0, offset= 0, sel= 0, group= 0;
			int expand= -1, protect = -1, special= -1, mute = -1;
			char name[64];
			
			/* determine what needs to be drawn */
			switch (ale->type) {
				case ACTTYPE_OBJECT: /* object */
				{
					Base *base= (Base *)ale->data;
					Object *ob= base->object;
					
					group= 4;
					indent= 0;
					
					/* icon depends on object-type */
					if (ob->type == OB_ARMATURE)
						special= ICON_ARMATURE;
					else	
						special= ICON_OBJECT;
						
					/* only show expand if there are any channels */
					if (EXPANDED_OBJC(ob))
						expand= ICON_TRIA_DOWN;
					else
						expand= ICON_TRIA_RIGHT;
					
					sel = SEL_OBJC(base);
					sprintf(name, ob->id.name+2);
				}
					break;
				case ACTTYPE_FILLACTD: /* action widget */
				{
					bAction *act= (bAction *)ale->data;
					
					group = 4;
					indent= 1;
					special= ICON_ACTION;
					
					if (EXPANDED_ACTC(act))
						expand= ICON_TRIA_DOWN;
					else
						expand= ICON_TRIA_RIGHT;
					
					sel = SEL_ACTC(act);
					sprintf(name, "Action");
				}
					break;
				case ACTTYPE_FILLIPOD: /* ipo (dopesheet) expand widget */
				{
					Object *ob = (Object *)ale->data;
					
					group = 4;
					indent = 1;
					special = ICON_IPO;
					
					if (FILTER_IPO_OBJC(ob))	
						expand = ICON_TRIA_DOWN;
					else
						expand = ICON_TRIA_RIGHT;
					
					//sel = SEL_OBJC(base);
					sprintf(name, "IPO Curves");
				}
					break;
				case ACTTYPE_FILLCOND: /* constraint channels (dopesheet) expand widget */
				{
					Object *ob = (Object *)ale->data;
					
					group = 4;
					indent = 1;
					special = ICON_CONSTRAINT;
					
					if (FILTER_CON_OBJC(ob))	
						expand = ICON_TRIA_DOWN;
					else
						expand = ICON_TRIA_RIGHT;
						
					//sel = SEL_OBJC(base);
					sprintf(name, "Constraints");
				}
					break;
				case ACTTYPE_FILLMATD: /* object materials (dopesheet) expand widget */
				{
					Object *ob = (Object *)ale->data;
					
					group = 4;
					indent = 1;
					special = ICON_MATERIAL;
					
					if (FILTER_MAT_OBJC(ob))
						expand = ICON_TRIA_DOWN;
					else
						expand = ICON_TRIA_RIGHT;
						
					sprintf(name, "Materials");
				}
					break;
				
				
				case ACTTYPE_DSMAT: /* single material (dopesheet) expand widget */
				{
					Material *ma = (Material *)ale->data;
					
					group = 0;
					indent = 0;
					special = ICON_MATERIAL;
					offset = 21;
					
					if (FILTER_MAT_OBJD(ma))
						expand = ICON_TRIA_DOWN;
					else
						expand = ICON_TRIA_RIGHT;
					
					sprintf(name, ma->id.name+2);
				}
					break;
				case ACTTYPE_DSLAM: /* lamp (dopesheet) expand widget */
				{
					Lamp *la = (Lamp *)ale->data;
					
					group = 4;
					indent = 1;
					special = ICON_LAMP;
					
					if (FILTER_LAM_OBJD(la))
						expand = ICON_TRIA_DOWN;
					else
						expand = ICON_TRIA_RIGHT;
					
					sprintf(name, la->id.name+2);
				}
					break;
				case ACTTYPE_DSCAM: /* camera (dopesheet) expand widget */
				{
					Camera *ca = (Camera *)ale->data;
					
					group = 4;
					indent = 1;
					special = ICON_CAMERA;
					
					if (FILTER_CAM_OBJD(ca))
						expand = ICON_TRIA_DOWN;
					else
						expand = ICON_TRIA_RIGHT;
					
					sprintf(name, ca->id.name+2);
				}
					break;
				case ACTTYPE_DSCUR: /* curve (dopesheet) expand widget */
				{
					Curve *cu = (Curve *)ale->data;
					
					group = 4;
					indent = 1;
					special = ICON_CURVE;
					
					if (FILTER_CUR_OBJD(cu))
						expand = ICON_TRIA_DOWN;
					else
						expand = ICON_TRIA_RIGHT;
					
					sprintf(name, cu->id.name+2);
				}
					break;
				case ACTTYPE_DSSKEY: /* shapekeys (dopesheet) expand widget */
				{
					Key *key= (Key *)ale->data;
					
					group = 4;
					indent = 1;
					special = ICON_EDIT;
					
					if (FILTER_SKE_OBJD(key))	
						expand = ICON_TRIA_DOWN;
					else
						expand = ICON_TRIA_RIGHT;
						
					//sel = SEL_OBJC(base);
					sprintf(name, "Shape Keys");
				}
					break;
					
				
				case ACTTYPE_GROUP: /* action group */
				{
					bActionGroup *agrp= (bActionGroup *)ale->data;
					
					group= 2;
					indent= 0;
					special= -1;
					
					offset= (ale->id) ? 21 : 0;
					
					/* only show expand if there are any channels */
					if (agrp->channels.first) {
						if (EXPANDED_AGRP(agrp))
							expand = ICON_TRIA_DOWN;
						else
							expand = ICON_TRIA_RIGHT;
					}
					
					if (EDITABLE_AGRP(agrp))
						protect = ICON_UNLOCKED;
					else
						protect = ICON_LOCKED;
						
					sel = SEL_AGRP(agrp);
					sprintf(name, agrp->name);
				}
					break;
				case ACTTYPE_ACHAN: /* action channel */
				{
					bActionChannel *achan= (bActionChannel *)ale->data;
					
					group= (ale->grp) ? 1 : 0;
					grp= ale->grp;
					
					indent = 0;
					special = -1;
					
					offset= (ale->id) ? 21 : 0;
					
					if (EXPANDED_ACHAN(achan))
						expand = ICON_TRIA_DOWN;
					else
						expand = ICON_TRIA_RIGHT;
						
					if (EDITABLE_ACHAN(achan))
						protect = ICON_UNLOCKED;
					else
						protect = ICON_LOCKED;
						
					if (achan->ipo) {
						if (achan->ipo->muteipo)
							mute = ICON_MUTE_IPO_ON;
						else
							mute = ICON_MUTE_IPO_OFF;
					}
					
					sel = SEL_ACHAN(achan);
					sprintf(name, achan->name);
				}
					break;
				case ACTTYPE_CONCHAN: /* constraint channel */
				{
					bConstraintChannel *conchan = (bConstraintChannel *)ale->data;
					
					group= (ale->grp) ? 1 : 0;
					grp= ale->grp;
					
					if (ale->id) {
						if (ale->ownertype == ACTTYPE_ACHAN) {
							/* for constraint channels under Action in Dopesheet */
							indent= 2;
							offset= 21;
						}
						else {
							/* for constraint channels under Object in Dopesheet */
							indent= 2;
							offset = 0;
						}
					}
					else {
						/* for normal constraint channels in Action Editor */
						indent= 2;
						offset= 0;
					}
					
					if (EDITABLE_CONCHAN(conchan))
						protect = ICON_UNLOCKED;
					else
						protect = ICON_LOCKED;
						
					if (conchan->ipo) {
						if (conchan->ipo->muteipo)
							mute = ICON_MUTE_IPO_ON;
						else
							mute = ICON_MUTE_IPO_OFF;
					}
					
					sel = SEL_CONCHAN(conchan);
					sprintf(name, conchan->name);
				}
					break;
				case ACTTYPE_ICU: /* ipo-curve channel */
				{
					IpoCurve *icu = (IpoCurve *)ale->data;
					
					indent = 2;
					protect = -1; // for now, until this can be supported by others
					
					group= (ale->grp) ? 1 : 0;
					grp= ale->grp;
					
					//offset= ((ale->id) && (GS(ale->id->name) == ID_MA)) ? 21 : 0;
					if (ale->id) {
						if ((GS(ale->id->name)==ID_MA) || (ale->ownertype == ACTTYPE_ACHAN))
							offset= 21;
						else
							offset= 0;
					}
					else
						offset= 0;
					
					
					if (icu->flag & IPO_MUTE)
						mute = ICON_MUTE_IPO_ON;
					else	
						mute = ICON_MUTE_IPO_OFF;
					
					sel = SEL_ICU(icu);
					if (G.saction->pin)
						sprintf(name, getname_ipocurve(icu, NULL));
					else
						sprintf(name, getname_ipocurve(icu, OBACT));
				}
					break;
				case ACTTYPE_FILLIPO: /* ipo expand widget */
				{
					bActionChannel *achan = (bActionChannel *)ale->data;
					
					indent = 1;
					special = geticon_ipo_blocktype(achan->ipo->blocktype);
					
					group= (ale->grp) ? 1 : 0;
					grp= ale->grp;
					
					offset= (ale->id) ? 21 : 0;
					
					if (FILTER_IPO_ACHAN(achan))	
						expand = ICON_TRIA_DOWN;
					else
						expand = ICON_TRIA_RIGHT;
					
					sel = SEL_ACHAN(achan);
					sprintf(name, "IPO Curves");
				}
					break;
				case ACTTYPE_FILLCON: /* constraint expand widget */
				{
					bActionChannel *achan = (bActionChannel *)ale->data;
					
					indent = 1;
					special = ICON_CONSTRAINT;
					
					group= (ale->grp) ? 1 : 0;
					grp= ale->grp;
					
					offset= (ale->id) ? 21 : 0;
					
					if (FILTER_CON_ACHAN(achan))	
						expand = ICON_TRIA_DOWN;
					else
						expand = ICON_TRIA_RIGHT;
						
					sel = SEL_ACHAN(achan);
					sprintf(name, "Constraint");
				}
					break;
				
				
				case ACTTYPE_SHAPEKEY: /* shapekey channel */
				{
					KeyBlock *kb = (KeyBlock *)ale->data;
					
					indent = 0;
					special = -1;
					
					offset= (ale->id) ? 21 : 0;
					
					if (kb->name[0] == '\0')
						sprintf(name, "Key %d", ale->index);
					else
						sprintf(name, kb->name);
				}
					break;
				
				case ACTTYPE_GPDATABLOCK: /* gpencil datablock */
				{
					bGPdata *gpd = (bGPdata *)ale->data;
					ScrArea *sa = (ScrArea *)ale->owner;
					
					indent = 0;
					group= 3;
					
					/* only show expand if there are any channels */
					if (gpd->layers.first) {
						if (gpd->flag & GP_DATA_EXPAND)
							expand = ICON_TRIA_DOWN;
						else
							expand = ICON_TRIA_RIGHT;
					}
					
					switch (sa->spacetype) {
						case SPACE_VIEW3D:
						{
							/* this shouldn't cause any overflow... */
							sprintf(name, "3DView[%02d]:%s", sa->win, view3d_get_name(sa->spacedata.first));
							special= ICON_VIEW3D;
						}
							break;
						case SPACE_NODE:
						{
							SpaceNode *snode= sa->spacedata.first;
							char treetype[12];
							
							if (snode->treetype == 1)
								sprintf(treetype, "Composite");
							else
								sprintf(treetype, "Material");
							sprintf(name, "Nodes[%02d]:%s", sa->win, treetype);
							
							special= ICON_NODE;
						}
							break;
						case SPACE_SEQ:
						{
							SpaceSeq *sseq= sa->spacedata.first;
							char imgpreview[10];
							
							switch (sseq->mainb) {
								case 1: 	sprintf(imgpreview, "Image..."); 	break;
								case 2: 	sprintf(imgpreview, "Luma..."); 	break;
								case 3: 	sprintf(imgpreview, "Chroma...");	break;
								case 4: 	sprintf(imgpreview, "Histogram");	break;
								
								default:	sprintf(imgpreview, "Sequence");	break;
							}
							sprintf(name, "Sequencer[%02d]:%s", sa->win, imgpreview);
							
							special= ICON_SEQUENCE;
						}
							break;
						case SPACE_IMAGE:
						{
							SpaceImage *sima= sa->spacedata.first;
							
							if (sima->image)
								sprintf(name, "Image[%02d]:%s", sa->win, sima->image->id.name+2);
							else
								sprintf(name, "Image[%02d]:<None>", sa->win);
								
							special= ICON_IMAGE_COL;
						}
							break;
						
						default:
						{
							sprintf(name, "[%02d]<Unknown GP-Data Source>", sa->win);
							special= -1;
						}
							break;
					}
				}
					break;
				case ACTTYPE_GPLAYER: /* gpencil layer */
				{
					bGPDlayer *gpl = (bGPDlayer *)ale->data;
					
					indent = 0;
					special = -1;
					expand = -1;
					group = 1;
					
					if (EDITABLE_GPL(gpl))
						protect = ICON_UNLOCKED;
					else
						protect = ICON_LOCKED;
						
					if (gpl->flag & GP_LAYER_HIDE)
						mute = ICON_MUTE_IPO_ON;
					else
						mute = ICON_MUTE_IPO_OFF;
					
					sel = SEL_GPL(gpl);
					BLI_snprintf(name, 32, gpl->info);
				}
					break;
			}	

			/* now, start drawing based on this information */
			/* draw backing strip behind channel name */
			if (group == 4) {
				/* only used in dopesheet... */
				if (ale->type == ACTTYPE_OBJECT) {
					/* object channel - darker */
					BIF_ThemeColor(TH_DOPESHEET_CHANNELOB);
					uiSetRoundBox((expand == ICON_TRIA_DOWN)? (1):(1|8));
					gl_round_box(GL_POLYGON, x+offset,  yminc, (float)NAMEWIDTH, ymaxc, 8);
				}
				else {
					/* sub-object folders - lighter */
					BIF_ThemeColor(TH_DOPESHEET_CHANNELSUBOB);
					
					offset += 7 * indent;
					//glRectf(x+offset,  yminc, (float)NAMEWIDTH, ymaxc);
					glBegin(GL_QUADS);
						glVertex2f(x+offset, yminc);
						glVertex2f(x+offset, ymaxc);
						glVertex2f((float)NAMEWIDTH, ymaxc);
						glVertex2f((float)NAMEWIDTH, yminc);
					glEnd();
					
					/* clear group value, otherwise we cause errors... */
					group = 0;
				}
			}
			else if (group == 3) {
				/* only for gp-data channels */
				BIF_ThemeColorShade(TH_GROUP, 20);
				uiSetRoundBox((expand == ICON_TRIA_DOWN)? (1):(1|8));
				gl_round_box(GL_POLYGON, x+offset,  yminc, (float)NAMEWIDTH, ymaxc, 8);
			}
			else if (group == 2) {
				/* only for action group channels */
				if (ale->flag & AGRP_ACTIVE)
					BIF_ThemeColorShade(TH_GROUP_ACTIVE, 10);
				else
					BIF_ThemeColorShade(TH_GROUP, 20);
				uiSetRoundBox((expand == ICON_TRIA_DOWN)? (1):(1|8));
				gl_round_box(GL_POLYGON, x+offset,  yminc, (float)NAMEWIDTH, ymaxc, 8);
			}
			else {
				/* for normal channels 
				 *	- use 3 shades of color group/standard color for 3 indention level
				 *	- only use group colors if allowed to, and if actually feasible
				 */
				if ( !(G.saction->flag & SACTION_NODRAWGCOLORS) && 
					 (grp) && (grp->customCol) ) 
				{
					char cp[3];
					
					if (indent == 2) {
						VECCOPY(cp, grp->cs.solid);
					}
					else if (indent == 1) {
						VECCOPY(cp, grp->cs.select);
					}
					else {
						VECCOPY(cp, grp->cs.active);
					}
					
					glColor3ub(cp[0], cp[1], cp[2]);
				}
				else
					BIF_ThemeColorShade(TH_HEADER, ((indent==0)?20: (indent==1)?-20: -40));
				
				indent += group;
				offset += 7 * indent;
				//glRectf(x+offset,  yminc, (float)NAMEWIDTH, ymaxc);
				glBegin(GL_QUADS);
					glVertex2f(x+offset, yminc);
					glVertex2f(x+offset, ymaxc);
					glVertex2f((float)NAMEWIDTH, ymaxc);
					glVertex2f((float)NAMEWIDTH, yminc);
				glEnd();
			}
			
			/* draw expand/collapse triangle */
			if (expand > 0) {
				BIF_icon_draw(x+offset, yminc, expand);
				offset += 17;
			}
			
			/* draw special icon indicating certain data-types */
			if (special > -1) {
				if (ELEM(group, 3, 4)) {
					/* for gpdatablock channels */
					BIF_icon_draw(x+offset, yminc, special);
					offset += 17;
				}
				else {
					/* for ipo/constraint channels */
					BIF_icon_draw(x+offset, yminc, special);
					offset += 17;
				}
			}
				
			/* draw name */
			if (sel)
				BIF_ThemeColor(TH_TEXT_HI);
			else
				BIF_ThemeColor(TH_TEXT);
			offset += 3;
			glRasterPos2f(x+offset, y-4);
			BMF_DrawString(G.font, name);
			
			/* reset offset - for RHS of panel */
			offset = 0;
			
			/* draw protect 'lock' */
			if (protect > -1) {
				offset = 16;
				BIF_icon_draw((float)NAMEWIDTH-offset, yminc, protect);
			}
			
			/* draw mute 'eye' */
			if (mute > -1) {
				offset += 16;
				BIF_icon_draw((float)(NAMEWIDTH-offset), yminc, mute);
			}
		}
		
		/* adjust y-position for next one */
		y-=CHANNELHEIGHT+CHANNELSKIP;
	}
	
	/* free tempolary channels */
	BLI_freelistN(&act_data);
	
	/* re-adjust view matrices for correct scaling */
    myortho2(0,	NAMEWIDTH, 0, (float)(ofsy+G.v2d->mask.ymax) - (ofsy+G.v2d->mask.ymin));	//	Scaling
}

/* sets or clears hidden flags - for actionchannels only */
void check_action_context(SpaceAction *saction)
{
	bActionChannel *achan;
	
	if (saction->mode != SACTCONT_ACTION) return;
	if (saction->action == NULL) return;
	
	for (achan=saction->action->chanbase.first; achan; achan=achan->next)
		achan->flag &= ~ACHAN_HIDDEN;
	
	if ((saction->pin==0) && ((saction->flag & SACTION_NOHIDE)==0) && (OBACT)) {
		Object *ob= OBACT;
		bPoseChannel *pchan;
		bArmature *arm= ob->data;
		
		for (achan=saction->action->chanbase.first; achan; achan=achan->next) {
			pchan= get_pose_channel(ob->pose, achan->name);
			if (pchan && pchan->bone) {
				if ((pchan->bone->layer & arm->layer)==0)
					achan->flag |= ACHAN_HIDDEN;
				else if (pchan->bone->flag & BONE_HIDDEN_P)
					achan->flag |= ACHAN_HIDDEN;
			}
		}
	}
}

static ActKeysInc *init_aki_data(void *data, short datatype, bActListElem *ale)
{
	static ActKeysInc aki;
	
	/* no need to set settings if wrong context */
	if ((data == NULL) || ELEM(datatype, ACTCONT_ACTION, ACTCONT_DOPESHEET)==0)
		return NULL;
	
	/* if strip is mapped, store settings */
	if (NLA_CHAN_SCALED(ale)) {
		/* NLA_CHAN_SCALED checks the standard scaling check (for Action Mode), 
		 * as well as making sure that channel has Object ID-owner 
		 */
		if (datatype == ACTCONT_DOPESHEET)
			aki.ob= (Object *)ale->id; // is more filtering on this needed?
		else if (datatype == ACTCONT_ACTION)
			aki.ob= OBACT;
	}
	else {
		aki.ob= NULL;
	}
	
	if (datatype == ACTCONT_DOPESHEET)
		aki.ads= (bDopeSheet *)data;
	else
		aki.ads= NULL;
	aki.actmode= datatype;
	
	/* set start/end frames to use for time-based keyframe culling hacks... */
	// FIXME: this needs to be a bit better defined...
	aki.start= G.v2d->cur.xmin - 10;
	aki.end= G.v2d->cur.xmax + 10;
	
	return &aki;
}

static void draw_channel_strips(void)
{
	ListBase act_data = {NULL, NULL};
	bActListElem *ale;
	int filter;
	void *data;
	short datatype;
	
	rcti scr_rct;
	gla2DDrawInfo *di;
	float y, sta, end;
	int act_start, act_end, dummy;
	char col1[3], col2[3];
	char col1a[3], col2a[3];
	char col1b[3], col2b[3];
	
	BIF_GetThemeColor3ubv(TH_SHADE2, col2);
	BIF_GetThemeColor3ubv(TH_HILITE, col1);
	BIF_GetThemeColor3ubv(TH_GROUP, col2a);
	BIF_GetThemeColor3ubv(TH_GROUP_ACTIVE, col1a);
	
	BIF_GetThemeColor3ubv(TH_DOPESHEET_CHANNELOB, col1b);
	BIF_GetThemeColor3ubv(TH_DOPESHEET_CHANNELSUBOB, col2b);

	/* get editor data */
	data= get_action_context(&datatype);
	if (data == NULL) return;

	scr_rct.xmin= G.saction->area->winrct.xmin + G.saction->v2d.mask.xmin;
	scr_rct.ymin= G.saction->area->winrct.ymin + G.saction->v2d.mask.ymin;
	scr_rct.xmax= G.saction->area->winrct.xmin + G.saction->v2d.hor.xmax;
	scr_rct.ymax= G.saction->area->winrct.ymin + G.saction->v2d.mask.ymax; 
	di= glaBegin2DDraw(&scr_rct, &G.v2d->cur);

	/* if in NLA there's a strip active, map the view */
	if (datatype == ACTCONT_ACTION) {
		if (NLA_ACTION_SCALED)
			map_active_strip(di, OBACT, 0);
		
		/* start and end of action itself */
		calc_action_range(data, &sta, &end, 0);
		gla2DDrawTranslatePt(di, sta, 0.0f, &act_start, &dummy);
		gla2DDrawTranslatePt(di, end, 0.0f, &act_end, &dummy);
		
		if (NLA_ACTION_SCALED)
			map_active_strip(di, OBACT, 1);
	}
	
	/* build list of channels to draw */
	filter= (ACTFILTER_FORDRAWING|ACTFILTER_VISIBLE|ACTFILTER_CHANNELS);
	actdata_filter(&act_data, filter, data, datatype);
	
	/* first backdrop strips */
	y = 0.0;
	glEnable(GL_BLEND);
	for (ale= act_data.first; ale; ale= ale->next) {
		int frame1_x, channel_y, sel=0;
		
		/* determine if any need to draw channel */
		if (ale->datatype != ALE_NONE) {
			/* determine if channel is selected */
			switch (ale->type) {
				case ACTTYPE_OBJECT:
				{
					Base *base= (Base *)ale->data;
					sel = SEL_OBJC(base);
				}
					break;
				case ACTTYPE_GROUP:
				{
					bActionGroup *agrp = (bActionGroup *)ale->data;
					sel = SEL_AGRP(agrp);
				}
					break;
				case ACTTYPE_ACHAN:
				{
					bActionChannel *achan = (bActionChannel *)ale->data;
					sel = SEL_ACHAN(achan);
				}
					break;
				case ACTTYPE_CONCHAN:
				{
					bConstraintChannel *conchan = (bConstraintChannel *)ale->data;
					sel = SEL_CONCHAN(conchan);
				}
					break;
				case ACTTYPE_ICU:
				{
					IpoCurve *icu = (IpoCurve *)ale->data;
					sel = SEL_ICU(icu);
				}
					break;
				case ACTTYPE_GPLAYER:
				{
					bGPDlayer *gpl = (bGPDlayer *)ale->data;
					sel = SEL_GPL(gpl);
				}
					break;
			}
			
			if (ELEM(datatype, ACTCONT_ACTION, ACTCONT_DOPESHEET)) {
				gla2DDrawTranslatePt(di, G.v2d->cur.xmin, y, &frame1_x, &channel_y);
				
				switch (ale->type) {
					case ACTTYPE_OBJECT:
					{
						if (sel) glColor4ub(col1b[0], col1b[1], col1b[2], 0x45); 
						else glColor4ub(col1b[0], col1b[1], col1b[2], 0x22); 
					}
						break;
						
					case ACTTYPE_FILLIPOD:
					case ACTTYPE_FILLACTD:
					case ACTTYPE_FILLCOND:
					case ACTTYPE_DSSKEY:
					{
						if (sel) glColor4ub(col2b[0], col2b[1], col2b[2], 0x45); 
						else glColor4ub(col2b[0], col2b[1], col2b[2], 0x22); 
					}
						break;
					
					case ALE_GROUP:
					{
						if (sel) glColor4ub(col1a[0], col1a[1], col1a[2], 0x22);
						else glColor4ub(col2a[0], col2a[1], col2a[2], 0x22);
					}
						break;
					
					default:
					{
						if (sel) glColor4ub(col1[0], col1[1], col1[2], 0x22);
						else glColor4ub(col2[0], col2[1], col2[2], 0x22);
					}
						break;
				}
				
				/* draw region twice: firstly backdrop, then the current range */
				glRectf((float)frame1_x,  (float)channel_y-CHANNELHEIGHT/2,  (float)G.v2d->hor.xmax,  (float)channel_y+CHANNELHEIGHT/2);
				
				if (datatype == ACTCONT_ACTION)
					glRectf((float)act_start,  (float)channel_y-CHANNELHEIGHT/2,  (float)act_end,  (float)channel_y+CHANNELHEIGHT/2);
			}
			else if (datatype == ACTCONT_SHAPEKEY) {
				gla2DDrawTranslatePt(di, 1, y, &frame1_x, &channel_y);
				
				/* all frames that have a frame number less than one
				 * get a desaturated orange background
				 */
				glColor4ub(col2[0], col2[1], col2[2], 0x22);
				glRectf(0.0f, (float)channel_y-CHANNELHEIGHT/2, (float)frame1_x, (float)channel_y+CHANNELHEIGHT/2);
				
				/* frames one and higher get a saturated orange background */
				glColor4ub(col2[0], col2[1], col2[2], 0x44);
				glRectf((float)frame1_x, (float)channel_y-CHANNELHEIGHT/2, (float)G.v2d->hor.xmax,  (float)channel_y+CHANNELHEIGHT/2.0f);
			}
			else if (datatype == ACTCONT_GPENCIL) {
				gla2DDrawTranslatePt(di, G.v2d->cur.xmin, y, &frame1_x, &channel_y);
				
				/* frames less than one get less saturated background */
				if (sel) glColor4ub(col1[0], col1[1], col1[2], 0x22);
				else glColor4ub(col2[0], col2[1], col2[2], 0x22);
				glRectf(0.0f, (float)channel_y-CHANNELHEIGHT/2, (float)frame1_x, (float)channel_y+CHANNELHEIGHT/2);
				
				/* frames one and higher get a saturated background */
				if (sel) glColor4ub(col1[0], col1[1], col1[2], 0x44);
				else glColor4ub(col2[0], col2[1], col2[2], 0x44);
				glRectf((float)frame1_x, (float)channel_y-CHANNELHEIGHT/2, (float)G.v2d->hor.xmax,  (float)channel_y+CHANNELHEIGHT/2);
			}
		}
		
		/*	Increment the step */
		y-=CHANNELHEIGHT+CHANNELSKIP;
	}		
	glDisable(GL_BLEND);
	
	/* Draw keyframes 
	 *	1) Only channels that are visible in the Action Editor get drawn/evaluated.
	 *	   This is to try to optimise this for heavier data sets
	 *	2) Keyframes which are out of view horizontally are disregarded 
	 */
	y = 0.0f;
	for (ale= act_data.first; ale; ale= ale->next) {
		const float yminc= y-CHANNELHEIGHT/2;
		const float ymaxc= y+CHANNELHEIGHT/2;
		
		/* check if visible */
		if ( IN_RANGE(yminc, G.v2d->cur.ymin, G.v2d->cur.ymax) ||
			 IN_RANGE(ymaxc, G.v2d->cur.ymin, G.v2d->cur.ymax) ) 
		{
			/* check if anything to show for this channel */
			if (ale->datatype != ALE_NONE) {
				ActKeysInc *aki= init_aki_data(data, datatype, ale); 
				
				if (NLA_CHAN_SCALED(ale)) {
					Object *nob= (NLA_ACTION_SCALED) ? OBACT : (Object *)ale->id;
					map_active_strip(di, nob, 0);
				}
				
				/* draw 'keyframes' for each specific datatype */
				switch (ale->datatype) {
					case ALE_OB:
						draw_object_channel(di, aki, ale->key_data, y);
						break;
					case ALE_ACT:
						draw_action_channel(di, aki, ale->key_data, y);
						break;
					case ALE_GROUP:
						draw_agroup_channel(di, aki, ale->data, y);
						break;
					case ALE_IPO:
						draw_ipo_channel(di, aki, ale->key_data, y);
						break;
					case ALE_ICU:
						draw_icu_channel(di, aki, ale->key_data, y);
						break;
					case ALE_GPFRAME:
						draw_gpl_channel(di, aki, ale->data, y);
						break;
				}
				
				if (NLA_CHAN_SCALED(ale)) {
					Object *nob= (NLA_ACTION_SCALED) ? OBACT : (Object *)ale->id;
					map_active_strip(di, nob, 1);
				}
			}
		}
		
		y-=CHANNELHEIGHT+CHANNELSKIP;
	}
	
	/* free tempolary channels used for drawing */
	BLI_freelistN(&act_data);

	/* black line marking 'current frame' for Time-Slide transform mode */
	if (G.saction->flag & SACTION_MOVING) {
		int frame1_x, channel_y;
		
		gla2DDrawTranslatePt(di, G.saction->timeslide, 0, &frame1_x, &channel_y);
		cpack(0x0);
		
		glBegin(GL_LINES);
		glVertex2f((float)frame1_x, (float)G.v2d->mask.ymin - 100);
		glVertex2f((float)frame1_x, (float)G.v2d->mask.ymax);
		glEnd();
	}
	
	glaEnd2DDraw(di);
}

/* ********* action panel *********** */


void do_actionbuts(unsigned short event)
{
	switch(event) {
		/* general */
	case REDRAWVIEW3D:
		allqueue(REDRAWVIEW3D, 0);
		break;
	case B_REDR:
		allqueue(REDRAWACTION, 0);
		break;
		
		/* action-groups */
	case B_ACTCUSTCOLORS:	/* only when of the color wells is edited */
	{
		bActionGroup *agrp= get_active_actiongroup(G.saction->action);
		
		if (agrp)
			agrp->customCol= -1;
			
		allqueue(REDRAWACTION, 0);
	}
		break;
	case B_ACTCOLSSELECTOR: /* sync color set after using selector */
	{
		bActionGroup *agrp= get_active_actiongroup(G.saction->action);
		
		if (agrp) 
			actionbone_group_copycolors(agrp, 1);
			
		allqueue(REDRAWACTION, 0);
	}
		break;
	case B_ACTGRP_SELALL: /* select all grouped channels */
	{
		bAction *act= G.saction->action;
		bActionGroup *agrp= get_active_actiongroup(act);
		
		/* select all in group, then reselect/activate group as the previous operation clears that */
		select_action_group_channels(act, agrp);
		agrp->flag |= (AGRP_ACTIVE|AGRP_SELECTED);
		
		allqueue(REDRAWACTION, 0);
	}
		break;
	case B_ACTGRP_ADDTOSELF: /* add all selected action channels to self */
		action_groups_group(0);
		break;
	case B_ACTGRP_UNGROUP: /* remove channels from active group */
		// FIXME: todo...
		printf("FIXME: remove achans from active Action-Group not implemented yet! \n");
		break;
	
	}
}

// currently not used...
static void action_panel_properties(short cntrl)	// ACTION_HANDLER_PROPERTIES
{
	uiBlock *block;
	void *data;
	short datatype;
	
	block= uiNewBlock(&curarea->uiblocks, "action_panel_properties", UI_EMBOSS, UI_HELV, curarea->win);
	uiPanelControl(UI_PNL_SOLID | UI_PNL_CLOSE | cntrl);
	uiSetPanelHandler(ACTION_HANDLER_PROPERTIES);  // for close and esc
	
	/* get datatype */
	data= get_action_context(&datatype);
	//if (data == NULL) return;
	
	if (uiNewPanel(curarea, block, "Active Channel Properties", "Action", 10, 230, 318, 204)==0) 
		return;
	
	/* currently, only show data for actions */
	if (datatype == ACTCONT_ACTION) {
		bActionGroup *agrp= get_active_actiongroup(data);
		//bActionChannel *achan= get_hilighted_action_channel(data);
		char *menustr;
		
		/* only for action-groups */
		if (agrp) {
			/* general stuff */
			uiDefBut(block, LABEL, 1, "Action Group:",					10, 180, 150, 19, NULL, 0.0, 0.0, 0, 0, "");
			
			uiDefBut(block, TEX, B_REDR, "Name: ",	10,160,150,20, agrp->name, 0.0, 31.0, 0, 0, "");
			uiBlockBeginAlign(block);
				uiDefButBitI(block, TOG, AGRP_EXPANDED, B_REDR, "Expanded", 170, 160, 75, 20, &agrp->flag, 0, 0, 0, 0, "Action Group is expanded");
				uiDefButBitI(block, TOG, AGRP_PROTECTED, B_REDR, "Protected", 245, 160, 75, 20, &agrp->flag, 0, 0, 0, 0, "Action Group is protected");
			uiBlockEndAlign(block);
			
			/* color stuff */
			uiDefBut(block, LABEL, 1, "Group Colors:",	10, 107, 150, 19, NULL, 0.0, 0.0, 0, 0, "");
			uiBlockBeginAlign(block);
				menustr= BIF_ThemeColorSetsPup(1);
				uiDefButI(block, MENU,B_ACTCOLSSELECTOR, menustr, 10,85,150,19, &agrp->customCol, -1, 20, 0.0, 0.0, "Index of set of Custom Colors to shade Group's bones with. 0 = Use Default Color Scheme, -1 = Use Custom Color Scheme");						
				MEM_freeN(menustr);
				
				/* show color-selection/preview */
				if (agrp->customCol) {
					/* do color copying/init (to stay up to date) */
					actionbone_group_copycolors(agrp, 1);
					
					/* color changing */
					uiDefButC(block, COL, B_ACTCUSTCOLORS, "",		10, 65, 50, 19, agrp->cs.active, 0, 0, 0, 0, "Color to use for 'top-level' channels");
					uiDefButC(block, COL, B_ACTCUSTCOLORS, "",		60, 65, 50, 19, agrp->cs.select, 0, 0, 0, 0, "Color to use for '2nd-level' channels");
					uiDefButC(block, COL, B_ACTCUSTCOLORS, "",		110, 65, 50, 19, agrp->cs.solid, 0, 0, 0, 0, "Color to use for '3rd-level' channels");
				}
			uiBlockEndAlign(block);
			
			/* commands for active group */
			uiDefBut(block, BUT, B_ACTGRP_SELALL, "Select Grouped",	170,85,150,20, 0, 21, 0, 0, 0, "Select all action-channels belonging to this group (same as doing Ctrl-Shift-LMB)");
			
			uiBlockBeginAlign(block);
				uiDefBut(block, BUT, B_ACTGRP_ADDTOSELF, "Add to Group",	170,60,150,20, 0, 21, 0, 0, 0, "Add selected action-channels to this group");
				uiDefBut(block, BUT, B_ACTGRP_UNGROUP, "Un-Group",	170,40,150,20, 0, 21, 0, 0, 0, "Remove selected action-channels from this group (unimplemented)");
			uiBlockEndAlign(block);
		}
	}
	else {
		/* Currently, there isn't anything to display for these types ... */
	}
}

static void action_blockhandlers(ScrArea *sa)
{
	SpaceAction *sact= sa->spacedata.first;
	short a;
	
	for (a=0; a<SPACE_MAXHANDLER; a+=2) {
		switch(sact->blockhandler[a]) {
			case ACTION_HANDLER_PROPERTIES:
				action_panel_properties(sact->blockhandler[a+1]);
				break;
		}
		
		/* clear action value for event */
		sact->blockhandler[a+1]= 0;
	}
	
	uiDrawBlocksPanels(sa, 0);
}

/* ************************* Action Editor Space ***************************** */

void drawactionspace(ScrArea *sa, void *spacedata)
{
	bAction *act = NULL;
	Key *key = NULL;
	void *data;
	short datatype;
	
	short ofsx = 0, ofsy = 0;
	float col[3];

	/* this is unlikely to occur, but it may */
	if (G.saction == NULL)
		return;

	/* warning: blocks need to be freed each time, handlers dont remove  */
	uiFreeBlocksWin(&sa->uiblocks, sa->win);
	
	/* get data */
	data = get_action_context(&datatype);
	switch (datatype) {
		case ACTCONT_ACTION:
			act = data;
			break;
		case ACTCONT_SHAPEKEY:
			key = data;
			break;
		case ACTCONT_GPENCIL:
			/* currently, 'data' value for grease-pencil is G.curscreen! */
			break;
	}
	
	/* Lets make sure the width of the left hand of the screen
	 * is set to an appropriate value based on whether sliders
	 * are showing of not
	 */
	if ((data) && (G.saction->flag & SACTION_SLIDERS)) 
		ACTWIDTH = NAMEWIDTH + SLIDERWIDTH;
	else 
		ACTWIDTH = NAMEWIDTH;

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	calc_scrollrcts(sa, G.v2d, curarea->winx, curarea->winy);

	/* background color for entire window (used in lefthand part though) */
	BIF_GetThemeColor3fv(TH_HEADER, col);
	glClearColor(col[0], col[1], col[2], 0.0); 
	glClear(GL_COLOR_BUFFER_BIT);
	
	if (curarea->winx>SCROLLB+10 && curarea->winy>SCROLLH+10) {
		if(G.v2d->scroll) {	
			ofsx= curarea->winrct.xmin;	
			ofsy= curarea->winrct.ymin;
			glViewport(ofsx+G.v2d->mask.xmin,  ofsy+G.v2d->mask.ymin, 
					   ( ofsx+G.v2d->mask.xmax-1)-(ofsx+G.v2d->mask.xmin)+1, 
					   ( ofsy+G.v2d->mask.ymax-1)-( ofsy+G.v2d->mask.ymin)+1); 
			glScissor(ofsx+G.v2d->mask.xmin,  ofsy+G.v2d->mask.ymin, 
					  ( ofsx+G.v2d->mask.xmax-1)-(ofsx+G.v2d->mask.xmin)+1, 
					  ( ofsy+G.v2d->mask.ymax-1)-( ofsy+G.v2d->mask.ymin)+1);
		}
	}

	BIF_GetThemeColor3fv(TH_BACK, col);
	glClearColor(col[0], col[1], col[2], 0.0);
	glClear(GL_COLOR_BUFFER_BIT);

	myortho2(G.v2d->cur.xmin, G.v2d->cur.xmax, G.v2d->cur.ymin, G.v2d->cur.ymax);
	bwin_clear_viewmat(sa->win);	/* clear buttons view */
	glLoadIdentity();

	/*	Draw backdrop */
	calc_ipogrid();	
	draw_ipogrid();

	check_action_context(G.saction);
	
	/* Draw channel strips */
	draw_channel_strips();
	
	/* reset matrices for stuff to be drawn on top of keys*/
	glViewport(ofsx+G.v2d->mask.xmin,  
             ofsy+G.v2d->mask.ymin, 
             ( ofsx+G.v2d->mask.xmax-1)-(ofsx+G.v2d->mask.xmin)+1, 
             ( ofsy+G.v2d->mask.ymax-1)-( ofsy+G.v2d->mask.ymin)+1); 
	glScissor(ofsx+G.v2d->mask.xmin,  
            ofsy+G.v2d->mask.ymin, 
            ( ofsx+G.v2d->mask.xmax-1)-(ofsx+G.v2d->mask.xmin)+1, 
            ( ofsy+G.v2d->mask.ymax-1)-( ofsy+G.v2d->mask.ymin)+1);
	myortho2(G.v2d->cur.xmin, G.v2d->cur.xmax,  G.v2d->cur.ymin, G.v2d->cur.ymax);
	
	/* Draw current frame */
	draw_cfra_action();
	
	/* Draw markers (local behind scene ones, as local obscure scene markers) */
	if (act) 
		draw_markers_timespace(&act->markers, DRAW_MARKERS_LOCAL);
	draw_markers_timespace(SCE_MARKERS, 0);
	
	/* Draw 'curtains' for preview */
	draw_anim_preview_timespace();

	/* Draw scroll */
	mywinset(curarea->win);	// reset scissor too
	if (curarea->winx>SCROLLB+10 && curarea->winy>SCROLLH+10) {
		myortho2(-0.375f, curarea->winx-0.375f, -0.375f, curarea->winy-0.375f);
		if (G.v2d->scroll) drawscroll(0);
	}

	/* Draw Left-Hand Panel if enough space in window */
	if (G.v2d->mask.xmin != 0) {
		/* Draw channel names */
		draw_channel_names();
		
		if (sa->winx > 50 + NAMEWIDTH + SLIDERWIDTH) {
			if (ELEM(G.saction->mode, SACTCONT_ACTION, SACTCONT_DOPESHEET)) {
				/* if there is an action, draw sliders for its
				 * ipo-curve channels in the action window
				 */
				action_icu_buts(G.saction);
			}
			else if (key) {
				/* if there is a mesh with rvk's selected,
				 * then draw the key frames in the action window
				 */
				meshactionbuts(G.saction, OBACT, key);
			}
		}
	}
	
	mywinset(curarea->win);	// reset scissor too
	myortho2(-0.375f, curarea->winx-0.375f, -0.375f, curarea->winy-0.375f);
	draw_area_emboss(sa);

	/* it is important to end a view in a transform compatible with buttons */
	bwin_scalematrix(sa->win, G.saction->blockscale, G.saction->blockscale, G.saction->blockscale);
	action_blockhandlers(sa);

	curarea->win_swap= WIN_BACK_OK;
}

/* *************************** Keyframe Drawing *************************** */

static void add_bezt_to_keycolumnslist(ListBase *keys, BezTriple *bezt)
{
	/* The equivilant of add_to_cfra_elem except this version 
	 * makes ActKeyColumns - one of the two datatypes required
	 * for action editor drawing.
	 */
	ActKeyColumn *ak, *akn;
	
	if (ELEM(NULL, keys, bezt)) return;
	
	/* try to any existing key to replace, or where to insert after */
	for (ak= keys->last; ak; ak= ak->prev) {
		/* do because of double keys */
		if (ak->cfra == bezt->vec[1][0]) {			
			/* set selection status and 'touched' status */
			if (BEZSELECTED(bezt)) ak->sel = SELECT;
			ak->modified += 1;
			
			return;
		}
		else if (ak->cfra < bezt->vec[1][0]) break;
	}
	
	/* add new block */
	akn= MEM_callocN(sizeof(ActKeyColumn), "ActKeyColumn");
	if (ak) BLI_insertlinkafter(keys, ak, akn);
	else BLI_addtail(keys, akn);
	
	akn->cfra= bezt->vec[1][0];
	akn->modified += 1;
	
	// TODO: handle type = bezt->h1 or bezt->h2
	akn->handle_type= 0; 
	
	if (BEZSELECTED(bezt))
		akn->sel = SELECT;
	else
		akn->sel = 0;
}

static void add_bezt_to_keyblockslist(ListBase *blocks, IpoCurve *icu, int index)
{
	/* The equivilant of add_to_cfra_elem except this version 
	 * makes ActKeyBlocks - one of the two datatypes required
	 * for action editor drawing.
	 */
	ActKeyBlock *ab, *abn;
	BezTriple *beztn=NULL, *prev=NULL;
	BezTriple *bezt;
	int v;
	
	/* get beztriples */
	beztn= (icu->bezt + index);
	
	/* we need to go through all beztriples, as they may not be in order (i.e. during transform) */
	for (v=0, bezt=icu->bezt; v<icu->totvert; v++, bezt++) {
		/* skip if beztriple is current */
		if (v != index) {
			/* check if beztriple is immediately before */
			if (beztn->vec[1][0] > bezt->vec[1][0]) {
				/* check if closer than previous was */
				if (prev) {
					if (prev->vec[1][0] < bezt->vec[1][0])
						prev= bezt;
				}
				else {
					prev= bezt;
				}
			}
		}
	}
	
	/* check if block needed - same value(s)?
	 *	-> firstly, handles must have same central value as each other
	 *	-> secondly, handles which control that section of the curve must be constant
	 */
	if ((!prev) || (!beztn)) return;
	if (IS_EQ(beztn->vec[1][1], prev->vec[1][1])==0) return;
	if (IS_EQ(beztn->vec[1][1], beztn->vec[0][1])==0) return;
	if (IS_EQ(prev->vec[1][1], prev->vec[2][1])==0) return;
	
	/* try to find a keyblock that starts on the previous beztriple 
	 * Note: we can't search from end to try to optimise this as it causes errors there's
	 * 		an A ___ B |---| B situation
	 */
	// FIXME: here there is a bug where we are trying to get the summary for the following channels
	//		A|--------------|A ______________ B|--------------|B
	//		A|------------------------------------------------|A
	//		A|----|A|---|A|-----------------------------------|A
	for (ab= blocks->first; ab; ab= ab->next) {
		/* check if alter existing block or add new block */
		if (ab->start == prev->vec[1][0]) {			
			/* set selection status and 'touched' status */
			if (BEZSELECTED(beztn)) ab->sel = SELECT;
			ab->modified += 1;
			
			return;
		}
		else if (ab->start < prev->vec[1][0]) break;
	}
	
	/* add new block */
	abn= MEM_callocN(sizeof(ActKeyBlock), "ActKeyBlock");
	if (ab) BLI_insertlinkbefore(blocks, ab, abn);
	else BLI_addtail(blocks, abn);
	
	abn->start= prev->vec[1][0];
	abn->end= beztn->vec[1][0];
	abn->val= beztn->vec[1][1];
	
	if (BEZSELECTED(prev) || BEZSELECTED(beztn))
		abn->sel = SELECT;
	else
		abn->sel = 0;
	abn->modified = 1;
}

/* helper function - find actkeycolumn that occurs on cframe */
static ActKeyColumn *cfra_find_actkeycolumn (ListBase *keys, float cframe)
{
	ActKeyColumn *ak, *ak2;
	
	if (keys==NULL) 
		return NULL;
	 
	/* search from both ends at the same time, and stop if we find match or if both ends meet */ 
	for (ak=keys->first, ak2=keys->last; ak && ak2; ak=ak->next, ak2=ak2->prev) {
		/* return whichever end encounters the frame */
		if (ak->cfra == cframe)
			return ak;
		if (ak2->cfra == cframe)
			return ak2;
		
		/* no matches on either end, so return NULL */
		if (ak == ak2)
			return NULL;
	}
	
	return NULL;
}

#if 0  // disabled, as some intel cards have problems with this
/* Draw a simple diamond shape with a filled in center (in screen space) */
static void draw_key_but(int x, int y, short w, short h, int sel)
{
	int xmin= x, ymin= y;
	int xmax= x+w-1, ymax= y+h-1;
	int xc= (xmin+xmax)/2, yc= (ymin+ymax)/2;
	
	/* interior - hardcoded colors (for selected and unselected only) */
	if (sel) glColor3ub(0xF1, 0xCA, 0x13);
	else glColor3ub(0xE9, 0xE9, 0xE9);
	
	glBegin(GL_QUADS);
	glVertex2i(xc, ymin);
	glVertex2i(xmax, yc);
	glVertex2i(xc, ymax);
	glVertex2i(xmin, yc);
	glEnd();
	
	
	/* outline */
	glColor3ub(0, 0, 0);
	
	glBegin(GL_LINE_LOOP);
	glVertex2i(xc, ymin);
	glVertex2i(xmax, yc);
	glVertex2i(xc, ymax);
	glVertex2i(xmin, yc);
	glEnd();
}
#endif

static void draw_keylist(gla2DDrawInfo *di, ListBase *keys, ListBase *blocks, float ypos)
{
	ActKeyColumn *ak;
	ActKeyBlock *ab;
	
	glEnable(GL_BLEND);
	
	/* draw keyblocks */
	if (blocks) {
		for (ab= blocks->first; ab; ab= ab->next) {
			short startCurves, endCurves, totCurves;
			
			/* find out how many curves occur at each keyframe */
			ak= cfra_find_actkeycolumn(keys, ab->start);
			startCurves = (ak)? ak->totcurve: 0;
			
			ak= cfra_find_actkeycolumn(keys, ab->end);
			endCurves = (ak)? ak->totcurve: 0;
			
			/* only draw keyblock if it appears in at all of the keyframes at lowest end */
			if (!startCurves && !endCurves) 
				continue;
			else
				totCurves = (startCurves>endCurves)? endCurves: startCurves;
				
			if (ab->totcurve >= totCurves) {
				int sc_xa, sc_ya;
				int sc_xb, sc_yb;
				
				/* get co-ordinates of block */
				gla2DDrawTranslatePt(di, ab->start, ypos, &sc_xa, &sc_ya);
				gla2DDrawTranslatePt(di, ab->end, ypos, &sc_xb, &sc_yb);
				
				/* draw block */
				if (ab->sel)
					BIF_ThemeColor4(TH_STRIP_SELECT);
				else
					BIF_ThemeColor4(TH_STRIP);
				glRectf((float)sc_xa, (float)sc_ya-3, (float)sc_xb, (float)sc_yb+5);
			}
		}
	}
	
	/* draw keys */
	if (keys) {
		for (ak= keys->first; ak; ak= ak->next) {
			int sc_x, sc_y;
			
			/* get co-ordinate to draw at */
			gla2DDrawTranslatePt(di, ak->cfra, ypos, &sc_x, &sc_y);
			
			/* draw using icons - old way which is slower but more proven */
			if (ak->sel & SELECT) BIF_icon_draw_aspect((float)sc_x-7, (float)sc_y-6, ICON_SPACE2, 1.0f);
			else BIF_icon_draw_aspect((float)sc_x-7, (float)sc_y-6, ICON_SPACE3, 1.0f);
			
			/* draw using OpenGL - slightly uglier but faster */
			// 	NOTE: disabled for now, as some intel cards seem to have problems with this
			//draw_key_but(sc_x-5, sc_y-4, 11, 11, (ak->sel & SELECT));
		}	
	}
	
	glDisable(GL_BLEND);
}

void draw_object_channel(gla2DDrawInfo *di, ActKeysInc *aki, Object *ob, float ypos)
{
	ListBase keys = {0, 0};
	ListBase blocks = {0, 0};

	ob_to_keylist(ob, &keys, &blocks, aki);
	draw_keylist(di, &keys, &blocks, ypos);
	
	BLI_freelistN(&keys);
	BLI_freelistN(&blocks);
}

void draw_ipo_channel(gla2DDrawInfo *di, ActKeysInc *aki, Ipo *ipo, float ypos)
{
	ListBase keys = {0, 0};
	ListBase blocks = {0, 0};

	ipo_to_keylist(ipo, &keys, &blocks, aki);
	draw_keylist(di, &keys, &blocks, ypos);
	
	BLI_freelistN(&keys);
	BLI_freelistN(&blocks);
}

void draw_icu_channel(gla2DDrawInfo *di, ActKeysInc *aki, IpoCurve *icu, float ypos)
{
	ListBase keys = {0, 0};
	ListBase blocks = {0, 0};

	icu_to_keylist(icu, &keys, &blocks, aki);
	draw_keylist(di, &keys, &blocks, ypos);
	
	BLI_freelistN(&keys);
	BLI_freelistN(&blocks);
}

void draw_agroup_channel(gla2DDrawInfo *di, ActKeysInc *aki, bActionGroup *agrp, float ypos)
{
	ListBase keys = {0, 0};
	ListBase blocks = {0, 0};

	agroup_to_keylist(agrp, &keys, &blocks, aki);
	draw_keylist(di, &keys, &blocks, ypos);
	
	BLI_freelistN(&keys);
	BLI_freelistN(&blocks);
}

void draw_action_channel(gla2DDrawInfo *di, ActKeysInc *aki, bAction *act, float ypos)
{
	ListBase keys = {0, 0};
	ListBase blocks = {0, 0};

	action_to_keylist(act, &keys, &blocks, aki);
	draw_keylist(di, &keys, &blocks, ypos);
	
	BLI_freelistN(&keys);
	BLI_freelistN(&blocks);
}

void draw_gpl_channel(gla2DDrawInfo *di, ActKeysInc *aki, bGPDlayer *gpl, float ypos)
{
	ListBase keys = {0, 0};
	
	gpl_to_keylist(gpl, &keys, NULL, aki);
	draw_keylist(di, &keys, NULL, ypos);
	BLI_freelistN(&keys);
}

/* --------------- Conversion: data -> keyframe list ------------------ */

void ob_to_keylist(Object *ob, ListBase *keys, ListBase *blocks, ActKeysInc *aki)
{
	bConstraintChannel *conchan;
	Key *key= ob_get_key(ob);

	if (ob) {
		bDopeSheet *ads= (aki)? (aki->ads) : NULL;
		int filterflag;
		
		/* get filterflag */
		if (ads)
			filterflag= ads->filterflag;
		else if (aki && aki->actmode == -1) /* only set like this by NLA */
			filterflag= ADS_FILTER_NLADUMMY;
		else
			filterflag= 0;
		
		/* Add object keyframes */
		if ((ob->ipo) && !(filterflag & ADS_FILTER_NOIPOS))
			ipo_to_keylist(ob->ipo, keys, blocks, aki);
		
		/* Add action keyframes */
		// FIXME: we may need to apply NLA-scaling here...
		if ((ob->action) && !(filterflag & ADS_FILTER_NOACTS)) {
			action_to_keylist(ob->action, keys, blocks, aki);
		}
		
		/* Add shapekey keyframes (only if dopesheet allows, if it is available) */
		if ((key && key->ipo) && !(filterflag & ADS_FILTER_NOSHAPEKEYS))
			ipo_to_keylist(key->ipo, keys, blocks, aki);
			
		/* Add material keyframes (only if dopesheet allows, if it is available) */
		if ((ob->totcol) && !(filterflag & ADS_FILTER_NOMAT)) {
			short a;
			
			for (a=0; a<ob->totcol; a++) {
				Material *ma= give_current_material(ob, a);
				
				if (ELEM(NULL, ma, ma->ipo) == 0)
					ipo_to_keylist(ma->ipo, keys, blocks, aki);
			}
		}
			
		/* Add object data keyframes */
		switch (ob->type) {
			case OB_CAMERA: /* ------- Camera ------------ */
			{
				Camera *ca= (Camera *)ob->data;
				if ((ca->ipo) && !(ads->filterflag & ADS_FILTER_NOCAM))
					ipo_to_keylist(ca->ipo, keys, blocks, aki);
			}
				break;
			case OB_LAMP: /* ---------- Lamp ----------- */
			{
				Lamp *la= (Lamp *)ob->data;
				if ((la->ipo) && !(ads->filterflag & ADS_FILTER_NOLAM))
					ipo_to_keylist(la->ipo, keys, blocks, aki);
			}
				break;
			case OB_CURVE: /* ------- Curve ---------- */
			{
				Curve *cu= (Curve *)ob->data;
				if ((cu->ipo) && !(ads->filterflag & ADS_FILTER_NOCUR))
					ipo_to_keylist(cu->ipo, keys, blocks, aki);
			}
				break;
		}
		
		/* Add constraint keyframes */
		if (!(filterflag & ADS_FILTER_NOCONSTRAINTS)) {
			for (conchan=ob->constraintChannels.first; conchan; conchan=conchan->next) {
				if (conchan->ipo)
					ipo_to_keylist(conchan->ipo, keys, blocks, aki);		
			}
		}
	}
}

static short bezt_in_aki_range (ActKeysInc *aki, BezTriple *bezt)
{
	/* when aki == NULL, we don't care about range */
	if (aki == NULL) 
		return 1;
		
	/* if nla-scaling is in effect, apply appropriate scaling adjustments */
	if (aki->ob) {
		float frame= get_action_frame_inv(aki->ob, bezt->vec[1][0]);
		return IN_RANGE(frame, aki->start, aki->end);
	}
	else {
		/* check if in range */
		return IN_RANGE(bezt->vec[1][0], aki->start, aki->end);
	}
}

void icu_to_keylist(IpoCurve *icu, ListBase *keys, ListBase *blocks, ActKeysInc *aki)
{
	BezTriple *bezt;
	ActKeyColumn *ak, *ak2;
	ActKeyBlock *ab, *ab2;
	int v;
	
	if (icu && icu->totvert) {
		/* loop through beztriples, making ActKeys and ActKeyBlocks */
		bezt= icu->bezt;
		
		for (v=0; v<icu->totvert; v++, bezt++) {
			/* only if keyframe is in range (optimisation) */
			if (bezt_in_aki_range(aki, bezt)) {
				add_bezt_to_keycolumnslist(keys, bezt);
				if (blocks) add_bezt_to_keyblockslist(blocks, icu, v);
			}
		}
		
		/* update the number of curves that elements have appeared in  */
		if (keys) {
			for (ak=keys->first, ak2=keys->last; ak && ak2; ak=ak->next, ak2=ak2->prev) {
				if (ak->modified) {
					ak->modified = 0;
					ak->totcurve += 1;
				}
				
				if (ak == ak2)
					break;
				
				if (ak2->modified) {
					ak2->modified = 0;
					ak2->totcurve += 1;
				}
			}
		}
		if (blocks) {
			for (ab=blocks->first, ab2=blocks->last; ab && ab2; ab=ab->next, ab2=ab2->prev) {
				if (ab->modified) {
					ab->modified = 0;
					ab->totcurve += 1;
				}
				
				if (ab == ab2)
					break;
				
				if (ab2->modified) {
					ab2->modified = 0;
					ab2->totcurve += 1;
				}
			}
		}
	}
}

void ipo_to_keylist(Ipo *ipo, ListBase *keys, ListBase *blocks, ActKeysInc *aki)
{
	IpoCurve *icu;
	
	if (ipo) {
		for (icu= ipo->curve.first; icu; icu= icu->next)
			icu_to_keylist(icu, keys, blocks, aki);
	}
}

void agroup_to_keylist(bActionGroup *agrp, ListBase *keys, ListBase *blocks, ActKeysInc *aki)
{
	bActionChannel *achan;
	bConstraintChannel *conchan;

	if (agrp) {
		/* loop through action channels */
		for (achan= agrp->channels.first; achan && achan->grp==agrp; achan= achan->next) {
			if (VISIBLE_ACHAN(achan)) {
				/* firstly, add keys from action channel's ipo block */
				if (achan->ipo)
					ipo_to_keylist(achan->ipo, keys, blocks, aki);
				
				/* then, add keys from constraint channels */
				for (conchan= achan->constraintChannels.first; conchan; conchan= conchan->next) {
					if (conchan->ipo)
						ipo_to_keylist(conchan->ipo, keys, blocks, aki);
				}
			}
		}
	}
}

void action_to_keylist(bAction *act, ListBase *keys, ListBase *blocks, ActKeysInc *aki)
{
	bActionChannel *achan;
	bConstraintChannel *conchan;

	if (act) {
		/* loop through action channels */
		for (achan= act->chanbase.first; achan; achan= achan->next) {
			/* firstly, add keys from action channel's ipo block */
			if (achan->ipo)
				ipo_to_keylist(achan->ipo, keys, blocks, aki);
			
			/* then, add keys from constraint channels */
			for (conchan= achan->constraintChannels.first; conchan; conchan= conchan->next) {
				if (conchan->ipo)
					ipo_to_keylist(conchan->ipo, keys, blocks, aki);
			}
		}
	}
}

void gpl_to_keylist(bGPDlayer *gpl, ListBase *keys, ListBase *blocks, ActKeysInc *aki)
{
	bGPDframe *gpf;
	ActKeyColumn *ak;
	
	if (gpl && keys) {
		/* loop over frames, converting directly to 'keyframes' (should be in order too) */
		for (gpf= gpl->frames.first; gpf; gpf= gpf->next) {
			ak= MEM_callocN(sizeof(ActKeyColumn), "ActKeyColumn");
			BLI_addtail(keys, ak);
			
			ak->cfra= (float)gpf->framenum;
			ak->modified = 1;
			ak->handle_type= 0; 
			
			if (gpf->flag & GP_FRAME_SELECT)
				ak->sel = SELECT;
			else
				ak->sel = 0;
		}
	}
}


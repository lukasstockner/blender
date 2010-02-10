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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributors: 2004/2005/2006 Blender Foundation, full recode
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <math.h>
#include <string.h>

/* external modules: */
#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_threads.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"        /* for rectcpy */

#include "DNA_group_types.h"
#include "DNA_image_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_texture_types.h"

#include "BKE_global.h"
#include "BKE_image.h"   // BKE_write_ibuf 
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"

/* this module */
#include "camera.h"
#include "database.h" 
#include "environment.h"
#include "envmap.h"
#include "lamp.h"
#include "object.h"
#include "object_halo.h"
#include "object_mesh.h"
#include "render_types.h"
#include "rendercore.h" 
#include "shadowbuf.h"
#include "texture.h"
#include "zbuf.h"

/* ------------------------------------------------------------------------- */

static void envmap_split_ima(EnvMap *env, ImBuf *ibuf)
{
	int dx, part;
	
	BKE_free_envmapdata(env);	
	
	dx= ibuf->y;
	dx/= 2;
	if(3*dx != ibuf->x) {
		printf("Incorrect envmap size\n");
		env->ok= 0;
		env->ima->ok= 0;
	}
	else {
		for(part=0; part<6; part++) {
			env->cube[part]= IMB_allocImBuf(dx, dx, 24, IB_rect, 0);
		}
		IMB_rectcpy(env->cube[0], ibuf, 
			0, 0, 0, 0, dx, dx);
		IMB_rectcpy(env->cube[1], ibuf, 
			0, 0, dx, 0, dx, dx);
		IMB_rectcpy(env->cube[2], ibuf, 
			0, 0, 2*dx, 0, dx, dx);
		IMB_rectcpy(env->cube[3], ibuf, 
			0, 0, 0, dx, dx, dx);
		IMB_rectcpy(env->cube[4], ibuf, 
			0, 0, dx, dx, dx, dx);
		IMB_rectcpy(env->cube[5], ibuf, 
			0, 0, 2*dx, dx, dx, dx);
		env->ok= ENV_OSA;
	}
}

/* ------------------------------------------------------------------------- */
/* ****************** RENDER ********************** */

/* copy current render */
static Render *envmap_render_copy(Render *re, EnvMap *env)
{
	Render *envre;
	int cuberes;
	
	envre= RE_NewRender("Envmap", RE_SLOT_DEFAULT);
	
	env->lastsize= re->params.r.size;
	cuberes = (env->cuberes * re->params.r.size) / 100;
	cuberes &= 0xFFFC;
	
	/* this flag has R_ZTRA in it for example */
	envre->params.flag= re->params.flag;
	
	/* set up renderdata */
	envre->params.r= re->params.r;
	envre->params.r.mode &= ~(R_BORDER | R_MBLUR);
	envre->params.r.layers.first= envre->params.r.layers.last= NULL;
	envre->params.r.filtertype= 0;
	envre->params.r.xparts= envre->params.r.yparts= 2;
	envre->params.r.size= 100;
	envre->params.r.yasp= envre->params.r.xasp= 1;
	
	RE_InitState(envre, NULL, &envre->params.r, NULL, cuberes, cuberes, NULL);
	envre->db.scene= re->db.scene;	/* unsure about this... */

	/* view stuff in env render */
	envre->cam.lens= 16.0f;
	if(env->type==ENV_PLANE)
		envre->cam.lens*= env->viewscale;
	envre->cam.ycor= 1.0f; 
	envre->cam.clipsta= env->clipsta;	/* render_scene_set_window() respects this for now */
	envre->cam.clipend= env->clipend;
	if(envre->cam.type == R_CAM_PANO)
		envre->cam.type= R_CAM_PERSP;
	
	RE_SetCamera(envre, env->object);
	
	/* callbacks */
	envre->cb.display_draw= re->cb.display_draw;
	envre->cb.ddh= re->cb.ddh;
	envre->cb.test_break= re->cb.test_break;
	envre->cb.tbh= re->cb.tbh;
	
	/* and for the evil stuff; copy the database... */
	envre->db.totvlak= re->db.totvlak;
	envre->db.totvert= re->db.totvert;
	envre->db.tothalo= re->db.tothalo;
	envre->db.totstrand= re->db.totstrand;
	envre->db.totlamp= re->db.totlamp;
	envre->db.sortedhalos= re->db.sortedhalos;
	envre->db.lights= re->db.lights;
	envre->db.objecttable= re->db.objecttable;
	envre->db.customdata_names= re->db.customdata_names;
	envre->db.raytree= re->db.raytree;
	envre->db.totinstance= re->db.totinstance;
	envre->db.instancetable= re->db.instancetable;
	envre->db.objectinstance= re->db.objectinstance;
	envre->sample.qmcsamplers= re->sample.qmcsamplers;
	
	return envre;
}

static void envmap_free_render_copy(Render *envre)
{

	envre->db.totvlak= 0;
	envre->db.totvert= 0;
	envre->db.tothalo= 0;
	envre->db.totstrand= 0;
	envre->db.totlamp= 0;
	envre->db.totinstance= 0;
	envre->db.sortedhalos= NULL;
	envre->db.lights.first= envre->db.lights.last= NULL;
	envre->db.objecttable.first= envre->db.objecttable.last= NULL;
	envre->db.customdata_names.first= envre->db.customdata_names.last= NULL;
	envre->db.raytree= NULL;
	envre->db.instancetable.first= envre->db.instancetable.last= NULL;
	envre->db.objectinstance= NULL;
	envre->sample.qmcsamplers= NULL;
	
	RE_FreeRender(envre);
}

/* ------------------------------------------------------------------------- */

static void envmap_transmatrix(float mat[][4], int part)
{
	float tmat[4][4], eul[3], rotmat[4][4];
	
	eul[0]= eul[1]= eul[2]= 0.0;
	
	if(part==0) {			/* neg z */
		;
	} else if(part==1) {	/* pos z */
		eul[0]= M_PI;
	} else if(part==2) {	/* pos y */
		eul[0]= M_PI/2.0;
	} else if(part==3) {	/* neg x */
		eul[0]= M_PI/2.0;
		eul[2]= M_PI/2.0;
	} else if(part==4) {	/* neg y */
		eul[0]= M_PI/2.0;
		eul[2]= M_PI;
	} else {				/* pos x */
		eul[0]= M_PI/2.0;
		eul[2]= -M_PI/2.0;
	}
	
	copy_m4_m4(tmat, mat);
	eul_to_mat4( rotmat,eul);
	mul_serie_m4(mat, tmat, rotmat,
					 0,   0,    0,
					 0,   0,    0);
}

/* ------------------------------------------------------------------------- */

static void env_rotate_scene(Render *re, float mat[][4], int mode)
{
	GroupObject *go;
	ObjectRen *obr;
	ObjectInstanceRen *obi;
	LampRen *lar = NULL;
	HaloRen *har = NULL;
	float imat[3][3], tmat[4][4], cmat[3][3], tmpmat[4][4];
	int a;
	
	if(mode==0) {
		invert_m4_m4(tmat, mat);
		copy_m3_m4(imat, tmat);
	}
	else {
		copy_m4_m4(tmat, mat);
		copy_m3_m4(imat, mat);
	}

	for(obi=re->db.instancetable.first; obi; obi=obi->next) {
		/* append or set matrix depending on dupli */
		if(obi->flag & R_DUPLI_TRANSFORMED) {
			copy_m4_m4(tmpmat, obi->mat);
			mul_m4_m4m4(obi->mat, tmpmat, tmat);
		}
		else if(mode==1)
			copy_m4_m4(obi->mat, tmat);
		else
			unit_m4(obi->mat);

		copy_m3_m4(cmat, obi->mat);
		invert_m3_m3(obi->nmat, cmat);
		transpose_m3(obi->nmat);

		/* indicate the renderer has to use transform matrices */
		if(mode==0)
			obi->flag &= ~R_ENV_TRANSFORMED;
		else
			obi->flag |= R_ENV_TRANSFORMED;
	}
	

	for(obr=re->db.objecttable.first; obr; obr=obr->next) {
		for(a=0; a<obr->tothalo; a++) {
			if((a & 255)==0) har= obr->bloha[a>>8];
			else har++;
		
			mul_m4_v3(tmat, har->co);
		}
	}
	
	for(go=re->db.lights.first; go; go= go->next) {
		lar= go->lampren;
		
		/* removed here some horrible code of someone in NaN who tried to fix
		   prototypes... just solved by introducing a correct cmat[3][3] instead
		   of using smat. this works, check square spots in reflections  (ton) */
		copy_m3_m3(cmat, lar->imat); 
		mul_m3_m3m3(lar->imat, cmat, imat); 

		mul_m3_v3(imat, lar->vec);
		mul_m4_v3(tmat, lar->co);

		lar->sh_invcampos[0]= -lar->co[0];
		lar->sh_invcampos[1]= -lar->co[1];
		lar->sh_invcampos[2]= -lar->co[2];
		mul_m3_v3(lar->imat, lar->sh_invcampos);
		lar->sh_invcampos[2]*= lar->sh_zfac;
		
		if(lar->shb)
			shadowbuf_rotate(lar, mat, !mode);
	}
}

/* ------------------------------------------------------------------------- */

static void env_layerflags(Render *re, unsigned int notlay)
{
	ObjectInstanceRen *obi;
	
	/* invert notlay, so if face is in multiple layers it will still be visible,
	   unless all 'notlay' bits match the face bits.
	   face: 0110
	   not:  0100
	   ~not: 1011
	   now (face & ~not) is true
	*/
	
	notlay= ~notlay;
	
	for(obi=re->db.instancetable.first; obi; obi=obi->next)
		if((obi->lay & notlay)==0)
			obi->flag |= R_HIDDEN;
}

static void env_hideobject(Render *re, Object *ob)
{
	ObjectInstanceRen *obi;
	
	for(obi=re->db.instancetable.first; obi; obi=obi->next)
		if(obi->obr->ob == ob)
			obi->flag |= R_HIDDEN;
}

static void env_showobjects(Render *re)
{
	ObjectInstanceRen *obi;
	
	for(obi=re->db.instancetable.first; obi; obi=obi->next)
		obi->flag &= ~R_HIDDEN;
}

/* ------------------------------------------------------------------------- */

static void env_set_imats(Render *re)
{
	Base *base;
	float mat[4][4];
	
	base= re->db.scene->base.first;
	while(base) {
		mul_m4_m4m4(mat, base->object->obmat, re->cam.viewmat);
		invert_m4_m4(base->object->imat, mat);
		
		base= base->next;
	}

}	

/* ------------------------------------------------------------------------- */

static void render_envmap(Render *re, EnvMap *env)
{
	/* only the cubemap and planar map is implemented */
	Render *envre;
	ImBuf *ibuf;
	float orthmat[4][4];
	float oldviewinv[4][4], mat[4][4], tmat[4][4];
	short part;
	
	/* need a recalc: ortho-render has no correct viewinv */
	invert_m4_m4(oldviewinv, re->cam.viewmat);

	envre= envmap_render_copy(re, env);
	
	/* precalc orthmat for object */
	copy_m4_m4(orthmat, env->object->obmat);
	normalize_m4(orthmat);
	
	/* need imat later for texture imat */
	mul_m4_m4m4(mat, orthmat, re->cam.viewmat);
	invert_m4_m4(tmat, mat);
	copy_m3_m4(env->obimat, tmat);

	for(part=0; part<6; part++) {
		if(env->type==ENV_PLANE && part!=1)
			continue;
		
		re->cb.display_clear(re->cb.dch, envre->result);
		
		copy_m4_m4(tmat, orthmat);
		envmap_transmatrix(tmat, part);
		invert_m4_m4(mat, tmat);
		/* mat now is the camera 'viewmat' */

		copy_m4_m4(envre->cam.viewmat, mat);
		copy_m4_m4(envre->cam.viewinv, tmat);
		
		/* we have to correct for the already rotated vertexcoords */
		mul_m4_m4m4(tmat, oldviewinv, envre->cam.viewmat);
		invert_m4_m4(env->imat, tmat);
		
		env_rotate_scene(envre, tmat, 1);
		environment_init(envre, envre->db.scene->world);
		halos_project(&envre->db, &envre->cam, 0, envre->xparts);
		env_layerflags(envre, env->notlay);
		env_hideobject(envre, env->object);
		env_set_imats(envre);
				
		if(re->cb.test_break(re->cb.tbh)==0) {
			RE_TileProcessor(envre, 0, 0);
		}
		
		/* rotate back */
		env_showobjects(envre);
		env_rotate_scene(envre, tmat, 0);

		if(re->cb.test_break(re->cb.tbh)==0) {
			RenderLayer *rl= envre->result->layers.first;
			int y;
			char *alpha;
			
			ibuf= IMB_allocImBuf(envre->rectx, envre->recty, 24, IB_rect, 0);
			ibuf->rect_float= rl->rectf;
			IMB_rect_from_float(ibuf);
			ibuf->rect_float= NULL;
			
			/* envmap renders without alpha */
			alpha= ((char *)ibuf->rect)+3;
			for(y= ibuf->x*ibuf->y - 1; y>=0; y--, alpha+=4)
				*alpha= 255;
			
			env->cube[part]= ibuf;
		}
		
		if(re->cb.test_break(re->cb.tbh)) break;

	}
	
	if(re->cb.test_break(re->cb.tbh)) BKE_free_envmapdata(env);
	else {
		if(envre->params.r.mode & R_OSA) env->ok= ENV_OSA;
		else env->ok= ENV_NORMAL;
		env->lastframe= re->db.scene->r.cfra;
	}
	
	/* restore */
	envmap_free_render_copy(envre);
	env_set_imats(re);

}

/* ------------------------------------------------------------------------- */

void envmaps_make(Render *re)
{
	Tex *tex;
	int do_init= 0, depth= 0, trace;
	
	if (!(re->params.r.mode & R_ENVMAP)) return;
	
	/* we dont raytrace, disabling the flag will cause ray_transp render solid */
	trace= (re->params.r.mode & R_RAYTRACE);
	re->params.r.mode &= ~R_RAYTRACE;

	re->cb.i.infostr= "Creating Environment maps";
	re->cb.stats_draw(re->cb.sdh, &re->cb.i);
	
	/* 5 = hardcoded max recursion level */
	while(depth<5) {
		tex= G.main->tex.first;
		while(tex) {
			if(tex->id.us && tex->type==TEX_ENVMAP) {
				if(tex->env && tex->env->object) {
					EnvMap *env= tex->env;
					
					if(env->object->lay & re->db.scene->lay) {
						if(env->stype==ENV_LOAD) {
							float orthmat[4][4], mat[4][4], tmat[4][4];
							
							/* precalc orthmat for object */
							copy_m4_m4(orthmat, env->object->obmat);
							normalize_m4(orthmat);
							
							/* need imat later for texture imat */
							mul_m4_m4m4(mat, orthmat, re->cam.viewmat);
							invert_m4_m4(tmat, mat);
							copy_m3_m4(env->obimat, tmat);
						}
						else {
							
							/* decide if to render an envmap (again) */
							if(env->depth >= depth) {
								
								/* set 'recalc' to make sure it does an entire loop of recalcs */
								
								if(env->ok) {
										/* free when OSA, and old one isn't OSA */
									if((re->params.r.mode & R_OSA) && env->ok==ENV_NORMAL) 
										BKE_free_envmapdata(env);
										/* free when size larger */
									else if(env->lastsize < re->params.r.size) 
										BKE_free_envmapdata(env);
										/* free when env is in recalcmode */
									else if(env->recalc)
										BKE_free_envmapdata(env);
								}
								
								if(env->ok==0 && depth==0) env->recalc= 1;
								
								if(env->ok==0) {
									do_init= 1;
									render_envmap(re, env);
									
									if(depth==env->depth) env->recalc= 0;
								}
							}
						}
					}
				}
			}
			tex= tex->id.next;
		}
		depth++;
	}

	if(do_init) {
		re->cb.display_init(re->cb.dih, re->result);
		re->cb.display_clear(re->cb.dch, re->result);
		// re->params.flag |= R_REDRAW_PRV;
	}	
	// restore
	re->params.r.mode |= trace;

}

/* ------------------------------------------------------------------------- */

static int envcube_isect(EnvMap *env, float *vec, float *answ)
{
	float labda;
	int face;
	
	if(env->type==ENV_PLANE) {
		face= 1;
		
		labda= 1.0/vec[2];
		answ[0]= env->viewscale*labda*vec[0];
		answ[1]= -env->viewscale*labda*vec[1];
	}
	else {
		/* which face */
		if( vec[2]<=-fabs(vec[0]) && vec[2]<=-fabs(vec[1]) ) {
			face= 0;
			labda= -1.0/vec[2];
			answ[0]= labda*vec[0];
			answ[1]= labda*vec[1];
		}
		else if( vec[2]>=fabs(vec[0]) && vec[2]>=fabs(vec[1]) ) {
			face= 1;
			labda= 1.0/vec[2];
			answ[0]= labda*vec[0];
			answ[1]= -labda*vec[1];
		}
		else if( vec[1]>=fabs(vec[0]) ) {
			face= 2;
			labda= 1.0/vec[1];
			answ[0]= labda*vec[0];
			answ[1]= labda*vec[2];
		}
		else if( vec[0]<=-fabs(vec[1]) ) {
			face= 3;
			labda= -1.0/vec[0];
			answ[0]= labda*vec[1];
			answ[1]= labda*vec[2];
		}
		else if( vec[1]<=-fabs(vec[0]) ) {
			face= 4;
			labda= -1.0/vec[1];
			answ[0]= -labda*vec[0];
			answ[1]= labda*vec[2];
		}
		else {
			face= 5;
			labda= 1.0/vec[0];
			answ[0]= -labda*vec[1];
			answ[1]= labda*vec[2];
		}
	}
	
	answ[0]= 0.5+0.5*answ[0];
	answ[1]= 0.5+0.5*answ[1];
	return face;
}

/* ------------------------------------------------------------------------- */

static void set_dxtdyt(float *dxts, float *dyts, float *dxt, float *dyt, int face)
{
	if(face==2 || face==4) {
		dxts[0]= dxt[0];
		dyts[0]= dyt[0];
		dxts[1]= dxt[2];
		dyts[1]= dyt[2];
	}
	else if(face==3 || face==5) {
		dxts[0]= dxt[1];
		dxts[1]= dxt[2];
		dyts[0]= dyt[1];
		dyts[1]= dyt[2];
	}
	else {
		dxts[0]= dxt[0];
		dyts[0]= dyt[0];
		dxts[1]= dxt[1];
		dyts[1]= dyt[1];
	}
}

/* ------------------------------------------------------------------------- */

void tex_envmap_init(Render *re, Tex *tex)
{
	/* just in case */
	tex->imaflag |= TEX_INTERPOL | TEX_MIPMAP;
	tex->extend= TEX_CLIP;
	
	if(tex->env) {
		if(tex->env->type==ENV_PLANE)
			tex->extend= TEX_EXTEND;
		
		/* only free envmap when rendermode was set to render envmaps, for previewrender */
		if(G.rendering && re)
			if(re->params.r.mode & R_ENVMAP)
				if(tex->env->stype==ENV_ANIM) 
					BKE_free_envmapdata(tex->env);
	}
}

int tex_envmap_sample(RenderParams *rpm, Tex *tex, float *texvec, float *dxt, float *dyt, int osatex, TexResult *texres)
{
	/* texvec should be the already reflected normal */
	EnvMap *env;
	ImBuf *ibuf;
	float fac, vec[3], sco[3], dxts[3], dyts[3];
	int face, face1;
	
	env= tex->env;
	if(env==NULL || (env->stype!=ENV_LOAD && env->object==NULL)) {
		texres->tin= 0.0;
		return 0;
	}
	if(env->stype==ENV_LOAD) {
		env->ima= tex->ima;
		if(env->ima && env->ima->ok) {
			if(env->cube[0]==NULL) {
				ImBuf *ibuf= BKE_image_get_ibuf(env->ima, NULL);
				if(ibuf)
					envmap_split_ima(env, ibuf);
				else
					env->ok= 0;
			}
		}
	}

	if(env->ok==0) {
		
		texres->tin= 0.0;
		return 0;
	}
	
	/* rotate to envmap space, if object is set */
	copy_v3_v3(vec, texvec);
	
	face= envcube_isect(env, vec, sco);
	ibuf= env->cube[face];
	
	if(osatex) {
		set_dxtdyt(dxts, dyts, dxt, dyt, face);
		imagewraposa(rpm, tex, NULL, ibuf, sco, dxts, dyts, texres);
		
		/* edges? */
		
		if(texres->ta<1.0) {
			TexResult texr1, texr2;
	
			texr1.nor= texr2.nor= NULL;

			add_v3_v3v3(vec, vec, dxt);
			face1= envcube_isect(env, vec, sco);
			sub_v3_v3v3(vec, vec, dxt);
			
			if(face!=face1) {
				ibuf= env->cube[face1];
				set_dxtdyt(dxts, dyts, dxt, dyt, face1);
				imagewraposa(rpm, tex, NULL, ibuf, sco, dxts, dyts, &texr1);
			}
			else texr1.tr= texr1.tg= texr1.tb= texr1.ta= 0.0;
			
			/* here was the nasty bug! results were not zero-ed. FPE! */
			
			add_v3_v3v3(vec, vec, dyt);
			face1= envcube_isect(env, vec, sco);
			sub_v3_v3v3(vec, vec, dyt);
			
			if(face!=face1) {
				ibuf= env->cube[face1];
				set_dxtdyt(dxts, dyts, dxt, dyt, face1);
				imagewraposa(rpm, tex, NULL, ibuf, sco, dxts, dyts, &texr2);
			}
			else texr2.tr= texr2.tg= texr2.tb= texr2.ta= 0.0;
			
			fac= (texres->ta+texr1.ta+texr2.ta);
			if(fac!=0.0) {
				fac= 1.0/fac;

				texres->tr= fac*(texres->ta*texres->tr + texr1.ta*texr1.tr + texr2.ta*texr2.tr );
				texres->tg= fac*(texres->ta*texres->tg + texr1.ta*texr1.tg + texr2.ta*texr2.tg );
				texres->tb= fac*(texres->ta*texres->tb + texr1.ta*texr1.tb + texr2.ta*texr2.tb );
			}
			texres->ta= 1.0;
		}
	}
	else {
		imagewrap(rpm, tex, NULL, ibuf, sco, texres);
	}
	
	return 1;
}

void envmap_map(Render *re, Tex *tex, float *texvec, float *dxt, float *dyt, int osatex)
{
	EnvMap *env= tex->env;

	if(env->object) mul_m3_v3(env->obimat, texvec);
	else mul_mat3_m4_v3(re->cam.viewinv, texvec);
	
	if(osatex) {
		if(env->object) {
			mul_m3_v3(env->obimat, dxt);
			mul_m3_v3(env->obimat, dyt);
		}
		else {
			mul_mat3_m4_v3(re->cam.viewinv, dxt);
			mul_mat3_m4_v3(re->cam.viewinv, dyt);
		}
	}
}

/* ------------------------------------------------------------------------- */

/* eof */

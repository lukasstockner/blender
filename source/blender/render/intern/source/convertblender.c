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

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_rand.h"
#include "BLI_memarena.h"
#include "BLI_ghash.h"

#include "DNA_camera_types.h"
#include "DNA_material_types.h"
#include "DNA_group_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_object_fluidsim.h"
#include "DNA_scene_types.h"
#include "DNA_texture_types.h"

#include "BKE_anim.h"
#include "BKE_colortools.h"
#include "BKE_DerivedMesh.h"
#include "BKE_displist.h"
#include "BKE_global.h"
#include "BKE_group.h"
#include "BKE_image.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_mball.h"
#include "BKE_modifier.h"
#include "BKE_node.h"
#include "BKE_object.h"
#include "BKE_particle.h"
#include "BKE_scene.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"
#include "BKE_world.h"

#include "PIL_time.h"

#include "cache.h"
#include "camera.h"
#include "database.h"
#include "diskocclusion.h"
#include "environment.h"
#include "envmap.h"
#include "lamp.h"
#include "object.h"
#include "object_halo.h"
#include "object_mesh.h"
#include "pointdensity.h"
#include "raytrace.h"
#include "render_types.h"
#include "rendercore.h"
#include "sampler.h"
#include "shading.h"
#include "shadowbuf.h"
#include "sss.h"
#include "object_strand.h"
#include "texture.h"
#include "texture_stack.h"
#include "volumetric.h"
#include "volume_precache.h"
#include "zbuf.h"

static void check_material_mapto(Material *ma)
{
	int a;
	ma->mapto_textured = 0;
	
	/* cache which inputs are actually textured.
	 * this can avoid a bit of time spent iterating through all the texture slots, map inputs and map tos
	 * every time a property which may or may not be textured is accessed */
	
	for(a=0; a<MAX_MTEX; a++) {
		if(ma->mtex[a] && ma->mtex[a]->tex) {
			/* currently used only in volume render, so we'll check for those flags */
			if(ma->mtex[a]->mapto & MAP_DENSITY) ma->mapto_textured |= MAP_DENSITY;
			if(ma->mtex[a]->mapto & MAP_EMISSION) ma->mapto_textured |= MAP_EMISSION;
			if(ma->mtex[a]->mapto & MAP_EMISSION_COL) ma->mapto_textured |= MAP_EMISSION_COL;
			if(ma->mtex[a]->mapto & MAP_SCATTERING) ma->mapto_textured |= MAP_SCATTERING;
			if(ma->mtex[a]->mapto & MAP_TRANSMISSION_COL) ma->mapto_textured |= MAP_TRANSMISSION_COL;
			if(ma->mtex[a]->mapto & MAP_REFLECTION) ma->mapto_textured |= MAP_REFLECTION;
			if(ma->mtex[a]->mapto & MAP_REFLECTION_COL) ma->mapto_textured |= MAP_REFLECTION_COL;
		}
	}
}
static void flag_render_node_material(Render *re, bNodeTree *ntree)
{
	bNode *node;

	for(node=ntree->nodes.first; node; node= node->next) {
		if(node->id) {
			if(GS(node->id->name)==ID_MA) {
				Material *ma= (Material *)node->id;

				if((ma->mode & MA_TRANSP) && (ma->mode & MA_ZTRANSP))
					re->params.flag |= R_ZTRA;

				ma->flag |= MA_IS_USED;
			}
			else if(node->type==NODE_GROUP)
				flag_render_node_material(re, (bNodeTree *)node->id);
		}
	}
}

Material *give_render_material(Render *re, Object *ob, int nr)
{
	extern Material defmaterial;	/* material.c */
	Material *ma;
	
	ma= give_current_material(ob, nr);
	if(ma==NULL) 
		ma= &defmaterial;
	
	if(re->params.r.mode & R_SPEED) ma->texco |= NEED_UV;
	
	if(ma->material_type == MA_TYPE_VOLUME) {
		ma->mode |= MA_TRANSP;
		ma->mode &= ~MA_SHADBUF;
	}
	if((ma->mode & MA_TRANSP) && (ma->mode & MA_ZTRANSP))
		re->params.flag |= R_ZTRA;
	
	/* for light groups */
	ma->flag |= MA_IS_USED;

	if(ma->nodetree && ma->use_nodes)
		flag_render_node_material(re, ma->nodetree);
	
	check_material_mapto(ma);
	
	return ma;
}

static void init_camera_inside_volumes(Render *re)
{
	ObjectInstanceRen *obi;
	VolumeOb *vo;
	float co[3] = {0.f, 0.f, 0.f};

	for(vo= re->db.volumes.first; vo; vo= vo->next) {
		for(obi= re->db.instancetable.first; obi; obi= obi->next) {
			if (obi->obr == vo->obr) {
				if (point_inside_volume_objectinstance(re, obi, co)) {
					MatInside *mi;
					
					mi = MEM_mallocN(sizeof(MatInside), "camera inside material");
					mi->ma = vo->ma;
					mi->obi = obi;
					
					BLI_addtail(&(re->db.render_volumes_inside), mi);
				}
			}
		}
	}
	
	/* debug {
	MatInside *m;
	for (m=re->db.render_volumes_inside.first; m; m=m->next) {
		printf("matinside: ma: %s \n", m->ma->id.name+2);
	}
	}*/
}

static void add_volume(Render *re, ObjectRen *obr, Material *ma)
{
	struct VolumeOb *vo;
	
	vo = MEM_mallocN(sizeof(VolumeOb), "volume object");
	
	vo->ma = ma;
	vo->obr = obr;
	
	BLI_addtail(&re->db.volumes, vo);
}

static void set_material_lightgroups(Render *re)
{
	Group *group;
	Material *ma;
	
	/* not for preview render */
	if(re->db.scene->r.scemode & R_PREVIEWBUTS)
		return;
	
	for(group= G.main->group.first; group; group=group->id.next)
		group->id.flag |= LIB_DOIT;
	
	/* it's a bit too many loops in loops... but will survive */
	/* hola! materials not in use...? */
	for(ma= G.main->mat.first; ma; ma=ma->id.next) {
		if(ma->group && (ma->group->id.flag & LIB_DOIT))
			lightgroup_create(re, ma->group, ma->mode & MA_GROUP_NOLAY);
	}
}

static void set_renderlayer_lightgroups(Render *re, Scene *sce)
{
	SceneRenderLayer *srl;
	
	for(srl= sce->r.layers.first; srl; srl= srl->next) {
		if(srl->light_override)
			lightgroup_create(re, srl->light_override, 0);
	}
}

/* ------------------------------------------------------------------------- */
/* Database																	 */
/* ------------------------------------------------------------------------- */

static int render_object_type(int type) 
{
	return ELEM5(type, OB_FONT, OB_CURVE, OB_SURF, OB_MESH, OB_MBALL);
}

static void find_dupli_instances(Render *re, ObjectRen *obr)
{
	ObjectInstanceRen *obi;
	float imat[4][4], obmat[4][4], obimat[4][4], nmat[3][3];
	int first = 1;

	mul_m4_m4m4(obmat, obr->obmat, re->cam.viewmat);
	invert_m4_m4(imat, obmat);

	/* for objects instanced by dupliverts/faces/particles, we go over the
	 * list of instances to find ones that instance obr, and setup their
	 * matrices and obr pointer */
	for(obi=re->db.instancetable.last; obi; obi=obi->prev) {
		if(!obi->obr && obi->ob == obr->ob && obi->psysindex == obr->psysindex) {
			obi->obr= obr;

			/* compute difference between object matrix and
			 * object matrix with dupli transform, in viewspace */
			copy_m4_m4(obimat, obi->mat);
			mul_m4_m4m4(obi->mat, imat, obimat);

			copy_m3_m4(nmat, obi->mat);
			invert_m3_m3(obi->nmat, nmat);
			transpose_m3(obi->nmat);

			if(!first) {
				re->db.totvert += obr->totvert;
				re->db.totvlak += obr->totvlak;
				re->db.tothalo += obr->tothalo;
				re->db.totstrand += obr->totstrand;
			}
			else
				first= 0;
		}
	}
}

static void assign_dupligroup_dupli(Render *re, ObjectInstanceRen *obi, ObjectRen *obr)
{
	float imat[4][4], obmat[4][4], obimat[4][4], nmat[3][3];

	mul_m4_m4m4(obmat, obr->obmat, re->cam.viewmat);
	invert_m4_m4(imat, obmat);

	obi->obr= obr;

	/* compute difference between object matrix and
	 * object matrix with dupli transform, in viewspace */
	copy_m4_m4(obimat, obi->mat);
	mul_m4_m4m4(obi->mat, imat, obimat);

	copy_m3_m4(nmat, obi->mat);
	invert_m3_m3(obi->nmat, nmat);
	transpose_m3(obi->nmat);

	re->db.totvert += obr->totvert;
	re->db.totvlak += obr->totvlak;
	re->db.tothalo += obr->tothalo;
	re->db.totstrand += obr->totstrand;
}

static ObjectRen *find_dupligroup_dupli(Render *re, Object *ob, int psysindex)
{
	ObjectRen *obr;

	/* if the object is itself instanced, we don't want to create an instance
	 * for it */
	if(ob->transflag & OB_RENDER_DUPLI)
		return NULL;

	/* try to find an object that was already created so we can reuse it
	 * and save memory */
	for(obr=re->db.objecttable.first; obr; obr=obr->next)
		if(obr->ob == ob && obr->psysindex == psysindex && (obr->flag & R_INSTANCEABLE))
			return obr;
	
	return NULL;
}

static void set_dupli_tex_mat(Render *re, ObjectInstanceRen *obi, DupliObject *dob)
{
	/* For duplis we need to have a matrix that transform the coordinate back
	 * to it's original position, without the dupli transforms. We also check
	 * the matrix is actually needed, to save memory on lots of dupliverts for
	 * example */
	static Object *lastob= NULL;
	static int needtexmat= 0;

	/* init */
	if(!re) {
		lastob= NULL;
		needtexmat= 0;
		return;
	}

	/* check if we actually need it */
	if(lastob != dob->ob) {
		Material ***material;
		short a, *totmaterial;

		lastob= dob->ob;
		needtexmat= 0;

		totmaterial= give_totcolp(dob->ob);
		material= give_matarar(dob->ob);

		if(totmaterial && material)
			for(a= 0; a<*totmaterial; a++)
				if((*material)[a] && (*material)[a]->texco & TEXCO_OBJECT)
					needtexmat= 1;
	}

	if(needtexmat) {
		float imat[4][4];

		obi->duplitexmat= BLI_memarena_alloc(re->db.memArena, sizeof(float)*4*4);
		invert_m4_m4(imat, dob->mat);
		mul_serie_m4(obi->duplitexmat, re->cam.viewmat, dob->omat, imat, re->cam.viewinv, 0, 0, 0, 0);
	}
}

static void add_render_object(Render *re, Object *ob, Object *par, DupliObject *dob, int timeoffset, int vectorlay)
{
	ObjectRen *obr;
	ObjectInstanceRen *obi= NULL;
	ParticleSystem *psys;
	int show_emitter, allow_render= 1, index, psysindex, i;

	index= (dob)? dob->index: 0;

	/* the emitter has to be processed first (render levels of modifiers) */
	/* so here we only check if the emitter should be rendered */
	if(ob->particlesystem.first) {
		show_emitter= 0;
		for(psys=ob->particlesystem.first; psys; psys=psys->next) {
			show_emitter += psys->part->draw & PART_DRAW_EMITTER;
			psys_render_set(ob, psys, re->cam.viewmat, re->cam.winmat, re->cam.winx, re->cam.winy, timeoffset);
		}

		/* if no psys has "show emitter" selected don't render emitter */
		if(show_emitter == 0)
			allow_render= 0;
	}

	/* one render object for the data itself */
	if(allow_render) {
		obr= render_object_create(&re->db, ob, par, index, 0, ob->lay);
		if((dob && !dob->animated) || (ob->transflag & OB_RENDER_DUPLI)) {
			obr->flag |= R_INSTANCEABLE;
			copy_m4_m4(obr->obmat, ob->obmat);
		}
		if(obr->lay & vectorlay)
			obr->flag |= R_NEED_VECTORS;
		init_render_object_data(re, obr, timeoffset);

		/* only add instance for objects that have not been used for dupli */
		if(!(ob->transflag & OB_RENDER_DUPLI)) {
			obi= render_instance_create(&re->db, obr, ob, par, index, 0, NULL, ob->lay);
			if(dob) set_dupli_tex_mat(re, obi, dob);
		}
		else
			find_dupli_instances(re, obr);

		for (i=1; i<=ob->totcol; i++) {
			Material* ma = give_render_material(re, ob, i);
			if (ma && ma->material_type == MA_TYPE_VOLUME)
				add_volume(re, obr, ma);
		}

		/* create low resolution version */
		if(ob->displacebound > 0.0f) {
			obr->flag |= R_HIGHRES;

			obr->lowres= render_object_create(&re->db, ob, par, index, 0, ob->lay);
			obr= obr->lowres;
			obr->flag |= R_LOWRES;

			if(obr->lay & vectorlay)
				obr->flag |= R_NEED_VECTORS;
			init_render_object_data(re, obr, timeoffset);
		}
	}

	/* and one render object per particle system */
	if(ob->particlesystem.first) {
		psysindex= 1;
		for(psys=ob->particlesystem.first; psys; psys=psys->next, psysindex++) {
			obr= render_object_create(&re->db, ob, par, index, psysindex, ob->lay);
			if((dob && !dob->animated) || (ob->transflag & OB_RENDER_DUPLI)) {
				obr->flag |= R_INSTANCEABLE;
				copy_m4_m4(obr->obmat, ob->obmat);
			}
			if(obr->lay & vectorlay)
				obr->flag |= R_NEED_VECTORS;
			init_render_object_data(re, obr, timeoffset);
			psys_render_restore(ob, psys);

			/* only add instance for objects that have not been used for dupli */
			if(!(ob->transflag & OB_RENDER_DUPLI)) {
				obi= render_instance_create(&re->db, obr, ob, par, index, psysindex, NULL, ob->lay);
				if(dob) set_dupli_tex_mat(re, obi, dob);
			}
			else
				find_dupli_instances(re, obr);
		}
	}
}

/* par = pointer to duplicator parent, needed for object lookup table */
/* index = when duplicater copies same object (particle), the counter */
static void init_render_object(Render *re, Object *ob, Object *par, DupliObject *dob, int timeoffset, int vectorlay)
{
	static double lasttime= 0.0;
	double time;
	float mat[4][4];

	if(ob->type==OB_LAMP)
		lamp_create(re, ob);
	else if(render_object_type(ob->type))
		add_render_object(re, ob, par, dob, timeoffset, vectorlay);
	else {
		mul_m4_m4m4(mat, ob->obmat, re->cam.viewmat);
		invert_m4_m4(ob->imat, mat);
	}
	
	time= PIL_check_seconds_timer();
	if(time - lasttime > 1.0) {
		lasttime= time;
		/* clumsy copying still */
		re->cb.i.totvert= re->db.totvert;
		re->cb.i.totface= re->db.totvlak;
		re->cb.i.totstrand= re->db.totstrand;
		re->cb.i.tothalo= re->db.tothalo;
		re->cb.i.totlamp= re->db.totlamp;
		re->cb.stats_draw(re->cb.sdh, &re->cb.i);
	}

	ob->flag |= OB_DONE;
}

void materials_init(Render *re)
{
	/* still bad... doing all */
	init_render_materials(re->params.r.mode);
	set_node_shader_lamp_loop(shade_material_loop);
}

void materials_free(Render *re)
{
	end_render_materials();
}

void textures_init(Render *re)
{
	tex_list_init(re, &G.main->tex);
}

void textures_free(Render *re)
{
	tex_list_free(re, &G.main->tex);

	if(re->db.scene)
		if(re->db.scene->r.scemode & R_FREE_IMAGE)
			if((re->params.r.scemode & R_PREVIEWBUTS)==0)
				BKE_image_free_all_textures();
}

void RE_Database_Free(Render *re)
{
	Object *ob;
	
	/* statistics for debugging render memory usage */
	if((G.f & G_DEBUG) && (G.rendering)) {
		if((re->params.r.scemode & R_PREVIEWBUTS)==0) {
			BKE_image_print_memlist();
			MEM_printmemlist_stats();
		}
	}

	/* remake metaball display lists */
	for(ob=G.main->object.first; ob; ob=ob->id.next) {
		if(ob->type==OB_MBALL) {
			if(ob->disp.first && ob->disp.first!=ob->disp.last) {
				DispList *dl= ob->disp.first;
				BLI_remlink(&ob->disp, dl);
				freedisplist(&ob->disp);
				BLI_addtail(&ob->disp, dl);
			}
		}
	}

	/* free database */
	materials_free(re);
	textures_free(re);
	environment_free(re);
	samplers_free(re);
	render_db_free(&re->db);

	re->cb.i.convertdone= 0;
}

static int allow_render_object(Render *re, Object *ob, int nolamps, int onlyselected, Object *actob)
{
	/* override not showing object when duplis are used with particles */
	if(ob->transflag & OB_DUPLIPARTS)
		; /* let particle system(s) handle showing vs. not showing */
	else if((ob->transflag & OB_DUPLI) && !(ob->transflag & OB_DUPLIFRAMES))
		return 0;
	
	/* don't add non-basic meta objects, ends up having renderobjects with no geometry */
	if (ob->type == OB_MBALL && ob!=find_basis_mball(re->db.scene, ob))
		return 0;
	
	if(nolamps && (ob->type==OB_LAMP))
		return 0;
	
	if(onlyselected && (ob!=actob && !(ob->flag & SELECT)))
		return 0;
	
	return 1;
}

static int allow_render_dupli_instance(Render *re, DupliObject *dob, Object *obd)
{
	ParticleSystem *psys;
	Material *ma;
	short a, *totmaterial;

	/* don't allow objects with halos. we need to have
	 * all halo's to sort them globally in advance */
	totmaterial= give_totcolp(obd);

	if(totmaterial) {
		for(a= 0; a<*totmaterial; a++) {
			ma= give_current_material(obd, a);
			if(ma && (ma->material_type == MA_TYPE_HALO))
				return 0;
		}
	}

	for(psys=obd->particlesystem.first; psys; psys=psys->next)
		if(!ELEM5(psys->part->ren_as, PART_DRAW_BB, PART_DRAW_LINE, PART_DRAW_PATH, PART_DRAW_OB, PART_DRAW_GR))
			return 0;

	/* don't allow lamp, animated duplis, or radio render */
	return (render_object_type(obd->type) &&
	        (!(dob->type == OB_DUPLIGROUP) || !dob->animated) &&
	        !(re->params.r.mode & R_RADIO));
}

static void dupli_render_particle_set(Render *re, Object *ob, int timeoffset, int level, int enable)
{
	/* ugly function, but we need to set particle systems to their render
	 * settings before calling object_duplilist, to get render level duplis */
	Group *group;
	GroupObject *go;
	ParticleSystem *psys;
	DerivedMesh *dm;

	if(level >= MAX_DUPLI_RECUR)
		return;
	
	if(ob->transflag & OB_DUPLIPARTS) {
		for(psys=ob->particlesystem.first; psys; psys=psys->next) {
			if(ELEM(psys->part->ren_as, PART_DRAW_OB, PART_DRAW_GR)) {
				if(enable)
					psys_render_set(ob, psys, re->cam.viewmat, re->cam.winmat, re->cam.winx, re->cam.winy, timeoffset);
				else
					psys_render_restore(ob, psys);
			}
		}

		if(level == 0 && enable) {
			/* this is to make sure we get render level duplis in groups:
			* the derivedmesh must be created before init_render_mesh,
			* since object_duplilist does dupliparticles before that */
			dm = mesh_create_derived_render(re->db.scene, ob, CD_MASK_BAREMESH|CD_MASK_MTFACE|CD_MASK_MCOL);
			dm->release(dm);

			for(psys=ob->particlesystem.first; psys; psys=psys->next)
				psys_get_modifier(ob, psys)->flag &= ~eParticleSystemFlag_psys_updated;
		}
	}

	if(ob->dup_group==NULL) return;
	group= ob->dup_group;

	for(go= group->gobject.first; go; go= go->next)
		dupli_render_particle_set(re, go->ob, timeoffset, level+1, enable);
}

static int get_vector_renderlayers(Scene *sce)
{
	SceneRenderLayer *srl;
	int lay= 0;

    for(srl= sce->r.layers.first; srl; srl= srl->next)
		if(srl->passflag & SCE_PASS_VECTOR)
			lay |= srl->lay;

	return lay;
}

static void add_group_render_dupli_obs(Render *re, Group *group, int nolamps, int onlyselected, Object *actob, int timeoffset, int vectorlay, int level)
{
	GroupObject *go;
	Object *ob;

	/* simple preventing of too deep nested groups */
	if(level>MAX_DUPLI_RECUR) return;

	/* recursively go into dupligroups to find objects with OB_RENDER_DUPLI
	 * that were not created yet */
	for(go= group->gobject.first; go; go= go->next) {
		ob= go->ob;

		if(ob->flag & OB_DONE) {
			if(ob->transflag & OB_RENDER_DUPLI) {
				if(allow_render_object(re, ob, nolamps, onlyselected, actob)) {
					init_render_object(re, ob, NULL, 0, timeoffset, vectorlay);
					ob->transflag &= ~OB_RENDER_DUPLI;

					if(ob->dup_group)
						add_group_render_dupli_obs(re, ob->dup_group, nolamps, onlyselected, actob, timeoffset, vectorlay, level+1);
				}
			}
		}
	}
}

static void database_init_objects(Render *re, unsigned int renderlay, int nolamps, int onlyselected, Object *actob, int timeoffset)
{
	Base *base;
	Object *ob;
	Group *group;
	ObjectInstanceRen *obi;
	Scene *sce;
	float mat[4][4];
	int lay, vectorlay, redoimat= 0;

	/* for duplis we need the Object texture mapping to work as if
	 * untransformed, set_dupli_tex_mat sets the matrix to allow that
	 * NULL is just for init */
	set_dupli_tex_mat(NULL, NULL, NULL);

	for(SETLOOPER(re->db.scene, base)) {
		ob= base->object;
		/* imat objects has to be done here, since displace can have texture using Object map-input */
		mul_m4_m4m4(mat, ob->obmat, re->cam.viewmat);
		invert_m4_m4(ob->imat, mat);
		/* each object should only be rendered once */
		ob->flag &= ~OB_DONE;
		ob->transflag &= ~OB_RENDER_DUPLI;
	}

	for(SETLOOPER(re->db.scene, base)) {
		ob= base->object;

		/* in the prev/next pass for making speed vectors, avoid creating
		 * objects that are not on a renderlayer with a vector pass, can
		 * save a lot of time in complex scenes */
		vectorlay= get_vector_renderlayers(sce);
		lay= (timeoffset)? renderlay & vectorlay: renderlay;

		/* if the object has been restricted from rendering in the outliner, ignore it */
		if(ob->restrictflag & OB_RESTRICT_RENDER) continue;

		/* OB_DONE means the object itself got duplicated, so was already converted */
		if(ob->flag & OB_DONE) {
			/* OB_RENDER_DUPLI means instances for it were already created, now
			 * it still needs to create the ObjectRen containing the data */
			if(ob->transflag & OB_RENDER_DUPLI) {
				if(allow_render_object(re, ob, nolamps, onlyselected, actob)) {
					init_render_object(re, ob, NULL, 0, timeoffset, vectorlay);
					ob->transflag &= ~OB_RENDER_DUPLI;
				}
			}
		}
		else if((base->lay & lay) || (ob->type==OB_LAMP && (base->lay & re->db.scene->lay)) ) {
			if((ob->transflag & OB_DUPLI) && (ob->type!=OB_MBALL)) {
				DupliObject *dob;
				ListBase *lb;

				redoimat= 1;

				/* create list of duplis generated by this object, particle
				 * system need to have render settings set for dupli particles */
				dupli_render_particle_set(re, ob, timeoffset, 0, 1);
				lb= object_duplilist(sce, ob);
				dupli_render_particle_set(re, ob, timeoffset, 0, 0);

				for(dob= lb->first; dob; dob= dob->next) {
					Object *obd= dob->ob;
					
					copy_m4_m4(obd->obmat, dob->mat);

					/* group duplis need to set ob matrices correct, for deform. so no_draw is part handled */
					if(!(obd->transflag & OB_RENDER_DUPLI) && dob->no_draw)
						continue;

					if(obd->restrictflag & OB_RESTRICT_RENDER)
						continue;

					if(obd->type==OB_MBALL)
						continue;

					if(!allow_render_object(re, obd, nolamps, onlyselected, actob))
						continue;

					if(allow_render_dupli_instance(re, dob, obd)) {
						ParticleSystem *psys;
						ObjectRen *obr = NULL;
						int psysindex;
						float mat[4][4];

						/* instances instead of the actual object are added in two cases, either
						 * this is a duplivert/face/particle, or it is a non-animated object in
						 * a dupligroup that has already been created before */
						if(dob->type != OB_DUPLIGROUP || (obr=find_dupligroup_dupli(re, obd, 0))) {
							mul_m4_m4m4(mat, dob->mat, re->cam.viewmat);
							obi= render_instance_create(&re->db, NULL, obd, ob, dob->index, 0, mat, obd->lay);

							/* fill in instance variables for texturing */
							set_dupli_tex_mat(re, obi, dob);
							if(dob->type != OB_DUPLIGROUP) {
								copy_v3_v3(obi->dupliorco, dob->orco);
								obi->dupliuv[0]= dob->uv[0];
								obi->dupliuv[1]= dob->uv[1];
							}
							else {
								/* for the second case, setup instance to point to the already
								 * created object, and possibly setup instances if this object
								 * itself was duplicated. for the first case find_dupli_instances
								 * will be called later. */
								assign_dupligroup_dupli(re, obi, obr);
								if(obd->transflag & OB_RENDER_DUPLI)
									find_dupli_instances(re, obr);
							}
						}
						else
							/* can't instance, just create the object */
							init_render_object(re, obd, ob, dob, timeoffset, vectorlay);

						/* same logic for particles, each particle system has it's own object, so
						 * need to go over them separately */
						psysindex= 1;
						for(psys=obd->particlesystem.first; psys; psys=psys->next) {
							if(dob->type != OB_DUPLIGROUP || (obr=find_dupligroup_dupli(re, ob, psysindex))) {
								obi= render_instance_create(&re->db, NULL, obd, ob, dob->index, psysindex++, mat, obd->lay);

								set_dupli_tex_mat(re, obi, dob);
								if(dob->type != OB_DUPLIGROUP) {
									copy_v3_v3(obi->dupliorco, dob->orco);
									obi->dupliuv[0]= dob->uv[0];
									obi->dupliuv[1]= dob->uv[1];
								}
								else {
									assign_dupligroup_dupli(re, obi, obr);
									if(obd->transflag & OB_RENDER_DUPLI)
										find_dupli_instances(re, obr);
								}
							}
						}
						
						if(dob->type != OB_DUPLIGROUP) {
							obd->flag |= OB_DONE;
							obd->transflag |= OB_RENDER_DUPLI;
						}
					}
					else
						init_render_object(re, obd, ob, dob, timeoffset, vectorlay);
					
					if(re->cb.test_break(re->cb.tbh)) break;
				}
				free_object_duplilist(lb);

				if(allow_render_object(re, ob, nolamps, onlyselected, actob))
					init_render_object(re, ob, NULL, 0, timeoffset, vectorlay);
			}
			else if(allow_render_object(re, ob, nolamps, onlyselected, actob))
				init_render_object(re, ob, NULL, 0, timeoffset, vectorlay);
		}

		if(re->cb.test_break(re->cb.tbh)) break;
	}

	/* objects in groups with OB_RENDER_DUPLI set still need to be created,
	 * since they may not be part of the scene */
	for(group= G.main->group.first; group; group=group->id.next)
		add_group_render_dupli_obs(re, group, nolamps, onlyselected, actob, timeoffset, renderlay, 0);

	/* imat objects has to be done again, since groups can mess it up */
	if(redoimat) {
		for(SETLOOPER(re->db.scene, base)) {
			ob= base->object;
			mul_m4_m4m4(mat, ob->obmat, re->cam.viewmat);
			invert_m4_m4(ob->imat, mat);
		}
	}

	if(!re->cb.test_break(re->cb.tbh))
		render_instances_init(&re->db);
}

static void render_db_preprocess(Render *re)
{
	int tothalo;
	
	/* halo sorting (don't sort stars) */
	if(!re->cb.test_break(re->cb.tbh)) {
		tothalo= re->db.tothalo;

		if(re->db.wrld.mode & WO_STARS)
			RE_make_stars(re, NULL, NULL, NULL, NULL);

		halos_sort(&re->db, tothalo);
	}
	
	/* volumes */
	init_camera_inside_volumes(re);
	
	/* shadow buffer */
	shadowbufs_make_threaded(re);
	
	/* raytree */
	if(!re->cb.test_break(re->cb.tbh))
		if(re->params.r.mode & R_RAYTRACE)
			raytree_create(re);

	/* environment maps */
	if(!re->cb.test_break(re->cb.tbh))
		envmaps_make(re);

	/* point density texture */
	if(!re->cb.test_break(re->cb.tbh))
		pointdensity_make(re, &G.main->tex);
	
	/* project into camera space */
	if(!re->cb.test_break(re->cb.tbh))
		halos_project(&re->db, &re->cam, 0, re->xparts);
	
	/* Occlusion */
	if((re->db.wrld.mode & (WO_AMB_OCC|WO_ENV_LIGHT|WO_INDIRECT_LIGHT)) && !re->cb.test_break(re->cb.tbh))
		if(re->db.wrld.ao_gather_method == WO_AOGATHER_APPROX)
			if(re->params.r.renderer==R_INTERN)
				if(re->params.r.mode & R_SHADOW)
					disk_occlusion_create(re);

	/* SSS */
	if((re->params.r.mode & R_SSS) && !re->cb.test_break(re->cb.tbh))
		if(re->params.r.renderer==R_INTERN)
			sss_create(re);
	
	/* Volumes */
	if(!re->cb.test_break(re->cb.tbh))
		if(re->params.r.mode & R_RAYTRACE)
			volume_precache_create(re);
}

/* used to be 'rotate scene' */
void RE_Database_FromScene(Render *re, Scene *scene, int use_camera_view)
{
	extern int slurph_opt;	/* key.c */
	Scene *sce;
	float mat[4][4];
	unsigned int lay;

	re->db.scene= scene;
	
	/* per second, per object, stats print this */
	re->cb.i.infostr= "Preparing Scene data";
	re->cb.i.cfra= scene->r.cfra;
	strncpy(re->cb.i.scenename, scene->id.name+2, 20);
	
	render_db_init(&re->db);

	slurph_opt= 0;
	re->cb.i.partsdone= 0;	/* signal now in use for previewrender */
	
	/* in localview, lamps are using normal layers, objects only local bits */
	if(re->db.scene->lay & 0xFF000000) lay= re->db.scene->lay & 0xFF000000;
	else lay= re->db.scene->lay;
	
	/* applies changes fully */
	if((re->params.r.scemode & R_PREVIEWBUTS)==0)
		scene_update_for_newframe(re->db.scene, lay);
	
	/* if no camera, viewmat should have been set! */
	if(use_camera_view && re->db.scene->camera) {
		normalize_m4(re->db.scene->camera->obmat);
		invert_m4_m4(mat, re->db.scene->camera->obmat);
		RE_SetView(re, mat);
		re->db.scene->camera->recalc= OB_RECALC_OB; /* force correct matrix for scaled cameras */
	}
	
	/* do first, because of ambient. also requires re->params.osa set correct */
	environment_init(re, re->db.scene->world);
	samplers_init(re);
	materials_init(re);
	textures_init(re);

	/* make render objects */
	database_init_objects(re, lay, 0, 0, 0, 0);
	
	if(!re->cb.test_break(re->cb.tbh)) {
		set_material_lightgroups(re);
		for(sce= re->db.scene; sce; sce= sce->set)
			set_renderlayer_lightgroups(re, sce);
		
		slurph_opt= 1;
		
		/* for now some clumsy copying still */
		re->cb.i.totvert= re->db.totvert;
		re->cb.i.totface= re->db.totvlak;
		re->cb.i.totstrand= re->db.totstrand;
		re->cb.i.tothalo= re->db.tothalo;
		re->cb.i.totlamp= re->db.totlamp;
		re->cb.stats_draw(re->cb.sdh, &re->cb.i);

		/* do DB preprocessing */
		render_db_preprocess(re);
	}
	
	if(re->cb.test_break(re->cb.tbh))
		RE_Database_Free(re);
	else
		re->cb.i.convertdone= 1;
	
	re->cb.i.infostr= NULL;
	re->cb.stats_draw(re->cb.sdh, &re->cb.i);
}

/* exported call to recalculate hoco for vertices, when winmat changed */
void RE_DataBase_ApplyWindow(Render *re)
{
	halos_project(&re->db, &re->cam, 0, re->xparts);
}

void RE_DataBase_GetView(Render *re, float mat[][4])
{
	copy_m4_m4(mat, re->cam.viewmat);
}

/* ------------------------------------------------------------------------- */
/* Speed Vectors															 */
/* ------------------------------------------------------------------------- */

static void database_fromscene_vectors(Render *re, Scene *scene, int timeoffset)
{
	extern int slurph_opt;	/* key.c */
	float mat[4][4];
	unsigned int lay;
	
	re->db.scene= scene;
	
	render_db_init(&re->db);

	re->cb.i.totface=re->cb.i.totvert=re->cb.i.totstrand=re->cb.i.totlamp=re->cb.i.tothalo= 0;

	slurph_opt= 0;
	
	/* in localview, lamps are using normal layers, objects only local bits */
	if(re->db.scene->lay & 0xFF000000) lay= re->db.scene->lay & 0xFF000000;
	else lay= re->db.scene->lay;
	
	/* applies changes fully */
	scene->r.cfra += timeoffset;
	scene_update_for_newframe(re->db.scene, lay);
	
	/* if no camera, viewmat should have been set! */
	if(re->db.scene->camera) {
		normalize_m4(re->db.scene->camera->obmat);
		invert_m4_m4(mat, re->db.scene->camera->obmat);
		RE_SetView(re, mat);
	}
	
	/* make render objects */
	database_init_objects(re, lay, 0, 0, 0, timeoffset);
	
	/* do this in end, particles for example need cfra */
	scene->r.cfra -= timeoffset;
}

/* choose to use static, to prevent giving too many args to this call */
static void speedvector_project(Render *re, float *zco, float *co, float *ho)
{
	static float pixelphix=0.0f, pixelphiy=0.0f, zmulx=0.0f, zmuly=0.0f;
	static int pano= 0;
	float div;
	
	/* initialize */
	if(re) {
		pano= re->cam.type == R_CAM_PANO;
		
		/* precalculate amount of radians 1 pixel rotates */
		if(pano) {
			/* size of 1 pixel mapped to viewplane coords */
			float psize= (re->cam.viewplane.xmax-re->cam.viewplane.xmin)/(float)re->cam.winx;
			/* x angle of a pixel */
			pixelphix= atan(psize/re->cam.clipsta);
			
			psize= (re->cam.viewplane.ymax-re->cam.viewplane.ymin)/(float)re->cam.winy;
			/* y angle of a pixel */
			pixelphiy= atan(psize/re->cam.clipsta);
		}
		zmulx= re->cam.winx/2;
		zmuly= re->cam.winy/2;
		
		return;
	}
	
	/* now map hocos to screenspace, uses very primitive clip still */
	if(ho[3]<0.1f) div= 10.0f;
	else div= 1.0f/ho[3];
	
	/* use cylinder projection */
	if(pano) {
		float vec[3], ang;
		/* angle between (0,0,-1) and (co) */
		copy_v3_v3(vec, co);

		ang= saacos(-vec[2]/sqrt(vec[0]*vec[0] + vec[2]*vec[2]));
		if(vec[0]<0.0f) ang= -ang;
		zco[0]= ang/pixelphix + zmulx;
		
		ang= 0.5f*M_PI - saacos(vec[1]/sqrt(vec[0]*vec[0] + vec[1]*vec[1] + vec[2]*vec[2]));
		zco[1]= ang/pixelphiy + zmuly;
		
	}
	else {
		zco[0]= zmulx*(1.0f+ho[0]*div);
		zco[1]= zmuly*(1.0f+ho[1]*div);
	}
}

static void calculate_speedvector(float *vectors, int step, float winsq, float winroot, float *co, float *ho, float *speed)
{
	float zco[2], len;

	speedvector_project(NULL, zco, co, ho);
	
	zco[0]= vectors[0] - zco[0];
	zco[1]= vectors[1] - zco[1];
	
	/* enable nice masks for hardly moving stuff or float inaccuracy */
	if(zco[0]<0.1f && zco[0]>-0.1f && zco[1]<0.1f && zco[1]>-0.1f ) {
		zco[0]= 0.0f;
		zco[1]= 0.0f;
	}
	
	/* maximize speed for image width, otherwise it never looks good */
	len= zco[0]*zco[0] + zco[1]*zco[1];
	if(len > winsq) {
		len= winroot/sqrt(len);
		zco[0]*= len;
		zco[1]*= len;
	}
	
	/* note; in main vecblur loop speedvec is negated again */
	if(step) {
		speed[2]= -zco[0];
		speed[3]= -zco[1];
	}
	else {
		speed[0]= zco[0];
		speed[1]= zco[1];
	}
}

static float *calculate_surfacecache_speedvectors(Render *re, ObjectInstanceRen *obi, SurfaceCache *mesh)
{
	float winsq= re->cam.winx*re->cam.winy, winroot= sqrt(winsq), (*winspeed)[4];
	float ho[4], prevho[4], nextho[4], winmat[4][4], vec[2];
	int a;

	if(mesh->co && mesh->prevco && mesh->nextco) {
		if(obi->flag & R_TRANSFORMED)
			mul_m4_m4m4(winmat, obi->mat, re->cam.winmat);
		else
			copy_m4_m4(winmat, re->cam.winmat);

		winspeed= MEM_callocN(sizeof(float)*4*mesh->totvert, "StrandSurfWin");

		for(a=0; a<mesh->totvert; a++) {
			camera_matrix_co_to_hoco(winmat, ho, mesh->co[a]);

			camera_matrix_co_to_hoco(winmat, prevho, mesh->prevco[a]);
			speedvector_project(NULL, vec, mesh->prevco[a], prevho);
			calculate_speedvector(vec, 0, winsq, winroot, mesh->co[a], ho, winspeed[a]);

			camera_matrix_co_to_hoco(winmat, nextho, mesh->nextco[a]);
			speedvector_project(NULL, vec, mesh->nextco[a], nextho);
			calculate_speedvector(vec, 1, winsq, winroot, mesh->co[a], ho, winspeed[a]);
		}

		return (float*)winspeed;
	}

	return NULL;
}

static void calculate_speedvectors(Render *re, ObjectInstanceRen *obi, float *vectors, int step)
{
	ObjectRen *obr= obi->obr;
	VertRen *ver= NULL;
	StrandRen *strand= NULL;
	StrandBuffer *strandbuf;
	SurfaceCache *mesh= NULL;
	float *speed, (*winspeed)[4]=NULL, ho[4], winmat[4][4];
	float *co1, *co2, *co3, *co4, w[4];
	float winsq= re->cam.winx*re->cam.winy, winroot= sqrt(winsq);
	int a, *face, *index;

	if(obi->flag & R_TRANSFORMED)
		mul_m4_m4m4(winmat, obi->mat, re->cam.winmat);
	else
		copy_m4_m4(winmat, re->cam.winmat);

	if(obr->vertnodes) {
		for(a=0; a<obr->totvert; a++, vectors+=2) {
			if((a & 255)==0) ver= obr->vertnodes[a>>8].vert;
			else ver++;

			speed= render_vert_get_winspeed(obi, ver, 1);
			camera_matrix_co_to_hoco(winmat, ho, ver->co);
			calculate_speedvector(vectors, step, winsq, winroot, ver->co, ho, speed);
		}
	}

	if(obr->strandnodes) {
		strandbuf= obr->strandbuf;
		mesh= (strandbuf)? strandbuf->surface: NULL;

		/* compute speed vectors at surface vertices */
		if(mesh)
			winspeed= (float(*)[4])calculate_surfacecache_speedvectors(re, obi, mesh);

		if(winspeed) {
			for(a=0; a<obr->totstrand; a++, vectors+=2) {
				if((a & 255)==0) strand= obr->strandnodes[a>>8].strand;
				else strand++;

				index= render_strand_get_face(obr, strand, 0);
				if(index && *index < mesh->totface) {
					speed= render_strand_get_winspeed(obi, strand, 1);

					/* interpolate speed vectors from strand surface */
					face= mesh->face[*index];

					co1= mesh->co[face[0]];
					co2= mesh->co[face[1]];
					co3= mesh->co[face[2]];
					co4= (face[3])? mesh->co[face[3]]: NULL;

					interp_weights_face_v3( w,co1, co2, co3, co4, strand->vert->co);

					speed[0]= speed[1]= speed[2]= speed[3]= 0.0f;
					QUATADDFAC(speed, speed, winspeed[face[0]], w[0]);
					QUATADDFAC(speed, speed, winspeed[face[1]], w[1]);
					QUATADDFAC(speed, speed, winspeed[face[2]], w[2]);
					if(face[3])
						QUATADDFAC(speed, speed, winspeed[face[3]], w[3]);
				}
			}

			MEM_freeN(winspeed);
		}
	}
}

static int load_fluidsimspeedvectors(Render *re, ObjectInstanceRen *obi, float *vectors, int step)
{
	ObjectRen *obr= obi->obr;
	Object *fsob= obr->ob;
	VertRen *ver= NULL;
	float *speed, div, zco[2], avgvel[4] = {0.0, 0.0, 0.0, 0.0};
	float zmulx= re->cam.winx/2, zmuly= re->cam.winy/2, len;
	float winsq= re->cam.winx*re->cam.winy, winroot= sqrt(winsq);
	int a, j;
	float hoco[4], ho[4], fsvec[4], camco[4];
	float mat[4][4], winmat[4][4];
	float imat[4][4];
	FluidsimModifierData *fluidmd = (FluidsimModifierData *)modifiers_findByType(fsob, eModifierType_Fluidsim);
	FluidsimSettings *fss;
	float *velarray = NULL;
	
	/* only one step needed */
	if(step) return 1;
	
	if(fluidmd)
		fss = fluidmd->fss;
	else
		return 0;
	
	copy_m4_m4(mat, re->cam.viewmat);
	invert_m4_m4(imat, mat);

	/* set first vertex OK */
	if(!fss->meshSurfNormals) return 0;
	
	if( obr->totvert != GET_INT_FROM_POINTER(fss->meshSurface) ) {
		//fprintf(stderr, "load_fluidsimspeedvectors - modified fluidsim mesh, not using speed vectors (%d,%d)...\n", obr->totvert, fsob->fluidsimSettings->meshSurface->totvert); // DEBUG
		return 0;
	}
	
	velarray = (float *)fss->meshSurfNormals;

	if(obi->flag & R_TRANSFORMED)
		mul_m4_m4m4(winmat, obi->mat, re->cam.winmat);
	else
		copy_m4_m4(winmat, re->cam.winmat);
	
	/* (bad) HACK calculate average velocity */
	/* better solution would be fixing getVelocityAt() in intern/elbeem/intern/solver_util.cpp
	so that also small drops/little water volumes return a velocity != 0. 
	But I had no luck in fixing that function - DG */
	for(a=0; a<obr->totvert; a++) {
		for(j=0;j<3;j++) avgvel[j] += velarray[3*a + j];
		
	}
	for(j=0;j<3;j++) avgvel[j] /= (float)(obr->totvert);
	
	
	for(a=0; a<obr->totvert; a++, vectors+=2) {
		if((a & 255)==0)
			ver= obr->vertnodes[a>>8].vert;
		else
			ver++;

		// get fluid velocity
		fsvec[3] = 0.; 
		//fsvec[0] = fsvec[1] = fsvec[2] = fsvec[3] = 0.; fsvec[2] = 2.; // NT fixed test
		for(j=0;j<3;j++) fsvec[j] = velarray[3*a + j];
		
		/* (bad) HACK insert average velocity if none is there (see previous comment) */
		if((fsvec[0] == 0.0) && (fsvec[1] == 0.0) && (fsvec[2] == 0.0))
		{
			fsvec[0] = avgvel[0];
			fsvec[1] = avgvel[1];
			fsvec[2] = avgvel[2];
		}
		
		// transform (=rotate) to cam space
		camco[0]= imat[0][0]*fsvec[0] + imat[0][1]*fsvec[1] + imat[0][2]*fsvec[2];
		camco[1]= imat[1][0]*fsvec[0] + imat[1][1]*fsvec[1] + imat[1][2]*fsvec[2];
		camco[2]= imat[2][0]*fsvec[0] + imat[2][1]*fsvec[1] + imat[2][2]*fsvec[2];

		// get homogenous coordinates
		camera_matrix_co_to_hoco(winmat, hoco, camco);
		camera_matrix_co_to_hoco(winmat, ho, ver->co);
		
		/* now map hocos to screenspace, uses very primitive clip still */
		// use ho[3] of original vertex, xy component of vel. direction
		if(ho[3]<0.1f) div= 10.0f;
		else div= 1.0f/ho[3];
		zco[0]= zmulx*hoco[0]*div;
		zco[1]= zmuly*hoco[1]*div;
		
		// maximize speed as usual
		len= zco[0]*zco[0] + zco[1]*zco[1];
		if(len > winsq) {
			len= winroot/sqrt(len);
			zco[0]*= len; zco[1]*= len;
		}
		
		speed= render_vert_get_winspeed(obi, ver, 1);
		// set both to the same value
		speed[0]= speed[2]= zco[0];
		speed[1]= speed[3]= zco[1];
		//if(a<20) fprintf(stderr,"speed %d %f,%f | camco %f,%f,%f | hoco %f,%f,%f,%f  \n", a, speed[0], speed[1], camco[0],camco[1], camco[2], hoco[0],hoco[1], hoco[2],hoco[3]); // NT DEBUG
	}

	return 1;
}

/* makes copy per object of all vectors */
/* result should be that we can free entire database */
static void copy_dbase_object_vectors(Render *re, ListBase *lb)
{
	ObjectInstanceRen *obi, *obilb;
	ObjectRen *obr;
	VertRen *ver= NULL;
	float *vec, ho[4], winmat[4][4];
	int a, totvector;

	for(obi= re->db.instancetable.first; obi; obi= obi->next) {
		obr= obi->obr;

		obilb= MEM_mallocN(sizeof(ObjectInstanceRen), "ObInstanceVector");
		memcpy(obilb, obi, sizeof(ObjectInstanceRen));
		BLI_addtail(lb, obilb);

		obilb->totvector= totvector= obr->totvert;

		if(totvector > 0) {
			vec= obilb->vectors= MEM_mallocN(2*sizeof(float)*totvector, "vector array");

			if(obi->flag & R_TRANSFORMED)
				mul_m4_m4m4(winmat, obi->mat, re->cam.winmat);
			else
				copy_m4_m4(winmat, re->cam.winmat);

			for(a=0; a<obr->totvert; a++, vec+=2) {
				if((a & 255)==0) ver= obr->vertnodes[a>>8].vert;
				else ver++;
				
				camera_matrix_co_to_hoco(winmat, ho, ver->co);
				speedvector_project(NULL, vec, ver->co, ho);
			}
		}
	}
}

static void free_dbase_object_vectors(ListBase *lb)
{
	ObjectInstanceRen *obi;
	
	for(obi= lb->first; obi; obi= obi->next)
		if(obi->vectors)
			MEM_freeN(obi->vectors);
	BLI_freelistN(lb);
}

void RE_Database_FromScene_Vectors(Render *re, Scene *sce)
{
	ObjectInstanceRen *obi, *oldobi;
	SurfaceCache *mesh;
	ListBase *table;
	ListBase oldtable= {NULL, NULL}, newtable= {NULL, NULL};
	ListBase surfacecache;
	int step;
	
	re->cb.i.infostr= "Calculating previous vectors";
	re->params.r.mode |= R_SPEED;
	
	speedvector_project(re, NULL, NULL, NULL);	/* initializes projection code */
	
	/* creates entire dbase */
	database_fromscene_vectors(re, sce, -1);
	
	/* copy away vertex info */
	copy_dbase_object_vectors(re, &oldtable);
		
	/* free dbase and make the future one */
	surfacecache= re->db.surfacecache;
	memset(&re->db.surfacecache, 0, sizeof(ListBase));
	RE_Database_Free(re);
	re->db.surfacecache= surfacecache;
	
	if(!re->cb.test_break(re->cb.tbh)) {
		/* creates entire dbase */
		re->cb.i.infostr= "Calculating next frame vectors";
		
		database_fromscene_vectors(re, sce, +1);
	}	
	/* copy away vertex info */
	copy_dbase_object_vectors(re, &newtable);
	
	/* free dbase and make the real one */
	surfacecache= re->db.surfacecache;
	memset(&re->db.surfacecache, 0, sizeof(ListBase));
	RE_Database_Free(re);
	re->db.surfacecache= surfacecache;
	
	if(!re->cb.test_break(re->cb.tbh))
		RE_Database_FromScene(re, sce, 1);
	
	if(!re->cb.test_break(re->cb.tbh)) {
		for(step= 0; step<2; step++) {
			
			if(step)
				table= &newtable;
			else
				table= &oldtable;
			
			oldobi= table->first;
			for(obi= re->db.instancetable.first; obi && oldobi; obi= obi->next) {
				int ok= 1;
				FluidsimModifierData *fluidmd;

				if(!(obi->obr->flag & R_NEED_VECTORS))
					continue;

				obi->totvector= obi->obr->totvert;

				/* find matching object in old table */
				if(oldobi->ob!=obi->ob || oldobi->par!=obi->par || oldobi->index!=obi->index || oldobi->psysindex!=obi->psysindex) {
					ok= 0;
					for(oldobi= table->first; oldobi; oldobi= oldobi->next)
						if(oldobi->ob==obi->ob && oldobi->par==obi->par && oldobi->index==obi->index && oldobi->psysindex==obi->psysindex)
							break;
					if(oldobi==NULL)
						oldobi= table->first;
					else
						ok= 1;
				}
				if(ok==0) {
					 printf("speed table: missing object %s\n", obi->ob->id.name+2);
					continue;
				}

				// NT check for fluidsim special treatment
				fluidmd = (FluidsimModifierData *)modifiers_findByType(obi->ob, eModifierType_Fluidsim);
				if(fluidmd && fluidmd->fss && (fluidmd->fss->type & OB_FLUIDSIM_DOMAIN)) {
					// use preloaded per vertex simulation data , only does calculation for step=1
					// NOTE/FIXME - velocities and meshes loaded unnecessarily often during the database_fromscene_vectors calls...
					load_fluidsimspeedvectors(re, obi, oldobi->vectors, step);
				}
				else {
					/* check if both have same amounts of vertices */
					if(obi->totvector==oldobi->totvector)
						calculate_speedvectors(re, obi, oldobi->vectors, step);
					else
						printf("Warning: object %s has different amount of vertices or strands on other frame\n", obi->ob->id.name+2);
				} // not fluidsim

				oldobi= oldobi->next;
			}
		}
	}
	
	free_dbase_object_vectors(&oldtable);
	free_dbase_object_vectors(&newtable);

	for(mesh=re->db.surfacecache.first; mesh; mesh=mesh->next) {
		if(mesh->prevco) {
			MEM_freeN(mesh->prevco);
			mesh->prevco= NULL;
		}
		if(mesh->nextco) {
			MEM_freeN(mesh->nextco);
			mesh->nextco= NULL;
		}
	}
	
	re->cb.i.infostr= NULL;
	re->cb.stats_draw(re->cb.sdh, &re->cb.i);
}


/* ------------------------------------------------------------------------- */
/* Baking																	 */
/* ------------------------------------------------------------------------- */

/* setup for shaded view or bake, so only lamps and materials are initialized */
/* type:
   RE_BAKE_LIGHT:  for shaded view, only add lamps
   RE_BAKE_ALL:    for baking, all lamps and objects
   RE_BAKE_NORMALS:for baking, no lamps and only selected objects
   RE_BAKE_AO:     for baking, no lamps, but all objects
   RE_BAKE_TEXTURE:for baking, no lamps, only selected objects
   RE_BAKE_DISPLACEMENT:for baking, no lamps, only selected objects
   RE_BAKE_SHADOW: for baking, only shadows, but all objects
*/
void RE_Database_Baking(Render *re, Scene *scene, int type, Object *actob)
{
	float mat[4][4];
	unsigned int lay;
	int onlyselected, nolamps;
	
	re->db.scene= scene;

	/* renderdata setup and exceptions */
	re->params.r= scene->r;
	
	RE_init_threadcount(re);
	
	re->params.flag |= R_GLOB_NOPUNOFLIP;
	re->params.flag |= R_BAKING;
	re->db.excludeob= actob;
	if(actob)
		re->params.flag |= R_BAKE_TRACE;

	if(type==RE_BAKE_NORMALS && re->params.r.bake_normal_space==R_BAKE_SPACE_TANGENT)
		re->params.flag |= R_NEED_TANGENT;
	
	if(!actob && ELEM4(type, RE_BAKE_LIGHT, RE_BAKE_NORMALS, RE_BAKE_TEXTURE, RE_BAKE_DISPLACEMENT)) {
		re->params.r.mode &= ~R_SHADOW;
		re->params.r.mode &= ~R_RAYTRACE;
	}
	
	if(!actob && (type==RE_BAKE_SHADOW)) {
		re->params.r.mode |= R_SHADOW;
	}
	
	/* setup render stuff */
	re->db.memArena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE);
	
	re->db.totvlak=re->db.totvert=re->db.totstrand=re->db.totlamp=re->db.tothalo= 0;
	re->db.lights.first= re->db.lights.last= NULL;
	re->db.lampren.first= re->db.lampren.last= NULL;

	/* if no camera, set unit */
	if(re->db.scene->camera) {
		normalize_m4(re->db.scene->camera->obmat);
		invert_m4_m4(mat, re->db.scene->camera->obmat);
		RE_SetView(re, mat);
	}
	else {
		unit_m4(mat);
		RE_SetView(re, mat);
	}
	
	/* do first, because of ambient. also requires re->params.osa set correct */
	environment_init(re, re->db.scene->world);
	samplers_init(re);
	materials_init(re);
	textures_init(re);
	
	/* MAKE RENDER DATA */
	nolamps= !ELEM3(type, RE_BAKE_LIGHT, RE_BAKE_ALL, RE_BAKE_SHADOW);
	onlyselected= ELEM3(type, RE_BAKE_NORMALS, RE_BAKE_TEXTURE, RE_BAKE_DISPLACEMENT);

	/* in localview, lamps are using normal layers, objects only local bits */
	if(re->db.scene->lay & 0xFF000000) lay= re->db.scene->lay & 0xFF000000;
	else lay= re->db.scene->lay;
	
	database_init_objects(re, lay, nolamps, onlyselected, actob, 0);

	set_material_lightgroups(re);

	/* shadow buffer */
	if(type!=RE_BAKE_LIGHT)
		shadowbufs_make_threaded(re);

	/* raytree */
	if(!re->cb.test_break(re->cb.tbh))
		if(re->params.r.mode & R_RAYTRACE)
			raytree_create(re);
	
	/* occlusion */
	if((re->db.wrld.mode & (WO_AMB_OCC|WO_ENV_LIGHT|WO_INDIRECT_LIGHT)) && !re->cb.test_break(re->cb.tbh))
		if(re->db.wrld.ao_gather_method == WO_AOGATHER_APPROX)
			if(re->params.r.mode & R_SHADOW)
				disk_occlusion_create(re);
}


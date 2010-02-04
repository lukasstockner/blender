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

#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_material_types.h"
#include "DNA_curve_types.h"
#include "DNA_effect_types.h"
#include "DNA_group_types.h"
#include "DNA_lamp_types.h"
#include "DNA_image_types.h"
#include "DNA_lattice_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_meta_types.h"
#include "DNA_modifier_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_object_force.h"
#include "DNA_object_fluidsim.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h"
#include "DNA_texture_types.h"
#include "DNA_view3d_types.h"

#include "BKE_anim.h"
#include "BKE_armature.h"
#include "BKE_action.h"
#include "BKE_curve.h"
#include "BKE_customdata.h"
#include "BKE_colortools.h"
#include "BKE_constraint.h"
#include "BKE_displist.h"
#include "BKE_deform.h"
#include "BKE_DerivedMesh.h"
#include "BKE_effect.h"
#include "BKE_global.h"
#include "BKE_group.h"
#include "BKE_key.h"
#include "BKE_ipo.h"
#include "BKE_image.h"
#include "BKE_lattice.h"
#include "BKE_library.h"
#include "BKE_material.h"
#include "BKE_main.h"
#include "BKE_mball.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_node.h"
#include "BKE_object.h"
#include "BKE_particle.h"
#include "BKE_scene.h"
#include "BKE_subsurf.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"
#include "BKE_world.h"

#include "PIL_time.h"
#include "IMB_imbuf_types.h"

#include "camera.h"
#include "database.h"
#include "environment.h"
#include "envmap.h"
#include "lamp.h"
#include "material.h"
#include "object.h"
#include "object_halo.h"
#include "object_mesh.h"
#include "render_types.h"
#include "rendercore.h"
#include "shading.h"
#include "shadowbuf.h"
#include "sss.h"
#include "object_strand.h"
#include "object_strand.h"
#include "texture.h"
#include "texture_stack.h"
#include "volume_precache.h"
#include "zbuf.h"

/* 10 times larger than normal epsilon, test it on default nurbs sphere with ray_transp (for quad detection) */
/* or for checking vertex normal flips */
#define FLT_EPSILON10 1.19209290e-06F

/******************************** Verts **************************************/

float *render_vert_get_sticky(ObjectRen *obr, VertRen *ver, int verify)
{
	float *sticky;
	int nr= ver->index>>8;
	
	sticky= obr->vertnodes[nr].sticky;
	if(sticky==NULL) {
		if(verify) 
			sticky= obr->vertnodes[nr].sticky= MEM_mallocN(256*RE_STICKY_ELEMS*sizeof(float), "sticky table");
		else
			return NULL;
	}
	return sticky + (ver->index & 255)*RE_STICKY_ELEMS;
}

float *render_vert_get_stress(ObjectRen *obr, VertRen *ver, int verify)
{
	float *stress;
	int nr= ver->index>>8;
	
	stress= obr->vertnodes[nr].stress;
	if(stress==NULL) {
		if(verify) 
			stress= obr->vertnodes[nr].stress= MEM_mallocN(256*RE_STRESS_ELEMS*sizeof(float), "stress table");
		else
			return NULL;
	}
	return stress + (ver->index & 255)*RE_STRESS_ELEMS;
}

/* this one callocs! */
float *render_vert_get_rad(ObjectRen *obr, VertRen *ver, int verify)
{
	float *rad;
	int nr= ver->index>>8;
	
	rad= obr->vertnodes[nr].rad;
	if(rad==NULL) {
		if(verify) 
			rad= obr->vertnodes[nr].rad= MEM_callocN(256*RE_RAD_ELEMS*sizeof(float), "rad table");
		else
			return NULL;
	}
	return rad + (ver->index & 255)*RE_RAD_ELEMS;
}

float *render_vert_get_strand(ObjectRen *obr, VertRen *ver, int verify)
{
	float *strand;
	int nr= ver->index>>8;
	
	strand= obr->vertnodes[nr].strand;
	if(strand==NULL) {
		if(verify) 
			strand= obr->vertnodes[nr].strand= MEM_mallocN(256*RE_STRAND_ELEMS*sizeof(float), "strand table");
		else
			return NULL;
	}
	return strand + (ver->index & 255)*RE_STRAND_ELEMS;
}

/* needs calloc */
float *render_vert_get_tangent(ObjectRen *obr, VertRen *ver, int verify)
{
	float *tangent;
	int nr= ver->index>>8;
	
	tangent= obr->vertnodes[nr].tangent;
	if(tangent==NULL) {
		if(verify) 
			tangent= obr->vertnodes[nr].tangent= MEM_callocN(256*RE_TANGENT_ELEMS*sizeof(float), "tangent table");
		else
			return NULL;
	}
	return tangent + (ver->index & 255)*RE_TANGENT_ELEMS;
}

/* needs calloc! not all renderverts have them */
/* also winspeed is exception, it is stored per instance */
float *render_vert_get_winspeed(ObjectInstanceRen *obi, VertRen *ver, int verify)
{
	float *winspeed;
	int totvector;
	
	winspeed= obi->vectors;
	if(winspeed==NULL) {
		if(verify) {
			totvector= obi->obr->totvert + obi->obr->totstrand;
			winspeed= obi->vectors= MEM_callocN(totvector*RE_WINSPEED_ELEMS*sizeof(float), "winspeed table");
		}
		else
			return NULL;
	}
	return winspeed + ver->index*RE_WINSPEED_ELEMS;
}

VertRen *render_object_vert_copy(ObjectRen *obr, VertRen *ver)
{
	VertRen *v1= render_object_vert_get(obr, obr->totvert++);
	float *fp1, *fp2;
	int index= v1->index;
	
	*v1= *ver;
	v1->index= index;
	
	fp1= render_vert_get_sticky(obr, ver, 0);
	if(fp1) {
		fp2= render_vert_get_sticky(obr, v1, 1);
		memcpy(fp2, fp1, RE_STICKY_ELEMS*sizeof(float));
	}
	fp1= render_vert_get_stress(obr, ver, 0);
	if(fp1) {
		fp2= render_vert_get_stress(obr, v1, 1);
		memcpy(fp2, fp1, RE_STRESS_ELEMS*sizeof(float));
	}
	fp1= render_vert_get_rad(obr, ver, 0);
	if(fp1) {
		fp2= render_vert_get_rad(obr, v1, 1);
		memcpy(fp2, fp1, RE_RAD_ELEMS*sizeof(float));
	}
	fp1= render_vert_get_strand(obr, ver, 0);
	if(fp1) {
		fp2= render_vert_get_strand(obr, v1, 1);
		memcpy(fp2, fp1, RE_STRAND_ELEMS*sizeof(float));
	}
	fp1= render_vert_get_tangent(obr, ver, 0);
	if(fp1) {
		fp2= render_vert_get_tangent(obr, v1, 1);
		memcpy(fp2, fp1, RE_TANGENT_ELEMS*sizeof(float));
	}
	return v1;
}

VertRen *render_object_vert_get(ObjectRen *obr, int nr)
{
	VertRen *v;
	int a;

	a= render_object_chunk_get((void**)&obr->vertnodes, &obr->vertnodeslen, nr, sizeof(VertTableNode));
	v= obr->vertnodes[a].vert;

	if(v == NULL) {
		int i;
		
		v= (VertRen *)MEM_callocN(256*sizeof(VertRen),"findOrAddVert");
		obr->vertnodes[a].vert= v;
		
		for(i= (nr & 0xFFFFFF00), a=0; a<256; a++, i++)
			v[a].index= i;
	}

	return v + (nr & 255);
}

/******************************** Vlaks **************************************/

MTFace *render_vlak_get_tface(ObjectRen *obr, VlakRen *vlr, int n, char **name, int verify)
{
	VlakTableNode *node;
	int nr= vlr->index>>8, vlakindex= (vlr->index&255);
	int index= (n<<8) + vlakindex;

	node= &obr->vlaknodes[nr];

	if(verify) {
		if(n>=node->totmtface) {
			MTFace *mtface= node->mtface;
			int size= size= (n+1)*256;

			node->mtface= MEM_callocN(size*sizeof(MTFace), "Vlak mtface");

			if(mtface) {
				size= node->totmtface*256;
				memcpy(node->mtface, mtface, size*sizeof(MTFace));
				MEM_freeN(mtface);
			}

			node->totmtface= n+1;
		}
	}
	else {
		if(n>=node->totmtface)
			return NULL;

		if(name) *name= obr->mtface[n];
	}

	return node->mtface + index;
}

MCol *render_vlak_get_mcol(ObjectRen *obr, VlakRen *vlr, int n, char **name, int verify)
{
	VlakTableNode *node;
	int nr= vlr->index>>8, vlakindex= (vlr->index&255);
	int index= (n<<8) + vlakindex;

	node= &obr->vlaknodes[nr];

	if(verify) {
		if(n>=node->totmcol) {
			MCol *mcol= node->mcol;
			int size= (n+1)*256;

			node->mcol= MEM_callocN(size*sizeof(MCol)*RE_MCOL_ELEMS, "Vlak mcol");

			if(mcol) {
				size= node->totmcol*256;
				memcpy(node->mcol, mcol, size*sizeof(MCol)*RE_MCOL_ELEMS);
				MEM_freeN(mcol);
			}

			node->totmcol= n+1;
		}
	}
	else {
		if(n>=node->totmcol)
			return NULL;

		if(name) *name= obr->mcol[n];
	}

	return node->mcol + index*RE_MCOL_ELEMS;
}

float *render_vlak_get_surfnor(ObjectRen *obr, VlakRen *vlak, int verify)
{
	float *surfnor;
	int nr= vlak->index>>8;
	
	surfnor= obr->vlaknodes[nr].surfnor;
	if(surfnor==NULL) {
		if(verify) 
			surfnor= obr->vlaknodes[nr].surfnor= MEM_callocN(256*RE_SURFNOR_ELEMS*sizeof(float), "surfnor table");
		else
			return NULL;
	}
	return surfnor + (vlak->index & 255)*RE_SURFNOR_ELEMS;
}

float *render_vlak_get_nmap_tangent(ObjectRen *obr, VlakRen *vlak, int verify)
{
	float *tangent;
	int nr= vlak->index>>8;

	tangent= obr->vlaknodes[nr].tangent;
	if(tangent==NULL) {
		if(verify) 
			tangent= obr->vlaknodes[nr].tangent= MEM_callocN(256*RE_NMAP_TANGENT_ELEMS*sizeof(float), "tangent table");
		else
			return NULL;
	}
	return tangent + (vlak->index & 255)*RE_NMAP_TANGENT_ELEMS;
}

VlakRen *render_object_vlak_copy(ObjectRen *obr, VlakRen *vlr)
{
	VlakRen *vlr1 = render_object_vlak_get(obr, obr->totvlak++);
	MTFace *mtface, *mtface1;
	MCol *mcol, *mcol1;
	float *surfnor, *surfnor1, *tangent, *tangent1;
	int i, index = vlr1->index;
	char *name;

	*vlr1= *vlr;
	vlr1->index= index;

	for (i=0; (mtface=render_vlak_get_tface(obr, vlr, i, &name, 0)) != NULL; i++) {
		mtface1= render_vlak_get_tface(obr, vlr1, i, &name, 1);
		memcpy(mtface1, mtface, sizeof(MTFace)*RE_MTFACE_ELEMS);
	}

	for (i=0; (mcol=render_vlak_get_mcol(obr, vlr, i, &name, 0)) != NULL; i++) {
		mcol1= render_vlak_get_mcol(obr, vlr1, i, &name, 1);
		memcpy(mcol1, mcol, sizeof(MCol)*RE_MCOL_ELEMS);
	}

	surfnor= render_vlak_get_surfnor(obr, vlr, 0);
	if(surfnor) {
		surfnor1= render_vlak_get_surfnor(obr, vlr1, 1);
		copy_v3_v3(surfnor1, surfnor);
	}

	tangent= render_vlak_get_nmap_tangent(obr, vlr, 0);
	if(tangent) {
		tangent1= render_vlak_get_nmap_tangent(obr, vlr1, 1);
		memcpy(tangent1, tangent, sizeof(float)*RE_NMAP_TANGENT_ELEMS);
	}

	return vlr1;
}

int render_vlak_get_normal(ObjectInstanceRen *obi, VlakRen *vlr, float *nor)
{
	float v1[3], (*nmat)[3]= obi->nmat;
	int flipped= 0;

	if(obi->flag & R_TRANSFORMED) {
		copy_v3_v3(nor, vlr->n);
		
		mul_m3_v3(nmat, nor);
		normalize_v3(nor);
	}
	else
		copy_v3_v3(nor, vlr->n);

	if((vlr->flag & R_NOPUNOFLIP)==0) {
		copy_v3_v3(v1, vlr->v1->co);
		if(obi->flag & R_TRANSFORMED)
			mul_m4_v3(obi->mat, v1);

		if(dot_v3v3(v1, nor) < 0.0f)
			flipped= 1;

		if(flipped) {
			nor[0]= -nor[0];
			nor[1]= -nor[1];
			nor[2]= -nor[2];
		}
	}

	return flipped;
}

VlakRen *render_object_vlak_get(ObjectRen *obr, int nr)
{
	VlakRen *v;
	int a;

	a= render_object_chunk_get((void**)&obr->vlaknodes, &obr->vlaknodeslen, nr, sizeof(VlakTableNode));
	v= obr->vlaknodes[a].vlak;
	
	if(v == NULL) {
		int i;

		v= (VlakRen *)MEM_callocN(256*sizeof(VlakRen),"findOrAddVlak");
		obr->vlaknodes[a].vlak= v;

		for(i= (nr & 0xFFFFFF00), a=0; a<256; a++, i++)
			v[a].index= i;
	}

	return v + (nr & 255);
}



/* ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- */
/* Orco hash and Materials                                                   */
/* ------------------------------------------------------------------------- */

static float *get_object_orco(Render *re, Object *ob)
{
	float *orco;
	
	if (!re->db.orco_hash)
		re->db.orco_hash = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp);
	
	orco = BLI_ghash_lookup(re->db.orco_hash, ob);
	
	if (!orco) {
		if (ELEM(ob->type, OB_CURVE, OB_FONT)) {
			orco = make_orco_curve(re->db.scene, ob);
		} else if (ob->type==OB_SURF) {
			orco = make_orco_surf(ob);
		} else if (ob->type==OB_MBALL) {
			orco = make_orco_mball(ob);
		}
		
		if (orco)
			BLI_ghash_insert(re->db.orco_hash, ob, orco);
	}
	
	return orco;
}

static void set_object_orco(Render *re, void *ob, float *orco)
{
	if (!re->db.orco_hash)
		re->db.orco_hash = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp);
	
	BLI_ghash_insert(re->db.orco_hash, ob, orco);
}

/* ------------------------------------------------------------------------- */
/* tool functions/defines for ad hoc simplification and possible future 
   cleanup      */
/* ------------------------------------------------------------------------- */

#define UVTOINDEX(u,v) (startvlak + (u) * sizev + (v))
/*

NOTE THAT U/V COORDINATES ARE SOMETIMES SWAPPED !!
	
^	()----p4----p3----()
|	|     |     |     |
u	|     |  F1 |  F2 |
	|     |     |     |
	()----p1----p2----()
	       v ->
*/

/* ------------------------------------------------------------------------- */

static void split_v_renderfaces(ObjectRen *obr, int startvlak, int startvert, int usize, int vsize, int uIndex, int cyclu, int cyclv)
{
	int vLen = vsize-1+(!!cyclv);
	int v;

	for (v=0; v<vLen; v++) {
		VlakRen *vlr = render_object_vlak_get(obr, startvlak + vLen*uIndex + v);
		VertRen *vert = render_object_vert_copy(obr, vlr->v2);

		if (cyclv) {
			vlr->v2 = vert;

			if (v==vLen-1) {
				VlakRen *vlr = render_object_vlak_get(obr, startvlak + vLen*uIndex + 0);
				vlr->v1 = vert;
			} else {
				VlakRen *vlr = render_object_vlak_get(obr, startvlak + vLen*uIndex + v+1);
				vlr->v1 = vert;
			}
		} else {
			vlr->v2 = vert;

			if (v<vLen-1) {
				VlakRen *vlr = render_object_vlak_get(obr, startvlak + vLen*uIndex + v+1);
				vlr->v1 = vert;
			}

			if (v==0) {
				vlr->v1 = render_object_vert_copy(obr, vlr->v1);
			} 
		}
	}
}

/* ------------------------------------------------------------------------- */

static int check_vnormal(float *n, float *veno)
{
	float inp;

	inp=n[0]*veno[0]+n[1]*veno[1]+n[2]*veno[2];
	if(inp < -FLT_EPSILON10) return 1;
	return 0;
}

/* ------------------------------------------------------------------------- */
/* Stress, tangents and normals                                              */
/* ------------------------------------------------------------------------- */

static void calc_edge_stress_add(float *accum, VertRen *v1, VertRen *v2)
{
	float len= len_v3v3(v1->co, v2->co)/len_v3v3(v1->orco, v2->orco);
	float *acc;
	
	acc= accum + 2*v1->index;
	acc[0]+= len;
	acc[1]+= 1.0f;
	
	acc= accum + 2*v2->index;
	acc[0]+= len;
	acc[1]+= 1.0f;
}

static void calc_edge_stress(Render *re, ObjectRen *obr, Mesh *me)
{
	float loc[3], size[3], *accum, *acc, *accumoffs, *stress;
	int a;
	
	if(obr->totvert==0) return;
	
	mesh_get_texspace(me, loc, NULL, size);
	
	accum= MEM_callocN(2*sizeof(float)*obr->totvert, "temp accum for stress");
	
	/* de-normalize orco */
	for(a=0; a<obr->totvert; a++) {
		VertRen *ver= render_object_vert_get(obr, a);
		if(ver->orco) {
			ver->orco[0]= ver->orco[0]*size[0] +loc[0];
			ver->orco[1]= ver->orco[1]*size[1] +loc[1];
			ver->orco[2]= ver->orco[2]*size[2] +loc[2];
		}
	}
	
	/* add stress values */
	accumoffs= accum;	/* so we can use vertex index */
	for(a=0; a<obr->totvlak; a++) {
		VlakRen *vlr= render_object_vlak_get(obr, a);

		if(vlr->v1->orco && vlr->v4) {
			calc_edge_stress_add(accumoffs, vlr->v1, vlr->v2);
			calc_edge_stress_add(accumoffs, vlr->v2, vlr->v3);
			calc_edge_stress_add(accumoffs, vlr->v3, vlr->v1);
			if(vlr->v4) {
				calc_edge_stress_add(accumoffs, vlr->v3, vlr->v4);
				calc_edge_stress_add(accumoffs, vlr->v4, vlr->v1);
				calc_edge_stress_add(accumoffs, vlr->v2, vlr->v4);
			}
		}
	}
	
	for(a=0; a<obr->totvert; a++) {
		VertRen *ver= render_object_vert_get(obr, a);
		if(ver->orco) {
			/* find stress value */
			acc= accumoffs + 2*ver->index;
			if(acc[1]!=0.0f)
				acc[0]/= acc[1];
			stress= render_vert_get_stress(obr, ver, 1);
			*stress= *acc;
			
			/* restore orcos */
			ver->orco[0] = (ver->orco[0]-loc[0])/size[0];
			ver->orco[1] = (ver->orco[1]-loc[1])/size[1];
			ver->orco[2] = (ver->orco[2]-loc[2])/size[2];
		}
	}
	
	MEM_freeN(accum);
}

/* gets tangent from tface or orco */
static void calc_tangent_vector(ObjectRen *obr, VertexTangent **vtangents, MemArena *arena, VlakRen *vlr, int do_nmap_tangent, int do_tangent)
{
	MTFace *tface= render_vlak_get_tface(obr, vlr, obr->actmtface, NULL, 0);
	VertRen *v1=vlr->v1, *v2=vlr->v2, *v3=vlr->v3, *v4=vlr->v4;
	float tang[3], *tav;
	float *uv1, *uv2, *uv3, *uv4;
	float uv[4][2];
	
	if(tface) {
		uv1= tface->uv[0];
		uv2= tface->uv[1];
		uv3= tface->uv[2];
		uv4= tface->uv[3];
	}
	else if(v1->orco) {
		uv1= uv[0]; uv2= uv[1]; uv3= uv[2]; uv4= uv[3];
		map_to_sphere( &uv[0][0], &uv[0][1],v1->orco[0], v1->orco[1], v1->orco[2]);
		map_to_sphere( &uv[1][0], &uv[1][1],v2->orco[0], v2->orco[1], v2->orco[2]);
		map_to_sphere( &uv[2][0], &uv[2][1],v3->orco[0], v3->orco[1], v3->orco[2]);
		if(v4)
			map_to_sphere( &uv[3][0], &uv[3][1],v4->orco[0], v4->orco[1], v4->orco[2]);
	}
	else return;

	tangent_from_uv(uv1, uv2, uv3, v1->co, v2->co, v3->co, vlr->n, tang);
	
	if(do_tangent) {
		tav= render_vert_get_tangent(obr, v1, 1);
		add_v3_v3v3(tav, tav, tang);
		tav= render_vert_get_tangent(obr, v2, 1);
		add_v3_v3v3(tav, tav, tang);
		tav= render_vert_get_tangent(obr, v3, 1);
		add_v3_v3v3(tav, tav, tang);
	}
	
	if(do_nmap_tangent) {
		sum_or_add_vertex_tangent(arena, &vtangents[v1->index], tang, uv1);
		sum_or_add_vertex_tangent(arena, &vtangents[v2->index], tang, uv2);
		sum_or_add_vertex_tangent(arena, &vtangents[v3->index], tang, uv3);
	}

	if(v4) {
		tangent_from_uv(uv1, uv3, uv4, v1->co, v3->co, v4->co, vlr->n, tang);
		
		if(do_tangent) {
			tav= render_vert_get_tangent(obr, v1, 1);
			add_v3_v3v3(tav, tav, tang);
			tav= render_vert_get_tangent(obr, v3, 1);
			add_v3_v3v3(tav, tav, tang);
			tav= render_vert_get_tangent(obr, v4, 1);
			add_v3_v3v3(tav, tav, tang);
		}

		if(do_nmap_tangent) {
			sum_or_add_vertex_tangent(arena, &vtangents[v1->index], tang, uv1);
			sum_or_add_vertex_tangent(arena, &vtangents[v3->index], tang, uv3);
			sum_or_add_vertex_tangent(arena, &vtangents[v4->index], tang, uv4);
		}
	}
}


static void calc_vertexnormals(Render *re, ObjectRen *obr, int do_tangent, int do_nmap_tangent)
{
	MemArena *arena= NULL;
	VertexTangent **vtangents= NULL;
	int a;

	if(do_nmap_tangent) {
		arena= BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE);
		BLI_memarena_use_calloc(arena);

		vtangents= MEM_callocN(sizeof(VertexTangent*)*obr->totvert, "VertexTangent");
	}

		/* clear all vertex normals */
	for(a=0; a<obr->totvert; a++) {
		VertRen *ver= render_object_vert_get(obr, a);
		ver->n[0]=ver->n[1]=ver->n[2]= 0.0f;
	}

		/* calculate cos of angles and point-masses, use as weight factor to
		   add face normal to vertex */
	for(a=0; a<obr->totvlak; a++) {
		VlakRen *vlr= render_object_vlak_get(obr, a);
		if(vlr->flag & ME_SMOOTH) {
			VertRen *v1= vlr->v1;
			VertRen *v2= vlr->v2;
			VertRen *v3= vlr->v3;
			VertRen *v4= vlr->v4;
			float n1[3], n2[3], n3[3], n4[3];
			float fac1, fac2, fac3, fac4=0.0f;
			
			if(re->params.flag & R_GLOB_NOPUNOFLIP)
				vlr->flag |= R_NOPUNOFLIP;
			
			sub_v3_v3v3(n1, v2->co, v1->co);
			normalize_v3(n1);
			sub_v3_v3v3(n2, v3->co, v2->co);
			normalize_v3(n2);
			if(v4==NULL) {
				sub_v3_v3v3(n3, v1->co, v3->co);
				normalize_v3(n3);

				fac1= saacos(-n1[0]*n3[0]-n1[1]*n3[1]-n1[2]*n3[2]);
				fac2= saacos(-n1[0]*n2[0]-n1[1]*n2[1]-n1[2]*n2[2]);
				fac3= saacos(-n2[0]*n3[0]-n2[1]*n3[1]-n2[2]*n3[2]);
			}
			else {
				sub_v3_v3v3(n3, v4->co, v3->co);
				normalize_v3(n3);
				sub_v3_v3v3(n4, v1->co, v4->co);
				normalize_v3(n4);

				fac1= saacos(-n4[0]*n1[0]-n4[1]*n1[1]-n4[2]*n1[2]);
				fac2= saacos(-n1[0]*n2[0]-n1[1]*n2[1]-n1[2]*n2[2]);
				fac3= saacos(-n2[0]*n3[0]-n2[1]*n3[1]-n2[2]*n3[2]);
				fac4= saacos(-n3[0]*n4[0]-n3[1]*n4[1]-n3[2]*n4[2]);

				if(!(vlr->flag & R_NOPUNOFLIP)) {
					if( check_vnormal(vlr->n, v4->n) ) fac4= -fac4;
				}

				v4->n[0] +=fac4*vlr->n[0];
				v4->n[1] +=fac4*vlr->n[1];
				v4->n[2] +=fac4*vlr->n[2];
			}

			if(!(vlr->flag & R_NOPUNOFLIP)) {
				if( check_vnormal(vlr->n, v1->n) ) fac1= -fac1;
				if( check_vnormal(vlr->n, v2->n) ) fac2= -fac2;
				if( check_vnormal(vlr->n, v3->n) ) fac3= -fac3;
			}

			v1->n[0] +=fac1*vlr->n[0];
			v1->n[1] +=fac1*vlr->n[1];
			v1->n[2] +=fac1*vlr->n[2];

			v2->n[0] +=fac2*vlr->n[0];
			v2->n[1] +=fac2*vlr->n[1];
			v2->n[2] +=fac2*vlr->n[2];

			v3->n[0] +=fac3*vlr->n[0];
			v3->n[1] +=fac3*vlr->n[1];
			v3->n[2] +=fac3*vlr->n[2];
			
		}
		if(do_nmap_tangent || do_tangent) {
			/* tangents still need to be calculated for flat faces too */
			/* weighting removed, they are not vertexnormals */
			calc_tangent_vector(obr, vtangents, arena, vlr, do_nmap_tangent, do_tangent);
		}
	}

		/* do solid faces */
	for(a=0; a<obr->totvlak; a++) {
		VlakRen *vlr= render_object_vlak_get(obr, a);
		if((vlr->flag & ME_SMOOTH)==0) {
			float *f1= vlr->v1->n;
			if(f1[0]==0.0 && f1[1]==0.0 && f1[2]==0.0) copy_v3_v3(f1, vlr->n);
			f1= vlr->v2->n;
			if(f1[0]==0.0 && f1[1]==0.0 && f1[2]==0.0) copy_v3_v3(f1, vlr->n);
			f1= vlr->v3->n;
			if(f1[0]==0.0 && f1[1]==0.0 && f1[2]==0.0) copy_v3_v3(f1, vlr->n);
			if(vlr->v4) {
				f1= vlr->v4->n;
				if(f1[0]==0.0 && f1[1]==0.0 && f1[2]==0.0) copy_v3_v3(f1, vlr->n);
			}
		}

		if(do_nmap_tangent) {
			VertRen *v1=vlr->v1, *v2=vlr->v2, *v3=vlr->v3, *v4=vlr->v4;
			MTFace *tface= render_vlak_get_tface(obr, vlr, obr->actmtface, NULL, 0);

			if(tface) {
				float *vtang, *ftang= render_vlak_get_nmap_tangent(obr, vlr, 1);

				vtang= find_vertex_tangent(vtangents[v1->index], tface->uv[0]);
				copy_v3_v3(ftang, vtang);
				normalize_v3(ftang);
				vtang= find_vertex_tangent(vtangents[v2->index], tface->uv[1]);
				copy_v3_v3(ftang+3, vtang);
				normalize_v3(ftang+3);
				vtang= find_vertex_tangent(vtangents[v3->index], tface->uv[2]);
				copy_v3_v3(ftang+6, vtang);
				normalize_v3(ftang+6);
				if(v4) {
					vtang= find_vertex_tangent(vtangents[v4->index], tface->uv[3]);
					copy_v3_v3(ftang+9, vtang);
					normalize_v3(ftang+9);
				}
			}
		}
	}
	
		/* normalize vertex normals */
	for(a=0; a<obr->totvert; a++) {
		VertRen *ver= render_object_vert_get(obr, a);
		normalize_v3(ver->n);
		if(do_tangent) {
			float *tav= render_vert_get_tangent(obr, ver, 0);
			if (tav) {
				/* orthonorm. */
				float tdn = tav[0]*ver->n[0] + tav[1]*ver->n[1] + tav[2]*ver->n[2];
				tav[0] -= ver->n[0]*tdn;
				tav[1] -= ver->n[1]*tdn;
				tav[2] -= ver->n[2]*tdn;
				normalize_v3(tav);
			}
		}
	}


	if(arena)
		BLI_memarena_free(arena);
	if(vtangents)
		MEM_freeN(vtangents);
}

/* ------------------------------------------------------------------------- */
/* Autosmoothing:                                                            */
/* ------------------------------------------------------------------------- */

typedef struct ASvert {
	int totface;
	ListBase faces;
} ASvert;

typedef struct ASface {
	struct ASface *next, *prev;
	VlakRen *vlr[4];
	VertRen *nver[4];
} ASface;

static void as_addvert(ASvert *asv, VertRen *v1, VlakRen *vlr)
{
	ASface *asf;
	int a;
	
	if(v1 == NULL) return;
	
	if(asv->faces.first==NULL) {
		asf= MEM_callocN(sizeof(ASface), "asface");
		BLI_addtail(&asv->faces, asf);
	}
	
	asf= asv->faces.last;
	for(a=0; a<4; a++) {
		if(asf->vlr[a]==NULL) {
			asf->vlr[a]= vlr;
			asv->totface++;
			break;
		}
	}
	
	/* new face struct */
	if(a==4) {
		asf= MEM_callocN(sizeof(ASface), "asface");
		BLI_addtail(&asv->faces, asf);
		asf->vlr[0]= vlr;
		asv->totface++;
	}
}

static int as_testvertex(VlakRen *vlr, VertRen *ver, ASvert *asv, float thresh) 
{
	/* return 1: vertex needs a copy */
	ASface *asf;
	float inp;
	int a;
	
	if(vlr==0) return 0;
	
	asf= asv->faces.first;
	while(asf) {
		for(a=0; a<4; a++) {
			if(asf->vlr[a] && asf->vlr[a]!=vlr) {
				inp= fabs( vlr->n[0]*asf->vlr[a]->n[0] + vlr->n[1]*asf->vlr[a]->n[1] + vlr->n[2]*asf->vlr[a]->n[2] );
				if(inp < thresh) return 1;
			}
		}
		asf= asf->next;
	}
	
	return 0;
}

static VertRen *as_findvertex(VlakRen *vlr, VertRen *ver, ASvert *asv, float thresh) 
{
	/* return when new vertex already was made */
	ASface *asf;
	float inp;
	int a;
	
	asf= asv->faces.first;
	while(asf) {
		for(a=0; a<4; a++) {
			if(asf->vlr[a] && asf->vlr[a]!=vlr) {
				/* this face already made a copy for this vertex! */
				if(asf->nver[a]) {
					inp= fabs( vlr->n[0]*asf->vlr[a]->n[0] + vlr->n[1]*asf->vlr[a]->n[1] + vlr->n[2]*asf->vlr[a]->n[2] );
					if(inp >= thresh) {
						return asf->nver[a];
					}
				}
			}
		}
		asf= asf->next;
	}
	
	return NULL;
}

/* note; autosmooth happens in object space still, after applying autosmooth we rotate */
/* note2; actually, when original mesh and displist are equal sized, face normals are from original mesh */
static void autosmooth(Render *re, ObjectRen *obr, float mat[][4], int degr)
{
	ASvert *asv, *asverts;
	ASface *asf;
	VertRen *ver, *v1;
	VlakRen *vlr;
	float thresh;
	int a, b, totvert;
	
	if(obr->totvert==0) return;
	asverts= MEM_callocN(sizeof(ASvert)*obr->totvert, "all smooth verts");
	
	thresh= cos( M_PI*(0.5f+(float)degr)/180.0 );
	
	/* step zero: give faces normals of original mesh, if this is provided */
	
	
	/* step one: construct listbase of all vertices and pointers to faces */
	for(a=0; a<obr->totvlak; a++) {
		vlr= render_object_vlak_get(obr, a);
		/* skip wire faces */
		if(vlr->v2 != vlr->v3) {
			as_addvert(asverts+vlr->v1->index, vlr->v1, vlr);
			as_addvert(asverts+vlr->v2->index, vlr->v2, vlr);
			as_addvert(asverts+vlr->v3->index, vlr->v3, vlr);
			if(vlr->v4) 
				as_addvert(asverts+vlr->v4->index, vlr->v4, vlr);
		}
	}
	
	totvert= obr->totvert;
	/* we now test all vertices, when faces have a normal too much different: they get a new vertex */
	for(a=0, asv=asverts; a<totvert; a++, asv++) {
		if(asv && asv->totface>1) {
			ver= render_object_vert_get(obr, a);

			asf= asv->faces.first;
			while(asf) {
				for(b=0; b<4; b++) {
				
					/* is there a reason to make a new vertex? */
					vlr= asf->vlr[b];
					if( as_testvertex(vlr, ver, asv, thresh) ) {
						
						/* already made a new vertex within threshold? */
						v1= as_findvertex(vlr, ver, asv, thresh);
						if(v1==NULL) {
							/* make a new vertex */
							v1= render_object_vert_copy(obr, ver);
						}
						asf->nver[b]= v1;
						if(vlr->v1==ver) vlr->v1= v1;
						if(vlr->v2==ver) vlr->v2= v1;
						if(vlr->v3==ver) vlr->v3= v1;
						if(vlr->v4==ver) vlr->v4= v1;
					}
				}
				asf= asf->next;
			}
		}
	}
	
	/* free */
	for(a=0; a<totvert; a++) {
		BLI_freelistN(&asverts[a].faces);
	}
	MEM_freeN(asverts);
	
	/* rotate vertices and calculate normal of faces */
	for(a=0; a<obr->totvert; a++) {
		ver= render_object_vert_get(obr, a);
		mul_m4_v3(mat, ver->co);
	}
	for(a=0; a<obr->totvlak; a++) {
		vlr= render_object_vlak_get(obr, a);
		
		/* skip wire faces */
		if(vlr->v2 != vlr->v3) {
			if(vlr->v4) 
				normal_quad_v3( vlr->n,vlr->v4->co, vlr->v3->co, vlr->v2->co, vlr->v1->co);
			else 
				normal_tri_v3( vlr->n,vlr->v3->co, vlr->v2->co, vlr->v1->co);
		}
	}		
}


/* ------------------------------------------------------------------------- */
/* Particles                                                                 */
/* ------------------------------------------------------------------------- */
typedef struct ParticleStrandData
{
	struct MCol *mcol;
	float *orco, *uvco, *surfnor;
	float time, adapt_angle, adapt_pix, size;
	int totuv, totcol;
	int first, line, adapt, override_uv;
}
ParticleStrandData;
/* future thread problem... */
static void static_particle_strand(Render *re, ObjectRen *obr, Material *ma, ParticleStrandData *sd, float *vec, float *vec1)
{
	static VertRen *v1= NULL, *v2= NULL;
	VlakRen *vlr= NULL;
	float nor[3], cross[3], crosslen, w, dx, dy, width;
	static float anor[3], avec[3];
	int flag, i;
	static int second=0;
	
	sub_v3_v3v3(nor, vec, vec1);
	normalize_v3(nor);		// nor needed as tangent 
	cross_v3_v3v3(cross, vec, nor);

	/* turn cross in pixelsize */
	w= vec[2]*re->cam.winmat[2][3] + re->cam.winmat[3][3];
	dx= re->cam.winx*cross[0]*re->cam.winmat[0][0];
	dy= re->cam.winy*cross[1]*re->cam.winmat[1][1];
	w= sqrt(dx*dx + dy*dy)/w;
	
	if(w!=0.0f) {
		float fac;
		if(ma->strand_ease!=0.0f) {
			if(ma->strand_ease<0.0f)
				fac= pow(sd->time, 1.0+ma->strand_ease);
			else
				fac= pow(sd->time, 1.0/(1.0f-ma->strand_ease));
		}
		else fac= sd->time;

		width= ((1.0f-fac)*ma->strand_sta + (fac)*ma->strand_end);

		/* use actual Blender units for strand width and fall back to minimum width */
		if(ma->mode & MA_STR_B_UNITS){
            crosslen= len_v3(cross);
            w= 2.0f*crosslen*ma->strand_min/w;

			if(width < w)
				width= w;

			/*cross is the radius of the strand so we want it to be half of full width */
			mul_v3_fl(cross,0.5/crosslen);
		}
		else
			width/=w;

		mul_v3_fl(cross, width);
	}
	else width= 1.0f;
	
	if(ma->mode & MA_TANGENT_STR)
		flag= R_SMOOTH|R_NOPUNOFLIP|R_TANGENT;
	else
		flag= R_SMOOTH;
	
	/* only 1 pixel wide strands filled in as quads now, otherwise zbuf errors */
	if(ma->strand_sta==1.0f)
		flag |= R_STRAND;
	
	/* single face line */
	if(sd->line) {
		vlr= render_object_vlak_get(obr, obr->totvlak++);
		vlr->flag= flag;
		vlr->v1= render_object_vert_get(obr, obr->totvert++);
		vlr->v2= render_object_vert_get(obr, obr->totvert++);
		vlr->v3= render_object_vert_get(obr, obr->totvert++);
		vlr->v4= render_object_vert_get(obr, obr->totvert++);
		
		copy_v3_v3(vlr->v1->co, vec);
		add_v3_v3v3(vlr->v1->co, vlr->v1->co, cross);
		copy_v3_v3(vlr->v1->n, nor);
		vlr->v1->orco= sd->orco;
		vlr->v1->accum= -1.0f;	// accum abuse for strand texco
		
		copy_v3_v3(vlr->v2->co, vec);
		sub_v3_v3v3(vlr->v2->co, vlr->v2->co, cross);
		copy_v3_v3(vlr->v2->n, nor);
		vlr->v2->orco= sd->orco;
		vlr->v2->accum= vlr->v1->accum;

		copy_v3_v3(vlr->v4->co, vec1);
		add_v3_v3v3(vlr->v4->co, vlr->v4->co, cross);
		copy_v3_v3(vlr->v4->n, nor);
		vlr->v4->orco= sd->orco;
		vlr->v4->accum= 1.0f;	// accum abuse for strand texco
		
		copy_v3_v3(vlr->v3->co, vec1);
		sub_v3_v3v3(vlr->v3->co, vlr->v3->co, cross);
		copy_v3_v3(vlr->v3->n, nor);
		vlr->v3->orco= sd->orco;
		vlr->v3->accum= vlr->v4->accum;

		normal_quad_v3( vlr->n,vlr->v4->co, vlr->v3->co, vlr->v2->co, vlr->v1->co);
		
		vlr->mat= ma;
		vlr->ec= ME_V2V3;

		if(sd->surfnor) {
			float *snor= render_vlak_get_surfnor(obr, vlr, 1);
			copy_v3_v3(snor, sd->surfnor);
		}

		if(sd->uvco){
			for(i=0; i<sd->totuv; i++){
				MTFace *mtf;
				mtf=render_vlak_get_tface(obr,vlr,i,NULL,1);
				mtf->uv[0][0]=mtf->uv[1][0]=
				mtf->uv[2][0]=mtf->uv[3][0]=(sd->uvco+2*i)[0];
				mtf->uv[0][1]=mtf->uv[1][1]=
				mtf->uv[2][1]=mtf->uv[3][1]=(sd->uvco+2*i)[1];
			}
			if(sd->override_uv>=0){
				MTFace *mtf;
				mtf=render_vlak_get_tface(obr,vlr,sd->override_uv,NULL,0);
				
				mtf->uv[0][0]=mtf->uv[3][0]=0.0f;
				mtf->uv[1][0]=mtf->uv[2][0]=1.0f;

				mtf->uv[0][1]=mtf->uv[1][1]=0.0f;
				mtf->uv[2][1]=mtf->uv[3][1]=1.0f;
			}
		}
		if(sd->mcol){
			for(i=0; i<sd->totcol; i++){
				MCol *mc;
				mc=render_vlak_get_mcol(obr,vlr,i,NULL,1);
				mc[0]=mc[1]=mc[2]=mc[3]=sd->mcol[i];
				mc[0]=mc[1]=mc[2]=mc[3]=sd->mcol[i];
			}
		}
	}
	/* first two vertices of a strand */
	else if(sd->first) {
		if(sd->adapt){
			copy_v3_v3(anor, nor);
			copy_v3_v3(avec, vec);
			second=1;
		}

		v1= render_object_vert_get(obr, obr->totvert++);
		v2= render_object_vert_get(obr, obr->totvert++);
		
		copy_v3_v3(v1->co, vec);
		add_v3_v3v3(v1->co, v1->co, cross);
		copy_v3_v3(v1->n, nor);
		v1->orco= sd->orco;
		v1->accum= -1.0f;	// accum abuse for strand texco
		
		copy_v3_v3(v2->co, vec);
		sub_v3_v3v3(v2->co, v2->co, cross);
		copy_v3_v3(v2->n, nor);
		v2->orco= sd->orco;
		v2->accum= v1->accum;
	}
	/* more vertices & faces to strand */
	else {
		if(sd->adapt==0 || second){
			vlr= render_object_vlak_get(obr, obr->totvlak++);
			vlr->flag= flag;
			vlr->v1= v1;
			vlr->v2= v2;
			vlr->v3= render_object_vert_get(obr, obr->totvert++);
			vlr->v4= render_object_vert_get(obr, obr->totvert++);
			
			v1= vlr->v4; // cycle
			v2= vlr->v3; // cycle

			
			if(sd->adapt){
				second=0;
				copy_v3_v3(anor,nor);
				copy_v3_v3(avec,vec);
			}

		}
		else if(sd->adapt){
			float dvec[3],pvec[3];
			sub_v3_v3v3(dvec,avec,vec);
			project_v3_v3v3(pvec,dvec,vec);
			sub_v3_v3v3(dvec,dvec,pvec);

			w= vec[2]*re->cam.winmat[2][3] + re->cam.winmat[3][3];
			dx= re->cam.winx*dvec[0]*re->cam.winmat[0][0]/w;
			dy= re->cam.winy*dvec[1]*re->cam.winmat[1][1]/w;
			w= sqrt(dx*dx + dy*dy);
			if(dot_v3v3(anor,nor)<sd->adapt_angle && w>sd->adapt_pix){
				vlr= render_object_vlak_get(obr, obr->totvlak++);
				vlr->flag= flag;
				vlr->v1= v1;
				vlr->v2= v2;
				vlr->v3= render_object_vert_get(obr, obr->totvert++);
				vlr->v4= render_object_vert_get(obr, obr->totvert++);
				
				v1= vlr->v4; // cycle
				v2= vlr->v3; // cycle

				copy_v3_v3(anor,nor);
				copy_v3_v3(avec,vec);
			}
			else{
				vlr= render_object_vlak_get(obr, obr->totvlak-1);
			}
		}
	
		copy_v3_v3(vlr->v4->co, vec);
		add_v3_v3v3(vlr->v4->co, vlr->v4->co, cross);
		copy_v3_v3(vlr->v4->n, nor);
		vlr->v4->orco= sd->orco;
		vlr->v4->accum= -1.0f + 2.0f*sd->time;	// accum abuse for strand texco
		
		copy_v3_v3(vlr->v3->co, vec);
		sub_v3_v3v3(vlr->v3->co, vlr->v3->co, cross);
		copy_v3_v3(vlr->v3->n, nor);
		vlr->v3->orco= sd->orco;
		vlr->v3->accum= vlr->v4->accum;
		
		normal_quad_v3( vlr->n,vlr->v4->co, vlr->v3->co, vlr->v2->co, vlr->v1->co);
		
		vlr->mat= ma;
		vlr->ec= ME_V2V3;

		if(sd->surfnor) {
			float *snor= render_vlak_get_surfnor(obr, vlr, 1);
			copy_v3_v3(snor, sd->surfnor);
		}

		if(sd->uvco){
			for(i=0; i<sd->totuv; i++){
				MTFace *mtf;
				mtf=render_vlak_get_tface(obr,vlr,i,NULL,1);
				mtf->uv[0][0]=mtf->uv[1][0]=
				mtf->uv[2][0]=mtf->uv[3][0]=(sd->uvco+2*i)[0];
				mtf->uv[0][1]=mtf->uv[1][1]=
				mtf->uv[2][1]=mtf->uv[3][1]=(sd->uvco+2*i)[1];
			}
			if(sd->override_uv>=0){
				MTFace *mtf;
				mtf=render_vlak_get_tface(obr,vlr,sd->override_uv,NULL,0);
				
				mtf->uv[0][0]=mtf->uv[3][0]=0.0f;
				mtf->uv[1][0]=mtf->uv[2][0]=1.0f;

				mtf->uv[0][1]=mtf->uv[1][1]=(vlr->v1->accum+1.0f)/2.0f;
				mtf->uv[2][1]=mtf->uv[3][1]=(vlr->v3->accum+1.0f)/2.0f;
			}
		}
		if(sd->mcol){
			for(i=0; i<sd->totcol; i++){
				MCol *mc;
				mc=render_vlak_get_mcol(obr,vlr,i,NULL,1);
				mc[0]=mc[1]=mc[2]=mc[3]=sd->mcol[i];
				mc[0]=mc[1]=mc[2]=mc[3]=sd->mcol[i];
			}
		}
	}
}

static void static_particle_wire(ObjectRen *obr, Material *ma, float *vec, float *vec1, int first, int line)
{
	VlakRen *vlr;
	static VertRen *v1;

	if(line) {
		vlr= render_object_vlak_get(obr, obr->totvlak++);
		vlr->v1= render_object_vert_get(obr, obr->totvert++);
		vlr->v2= render_object_vert_get(obr, obr->totvert++);
		vlr->v3= vlr->v2;
		vlr->v4= NULL;
		
		copy_v3_v3(vlr->v1->co, vec);
		copy_v3_v3(vlr->v2->co, vec1);
		
		sub_v3_v3v3(vlr->n, vec, vec1);
		normalize_v3(vlr->n);
		copy_v3_v3(vlr->v1->n, vlr->n);
		copy_v3_v3(vlr->v2->n, vlr->n);
		
		vlr->mat= ma;
		vlr->ec= ME_V1V2;

	}
	else if(first) {
		v1= render_object_vert_get(obr, obr->totvert++);
		copy_v3_v3(v1->co, vec);
	}
	else {
		vlr= render_object_vlak_get(obr, obr->totvlak++);
		vlr->v1= v1;
		vlr->v2= render_object_vert_get(obr, obr->totvert++);
		vlr->v3= vlr->v2;
		vlr->v4= NULL;
		
		v1= vlr->v2; // cycle
		copy_v3_v3(v1->co, vec);
		
		sub_v3_v3v3(vlr->n, vec, vec1);
		normalize_v3(vlr->n);
		copy_v3_v3(v1->n, vlr->n);
		
		vlr->mat= ma;
		vlr->ec= ME_V1V2;
	}

}

static void particle_curve(Render *re, ObjectRen *obr, DerivedMesh *dm, Material *ma, ParticleStrandData *sd, float *loc, float *loc1,	int seed)
{
	HaloRen *har=0;

	if(ma->material_type == MA_TYPE_WIRE)
		static_particle_wire(obr, ma, loc, loc1, sd->first, sd->line);
	else if(ma->material_type == MA_TYPE_HALO) {
		har= halo_init_particle(re, obr, dm, ma, loc, loc1, sd->orco, sd->uvco, sd->size, 1.0, seed);
		if(har) har->lay= obr->ob->lay;
	}
	else
		static_particle_strand(re, obr, ma, sd, loc, loc1);
}
static void particle_billboard(Render *re, ObjectRen *obr, Material *ma, ParticleBillboardData *bb)
{
	VlakRen *vlr;
	MTFace *mtf;
	float xvec[3], yvec[3], zvec[3], bb_center[3];
	float uvx = 0.0f, uvy = 0.0f, uvdx = 1.0f, uvdy = 1.0f, time = 0.0f;

	vlr= render_object_vlak_get(obr, obr->totvlak++);
	vlr->v1= render_object_vert_get(obr, obr->totvert++);
	vlr->v2= render_object_vert_get(obr, obr->totvert++);
	vlr->v3= render_object_vert_get(obr, obr->totvert++);
	vlr->v4= render_object_vert_get(obr, obr->totvert++);

	psys_make_billboard(bb, xvec, yvec, zvec, bb_center);

	add_v3_v3v3(vlr->v1->co, bb_center, xvec);
	add_v3_v3v3(vlr->v1->co, vlr->v1->co, yvec);
	mul_m4_v3(re->cam.viewmat, vlr->v1->co);

	sub_v3_v3v3(vlr->v2->co, bb_center, xvec);
	add_v3_v3v3(vlr->v2->co, vlr->v2->co, yvec);
	mul_m4_v3(re->cam.viewmat, vlr->v2->co);

	sub_v3_v3v3(vlr->v3->co, bb_center, xvec);
	sub_v3_v3v3(vlr->v3->co, vlr->v3->co, yvec);
	mul_m4_v3(re->cam.viewmat, vlr->v3->co);

	add_v3_v3v3(vlr->v4->co, bb_center, xvec);
	sub_v3_v3v3(vlr->v4->co, vlr->v4->co, yvec);
	mul_m4_v3(re->cam.viewmat, vlr->v4->co);

	normal_quad_v3( vlr->n,vlr->v4->co, vlr->v3->co, vlr->v2->co, vlr->v1->co);
	copy_v3_v3(vlr->v1->n,vlr->n);
	copy_v3_v3(vlr->v2->n,vlr->n);
	copy_v3_v3(vlr->v3->n,vlr->n);
	copy_v3_v3(vlr->v4->n,vlr->n);
	
	vlr->mat= ma;
	vlr->ec= ME_V2V3;

	if(bb->uv_split > 1){
		uvdx = uvdy = 1.0f / (float)bb->uv_split;
		if(bb->anim == PART_BB_ANIM_TIME) {
			if(bb->split_offset == PART_BB_OFF_NONE)
				time = bb->time;
			else if(bb->split_offset == PART_BB_OFF_LINEAR)
				time = (float)fmod(bb->time + (float)bb->num / (float)(bb->uv_split * bb->uv_split), 1.0f);
			else /* split_offset==PART_BB_OFF_RANDOM */
				time = (float)fmod(bb->time + bb->random, 1.0f);

		}
		else if(bb->anim == PART_BB_ANIM_ANGLE) {
			if(bb->align == PART_BB_VIEW) {
				time = (float)fmod((bb->tilt + 1.0f) / 2.0f, 1.0);
			}
			else{
				float axis1[3] = {0.0f,0.0f,0.0f};
				float axis2[3] = {0.0f,0.0f,0.0f};
				axis1[(bb->align + 1) % 3] = 1.0f;
				axis2[(bb->align + 2) % 3] = 1.0f;
				if(bb->lock == 0) {
					zvec[bb->align] = 0.0f;
					normalize_v3(zvec);
				}
				time = saacos(dot_v3v3(zvec, axis1)) / (float)M_PI;
				if(dot_v3v3(zvec, axis2) < 0.0f)
					time = 1.0f - time / 2.0f;
				else
					time = time / 2.0f;
			}
			if(bb->split_offset == PART_BB_OFF_LINEAR)
				time = (float)fmod(bb->time + (float)bb->num / (float)(bb->uv_split * bb->uv_split), 1.0f);
			else if(bb->split_offset == PART_BB_OFF_RANDOM)
				time = (float)fmod(bb->time + bb->random, 1.0f);
		}
		else{
			if(bb->split_offset == PART_BB_OFF_NONE)
				time = 0.0f;
			else if(bb->split_offset == PART_BB_OFF_LINEAR)
				time = (float)fmod((float)bb->num /(float)(bb->uv_split * bb->uv_split) , 1.0f);
			else /* split_offset==PART_BB_OFF_RANDOM */
				time = bb->random;
		}
		uvx = uvdx * floor((float)(bb->uv_split * bb->uv_split) * (float)fmod((double)time, (double)uvdx));
		uvy = uvdy * floor((1.0f - time) * (float)bb->uv_split);
		if(fmod(time, 1.0f / bb->uv_split) == 0.0f)
			uvy -= uvdy;
	}

	/* normal UVs */
	if(bb->uv[0] >= 0){
		mtf = render_vlak_get_tface(obr, vlr, bb->uv[0], NULL, 1);
		mtf->uv[0][0] = 1.0f;
		mtf->uv[0][1] = 1.0f;
		mtf->uv[1][0] = 0.0f;
		mtf->uv[1][1] = 1.0f;
		mtf->uv[2][0] = 0.0f;
		mtf->uv[2][1] = 0.0f;
		mtf->uv[3][0] = 1.0f;
		mtf->uv[3][1] = 0.0f;
	}

	/* time-index UVs */
	if(bb->uv[1] >= 0){
		mtf = render_vlak_get_tface(obr, vlr, bb->uv[1], NULL, 1);
		mtf->uv[0][0] = mtf->uv[1][0] = mtf->uv[2][0] = mtf->uv[3][0] = bb->time;
		mtf->uv[0][1] = mtf->uv[1][1] = mtf->uv[2][1] = mtf->uv[3][1] = (float)bb->num/(float)bb->totnum;
	}

	/* split UVs */
	if(bb->uv_split > 1 && bb->uv[2] >= 0){
		mtf = render_vlak_get_tface(obr, vlr, bb->uv[2], NULL, 1);
		mtf->uv[0][0] = uvx + uvdx;
		mtf->uv[0][1] = uvy + uvdy;
		mtf->uv[1][0] = uvx;
		mtf->uv[1][1] = uvy + uvdy;
		mtf->uv[2][0] = uvx;
		mtf->uv[2][1] = uvy;
		mtf->uv[3][0] = uvx + uvdx;
		mtf->uv[3][1] = uvy;
	}
}
static void particle_normal_ren(short ren_as, ParticleSettings *part, Render *re, ObjectRen *obr, DerivedMesh *dm, Material *ma, ParticleStrandData *sd, ParticleBillboardData *bb, ParticleKey *state, int seed, float hasize)
{
	float loc[3], loc0[3], loc1[3], vel[3];
	
	copy_v3_v3(loc, state->co);

	if(ren_as != PART_DRAW_BB)
		mul_m4_v3(re->cam.viewmat, loc);

	switch(ren_as) {
		case PART_DRAW_LINE:
			sd->line = 1;
			sd->time = 0.0f;
			sd->size = hasize;

			copy_v3_v3(vel, state->vel);
			mul_mat3_m4_v3(re->cam.viewmat, vel);
			normalize_v3(vel);

			if(part->draw & PART_DRAW_VEL_LENGTH)
				mul_v3_fl(vel, len_v3(state->vel));

			madd_v3_v3v3fl(loc0, loc, vel, -part->draw_line[0]);
			madd_v3_v3v3fl(loc1, loc, vel, part->draw_line[1]);

			particle_curve(re, obr, dm, ma, sd, loc0, loc1, seed);

			break;

		case PART_DRAW_BB:

			copy_v3_v3(bb->vec, loc);
			copy_v3_v3(bb->vel, state->vel);

			particle_billboard(re, obr, ma, bb);

			break;

		default:
		{
			HaloRen *har=0;

			har = halo_init_particle(re, obr, dm, ma, loc, NULL, sd->orco, sd->uvco, hasize, 0.0, seed);
			
			if(har) har->lay= obr->ob->lay;

			break;
		}
	}
}
static void get_particle_uvco_mcol(short from, DerivedMesh *dm, float *fuv, int num, ParticleStrandData *sd)
{
	int i;

	/* get uvco */
	if(sd->uvco && ELEM(from,PART_FROM_FACE,PART_FROM_VOLUME)) {
		for(i=0; i<sd->totuv; i++) {
			if(num != DMCACHE_NOTFOUND) {
				MFace *mface = dm->getFaceData(dm, num, CD_MFACE);
				MTFace *mtface = (MTFace*)CustomData_get_layer_n(&dm->faceData, CD_MTFACE, i);
				mtface += num;
				
				psys_interpolate_uvs(mtface, mface->v4, fuv, sd->uvco + 2 * i);
			}
			else {
				sd->uvco[2*i] = 0.0f;
				sd->uvco[2*i + 1] = 0.0f;
			}
		}
	}

	/* get mcol */
	if(sd->mcol && ELEM(from,PART_FROM_FACE,PART_FROM_VOLUME)) {
		for(i=0; i<sd->totcol; i++) {
			if(num != DMCACHE_NOTFOUND) {
				MFace *mface = dm->getFaceData(dm, num, CD_MFACE);
				MCol *mc = (MCol*)CustomData_get_layer_n(&dm->faceData, CD_MCOL, i);
				mc += num * 4;

				psys_interpolate_mcol(mc, mface->v4, fuv, sd->mcol + i);
			}
			else
				memset(&sd->mcol[i], 0, sizeof(MCol));
		}
	}
}
static int render_new_particle_system(Render *re, ObjectRen *obr, ParticleSystem *psys, int timeoffset)
{
	Object *ob= obr->ob;
//	Object *tob=0;
	Material *ma=0;
	ParticleSystemModifierData *psmd;
	ParticleSystem *tpsys=0;
	ParticleSettings *part, *tpart=0;
	ParticleData *pars, *pa=0,*tpa=0;
	ParticleKey *states=0;
	ParticleKey state;
	ParticleCacheKey *cache=0;
	ParticleBillboardData bb;
	ParticleSimulationData sim = {re->db.scene, ob, psys, NULL};
	ParticleStrandData sd;
	StrandBuffer *strandbuf=0;
	StrandVert *svert=0;
	StrandBound *sbound= 0;
	StrandRen *strand=0;
	RNG *rng= 0;
	float loc[3],loc1[3],loc0[3],mat[4][4],nmat[3][3],co[3],nor[3],time;
	float strandlen=0.0f, curlen=0.0f;
	float hasize, pa_size, r_tilt, r_length, cfra=bsystem_time(re->db.scene, ob, (float)re->db.scene->r.cfra, 0.0);
	float pa_time, pa_birthtime, pa_dietime;
	float random, simplify[2];
	int i, a, k, max_k=0, totpart, dosimplify = 0, dosurfacecache = 0;
	int totchild=0;
	int seed, path_nbr=0, orco1=0, num;
	int totface, *origindex = 0;
	char **uv_name=0;

/* 1. check that everything is ok & updated */
	if(psys==NULL)
		return 0;
	
	totchild=psys->totchild;

	part=psys->part;
	pars=psys->particles;

	if(part==NULL || pars==NULL || !psys_check_enabled(ob, psys))
		return 0;
	
	if(part->ren_as==PART_DRAW_OB || part->ren_as==PART_DRAW_GR || part->ren_as==PART_DRAW_NOT)
		return 1;

/* 2. start initialising things */

	/* last possibility to bail out! */
	sim.psmd = psmd = psys_get_modifier(ob,psys);
	if(!(psmd->modifier.mode & eModifierMode_Render))
		return 0;

	if(part->phystype==PART_PHYS_KEYED)
		psys_count_keyed_targets(&sim);


	if(G.rendering == 0) { /* preview render */
		totchild = (int)((float)totchild * (float)part->disp / 100.0f);
	}

	psys->flag |= PSYS_DRAWING;

	rng= rng_new(psys->seed);

	totpart=psys->totpart;

	memset(&sd, 0, sizeof(ParticleStrandData));
	sd.override_uv = -1;

/* 2.1 setup material stff */
	ma= give_render_material(re, ob, part->omat);
	
#if 0 // XXX old animation system
	if(ma->ipo){
		calc_ipo(ma->ipo, cfra);
		execute_ipo((ID *)ma, ma->ipo);
	}
#endif // XXX old animation system

	hasize = ma->hasize;
	seed = ma->seed1;

	re->params.flag |= R_HALO;

	render_object_customdata_set(obr, &psmd->dm->faceData);
	sd.totuv = CustomData_number_of_layers(&psmd->dm->faceData, CD_MTFACE);
	sd.totcol = CustomData_number_of_layers(&psmd->dm->faceData, CD_MCOL);

	if(ma->texco & TEXCO_UV && sd.totuv) {
		sd.uvco = MEM_callocN(sd.totuv * 2 * sizeof(float), "particle_uvs");

		if(ma->strand_uvname[0]) {
			sd.override_uv = CustomData_get_named_layer_index(&psmd->dm->faceData, CD_MTFACE, ma->strand_uvname);
			sd.override_uv -= CustomData_get_layer_index(&psmd->dm->faceData, CD_MTFACE);
		}
	}
	else
		sd.uvco = NULL;

	if(sd.totcol)
		sd.mcol = MEM_callocN(sd.totcol * sizeof(MCol), "particle_mcols");

/* 2.2 setup billboards */
	if(part->ren_as == PART_DRAW_BB) {
		int first_uv = CustomData_get_layer_index(&psmd->dm->faceData, CD_MTFACE);

		bb.uv[0] = CustomData_get_named_layer_index(&psmd->dm->faceData, CD_MTFACE, psys->bb_uvname[0]);
		if(bb.uv[0] < 0)
			bb.uv[0] = CustomData_get_active_layer_index(&psmd->dm->faceData, CD_MTFACE);

		bb.uv[1] = CustomData_get_named_layer_index(&psmd->dm->faceData, CD_MTFACE, psys->bb_uvname[1]);

		bb.uv[2] = CustomData_get_named_layer_index(&psmd->dm->faceData, CD_MTFACE, psys->bb_uvname[2]);

		if(first_uv >= 0) {
			bb.uv[0] -= first_uv;
			bb.uv[1] -= first_uv;
			bb.uv[2] -= first_uv;
		}

		bb.align = part->bb_align;
		bb.anim = part->bb_anim;
		bb.lock = part->draw & PART_DRAW_BB_LOCK;
		bb.ob = (part->bb_ob ? part->bb_ob : re->db.scene->camera);
		bb.offset[0] = part->bb_offset[0];
		bb.offset[1] = part->bb_offset[1];
		bb.split_offset = part->bb_split_offset;
		bb.totnum = totpart+totchild;
		bb.uv_split = part->bb_uv_split;
	}

#if 0 // XXX old animation system
/* 2.3 setup time */
	if(part->flag&PART_ABS_TIME && part->ipo) {
		calc_ipo(part->ipo, cfra);
		execute_ipo((ID *)part, part->ipo);
	}

	if(part->flag & PART_GLOB_TIME)
#endif // XXX old animation system
	cfra = bsystem_time(re->db.scene, 0, (float)re->db.scene->r.cfra, 0.0);

///* 2.4 setup reactors */
//	if(part->type == PART_REACTOR){
//		psys_get_reactor_target(ob, psys, &tob, &tpsys);
//		if(tpsys && (part->from==PART_FROM_PARTICLE || part->phystype==PART_PHYS_NO)){
//			psmd = psys_get_modifier(tob,tpsys);
//			tpart = tpsys->part;
//		}
//	}
	
/* 2.5 setup matrices */
	mul_m4_m4m4(mat, ob->obmat, re->cam.viewmat);
	invert_m4_m4(ob->imat, mat);	/* need to be that way, for imat texture */
	copy_m3_m4(nmat, ob->imat);
	transpose_m3(nmat);

/* 2.6 setup strand rendering */
	if(part->ren_as == PART_DRAW_PATH && psys->pathcache){
		path_nbr=(int)pow(2.0,(double) part->ren_step);

		if(path_nbr) {
			if(!ELEM(ma->material_type, MA_TYPE_HALO, MA_TYPE_WIRE)) {
				sd.orco = MEM_mallocN(3*sizeof(float)*(totpart+totchild), "particle orcos");
				set_object_orco(re, psys, sd.orco);
			}
		}

		if(part->draw & PART_DRAW_REN_ADAPT) {
			sd.adapt = 1;
			sd.adapt_pix = (float)part->adapt_pix;
			sd.adapt_angle = cos((float)part->adapt_angle * (float)(M_PI / 180.0));
		}

		if(re->params.r.renderer==R_INTERN && part->draw&PART_DRAW_REN_STRAND) {
			strandbuf= render_object_strand_buffer_add(obr, (totpart+totchild)*(path_nbr+1));
			strandbuf->ma= ma;
			strandbuf->lay= ob->lay;
			copy_m4_m4(strandbuf->winmat, re->cam.winmat);
			strandbuf->winx= re->cam.winx;
			strandbuf->winy= re->cam.winy;
			strandbuf->maxdepth= 2;
			strandbuf->adaptcos= cos((float)part->adapt_angle*(float)(M_PI/180.0));
			strandbuf->overrideuv= sd.override_uv;
			strandbuf->minwidth= ma->strand_min;

			if(ma->strand_widthfade == 0.0f)
				strandbuf->widthfade= 0.0f;
			else if(ma->strand_widthfade >= 1.0f)
				strandbuf->widthfade= 2.0f - ma->strand_widthfade;
			else
				strandbuf->widthfade= 1.0f/MAX2(ma->strand_widthfade, 1e-5f);

			if(part->flag & PART_HAIR_BSPLINE)
				strandbuf->flag |= R_STRAND_BSPLINE;
			if(ma->mode & MA_STR_B_UNITS)
				strandbuf->flag |= R_STRAND_B_UNITS;

			svert= strandbuf->vert;

			if(re->params.r.mode & R_SPEED)
				dosurfacecache= 1;
			else if((re->db.wrld.mode & WO_AMB_OCC) && (re->db.wrld.ao_gather_method == WO_AOGATHER_APPROX))
				if(ma->amb != 0.0f)
					dosurfacecache= 1;

			totface= psmd->dm->getNumFaces(psmd->dm);
			origindex= psmd->dm->getFaceDataArray(psmd->dm, CD_ORIGINDEX);
			for(a=0; a<totface; a++)
				strandbuf->totbound= MAX2(strandbuf->totbound, (origindex)? origindex[a]: a);
			strandbuf->totbound++;
			strandbuf->bound= MEM_callocN(sizeof(StrandBound)*strandbuf->totbound, "StrandBound");
			sbound= strandbuf->bound;
			sbound->start= sbound->end= 0;
		}
	}

	if(sd.orco == 0) {
		sd.orco = MEM_mallocN(3 * sizeof(float), "particle orco");
		orco1 = 1;
	}

	if(path_nbr == 0)
		psys->lattice = psys_get_lattice(&sim);

/* 3. start creating renderable things */
	for(a=0,pa=pars; a<totpart+totchild; a++, pa++, seed++) {
		random = rng_getFloat(rng);
		/* setup per particle individual stuff */
		if(a<totpart){
			if(pa->flag & PARS_UNEXIST) continue;

			pa_time=(cfra-pa->time)/pa->lifetime;
			pa_birthtime = pa->time;
			pa_dietime = pa->dietime;
#if 0 // XXX old animation system
			if((part->flag&PART_ABS_TIME) == 0){
				if(ma->ipo) {
					/* correction for lifetime */
					calc_ipo(ma->ipo, 100.0f * pa_time);
					execute_ipo((ID *)ma, ma->ipo);
				}
				if(part->ipo){
					/* correction for lifetime */
					calc_ipo(part->ipo, 100.0f*pa_time);
					execute_ipo((ID *)part, part->ipo);
				}
			}
#endif // XXX old animation system

			hasize = ma->hasize;

			/* get orco */
			if(tpsys && (part->from==PART_FROM_PARTICLE || part->phystype==PART_PHYS_NO)){
				tpa=tpsys->particles+pa->num;
				psys_particle_on_emitter(psmd,tpart->from,tpa->num,pa->num_dmcache,tpa->fuv,tpa->foffset,co,nor,0,0,sd.orco,0);
			}
			else
				psys_particle_on_emitter(psmd,part->from,pa->num,pa->num_dmcache,pa->fuv,pa->foffset,co,nor,0,0,sd.orco,0);

			/* get uvco & mcol */
			num= pa->num_dmcache;

			if(num == DMCACHE_NOTFOUND)
				if(pa->num < psmd->dm->getNumFaces(psmd->dm))
					num= pa->num;

			get_particle_uvco_mcol(part->from, psmd->dm, pa->fuv, num, &sd);

			pa_size = pa->size;

			BLI_srandom(psys->seed+a);

			r_tilt = 2.0f*(BLI_frand() - 0.5f);
			r_length = BLI_frand();

			if(path_nbr) {
				cache = psys->pathcache[a];
				max_k = (int)cache->steps;
			}

			if(totchild && (part->draw&PART_DRAW_PARENT)==0) continue;
		}
		else {
			ChildParticle *cpa= psys->child+a-totpart;

			if(path_nbr) {
				cache = psys->childcache[a-totpart];

				if(cache->steps < 0)
					continue;

				max_k = (int)cache->steps;
			}
			
			pa_time = psys_get_child_time(psys, cpa, cfra, &pa_birthtime, &pa_dietime);

#if 0 // XXX old animation system
			if((part->flag & PART_ABS_TIME) == 0) {
				if(ma->ipo){
					/* correction for lifetime */
					calc_ipo(ma->ipo, 100.0f * pa_time);
					execute_ipo((ID *)ma, ma->ipo);
				}
				if(part->ipo) {
					/* correction for lifetime */
					calc_ipo(part->ipo, 100.0f * pa_time);
					execute_ipo((ID *)part, part->ipo);
				}
			}
#endif // XXX old animation system

			pa_size = psys_get_child_size(psys, cpa, cfra, &pa_time);

			r_tilt = 2.0f*(PSYS_FRAND(a + 21) - 0.5f);
			r_length = PSYS_FRAND(a + 22);

			num = cpa->num;

			/* get orco */
			if(part->childtype == PART_CHILD_FACES) {
				psys_particle_on_emitter(psmd,
					PART_FROM_FACE, cpa->num,DMCACHE_ISCHILD,
					cpa->fuv,cpa->foffset,co,nor,0,0,sd.orco,0);
			}
			else {
				ParticleData *par = psys->particles + cpa->parent;
				psys_particle_on_emitter(psmd, part->from,
					par->num,DMCACHE_ISCHILD,par->fuv,
					par->foffset,co,nor,0,0,sd.orco,0);
			}

			/* get uvco & mcol */
			if(part->from!=PART_FROM_PARTICLE && part->childtype==PART_CHILD_FACES) {
				get_particle_uvco_mcol(PART_FROM_FACE, psmd->dm, cpa->fuv, cpa->num, &sd);
			}
			else {
				ParticleData *parent = psys->particles + cpa->parent;
				num = parent->num_dmcache;

				if(num == DMCACHE_NOTFOUND)
					if(parent->num < psmd->dm->getNumFaces(psmd->dm))
						num = parent->num;

				get_particle_uvco_mcol(part->from, psmd->dm, parent->fuv, num, &sd);
			}

			dosimplify = psys_render_simplify_params(psys, cpa, simplify);

			if(strandbuf) {
				int orignum= (origindex)? origindex[cpa->num]: cpa->num;

				if(orignum > sbound - strandbuf->bound) {
					sbound= strandbuf->bound + orignum;
					sbound->start= sbound->end= obr->totstrand;
				}
			}
		}

		/* surface normal shading setup */
		if(ma->mode_l & MA_STR_SURFDIFF) {
			mul_m3_v3(nmat, nor);
			sd.surfnor= nor;
		}
		else
			sd.surfnor= NULL;

		/* strand render setup */
		if(strandbuf) {
			strand= render_object_strand_get(obr, obr->totstrand++);
			strand->buffer= strandbuf;
			strand->vert= svert;
			copy_v3_v3(strand->orco, sd.orco);

			if(dosimplify) {
				float *ssimplify= render_strand_get_simplify(obr, strand, 1);
				ssimplify[0]= simplify[0];
				ssimplify[1]= simplify[1];
			}

			if(sd.surfnor) {
				float *snor= render_strand_get_surfnor(obr, strand, 1);
				copy_v3_v3(snor, sd.surfnor);
			}

			if(dosurfacecache && num >= 0) {
				int *facenum= render_strand_get_face(obr, strand, 1);
				*facenum= num;
			}

			if(sd.uvco) {
				for(i=0; i<sd.totuv; i++) {
					if(i != sd.override_uv) {
						float *uv= render_strand_get_uv(obr, strand, i, NULL, 1);

						uv[0]= sd.uvco[2*i];
						uv[1]= sd.uvco[2*i+1];
					}
				}
			}
			if(sd.mcol) {
				for(i=0; i<sd.totcol; i++) {
					MCol *mc= render_strand_get_mcol(obr, strand, i, NULL, 1);
					*mc = sd.mcol[i];
				}
			}

			sbound->end++;
		}

		/* strandco computation setup */
		if(path_nbr) {
			strandlen= 0.0f;
			curlen= 0.0f;
			for(k=1; k<=path_nbr; k++)
				if(k<=max_k)
					strandlen += len_v3v3((cache+k-1)->co, (cache+k)->co);
		}

		if(path_nbr) {
			/* render strands */
			for(k=0; k<=path_nbr; k++){
				if(k<=max_k){
					copy_v3_v3(state.co,(cache+k)->co);
					copy_v3_v3(state.vel,(cache+k)->vel);
				}
				else
					continue;	

				if(k > 0)
					curlen += len_v3v3((cache+k-1)->co, (cache+k)->co);
				time= curlen/strandlen;

				copy_v3_v3(loc,state.co);
				mul_m4_v3(re->cam.viewmat,loc);

				if(strandbuf) {
					copy_v3_v3(svert->co, loc);
					svert->strandco= -1.0f + 2.0f*time;
					svert++;
					strand->totvert++;
				}
				else{
					sd.size = hasize;

					if(k==1){
						sd.first = 1;
						sd.time = 0.0f;
						sub_v3_v3v3(loc0,loc1,loc);
						add_v3_v3v3(loc0,loc1,loc0);

						particle_curve(re, obr, psmd->dm, ma, &sd, loc1, loc0, seed);
					}

					sd.first = 0;
					sd.time = time;

					if(k)
						particle_curve(re, obr, psmd->dm, ma, &sd, loc, loc1, seed);

					copy_v3_v3(loc1,loc);
				}
			}

		}
		else {
			/* render normal particles */
			if(part->trail_count > 1) {
				float length = part->path_end * (1.0 - part->randlength * r_length);
				int trail_count = part->trail_count * (1.0 - part->randlength * r_length);
				float ct = (part->draw & PART_ABS_PATH_TIME) ? cfra : pa_time;
				float dt = length / (trail_count ? (float)trail_count : 1.0f);

				for(i=0; i < trail_count; i++, ct -= dt) {
					if(part->draw & PART_ABS_PATH_TIME) {
						if(ct < pa_birthtime || ct > pa_dietime)
							continue;
					}
					else if(ct < 0.0f || ct > 1.0f)
						continue;

					state.time = (part->draw & PART_ABS_PATH_TIME) ? -ct : ct;
					psys_get_particle_on_path(&sim,a,&state,1);

					if(psys->parent)
						mul_m4_v3(psys->parent->obmat, state.co);

					if(part->ren_as == PART_DRAW_BB) {
						bb.random = random;
						bb.size = pa_size;
						bb.tilt = part->bb_tilt * (1.0f - part->bb_rand_tilt * r_tilt);
						bb.time = ct;
						bb.num = a;
					}

					particle_normal_ren(part->ren_as, part, re, obr, psmd->dm, ma, &sd, &bb, &state, seed, hasize);
				}
			}
			else {
				time=0.0f;
				state.time=cfra;
				if(psys_get_particle_state(&sim,a,&state,0)==0)
					continue;

				if(psys->parent)
					mul_m4_v3(psys->parent->obmat, state.co);

				if(part->ren_as == PART_DRAW_BB) {
					bb.random = random;
					bb.size = pa_size;
					bb.tilt = part->bb_tilt * (1.0f - part->bb_rand_tilt * r_tilt);
					bb.time = pa_time;
					bb.num = a;
				}

				particle_normal_ren(part->ren_as, part, re, obr, psmd->dm, ma, &sd, &bb, &state, seed, hasize);
			}
		}

		if(orco1==0)
			sd.orco+=3;

		if(re->cb.test_break(re->cb.tbh))
			break;
	}

	if(dosurfacecache)
		strandbuf->surface= cache_strand_surface(re, obr, psmd->dm, mat, timeoffset);

/* 4. clean up */
#if 0 // XXX old animation system
	if(ma) do_mat_ipo(re->db.scene, ma);
#endif // XXX old animation system
	
	if(orco1)
		MEM_freeN(sd.orco);

	if(sd.uvco)
		MEM_freeN(sd.uvco);
	
	if(sd.mcol)
		MEM_freeN(sd.mcol);

	if(uv_name)
		MEM_freeN(uv_name);

	if(states)
		MEM_freeN(states);
	
	rng_free(rng);

	psys->flag &= ~PSYS_DRAWING;

	if(psys->lattice){
		end_latt_deform(psys->lattice);
		psys->lattice= NULL;
	}

	if(path_nbr && (ma->mode_l & MA_TANGENT_STR)==0)
		calc_vertexnormals(re, obr, 0, 0);

	return 1;
}

/* ------------------------------------------------------------------------- */
/* Halo's   																 */
/* ------------------------------------------------------------------------- */

static void make_render_halos(Render *re, ObjectRen *obr, Mesh *me, int totvert, MVert *mvert, Material *ma, float *orco)
{
	Object *ob= obr->ob;
	HaloRen *har;
	float xn, yn, zn, nor[3], view[3];
	float vec[3], hasize, mat[4][4], imat[3][3];
	int a, ok, seed= ma->seed1;

	mul_m4_m4m4(mat, ob->obmat, re->cam.viewmat);
	copy_m3_m4(imat, ob->imat);

	re->params.flag |= R_HALO;

	for(a=0; a<totvert; a++, mvert++) {
		ok= 1;

		if(ok) {
			hasize= ma->hasize;

			copy_v3_v3(vec, mvert->co);
			mul_m4_v3(mat, vec);

			if(ma->mode & MA_HALOPUNO) {
				xn= mvert->no[0];
				yn= mvert->no[1];
				zn= mvert->no[2];

				/* transpose ! */
				nor[0]= imat[0][0]*xn+imat[0][1]*yn+imat[0][2]*zn;
				nor[1]= imat[1][0]*xn+imat[1][1]*yn+imat[1][2]*zn;
				nor[2]= imat[2][0]*xn+imat[2][1]*yn+imat[2][2]*zn;
				normalize_v3(nor);

				copy_v3_v3(view, vec);
				normalize_v3(view);

				zn= nor[0]*view[0]+nor[1]*view[1]+nor[2]*view[2];
				if(zn>=0.0) hasize= 0.0;
				else hasize*= zn*zn*zn*zn;
			}

			if(orco) har= halo_init(re, obr, ma, vec, NULL, orco, hasize, 0.0, seed);
			else har= halo_init(re, obr, ma, vec, NULL, mvert->co, hasize, 0.0, seed);
			if(har) har->lay= ob->lay;
		}
		if(orco) orco+= 3;
		seed++;
	}
}

/* ------------------------------------------------------------------------- */
/* Displacement Mapping														 */
/* ------------------------------------------------------------------------- */

static short test_for_displace(Render *re, Object *ob)
{
	/* return 1 when this object uses displacement textures. */
	Material *ma;
	int i;
	
	for (i=1; i<=ob->totcol; i++) {
		ma=give_render_material(re, ob, i);
		/* ma->mapto is ORed total of all mapto channels */
		if(ma && (ma->mapto & MAP_DISPLACE)) return 1;
	}
	return 0;
}

static void displace_render_vert(Render *re, ObjectRen *obr, ShadeInput *shi, VertRen *vr, int vindex, float *scale, float mat[][4], float imat[][3])
{
	MTFace *tface;
	short texco= shi->material.mat->texco;
	float sample=0, displace[3];
	char *name;
	int i;

	/* shi->geometry.co is current render coord, just make sure at least some vector is here */
	copy_v3_v3(shi->geometry.co, vr->co);
	/* vertex normal is used for textures type 'col' and 'var' */
	copy_v3_v3(shi->geometry.vn, vr->n);

	if(mat)
		mul_m4_v3(mat, shi->geometry.co);

	if(imat) {
		shi->geometry.vn[0]= imat[0][0]*vr->n[0]+imat[0][1]*vr->n[1]+imat[0][2]*vr->n[2];
		shi->geometry.vn[1]= imat[1][0]*vr->n[0]+imat[1][1]*vr->n[1]+imat[1][2]*vr->n[2];
		shi->geometry.vn[2]= imat[2][0]*vr->n[0]+imat[2][1]*vr->n[1]+imat[2][2]*vr->n[2];
	}

	if (texco & TEXCO_UV) {
		shi->texture.totuv= 0;
		shi->texture.actuv= obr->actmtface;

		for (i=0; (tface=render_vlak_get_tface(obr, shi->primitive.vlr, i, &name, 0)); i++) {
			ShadeInputUV *suv= &shi->texture.uv[i];

			/* shi.uv needs scale correction from tface uv */
			suv->uv[0]= 2*tface->uv[vindex][0]-1.0f;
			suv->uv[1]= 2*tface->uv[vindex][1]-1.0f;
			suv->uv[2]= 0.0f;
			suv->name= name;
			shi->texture.totuv++;
		}
	}

	/* set all rendercoords, 'texco' is an ORed value for all textures needed */
	if ((texco & TEXCO_ORCO) && (vr->orco)) {
		copy_v3_v3(shi->texture.lo, vr->orco);
	}
	if (texco & TEXCO_STICKY) {
		float *sticky= render_vert_get_sticky(obr, vr, 0);
		if(sticky) {
			shi->texture.sticky[0]= sticky[0];
			shi->texture.sticky[1]= sticky[1];
			shi->texture.sticky[2]= 0.0f;
		}
	}
	if (texco & TEXCO_GLOB) {
		copy_v3_v3(shi->texture.gl, shi->geometry.co);
		mul_m4_v3(re->cam.viewinv, shi->texture.gl);
	}
	if (texco & TEXCO_NORM) {
		copy_v3_v3(shi->texture.orn, shi->geometry.vn);
	}
	if(texco & TEXCO_REFL) {
		/* not (yet?) */
	}
	
	mat_displacement(re, shi, displace);
	
	//printf("no=%f, %f, %f\nbefore co=%f, %f, %f\n", vr->n[0], vr->n[1], vr->n[2], 
	//vr->co[0], vr->co[1], vr->co[2]);

	mul_v3_v3(displace, scale);
	
	if(mat)
		mul_m3_v3(imat, displace);

	/* 0.5 could become button once?  */
	vr->co[0] += displace[0]; 
	vr->co[1] += displace[1];
	vr->co[2] += displace[2];
	
	//printf("after co=%f, %f, %f\n", vr->co[0], vr->co[1], vr->co[2]); 
	
	/* we just don't do this vertex again, bad luck for other face using same vertex with
		different material... */
	vr->flag |= 1;
	
	/* Pass sample back so displace_face can decide which way to split the quad */
	sample  = shi->texture.displace[0]*shi->texture.displace[0];
	sample += shi->texture.displace[1]*shi->texture.displace[1];
	sample += shi->texture.displace[2]*shi->texture.displace[2];
	
	vr->accum=sample; 
	/* Should be sqrt(sample), but I'm only looking for "bigger".  Save the cycles. */
	return;
}

static void displace_render_face(Render *re, ObjectRen *obr, VlakRen *vlr, float *scale, float mat[][4], float imat[][3])
{
	ShadeInput shi;

	/* Warning, This is not that nice, and possibly a bit slow,
	however some variables were not initialized properly in, unless using shade_input_initialize(...), we need to do a memset */
	memset(&shi, 0, sizeof(ShadeInput)); 
	/* end warning! - Campbell */
	
	/* set up shadeinput struct for multitex() */
	
	/* memset above means we dont need this */
	/*shi.osatex= 0;*/		/* signal not to use dx[] and dy[] texture AA vectors */

	shi.primitive.obr= obr;
	shi.primitive.vlr= vlr;		/* current render face */
	shi.material.mat= vlr->mat;		/* current input material */
	shi.shading.thread= 0;

	/* TODO, assign these, displacement with new bumpmap is skipped without - campbell */
#if 0
	/* order is not known ? */
	shi.primitive.v1= vlr->v1;
	shi.primitive.v2= vlr->v2;
	shi.primitive.v3= vlr->v3;
#endif
	
	/* Displace the verts, flag is set when done */
	if (!vlr->v1->flag)
		displace_render_vert(re, obr, &shi, vlr->v1,0,  scale, mat, imat);
	
	if (!vlr->v2->flag)
		displace_render_vert(re, obr, &shi, vlr->v2, 1, scale, mat, imat);

	if (!vlr->v3->flag)
		displace_render_vert(re, obr, &shi, vlr->v3, 2, scale, mat, imat);

	if (vlr->v4) {
		if (!vlr->v4->flag)
			displace_render_vert(re, obr, &shi, vlr->v4, 3, scale, mat, imat);

		/*	closest in displace value.  This will help smooth edges.   */ 
		if ( fabs(vlr->v1->accum - vlr->v3->accum) > fabs(vlr->v2->accum - vlr->v4->accum)) 
			vlr->flag |= R_DIVIDE_24;
		else vlr->flag &= ~R_DIVIDE_24;
	}
	
	/* Recalculate the face normal  - if flipped before, flip now */
	if(vlr->v4) {
		normal_quad_v3( vlr->n,vlr->v4->co, vlr->v3->co, vlr->v2->co, vlr->v1->co);
	}	
	else {
		normal_tri_v3( vlr->n,vlr->v3->co, vlr->v2->co, vlr->v1->co);
	}
}

static void do_displacement(Render *re, ObjectRen *obr, float mat[][4], float imat[][3])
{
	VertRen *vr;
	VlakRen *vlr;
//	float min[3]={1e30, 1e30, 1e30}, max[3]={-1e30, -1e30, -1e30};
	float scale[3]={1.0f, 1.0f, 1.0f}, temp[3];//, xn
	int i; //, texflag=0;
	Object *obt;
		
	/* Object Size with parenting */
	obt=obr->ob;
	while(obt){
		add_v3_v3v3(temp, obt->size, obt->dsize);
		scale[0]*=temp[0]; scale[1]*=temp[1]; scale[2]*=temp[2];
		obt=obt->parent;
	}
	
	/* Clear all flags */
	for(i=0; i<obr->totvert; i++){ 
		vr= render_object_vert_get(obr, i);
		vr->flag= 0;
	}

	for(i=0; i<obr->totvlak; i++){
		vlr=render_object_vlak_get(obr, i);
		displace_render_face(re, obr, vlr, scale, mat, imat);
	}
	
	/* Recalc vertex normals */
	calc_vertexnormals(re, obr, 0, 0);
}

/* ------------------------------------------------------------------------- */
/* Metaball   																 */
/* ------------------------------------------------------------------------- */

static void init_render_mball(Render *re, ObjectRen *obr)
{
	Object *ob= obr->ob;
	DispList *dl;
	VertRen *ver;
	VlakRen *vlr, *vlr1;
	Material *ma;
	float *data, *nors, *orco, mat[4][4], imat[3][3], xn, yn, zn;
	int a, need_orco, vlakindex, *index;

	if (ob!=find_basis_mball(re->db.scene, ob))
		return;

	mul_m4_m4m4(mat, ob->obmat, re->cam.viewmat);
	invert_m4_m4(ob->imat, mat);
	copy_m3_m4(imat, ob->imat);

	ma= give_render_material(re, ob, 1);

	need_orco= 0;
	if(ma->texco & TEXCO_ORCO) {
		need_orco= 1;
	}
	
	makeDispListMBall(re->db.scene, ob);
	dl= ob->disp.first;
	if(dl==0) return;

	data= dl->verts;
	nors= dl->nors;
	orco= get_object_orco(re, ob);

	for(a=0; a<dl->nr; a++, data+=3, nors+=3, orco+=3) {

		ver= render_object_vert_get(obr, obr->totvert++);
		copy_v3_v3(ver->co, data);
		mul_m4_v3(mat, ver->co);

		/* render normals are inverted */
		xn= -nors[0];
		yn= -nors[1];
		zn= -nors[2];

		/* transpose ! */
		ver->n[0]= imat[0][0]*xn+imat[0][1]*yn+imat[0][2]*zn;
		ver->n[1]= imat[1][0]*xn+imat[1][1]*yn+imat[1][2]*zn;
		ver->n[2]= imat[2][0]*xn+imat[2][1]*yn+imat[2][2]*zn;
		normalize_v3(ver->n);
		//if(ob->transflag & OB_NEG_SCALE) mul_v3_fl(ver->n. -1.0);
		
		if(need_orco) ver->orco= orco;
	}

	index= dl->index;
	for(a=0; a<dl->parts; a++, index+=4) {

		vlr= render_object_vlak_get(obr, obr->totvlak++);
		vlr->v1= render_object_vert_get(obr, index[0]);
		vlr->v2= render_object_vert_get(obr, index[1]);
		vlr->v3= render_object_vert_get(obr, index[2]);
		vlr->v4= 0;

		if(ob->transflag & OB_NEG_SCALE) 
			normal_tri_v3( vlr->n,vlr->v1->co, vlr->v2->co, vlr->v3->co);
		else
			normal_tri_v3( vlr->n,vlr->v3->co, vlr->v2->co, vlr->v1->co);

		vlr->mat= ma;
		vlr->flag= ME_SMOOTH+R_NOPUNOFLIP;
		vlr->ec= 0;

		/* mball -too bad- always has triangles, because quads can be non-planar */
		if(index[3] && index[3]!=index[2]) {
			vlr1= render_object_vlak_get(obr, obr->totvlak++);
			vlakindex= vlr1->index;
			*vlr1= *vlr;
			vlr1->index= vlakindex;
			vlr1->v2= vlr1->v3;
			vlr1->v3= render_object_vert_get(obr, index[3]);
			if(ob->transflag & OB_NEG_SCALE) 
				normal_tri_v3( vlr1->n,vlr1->v1->co, vlr1->v2->co, vlr1->v3->co);
			else
				normal_tri_v3( vlr1->n,vlr1->v3->co, vlr1->v2->co, vlr1->v1->co);
		}
	}

	/* enforce display lists remade */
	freedisplist(&ob->disp);
	
	/* this enforces remake for real, orco displist is small (in scale) */
	ob->recalc |= OB_RECALC_DATA;
}

/* ------------------------------------------------------------------------- */
/* Surfaces and Curves														 */
/* ------------------------------------------------------------------------- */

/* returns amount of vertices added for orco */
static int dl_surf_to_renderdata(ObjectRen *obr, DispList *dl, Material **matar, float *orco, float mat[4][4])
{
	Object *ob= obr->ob;
	VertRen *v1, *v2, *v3, *v4, *ver;
	VlakRen *vlr, *vlr1, *vlr2, *vlr3;
	Curve *cu= ob->data;
	float *data, n1[3];
	int u, v, orcoret= 0;
	int p1, p2, p3, p4, a;
	int sizeu, nsizeu, sizev, nsizev;
	int startvert, startvlak;
	
	startvert= obr->totvert;
	nsizeu = sizeu = dl->parts; nsizev = sizev = dl->nr; 
	
	data= dl->verts;
	for (u = 0; u < sizeu; u++) {
		v1 = render_object_vert_get(obr, obr->totvert++); /* save this for possible V wrapping */
		copy_v3_v3(v1->co, data); data += 3;
		if(orco) {
			v1->orco= orco; orco+= 3; orcoret++;
		}	
		mul_m4_v3(mat, v1->co);
		
		for (v = 1; v < sizev; v++) {
			ver= render_object_vert_get(obr, obr->totvert++);
			copy_v3_v3(ver->co, data); data += 3;
			if(orco) {
				ver->orco= orco; orco+= 3; orcoret++;
			}	
			mul_m4_v3(mat, ver->co);
		}
		/* if V-cyclic, add extra vertices at end of the row */
		if (dl->flag & DL_CYCL_U) {
			ver= render_object_vert_get(obr, obr->totvert++);
			copy_v3_v3(ver->co, v1->co);
			if(orco) {
				ver->orco= orco; orco+=3; orcoret++; //orcobase + 3*(u*sizev + 0);
			}
		}	
	}	
	
	/* Done before next loop to get corner vert */
	if (dl->flag & DL_CYCL_U) nsizev++;
	if (dl->flag & DL_CYCL_V) nsizeu++;
	
	/* if U cyclic, add extra row at end of column */
	if (dl->flag & DL_CYCL_V) {
		for (v = 0; v < nsizev; v++) {
			v1= render_object_vert_get(obr, startvert + v);
			ver= render_object_vert_get(obr, obr->totvert++);
			copy_v3_v3(ver->co, v1->co);
			if(orco) {
				ver->orco= orco; orco+=3; orcoret++; //ver->orco= orcobase + 3*(0*sizev + v);
			}
		}
	}
	
	sizeu = nsizeu;
	sizev = nsizev;
	
	startvlak= obr->totvlak;
	
	for(u = 0; u < sizeu - 1; u++) {
		p1 = startvert + u * sizev; /* walk through face list */
		p2 = p1 + 1;
		p3 = p2 + sizev;
		p4 = p3 - 1;
		
		for(v = 0; v < sizev - 1; v++) {
			v1= render_object_vert_get(obr, p1);
			v2= render_object_vert_get(obr, p2);
			v3= render_object_vert_get(obr, p3);
			v4= render_object_vert_get(obr, p4);
			
			vlr= render_object_vlak_get(obr, obr->totvlak++);
			vlr->v1= v1; vlr->v2= v2; vlr->v3= v3; vlr->v4= v4;
			
			normal_quad_v3( n1,vlr->v4->co, vlr->v3->co, vlr->v2->co, vlr->v1->co);
			
			copy_v3_v3(vlr->n, n1);
			
			vlr->mat= matar[ dl->col];
			vlr->ec= ME_V1V2+ME_V2V3;
			vlr->flag= dl->rt;
			if( (cu->flag & CU_NOPUNOFLIP) ) {
				vlr->flag |= R_NOPUNOFLIP;
			}
			
			add_v3_v3v3(v1->n, v1->n, n1);
			add_v3_v3v3(v2->n, v2->n, n1);
			add_v3_v3v3(v3->n, v3->n, n1);
			add_v3_v3v3(v4->n, v4->n, n1);
			
			p1++; p2++; p3++; p4++;
		}
	}	
	/* fix normals for U resp. V cyclic faces */
	sizeu--; sizev--;  /* dec size for face array */
	if (dl->flag & DL_CYCL_V) {
		
		for (v = 0; v < sizev; v++)
		{
			/* optimize! :*/
			vlr= render_object_vlak_get(obr, UVTOINDEX(sizeu - 1, v));
			vlr1= render_object_vlak_get(obr, UVTOINDEX(0, v));
			add_v3_v3v3(vlr1->v1->n, vlr1->v1->n, vlr->n);
			add_v3_v3v3(vlr1->v2->n, vlr1->v2->n, vlr->n);
			add_v3_v3v3(vlr->v3->n, vlr->v3->n, vlr1->n);
			add_v3_v3v3(vlr->v4->n, vlr->v4->n, vlr1->n);
		}
	}
	if (dl->flag & DL_CYCL_U) {
		
		for (u = 0; u < sizeu; u++)
		{
			/* optimize! :*/
			vlr= render_object_vlak_get(obr, UVTOINDEX(u, 0));
			vlr1= render_object_vlak_get(obr, UVTOINDEX(u, sizev-1));
			add_v3_v3v3(vlr1->v2->n, vlr1->v2->n, vlr->n);
			add_v3_v3v3(vlr1->v3->n, vlr1->v3->n, vlr->n);
			add_v3_v3v3(vlr->v1->n, vlr->v1->n, vlr1->n);
			add_v3_v3v3(vlr->v4->n, vlr->v4->n, vlr1->n);
		}
	}
	/* last vertex is an extra case: 
		
		^	()----()----()----()
		|	|     |     ||     |
		u	|     |(0,n)||(0,0)|
		|     |     ||     |
		()====()====[]====()
		|     |     ||     |
		|     |(m,n)||(m,0)|
		|     |     ||     |
		()----()----()----()
		v ->
		
		vertex [] is no longer shared, therefore distribute
		normals of the surrounding faces to all of the duplicates of []
		*/
	
	if ((dl->flag & DL_CYCL_V) && (dl->flag & DL_CYCL_U))
	{
		vlr= render_object_vlak_get(obr, UVTOINDEX(sizeu - 1, sizev - 1)); /* (m,n) */
		vlr1= render_object_vlak_get(obr, UVTOINDEX(0,0));  /* (0,0) */
		add_v3_v3v3(n1, vlr->n, vlr1->n);
		vlr2= render_object_vlak_get(obr, UVTOINDEX(0, sizev-1)); /* (0,n) */
		add_v3_v3v3(n1, n1, vlr2->n);
		vlr3= render_object_vlak_get(obr, UVTOINDEX(sizeu-1, 0)); /* (m,0) */
		add_v3_v3v3(n1, n1, vlr3->n);
		copy_v3_v3(vlr->v3->n, n1);
		copy_v3_v3(vlr1->v1->n, n1);
		copy_v3_v3(vlr2->v2->n, n1);
		copy_v3_v3(vlr3->v4->n, n1);
	}
	for(a = startvert; a < obr->totvert; a++) {
		ver= render_object_vert_get(obr, a);
		normalize_v3(ver->n);
	}
	
	
	return orcoret;
}

static void init_render_surf(Render *re, ObjectRen *obr)
{
	Object *ob= obr->ob;
	Nurb *nu=0;
	Curve *cu;
	ListBase displist;
	DispList *dl;
	Material **matar;
	float *orco=NULL, *orcobase=NULL, mat[4][4];
	int a, totmat, need_orco=0;

	cu= ob->data;
	nu= cu->nurb.first;
	if(nu==0) return;

	mul_m4_m4m4(mat, ob->obmat, re->cam.viewmat);
	invert_m4_m4(ob->imat, mat);

	/* material array */
	totmat= ob->totcol+1;
	matar= MEM_callocN(sizeof(Material*)*totmat, "init_render_surf matar");

	for(a=0; a<totmat; a++) {
		matar[a]= give_render_material(re, ob, a+1);

		if(matar[a] && matar[a]->texco & TEXCO_ORCO)
			need_orco= 1;
	}

	if(ob->parent && (ob->parent->type==OB_LATTICE)) need_orco= 1;

	if(need_orco) orcobase= orco= get_object_orco(re, ob);

	displist.first= displist.last= 0;
	makeDispListSurf(re->db.scene, ob, &displist, 1, 0);

	/* walk along displaylist and create rendervertices/-faces */
	for(dl=displist.first; dl; dl=dl->next) {
		/* watch out: u ^= y, v ^= x !! */
		if(dl->type==DL_SURF)
			orco+= 3*dl_surf_to_renderdata(obr, dl, matar, orco, mat);
	}

	freedisplist(&displist);
	MEM_freeN(matar);
}

static void init_render_curve(Render *re, ObjectRen *obr, int timeoffset)
{
	Object *ob= obr->ob;
	Curve *cu;
	VertRen *ver;
	VlakRen *vlr;
	DispList *dl;
	ListBase olddl={NULL, NULL};
	Material **matar;
	float len, *data, *fp, *orco=NULL, *orcobase= NULL;
	float n[3], mat[4][4];
	int nr, startvert, startvlak, a, b;
	int frontside, need_orco=0, totmat;

	cu= ob->data;
	if(ob->type==OB_FONT && cu->str==NULL) return;
	else if(ob->type==OB_CURVE && cu->nurb.first==NULL) return;

	/* no modifier call here, is in makedisp */

	if(cu->resolu_ren) 
		SWAP(ListBase, olddl, cu->disp);
	
	/* test displist */
	if(cu->disp.first==NULL) 
		makeDispListCurveTypes(re->db.scene, ob, 0);
	dl= cu->disp.first;
	if(cu->disp.first==NULL) return;
	
	mul_m4_m4m4(mat, ob->obmat, re->cam.viewmat);
	invert_m4_m4(ob->imat, mat);

	/* material array */
	totmat= ob->totcol+1;
	matar= MEM_callocN(sizeof(Material*)*totmat, "init_render_surf matar");

	for(a=0; a<totmat; a++) {
		matar[a]= give_render_material(re, ob, a+1);

		if(matar[a] && matar[a]->texco & TEXCO_ORCO)
			need_orco= 1;
	}

	if(need_orco) orcobase=orco= get_object_orco(re, ob);

	dl= cu->disp.first;
	while(dl) {
		if(dl->type==DL_INDEX3) {
			int *index;

			startvert= obr->totvert;
			data= dl->verts;

			n[0]= ob->imat[0][2];
			n[1]= ob->imat[1][2];
			n[2]= ob->imat[2][2];
			normalize_v3(n);

			for(a=0; a<dl->nr; a++, data+=3) {
				ver= render_object_vert_get(obr, obr->totvert++);
				copy_v3_v3(ver->co, data);

				/* flip normal if face is backfacing, also used in face loop below */
				if(ver->co[2] < 0.0) {
					copy_v3_v3(ver->n, n);
					ver->flag = 1;
				}
				else {
					ver->n[0]= -n[0]; ver->n[1]= -n[1]; ver->n[2]= -n[2];
					ver->flag = 0;
				}

				mul_m4_v3(mat, ver->co);
				
				if (orco) {
					ver->orco = orco;
					orco += 3;
				}
			}
			
			if(timeoffset==0) {
				startvlak= obr->totvlak;
				index= dl->index;
				for(a=0; a<dl->parts; a++, index+=3) {

					vlr= render_object_vlak_get(obr, obr->totvlak++);
					vlr->v1= render_object_vert_get(obr, startvert+index[0]);
					vlr->v2= render_object_vert_get(obr, startvert+index[1]);
					vlr->v3= render_object_vert_get(obr, startvert+index[2]);
					vlr->v4= NULL;
					
					if(vlr->v1->flag) {
						copy_v3_v3(vlr->n, n);
					}
					else {
						vlr->n[0]= -n[0]; vlr->n[1]= -n[1]; vlr->n[2]= -n[2];
					}
					
					vlr->mat= matar[ dl->col ];
					vlr->flag= 0;
					if( (cu->flag & CU_NOPUNOFLIP) ) {
						vlr->flag |= R_NOPUNOFLIP;
					}
					vlr->ec= 0;
				}
			}
		}
		else if (dl->type==DL_SURF) {
			
			/* cyclic U means an extruded full circular curve, we skip bevel splitting then */
			if (dl->flag & DL_CYCL_U) {
				orco+= 3*dl_surf_to_renderdata(obr, dl, matar, orco, mat);
			}
			else {
				int p1,p2,p3,p4;

				fp= dl->verts;
				startvert= obr->totvert;
				nr= dl->nr*dl->parts;

				while(nr--) {
					ver= render_object_vert_get(obr, obr->totvert++);
						
					copy_v3_v3(ver->co, fp);
					mul_m4_v3(mat, ver->co);
					fp+= 3;

					if (orco) {
						ver->orco = orco;
						orco += 3;
					}
				}

				if(dl->bevelSplitFlag || timeoffset==0) {
					startvlak= obr->totvlak;

					for(a=0; a<dl->parts; a++) {

						frontside= (a >= dl->nr/2);
						
						if (surfindex_displist(dl, a, &b, &p1, &p2, &p3, &p4)==0)
							break;
						
						p1+= startvert;
						p2+= startvert;
						p3+= startvert;
						p4+= startvert;

						for(; b<dl->nr; b++) {
							vlr= render_object_vlak_get(obr, obr->totvlak++);
							vlr->v1= render_object_vert_get(obr, p2);
							vlr->v2= render_object_vert_get(obr, p1);
							vlr->v3= render_object_vert_get(obr, p3);
							vlr->v4= render_object_vert_get(obr, p4);
							vlr->ec= ME_V2V3+ME_V3V4;
							if(a==0) vlr->ec+= ME_V1V2;

							vlr->flag= dl->rt;

							/* this is not really scientific: the vertices
								* 2, 3 en 4 seem to give better vertexnormals than 1 2 3:
								* front and backside treated different!!
								*/

							if(frontside)
								normal_tri_v3( vlr->n,vlr->v2->co, vlr->v3->co, vlr->v4->co);
							else 
								normal_tri_v3( vlr->n,vlr->v1->co, vlr->v2->co, vlr->v3->co);

							vlr->mat= matar[ dl->col ];

							p4= p3;
							p3++;
							p2= p1;
							p1++;
						}
					}

					if (dl->bevelSplitFlag) {
						for(a=0; a<dl->parts-1+!!(dl->flag&DL_CYCL_V); a++)
							if(dl->bevelSplitFlag[a>>5]&(1<<(a&0x1F)))
								split_v_renderfaces(obr, startvlak, startvert, dl->parts, dl->nr, a, dl->flag&DL_CYCL_V, dl->flag&DL_CYCL_U);
					}

					/* vertex normals */
					for(a= startvlak; a<obr->totvlak; a++) {
						vlr= render_object_vlak_get(obr, a);

						add_v3_v3v3(vlr->v1->n, vlr->v1->n, vlr->n);
						add_v3_v3v3(vlr->v3->n, vlr->v3->n, vlr->n);
						add_v3_v3v3(vlr->v2->n, vlr->v2->n, vlr->n);
						add_v3_v3v3(vlr->v4->n, vlr->v4->n, vlr->n);
					}
					for(a=startvert; a<obr->totvert; a++) {
						ver= render_object_vert_get(obr, a);
						len= normalize_v3(ver->n);
						if(len==0.0) ver->flag= 1;	/* flag abuse, its only used in zbuf now  */
						else ver->flag= 0;
					}
					for(a= startvlak; a<obr->totvlak; a++) {
						vlr= render_object_vlak_get(obr, a);
						if(vlr->v1->flag) copy_v3_v3(vlr->v1->n, vlr->n);
						if(vlr->v2->flag) copy_v3_v3(vlr->v2->n, vlr->n);
						if(vlr->v3->flag) copy_v3_v3(vlr->v3->n, vlr->n);
						if(vlr->v4->flag) copy_v3_v3(vlr->v4->n, vlr->n);
					}
				}
			}
		}

		dl= dl->next;
	}
	
	/* not very elegant... but we want original displist in UI */
	if(cu->resolu_ren) {
		freedisplist(&cu->disp);
		SWAP(ListBase, olddl, cu->disp);
	}

	MEM_freeN(matar);
}

/* ------------------------------------------------------------------------- */
/* Mesh     																 */
/* ------------------------------------------------------------------------- */

struct edgesort {
	int v1, v2;
	int f;
	int i1, i2;
};

/* edges have to be added with lowest index first for sorting */
static void to_edgesort(struct edgesort *ed, int i1, int i2, int v1, int v2, int f)
{
	if(v1>v2) {
		SWAP(int, v1, v2);
		SWAP(int, i1, i2);
	}

	ed->v1= v1;
	ed->v2= v2;
	ed->i1= i1;
	ed->i2= i2;
	ed->f = f;
}

static int vergedgesort(const void *v1, const void *v2)
{
	const struct edgesort *x1=v1, *x2=v2;
	
	if( x1->v1 > x2->v1) return 1;
	else if( x1->v1 < x2->v1) return -1;
	else if( x1->v2 > x2->v2) return 1;
	else if( x1->v2 < x2->v2) return -1;
	
	return 0;
}

static struct edgesort *make_mesh_edge_lookup(DerivedMesh *dm, int *totedgesort)
{
	MFace *mf, *mface;
	MTFace *tface=NULL;
	struct edgesort *edsort, *ed;
	unsigned int *mcol=NULL;
	int a, totedge=0, totface;
	
	mface= dm->getFaceArray(dm);
	totface= dm->getNumFaces(dm);
	tface= dm->getFaceDataArray(dm, CD_MTFACE);
	mcol= dm->getFaceDataArray(dm, CD_MCOL);
	
	if(mcol==NULL && tface==NULL) return NULL;
	
	/* make sorted table with edges and face indices in it */
	for(a= totface, mf= mface; a>0; a--, mf++) {
		if(mf->v4) totedge+=4;
		else if(mf->v3) totedge+=3;
	}

	if(totedge==0)
		return NULL;
	
	ed= edsort= MEM_callocN(totedge*sizeof(struct edgesort), "edgesort");
	
	for(a=0, mf=mface; a<totface; a++, mf++) {
		to_edgesort(ed++, 0, 1, mf->v1, mf->v2, a);
		to_edgesort(ed++, 1, 2, mf->v2, mf->v3, a);
		if(mf->v4) {
			to_edgesort(ed++, 2, 3, mf->v3, mf->v4, a);
			to_edgesort(ed++, 3, 0, mf->v4, mf->v1, a);
		}
		else if(mf->v3)
			to_edgesort(ed++, 2, 3, mf->v3, mf->v1, a);
	}
	
	qsort(edsort, totedge, sizeof(struct edgesort), vergedgesort);
	
	*totedgesort= totedge;

	return edsort;
}

static void use_mesh_edge_lookup(ObjectRen *obr, DerivedMesh *dm, MEdge *medge, VlakRen *vlr, struct edgesort *edgetable, int totedge)
{
	struct edgesort ed, *edp;
	CustomDataLayer *layer;
	MTFace *mtface, *mtf;
	MCol *mcol, *mc;
	int index, mtfn, mcn;
	char *name;
	
	if(medge->v1 < medge->v2) {
		ed.v1= medge->v1;
		ed.v2= medge->v2;
	}
	else {
		ed.v1= medge->v2;
		ed.v2= medge->v1;
	}
	
	edp= bsearch(&ed, edgetable, totedge, sizeof(struct edgesort), vergedgesort);

	/* since edges have different index ordering, we have to duplicate mcol and tface */
	if(edp) {
		mtfn= mcn= 0;

		for(index=0; index<dm->faceData.totlayer; index++) {
			layer= &dm->faceData.layers[index];
			name= layer->name;

			if(layer->type == CD_MTFACE && mtfn < MAX_MTFACE) {
				mtface= &((MTFace*)layer->data)[edp->f];
				mtf= render_vlak_get_tface(obr, vlr, mtfn++, &name, 1);

				*mtf= *mtface;

				memcpy(mtf->uv[0], mtface->uv[edp->i1], sizeof(float)*2);
				memcpy(mtf->uv[1], mtface->uv[edp->i2], sizeof(float)*2);
				memcpy(mtf->uv[2], mtface->uv[1], sizeof(float)*2);
				memcpy(mtf->uv[3], mtface->uv[1], sizeof(float)*2);
			}
			else if(layer->type == CD_MCOL && mcn < MAX_MCOL) {
				mcol= &((MCol*)layer->data)[edp->f*4];
				mc= render_vlak_get_mcol(obr, vlr, mcn++, &name, 1);

				mc[0]= mcol[edp->i1];
				mc[1]= mc[2]= mc[3]= mcol[edp->i2];
			}
		}
	}
}


static void init_render_mesh(Render *re, ObjectRen *obr, int timeoffset)
{
	Object *ob= obr->ob;
	Mesh *me;
	MVert *mvert = NULL;
	MFace *mface;
	VlakRen *vlr; //, *vlr1;
	VertRen *ver;
	Material *ma;
	MSticky *ms = NULL;
	DerivedMesh *dm;
	CustomDataMask mask;
	float xn, yn, zn,  imat[3][3], mat[4][4];  //nor[3],
	float *orco=0;
	int need_orco=0, need_stress=0, need_nmap_tangent=0, need_tangent=0;
	int a, a1, ok, vertofs;
	int end, do_autosmooth=0, totvert = 0;
	int use_original_normals= 0;

	me= ob->data;

	mul_m4_m4m4(mat, ob->obmat, re->cam.viewmat);
	invert_m4_m4(ob->imat, mat);
	copy_m3_m4(imat, ob->imat);

	if(me->totvert==0)
		return;
	
	need_orco= 0;
	for(a=1; a<=ob->totcol; a++) {
		ma= give_render_material(re, ob, a);
		if(ma) {
			if(ma->texco & (TEXCO_ORCO|TEXCO_STRESS))
				need_orco= 1;
			if(ma->texco & TEXCO_STRESS)
				need_stress= 1;
			/* normalmaps, test if tangents needed, separated from shading */
			if(ma->mode_l & MA_TANGENT_V) {
				need_tangent= 1;
				if(me->mtface==NULL)
					need_orco= 1;
			}
			if(ma->mode_l & MA_NORMAP_TANG) {
				if(me->mtface==NULL) {
					need_orco= 1;
					need_tangent= 1;
				}
				need_nmap_tangent= 1;
			}
		}
	}

	if(re->params.flag & R_NEED_TANGENT) {
		/* exception for tangent space baking */
		if(me->mtface==NULL) {
			need_orco= 1;
			need_tangent= 1;
		}
		need_nmap_tangent= 1;
	}
	
	/* check autosmooth and displacement, we then have to skip only-verts optimize */
	do_autosmooth |= (me->flag & ME_AUTOSMOOTH);
	if(do_autosmooth)
		timeoffset= 0;
	if(test_for_displace(re, ob ) )
		timeoffset= 0;
	
	mask= CD_MASK_BAREMESH|CD_MASK_MTFACE|CD_MASK_MCOL;
	if(!timeoffset)
		if(need_orco)
			mask |= CD_MASK_ORCO;

	if(obr->flag & R_LOWRES)
		dm= mesh_get_derived_final(re->db.scene, ob, mask);
	else
		dm= mesh_create_derived_render(re->db.scene, ob, mask);

	if(dm==NULL) return;	/* in case duplicated object fails? */

	if(mask & CD_MASK_ORCO) {
		orco= dm->getVertDataArray(dm, CD_ORCO);
		if(orco) {
			orco= MEM_dupallocN(orco);
			set_object_orco(re, ob, orco);
		}
	}

	mvert= dm->getVertArray(dm);
	totvert= dm->getNumVerts(dm);

	/* attempt to autsmooth on original mesh, only without subsurf */
	if(do_autosmooth && me->totvert==totvert && me->totface==dm->getNumFaces(dm))
		use_original_normals= 1;
	
	ms = (totvert==me->totvert)?me->msticky:NULL;
	
	ma= give_render_material(re, ob, 1);

	if(ma->material_type == MA_TYPE_HALO) {
		make_render_halos(re, obr, me, totvert, mvert, ma, orco);
	}
	else {

		for(a=0; a<totvert; a++, mvert++) {
			ver= render_object_vert_get(obr, obr->totvert++);
			copy_v3_v3(ver->co, mvert->co);
			if(do_autosmooth==0)	/* autosmooth on original unrotated data to prevent differences between frames */
				mul_m4_v3(mat, ver->co);
  
			if(orco) {
				ver->orco= orco;
				orco+=3;
			}
			if(ms) {
				float *sticky= render_vert_get_sticky(obr, ver, 1);
				sticky[0]= ms->co[0];
				sticky[1]= ms->co[1];
				ms++;
			}
		}
		
		if(!timeoffset) {
			/* store customdata names, because DerivedMesh is freed */
			render_object_customdata_set(obr, &dm->faceData);
			
			/* still to do for keys: the correct local texture coordinate */

			/* faces in order of color blocks */
			vertofs= obr->totvert - totvert;
			for(a1=0; (a1<ob->totcol || (a1==0 && ob->totcol==0)); a1++) {

				ma= give_render_material(re, ob, a1+1);
				
				/* test for 100% transparant */
				ok= 1;
				if(ma->alpha==0.0 && ma->spectra==0.0) {
					ok= 0;
					/* texture on transparency? */
					for(a=0; a<MAX_MTEX; a++) {
						if(ma->mtex[a] && ma->mtex[a]->tex) {
							if(ma->mtex[a]->mapto & MAP_ALPHA) ok= 1;
						}
					}
				}
				
				/* if wire material, and we got edges, don't do the faces */
				if(ma->material_type == MA_TYPE_WIRE) {
					end= dm->getNumEdges(dm);
					if(end) ok= 0;
				}

				if(ok) {
					end= dm->getNumFaces(dm);
					mface= dm->getFaceArray(dm);

					for(a=0; a<end; a++, mface++) {
						int v1, v2, v3, v4, flag;
						
						if( mface->mat_nr==a1 ) {
							float len;
								
							v1= mface->v1;
							v2= mface->v2;
							v3= mface->v3;
							v4= mface->v4;
							flag= mface->flag & ME_SMOOTH;

							vlr= render_object_vlak_get(obr, obr->totvlak++);
							vlr->v1= render_object_vert_get(obr, vertofs+v1);
							vlr->v2= render_object_vert_get(obr, vertofs+v2);
							vlr->v3= render_object_vert_get(obr, vertofs+v3);
							if(v4) vlr->v4= render_object_vert_get(obr, vertofs+v4);
							else vlr->v4= 0;

							/* render normals are inverted in render */
							if(use_original_normals) {
								MFace *mf= me->mface+a;
								MVert *mv= me->mvert;
								
								if(vlr->v4) 
									len= normal_quad_v3( vlr->n, mv[mf->v4].co, mv[mf->v3].co, mv[mf->v2].co, mv[mf->v1].co);
								else 
									len= normal_tri_v3( vlr->n,mv[mf->v3].co, mv[mf->v2].co, mv[mf->v1].co);
							}
							else {
								if(vlr->v4) 
									len= normal_quad_v3( vlr->n,vlr->v4->co, vlr->v3->co, vlr->v2->co, vlr->v1->co);
								else 
									len= normal_tri_v3( vlr->n,vlr->v3->co, vlr->v2->co, vlr->v1->co);
							}

							vlr->mat= ma;
							vlr->flag= flag;
							if((me->flag & ME_NOPUNOFLIP) ) {
								vlr->flag |= R_NOPUNOFLIP;
							}
							vlr->ec= 0; /* mesh edges rendered separately */

							if(len==0) obr->totvlak--;
							else {
								CustomDataLayer *layer;
								MTFace *mtface, *mtf;
								MCol *mcol, *mc;
								int index, mtfn= 0, mcn= 0;
								char *name;

								for(index=0; index<dm->faceData.totlayer; index++) {
									layer= &dm->faceData.layers[index];
									name= layer->name;
									
									if(layer->type == CD_MTFACE && mtfn < MAX_MTFACE) {
										mtf= render_vlak_get_tface(obr, vlr, mtfn++, &name, 1);
										mtface= (MTFace*)layer->data;
										*mtf= mtface[a];
									}
									else if(layer->type == CD_MCOL && mcn < MAX_MCOL) {
										mc= render_vlak_get_mcol(obr, vlr, mcn++, &name, 1);
										mcol= (MCol*)layer->data;
										memcpy(mc, &mcol[a*4], sizeof(MCol)*4);
									}
								}
							}
						}
					}
				}
			}
			
			/* exception... we do edges for wire mode. potential conflict when faces exist... */
			end= dm->getNumEdges(dm);
			mvert= dm->getVertArray(dm);
			ma= give_render_material(re, ob, 1);
			if(end && (ma->material_type == MA_TYPE_WIRE)) {
				MEdge *medge;
				struct edgesort *edgetable;
				int totedge= 0;
				
				medge= dm->getEdgeArray(dm);
				
				/* we want edges to have UV and vcol too... */
				edgetable= make_mesh_edge_lookup(dm, &totedge);
				
				for(a1=0; a1<end; a1++, medge++) {
					if (medge->flag&ME_EDGERENDER) {
						MVert *v0 = &mvert[medge->v1];
						MVert *v1 = &mvert[medge->v2];

						vlr= render_object_vlak_get(obr, obr->totvlak++);
						vlr->v1= render_object_vert_get(obr, vertofs+medge->v1);
						vlr->v2= render_object_vert_get(obr, vertofs+medge->v2);
						vlr->v3= vlr->v2;
						vlr->v4= NULL;
						
						if(edgetable)
							use_mesh_edge_lookup(obr, dm, medge, vlr, edgetable, totedge);
						
						xn= -(v0->no[0]+v1->no[0]);
						yn= -(v0->no[1]+v1->no[1]);
						zn= -(v0->no[2]+v1->no[2]);
						/* transpose ! */
						vlr->n[0]= imat[0][0]*xn+imat[0][1]*yn+imat[0][2]*zn;
						vlr->n[1]= imat[1][0]*xn+imat[1][1]*yn+imat[1][2]*zn;
						vlr->n[2]= imat[2][0]*xn+imat[2][1]*yn+imat[2][2]*zn;
						normalize_v3(vlr->n);
						
						vlr->mat= ma;
						vlr->flag= 0;
						vlr->ec= ME_V1V2;
					}
				}
				if(edgetable)
					MEM_freeN(edgetable);
			}
		}
	}
	
	if(!timeoffset) {
		if (test_for_displace(re, ob ) ) {
			calc_vertexnormals(re, obr, 0, 0);
			if(do_autosmooth)
				do_displacement(re, obr, mat, imat);
			else
				do_displacement(re, obr, NULL, NULL);
		}

		if(do_autosmooth) {
			autosmooth(re, obr, mat, me->smoothresh);
		}

		calc_vertexnormals(re, obr, need_tangent, need_nmap_tangent);

		if(need_stress)
			calc_edge_stress(re, obr, me);
	}

	dm->release(dm);
}

void init_render_object_data(Render *re, ObjectRen *obr, int timeoffset)
{
	Object *ob= obr->ob;
	ParticleSystem *psys;
	int i;

	if(obr->psysindex) {
		if((!obr->prev || obr->prev->ob != ob) && ob->type==OB_MESH) {
			/* the emitter mesh wasn't rendered so the modifier stack wasn't
			 * evaluated with render settings */
			DerivedMesh *dm;
			dm = mesh_create_derived_render(re->db.scene, ob, CD_MASK_BAREMESH|CD_MASK_MTFACE|CD_MASK_MCOL);
			dm->release(dm);
		}

		for(psys=ob->particlesystem.first, i=0; i<obr->psysindex-1; i++)
			psys= psys->next;

		render_new_particle_system(re, obr, psys, timeoffset);
	}
	else {
		if ELEM(ob->type, OB_FONT, OB_CURVE)
			init_render_curve(re, obr, timeoffset);
		else if(ob->type==OB_SURF)
			init_render_surf(re, obr);
		else if(ob->type==OB_MESH)
			init_render_mesh(re, obr, timeoffset);
		else if(ob->type==OB_MBALL)
			init_render_mball(re, obr);
	}

	finalize_render_object(re, obr, timeoffset);
	
	re->db.totvert += obr->totvert;
	re->db.totvlak += obr->totvlak;
	re->db.tothalo += obr->tothalo;
	re->db.totstrand += obr->totstrand;
}

/* ------------------------------------------------------------------------- */
/* Object Finalization														 */
/* ------------------------------------------------------------------------- */

/* prevent phong interpolation for giving ray shadow errors (terminator problem) */
static void set_phong_threshold(ObjectRen *obr)
{
//	VertRen *ver;
	VlakRen *vlr;
	float thresh= 0.0, dot;
	int tot=0, i;
	
	/* Added check for 'pointy' situations, only dotproducts of 0.9 and larger 
	   are taken into account. This threshold is meant to work on smooth geometry, not
	   for extreme cases (ton) */
	
	for(i=0; i<obr->totvlak; i++) {
		vlr= render_object_vlak_get(obr, i);
		if(vlr->flag & R_SMOOTH) {
			dot= dot_v3v3(vlr->n, vlr->v1->n);
			dot= ABS(dot);
			if(dot>0.9) {
				thresh+= dot; tot++;
			}
			dot= dot_v3v3(vlr->n, vlr->v2->n);
			dot= ABS(dot);
			if(dot>0.9) {
				thresh+= dot; tot++;
			}

			dot= dot_v3v3(vlr->n, vlr->v3->n);
			dot= ABS(dot);
			if(dot>0.9) {
				thresh+= dot; tot++;
			}

			if(vlr->v4) {
				dot= dot_v3v3(vlr->n, vlr->v4->n);
				dot= ABS(dot);
				if(dot>0.9) {
					thresh+= dot; tot++;
				}
			}
		}
	}
	
	if(tot) {
		thresh/= (float)tot;
		obr->ob->smoothresh= cos(0.5*M_PI-saacos(thresh));
	}
}

/* per face check if all samples should be taken.
   if raytrace or multisample, do always for raytraced material, or when material full_osa set */
static void set_fullsample_flag(Render *re, ObjectRen *obr)
{
	VlakRen *vlr;
	int a, trace, mode;

	if(re->params.osa==0)
		return;
	
	trace= re->params.r.mode & R_RAYTRACE;
	
	for(a=obr->totvlak-1; a>=0; a--) {
		vlr= render_object_vlak_get(obr, a);
		mode= vlr->mat->mode;
		
		if(mode & MA_FULL_OSA) 
			vlr->flag |= R_FULL_OSA;
		else if(trace) {
			if(mode & MA_SHLESS);
			else if(vlr->mat->material_type == MA_TYPE_VOLUME);
			else if((mode & MA_RAYMIRROR) || ((mode & MA_TRANSP) && (mode & MA_RAYTRANSP)))
				/* for blurry reflect/refract, better to take more samples 
				 * inside the raytrace than as OSA samples */
				if ((vlr->mat->gloss_mir == 1.0) && (vlr->mat->gloss_tra == 1.0)) 
					vlr->flag |= R_FULL_OSA;
		}
	}
}

/* split quads for pradictable baking
 * dir 1 == (0,1,2) (0,2,3),  2 == (1,3,0) (1,2,3) 
 */
static void split_quads(ObjectRen *obr, int dir) 
{
	VlakRen *vlr, *vlr1;
	int a;

	for(a=obr->totvlak-1; a>=0; a--) {
		vlr= render_object_vlak_get(obr, a);
		
		/* test if rendering as a quad or triangle, skip wire */
		if(vlr->v4 && (vlr->flag & R_STRAND)==0 && (vlr->mat->material_type != MA_TYPE_WIRE)) {
			
			if(vlr->v4) {

				vlr1= render_object_vlak_copy(obr, vlr);
				vlr1->flag |= R_FACE_SPLIT;
				
				if( dir==2 ) vlr->flag |= R_DIVIDE_24;
				else vlr->flag &= ~R_DIVIDE_24;

				/* new vertex pointers */
				if (vlr->flag & R_DIVIDE_24) {
					vlr1->v1= vlr->v2;
					vlr1->v2= vlr->v3;
					vlr1->v3= vlr->v4;

					vlr->v3 = vlr->v4;
					
					vlr1->flag |= R_DIVIDE_24;
				}
				else {
					vlr1->v1= vlr->v1;
					vlr1->v2= vlr->v3;
					vlr1->v3= vlr->v4;
					
					vlr1->flag &= ~R_DIVIDE_24;
				}
				vlr->v4 = vlr1->v4 = NULL;
				
				/* new normals */
				normal_tri_v3( vlr->n,vlr->v3->co, vlr->v2->co, vlr->v1->co);
				normal_tri_v3( vlr1->n,vlr1->v3->co, vlr1->v2->co, vlr1->v1->co);
			}
			/* clear the flag when not divided */
			else vlr->flag &= ~R_DIVIDE_24;
		}
	}
}

static void check_non_flat_quads(ObjectRen *obr)
{
	VlakRen *vlr, *vlr1;
	VertRen *v1, *v2, *v3, *v4;
	float nor[3], xn, flen;
	int a;

	for(a=obr->totvlak-1; a>=0; a--) {
		vlr= render_object_vlak_get(obr, a);
		
		/* test if rendering as a quad or triangle, skip wire */
		if(vlr->v4 && (vlr->flag & R_STRAND)==0 && (vlr->mat->material_type != MA_TYPE_WIRE)) {
			
			/* check if quad is actually triangle */
			v1= vlr->v1;
			v2= vlr->v2;
			v3= vlr->v3;
			v4= vlr->v4;
			sub_v3_v3v3(nor, v1->co, v2->co);
			if( ABS(nor[0])<FLT_EPSILON10 &&  ABS(nor[1])<FLT_EPSILON10 && ABS(nor[2])<FLT_EPSILON10 ) {
				vlr->v1= v2;
				vlr->v2= v3;
				vlr->v3= v4;
				vlr->v4= NULL;
			}
			else {
				sub_v3_v3v3(nor, v2->co, v3->co);
				if( ABS(nor[0])<FLT_EPSILON10 &&  ABS(nor[1])<FLT_EPSILON10 && ABS(nor[2])<FLT_EPSILON10 ) {
					vlr->v2= v3;
					vlr->v3= v4;
					vlr->v4= NULL;
				}
				else {
					sub_v3_v3v3(nor, v3->co, v4->co);
					if( ABS(nor[0])<FLT_EPSILON10 &&  ABS(nor[1])<FLT_EPSILON10 && ABS(nor[2])<FLT_EPSILON10 ) {
						vlr->v4= NULL;
					}
					else {
						sub_v3_v3v3(nor, v4->co, v1->co);
						if( ABS(nor[0])<FLT_EPSILON10 &&  ABS(nor[1])<FLT_EPSILON10 && ABS(nor[2])<FLT_EPSILON10 ) {
							vlr->v4= NULL;
						}
					}
				}
			}
			
			if(vlr->v4) {
				
				/* Face is divided along edge with the least gradient 		*/
				/* Flagged with R_DIVIDE_24 if divide is from vert 2 to 4 	*/
				/* 		4---3		4---3 */
				/*		|\ 1|	or  |1 /| */
				/*		|0\ |		|/ 0| */
				/*		1---2		1---2 	0 = orig face, 1 = new face */
				
				/* render normals are inverted in render! we calculate normal of single tria here */
				flen= normal_tri_v3( nor,vlr->v4->co, vlr->v3->co, vlr->v1->co);
				if(flen==0.0) normal_tri_v3( nor,vlr->v4->co, vlr->v2->co, vlr->v1->co);
				
				xn= nor[0]*vlr->n[0] + nor[1]*vlr->n[1] + nor[2]*vlr->n[2];

				if(ABS(xn) < 0.999995 ) {	// checked on noisy fractal grid
					
					float d1, d2;

					vlr1= render_object_vlak_copy(obr, vlr);
					vlr1->flag |= R_FACE_SPLIT;
					
					/* split direction based on vnorms */
					normal_tri_v3( nor,vlr->v1->co, vlr->v2->co, vlr->v3->co);
					d1= nor[0]*vlr->v1->n[0] + nor[1]*vlr->v1->n[1] + nor[2]*vlr->v1->n[2];

					normal_tri_v3( nor,vlr->v2->co, vlr->v3->co, vlr->v4->co);
					d2= nor[0]*vlr->v2->n[0] + nor[1]*vlr->v2->n[1] + nor[2]*vlr->v2->n[2];
				
					if( fabs(d1) < fabs(d2) ) vlr->flag |= R_DIVIDE_24;
					else vlr->flag &= ~R_DIVIDE_24;

					/* new vertex pointers */
					if (vlr->flag & R_DIVIDE_24) {
						vlr1->v1= vlr->v2;
						vlr1->v2= vlr->v3;
						vlr1->v3= vlr->v4;

						vlr->v3 = vlr->v4;
						
						vlr1->flag |= R_DIVIDE_24;
					}
					else {
						vlr1->v1= vlr->v1;
						vlr1->v2= vlr->v3;
						vlr1->v3= vlr->v4;
						
						vlr1->flag &= ~R_DIVIDE_24;
					}
					vlr->v4 = vlr1->v4 = NULL;
					
					/* new normals */
					normal_tri_v3( vlr->n,vlr->v3->co, vlr->v2->co, vlr->v1->co);
					normal_tri_v3( vlr1->n,vlr1->v3->co, vlr1->v2->co, vlr1->v1->co);
				}
				/* clear the flag when not divided */
				else vlr->flag &= ~R_DIVIDE_24;
			}
		}
	}
}

void finalize_render_object(Render *re, ObjectRen *obr, int timeoffset)
{
	Object *ob= obr->ob;
	VertRen *ver= NULL;
	StrandRen *strand= NULL;
	StrandBound *sbound= NULL;
	float min[3], max[3], smin[3], smax[3];
	int a, b;

	if(obr->totvert || obr->totvlak || obr->tothalo || obr->totstrand) {
		/* the exception below is because displace code now is in init_render_mesh call, 
		I will look at means to have autosmooth enabled for all object types 
		and have it as general postprocess, like displace */
		if(ob->type!=OB_MESH && test_for_displace(re, ob)) 
			do_displacement(re, obr, NULL, NULL);
	
		if(!timeoffset) {
			/* phong normal interpolation can cause error in tracing
			 * (terminator problem) */
			ob->smoothresh= 0.0;
			if((re->params.r.mode & R_RAYTRACE) && (re->params.r.mode & R_SHADOW)) 
				set_phong_threshold(obr);
			
			if (re->params.flag & R_BAKING && re->params.r.bake_quad_split != 0) {
				/* Baking lets us define a quad split order */
				split_quads(obr, re->params.r.bake_quad_split);
			} else {
				check_non_flat_quads(obr);
			}
			
			set_fullsample_flag(re, obr);

			/* compute bounding boxes for clipping */
			INIT_MINMAX(min, max);
			for(a=0; a<obr->totvert; a++) {
				if((a & 255)==0) ver= obr->vertnodes[a>>8].vert;
				else ver++;

				DO_MINMAX(ver->co, min, max);
			}

			if(obr->strandbuf) {
				sbound= obr->strandbuf->bound;
				for(b=0; b<obr->strandbuf->totbound; b++, sbound++) {
					INIT_MINMAX(smin, smax);

					for(a=sbound->start; a<sbound->end; a++) {
						strand= render_object_strand_get(obr, a);
						strand_minmax(strand, smin, smax);
					}

					copy_v3_v3(sbound->boundbox[0], smin);
					copy_v3_v3(sbound->boundbox[1], smax);

					DO_MINMAX(smin, min, max);
					DO_MINMAX(smax, min, max);
				}
			}

			copy_v3_v3(obr->boundbox[0], min);
			copy_v3_v3(obr->boundbox[1], max);
		}
	}
}



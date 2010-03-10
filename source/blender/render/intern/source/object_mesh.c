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

#include "cache.h"
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

float *render_vert_get_orco(ObjectRen *obr, VertRen *ver, int verify)
{
	float *orco;
	int nr= ver->index>>8;
	
	orco= obr->vertnodes[nr].orco;
	if(orco==NULL) {
		if(verify) 
			orco= obr->vertnodes[nr].orco= MEM_mallocN(256*RE_ORCO_ELEMS*sizeof(float), "orco table");
		else
			return NULL;
	}
	return orco + (ver->index & 255)*RE_ORCO_ELEMS;
}

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

float *render_vert_get_strandco(ObjectRen *obr, VertRen *ver, int verify)
{
	float *strandco;
	int nr= ver->index>>8;
	
	strandco= obr->vertnodes[nr].strandco;
	if(strandco==NULL) {
		if(verify) 
			strandco= obr->vertnodes[nr].strandco= MEM_callocN(256*RE_STRANDCO_ELEMS*sizeof(float), "strandco table");
		else
			return NULL;
	}
	return strandco + (ver->index & 255)*RE_STRANDCO_ELEMS;
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
	
	fp1= render_vert_get_orco(obr, ver, 0);
	if(fp1) {
		fp2= render_vert_get_orco(obr, v1, 1);
		memcpy(fp2, fp1, RE_ORCO_ELEMS*sizeof(float));
	}
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
	fp1= render_vert_get_strandco(obr, ver, 0);
	if(fp1) {
		fp2= render_vert_get_strandco(obr, v1, 1);
		memcpy(fp2, fp1, RE_STRANDCO_ELEMS*sizeof(float));
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

int render_vlak_get_normal(ObjectInstanceRen *obi, VlakRen *vlr, float *nor, int quad)
{
	float v1[3], n[3], (*nmat)[3]= obi->nmat;
	int flipped= 0;

	if(vlr->v4) {
		/* in case of non-planar quad the normal is not accurate */
		if(quad)
			normal_tri_v3(n, vlr->v4->co, vlr->v3->co, vlr->v1->co);
		else
			normal_tri_v3(n, vlr->v3->co, vlr->v2->co, vlr->v1->co);
	}
	else
		copy_v3_v3(n, vlr->n);

	if(obi->flag & R_TRANSFORMED) {
		copy_v3_v3(nor, n);
		
		mul_m3_v3(nmat, nor);
		normalize_v3(nor);
	}
	else
		copy_v3_v3(nor, n);

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

static void calc_edge_stress_add(float *accum, ObjectRen *obr, VertRen *v1, VertRen *v2)
{
	float *orco1= render_vert_get_orco(obr, v1, 0);
	float *orco2= render_vert_get_orco(obr, v2, 0);
	float len= len_v3v3(v1->co, v2->co)/len_v3v3(orco1, orco2);
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
		float *vorco= render_vert_get_orco(obr, ver, 0);

		if(vorco) {
			vorco[0]= vorco[0]*size[0] +loc[0];
			vorco[1]= vorco[1]*size[1] +loc[1];
			vorco[2]= vorco[2]*size[2] +loc[2];
		}
	}
	
	/* add stress values */
	accumoffs= accum;	/* so we can use vertex index */
	for(a=0; a<obr->totvlak; a++) {
		VlakRen *vlr= render_object_vlak_get(obr, a);

		if(render_vert_get_orco(obr, vlr->v1, 0) && vlr->v4) {
			calc_edge_stress_add(accumoffs, obr, vlr->v1, vlr->v2);
			calc_edge_stress_add(accumoffs, obr, vlr->v2, vlr->v3);
			calc_edge_stress_add(accumoffs, obr, vlr->v3, vlr->v1);
			if(vlr->v4) {
				calc_edge_stress_add(accumoffs, obr, vlr->v3, vlr->v4);
				calc_edge_stress_add(accumoffs, obr, vlr->v4, vlr->v1);
				calc_edge_stress_add(accumoffs, obr, vlr->v2, vlr->v4);
			}
		}
	}
	
	for(a=0; a<obr->totvert; a++) {
		VertRen *ver= render_object_vert_get(obr, a);
		float *vorco= render_vert_get_orco(obr, ver, 0);

		if(vorco) {
			/* find stress value */
			acc= accumoffs + 2*ver->index;
			if(acc[1]!=0.0f)
				acc[0]/= acc[1];
			stress= render_vert_get_stress(obr, ver, 1);
			*stress= *acc;
			
			/* restore orcos */
			vorco[0] = (vorco[0]-loc[0])/size[0];
			vorco[1] = (vorco[1]-loc[1])/size[1];
			vorco[2] = (vorco[2]-loc[2])/size[2];
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
	else if(render_vert_get_orco(obr, v1, 0)) {
		float *orco1= render_vert_get_orco(obr, v1, 0);
		float *orco2= render_vert_get_orco(obr, v2, 0);
		float *orco3= render_vert_get_orco(obr, v3, 0);

		uv1= uv[0]; uv2= uv[1]; uv3= uv[2]; uv4= uv[3];
		map_to_sphere( &uv[0][0], &uv[0][1],orco1[0], orco1[1], orco1[2]);
		map_to_sphere( &uv[1][0], &uv[1][1],orco2[0], orco2[1], orco2[2]);
		map_to_sphere( &uv[2][0], &uv[2][1],orco3[0], orco3[1], orco3[2]);
		if(v4) {
			float *orco4= render_vert_get_orco(obr, v4, 0);
			map_to_sphere( &uv[3][0], &uv[3][1],orco4[0], orco4[1], orco4[2]);
		}
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


void render_object_calc_vnormals(Render *re, ObjectRen *obr, int do_tangent, int do_nmap_tangent)
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

static void displace_render_vert(Render *re, ObjectRen *obr, ShadeInput *shi, VertRen *vr, int vindex, float *scale, float mat[][4], float imat[][3], float *sample)
{
	MTFace *tface;
	short texco= shi->material.mat->texco;
	float displace[3], *orco;
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
	if (texco & TEXCO_ORCO) {
		orco= render_vert_get_orco(obr, vr, 0);
		if(orco)
			copy_v3_v3(shi->texture.lo, orco);
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
	/* Should be sqrt(sample), but I'm only looking for "bigger".  Save the cycles. */
	sample[vr->index]= dot_v3v3(shi->texture.displace, shi->texture.displace);
}

static void displace_render_face(Render *re, ObjectRen *obr, VlakRen *vlr, float *scale, float mat[][4], float imat[][3], float *sample)
{
	ShadeInput shi;
	VertRen *v1= vlr->v1;
	VertRen *v2= vlr->v2;
	VertRen *v3= vlr->v3;
	VertRen *v4= vlr->v4;

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
	shi.primitive.v1= v1;
	shi.primitive.v2= v2;
	shi.primitive.v3= v3;
#endif
	
	/* Displace the verts, flag is set when done */
	if(!v1->flag)
		displace_render_vert(re, obr, &shi, v1,0,  scale, mat, imat, sample);
	
	if(!v2->flag)
		displace_render_vert(re, obr, &shi, v2, 1, scale, mat, imat, sample);

	if(!v3->flag)
		displace_render_vert(re, obr, &shi, v3, 2, scale, mat, imat, sample);

	if(v4) {
		if(!v4->flag)
			displace_render_vert(re, obr, &shi, v4, 3, scale, mat, imat, sample);

		/*	closest in displace value.  This will help smooth edges.   */ 
		if(fabs(sample[v1->index] - sample[v3->index]) > fabs(sample[v2->index] - sample[v4->index]))
			vlr->flag |= R_DIVIDE_24;
		else vlr->flag &= ~R_DIVIDE_24;
	}
	
	/* Recalculate the face normal  - if flipped before, flip now */
	if(v4)
		normal_quad_v3(vlr->n, v4->co, v3->co, v2->co, v1->co);
	else
		normal_tri_v3(vlr->n, v3->co, v2->co, v1->co);
}

static void do_displacement(Render *re, ObjectRen *obr, float mat[][4], float imat[][3])
{
	Object *obt;
	VlakRen *vlr;
	VertRen *vr;
	float scale[3]={1.0f, 1.0f, 1.0f}, temp[3], *sample;
	int i;

	sample= MEM_callocN(sizeof(float)*obr->totvert, "do_displacement sample");
		
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
		displace_render_face(re, obr, vlr, scale, mat, imat, sample);
	}

	MEM_freeN(sample);
	
	/* Recalc vertex normals */
	render_object_calc_vnormals(re, obr, 0, 0);
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
	orco= make_orco_mball(ob);

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
		//if(ob->transflag & OB_NEG_SCALE) negate_v3(ver->n);
		
		if(need_orco)
			copy_v3_v3(render_vert_get_orco(obr, ver, 1), orco);
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
			copy_v3_v3(render_vert_get_orco(obr, v1, 1), orco);
			orco+= 3; orcoret++;
		}	
		mul_m4_v3(mat, v1->co);
		
		for (v = 1; v < sizev; v++) {
			ver= render_object_vert_get(obr, obr->totvert++);
			copy_v3_v3(ver->co, data); data += 3;
			if(orco) {
				copy_v3_v3(render_vert_get_orco(obr, ver, 1), orco);
				orco+= 3; orcoret++;
			}	
			mul_m4_v3(mat, ver->co);
		}
		/* if V-cyclic, add extra vertices at end of the row */
		if (dl->flag & DL_CYCL_U) {
			ver= render_object_vert_get(obr, obr->totvert++);
			copy_v3_v3(ver->co, v1->co);
			if(orco) {
				copy_v3_v3(render_vert_get_orco(obr, ver, 1), orco);
				orco+=3; orcoret++; //orcobase + 3*(u*sizev + 0);
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
				copy_v3_v3(render_vert_get_orco(obr, ver, 1), orco);
				orco+=3; orcoret++; //orcobase + 3*(0*sizev + v);
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

static void init_render_dm(DerivedMesh *dm, Render *re, ObjectRen *obr,
	int timeoffset, float *orco, float mat[4][4])
{
	Object *ob= obr->ob;
	int a, a1, end, totvert, vertofs;
	VertRen *ver;
	VlakRen *vlr;
	Curve *cu= NULL;
	MVert *mvert = NULL;
	MFace *mface;
	Material *ma;

	mvert= dm->getVertArray(dm);
	totvert= dm->getNumVerts(dm);

	if ELEM(ob->type, OB_FONT, OB_CURVE) {
		cu= ob->data;
	}

	for(a=0; a<totvert; a++, mvert++) {
		ver= render_object_vert_get(obr, obr->totvert++);
		VECCOPY(ver->co, mvert->co);
		mul_m4_v3(mat, ver->co);

		if(orco) {
			copy_v3_v3(render_vert_get_orco(obr, ver, 1), orco);
			orco+=3;
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
					if(vlr->v4)
						len= normal_quad_v3( vlr->n,vlr->v4->co, vlr->v3->co, vlr->v2->co, vlr->v1->co);
					else
						len= normal_tri_v3( vlr->n,vlr->v3->co, vlr->v2->co, vlr->v1->co);

					vlr->mat= ma;
					vlr->flag= flag;
					if(cu &&(cu->flag & ME_NOPUNOFLIP)) {
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

		/* Normals */
		render_object_calc_vnormals(re, obr, 0, 0);
	}

}

static void init_render_surf(Render *re, ObjectRen *obr, int timeoffset)
{
	Object *ob= obr->ob;
	Nurb *nu=0;
	Curve *cu;
	ListBase displist= {NULL, NULL};
	DispList *dl;
	Material **matar;
	float *orco=NULL, *orcobase=NULL, mat[4][4];
	int a, totmat, need_orco=0;
	DerivedMesh *dm= NULL;

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

	makeDispListSurf(re->db.scene, ob, &displist, &dm, 1, 0);

	if (dm) {
		if(need_orco)
			orco= makeOrcoDispList(re->db.scene, ob, dm, 1);

		init_render_dm(dm, re, obr, timeoffset, orco, mat);
		dm->release(dm);

		if(orco)
			MEM_freeN(orco);
	} else {
		if(need_orco)
			orcobase= orco= make_orco_surf(ob);

		/* walk along displaylist and create rendervertices/-faces */
		for(dl=displist.first; dl; dl=dl->next) {
			/* watch out: u ^= y, v ^= x !! */
			if(dl->type==DL_SURF)
				orco+= 3*dl_surf_to_renderdata(obr, dl, matar, orco, mat);
		}

		if(orcobase)
			MEM_freeN(orcobase);
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
	DerivedMesh *dm = NULL;
	ListBase disp={NULL, NULL};
	Material **matar;
	float len, *data, *fp, *orco=NULL, *orcobase= NULL;
	float n[3], mat[4][4];
	int nr, startvert, startvlak, a, b;
	int frontside, need_orco=0, totmat;

	cu= ob->data;
	if(ob->type==OB_FONT && cu->str==NULL) return;
	else if(ob->type==OB_CURVE && cu->nurb.first==NULL) return;

	makeDispListCurveTypes_forRender(re->db.scene, ob, &disp, &dm, 0);
	dl= disp.first;
	if(dl==NULL) return;
	
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

	if (dm) {
		if(need_orco)
			orco= makeOrcoDispList(re->db.scene, ob, dm, 1);

		init_render_dm(dm, re, obr, timeoffset, orco, mat);
		dm->release(dm);

		if(orco)
			MEM_freeN(orco);
	} else {
		if(need_orco)
			orcobase=orco= make_orco_curve(re->db.scene, ob);

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
						copy_v3_v3(render_vert_get_orco(obr, ver, 1), orco);
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
							copy_v3_v3(render_vert_get_orco(obr, ver, 1), orco);
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

		if(orcobase)
			MEM_freeN(orcobase);
	}

	freedisplist(&disp);

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

	if(mask & CD_MASK_ORCO)
		orco= dm->getVertDataArray(dm, CD_ORCO);

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
				copy_v3_v3(render_vert_get_orco(obr, ver, 1), orco);
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
		if (!(ob->flag & OB_RENDER_SUBDIVIDE) && test_for_displace(re, ob ) ) {
			render_object_calc_vnormals(re, obr, 0, 0);
			if(do_autosmooth)
				do_displacement(re, obr, mat, imat);
			else
				do_displacement(re, obr, NULL, NULL);
		}

		if(do_autosmooth) {
			autosmooth(re, obr, mat, me->smoothresh);
		}

		render_object_calc_vnormals(re, obr, need_tangent, need_nmap_tangent);

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

		init_render_particle_system(re, obr, psys, timeoffset);
	}
	else {
		if ELEM(ob->type, OB_FONT, OB_CURVE)
			init_render_curve(re, obr, timeoffset);
		else if(ob->type==OB_SURF)
			init_render_surf(re, obr, timeoffset);
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
static void set_fullsample_trace_flag(Render *re, ObjectRen *obr)
{
	VlakRen *vlr;
	int a, trace, mode, osa;

	osa= re->params.osa;
	trace= re->params.r.mode & R_RAYTRACE;
	
	for(a=obr->totvlak-1; a>=0; a--) {
		vlr= render_object_vlak_get(obr, a);
		mode= vlr->mat->mode;

		if(osa && (mode & MA_FULL_OSA)) {
			vlr->flag |= R_FULL_OSA;
		}
		else if(trace) {
			/* TODO: remove temporary raytrace_all hack */
			if(mode & MA_TRACEBLE || re->params.r.raytrace_all) {
				vlr->flag |= R_TRACEBLE;
			}
			else if(osa) {
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
		if((ob->type!=OB_MESH || obr->flag & R_TEMP_COPY) && test_for_displace(re, ob)) 
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
				if((re->params.r.simplify_flag & R_SIMPLE_NO_TRIANGULATE) == 0)
					check_non_flat_quads(obr);
			}
			
			set_fullsample_trace_flag(re, obr);

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


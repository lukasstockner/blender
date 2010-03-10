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

#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "DNA_material_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_particle_types.h"

#include "BLI_math.h"
#include "BLI_rand.h"

#include "BKE_DerivedMesh.h"
#include "BKE_global.h"
#include "BKE_lattice.h"
#include "BKE_object.h"
#include "BKE_particle.h"

#include "cache.h"
#include "object.h"
#include "object_halo.h"
#include "object_mesh.h"
#include "object_strand.h"
#include "render_types.h"

/* ------------------------------------------------------------------------- */
/* Particles                                                                 */
/* ------------------------------------------------------------------------- */
typedef struct ParticleStrandData
{
	struct MCol *mcol;
	float orco[3], *uvco, *surfnor;
	float time, adapt_angle, adapt_pix, size;
	int totuv, totcol;
	int first, line, adapt, override_uv;
} ParticleStrandData;

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
		copy_v3_v3(render_vert_get_orco(obr, vlr->v1, 1), sd->orco);
		*render_vert_get_strandco(obr, vlr->v1, 1)= -1.0f;
		
		copy_v3_v3(vlr->v2->co, vec);
		sub_v3_v3v3(vlr->v2->co, vlr->v2->co, cross);
		copy_v3_v3(vlr->v2->n, nor);
		copy_v3_v3(render_vert_get_orco(obr, vlr->v2, 1), sd->orco);
		*render_vert_get_strandco(obr, vlr->v2, 1)= -1.0f;

		copy_v3_v3(vlr->v4->co, vec1);
		add_v3_v3v3(vlr->v4->co, vlr->v4->co, cross);
		copy_v3_v3(vlr->v4->n, nor);
		copy_v3_v3(render_vert_get_orco(obr, vlr->v4, 1), sd->orco);
		*render_vert_get_strandco(obr, vlr->v4, 1)= 1.0f;
		
		copy_v3_v3(vlr->v3->co, vec1);
		sub_v3_v3v3(vlr->v3->co, vlr->v3->co, cross);
		copy_v3_v3(vlr->v3->n, nor);
		copy_v3_v3(render_vert_get_orco(obr, vlr->v3, 1), sd->orco);
		*render_vert_get_strandco(obr, vlr->v3, 1)= 1.0f;

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
		copy_v3_v3(render_vert_get_orco(obr, v1, 1), sd->orco);
		*render_vert_get_strandco(obr, v1, 1)= -1.0f;
		
		copy_v3_v3(v2->co, vec);
		sub_v3_v3v3(v2->co, v2->co, cross);
		copy_v3_v3(v2->n, nor);
		copy_v3_v3(render_vert_get_orco(obr, v2, 1), sd->orco);
		*render_vert_get_strandco(obr, v2, 1)= -1.0f;
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
		copy_v3_v3(render_vert_get_orco(obr, vlr->v4, 1), sd->orco);
		*render_vert_get_strandco(obr, vlr->v4, 1)= -1.0f + 2.0f*sd->time;
		
		copy_v3_v3(vlr->v3->co, vec);
		sub_v3_v3v3(vlr->v3->co, vlr->v3->co, cross);
		copy_v3_v3(vlr->v3->n, nor);
		copy_v3_v3(render_vert_get_orco(obr, vlr->v3, 1), sd->orco);
		*render_vert_get_strandco(obr, vlr->v3, 1)= -1.0f + 2.0f*sd->time;
		
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
				MTFace *mtf= render_vlak_get_tface(obr,vlr,sd->override_uv,NULL,0);
				float sco1= *render_vert_get_strandco(obr, vlr->v1, 0);
				float sco3= *render_vert_get_strandco(obr, vlr->v3, 0);
				
				mtf->uv[0][0]=mtf->uv[3][0]=0.0f;
				mtf->uv[1][0]=mtf->uv[2][0]=1.0f;

				mtf->uv[0][1]=mtf->uv[1][1]=(sco1+1.0f)/2.0f;
				mtf->uv[2][1]=mtf->uv[3][1]=(sco3+1.0f)/2.0f;
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

void init_render_particle_system(Render *re, ObjectRen *obr, ParticleSystem *psys, int timeoffset)
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
	int seed, path_nbr=0, num;
	int totface, *origindex = 0;
	char **uv_name=0;

/* 1. check that everything is ok & updated */
	if(psys==NULL)
		return;
	
	totchild=psys->totchild;

	part=psys->part;
	pars=psys->particles;

	if(part==NULL || pars==NULL || !psys_check_enabled(ob, psys))
		return;
	
	if(part->ren_as==PART_DRAW_OB || part->ren_as==PART_DRAW_GR || part->ren_as==PART_DRAW_NOT)
		return;

/* 2. start initialising things */

	/* last possibility to bail out! */
	sim.psmd = psmd = psys_get_modifier(ob,psys);
	if(!(psmd->modifier.mode & eModifierMode_Render))
		return;

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
			else if((re->db.wrld.mode & (WO_AMB_OCC|WO_ENV_LIGHT|WO_INDIRECT_LIGHT)) && (re->db.wrld.ao_gather_method == WO_LIGHT_GATHER_APPROX))
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
			copy_v3_v3(render_strand_get_orco(obr, strand, 1), sd.orco);

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

		if(re->cb.test_break(re->cb.tbh))
			break;
	}

	if(dosurfacecache)
		strandbuf->surface= surface_cache_create(re, obr, psmd->dm, mat, timeoffset);

/* 4. clean up */
#if 0 // XXX old animation system
	if(ma) do_mat_ipo(re->db.scene, ma);
#endif // XXX old animation system
	
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
		render_object_calc_vnormals(re, obr, 0, 0);
}


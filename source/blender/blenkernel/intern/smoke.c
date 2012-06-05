/*
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
 * The Original Code is Copyright (C) Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Daniel Genrich
 *                 Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/smoke.c
 *  \ingroup bke
 */


/* Part of the code copied from elbeem fluid library, copyright by Nils Thuerey */

#include <GL/glew.h>

#include "MEM_guardedalloc.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h> /* memset */

#include "BLI_linklist.h"
#include "BLI_rand.h"
#include "BLI_jitter.h"
#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_edgehash.h"
#include "BLI_kdtree.h"
#include "BLI_kdopbvh.h"
#include "BLI_utildefines.h"

#include "DNA_customdata_types.h"
#include "DNA_group_types.h"
#include "DNA_lamp_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_scene_types.h"
#include "DNA_smoke_types.h"

#include "BKE_bvhutils.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_collision.h"
#include "BKE_customdata.h"
#include "BKE_deform.h"
#include "BKE_DerivedMesh.h"
#include "BKE_effect.h"
#include "BKE_modifier.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_smoke.h"

/* UNUSED so far, may be enabled later */
/* #define USE_SMOKE_COLLISION_DM */

#ifdef WITH_SMOKE

#include "smoke_API.h"

#ifdef _WIN32
#include <time.h>
#include <stdio.h>
#include <conio.h>
#include <windows.h>

static LARGE_INTEGER liFrequency;
static LARGE_INTEGER liStartTime;
static LARGE_INTEGER liCurrentTime;

static void tstart ( void )
{
	QueryPerformanceFrequency ( &liFrequency );
	QueryPerformanceCounter ( &liStartTime );
}
static void tend ( void )
{
	QueryPerformanceCounter ( &liCurrentTime );
}
static double UNUSED_FUNCTION(tval)( void )
{
	return ((double)( (liCurrentTime.QuadPart - liStartTime.QuadPart)* (double)1000.0/(double)liFrequency.QuadPart ));
}
#else
#include <sys/time.h>
static struct timeval _tstart, _tend;
static struct timezone tz;
static void tstart ( void )
{
	gettimeofday ( &_tstart, &tz );
}
static void tend ( void )
{
	gettimeofday ( &_tend,&tz );
}

static double UNUSED_FUNCTION(tval)( void )
{
	double t1, t2;
	t1 = ( double ) _tstart.tv_sec*1000 + ( double ) _tstart.tv_usec/ ( 1000 );
	t2 = ( double ) _tend.tv_sec*1000 + ( double ) _tend.tv_usec/ ( 1000 );
	return t2-t1;
}
#endif

struct Object;
struct Scene;
struct DerivedMesh;
struct SmokeModifierData;

#define TRI_UVOFFSET (1./4.)

// timestep default value for nice appearance 0.1f
#define DT_DEFAULT 0.1f

#define ADD_IF_LOWER_POS(a,b) (MIN2((a)+(b), MAX2((a),(b))))
#define ADD_IF_LOWER_NEG(a,b) (MAX2((a)+(b), MIN2((a),(b))))
#define ADD_IF_LOWER(a,b) (((b)>0)?ADD_IF_LOWER_POS((a),(b)):ADD_IF_LOWER_NEG((a),(b)))

/* forward declerations */
static void calcTriangleDivs(Object *ob, MVert *verts, int numverts, MFace *tris, int numfaces, int numtris, int **tridivs, float cell_len);
static void get_cell(float *p0, int res[3], float dx, float *pos, int *cell, int correct);
static void fill_scs_points(Object *ob, DerivedMesh *dm, SmokeCollSettings *scs);

#else /* WITH_SMOKE */

/* Stubs to use when smoke is disabled */
struct WTURBULENCE *smoke_turbulence_init(int *UNUSED(res), int UNUSED(amplify), int UNUSED(noisetype)) { return NULL; }
struct FLUID_3D *smoke_init(int *UNUSED(res), float *UNUSED(p0)) { return NULL; }
void smoke_free(struct FLUID_3D *UNUSED(fluid)) {}
void smoke_turbulence_free(struct WTURBULENCE *UNUSED(wt)) {}
void smoke_initWaveletBlenderRNA(struct WTURBULENCE *UNUSED(wt), float *UNUSED(strength)) {}
void smoke_initBlenderRNA(struct FLUID_3D *UNUSED(fluid), float *UNUSED(alpha), float *UNUSED(beta), float *UNUSED(dt_factor), float *UNUSED(vorticity), int *UNUSED(border_colli),
						  float *UNUSED(burning_rate), float *UNUSED(flame_smoke), float *UNUSED(flame_vorticity), float *UNUSED(flame_ignition_temp), float *UNUSED(flame_max_temp)) {}
long long smoke_get_mem_req(int UNUSED(xres), int UNUSED(yres), int UNUSED(zres), int UNUSED(amplify)) { return 0; }
void smokeModifier_do(SmokeModifierData *UNUSED(smd), Scene *UNUSED(scene), Object *UNUSED(ob), DerivedMesh *UNUSED(dm)) {}

#endif /* WITH_SMOKE */

#ifdef WITH_SMOKE

static int smokeModifier_init(SmokeModifierData *smd, Object *ob, Scene *scene, DerivedMesh *dm)
{
	if((smd->type & MOD_SMOKE_TYPE_DOMAIN) && smd->domain && !smd->domain->fluid)
	{
		size_t i;
		float min[3] = {FLT_MAX, FLT_MAX, FLT_MAX}, max[3] = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
		float size[3];
		MVert *verts = dm->getVertArray(dm);
		float scale = 0.0;
		int res;		

		res = smd->domain->maxres;

		// get BB of domain
		for(i = 0; i < dm->getNumVerts(dm); i++)
		{
			float tmp[3];

			copy_v3_v3(tmp, verts[i].co);
			mul_m4_v3(ob->obmat, tmp);

			// min BB
			min[0] = MIN2(min[0], tmp[0]);
			min[1] = MIN2(min[1], tmp[1]);
			min[2] = MIN2(min[2], tmp[2]);

			// max BB
			max[0] = MAX2(max[0], tmp[0]);
			max[1] = MAX2(max[1], tmp[1]);
			max[2] = MAX2(max[2], tmp[2]);
		}

		copy_v3_v3(smd->domain->p0, min);
		copy_v3_v3(smd->domain->p1, max);

		// calc other res with max_res provided
		sub_v3_v3v3(size, max, min);

		// prevent crash when initializing a plane as domain
		if((size[0] < FLT_EPSILON) || (size[1] < FLT_EPSILON) || (size[2] < FLT_EPSILON))
			return 0;

		if(size[0] > size[1])
		{
			if(size[0] > size[2])
			{
				scale = res / size[0];
				smd->domain->scale = size[0];
				smd->domain->dx = 1.0f / res; 
				smd->domain->res[0] = res;
				smd->domain->res[1] = (int)(size[1] * scale + 0.5);
				smd->domain->res[2] = (int)(size[2] * scale + 0.5);
			}
			else {
				scale = res / size[2];
				smd->domain->scale = size[2];
				smd->domain->dx = 1.0f / res;
				smd->domain->res[2] = res;
				smd->domain->res[0] = (int)(size[0] * scale + 0.5);
				smd->domain->res[1] = (int)(size[1] * scale + 0.5);
			}
		}
		else {
			if(size[1] > size[2])
			{
				scale = res / size[1];
				smd->domain->scale = size[1];
				smd->domain->dx = 1.0f / res; 
				smd->domain->res[1] = res;
				smd->domain->res[0] = (int)(size[0] * scale + 0.5);
				smd->domain->res[2] = (int)(size[2] * scale + 0.5);
			}
			else {
				scale = res / size[2];
				smd->domain->scale = size[2];
				smd->domain->dx = 1.0f / res;
				smd->domain->res[2] = res;
				smd->domain->res[0] = (int)(size[0] * scale + 0.5);
				smd->domain->res[1] = (int)(size[1] * scale + 0.5);
			}
		}

		// TODO: put in failsafe if res<=0 - dg

		// dt max is 0.1
		smd->domain->fluid = smoke_init(smd->domain->res, smd->domain->p0, DT_DEFAULT);
		smd->time = scene->r.cfra;

		if(smd->domain->flags & MOD_SMOKE_HIGHRES)
		{
			smd->domain->wt = smoke_turbulence_init(smd->domain->res, smd->domain->amplify + 1, smd->domain->noise);
			smd->domain->res_wt[0] = smd->domain->res[0] * (smd->domain->amplify + 1);
			smd->domain->res_wt[1] = smd->domain->res[1] * (smd->domain->amplify + 1);			
			smd->domain->res_wt[2] = smd->domain->res[2] * (smd->domain->amplify + 1);			
			smd->domain->dx_wt = smd->domain->dx / (smd->domain->amplify + 1);		
		}

		if(!smd->domain->shadow)
			smd->domain->shadow = MEM_callocN(sizeof(float) * smd->domain->res[0] * smd->domain->res[1] * smd->domain->res[2], "SmokeDomainShadow");

		smoke_initBlenderRNA(smd->domain->fluid, &(smd->domain->alpha), &(smd->domain->beta), &(smd->domain->time_scale), &(smd->domain->vorticity), &(smd->domain->border_collisions),
							 &(smd->domain->burning_rate), &(smd->domain->flame_smoke), &(smd->domain->flame_vorticity), &(smd->domain->flame_ignition), &(smd->domain->flame_max_temp));

		if(smd->domain->wt)	
		{
			smoke_initWaveletBlenderRNA(smd->domain->wt, &(smd->domain->strength));
		}
		return 1;
	}
	else if((smd->type & MOD_SMOKE_TYPE_FLOW) && smd->flow)
	{
		// handle flow object here
		// XXX TODO

		smd->time = scene->r.cfra;

		return 1;
	}
	else if((smd->type & MOD_SMOKE_TYPE_COLL))
	{
		// todo: delete this when loading colls work -dg

		if(!smd->coll)
		{
			smokeModifier_createType(smd);
		}

		if(!smd->coll->points)
		{
			// init collision points
			SmokeCollSettings *scs = smd->coll;

			smd->time = scene->r.cfra;

			// copy obmat
			copy_m4_m4(scs->mat, ob->obmat);
			copy_m4_m4(scs->mat_old, ob->obmat);

			DM_ensure_tessface(dm);
			fill_scs_points(ob, dm, scs);
		}

		if(!smd->coll->bvhtree)
		{
			smd->coll->bvhtree = NULL; // bvhtree_build_from_smoke ( ob->obmat, dm->getTessFaceArray(dm), dm->getNumTessFaces(dm), dm->getVertArray(dm), dm->getNumVerts(dm), 0.0 );
		}
		return 1;
	}

	return 2;
}

static void fill_scs_points(Object *ob, DerivedMesh *dm, SmokeCollSettings *scs)
{
	MVert *mvert = dm->getVertArray(dm);
	MFace *mface = dm->getTessFaceArray(dm);
	int i = 0, divs = 0;

	// DG TODO: need to do this dynamically according to the domain object!
	float cell_len = scs->dx;
	int newdivs = 0;
	int quads = 0, facecounter = 0;

	// count quads
	for(i = 0; i < dm->getNumTessFaces(dm); i++)
	{
		if(mface[i].v4)
			quads++;
	}

	scs->numtris = dm->getNumTessFaces(dm) + quads;
	scs->tridivs = NULL;
	calcTriangleDivs(ob, mvert, dm->getNumVerts(dm), mface,  dm->getNumTessFaces(dm), scs->numtris, &(scs->tridivs), cell_len);

	// count triangle divisions
	for(i = 0; i < dm->getNumTessFaces(dm) + quads; i++)
	{
		divs += (scs->tridivs[3 * i] + 1) * (scs->tridivs[3 * i + 1] + 1) * (scs->tridivs[3 * i + 2] + 1);
	}

	scs->points = MEM_callocN(sizeof(float) * (dm->getNumVerts(dm) + divs) * 3, "SmokeCollPoints");
	scs->points_old = MEM_callocN(sizeof(float) * (dm->getNumVerts(dm) + divs) * 3, "SmokeCollPointsOld");

	for(i = 0; i < dm->getNumVerts(dm); i++)
	{
		float tmpvec[3];
		copy_v3_v3(tmpvec, mvert[i].co);
		// mul_m4_v3(ob->obmat, tmpvec); // DG: use local coordinates, we save MAT anyway
		copy_v3_v3(&scs->points[i * 3], tmpvec);
	}
	
	for(i = 0, facecounter = 0; i < dm->getNumTessFaces(dm); i++)
	{
		int again = 0;
		do
		{
			int j, k;
			int divs1 = scs->tridivs[3 * facecounter + 0];
			int divs2 = scs->tridivs[3 * facecounter + 1];
			//int divs3 = scs->tridivs[3 * facecounter + 2];
			float side1[3], side2[3], trinormorg[3], trinorm[3];
			
			if(again == 1 && mface[i].v4)
			{
				sub_v3_v3v3(side1,  mvert[ mface[i].v3 ].co, mvert[ mface[i].v1 ].co);
				sub_v3_v3v3(side2,  mvert[ mface[i].v4 ].co, mvert[ mface[i].v1 ].co);
			}
			else {
				sub_v3_v3v3(side1,  mvert[ mface[i].v2 ].co, mvert[ mface[i].v1 ].co);
				sub_v3_v3v3(side2,  mvert[ mface[i].v3 ].co, mvert[ mface[i].v1 ].co);
			}

			cross_v3_v3v3(trinormorg, side1, side2);
			normalize_v3(trinormorg);
			copy_v3_v3(trinorm, trinormorg);
			mul_v3_fl(trinorm, 0.25 * cell_len);

			for(j = 0; j <= divs1; j++)
			{
				for(k = 0; k <= divs2; k++)
				{
					float p1[3], p2[3], p3[3], p[3]={0,0,0}; 
					const float uf = (float)(j + TRI_UVOFFSET) / (float)(divs1 + 0.0);
					const float vf = (float)(k + TRI_UVOFFSET) / (float)(divs2 + 0.0);
					float tmpvec[3];
					
					if(uf+vf > 1.0) 
					{
						// printf("bigger - divs1: %d, divs2: %d\n", divs1, divs2);
						continue;
					}

					copy_v3_v3(p1, mvert[ mface[i].v1 ].co);
					if(again == 1 && mface[i].v4)
					{
						copy_v3_v3(p2, mvert[ mface[i].v3 ].co);
						copy_v3_v3(p3, mvert[ mface[i].v4 ].co);
					}
					else {
						copy_v3_v3(p2, mvert[ mface[i].v2 ].co);
						copy_v3_v3(p3, mvert[ mface[i].v3 ].co);
					}

					mul_v3_fl(p1, (1.0-uf-vf));
					mul_v3_fl(p2, uf);
					mul_v3_fl(p3, vf);
					
					add_v3_v3v3(p, p1, p2);
					add_v3_v3(p, p3);

					if(newdivs > divs)
						printf("mem problem\n");

					// mMovPoints.push_back(p + trinorm);
					add_v3_v3v3(tmpvec, p, trinorm);
					// mul_m4_v3(ob->obmat, tmpvec); // DG: use local coordinates, we save MAT anyway
					copy_v3_v3(&scs->points[3 * (dm->getNumVerts(dm) + newdivs)], tmpvec);
					newdivs++;

					if(newdivs > divs)
						printf("mem problem\n");

					// mMovPoints.push_back(p - trinorm);
					copy_v3_v3(tmpvec, p);
					sub_v3_v3(tmpvec, trinorm);
					// mul_m4_v3(ob->obmat, tmpvec); // DG: use local coordinates, we save MAT anyway
					copy_v3_v3(&scs->points[3 * (dm->getNumVerts(dm) + newdivs)], tmpvec);
					newdivs++;
				}
			}

			if(again == 0 && mface[i].v4)
				again++;
			else
				again = 0;

			facecounter++;

		} while(again!=0);
	}

	scs->numverts = dm->getNumVerts(dm);
	// DG TODO: also save triangle count?

	scs->numpoints = dm->getNumVerts(dm) + newdivs;

	for(i = 0; i < scs->numpoints * 3; i++)
	{
		scs->points_old[i] = scs->points[i];
	}
}


static void fill_scs_points_anim(Object *UNUSED(ob), DerivedMesh *dm, SmokeCollSettings *scs)
{
	MVert *mvert = dm->getVertArray(dm);
	MFace *mface = dm->getTessFaceArray(dm);
	int quads = 0, numtris = 0, facecounter = 0;
	unsigned int i = 0;
	int divs = 0, newdivs = 0;
	
	// DG TODO: need to do this dynamically according to the domain object!
	float cell_len = scs->dx;

	// count quads
	for(i = 0; i < dm->getNumTessFaces(dm); i++)
	{
		if(mface[i].v4)
			quads++;
	}

	numtris = dm->getNumTessFaces(dm) + quads;

	// check if mesh changed topology
	if(scs->numtris != numtris)
		return;
	if(scs->numverts != dm->getNumVerts(dm))
		return;

	// update new positions
	for(i = 0; i < dm->getNumVerts(dm); i++)
	{
		float tmpvec[3];
		copy_v3_v3(tmpvec, mvert[i].co);
		copy_v3_v3(&scs->points[i * 3], tmpvec);
	}

	// for every triangle // update div points
	for(i = 0, facecounter = 0; i < dm->getNumTessFaces(dm); i++)
	{
		int again = 0;
		do
		{
			int j, k;
			int divs1 = scs->tridivs[3 * facecounter + 0];
			int divs2 = scs->tridivs[3 * facecounter + 1];
			float srcside1[3], srcside2[3], destside1[3], destside2[3], src_trinormorg[3], dest_trinormorg[3], src_trinorm[3], dest_trinorm[3];
			
			if(again == 1 && mface[i].v4)
			{
				sub_v3_v3v3(srcside1,  &scs->points_old[mface[i].v3 * 3], &scs->points_old[mface[i].v1 * 3]);
				sub_v3_v3v3(destside1,  &scs->points[mface[i].v3 * 3], &scs->points[mface[i].v1 * 3]);

				sub_v3_v3v3(srcside2,  &scs->points_old[mface[i].v4 * 3], &scs->points_old[mface[i].v1 * 3]);
				sub_v3_v3v3(destside2,  &scs->points[mface[i].v4 * 3], &scs->points[mface[i].v1 * 3]);
			}
			else {
				sub_v3_v3v3(srcside1,  &scs->points_old[mface[i].v2 * 3], &scs->points_old[mface[i].v1 * 3]);
				sub_v3_v3v3(destside1,  &scs->points[mface[i].v2 * 3], &scs->points[mface[i].v1 * 3]);

				sub_v3_v3v3(srcside2,  &scs->points_old[mface[i].v3 * 3], &scs->points_old[mface[i].v1 * 3]);
				sub_v3_v3v3(destside2,  &scs->points[mface[i].v3 * 3], &scs->points[mface[i].v1 * 3]);
			}

			cross_v3_v3v3(src_trinormorg, srcside1, srcside2);
			cross_v3_v3v3(dest_trinormorg, destside1, destside2);

			normalize_v3(src_trinormorg);
			normalize_v3(dest_trinormorg);

			copy_v3_v3(src_trinorm, src_trinormorg);
			copy_v3_v3(dest_trinorm, dest_trinormorg);

			mul_v3_fl(src_trinorm, 0.25 * cell_len);
			mul_v3_fl(dest_trinorm, 0.25 * cell_len);

			for(j = 0; j <= divs1; j++)
			{
				for(k = 0; k <= divs2; k++)
				{
					float src_p1[3], src_p2[3], src_p3[3], src_p[3]={0,0,0};
					float dest_p1[3], dest_p2[3], dest_p3[3], dest_p[3]={0,0,0};
					const float uf = (float)(j + TRI_UVOFFSET) / (float)(divs1 + 0.0);
					const float vf = (float)(k + TRI_UVOFFSET) / (float)(divs2 + 0.0);
					float src_tmpvec[3], dest_tmpvec[3];
					
					if(uf+vf > 1.0) 
					{
						// printf("bigger - divs1: %d, divs2: %d\n", divs1, divs2);
						continue;
					}

					copy_v3_v3(src_p1, &scs->points_old[mface[i].v1 * 3]);
					copy_v3_v3(dest_p1, &scs->points[mface[i].v1 * 3]);
					if(again == 1 && mface[i].v4)
					{
						copy_v3_v3(src_p2, &scs->points_old[mface[i].v3 * 3]);
						copy_v3_v3(dest_p2, &scs->points[mface[i].v3 * 3]);

						copy_v3_v3(src_p3,&scs->points_old[mface[i].v4 * 3]);
						copy_v3_v3(dest_p3, &scs->points[mface[i].v4 * 3]);
					}
					else {
						copy_v3_v3(src_p2, &scs->points_old[mface[i].v2 * 3]);
						copy_v3_v3(dest_p2, &scs->points[mface[i].v2 * 3]);
						copy_v3_v3(src_p3, &scs->points_old[mface[i].v3 * 3]);
						copy_v3_v3(dest_p3, &scs->points[mface[i].v3 * 3]);
					}

					mul_v3_fl(src_p1, (1.0-uf-vf));
					mul_v3_fl(dest_p1, (1.0-uf-vf));

					mul_v3_fl(src_p2, uf);
					mul_v3_fl(dest_p2, uf);

					mul_v3_fl(src_p3, vf);
					mul_v3_fl(dest_p3, vf);

					add_v3_v3v3(src_p, src_p1, src_p2);
					add_v3_v3v3(dest_p, dest_p1, dest_p2);

					add_v3_v3(src_p, src_p3);
					add_v3_v3(dest_p, dest_p3);

					if(newdivs > divs)
						printf("mem problem\n");

					// mMovPoints.push_back(p + trinorm);
					add_v3_v3v3(src_tmpvec, src_p, src_trinorm);
					add_v3_v3v3(dest_tmpvec, dest_p, dest_trinorm);

					// mul_m4_v3(ob->obmat, tmpvec); // DG: use local coordinates, we save MAT anyway
					copy_v3_v3(&scs->points_old[3 * (dm->getNumVerts(dm) + newdivs)], src_tmpvec);
					copy_v3_v3(&scs->points[3 * (dm->getNumVerts(dm) + newdivs)], dest_tmpvec);
					newdivs++;

					if(newdivs > divs)
						printf("mem problem\n");

					// mMovPoints.push_back(p - trinorm);
					copy_v3_v3(src_tmpvec, src_p);
					copy_v3_v3(dest_tmpvec, dest_p);

					sub_v3_v3(src_tmpvec, src_trinorm);
					sub_v3_v3(dest_tmpvec, dest_trinorm);

					// mul_m4_v3(ob->obmat, tmpvec); // DG: use local coordinates, we save MAT anyway
					copy_v3_v3(&scs->points_old[3 * (dm->getNumVerts(dm) + newdivs)], src_tmpvec);
					copy_v3_v3(&scs->points[3 * (dm->getNumVerts(dm) + newdivs)], dest_tmpvec);
					newdivs++;
				}
			}

			if(again == 0 && mface[i].v4)
				again++;
			else
				again = 0;

			facecounter++;

		} while(again!=0);
	}

	// scs->numpoints = dm->getNumVerts(dm) + newdivs;

}

/*! init triangle divisions */
static void calcTriangleDivs(Object *ob, MVert *verts, int UNUSED(numverts), MFace *faces, int numfaces, int numtris, int **tridivs, float cell_len)
{
	// mTriangleDivs1.resize( faces.size() );
	// mTriangleDivs2.resize( faces.size() );
	// mTriangleDivs3.resize( faces.size() );

	size_t i = 0, facecounter = 0;
	float maxscale[3] = {1,1,1}; // = channelFindMaxVf(mcScale); get max scale value
	float maxpart = ABS(maxscale[0]);
	float scaleFac = 0;
	float fsTri = 0;
	if(ABS(maxscale[1])>maxpart) maxpart = ABS(maxscale[1]);
	if(ABS(maxscale[2])>maxpart) maxpart = ABS(maxscale[2]);
	scaleFac = 1.0 / maxpart;
	// featureSize = mLevel[mMaxRefine].nodeSize
	fsTri = cell_len * 0.75 * scaleFac; // fsTri = cell_len * 0.9;

	if(*tridivs)
		MEM_freeN(*tridivs);

	*tridivs = MEM_callocN(sizeof(int) * numtris * 3, "Smoke_Tridivs");

	for(i = 0, facecounter = 0; i < numfaces; i++) 
	{
		float p0[3], p1[3], p2[3];
		float side1[3];
		float side2[3];
		float side3[3];
		int divs1=0, divs2=0, divs3=0;

		copy_v3_v3(p0, verts[faces[i].v1].co);
		mul_m4_v3(ob->obmat, p0);
		copy_v3_v3(p1, verts[faces[i].v2].co);
		mul_m4_v3(ob->obmat, p1);
		copy_v3_v3(p2, verts[faces[i].v3].co);
		mul_m4_v3(ob->obmat, p2);

		sub_v3_v3v3(side1, p1, p0);
		sub_v3_v3v3(side2, p2, p0);
		sub_v3_v3v3(side3, p1, p2);

		if(dot_v3v3(side1, side1) > fsTri*fsTri)
		{ 
			float tmp = normalize_v3(side1);
			divs1 = (int)ceil(tmp/fsTri); 
		}
		if(dot_v3v3(side2, side2) > fsTri*fsTri)
		{ 
			float tmp = normalize_v3(side2);
			divs2 = (int)ceil(tmp/fsTri); 
			
			/*		
			// debug
			if(i==0)
				printf("b tmp: %f, fsTri: %f, divs2: %d\n", tmp, fsTri, divs2);
			*/
			
		}

		(*tridivs)[3 * facecounter + 0] = divs1;
		(*tridivs)[3 * facecounter + 1] = divs2;
		(*tridivs)[3 * facecounter + 2] = divs3;

		// TODO quad case
		if(faces[i].v4)
		{
			divs1=0, divs2=0, divs3=0;

			facecounter++;
			
			copy_v3_v3(p0, verts[faces[i].v3].co);
			mul_m4_v3(ob->obmat, p0);
			copy_v3_v3(p1, verts[faces[i].v4].co);
			mul_m4_v3(ob->obmat, p1);
			copy_v3_v3(p2, verts[faces[i].v1].co);
			mul_m4_v3(ob->obmat, p2);

			sub_v3_v3v3(side1, p1, p0);
			sub_v3_v3v3(side2, p2, p0);
			sub_v3_v3v3(side3, p1, p2);

			if(dot_v3v3(side1, side1) > fsTri*fsTri)
			{ 
				float tmp = normalize_v3(side1);
				divs1 = (int)ceil(tmp/fsTri); 
			}
			if(dot_v3v3(side2, side2) > fsTri*fsTri)
			{ 
				float tmp = normalize_v3(side2);
				divs2 = (int)ceil(tmp/fsTri); 
			}

			(*tridivs)[3 * facecounter + 0] = divs1;
			(*tridivs)[3 * facecounter + 1] = divs2;
			(*tridivs)[3 * facecounter + 2] = divs3;
		}
		facecounter++;
	}
}

#endif /* WITH_SMOKE */

static void smokeModifier_freeDomain(SmokeModifierData *smd)
{
	if(smd->domain)
	{
		if(smd->domain->shadow)
				MEM_freeN(smd->domain->shadow);
			smd->domain->shadow = NULL;

		if(smd->domain->fluid)
			smoke_free(smd->domain->fluid);

		if(smd->domain->wt)
			smoke_turbulence_free(smd->domain->wt);

		if(smd->domain->effector_weights)
				MEM_freeN(smd->domain->effector_weights);
		smd->domain->effector_weights = NULL;

		BKE_ptcache_free_list(&(smd->domain->ptcaches[0]));
		smd->domain->point_cache[0] = NULL;

		MEM_freeN(smd->domain);
		smd->domain = NULL;
	}
}

static void smokeModifier_freeFlow(SmokeModifierData *smd)
{
	if(smd->flow)
	{
/*
		if(smd->flow->bvh)
		{
			free_bvhtree_from_mesh(smd->flow->bvh);
			MEM_freeN(smd->flow->bvh);
		}
		smd->flow->bvh = NULL;
*/
		if (smd->flow->dm) smd->flow->dm->release(smd->flow->dm);
		if (smd->flow->verts_old) MEM_freeN(smd->flow->verts_old);
		MEM_freeN(smd->flow);
		smd->flow = NULL;
	}
}

static void smokeModifier_freeCollision(SmokeModifierData *smd)
{
	if(smd->coll)
	{
		SmokeCollSettings *scs = smd->coll;

		if(scs->numpoints)
		{
			if(scs->points)
			{
				MEM_freeN(scs->points);
				scs->points = NULL;
			}
			if(scs->points_old)
			{
				MEM_freeN(scs->points_old);
				scs->points_old = NULL;
			}
			if(scs->tridivs)
			{
				MEM_freeN(scs->tridivs);
				scs->tridivs = NULL;
			}
		}

		if(scs->bvhtree)
		{
			BLI_bvhtree_free(scs->bvhtree);
			scs->bvhtree = NULL;
		}

#ifdef USE_SMOKE_COLLISION_DM
		if(smd->coll->dm)
			smd->coll->dm->release(smd->coll->dm);
		smd->coll->dm = NULL;
#endif

		MEM_freeN(smd->coll);
		smd->coll = NULL;
	}
}

void smokeModifier_reset_turbulence(struct SmokeModifierData *smd)
{
	if(smd && smd->domain && smd->domain->wt)
	{
		smoke_turbulence_free(smd->domain->wt);
		smd->domain->wt = NULL;
	}
}

void smokeModifier_reset(struct SmokeModifierData *smd)
{
	if(smd)
	{
		if(smd->domain)
		{
			if(smd->domain->shadow)
				MEM_freeN(smd->domain->shadow);
			smd->domain->shadow = NULL;

			if(smd->domain->fluid)
			{
				smoke_free(smd->domain->fluid);
				smd->domain->fluid = NULL;
			}

			smokeModifier_reset_turbulence(smd);

			smd->time = -1;

			// printf("reset domain end\n");
		}
		else if(smd->flow)
		{
			/*
			if(smd->flow->bvh)
			{
				free_bvhtree_from_mesh(smd->flow->bvh);
				MEM_freeN(smd->flow->bvh);
			}
			smd->flow->bvh = NULL;
			*/
		}
		else if(smd->coll)
		{
			SmokeCollSettings *scs = smd->coll;

			if(scs->numpoints && scs->points)
			{
				MEM_freeN(scs->points);
				scs->points = NULL;
			
				if(scs->points_old)
				{
					MEM_freeN(scs->points_old);
					scs->points_old = NULL;
				}
				if(scs->tridivs)
				{
					MEM_freeN(scs->tridivs);
					scs->tridivs = NULL;
				}
			}
		}
	}
}

void smokeModifier_free(SmokeModifierData *smd)
{
	if(smd)
	{
		smokeModifier_freeDomain(smd);
		smokeModifier_freeFlow(smd);
		smokeModifier_freeCollision(smd);
	}
}

void smokeModifier_createType(struct SmokeModifierData *smd)
{
	if(smd)
	{
		if(smd->type & MOD_SMOKE_TYPE_DOMAIN)
		{
			if(smd->domain)
				smokeModifier_freeDomain(smd);

			smd->domain = MEM_callocN(sizeof(SmokeDomainSettings), "SmokeDomain");

			smd->domain->smd = smd;

			smd->domain->point_cache[0] = BKE_ptcache_add(&(smd->domain->ptcaches[0]));
			smd->domain->point_cache[0]->flag |= PTCACHE_DISK_CACHE;
			smd->domain->point_cache[0]->step = 1;

			/* Deprecated */
			smd->domain->point_cache[1] = NULL;
			smd->domain->ptcaches[1].first = smd->domain->ptcaches[1].last = NULL;
			/* set some standard values */
			smd->domain->fluid = NULL;
			smd->domain->wt = NULL;			
			smd->domain->eff_group = NULL;
			smd->domain->fluid_group = NULL;
			smd->domain->coll_group = NULL;
			smd->domain->maxres = 32;
			smd->domain->amplify = 1;			
			smd->domain->omega = 1.0;			
			smd->domain->alpha = -0.001;
			smd->domain->beta = 0.1;
			smd->domain->time_scale = 1.0;
			smd->domain->vorticity = 2.0;
			smd->domain->border_collisions = SM_BORDER_OPEN; // open domain
			smd->domain->flags = MOD_SMOKE_DISSOLVE_LOG | MOD_SMOKE_HIGH_SMOOTH;
			smd->domain->strength = 2.0;
			smd->domain->noise = MOD_SMOKE_NOISEWAVE;
			smd->domain->diss_speed = 5;

			smd->domain->burning_rate = 0.75f;
			smd->domain->flame_smoke = 1.0f;
			smd->domain->flame_vorticity = 0.5f;
			smd->domain->flame_ignition = 1.25f;
			smd->domain->flame_max_temp = 1.75f;

			smd->domain->viewsettings = MOD_SMOKE_VIEW_SHOWBIG;
			smd->domain->effector_weights = BKE_add_effector_weights(NULL);
		}
		else if(smd->type & MOD_SMOKE_TYPE_FLOW)
		{
			if(smd->flow)
				smokeModifier_freeFlow(smd);

			smd->flow = MEM_callocN(sizeof(SmokeFlowSettings), "SmokeFlow");

			smd->flow->smd = smd;

			/* set some standard values */
			smd->flow->density = 1.0;
			smd->flow->fuel_amount = 1.0;
			smd->flow->temp = 1.0;
			smd->flow->flags = MOD_SMOKE_FLOW_ABSOLUTE;
			smd->flow->vel_multi = 1.0;
			smd->flow->surface_distance = 1.5;
			smd->flow->source = MOD_SMOKE_FLOW_SOURCE_MESH;

			smd->flow->dm = NULL;
			smd->flow->psys = NULL;

		}
		else if(smd->type & MOD_SMOKE_TYPE_COLL)
		{
			if(smd->coll)
				smokeModifier_freeCollision(smd);

			smd->coll = MEM_callocN(sizeof(SmokeCollSettings), "SmokeColl");

			smd->coll->smd = smd;
			smd->coll->points = NULL;
			smd->coll->points_old = NULL;
			smd->coll->tridivs = NULL;
			smd->coll->vel = NULL;
			smd->coll->numpoints = 0;
			smd->coll->numtris = 0;
			smd->coll->bvhtree = NULL;
			smd->coll->type = 0; // static obstacle
			smd->coll->dx = 1.0f / 50.0f;

#ifdef USE_SMOKE_COLLISION_DM
			smd->coll->dm = NULL;
#endif
		}
	}
}

void smokeModifier_copy(struct SmokeModifierData *smd, struct SmokeModifierData *tsmd)
{
	tsmd->type = smd->type;
	tsmd->time = smd->time;
	
	smokeModifier_createType(tsmd);

	if (tsmd->domain) {
		tsmd->domain->maxres = smd->domain->maxres;
		tsmd->domain->amplify = smd->domain->amplify;
		tsmd->domain->omega = smd->domain->omega;
		tsmd->domain->alpha = smd->domain->alpha;
		tsmd->domain->beta = smd->domain->beta;
		tsmd->domain->flags = smd->domain->flags;
		tsmd->domain->strength = smd->domain->strength;
		tsmd->domain->noise = smd->domain->noise;
		tsmd->domain->diss_speed = smd->domain->diss_speed;
		tsmd->domain->viewsettings = smd->domain->viewsettings;
		tsmd->domain->fluid_group = smd->domain->fluid_group;
		tsmd->domain->coll_group = smd->domain->coll_group;
		tsmd->domain->vorticity = smd->domain->vorticity;
		tsmd->domain->time_scale = smd->domain->time_scale;
		tsmd->domain->border_collisions = smd->domain->border_collisions;

		tsmd->domain->burning_rate = smd->domain->burning_rate;
		tsmd->domain->flame_smoke = smd->domain->flame_smoke;
		tsmd->domain->flame_vorticity = smd->domain->flame_vorticity;
		tsmd->domain->flame_ignition = smd->domain->flame_ignition;
		tsmd->domain->flame_max_temp = smd->domain->flame_max_temp;
		
		MEM_freeN(tsmd->domain->effector_weights);
		tsmd->domain->effector_weights = MEM_dupallocN(smd->domain->effector_weights);
	} 
	else if (tsmd->flow) {
		tsmd->flow->psys = smd->flow->psys;
		tsmd->flow->type = smd->flow->type;
		tsmd->flow->flags = smd->flow->flags;
		tsmd->flow->vel_multi = smd->flow->vel_multi;
		tsmd->flow->vel_normal = smd->flow->vel_normal;
		tsmd->flow->vel_random = smd->flow->vel_random;

		tsmd->flow->density = smd->flow->density;
		tsmd->flow->fuel_amount = smd->flow->fuel_amount;
		tsmd->flow->temp = smd->flow->temp;
		tsmd->flow->volume_density = smd->flow->volume_density;
		tsmd->flow->surface_distance = smd->flow->surface_distance;
		tsmd->flow->vgroup_density = smd->flow->vgroup_density;

		tsmd->flow->source = smd->flow->source;
	}
	else if (tsmd->coll) {
		;
		/* leave it as initialized, collision settings is mostly caches */
	}
}

#ifdef WITH_SMOKE

// forward decleration
static void smoke_calc_transparency(float *result, float *input, float *p0, float *p1, int res[3], float dx, float *light, bresenham_callback cb, float correct);
static float calc_voxel_transp(float *result, float *input, int res[3], int *pixel, float *tRay, float correct);

static int get_lamp(Scene *scene, float *light)
{	
	Base *base_tmp = NULL;	
	int found_lamp = 0;

	// try to find a lamp, preferably local
	for(base_tmp = scene->base.first; base_tmp; base_tmp= base_tmp->next) {
		if(base_tmp->object->type == OB_LAMP) {
			Lamp *la = base_tmp->object->data;

			if(la->type == LA_LOCAL) {
				copy_v3_v3(light, base_tmp->object->obmat[3]);
				return 1;
			}
			else if(!found_lamp) {
				copy_v3_v3(light, base_tmp->object->obmat[3]);
				found_lamp = 1;
			}
		}
	}

	return found_lamp;
}

static void smoke_calc_domain(Scene *UNUSED(scene), Object *UNUSED(ob), SmokeModifierData *UNUSED(smd))
{
#if 0
	SmokeDomainSettings *sds = smd->domain;
	GroupObject *go = NULL;			
	Base *base = NULL;

	/* do collisions, needs to be done before emission, so that smoke isn't emitted inside collision cells */
	if(1)
	{
		unsigned int i;
		Object **collobjs = NULL;
		unsigned int numcollobj = 0;
		collobjs = get_collisionobjects(scene, ob, sds->coll_group, &numcollobj);

		for(i = 0; i < numcollobj; i++)
		{
			Object *collob= collobjs[i];
			SmokeModifierData *smd2 = (SmokeModifierData*)modifiers_findByType(collob, eModifierType_Smoke);		
			
			// check for active smoke modifier
			// if(md && md->mode & (eModifierMode_Realtime | eModifierMode_Render))
			// SmokeModifierData *smd2 = (SmokeModifierData *)md;

			if((smd2->type & MOD_SMOKE_TYPE_COLL) && smd2->coll && smd2->coll->points && smd2->coll->points_old)
			{
				// ??? anything to do here?

				// TODO: only something to do for ANIMATED obstacles: need to update positions

			}
		}

		if(collobjs)
			MEM_freeN(collobjs);
	}

#endif
}

/* Animated obstacles: dx_step = ((x_new - x_old) / totalsteps) * substep */
static void update_obstacles(Scene *scene, Object *ob, SmokeDomainSettings *sds, float dt, int substep, int totalsteps)
{
	Object **collobjs = NULL;
	unsigned int numcollobj = 0;

	unsigned int collIndex;
	unsigned char *obstacles = smoke_get_obstacle(sds->fluid);
	float *velx = NULL;
	float *vely = NULL;
	float *velz = NULL;
	float *velxOrig = smoke_get_velocity_x(sds->fluid);
	float *velyOrig = smoke_get_velocity_y(sds->fluid);
	float *velzOrig = smoke_get_velocity_z(sds->fluid);
	// float *density = smoke_get_density(sds->fluid);
	unsigned int z;

	smoke_get_ob_velocity(sds->fluid, &velx, &vely, &velz);

	// TODO: delete old obstacle flags
	for(z = 0; z < sds->res[0] * sds->res[1] * sds->res[2]; z++)
	{
		if(obstacles[z])
		{
			// density[z] = 0;

			velxOrig[z] = 0;
			velyOrig[z] = 0;
			velzOrig[z] = 0;
		}

		if(obstacles[z] & 8) // Do not delete static obstacles
		{
			obstacles[z] = 0;
		}

		velx[z] = 0;
		vely[z] = 0;
		velz[z] = 0;
	}

	collobjs = get_collisionobjects(scene, ob, sds->coll_group, &numcollobj, eModifierType_Smoke);

	// update obstacle tags in cells
	for(collIndex = 0; collIndex < numcollobj; collIndex++)
	{
		Object *collob= collobjs[collIndex];
		SmokeModifierData *smd2 = (SmokeModifierData*)modifiers_findByType(collob, eModifierType_Smoke);

		// DG TODO: check if modifier is active?
		
		if((smd2->type & MOD_SMOKE_TYPE_COLL) && smd2->coll && smd2->coll->points && smd2->coll->points_old)
		{
			SmokeCollSettings *scs = smd2->coll;
			unsigned int i;

			/*
			// DG TODO: support static cobstacles, but basicly we could even support static + rigid with one set of code
			if(scs->type > SM_COLL_STATIC)
			*/

			/* Handle collisions */
			for(i = 0; i < scs->numpoints; i++)
			{
				// 1. get corresponding cell
				int cell[3];
				float pos[3], oldpos[3], vel[3];
				float cPos[3], cOldpos[3]; /* current position in substeps */
				int badcell = 0;
				size_t index;
				int j;

				// translate local points into global positions
				copy_v3_v3(cPos, &scs->points[3 * i]);
				mul_m4_v3(scs->mat, cPos);
				copy_v3_v3(pos, cPos);

				copy_v3_v3(cOldpos, &scs->points_old[3 * i]);
				mul_m4_v3(scs->mat_old, cOldpos);
				copy_v3_v3(oldpos, cOldpos);

				/* support for rigid bodies, armatures etc */
				{
					float tmp[3];

					/* x_current = x_old + (x_new - x_old) * step_current / steps_total */
					float mulStep = (float)(((float)substep) / ((float)totalsteps));

					sub_v3_v3v3(tmp, cPos, cOldpos);
					mul_v3_fl(tmp, mulStep);
					add_v3_v3(cOldpos, tmp);
				}

				sub_v3_v3v3(vel, pos, oldpos);
				/* Scale velocity to incorperate the object movement during this step */
				mul_v3_fl(vel, 1.0 / (totalsteps * dt * sds->scale));
				// mul_v3_fl(vel, 1.0 / dt);

				// DG TODO: cap velocity to maxVelMag (or maxvel)

				// oldpos + velocity * dt = newpos
				get_cell(sds->p0, sds->res, sds->dx*sds->scale, cOldpos /* use current position here instead of "pos" */, cell, 0);

				// check if cell is valid (in the domain boundary)
				for(j = 0; j < 3; j++)
					if((cell[j] > sds->res[j] - 1) || (cell[j] < 0))
					{
						badcell = 1;
						break;
					}
														
				if(badcell)
					continue;

				// 2. set cell values (heat, density and velocity)
				index = smoke_get_index(cell[0], sds->res[0], cell[1], sds->res[1], cell[2]);

				// Don't overwrite existing obstacles
				if(obstacles[index])
					continue;
												
				// printf("cell[0]: %d, cell[1]: %d, cell[2]: %d\n", cell[0], cell[1], cell[2]);								
				// printf("res[0]: %d, res[1]: %d, res[2]: %d, index: %d\n\n", sds->res[0], sds->res[1], sds->res[2], index);																	
				obstacles[index] = 1 | 8 /* ANIMATED */;

				if(len_v3(vel) > FLT_EPSILON)
				{
					// Collision object is moving

					velx[index] = vel[0]; // use "+="?
					vely[index] = vel[1];
					velz[index] = vel[2];
				}
			}
		}
	}

	if(collobjs)
		MEM_freeN(collobjs);
}

static void emit_from_particles(Object *flow_ob, SmokeDomainSettings *sds, SmokeFlowSettings *sfs, float *emission_map, float *velocity_map, Scene *scene, float time)
{
	if(sfs && sfs->psys && sfs->psys->part && sfs->psys->part->type==PART_EMITTER) // is particle system selected
	{
		ParticleSimulationData sim;
		ParticleSystem *psys = sfs->psys;
		int totpart=psys->totpart, totchild;
		int p = 0;

		sim.scene = scene;
		sim.ob = flow_ob;
		sim.psys = psys;

		if(psys->part->type==PART_HAIR)
		{
			// TODO: PART_HAIR not supported whatsoever
			totchild=0;
		}
		else
			totchild=psys->totchild*psys->part->disp/100;

		for(p=0; p<totpart+totchild; p++)								
		{
			int cell[3];
			size_t i = 0;
			size_t index = 0;
			int badcell = 0;
			ParticleKey state;

			if(p < totpart) {
				if(psys->particles[p].flag & (PARS_NO_DISP|PARS_UNEXIST))
					continue;
			}
			else {
				/* handle child particle */
				ChildParticle *cpa = &psys->child[p - totpart];

				if(psys->particles[cpa->parent].flag & (PARS_NO_DISP|PARS_UNEXIST))
					continue;
			}

			state.time = time;
			if(psys_get_particle_state(&sim, p, &state, 0) == 0)
				continue;

			// 1. get corresponding cell
			get_cell(sds->p0, sds->res, sds->dx*sds->scale, state.co, cell, 0);																	
			// check if cell is valid (in the domain boundary)									
			for(i = 0; i < 3; i++)									
			{										
				if((cell[i] > sds->res[i] - 1) || (cell[i] < 0))										
				{											
					badcell = 1;											
					break;										
				}									
			}																			
			if(badcell)										
				continue;																		
			// 2. set cell values (heat, density and velocity)									
			index = smoke_get_index(cell[0], sds->res[0], cell[1], sds->res[1], cell[2]);																										
			// Add density to emission map
			emission_map[index] = 1.0f;
			// Uses particle velocity as initial velocity for smoke
			if(sfs->flags & MOD_SMOKE_FLOW_INITVELOCITY && (psys->part->phystype != PART_PHYS_NO))
			{
				velocity_map[index*3] = state.vel[0]*sfs->vel_multi;
				velocity_map[index*3+1] = state.vel[1]*sfs->vel_multi;
				velocity_map[index*3+2] = state.vel[2]*sfs->vel_multi;
			}
		}	// particles loop
	}
}

static void emit_from_derivedmesh(Object *flow_ob, SmokeDomainSettings *sds, SmokeFlowSettings *sfs, float *emission_map, float *velocity_map, float dt)
{
	if (!sfs->dm) return;
	{
		DerivedMesh *dm = NULL;
		int defgrp_index = sfs->vgroup_density-1;
		MDeformVert *dvert = NULL;
		MVert *mvert = NULL;
		MFace *mface = NULL;
		BVHTreeFromMesh treeData = {0};
		int numOfVerts, i, z;
		int *res = sds->res;

		float *vert_vel = NULL;
		int has_velocity = 0;

		dm = CDDM_copy(sfs->dm);
		CDDM_calc_normals(dm);
		mvert = dm->getVertArray(dm);
		mface = dm->getTessFaceArray(dm);
		numOfVerts = dm->getNumVerts(dm);
		dvert = dm->getVertDataArray(dm, CD_MDEFORMVERT);

		if (sfs->flags & MOD_SMOKE_FLOW_INITVELOCITY) {
			vert_vel = MEM_callocN(sizeof(float) * numOfVerts * 3, "smoke_flow_velocity");

			if (sfs->numverts != numOfVerts || !sfs->verts_old) {
				if (sfs->verts_old) MEM_freeN(sfs->verts_old);
				sfs->verts_old = MEM_callocN(sizeof(float) * numOfVerts * 3, "smoke_flow_verts_old");
				sfs->numverts = numOfVerts;
			}
			else {
				has_velocity = 1;
			}
		}

		/*	Transform collider vertices to
		*   domain grid space for fast lookups */
		for (i = 0; i < numOfVerts; i++) {
			float n[3];
			/* vert pos */
			mul_m4_v3(flow_ob->obmat, mvert[i].co);
			sub_v3_v3(mvert[i].co, sds->p0);
			mul_v3_fl(mvert[i].co, (1.0f/sds->dx)/sds->scale);
			/* vert normal */
			normal_short_to_float_v3(n, mvert[i].no);
			mul_mat3_m4_v3(flow_ob->obmat, n);
			normalize_v3(n);
			normal_float_to_short_v3(mvert[i].no, n);
			/* vert velocity */
			if (sfs->flags & MOD_SMOKE_FLOW_INITVELOCITY) {
				if (has_velocity) {
					sub_v3_v3v3(&vert_vel[i*3], mvert[i].co, &sfs->verts_old[i*3]);
					mul_v3_fl(&vert_vel[i*3], sds->dx/dt);
				}
				copy_v3_v3(&sfs->verts_old[i*3], mvert[i].co);
			}
		}

		if (bvhtree_from_mesh_faces(&treeData, dm, 0.0f, 4, 8)) {
			#pragma omp parallel for schedule(static)
			for (z = 0; z < res[2]; z++) {
				int x,y;
				for (x = 0; x < res[0]; x++)
				for (y = 0; y < res[1]; y++) {
					int index = x + y*res[0] + z*res[0]*res[1];

					float ray_start[3] = {(float)x + 0.5f, (float)y + 0.5f, (float)z + 0.5f};
					float ray_dir[3] = {1.0f, 0.0f, 0.0f};

					BVHTreeRayHit hit = {0};
					BVHTreeNearest nearest = {0};

					float volume_factor = 0.0f;
					float sample_str = 0.0f;

					hit.index = -1;
					hit.dist = 9999;
					nearest.index = -1;
					nearest.dist = sfs->surface_distance * sfs->surface_distance; /* find_nearest uses squared distance */

					/* Check volume collision */
					if (sfs->volume_density) {
						if (BLI_bvhtree_ray_cast(treeData.tree, ray_start, ray_dir, 0.0f, &hit, treeData.raycast_callback, &treeData) != -1) {
							float dot = ray_dir[0] * hit.no[0] + ray_dir[1] * hit.no[1] + ray_dir[2] * hit.no[2];
							/*  If ray and hit face normal are facing same direction
							 *	hit point is inside a closed mesh. */
							if (dot >= 0) {
								/* Also cast a ray in opposite direction to make sure
								 * point is at least surrounded by two brush faces */
								negate_v3(ray_dir);
								hit.index = -1;
								hit.dist = 9999;

								BLI_bvhtree_ray_cast(treeData.tree, ray_start, ray_dir, 0.0f, &hit, treeData.raycast_callback, &treeData);
								if (hit.index != -1) {
									/* Add factor on supersample filter	*/
									volume_factor = sfs->volume_density;
									nearest.dist = hit.dist*hit.dist;
								}
							}
						}
					}

					/* find the nearest point on the mesh */
					if (BLI_bvhtree_find_nearest(treeData.tree, ray_start, &nearest, treeData.nearest_callback, &treeData) != -1) {
						float weights[4];
						int v1, v2, v3, f_index = nearest.index;
						float n1[3], n2[3], n3[3], hit_normal[3];

						/* emit from surface based on distance */
						if (sfs->surface_distance) {
							sample_str = sqrtf(nearest.dist) / sfs->surface_distance;
							CLAMP(sample_str, 0.0f, 1.0f);
							sample_str = pow(1.0f - sample_str, 0.5f);
						}
						else
							sample_str = 0.0f;

						/* calculate barycentric weights for nearest point */
						v1 = mface[f_index].v1;
						v2 = (nearest.flags & BVH_ONQUAD) ? mface[f_index].v3 : mface[f_index].v2;
						v3 = (nearest.flags & BVH_ONQUAD) ? mface[f_index].v4 : mface[f_index].v3;
						interp_weights_face_v3(weights, mvert[v1].co, mvert[v2].co, mvert[v3].co, NULL, nearest.co);

						if(sfs->flags & MOD_SMOKE_FLOW_INITVELOCITY) {
							/* interpolate vertex normal vectors to get nearest point normal */
							normal_short_to_float_v3(n1, mvert[v1].no);
							normal_short_to_float_v3(n2, mvert[v2].no);
							normal_short_to_float_v3(n3, mvert[v3].no);
							interp_v3_v3v3v3(hit_normal, n1, n2, n3, weights);
							normalize_v3(hit_normal);
							/* apply normal directional and random velocity
							* - TODO: random disabled for now since it doesnt really work well as pressure calc smoothens it out... */
							velocity_map[index*3]   += hit_normal[0]*sfs->vel_normal * 0.25f;
							velocity_map[index*3+1] += hit_normal[1]*sfs->vel_normal * 0.25f;
							velocity_map[index*3+2] += hit_normal[2]*sfs->vel_normal * 0.25f;
							/* TODO: for fire emitted from mesh surface we can use
							*  Vf = Vs + (Ps/Pf - 1)*S to model gaseous expansion from solid to fuel */
							/* apply object velocity */
							if (has_velocity && sfs->vel_multi) {
								float hit_vel[3];
								interp_v3_v3v3v3(hit_vel, &vert_vel[v1*3], &vert_vel[v2*3], &vert_vel[v3*3], weights);
								velocity_map[index*3]   += hit_vel[0] * sfs->vel_multi;
								velocity_map[index*3+1] += hit_vel[1] * sfs->vel_multi;
								velocity_map[index*3+2] += hit_vel[2] * sfs->vel_multi;
							}
						}

						/* apply vertex group influence if used */
						if (defgrp_index >= 0 && dvert) {
							float weight_mask = defvert_find_weight(&dvert[v1], defgrp_index) * weights[0] +
												defvert_find_weight(&dvert[v2], defgrp_index) * weights[1] +
												defvert_find_weight(&dvert[v3], defgrp_index) * weights[2];
							sample_str *= weight_mask;
						}
					}

					/* multiply initial velocity by emitter influence */
					if(sfs->flags & MOD_SMOKE_FLOW_INITVELOCITY) {
						velocity_map[index*3]   *= sample_str;
						velocity_map[index*3+1] *= sample_str;
						velocity_map[index*3+2] *= sample_str;
					}

					/* apply volume factor */
					sample_str = MAX2(volume_factor, sample_str);
					emission_map[index] = sample_str;
				}
			}
		}
		/* free bvh tree */
		free_bvhtree_from_mesh(&treeData);
		dm->release(dm);

		if (vert_vel) MEM_freeN(vert_vel);

	}
}

static void update_flowsfluids(Scene *scene, Object *ob, SmokeDomainSettings *sds, float time, float dt)
{
	Object **flowobjs = NULL;
	unsigned int numflowobj = 0;
	unsigned int flowIndex;

	flowobjs = get_collisionobjects(scene, ob, sds->fluid_group, &numflowobj, eModifierType_Smoke);

	// update obstacle tags in cells
	for(flowIndex = 0; flowIndex < numflowobj; flowIndex++)
	{
		Object *collob= flowobjs[flowIndex];
		SmokeModifierData *smd2 = (SmokeModifierData*)modifiers_findByType(collob, eModifierType_Smoke);

		// check for initialized smoke object
		if((smd2->type & MOD_SMOKE_TYPE_FLOW) && smd2->flow)						
		{
			// we got nice flow object
			SmokeFlowSettings *sfs = smd2->flow;
							
			float *density = smoke_get_density(sds->fluid);
			float *fuel = smoke_get_fuel(sds->fluid);	
			float *bigdensity = smoke_turbulence_get_density(sds->wt);	
			float *bigfuel = smoke_turbulence_get_fuel(sds->wt);
			float *heat = smoke_get_heat(sds->fluid);								
			float *velocity_x = smoke_get_velocity_x(sds->fluid);								
			float *velocity_y = smoke_get_velocity_y(sds->fluid);								
			float *velocity_z = smoke_get_velocity_z(sds->fluid);								
			//unsigned char *obstacle = smoke_get_obstacle(sds->fluid);							
			// DG TODO UNUSED unsigned char *obstacleAnim = smoke_get_obstacle_anim(sds->fluid);
			int bigres[3];
			short absolute_flow = (sfs->flags & MOD_SMOKE_FLOW_ABSOLUTE);
			short high_emission_smoothing = (sds->flags & MOD_SMOKE_HIGH_SMOOTH);
			float *velocity_map = NULL;
			float *emission_map = NULL;

			int ii, jj, kk, x, y, z, block_size;
			size_t index, index_big;

			/*
			* A temporary volume map used to store whole emissive
			* area to be added to smoke density and interpolated
			* for high resolution smoke.
			*/
			emission_map = MEM_callocN(sizeof(float) * sds->res[0]*sds->res[1]*sds->res[2], "SmokeEmissionDensity");
			if(sfs->flags & MOD_SMOKE_FLOW_INITVELOCITY) {
				velocity_map = MEM_callocN(sizeof(float) * sds->res[0]*sds->res[1]*sds->res[2] * 3, "SmokeEmissionVelocity");
			}

			if (sfs->source == MOD_SMOKE_FLOW_SOURCE_PARTICLES) {
				emit_from_particles(collob, sds, sfs, emission_map, velocity_map, scene, time);
			}
			else {
				emit_from_derivedmesh(collob, sds, sfs, emission_map, velocity_map, dt);
			}

			// loop through every low res cell
			for(x = 0; x < sds->res[0]; x++)
				for(y = 0; y < sds->res[1]; y++)
					for(z = 0; z < sds->res[2]; z++)													
					{
						// get cell index
						index = smoke_get_index(x, sds->res[0], y, sds->res[1], z);

						if(sfs->type == MOD_SMOKE_FLOW_TYPE_OUTFLOW) { // outflow
							heat[index] = 0.f;
							density[index] = 0.f;
							fuel[index] = 0.f;
							velocity_x[index] = 0.f;
							velocity_y[index] = 0.f;
							velocity_z[index] = 0.f;
						}
						else { // inflow
							heat[index] = MAX2(emission_map[index]*sfs->temp, heat[index]);
							if (absolute_flow) // absolute
							{
								if (sfs->type != MOD_SMOKE_FLOW_TYPE_FIRE) {
									if (emission_map[index] * sfs->density > density[index])
										density[index] = emission_map[index] * sfs->density;
								}
								if (sfs->type != MOD_SMOKE_FLOW_TYPE_SMOKE) {
									if (emission_map[index] * sfs->fuel_amount > fuel[index])
										fuel[index] = emission_map[index] * sfs->fuel_amount;
								}
							}
							else // additive
							{
								if (sfs->type != MOD_SMOKE_FLOW_TYPE_FIRE)
									density[index] += emission_map[index] * sfs->density;
								if (sfs->type != MOD_SMOKE_FLOW_TYPE_SMOKE)
									fuel[index] += emission_map[index] * sfs->fuel_amount;

								CLAMP(density[index], 0.0f, 1.0f);
								CLAMP(fuel[index], 0.0f, 1.0f);
							}
							/* initial velocity */
							if(sfs->flags & MOD_SMOKE_FLOW_INITVELOCITY) {
								velocity_x[index] = ADD_IF_LOWER(velocity_x[index], velocity_map[index*3]);
								velocity_y[index] = ADD_IF_LOWER(velocity_y[index], velocity_map[index*3+1]);
								velocity_z[index] = ADD_IF_LOWER(velocity_z[index], velocity_map[index*3+2]);
							}
						}

						/* loop through high res blocks if high res enabled */
						if (bigdensity) {
							// neighbor cell emission densities (for high resolution smoke smooth interpolation)
							float c000, c001, c010, c011,  c100, c101, c110, c111;

							smoke_turbulence_get_res(sds->wt, bigres);
							block_size = sds->amplify + 1;	// high res block size

							c000 = (x>0 && y>0 && z>0) ? emission_map[smoke_get_index(x-1, sds->res[0], y-1, sds->res[1], z-1)] : 0;
							c001 = (x>0 && y>0) ? emission_map[smoke_get_index(x-1, sds->res[0], y-1, sds->res[1], z)] : 0;
							c010 = (x>0 && z>0) ? emission_map[smoke_get_index(x-1, sds->res[0], y, sds->res[1], z-1)] : 0;
							c011 = (x>0) ? emission_map[smoke_get_index(x-1, sds->res[0], y, sds->res[1], z)] : 0;

							c100 = (y>0 && z>0) ? emission_map[smoke_get_index(x, sds->res[0], y-1, sds->res[1], z-1)] : 0;
							c101 = (y>0) ? emission_map[smoke_get_index(x, sds->res[0], y-1, sds->res[1], z)] : 0;
							c110 = (z>0) ? emission_map[smoke_get_index(x, sds->res[0], y, sds->res[1], z-1)] : 0;
							c111 = emission_map[smoke_get_index(x, sds->res[0], y, sds->res[1], z)]; // this cell

							for(ii = 0; ii < block_size; ii++)
								for(jj = 0; jj < block_size; jj++)
									for(kk = 0; kk < block_size; kk++)													
									{

										float fx,fy,fz, interpolated_value;
										int shift_x, shift_y, shift_z;


										/*
										* Do volume interpolation if emitter smoothing
										* is enabled
										*/
										if (high_emission_smoothing)
										{
											// convert block position to relative
											// for interpolation smoothing
											fx = (float)ii/block_size + 0.5f/block_size;
											fy = (float)jj/block_size + 0.5f/block_size;
											fz = (float)kk/block_size + 0.5f/block_size;

											// calculate trilinear interpolation
											interpolated_value = c000 * (1-fx) * (1-fy) * (1-fz) +
												c100 * fx * (1-fy) * (1-fz) +
												c010 * (1-fx) * fy * (1-fz) +
												c001 * (1-fx) * (1-fy) * fz +
												c101 * fx * (1-fy) * fz +
												c011 * (1-fx) * fy * fz +
												c110 * fx * fy * (1-fz) +
												c111 * fx * fy * fz;


											// add some contrast / sharpness
											// depending on hi-res block size
											interpolated_value = (interpolated_value-0.4f)*(block_size/2) + 0.4f;
											CLAMP(interpolated_value, 0.0f, 1.0f);

											// shift smoke block index
											// (because pixel center is actually
											// in halfway of the low res block)
											shift_x = (x < 1) ? 0 : block_size/2;
											shift_y = (y < 1) ? 0 : block_size/2;
											shift_z = (z < 1) ? 0 : block_size/2;
										}
										else 
										{
											// without interpolation use same low resolution
											// block value for all hi-res blocks
											interpolated_value = c111;
											shift_x = 0;
											shift_y = 0;
											shift_z = 0;
										}

										// get shifted index for current high resolution block
										index_big = smoke_get_index(block_size * x + ii - shift_x, bigres[0], block_size * y + jj - shift_y, bigres[1], block_size * z + kk - shift_z);														

										if(sfs->type == MOD_SMOKE_FLOW_TYPE_OUTFLOW) { // outflow
											if (interpolated_value) {
												bigdensity[index_big] = 0.f;
												bigfuel[index_big] = 0.f;
											}
										}
										else { // inflow
											if (absolute_flow) 
											{
												if (sfs->type != MOD_SMOKE_FLOW_TYPE_FIRE) {
													if (interpolated_value * sfs->density > bigdensity[index_big])
														bigdensity[index_big] = interpolated_value * sfs->density;
												}
												if (sfs->type != MOD_SMOKE_FLOW_TYPE_SMOKE) {
													if (interpolated_value * sfs->fuel_amount > bigfuel[index_big])
														bigfuel[index_big] = interpolated_value * sfs->fuel_amount;
												}
											}
											else 
											{
												if (sfs->type != MOD_SMOKE_FLOW_TYPE_FIRE)
													bigdensity[index_big] += interpolated_value * sfs->density;
												if (sfs->type != MOD_SMOKE_FLOW_TYPE_SMOKE)
													bigfuel[index_big] += interpolated_value * sfs->fuel_amount;

												CLAMP(bigdensity[index_big], 0.0f, 1.0f);
												CLAMP(bigfuel[index_big], 0.0f, 1.0f);
											}
										}
									} // hires loop
						}  // bigdensity

					} // low res loop

			// free emission maps
			if (emission_map) 
				MEM_freeN(emission_map);
			if (velocity_map)
				MEM_freeN(velocity_map);

		} // end emission
	}

	if(flowobjs)
		MEM_freeN(flowobjs);
}

static void update_effectors(Scene *scene, Object *ob, SmokeDomainSettings *sds, float UNUSED(dt))
{
	ListBase *effectors = pdInitEffectors(scene, ob, NULL, sds->effector_weights);

	if(effectors)
	{
		float *density = smoke_get_density(sds->fluid);
		float *force_x = smoke_get_force_x(sds->fluid);
		float *force_y = smoke_get_force_y(sds->fluid);
		float *force_z = smoke_get_force_z(sds->fluid);
		float *velocity_x = smoke_get_velocity_x(sds->fluid);
		float *velocity_y = smoke_get_velocity_y(sds->fluid);
		float *velocity_z = smoke_get_velocity_z(sds->fluid);
		unsigned char *obstacle = smoke_get_obstacle(sds->fluid);
		int x, y, z;

		// precalculate wind forces
		for(x = 0; x < sds->res[0]; x++)
			for(y = 0; y < sds->res[1]; y++)
				for(z = 0; z < sds->res[2]; z++)
		{	
			EffectedPoint epoint;
			float voxelCenter[3] = {0,0,0}, vel[3] = {0,0,0}, retvel[3] = {0,0,0};
			unsigned int index = smoke_get_index(x, sds->res[0], y, sds->res[1], z);

			if((density[index] < FLT_EPSILON) || obstacle[index])					
				continue;	

			vel[0] = velocity_x[index];
			vel[1] = velocity_y[index];
			vel[2] = velocity_z[index];

			voxelCenter[0] = sds->p0[0] + sds->dx * sds->scale * x + sds->dx * sds->scale * 0.5;
			voxelCenter[1] = sds->p0[1] + sds->dx * sds->scale * y + sds->dx * sds->scale * 0.5;
			voxelCenter[2] = sds->p0[2] + sds->dx * sds->scale * z + sds->dx * sds->scale * 0.5;

			pd_point_from_loc(scene, voxelCenter, vel, index, &epoint);
			pdDoEffectors(effectors, NULL, sds->effector_weights, &epoint, retvel, NULL);

			// TODO dg - do in force!
			force_x[index] = MIN2(MAX2(-1.0, retvel[0] * 0.2), 1.0); 
			force_y[index] = MIN2(MAX2(-1.0, retvel[1] * 0.2), 1.0); 
			force_z[index] = MIN2(MAX2(-1.0, retvel[2] * 0.2), 1.0);
		}
	}

	pdEndEffectors(&effectors);
}

static void step(Scene *scene, Object *ob, SmokeModifierData *smd, float fps)
{
	/* stability values copied from wturbulence.cpp */
	const int maxSubSteps = 25;
	float maxVel;
	// maxVel should be 1.5 (1.5 cell max movement) * dx (cell size)

	float dt = DT_DEFAULT;
	float maxVelMag = 0.0f;
	int totalSubsteps;
	int substep = 0;
	float dtSubdiv;

	SmokeDomainSettings *sds = smd->domain;

	/* get max velocity and lower the dt value if it is too high */
	size_t size= sds->res[0] * sds->res[1] * sds->res[2];

	float *velX = smoke_get_velocity_x(sds->fluid);
	float *velY = smoke_get_velocity_y(sds->fluid);
	float *velZ = smoke_get_velocity_z(sds->fluid);
	size_t i;

	/* adapt timestep for different framerates, dt = 0.1 is at 25fps */
	dt *= (25.0f / fps);

	// maximum timestep/"CFL" constraint: dt < 5.0 *dx / maxVel
	maxVel = (sds->dx * 5.0);

	for(i = 0; i < size; i++)
	{
		float vtemp = (velX[i]*velX[i]+velY[i]*velY[i]+velZ[i]*velZ[i]);
		if(vtemp > maxVelMag)
			maxVelMag = vtemp;
	}

	maxVelMag = sqrt(maxVelMag) * dt * sds->time_scale;
	totalSubsteps = (int)((maxVelMag / maxVel) + 1.0f); /* always round up */
	totalSubsteps = (totalSubsteps < 1) ? 1 : totalSubsteps;
	totalSubsteps = (totalSubsteps > maxSubSteps) ? maxSubSteps : totalSubsteps;

	/* Disable substeps for now, since it results in numerical instability */
	totalSubsteps = 1.0f; 

	dtSubdiv = (float)dt / (float)totalSubsteps;

	// printf("totalSubsteps: %d, maxVelMag: %f, dt: %f\n", totalSubsteps, maxVelMag, dt);

	for(substep = 0; substep < totalSubsteps; substep++)
	{
		// calc animated obstacle velocities
		update_obstacles(scene, ob, sds, dtSubdiv, substep, totalSubsteps);
		update_flowsfluids(scene, ob, sds, smd->time, dtSubdiv);
		update_effectors(scene, ob, sds, dtSubdiv); // DG TODO? problem --> uses forces instead of velocity, need to check how they need to be changed with variable dt
		
		smoke_step(sds->fluid, dtSubdiv);

		// move animated obstacle: Done in update_obstacles() */

		// where to delete old obstacles from array? Done in update_obstacles() */
	}
}

void smokeModifier_do(SmokeModifierData *smd, Scene *scene, Object *ob, DerivedMesh *dm)
{	
	if((smd->type & MOD_SMOKE_TYPE_FLOW))
	{
		if(scene->r.cfra >= smd->time)
			smokeModifier_init(smd, ob, scene, dm);

		if (smd->flow->dm) smd->flow->dm->release(smd->flow->dm);
		smd->flow->dm = CDDM_copy(dm);
		DM_ensure_tessface(smd->flow->dm);

		if(scene->r.cfra > smd->time)
		{
			// XXX TODO
			smd->time = scene->r.cfra;

			// rigid movement support
			/*
			copy_m4_m4(smd->flow->mat_old, smd->flow->mat);
			copy_m4_m4(smd->flow->mat, ob->obmat);
			*/
		}
		else if(scene->r.cfra < smd->time)
		{
			smd->time = scene->r.cfra;
			smokeModifier_reset(smd);
		}
	}
	else if(smd->type & MOD_SMOKE_TYPE_COLL)
	{
		/* Check if domain resolution changed */
		/* DG TODO: can this be solved more elegant using dependancy graph? */
		{
			SmokeCollSettings *scs = smd->coll;
			Base *base = scene->base.first;
			int changed = 0;
			float dx = FLT_MAX;
			float scale = 1.0f;
			int haveDomain = 0;

			for ( ; base; base = base->next) 
			{
				SmokeModifierData *smd2 = (SmokeModifierData *)modifiers_findByType(base->object, eModifierType_Smoke);

				if (smd2 && (smd2->type & MOD_SMOKE_TYPE_DOMAIN) && smd2->domain)
				{
					SmokeDomainSettings *sds = smd2->domain;

					if(sds->dx * sds->scale < dx)
					{
						dx = sds->dx;
						scale = sds->scale;
						changed = 1;
					}

					haveDomain = 1;
				}
			}

			if(!haveDomain)
				return;
			
			if(changed)
			{
				if(dx*scale != scs->dx)
				{
					scs->dx = dx*scale;
					smokeModifier_reset(smd);
				}
			}
		}

		if(scene->r.cfra >= smd->time)
			smokeModifier_init(smd, ob, scene, dm);

		if(scene->r.cfra > smd->time)
		{
			unsigned int i;
			SmokeCollSettings *scs = smd->coll;
			float *points_old = scs->points_old;
			float *points = scs->points;
			unsigned int numpoints = scs->numpoints; 

			// XXX TODO <-- DG: what is TODO here?
			smd->time = scene->r.cfra;

			// rigid movement support
			copy_m4_m4(scs->mat_old, scs->mat);
			copy_m4_m4(scs->mat, ob->obmat);

			if(scs->type != SM_COLL_ANIMATED) // if(not_animated)
			{
				// nothing to do, "mat" is already up to date
			}
			else
			{
				// XXX TODO: need to update positions + divs

				if(scs->numverts != dm->getNumVerts(dm))
				{
					// DG TODO: reset modifier?
					return;
				}

				for(i = 0; i < numpoints * 3; i++)
				{
					points_old[i] = points[i];
				}

				DM_ensure_tessface(dm);
				fill_scs_points_anim(ob, dm, scs);
			}
		}
		else if(scene->r.cfra < smd->time)
		{
			smd->time = scene->r.cfra;
			smokeModifier_reset(smd);
		}
	}
	else if(smd->type & MOD_SMOKE_TYPE_DOMAIN)
	{
		SmokeDomainSettings *sds = smd->domain;
		float light[3];	
		PointCache *cache = NULL;
		PTCacheID pid;
		int startframe, endframe, framenr;
		float timescale;

		framenr = scene->r.cfra;

		//printf("time: %d\n", scene->r.cfra);

		cache = sds->point_cache[0];
		BKE_ptcache_id_from_smoke(&pid, ob, smd);
		BKE_ptcache_id_time(&pid, scene, framenr, &startframe, &endframe, &timescale);

		if(!smd->domain->fluid || framenr == startframe)
		{
			BKE_ptcache_id_reset(scene, &pid, PTCACHE_RESET_OUTDATED);
			BKE_ptcache_validate(cache, framenr);
			cache->flag &= ~PTCACHE_REDO_NEEDED;
		}

		if(!smd->domain->fluid && (framenr != startframe) && (smd->domain->flags & MOD_SMOKE_FILE_LOAD)==0 && (cache->flag & PTCACHE_BAKED)==0)
			return;

		smd->domain->flags &= ~MOD_SMOKE_FILE_LOAD;

		CLAMP(framenr, startframe, endframe);

		/* If already viewing a pre/after frame, no need to reload */
		if ((smd->time == framenr) && (framenr != scene->r.cfra))
			return;

		// printf("startframe: %d, framenr: %d\n", startframe, framenr);

		if(smokeModifier_init(smd, ob, scene, dm)==0)
		{
			printf("bad smokeModifier_init\n");
			return;
		}

		/* try to read from cache */
		if(BKE_ptcache_read(&pid, (float)framenr) == PTCACHE_READ_EXACT) {
			BKE_ptcache_validate(cache, framenr);
			smd->time = framenr;
			return;
		}
		
		/* only calculate something when we advanced a single frame */
		if(framenr != (int)smd->time+1)
			return;

		/* don't simulate if viewing start frame, but scene frame is not real start frame */
		if (framenr != scene->r.cfra)
			return;

		tstart();

		smoke_calc_domain(scene, ob, smd);

		/* if on second frame, write cache for first frame */
		if((int)smd->time == startframe && (cache->flag & PTCACHE_OUTDATED || cache->last_exact==0)) {
			// create shadows straight after domain initialization so we get nice shadows for startframe, too
			if(get_lamp(scene, light))
				smoke_calc_transparency(sds->shadow, smoke_get_density(sds->fluid), sds->p0, sds->p1, sds->res, sds->dx, light, calc_voxel_transp, -7.0*sds->dx);

			if(sds->wt)
			{
				if(sds->flags & MOD_SMOKE_DISSOLVE)
					smoke_dissolve_wavelet(sds->wt, sds->diss_speed, sds->flags & MOD_SMOKE_DISSOLVE_LOG);
				smoke_turbulence_step(sds->wt, sds->fluid);
			}

			BKE_ptcache_write(&pid, startframe);
		}
		
		// set new time
		smd->time = scene->r.cfra;

		/* do simulation */

		// low res

		// simulate the actual smoke (c++ code in intern/smoke)
		// DG: interesting commenting this line + deactivating loading of noise files
		if(framenr!=startframe)
		{
			if(sds->flags & MOD_SMOKE_DISSOLVE)
				smoke_dissolve(sds->fluid, sds->diss_speed, sds->flags & MOD_SMOKE_DISSOLVE_LOG);
			
			step(scene, ob, smd, scene->r.frs_sec / scene->r.frs_sec_base);
		}

		// create shadows before writing cache so they get stored
		if(get_lamp(scene, light))
			smoke_calc_transparency(sds->shadow, smoke_get_density(sds->fluid), sds->p0, sds->p1, sds->res, sds->dx, light, calc_voxel_transp, -7.0*sds->dx);

		if(sds->wt)
		{
			if(sds->flags & MOD_SMOKE_DISSOLVE)
				smoke_dissolve_wavelet(sds->wt, sds->diss_speed, sds->flags & MOD_SMOKE_DISSOLVE_LOG);
			smoke_turbulence_step(sds->wt, sds->fluid);
		}
	
		BKE_ptcache_validate(cache, framenr);
		if(framenr != startframe)
			BKE_ptcache_write(&pid, framenr);

		tend();
		// printf ( "Frame: %d, Time: %f\n\n", (int)smd->time, ( float ) tval() );
	}
}

static float calc_voxel_transp(float *result, float *input, int res[3], int *pixel, float *tRay, float correct)
{
	const size_t index = smoke_get_index(pixel[0], res[0], pixel[1], res[1], pixel[2]);

	// T_ray *= T_vox
	*tRay *= exp(input[index]*correct);
	
	if(result[index] < 0.0f)	
	{
// #pragma omp critical		
		result[index] = *tRay;	
	}	

	return *tRay;
}

long long smoke_get_mem_req(int xres, int yres, int zres, int amplify)
{
	int totalCells = xres * yres * zres;
	int amplifiedCells = totalCells * amplify * amplify * amplify;

	// print out memory requirements
	long long int coarseSize = sizeof(float) * totalCells * 22 +
	sizeof(unsigned char) * totalCells;

	long long int fineSize = sizeof(float) * amplifiedCells * 7 + // big grids
	sizeof(float) * totalCells * 8 +     // small grids
	sizeof(float) * 128 * 128 * 128;     // noise tile

	long long int totalMB = (coarseSize + fineSize) / (1024 * 1024);

	return totalMB;
}

static void bresenham_linie_3D(int x1, int y1, int z1, int x2, int y2, int z2, float *tRay, bresenham_callback cb, float *result, float *input, int res[3], float correct)
{
	int dx, dy, dz, i, l, m, n, x_inc, y_inc, z_inc, err_1, err_2, dx2, dy2, dz2;
	int pixel[3];

	pixel[0] = x1;
	pixel[1] = y1;
	pixel[2] = z1;

	dx = x2 - x1;
	dy = y2 - y1;
	dz = z2 - z1;

	x_inc = (dx < 0) ? -1 : 1;
	l = abs(dx);
	y_inc = (dy < 0) ? -1 : 1;
	m = abs(dy);
	z_inc = (dz < 0) ? -1 : 1;
	n = abs(dz);
	dx2 = l << 1;
	dy2 = m << 1;
	dz2 = n << 1;

	if ((l >= m) && (l >= n)) {
		err_1 = dy2 - l;
		err_2 = dz2 - l;
		for (i = 0; i < l; i++) {
			if(cb(result, input, res, pixel, tRay, correct) <= FLT_EPSILON)
				break;
			if (err_1 > 0) {
				pixel[1] += y_inc;
				err_1 -= dx2;
			}
			if (err_2 > 0) {
				pixel[2] += z_inc;
				err_2 -= dx2;
			}
			err_1 += dy2;
			err_2 += dz2;
			pixel[0] += x_inc;
		}
	} 
	else if ((m >= l) && (m >= n)) {
		err_1 = dx2 - m;
		err_2 = dz2 - m;
		for (i = 0; i < m; i++) {
			if(cb(result, input, res, pixel, tRay, correct) <= FLT_EPSILON)
				break;
			if (err_1 > 0) {
				pixel[0] += x_inc;
				err_1 -= dy2;
			}
			if (err_2 > 0) {
				pixel[2] += z_inc;
				err_2 -= dy2;
			}
			err_1 += dx2;
			err_2 += dz2;
			pixel[1] += y_inc;
		}
	} 
	else {
		err_1 = dy2 - n;
		err_2 = dx2 - n;
		for (i = 0; i < n; i++) {
			if(cb(result, input, res, pixel, tRay, correct) <= FLT_EPSILON)
				break;
			if (err_1 > 0) {
				pixel[1] += y_inc;
				err_1 -= dz2;
			}
			if (err_2 > 0) {
				pixel[0] += x_inc;
				err_2 -= dz2;
			}
			err_1 += dy2;
			err_2 += dx2;
			pixel[2] += z_inc;
		}
	}
	cb(result, input, res, pixel, tRay, correct);
}

static void get_cell(float *p0, int res[3], float dx, float *pos, int *cell, int correct)
{
	float tmp[3];

	sub_v3_v3v3(tmp, pos, p0);
	mul_v3_fl(tmp, 1.0 / dx);

	if (correct) {
		cell[0] = MIN2(res[0] - 1, MAX2(0, (int)floor(tmp[0])));
		cell[1] = MIN2(res[1] - 1, MAX2(0, (int)floor(tmp[1])));
		cell[2] = MIN2(res[2] - 1, MAX2(0, (int)floor(tmp[2])));
	}
	else {
		cell[0] = (int)floor(tmp[0]);
		cell[1] = (int)floor(tmp[1]);
		cell[2] = (int)floor(tmp[2]);
	}
}

static void smoke_calc_transparency(float *result, float *input, float *p0, float *p1, int res[3], float dx, float *light, bresenham_callback cb, float correct)
{
	float bv[6];
	int a, z, slabsize=res[0]*res[1], size= res[0]*res[1]*res[2];

	for(a=0; a<size; a++)
		result[a]= -1.0f;

	bv[0] = p0[0];
	bv[1] = p1[0];
	// y
	bv[2] = p0[1];
	bv[3] = p1[1];
	// z
	bv[4] = p0[2];
	bv[5] = p1[2];

// #pragma omp parallel for schedule(static,1)
	for(z = 0; z < res[2]; z++)
	{
		size_t index = z*slabsize;
		int x,y;

		for(y = 0; y < res[1]; y++)
			for(x = 0; x < res[0]; x++, index++)
			{
				float voxelCenter[3];
				float pos[3];
				int cell[3];
				float tRay = 1.0;

				if(result[index] >= 0.0f)					
					continue;								
				voxelCenter[0] = p0[0] + dx *  x + dx * 0.5;
				voxelCenter[1] = p0[1] + dx *  y + dx * 0.5;
				voxelCenter[2] = p0[2] + dx *  z + dx * 0.5;

				// get starting position (in voxel coords)
				if(BLI_bvhtree_bb_raycast(bv, light, voxelCenter, pos) > FLT_EPSILON)
				{
					// we're ouside
					get_cell(p0, res, dx, pos, cell, 1);
				}
				else {
					// we're inside
					get_cell(p0, res, dx, light, cell, 1);
				}

				bresenham_linie_3D(cell[0], cell[1], cell[2], x, y, z, &tRay, cb, result, input, res, correct);

				// convention -> from a RGBA float array, use G value for tRay
// #pragma omp critical
				result[index] = tRay;			
			}
	}
}

#endif /* WITH_SMOKE */

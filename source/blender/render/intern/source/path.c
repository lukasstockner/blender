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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "DNA_material_types.h"

#include "BKE_utildefines.h"

#include "BLI_math.h"
#include "BLI_rand.h"

#include "RE_raytrace.h"

#include "cache.h"
#include "camera.h"
#include "database.h"
#include "environment.h"
#include "material.h"
#include "object_mesh.h"
#include "part.h"
#include "raytrace.h"
#include "render_types.h"
#include "rendercore.h"
#include "sampler.h"
#include "shading.h"

typedef struct Hit {
	ObjectInstanceRen *obi;
	VlakRen *vlr;
	float co[3];
	float vec[3];
	float n[3];
	float u, v;
	float maxdist, dist;
} Hit;

static void shadeinput_from_hit(Render *re, RenderLayer *rl, ShadeInput *shi, Hit *hit, float view[3])
{
	memset(shi, 0, sizeof(ShadeInput));

	copy_v3_v3(shi->geometry.view, view);
	copy_v3_v3(shi->geometry.co, hit->co);

	shi->material.mat_override= (rl)? rl->mat_override: NULL;
	shi->material.mat= hit->vlr->mat;
	shade_input_set_triangle_i(re, shi, hit->obi, hit->vlr, 0, 1, 2);
	shade_input_init_material(re, shi);

	shi->geometry.u= hit->u;
	shi->geometry.v= hit->v;
	shi->geometry.dx_u= shi->geometry.dx_v= shi->geometry.dy_u= shi->geometry.dy_v= 0.0f;

	/*shade_input_set_normals(shi);*/
	shade_input_set_shade_texco(re, shi);
	copy_v3_v3(shi->geometry.vn, hit->n);

	if(dot_v3v3(shi->geometry.facenor, shi->geometry.view) > 0.0f)
		shade_input_flip_normals(shi);
}

static int isec_trace_ray(Render *re, Hit *from, Hit *to, int depth)
{
	Isect isec;

	memset(&isec, 0, sizeof(isec));

	copy_v3_v3(isec.start, from->co);
	mul_v3_v3fl(isec.vec, from->vec, from->maxdist);
	isec.labda= 1.0f;

	isec.mode= RE_RAY_MIRROR;
	isec.skip = RE_SKIP_VLR_NEIGHBOUR|RE_SKIP_VLR_RENDER_CHECK;

	isec.orig.ob = from->obi;
	isec.orig.face = from->vlr;

	isec.lay= -1;

	if(!RE_rayobject_raycast(re->db.raytree, &isec))
		return 0;
	
	to->obi= isec.hit.ob;
	to->vlr= isec.hit.face;
	to->u= isec.u;
	to->v= isec.v;
	to->maxdist= 1e5f;
	to->dist= from->maxdist*isec.labda;

	madd_v3_v3v3fl(to->co, isec.start, isec.vec, isec.labda);
	copy_v3_v3(to->n, to->vlr->n);
	mul_v3_fl(to->n, -1.0f);

	return 1;
}

static float integrate_path(Render *re, RenderLayer *rl, int thread, Hit *from, float sample[3], int depth, float hemi[2])
{
	Hit to;
	ShadeInput shi;
	float bsdf[3], nsample[3], view[3], probability, basis[3][3], r[2];

	if(isec_trace_ray(re, from, &to, depth)) {
		copy_v3_v3(view, from->vec);
		negate_v3(view);

		shadeinput_from_hit(re, rl, &shi, &to, view);

		mat_shading_begin(re, &shi, &shi.material, 1);

		/* generate new ray */
		if(!hemi) {
			r[0]= BLI_thread_frand(thread);
			r[1]= BLI_thread_frand(thread);
		}
		else
			copy_v2_v2(r, hemi);

		sample_project_hemi_cosine_weighted(to.vec, r);

		copy_v3_v3(basis[2], shi.geometry.vn);
		ortho_basis_v3v3_v3(basis[0], basis[1], shi.geometry.vn);
		mul_m3_v3(basis, to.vec);

		/* sample emit & bsdf */
		mat_emit(sample, &shi.material, &shi.geometry, shi.shading.thread);
		mat_bsdf_f(bsdf, &shi.material, &shi.geometry, shi.shading.thread, to.vec, BSDF_DIFFUSE);

		mat_shading_end(re, &shi.material);

		if(!is_zero_v3(bsdf)) {
			/* russian roulette */
			//probability= (bsdf[0] + bsdf[1] + bsdf[2])/3.0f;
			//probability= CLAMPIS(probability, 0.0f, 0.75f);
			probability= (depth > 0)? 0.75f: 1.0f;

			if(probability == 1.0f || BLI_thread_frand(thread) < probability) {
				/* keep going */
				integrate_path(re, rl, thread, &to, nsample, depth+1, NULL);

				/* bsdf & correction for russian roulette */
				nsample[0]= bsdf[0]*nsample[0];
				nsample[1]= bsdf[1]*nsample[1];
				nsample[2]= bsdf[2]*nsample[2];
				mul_v3_fl(nsample, (float)M_PI/(probability*dot_v3v3(to.vec, shi.geometry.vn)));

				/* accumulate */
				add_v3_v3v3(sample, sample, nsample);
			}
			else
				; /* path terminated */
		}

		return to.dist;
	}
	else {
		/* no hit, sample environment */
		environment_shade(re, sample, NULL, from->vec, NULL, thread);
		return 1e5f;
	}
}

static void integrate_pixel(Render *re, RenderLayer *rl, int thread, int x, int y, float color[3])
{
	Hit from;
	QMCSampler *qsa, *qsa2;
	float accum[3], sample[3], raster[2], hemi[2];
	int a, totsample = re->params.r.path_samples;

	zero_v3(accum);

	qsa = sampler_acquire(re, thread, SAMP_TYPE_HAMMERSLEY, totsample);
	qsa2 = sampler_acquire(re, thread, SAMP_TYPE_HAMMERSLEY, totsample);

	for(a=0; a<totsample; a++) {
		/* random raster location */
		sampler_get_float_2d(raster, qsa, a);
		raster[0] += x;
		raster[1] += y;

		/* raster to ray */
		memset(&from, 0, sizeof(from));
		from.maxdist= 1e5f;
		camera_raster_to_ray(&re->cam, from.co, from.vec, raster[0], raster[1]);

		/* integrate path */
		sampler_get_float_2d(hemi, qsa2, a);
		integrate_path(re, rl, thread, &from, sample, 0, hemi);

		add_v3_v3v3(accum, accum, sample);
	}

	mul_v3_fl(accum, 1.0f/re->params.r.path_samples);
	copy_v3_v3(color, accum);

	sampler_release(re, qsa);
	sampler_release(re, qsa2);
}

void render_path_trace_part(Render *re, RenderPart *pa)
{
	RenderResult *rr= pa->result;
	RenderLayer *rl= rr->layers.first;
	float sample[3], *fp;
	int x, y, seed=0, offs=0, crop= 0;

	if(re->cb.test_break(re->cb.tbh)) return; 
	if(!re->db.raytree) return;

	/* filtered render, for now we assume only 1 filter size */
	if(pa->crop) {
		crop= 1;
		offs= pa->rectx + 1;
	}
	
	/* initialize scanline updates for main thread,
	   scanline updates have to be 2 lines behind for crop */
	rr->renrect.ymin= 0;
	rr->renrect.ymax= -2*crop;
	rr->renlay= rl;

	for(y=pa->disprect.ymin+crop; y<pa->disprect.ymax-crop; y++, rr->renrect.ymax++) {
		for(x=pa->disprect.xmin+crop; x<pa->disprect.xmax-crop; x++) {
			BLI_thread_srandom(pa->thread, seed++);

			integrate_pixel(re, rl, pa->thread, x, y, sample);

			fp= rl->rectf + 4*offs;
			copy_v3_v3(fp, sample);
			fp[3]= 1.0f;

			offs++;
		}

		offs+= 2*crop;

		if(y&1 && re->cb.test_break(re->cb.tbh))
			break; 
	}
	
	/* display active layer */
	rr->renrect.ymin=rr->renrect.ymax= 0;
	rr->renlay= render_get_active_layer(re, rr);
}


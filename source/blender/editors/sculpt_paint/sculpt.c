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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2006 by Nicholas Bishop
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Jason Wilkins, Tom Musgrove.
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 * Implements the Sculpt Mode tools
 *
 */

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_dynstr.h"
#include "BLI_ghash.h"
#include "BLI_pbvh.h"
#include "BLI_threads.h"
#include "BLI_editVert.h"
#include "BLI_rand.h"

#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_brush_types.h"

#include "BKE_brush.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_dmgrid.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_key.h"
#include "BKE_library.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_multires.h"
#include "BKE_paint.h"
#include "BKE_report.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "WM_api.h"
#include "WM_types.h"
#include "ED_screen.h"
#include "ED_sculpt.h"
#include "ED_view3d.h"
#include "paint_intern.h"
#include "sculpt_intern.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "GPU_buffers.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifdef _OPENMP
#include <omp.h>
#endif

/* ==== FORWARD DEFINITIONS =====
 *
 */

static int sculpt_stroke_get_location(bContext *C, struct PaintStroke *stroke, float out[3], float mouse[2]);

/* Checks whether full update mode (slower) needs to be used to work with modifiers */
int sculpt_modifiers_active(Scene *scene, Object *ob)
{
	ModifierData *md;
	MultiresModifierData *mmd= ED_paint_multires_active(scene, ob);

	/* check if there are any modifiers after what we are sculpting,
	   for a multires modifier with a deform modifier in front, we
	   do no need to recalculate the modifier stack. note that this
	   needs to be in sync with ccgDM_use_grid_pbvh! */
	if(mmd)
		md= mmd->modifier.next;
	else
		md= modifiers_getVirtualModifierList(ob);
	
	for(; md; md= md->next) {
		if(modifier_isEnabled(scene, md, eModifierMode_Realtime)) {

			/* exception for shape keys because we can edit those */
			if(md->type == eModifierType_ShapeKey)
				continue;

			/*exception for multires on level zero, it's
			  not caught by the earlier multires check */
			else if(md->type == eModifierType_Multires &&
			   ((MultiresModifierData*)md)->sculptlvl == 0)
				continue;

			else
				return 1;
		}
	}

	return 0;
}

/* ===== STRUCTS =====
 *
 */

typedef enum StrokeFlags {
	CLIP_X = 1,
	CLIP_Y = 2,
	CLIP_Z = 4
} StrokeFlags;

/* Cache stroke properties. Used because
   RNA property lookup isn't particularly fast.

   For descriptions of these settings, check the operator properties.
*/
typedef struct StrokeCache {
	/* Invariants */
	float scale[3];
	int flag;
	float clip_tolerance[3];

	/* Variants */
	float pen_flip;
	float invert;
	float bstrength;

	/* The rest is temporary storage that isn't saved as a property */

	/* Clean this up! */
	PaintStroke *stroke;
	ViewContext *vc;
	Brush *brush;

	float (*face_norms)[3]; /* Copy of the mesh faces' normals */
	float grab_delta[3], grab_delta_symmetry[3];
	float old_grab_location[3], orig_grab_location[3];

	int mirror_symmetry_pass; /* the symmetry pass we are currently on between 0 and 7*/
	float true_view_normal[3];
	float view_normal[3];
	float last_area_normal[3];
	float last_center[3];
	int radial_symmetry_pass;
	float symm_rot_mat[4][4];
	int original;

	float vertex_rotation;

	char saved_active_brush_name[24];
	int alt_smooth;

	float plane_trim_squared;
} StrokeCache;

static float frontface(Brush *brush, float sculpt_normal[3], short no[3], float fno[3])
{
	if (brush->flag & BRUSH_FRONTFACE) {
		float dot;

		if (no) {
			float tmp[3];

			normal_short_to_float_v3(tmp, no);
			dot= dot_v3v3(tmp, sculpt_normal);
		}
		else {
			dot= dot_v3v3(fno, sculpt_normal);
		}

		return dot > 0 ? dot : 0;
	}
	else {
		return 1;
	}
}

/* ===== Sculpting =====
 *
 */
  

static float overlapped_curve(Brush* br, float x)
{
	int i;
	const int n = 100 / br->spacing;
	const float h = br->spacing / 50.0f;
	const float x0 = x-1;

	float sum;

	sum = 0;
	for (i= 0; i < n; i++) {
		float xx;

		xx = fabs(x0 + i*h);

		if (xx < 1.0f)
			sum += brush_curve_strength(br, xx, 1);
	}

	return sum;
}

static float integrate_overlap(Brush* br)
{
	int i;
	int m= 10;
	float g = 1.0f/m;
	float overlap;
	float max;

	overlap= 0;
	max= 0;
	for(i= 0; i < m; i++) {
		overlap = overlapped_curve(br, i*g);

		if (overlap > max)
			max = overlap;
	}

	return max;
}

/* Return modified brush strength. Includes the direction of the brush, positive
   values pull vertices, negative values push. Uses tablet pressure and a
   special multiplier found experimentally to scale the strength factor. */
static float brush_strength(Sculpt *sd, StrokeCache *cache, float feather)
{
	Brush *brush = paint_brush(&sd->paint);

	/* Primary strength input; square it to make lower values more sensitive */
	const float root_alpha = brush_alpha(brush);
	float alpha        = root_alpha*root_alpha;
	float dir          = brush->flag & BRUSH_DIR_IN ? -1 : 1;
	float pressure     = brush_use_alpha_pressure(brush) ? paint_stroke_pressure(cache->stroke) : 1;
	float pen_flip     = cache->pen_flip ? -1 : 1;
	float invert       = cache->invert ? -1 : 1;
	float accum        = integrate_overlap(brush);
	float overlap      = (brush->flag & BRUSH_SPACE_ATTEN && brush->flag & BRUSH_SPACE && !(brush->flag & BRUSH_ANCHORED)) && (brush->spacing < 100) ? 1.0f/accum : 1; // spacing is integer percentage of radius, divide by 50 to get normalized diameter
	float flip         = dir * invert * pen_flip;

	switch(brush->sculpt_tool){
		case SCULPT_TOOL_CLAY:
		case SCULPT_TOOL_CLAY_TUBES:
		case SCULPT_TOOL_DRAW:
		case SCULPT_TOOL_LAYER:
			return alpha * flip * pressure * overlap * feather;

		case SCULPT_TOOL_CREASE:
		case SCULPT_TOOL_BLOB:
			return alpha * flip * pressure * overlap * feather;

		case SCULPT_TOOL_INFLATE:
			if (flip > 0) {
				return 0.250f * alpha * flip * pressure * overlap * feather;
			}
			else {
				return 0.125f * alpha * flip * pressure * overlap * feather;
			}

		case SCULPT_TOOL_FILL:
		case SCULPT_TOOL_SCRAPE:
		case SCULPT_TOOL_FLATTEN:
			if (flip > 0) {
				overlap = (1+overlap) / 2;
				return alpha * flip * pressure * overlap * feather;
			}
			else {
				/* reduce strength for DEEPEN, PEAKS, and CONTRAST */
				return 0.5f * alpha * flip * pressure * overlap * feather; 
			}

		case SCULPT_TOOL_SMOOTH:
			return alpha * pressure * feather;

		case SCULPT_TOOL_PINCH:
			if (flip > 0) {
				return alpha * flip * pressure * overlap * feather;
			}
			else {
				return 0.25f * alpha * flip * pressure * overlap * feather;
			}

		case SCULPT_TOOL_NUDGE:
			overlap = (1+overlap) / 2;
			return alpha * pressure * overlap * feather;

		case SCULPT_TOOL_THUMB:
			return alpha*pressure*feather;

		case SCULPT_TOOL_SNAKE_HOOK:
			return feather;

		case SCULPT_TOOL_GRAB:
		case SCULPT_TOOL_ROTATE:
			return feather;

		default:
			return 0;
	}
}

#if 0

/* Get a pixel from the texcache at (px, py) */
static unsigned char get_texcache_pixel(const SculptSession *ss, int px, int py)
{
	unsigned *p;
	p = ss->texcache + py * ss->texcache_side + px;
	return ((unsigned char*)(p))[0];
}

static float get_texcache_pixel_bilinear(const SculptSession *ss, float u, float v)
{
	unsigned x, y, x2, y2;
	const int tc_max = ss->texcache_side - 1;
	float urat, vrat, uopp;

	if(u < 0) u = 0;
	else if(u >= ss->texcache_side) u = tc_max;
	if(v < 0) v = 0;
	else if(v >= ss->texcache_side) v = tc_max;

	x = floor(u);
	y = floor(v);
	x2 = x + 1;
	y2 = y + 1;

	if(x2 > ss->texcache_side) x2 = tc_max;
	if(y2 > ss->texcache_side) y2 = tc_max;
	
	urat = u - x;
	vrat = v - y;
	uopp = 1 - urat;
		
	return ((get_texcache_pixel(ss, x, y) * uopp +
		 get_texcache_pixel(ss, x2, y) * urat) * (1 - vrat) + 
		(get_texcache_pixel(ss, x, y2) * uopp +
		 get_texcache_pixel(ss, x2, y2) * urat) * vrat) / 255.0;
}

#endif

static float tex_strength(SculptSession *ss, Brush *br, float *co,
			  float mask, float dist)
{
	return paint_stroke_combined_strength(ss->cache->stroke, dist, co, mask);
}

/* Handles clipping against a mirror modifier and SCULPT_LOCK axis flags */
static void sculpt_clip(Sculpt *sd, SculptSession *ss, float *co, const float val[3])
{
	int i;

	for(i=0; i<3; ++i) {
		if(sd->paint.flags & (PAINT_LOCK_X << i))
			continue;

		if((ss->cache->flag & (CLIP_X << i)) && (fabs(co[i]) <= ss->cache->clip_tolerance[i]))
			co[i]= 0.0f;
		else
			co[i]= val[i];
	}
}

static void add_norm_if(float view_vec[3], float out[3], float out_flip[3], float fno[3])
{
	if((dot_v3v3(view_vec, fno)) > 0) {
		add_v3_v3(out, fno);
	} else {
		add_v3_v3(out_flip, fno); /* out_flip is used when out is {0,0,0} */
	}
}

static void calc_area_normal(Sculpt *sd, Object *ob, float an[3], PBVHNode **nodes, int totnode)
{
	SculptSession *ss = ob->paint->sculpt;
	int n;

	float out_flip[3] = {0.0f, 0.0f, 0.0f};

	zero_v3(an);

	#pragma omp parallel for schedule(guided) if (sd->paint.flags & PAINT_USE_OPENMP)
	for(n=0; n<totnode; n++) {
		PBVHVertexIter vd;
		PaintStrokeTest test;
		PBVHUndoNode *unode;
		float private_an[3] = {0.0f, 0.0f, 0.0f};
		float private_out_flip[3] = {0.0f, 0.0f, 0.0f};

		unode = pbvh_undo_push_node(nodes[n], PBVH_UNDO_CO_NO, ob, NULL);
		paint_stroke_test_init(&test, ss->cache->stroke);

		if(ss->cache->original) {
			BLI_pbvh_vertex_iter_begin(ob->paint->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
				if(paint_stroke_test_fast(&test, pbvh_undo_node_co(unode)[vd.i])) {
					float fno[3];

					normal_short_to_float_v3(fno, pbvh_undo_node_no(unode)[vd.i]);
					add_norm_if(ss->cache->view_normal, private_an, private_out_flip, fno);
				}
			}
			BLI_pbvh_vertex_iter_end;
		}
		else {
			BLI_pbvh_vertex_iter_begin(ob->paint->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
				if(paint_stroke_test_fast(&test, vd.co)) {
					if(vd.no) {
						float fno[3];

						normal_short_to_float_v3(fno, vd.no);
						add_norm_if(ss->cache->view_normal, private_an, private_out_flip, fno);
					}
					else {
						add_norm_if(ss->cache->view_normal, private_an, private_out_flip, vd.fno);
					}
				}
			}
			BLI_pbvh_vertex_iter_end;
		}

		#pragma omp critical
		{
			add_v3_v3(an, private_an);
			add_v3_v3(out_flip, private_out_flip);
		}
	}

	if (is_zero_v3(an))
		copy_v3_v3(an, out_flip);

	normalize_v3(an);
}

/* This initializes the faces to be moved for this sculpt for draw/layer/flatten; then it
 finds average normal for all active vertices - note that this is called once for each mirroring direction */
static void calc_sculpt_normal(Sculpt *sd, Object *ob, float an[3], PBVHNode **nodes, int totnode)
{
	SculptSession *ss = ob->paint->sculpt;
	Brush *brush = paint_brush(&sd->paint);

	if (ss->cache->mirror_symmetry_pass == 0 &&
	    ss->cache->radial_symmetry_pass == 0 &&
	    (paint_stroke_first_dab(ss->cache->stroke) || !(brush->flag & BRUSH_ORIGINAL_NORMAL)))
	{
		switch (brush->sculpt_plane) {
			case SCULPT_DISP_DIR_VIEW:
				viewvector(ss->cache->vc->rv3d, ss->cache->vc->rv3d->twmat[3], an);
				break;

			case SCULPT_DISP_DIR_X:
				an[1] = 0.0;
				an[2] = 0.0;
				an[0] = 1.0;
				break;

			case SCULPT_DISP_DIR_Y:
				an[0] = 0.0;
				an[2] = 0.0;
				an[1] = 1.0;
				break;

			case SCULPT_DISP_DIR_Z:
				an[0] = 0.0;
				an[1] = 0.0;
				an[2] = 1.0;
				break;

			case SCULPT_DISP_DIR_AREA:
				calc_area_normal(sd, ob, an, nodes, totnode);

			default:
				break;
		}

		copy_v3_v3(ss->cache->last_area_normal, an);
	}
	else {
		copy_v3_v3(an, ss->cache->last_area_normal);
		paint_flip_coord(an, an, ss->cache->mirror_symmetry_pass);
		mul_m4_v3(ss->cache->symm_rot_mat, an);
	}
}

/* For the smooth brush, uses the neighboring vertices around vert to calculate
   a smoothed location for vert. Skips corner vertices (used by only one
   polygon.) */
static void neighbor_average(Object *ob, float avg[3], const unsigned vert)
{
	SculptSession *ss = ob->paint->sculpt;
	int i, skip= -1, total=0;
	IndexNode *node= ss->fmap[vert].first;
	char ncount= BLI_countlist(&ss->fmap[vert]);
	MFace *f;

	avg[0] = avg[1] = avg[2] = 0;
		
	/* Don't modify corner vertices */
	if(ncount==1) {
		copy_v3_v3(avg, ss->mvert[vert].co);
		return;
	}

	while(node){
		f= &ss->mface[node->index];
		
		if(f->v4) {
			skip= (f->v1==vert?2:
				   f->v2==vert?3:
				   f->v3==vert?0:
				   f->v4==vert?1:-1);
		}

		for(i=0; i<(f->v4?4:3); ++i) {
			if(i != skip && (ncount!=2 || BLI_countlist(&ss->fmap[(&f->v1)[i]]) <= 2)) {
				add_v3_v3(avg, ss->mvert[(&f->v1)[i]].co);
				++total;
			}
		}

		node= node->next;
	}

	if(total>0)
		mul_v3_fl(avg, 1.0f / total);
	else
		copy_v3_v3(avg, ss->mvert[vert].co);
}

static void do_mesh_smooth_brush(Sculpt *sd, Object *ob, PBVHNode *node, float bstrength)
{
	SculptSession *ss = ob->paint->sculpt;
	Brush *brush = paint_brush(&sd->paint);
	PBVHVertexIter vd;
	PaintStrokeTest test;
	
	CLAMP(bstrength, 0.0f, 1.0f);

	paint_stroke_test_init(&test, ss->cache->stroke);

	BLI_pbvh_vertex_iter_begin(ob->paint->pbvh, node, vd, PBVH_ITER_UNIQUE) {
		if(paint_stroke_test(&test, vd.co)) {
			const float fade = bstrength*tex_strength(ss, brush, vd.co, vd.mask_combined, test.dist)*frontface(brush, ss->cache->view_normal, vd.no, vd.fno);
			float avg[3], val[3];

			neighbor_average(ob, avg, vd.vert_indices[vd.i]);
			sub_v3_v3v3(val, avg, vd.co);
			mul_v3_fl(val, fade);

			add_v3_v3(val, vd.co);

			sculpt_clip(sd, ss, vd.co, val);

			if(vd.mvert)
					vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
		}
	}
	BLI_pbvh_vertex_iter_end;
}

static void do_multires_smooth_brush(Sculpt *sd, Object *ob, PBVHNode *node, float bstrength)
{
	SculptSession *ss = ob->paint->sculpt;
	Brush *brush = paint_brush(&sd->paint);
	PaintStrokeTest test;
	DMGridData **griddata, *data;
	DMGridAdjacency *gridadj, *adj;
	GridKey *gridkey;
	float (*tmpgrid)[3], (*tmprow)[3];
	int v1, v2, v3, v4;
	int *grid_indices, totgrid, gridsize, i, x, y;

	paint_stroke_test_init(&test, ss->cache->stroke);

	CLAMP(bstrength, 0.0f, 1.0f);

	BLI_pbvh_node_get_grids(ob->paint->pbvh, node, &grid_indices, &totgrid,
				NULL, &gridsize, &griddata, &gridadj, &gridkey);

	#pragma omp critical
	{
		tmpgrid= MEM_mallocN(sizeof(float)*3*gridsize*gridsize, "tmpgrid");
		tmprow=  MEM_mallocN(sizeof(float)*3*gridsize, "tmprow");
	}

	for(i = 0; i < totgrid; ++i) {
		data = griddata[grid_indices[i]];
		adj = &gridadj[grid_indices[i]];

		memset(tmpgrid, 0, sizeof(float)*3*gridsize*gridsize);

		for (y= 0; y < gridsize-1; y++) {
			float tmp[3];

			v1 = y*gridsize;
			add_v3_v3v3(tmprow[0], GRIDELEM_CO_AT(data, v1, gridkey),
				    GRIDELEM_CO_AT(data, v1+gridsize, gridkey));

			for (x= 0; x < gridsize-1; x++) {
				v1 = x + y*gridsize;
				v2 = v1 + 1;
				v3 = v1 + gridsize;
				v4 = v3 + 1;

				add_v3_v3v3(tmprow[x+1], GRIDELEM_CO_AT(data, v2, gridkey), GRIDELEM_CO_AT(data, v4, gridkey));
				add_v3_v3v3(tmp, tmprow[x+1], tmprow[x]);

				add_v3_v3(tmpgrid[v1], tmp);
				add_v3_v3(tmpgrid[v2], tmp);
				add_v3_v3(tmpgrid[v3], tmp);
				add_v3_v3(tmpgrid[v4], tmp);
			}
		}

		/* blend with existing coordinates */
		for(y = 0; y < gridsize; ++y)  {
			for(x = 0; x < gridsize; ++x)  {
				float *co;
				float *fno;
				int index;

				if(x == 0 && adj->index[0] == -1)
					continue;

				if(x == gridsize - 1 && adj->index[2] == -1)
					continue;

				if(y == 0 && adj->index[3] == -1)
					continue;

				if(y == gridsize - 1 && adj->index[1] == -1)
					continue;

				index = x + y*gridsize;
				co=  GRIDELEM_CO_AT(data, index, gridkey);
				fno= GRIDELEM_NO_AT(data, index, gridkey);

				if(paint_stroke_test(&test, co)) {
					float mask = (gridkey->mask ?
						      *GRIDELEM_MASK_AT(data, x + y*gridsize, gridkey) : 0);
					const float fade = bstrength*tex_strength(ss, brush, co, mask, test.dist)*frontface(brush, ss->cache->view_normal, NULL, fno);
					float *avg, val[3];
					float n;

					avg = tmpgrid[x + y*gridsize];

					n = 1/16.0f;

					if(x == 0 || x == gridsize - 1)
						n *= 2;

					if(y == 0 || y == gridsize - 1)
						n *= 2;

					mul_v3_fl(avg, n);

					sub_v3_v3v3(val, avg, co);
					mul_v3_fl(val, fade);

					add_v3_v3(val, co);

					sculpt_clip(sd, ss, co, val);
				}
			}
		}
	}

	#pragma omp critical
	{
		MEM_freeN(tmpgrid);
		MEM_freeN(tmprow);
	}
}

static void smooth(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode, float bstrength)
{
	SculptSession *ss = ob->paint->sculpt;
	const int max_iterations = 4;
	const float fract = 1.0f/max_iterations;
	int iteration, n, count;
	float last;

	CLAMP(bstrength, 0, 1);

	count = (int)(bstrength*max_iterations);
	last  = max_iterations*(bstrength - count*fract);

	for(iteration = 0; iteration <= count; ++iteration) {
		#pragma omp parallel for schedule(guided) if (sd->paint.flags & PAINT_USE_OPENMP)
		for(n=0; n<totnode; n++) {
			if(ss->multires) {
				do_multires_smooth_brush(sd, ob, nodes[n], iteration != count ? 1.0f : last);
			}
			else if(ss->fmap)
				do_mesh_smooth_brush(sd, ob, nodes[n], iteration != count ? 1.0f : last);
		}

		if(ss->multires)
			multires_stitch_grids(ob);
	}
}

static void do_smooth_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
	SculptSession *ss = ob->paint->sculpt;
	smooth(sd, ob, nodes, totnode, ss->cache->bstrength);
}

static void do_draw_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
	SculptSession *ss = ob->paint->sculpt;
	Brush *brush = paint_brush(&sd->paint);
	float offset[3], area_normal[3];
	float bstrength= ss->cache->bstrength;
	int n;

	calc_sculpt_normal(sd, ob, area_normal, nodes, totnode);
	
	/* offset with as much as possible factored in already */
	mul_v3_v3fl(offset, area_normal, paint_stroke_radius(ss->cache->stroke));
	mul_v3_v3(offset, ss->cache->scale);
	mul_v3_fl(offset, bstrength);

	/* threaded loop over nodes */
	#pragma omp parallel for schedule(guided) if (sd->paint.flags & PAINT_USE_OPENMP)
	for(n=0; n<totnode; n++) {
		PBVHVertexIter vd;
		PaintStrokeTest test;
		float (*proxy)[3];

		proxy= BLI_pbvh_node_add_proxy(ob->paint->pbvh, nodes[n])->co;

		paint_stroke_test_init(&test, ss->cache->stroke);

		BLI_pbvh_vertex_iter_begin(ob->paint->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
			if (paint_stroke_test(&test, vd.co)) {
			//if(paint_stroke_test_cyl(&test, vd.co, ss->cache->location, area_normal)) {
				/* offset vertex */
				float fade = tex_strength(ss, brush, vd.co, vd.mask_combined, test.dist)*frontface(brush, area_normal, vd.no, vd.fno);

				mul_v3_v3fl(proxy[vd.i], offset, fade);

				if(vd.mvert)
					vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
			}
		}
		BLI_pbvh_vertex_iter_end;
	}
}

static void do_crease_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
	SculptSession *ss = ob->paint->sculpt;
	Brush *brush = paint_brush(&sd->paint);
	float offset[3], area_normal[3];
	float bstrength= ss->cache->bstrength;
	float flippedbstrength, crease_correction;
	int n;

	calc_sculpt_normal(sd, ob, area_normal, nodes, totnode);
	
	/* offset with as much as possible factored in already */
	mul_v3_v3fl(offset, area_normal, paint_stroke_radius(ss->cache->stroke));
	mul_v3_v3(offset, ss->cache->scale);
	mul_v3_fl(offset, bstrength);
	
	/* we divide out the squared alpha and multiply by the squared crease to give us the pinch strength */
	
	if(brush_alpha(brush) > 0.0f)
		crease_correction = brush->crease_pinch_factor*brush->crease_pinch_factor/(brush_alpha(brush)*brush_alpha(brush));
	else
		crease_correction = brush->crease_pinch_factor*brush->crease_pinch_factor;

	/* we always want crease to pinch or blob to relax even when draw is negative */
	flippedbstrength = (bstrength < 0) ? -crease_correction*bstrength : crease_correction*bstrength;

	if(brush->sculpt_tool == SCULPT_TOOL_BLOB) flippedbstrength *= -1.0f;

	/* threaded loop over nodes */
	#pragma omp parallel for schedule(guided) if (sd->paint.flags & PAINT_USE_OPENMP)
	for(n=0; n<totnode; n++) {
		PBVHVertexIter vd;
		PaintStrokeTest test;
		float (*proxy)[3];

		proxy= BLI_pbvh_node_add_proxy(ob->paint->pbvh, nodes[n])->co;

		paint_stroke_test_init(&test, ss->cache->stroke);

		BLI_pbvh_vertex_iter_begin(ob->paint->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
			if(paint_stroke_test(&test, vd.co)) {
				/* offset vertex */
				const float fade = tex_strength(ss, brush, vd.co, vd.mask_combined, test.dist)*frontface(brush, area_normal, vd.no, vd.fno);
				float val1[3];
				float val2[3];

				/* first we pinch */
				sub_v3_v3v3(val1, test.location, vd.co);
				//mul_v3_v3(val1, ss->cache->scale);
				mul_v3_fl(val1, fade*flippedbstrength);

				/* then we draw */
				mul_v3_v3fl(val2, offset, fade);

				add_v3_v3v3(proxy[vd.i], val1, val2);

				if(vd.mvert)
					vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
			}
		}
		BLI_pbvh_vertex_iter_end;
	}
}

static void do_pinch_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
	SculptSession *ss = ob->paint->sculpt;
	Brush *brush = paint_brush(&sd->paint);
	float bstrength= ss->cache->bstrength;
	int n;

	#pragma omp parallel for schedule(guided) if (sd->paint.flags & PAINT_USE_OPENMP)
	for(n=0; n<totnode; n++) {
		PBVHVertexIter vd;
		PaintStrokeTest test;
		float (*proxy)[3];

		proxy= BLI_pbvh_node_add_proxy(ob->paint->pbvh, nodes[n])->co;

		paint_stroke_test_init(&test, ss->cache->stroke);

		BLI_pbvh_vertex_iter_begin(ob->paint->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
			if(paint_stroke_test(&test, vd.co)) {
				float fade = bstrength*tex_strength(ss, brush, vd.co, vd.mask_combined, test.dist)*frontface(brush, ss->cache->view_normal, vd.no, vd.fno);
				float val[3];

				sub_v3_v3v3(val, test.location, vd.co);
				mul_v3_v3fl(proxy[vd.i], val, fade);

				if(vd.mvert)
					vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
				}
		}
		BLI_pbvh_vertex_iter_end;
	}
}

static void do_grab_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
	SculptSession *ss = ob->paint->sculpt;
	Brush *brush= paint_brush(&sd->paint);
	float bstrength= ss->cache->bstrength;
	float grab_delta[3], an[3];
	int n;
	float len;

	if (brush->normal_weight > 0)
		calc_sculpt_normal(sd, ob, an, nodes, totnode);

	copy_v3_v3(grab_delta, ss->cache->grab_delta_symmetry);

	len = len_v3(grab_delta);

	if (brush->normal_weight > 0) {
		mul_v3_fl(an, len*brush->normal_weight);
		mul_v3_fl(grab_delta, 1.0f - brush->normal_weight);
		add_v3_v3(grab_delta, an);
	}

	#pragma omp parallel for schedule(guided) if (sd->paint.flags & PAINT_USE_OPENMP)
	for(n=0; n<totnode; n++) {
		PBVHVertexIter vd;
		PBVHUndoNode* unode;
		PaintStrokeTest test;
		float (*origco)[3];
		short (*origno)[3];
		float (*proxy)[3];

		unode=  pbvh_undo_push_node(nodes[n], PBVH_UNDO_CO_NO, ob, NULL);
		origco= pbvh_undo_node_co(unode);
		origno= pbvh_undo_node_no(unode);

		proxy= BLI_pbvh_node_add_proxy(ob->paint->pbvh, nodes[n])->co;

		paint_stroke_test_init(&test, ss->cache->stroke);

		BLI_pbvh_vertex_iter_begin(ob->paint->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
			if(paint_stroke_test(&test, origco[vd.i])) {
				const float fade = bstrength*tex_strength(ss, brush, origco[vd.i], vd.mask_combined, test.dist)*frontface(brush, an, origno[vd.i], NULL);

				mul_v3_v3fl(proxy[vd.i], grab_delta, fade);

				if(vd.mvert)
					vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
			}
		}
		BLI_pbvh_vertex_iter_end;
	}
}

static void do_nudge_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
	SculptSession *ss = ob->paint->sculpt;
	Brush *brush = paint_brush(&sd->paint);
	float bstrength = ss->cache->bstrength;
	float grab_delta[3];
	int n;
	float an[3];
	float tmp[3], cono[3];

	copy_v3_v3(grab_delta, ss->cache->grab_delta_symmetry);

	calc_sculpt_normal(sd, ob, an, nodes, totnode);

	cross_v3_v3v3(tmp, an, grab_delta);
	cross_v3_v3v3(cono, tmp, an);

	#pragma omp parallel for schedule(guided) if (sd->paint.flags & PAINT_USE_OPENMP)
	for(n = 0; n < totnode; n++) {
		PBVHVertexIter vd;
		PaintStrokeTest test;
		float (*proxy)[3];

		proxy= BLI_pbvh_node_add_proxy(ob->paint->pbvh, nodes[n])->co;

		paint_stroke_test_init(&test, ss->cache->stroke);

		BLI_pbvh_vertex_iter_begin(ob->paint->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
			if(paint_stroke_test(&test, vd.co)) {
				const float fade = bstrength*tex_strength(ss, brush, vd.co, vd.mask_combined, test.dist)*frontface(brush, an, vd.no, vd.fno);

				mul_v3_v3fl(proxy[vd.i], cono, fade);

				if(vd.mvert)
					vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
			}
		}
		BLI_pbvh_vertex_iter_end;
	}
}

static void do_snake_hook_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
	SculptSession *ss = ob->paint->sculpt;
	Brush *brush = paint_brush(&sd->paint);
	float bstrength = ss->cache->bstrength;
	float grab_delta[3], an[3];
	int n;
	float len;

	if (brush->normal_weight > 0)
		calc_sculpt_normal(sd, ob, an, nodes, totnode);

	copy_v3_v3(grab_delta, ss->cache->grab_delta_symmetry);

	len = len_v3(grab_delta);

	if (bstrength < 0)
		negate_v3(grab_delta);

	if (brush->normal_weight > 0) {
		mul_v3_fl(an, len*brush->normal_weight);
		mul_v3_fl(grab_delta, 1.0f - brush->normal_weight);
		add_v3_v3(grab_delta, an);
	}

	#pragma omp parallel for schedule(guided) if (sd->paint.flags & PAINT_USE_OPENMP)
	for(n = 0; n < totnode; n++) {
		PBVHVertexIter vd;
		PaintStrokeTest test;
		float (*proxy)[3];

		proxy= BLI_pbvh_node_add_proxy(ob->paint->pbvh, nodes[n])->co;

		paint_stroke_test_init(&test, ss->cache->stroke);

		BLI_pbvh_vertex_iter_begin(ob->paint->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
			if(paint_stroke_test(&test, vd.co)) {
				const float fade = bstrength*tex_strength(ss, brush, vd.co, vd.mask_combined, test.dist)*frontface(brush, an, vd.no, vd.fno);

				mul_v3_v3fl(proxy[vd.i], grab_delta, fade);

				if(vd.mvert)
					vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
			}
		}
		BLI_pbvh_vertex_iter_end;
	}
}

static void do_thumb_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
	SculptSession *ss = ob->paint->sculpt;
	Brush *brush = paint_brush(&sd->paint);
	float bstrength = ss->cache->bstrength;
	float grab_delta[3];
	int n;
	float an[3];
	float tmp[3], cono[3];

	copy_v3_v3(grab_delta, ss->cache->grab_delta_symmetry);

	calc_sculpt_normal(sd, ob, an, nodes, totnode);

	cross_v3_v3v3(tmp, an, grab_delta);
	cross_v3_v3v3(cono, tmp, an);

	#pragma omp parallel for schedule(guided) if (sd->paint.flags & PAINT_USE_OPENMP)
	for(n = 0; n < totnode; n++) {
		PBVHVertexIter vd;
		PBVHUndoNode* unode;
		PaintStrokeTest test;
		float (*origco)[3];
		short (*origno)[3];
		float (*proxy)[3];

		unode=  pbvh_undo_push_node(nodes[n], PBVH_UNDO_CO_NO, ob, NULL);
		origco= pbvh_undo_node_co(unode);
		origno= pbvh_undo_node_no(unode);

		proxy= BLI_pbvh_node_add_proxy(ob->paint->pbvh, nodes[n])->co;

		paint_stroke_test_init(&test, ss->cache->stroke);

		BLI_pbvh_vertex_iter_begin(ob->paint->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
			if(paint_stroke_test(&test, origco[vd.i])) {
				const float fade = bstrength*tex_strength(ss, brush, origco[vd.i], vd.mask_combined, test.dist)*frontface(brush, an, origno[vd.i], NULL);

				mul_v3_v3fl(proxy[vd.i], cono, fade);

				if(vd.mvert)
					vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
			}
		}
		BLI_pbvh_vertex_iter_end;
	}
}

static void do_rotate_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
	SculptSession *ss = ob->paint->sculpt;
	Brush *brush= paint_brush(&sd->paint);
	float bstrength= ss->cache->bstrength;
	float an[3];
	int n;
	float m[3][3];
	static const int flip[8] = { 1, -1, -1, 1, -1, 1, 1, -1 };
	float angle = ss->cache->vertex_rotation * flip[ss->cache->mirror_symmetry_pass];

	calc_sculpt_normal(sd, ob, an, nodes, totnode);

	axis_angle_to_mat3(m, an, angle);

	#pragma omp parallel for schedule(guided) if (sd->paint.flags & PAINT_USE_OPENMP)
	for(n=0; n<totnode; n++) {
		PBVHVertexIter vd;
		PBVHUndoNode* unode;
		PaintStrokeTest test;
		float (*origco)[3];
		short (*origno)[3];
		float (*proxy)[3];

		unode=  pbvh_undo_push_node(nodes[n], PBVH_UNDO_CO_NO, ob, NULL);
		origco= pbvh_undo_node_co(unode);
		origno= pbvh_undo_node_no(unode);

		proxy= BLI_pbvh_node_add_proxy(ob->paint->pbvh, nodes[n])->co;

		paint_stroke_test_init(&test, ss->cache->stroke);

		BLI_pbvh_vertex_iter_begin(ob->paint->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
			if(paint_stroke_test(&test, origco[vd.i])) {
				const float fade = bstrength*tex_strength(ss, brush, origco[vd.i], vd.mask_combined, test.dist)*frontface(brush, an, origno[vd.i], NULL);

				mul_v3_m3v3(proxy[vd.i], m, origco[vd.i]);
				sub_v3_v3(proxy[vd.i], origco[vd.i]);
				mul_v3_fl(proxy[vd.i], fade);

				if(vd.mvert)
					vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
			}
		}
		BLI_pbvh_vertex_iter_end;
	}
}

static void do_layer_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
	SculptSession *ss = ob->paint->sculpt;
	Brush *brush = paint_brush(&sd->paint);
	float bstrength= ss->cache->bstrength;
	float area_normal[3], offset[3];
	float lim= paint_stroke_radius(ss->cache->stroke) / 4;
	int n;

	if(bstrength < 0)
		lim = -lim;

	calc_sculpt_normal(sd, ob, area_normal, nodes, totnode);

	mul_v3_v3v3(offset, ss->cache->scale, area_normal);

	#pragma omp parallel for schedule(guided) if (sd->paint.flags & PAINT_USE_OPENMP)
	for(n=0; n<totnode; n++) {
		PBVHVertexIter vd;
		PaintStrokeTest test;
		PBVHUndoNode *unode;
		float (*origco)[3], *layer_disp;
		//float (*proxy)[3]; // XXX layer brush needs conversion to proxy but its more complicated

		//proxy= BLI_pbvh_node_add_proxy(ob->paint->pbvh, nodes[n])->co;
		
		unode= pbvh_undo_push_node(nodes[n], PBVH_UNDO_CO_NO, ob, NULL);
		origco= pbvh_undo_node_co(unode);
		if(!pbvh_undo_node_layer_disp(unode)) {
			#pragma omp critical 
			pbvh_undo_node_set_layer_disp(unode, MEM_callocN(sizeof(float)*
				pbvh_undo_node_totvert(unode), "layer disp"));
		}

		layer_disp= pbvh_undo_node_layer_disp(unode);

		paint_stroke_test_init(&test, ss->cache->stroke);

		BLI_pbvh_vertex_iter_begin(ob->paint->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
			if(paint_stroke_test(&test, vd.co)) {
				const float fade = bstrength*paint_stroke_radius(ss->cache->stroke)*tex_strength(ss, brush, vd.co, vd.mask_combined, test.dist)*frontface(brush, area_normal, vd.no, vd.fno);
				float *disp= &layer_disp[vd.i];
				float val[3];

				*disp+= fade;

				/* Don't let the displacement go past the limit */
				if((lim < 0 && *disp < lim) || (lim > 0 && *disp > lim))
					*disp = lim;

				mul_v3_v3fl(val, offset, *disp);

				if(ss->layer_co && (brush->flag & BRUSH_PERSISTENT)) {
					int index= vd.vert_indices[vd.i];

					/* persistent base */
					add_v3_v3(val, ss->layer_co[index]);
				}
				else {
					add_v3_v3(val, origco[vd.i]);
				}

				sculpt_clip(sd, ss, vd.co, val);

				if(vd.mvert)
					vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
			}
		}
		BLI_pbvh_vertex_iter_end;
	}
}

static void do_inflate_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
	SculptSession *ss = ob->paint->sculpt;
	Brush *brush = paint_brush(&sd->paint);
	float bstrength= ss->cache->bstrength;
	int n;

	#pragma omp parallel for schedule(guided) if (sd->paint.flags & PAINT_USE_OPENMP)
	for(n=0; n<totnode; n++) {
		PBVHVertexIter vd;
		PaintStrokeTest test;
		float (*proxy)[3];

		proxy= BLI_pbvh_node_add_proxy(ob->paint->pbvh, nodes[n])->co;

		paint_stroke_test_init(&test, ss->cache->stroke);

		BLI_pbvh_vertex_iter_begin(ob->paint->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
			if(paint_stroke_test(&test, vd.co)) {
				const float fade = bstrength*tex_strength(ss, brush, vd.co, vd.mask_combined, test.dist)*frontface(brush, ss->cache->view_normal, vd.no, vd.fno);
				float val[3];

				if(vd.fno) copy_v3_v3(val, vd.fno);
				else normal_short_to_float_v3(val, vd.no);
				
				mul_v3_fl(val, fade * paint_stroke_radius(ss->cache->stroke));
				mul_v3_v3v3(proxy[vd.i], val, ss->cache->scale);

				if(vd.mvert)
					vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
			}
		}
		BLI_pbvh_vertex_iter_end;
	}
}

static void calc_flatten_center(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode, float fc[3])
{
	SculptSession *ss = ob->paint->sculpt;
	int n;

	float count = 0;

	zero_v3(fc);

	#pragma omp parallel for schedule(guided) if (sd->paint.flags & PAINT_USE_OPENMP)
	for(n=0; n<totnode; n++) {
		PBVHVertexIter vd;
		PaintStrokeTest test;
		PBVHUndoNode *unode;
		float private_fc[3] = {0.0f, 0.0f, 0.0f};
		int private_count = 0;

		unode = pbvh_undo_push_node(nodes[n], PBVH_UNDO_CO_NO, ob, NULL);
		paint_stroke_test_init(&test, ss->cache->stroke);

		if(ss->cache->original) {
			BLI_pbvh_vertex_iter_begin(ob->paint->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
				if(paint_stroke_test_fast(&test, pbvh_undo_node_co(unode)[vd.i])) {
					add_v3_v3(private_fc, vd.co);
					private_count++;
				}
			}
			BLI_pbvh_vertex_iter_end;
		}
		else {
			BLI_pbvh_vertex_iter_begin(ob->paint->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
				if(paint_stroke_test_fast(&test, vd.co)) {
					add_v3_v3(private_fc, vd.co);
					private_count++;
				}
			}
			BLI_pbvh_vertex_iter_end;
		}

		#pragma omp critical
		{
			add_v3_v3(fc, private_fc);
			count += private_count;
		}
	}

	mul_v3_fl(fc, 1.0f / count);
}

/* this calculates flatten center and area normal together, 
amortizing the memory bandwidth and loop overhead to calculate both at the same time */
static void calc_area_normal_and_flatten_center(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode, float an[3], float fc[3])
{
	SculptSession *ss = ob->paint->sculpt;
	int n;

	// an
	float out_flip[3] = {0.0f, 0.0f, 0.0f};

	// fc
	float count = 0;

	// an
	zero_v3(an);

	// fc
	zero_v3(fc);

	#pragma omp parallel for schedule(guided) if (sd->paint.flags & PAINT_USE_OPENMP)
	for(n=0; n<totnode; n++) {
		PBVHVertexIter vd;
		PaintStrokeTest test;
		PBVHUndoNode *unode;
		float private_an[3] = {0.0f, 0.0f, 0.0f};
		float private_out_flip[3] = {0.0f, 0.0f, 0.0f};
		float private_fc[3] = {0.0f, 0.0f, 0.0f};
		int private_count = 0;

		unode = pbvh_undo_push_node(nodes[n], PBVH_UNDO_CO_NO, ob, NULL);
		paint_stroke_test_init(&test, ss->cache->stroke);

		if(ss->cache->original) {
			BLI_pbvh_vertex_iter_begin(ob->paint->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
				if(paint_stroke_test_fast(&test, pbvh_undo_node_co(unode)[vd.i])) {
					// an
					float fno[3];

					normal_short_to_float_v3(fno, pbvh_undo_node_no(unode)[vd.i]);
					add_norm_if(ss->cache->view_normal, private_an, private_out_flip, fno);

					// fc
					add_v3_v3(private_fc, vd.co);
					private_count++;
				}
			}
			BLI_pbvh_vertex_iter_end;
		}
		else {
			BLI_pbvh_vertex_iter_begin(ob->paint->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
				if(paint_stroke_test_fast(&test, vd.co)) {
					// an
					if(vd.no) {
						float fno[3];

						normal_short_to_float_v3(fno, vd.no);
						add_norm_if(ss->cache->view_normal, private_an, private_out_flip, fno);
					}
					else {
						add_norm_if(ss->cache->view_normal, private_an, private_out_flip, vd.fno);
					}

					// fc
					add_v3_v3(private_fc, vd.co);
					private_count++;
				}
			}
			BLI_pbvh_vertex_iter_end;
		}

		#pragma omp critical
		{
			// an
			add_v3_v3(an, private_an);
			add_v3_v3(out_flip, private_out_flip);

			// fc
			add_v3_v3(fc, private_fc);
			count += private_count;
		}
	}

	// an
	if (is_zero_v3(an))
		copy_v3_v3(an, out_flip);

	normalize_v3(an);

	// fc
	if (count != 0) {
		mul_v3_fl(fc, 1.0f / count);
	}
	else {
		zero_v3(fc);
	}
}

static void calc_sculpt_plane(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode, float an[3], float fc[3])
{
	SculptSession *ss = ob->paint->sculpt;
	Brush *brush = paint_brush(&sd->paint);

	if (ss->cache->mirror_symmetry_pass == 0 &&
	    ss->cache->radial_symmetry_pass == 0 &&
	    (paint_stroke_first_dab(ss->cache->stroke) || !(brush->flag & BRUSH_ORIGINAL_NORMAL)))
	{
		switch (brush->sculpt_plane) {
			case SCULPT_DISP_DIR_VIEW:
				viewvector(ss->cache->vc->rv3d, ss->cache->vc->rv3d->twmat[3], an);
				break;

			case SCULPT_DISP_DIR_X:
				an[1] = 0.0;
				an[2] = 0.0;
				an[0] = 1.0;
				break;

			case SCULPT_DISP_DIR_Y:
				an[0] = 0.0;
				an[2] = 0.0;
				an[1] = 1.0;
				break;

			case SCULPT_DISP_DIR_Z:
				an[0] = 0.0;
				an[1] = 0.0;
				an[2] = 1.0;
				break;

			case SCULPT_DISP_DIR_AREA:
				calc_area_normal_and_flatten_center(sd, ob, nodes, totnode, an, fc);

			default:
				break;
		}

		// fc
		/* flatten center has not been calculated yet if we are not using the area normal */
		if (brush->sculpt_plane != SCULPT_DISP_DIR_AREA)
			calc_flatten_center(sd, ob, nodes, totnode, fc);

		// an
		copy_v3_v3(ss->cache->last_area_normal, an);

		// fc
		copy_v3_v3(ss->cache->last_center, fc);
	}
	else {
		// an
		copy_v3_v3(an, ss->cache->last_area_normal);

		// fc
		copy_v3_v3(fc, ss->cache->last_center);

		// an
		paint_flip_coord(an, an, ss->cache->mirror_symmetry_pass);

		// fc
		paint_flip_coord(fc, fc, ss->cache->mirror_symmetry_pass);

		// an
		mul_m4_v3(ss->cache->symm_rot_mat, an);

		// fc
		mul_m4_v3(ss->cache->symm_rot_mat, fc);
	}
}

/* Projects a point onto a plane along the plane's normal */
static void point_plane_project(float intr[3], float co[3], float plane_normal[3], float plane_center[3])
{
    sub_v3_v3v3(intr, co, plane_center);
    mul_v3_v3fl(intr, plane_normal, dot_v3v3(plane_normal, intr));
    sub_v3_v3v3(intr, co, intr); 
}

static int plane_trim(StrokeCache *cache, Brush *brush, float val[3])
{
	return !(brush->flag & BRUSH_PLANE_TRIM) ||
		(dot_v3v3(val, val) <= paint_stroke_radius_squared(cache->stroke)*cache->plane_trim_squared);
}

static int plane_point_side_flip(float co[3], float plane_normal[3], float plane_center[3], int flip)
{
    float delta[3];
    float d;

    sub_v3_v3v3(delta, co, plane_center);
    d = dot_v3v3(plane_normal, delta);

    if (flip) d = -d;

    return d <= 0.0f;
}

static int plane_point_side(float co[3], float plane_normal[3], float plane_center[3])
{
    float delta[3];

    sub_v3_v3v3(delta, co, plane_center);
    return  dot_v3v3(plane_normal, delta) <= 0.0f;
}

static float get_offset(Sculpt *sd, SculptSession *ss)
{
	Brush* brush = paint_brush(&sd->paint);

	float rv = brush->plane_offset;

	if (brush->flag & BRUSH_OFFSET_PRESSURE) {
		rv *= paint_stroke_pressure(ss->cache->stroke);
	}

	return rv;
}

static void do_flatten_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
	SculptSession *ss = ob->paint->sculpt;
	Brush *brush = paint_brush(&sd->paint);

	float bstrength = ss->cache->bstrength;
	const float radius = paint_stroke_radius(ss->cache->stroke);

	float an[3];
	float fc[3];

	float offset = get_offset(sd, ss);

	float displace;

	int n;

	float temp[3];

	calc_sculpt_plane(sd, ob, nodes, totnode, an, fc);

	displace = radius*offset;

	mul_v3_v3v3(temp, an, ss->cache->scale);
	mul_v3_fl(temp, displace);
	add_v3_v3(fc, temp);

	#pragma omp parallel for schedule(guided) if (sd->paint.flags & PAINT_USE_OPENMP)
	for(n = 0; n < totnode; n++) {
		PBVHVertexIter  vd;
		PaintStrokeTest test;
		float (*proxy)[3];

		proxy= BLI_pbvh_node_add_proxy(ob->paint->pbvh, nodes[n])->co;

		paint_stroke_test_init(&test, ss->cache->stroke);

		BLI_pbvh_vertex_iter_begin(ob->paint->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
			if (paint_stroke_test_sq(&test, vd.co)) {
				float intr[3];
				float val[3];

				point_plane_project(intr, vd.co, an, fc);

				sub_v3_v3v3(val, intr, vd.co);

				if (plane_trim(ss->cache, brush, val)) {
					const float fade = bstrength*tex_strength(ss, brush, vd.co, vd.mask_combined, sqrt(test.dist))*frontface(brush, an, vd.no, vd.fno);

					mul_v3_v3fl(proxy[vd.i], val, fade);

					if(vd.mvert)
						vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
				}
			}
		}
		BLI_pbvh_vertex_iter_end;
	}
}

static void do_clay_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
	SculptSession *ss = ob->paint->sculpt;
	Brush *brush = paint_brush(&sd->paint);

	float bstrength = ss->cache->bstrength;
	float radius    = paint_stroke_radius(ss->cache->stroke);
	float offset    = get_offset(sd, ss);
	
	float displace;

	float an[3]; // area normal
	float fc[3]; // flatten center

	int n;

	float temp[3];
	//float p[3];

	int flip;

	calc_sculpt_plane(sd, ob, nodes, totnode, an, fc);

	flip = bstrength < 0;

	if (flip) {
		bstrength = -bstrength;
		radius    = -radius;
	}

	displace = radius * (0.25f+offset);

	mul_v3_v3v3(temp, an, ss->cache->scale);
	mul_v3_fl(temp, displace);
	add_v3_v3(fc, temp);

	//add_v3_v3v3(p, ss->cache->location, an);

	#pragma omp parallel for schedule(guided) if (sd->paint.flags & PAINT_USE_OPENMP)
	for (n = 0; n < totnode; n++) {
		PBVHVertexIter vd;
		PaintStrokeTest test;
		float (*proxy)[3];

		proxy= BLI_pbvh_node_add_proxy(ob->paint->pbvh, nodes[n])->co;

		paint_stroke_test_init(&test, ss->cache->stroke);

		BLI_pbvh_vertex_iter_begin(ob->paint->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
			if (paint_stroke_test_sq(&test, vd.co)) {
				if (plane_point_side_flip(vd.co, an, fc, flip)) {
				//if (paint_stroke_test_cyl(&test, vd.co, ss->cache->location, p)) {
					float intr[3];
					float val[3];

					point_plane_project(intr, vd.co, an, fc);

					sub_v3_v3v3(val, intr, vd.co);

					if (plane_trim(ss->cache, brush, val)) {
						const float fade = bstrength*tex_strength(ss, brush, vd.co, vd.mask_combined, sqrt(test.dist))*frontface(brush, an, vd.no, vd.fno);

						mul_v3_v3fl(proxy[vd.i], val, fade);

						if(vd.mvert)
							vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
					}
				}
			}
		}
		BLI_pbvh_vertex_iter_end;
	}
}

static void do_clay_tubes_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
	SculptSession *ss = ob->paint->sculpt;
	Brush *brush = paint_brush(&sd->paint);

	float bstrength = ss->cache->bstrength;
	float radius    = paint_stroke_radius(ss->cache->stroke);
	float offset    = get_offset(sd, ss);
	
	float displace;

	float sn[3]; // sculpt normal
	float an[3]; // area normal
	float fc[3]; // flatten center

	int n;

	float temp[3];
	float mat[4][4];
	float scale[4][4];
	float tmat[4][4];

	int flip;

	calc_sculpt_plane(sd, ob, nodes, totnode, sn, fc);

	if (brush->sculpt_plane != SCULPT_DISP_DIR_AREA || (brush->flag & BRUSH_ORIGINAL_NORMAL))
		calc_area_normal(sd, ob, an, nodes, totnode);
	else
		copy_v3_v3(an, sn);

	if(paint_stroke_first_dab(ss->cache->stroke))
		return; // delay the first dab because grab delta is not setup

	flip = bstrength < 0;

	if (flip) {
		bstrength = -bstrength;
		radius    = -radius;
	}

	displace = radius * (0.25f+offset);

	mul_v3_v3v3(temp, sn, ss->cache->scale);
	mul_v3_fl(temp, displace);
	add_v3_v3(fc, temp);

	cross_v3_v3v3(mat[0], an, ss->cache->grab_delta_symmetry); mat[0][3] = 0;
	cross_v3_v3v3(mat[1], an, mat[0]); mat[1][3] = 0;
	copy_v3_v3(mat[2], an); mat[2][3] = 0;
	paint_stroke_symmetry_location(ss->cache->stroke, mat[3]);
	mat[3][3] = 1;
	normalize_m4(mat);
	scale_m4_fl(scale, paint_stroke_radius(ss->cache->stroke));
	mul_m4_m4m4(tmat, scale, mat);
	invert_m4_m4(mat, tmat);

	#pragma omp parallel for schedule(guided) if (sd->paint.flags & PAINT_USE_OPENMP)
	for (n = 0; n < totnode; n++) {
		PBVHVertexIter vd;
		PaintStrokeTest test;
		float (*proxy)[3];

		proxy= BLI_pbvh_node_add_proxy(ob->paint->pbvh, nodes[n])->co;

		paint_stroke_test_init(&test, ss->cache->stroke);

		BLI_pbvh_vertex_iter_begin(ob->paint->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
			if (paint_stroke_test_cube(&test, vd.co, mat)) {
				if (plane_point_side_flip(vd.co, sn, fc, flip)) {
					float intr[3];
					float val[3];

					point_plane_project(intr, vd.co, sn, fc);

					sub_v3_v3v3(val, intr, vd.co);

					if (plane_trim(ss->cache, brush, val)) {
						const float fade = bstrength*tex_strength(ss, brush, vd.co, vd.mask_combined, paint_stroke_radius(ss->cache->stroke)*test.dist)*frontface(brush, an, vd.no, vd.fno);

						mul_v3_v3fl(proxy[vd.i], val, fade);

						if(vd.mvert)
							vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
					}
				}
			}
		}
		BLI_pbvh_vertex_iter_end;
	}
}

static void do_fill_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
	SculptSession *ss = ob->paint->sculpt;
	Brush *brush = paint_brush(&sd->paint);

	float bstrength = ss->cache->bstrength;
	const float radius = paint_stroke_radius(ss->cache->stroke);

	float an[3];
	float fc[3];
	float offset = get_offset(sd, ss);

	float displace;

	int n;

	float temp[3];

	calc_sculpt_plane(sd, ob, nodes, totnode, an, fc);

	displace = radius*offset;

	mul_v3_v3v3(temp, an, ss->cache->scale);
	mul_v3_fl(temp, displace);
	add_v3_v3(fc, temp);

	#pragma omp parallel for schedule(guided) if (sd->paint.flags & PAINT_USE_OPENMP)
	for (n = 0; n < totnode; n++) {
		PBVHVertexIter vd;
		PaintStrokeTest test;
		float (*proxy)[3];

		proxy= BLI_pbvh_node_add_proxy(ob->paint->pbvh, nodes[n])->co;

		paint_stroke_test_init(&test, ss->cache->stroke);

		BLI_pbvh_vertex_iter_begin(ob->paint->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
			if (paint_stroke_test_sq(&test, vd.co)) {
				if (plane_point_side(vd.co, an, fc)) {
					float intr[3];
					float val[3];

					point_plane_project(intr, vd.co, an, fc);

					sub_v3_v3v3(val, intr, vd.co);

					if (plane_trim(ss->cache, brush, val)) {
						const float fade = bstrength*tex_strength(ss, brush, vd.co, vd.mask_combined, sqrt(test.dist))*frontface(brush, an, vd.no, vd.fno);

						mul_v3_v3fl(proxy[vd.i], val, fade);

						if(vd.mvert)
							vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
					}
				}
			}
		}
		BLI_pbvh_vertex_iter_end;
	}
}

static void do_scrape_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
	SculptSession *ss = ob->paint->sculpt;
	Brush *brush = paint_brush(&sd->paint);

	float bstrength = ss->cache->bstrength;
	const float radius = paint_stroke_radius(ss->cache->stroke);

	float an[3];
	float fc[3];
	float offset = get_offset(sd, ss);

	float displace;

	int n;

	float temp[3];

	calc_sculpt_plane(sd, ob, nodes, totnode, an, fc);

	displace = -radius*offset;

	mul_v3_v3v3(temp, an, ss->cache->scale);
	mul_v3_fl(temp, displace);
	add_v3_v3(fc, temp);

	#pragma omp parallel for schedule(guided) if (sd->paint.flags & PAINT_USE_OPENMP)
	for (n = 0; n < totnode; n++) {
		PBVHVertexIter vd;
		PaintStrokeTest test;
		float (*proxy)[3];

		proxy= BLI_pbvh_node_add_proxy(ob->paint->pbvh, nodes[n])->co;

		paint_stroke_test_init(&test, ss->cache->stroke);

		BLI_pbvh_vertex_iter_begin(ob->paint->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
			if (paint_stroke_test_sq(&test, vd.co)) {
				if (!plane_point_side(vd.co, an, fc)) {
					float intr[3];
					float val[3];

					point_plane_project(intr, vd.co, an, fc);

					sub_v3_v3v3(val, intr, vd.co);

					if (plane_trim(ss->cache, brush, val)) {
						const float fade = bstrength*tex_strength(ss, brush, vd.co, vd.mask_combined, sqrt(test.dist))*frontface(brush, an, vd.no, vd.fno);

						mul_v3_v3fl(proxy[vd.i], val, fade);

						if(vd.mvert)
							vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
					}
				}
			}
		}
		BLI_pbvh_vertex_iter_end;
	}
}

void sculpt_vertcos_to_key(Object *ob, KeyBlock *kb, float (*vertCos)[3])
{
	Mesh *me= (Mesh*)ob->data;
	float (*ofs)[3]= NULL;
	int a, is_basis= 0;
	KeyBlock *currkey;

	/* for relative keys editing of base should update other keys */
	if (me->key->type == KEY_RELATIVE)
		for (currkey = me->key->block.first; currkey; currkey= currkey->next)
			if(ob->shapenr-1 == currkey->relative) {
				is_basis= 1;
				break;
			}

	if (is_basis) {
		ofs= key_to_vertcos(ob, kb);

		/* calculate key coord offsets (from previous location) */
		for (a= 0; a < me->totvert; a++) {
			VECSUB(ofs[a], vertCos[a], ofs[a]);
		}

		/* apply offsets on other keys */
		currkey = me->key->block.first;
		while (currkey) {
			int apply_offset = ((currkey != kb) && (ob->shapenr-1 == currkey->relative));

			if (apply_offset)
				offset_to_key(ob, currkey, ofs);

			currkey= currkey->next;
		}

		MEM_freeN(ofs);
	}

	/* modifying of basis key should update mesh */
	if (kb == me->key->refkey) {
		MVert *mvert= me->mvert;

		for (a= 0; a < me->totvert; a++, mvert++)
			VECCOPY(mvert->co, vertCos[a]);

		mesh_calc_normals(me->mvert, me->totvert, me->mface, me->totface, NULL);
	}

	/* apply new coords on active key block */
	vertcos_to_key(ob, kb, vertCos);
}

/* copy the modified vertices from bvh to the active key */
static void sculpt_update_keyblock(Object *ob)
{
	SculptSession *ss = ob->paint->sculpt;
	float (*vertCos)[3]= BLI_pbvh_get_vertCos(ob->paint->pbvh);

	if (vertCos) {
		sculpt_vertcos_to_key(ob, ss->kb, vertCos);
		MEM_freeN(vertCos);
	}
}

static void sculpt_stroke_brush_action(bContext *C, PaintStroke *stroke)
{
	Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
	Object *ob = CTX_data_active_object(C);
	SculptSession *ss = ob->paint->sculpt;
	Brush *brush = paint_brush(&sd->paint);
	PBVHSearchSphereData data;
	PBVHNode **nodes = NULL;
	float location[3];
	int n, totnode;

	/* Build a list of all nodes that are potentially within the brush's area of influence */
	paint_stroke_symmetry_location(ss->cache->stroke, location);
	data.center = location;
	data.radius_squared = paint_stroke_radius_squared(ss->cache->stroke);
	data.original = ELEM4(brush->sculpt_tool, SCULPT_TOOL_GRAB, SCULPT_TOOL_ROTATE, SCULPT_TOOL_THUMB, SCULPT_TOOL_LAYER);
	BLI_pbvh_search_gather(ob->paint->pbvh, BLI_pbvh_search_sphere_cb, &data, &nodes, &totnode);

	/* Only act if some verts are inside the brush area */
	if (totnode) {
		/* Apply one type of brush action */
		if(brush->flag & BRUSH_MASK)
			paintmask_brush_apply(&sd->paint, stroke, nodes,
					      totnode,
					      ss->cache->bstrength);
		else {
			#pragma omp parallel for schedule(guided) if (sd->paint.flags & PAINT_USE_OPENMP)
			for (n= 0; n < totnode; n++) {
				pbvh_undo_push_node(nodes[n], PBVH_UNDO_CO_NO, ob, NULL);
				BLI_pbvh_node_mark_update(nodes[n]);
			}

			switch(brush->sculpt_tool){
			case SCULPT_TOOL_DRAW:
				do_draw_brush(sd, ob, nodes, totnode);
				break;
			case SCULPT_TOOL_SMOOTH:
				do_smooth_brush(sd, ob, nodes, totnode);
				break;
			case SCULPT_TOOL_CREASE:
				do_crease_brush(sd, ob, nodes, totnode);
				break;
			case SCULPT_TOOL_BLOB:
				do_crease_brush(sd, ob, nodes, totnode);
				break;
			case SCULPT_TOOL_PINCH:
				do_pinch_brush(sd, ob, nodes, totnode);
				break;
			case SCULPT_TOOL_INFLATE:
				do_inflate_brush(sd, ob, nodes, totnode);
				break;
			case SCULPT_TOOL_GRAB:
				do_grab_brush(sd, ob, nodes, totnode);
				break;
			case SCULPT_TOOL_ROTATE:
				do_rotate_brush(sd, ob, nodes, totnode);
				break;
			case SCULPT_TOOL_SNAKE_HOOK:
				do_snake_hook_brush(sd, ob, nodes, totnode);
				break;
			case SCULPT_TOOL_NUDGE:
				do_nudge_brush(sd, ob, nodes, totnode);
				break;
			case SCULPT_TOOL_THUMB:
				do_thumb_brush(sd, ob, nodes, totnode);
				break;
			case SCULPT_TOOL_LAYER:
				do_layer_brush(sd, ob, nodes, totnode);
				break;
			case SCULPT_TOOL_FLATTEN:
				do_flatten_brush(sd, ob, nodes, totnode);
				break;
			case SCULPT_TOOL_CLAY:
				do_clay_brush(sd, ob, nodes, totnode);
				break;
			case SCULPT_TOOL_CLAY_TUBES:
				do_clay_tubes_brush(sd, ob, nodes, totnode);
				break;
			case SCULPT_TOOL_FILL:
				do_fill_brush(sd, ob, nodes, totnode);
				break;
			case SCULPT_TOOL_SCRAPE:
				do_scrape_brush(sd, ob, nodes, totnode);
				break;
			}
		}

		if (brush->sculpt_tool != SCULPT_TOOL_SMOOTH && brush->autosmooth_factor > 0) {
			if (brush->flag & BRUSH_INVERSE_SMOOTH_PRESSURE) {
				smooth(sd, ob, nodes, totnode, brush->autosmooth_factor *
				       (1 - paint_stroke_pressure(ss->cache->stroke)));
			}
			else {
				smooth(sd, ob, nodes, totnode, brush->autosmooth_factor);
			}
		}

		/* copy the modified vertices from mesh to the active key */
		if(ss->kb)
			mesh_to_key(ob->data, ss->kb);

		/* optimization: we could avoid copying new coords to keyblock at each */
		/* stroke step if there are no modifiers due to pbvh is used for displaying */
		/* so to increase speed we'll copy new coords to keyblock when stroke is done */
		if(ss->kb && ss->modifiers_active) sculpt_update_keyblock(ob);

		MEM_freeN(nodes);
	}
}

static void sculpt_combine_proxies(Sculpt *sd, Object *ob)
{
	SculptSession *ss = ob->paint->sculpt;
	Brush *brush= paint_brush(&sd->paint);
	PBVHNode** nodes;
	int totnode;
	int n;

	BLI_pbvh_gather_proxies(ob->paint->pbvh, &nodes, &totnode);

	switch (brush->sculpt_tool) {
		case SCULPT_TOOL_GRAB:
		case SCULPT_TOOL_ROTATE:
		case SCULPT_TOOL_THUMB:
			#pragma omp parallel for schedule(guided) if (sd->paint.flags & PAINT_USE_OPENMP)
			for (n= 0; n < totnode; n++) {
				PBVHVertexIter vd;
				PBVHProxyNode* proxies;
				PBVHUndoNode *unode;
				int proxy_count;
				float (*origco)[3];

				unode= pbvh_undo_push_node(nodes[n], PBVH_UNDO_CO_NO, ob, NULL);
				origco= pbvh_undo_node_co(unode);

				BLI_pbvh_node_get_proxies(nodes[n], &proxies, &proxy_count);

				BLI_pbvh_vertex_iter_begin(ob->paint->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
					float val[3];
					int p;

					copy_v3_v3(val, origco[vd.i]);

					for (p= 0; p < proxy_count; p++)
						add_v3_v3(val, proxies[p].co[vd.i]);

					sculpt_clip(sd, ss, vd.co, val);
				}
				BLI_pbvh_vertex_iter_end;

				BLI_pbvh_node_free_proxies(nodes[n]);
			}

			break;

		case SCULPT_TOOL_DRAW:
		case SCULPT_TOOL_CLAY:
		case SCULPT_TOOL_CLAY_TUBES:
		case SCULPT_TOOL_CREASE:
		case SCULPT_TOOL_BLOB:
		case SCULPT_TOOL_FILL:
		case SCULPT_TOOL_FLATTEN:
		case SCULPT_TOOL_INFLATE:
		case SCULPT_TOOL_NUDGE:
		case SCULPT_TOOL_PINCH:
		case SCULPT_TOOL_SCRAPE:
		case SCULPT_TOOL_SNAKE_HOOK:
			#pragma omp parallel for schedule(guided) if (sd->paint.flags & PAINT_USE_OPENMP)
			for (n= 0; n < totnode; n++) {
				PBVHVertexIter vd;
				PBVHProxyNode* proxies;
				int proxy_count;

				BLI_pbvh_node_get_proxies(nodes[n], &proxies, &proxy_count);

				BLI_pbvh_vertex_iter_begin(ob->paint->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
					float val[3];
					int p;

					copy_v3_v3(val, vd.co);

					for (p= 0; p < proxy_count; p++)
						add_v3_v3(val, proxies[p].co[vd.i]);

					sculpt_clip(sd, ss, vd.co, val);
				}
				BLI_pbvh_vertex_iter_end;

				BLI_pbvh_node_free_proxies(nodes[n]);

			}

			break;

		case SCULPT_TOOL_SMOOTH:
		case SCULPT_TOOL_LAYER:
		default:
			break;
	}

	if (nodes)
		MEM_freeN(nodes);
}

static void sculpt_update_tex(Sculpt *sd, SculptSession *ss)
{
	Brush *brush = paint_brush(&sd->paint);
	const int radius= brush_size(brush);

	if(ss->texcache) {
		MEM_freeN(ss->texcache);
		ss->texcache= NULL;
	}

	/* Need to allocate a bigger buffer for bigger brush size */
	ss->texcache_side = 2*radius;
	if(!ss->texcache || ss->texcache_side > ss->texcache_actual) {
		ss->texcache = brush_gen_texture_cache(brush, radius);
		ss->texcache_actual = ss->texcache_side;
	}
}

void sculpt_update_mesh_elements(Scene *scene, Object *ob, int need_fmap)
{
	DerivedMesh *dm = mesh_get_derived_final(scene, ob, CD_MASK_BAREMESH);
	SculptSession *ss = ob->paint->sculpt;
	MultiresModifierData *mmd= ED_paint_multires_active(scene, ob);

	ob= ob;

	ss->modifiers_active= sculpt_modifiers_active(scene, ob);

	if((ob->shapeflag & OB_SHAPE_LOCK) && !mmd) ss->kb= ob_get_keyblock(ob);
	else ss->kb= NULL;

	if(mmd) {
		ss->multires = mmd;
		ss->totvert = dm->getNumVerts(dm);
		ss->totface = dm->getNumFaces(dm);
		ss->mvert= NULL;
		ss->mface= NULL;
		ss->face_normals= NULL;
	}
	else {
		Mesh *me = get_mesh(ob);
		ss->totvert = me->totvert;
		ss->totface = me->totface;
		ss->mvert = me->mvert;
		ss->mface = me->mface;
		ss->face_normals = NULL;
		ss->multires = NULL;
	}

	ob->paint->pbvh = dm->getPBVH(ob, dm);
	ss->fmap = (need_fmap && dm->getFaceMap)? dm->getFaceMap(ob, dm): NULL;

	/* if pbvh is deformed, key block is already applied to it */
	if (ss->kb && !BLI_pbvh_isDeformed(ob->paint->pbvh)) {
		float (*vertCos)[3]= key_to_vertcos(ob, ss->kb);

		if (vertCos) {
			/* apply shape keys coordinates to PBVH */
			BLI_pbvh_apply_vertCos(ob->paint->pbvh, vertCos);
			MEM_freeN(vertCos);
		}
	}
}

static int sculpt_mode_poll(bContext *C)
{
	Object *ob = CTX_data_active_object(C);
	return ob && ob->mode & OB_MODE_SCULPT;
}

int sculpt_poll(bContext *C)
{
	return sculpt_mode_poll(C) && paint_poll(C);
}

static char *sculpt_tool_name(Sculpt *sd)
{
	Brush *brush = paint_brush(&sd->paint);

	switch(brush->sculpt_tool) {
	case SCULPT_TOOL_DRAW:
		return "Draw Brush"; break;
	case SCULPT_TOOL_SMOOTH:
		return "Smooth Brush"; break;
	case SCULPT_TOOL_CREASE:
		return "Crease Brush"; break;
	case SCULPT_TOOL_BLOB:
		return "Blob Brush"; break;
	case SCULPT_TOOL_PINCH:
		return "Pinch Brush"; break;
	case SCULPT_TOOL_INFLATE:
		return "Inflate Brush"; break;
	case SCULPT_TOOL_GRAB:
		return "Grab Brush"; break;
	case SCULPT_TOOL_NUDGE:
		return "Nudge Brush"; break;
	case SCULPT_TOOL_THUMB:
		return "Thumb Brush"; break;
	case SCULPT_TOOL_LAYER:
		return "Layer Brush"; break;
	case SCULPT_TOOL_FLATTEN:
		return "Flatten Brush"; break;
	case SCULPT_TOOL_CLAY:
		return "Clay Brush"; break;
	case SCULPT_TOOL_CLAY_TUBES:
		return "Clay Tubes Brush"; break;
	case SCULPT_TOOL_FILL:
		return "Fill Brush"; break;
	case SCULPT_TOOL_SCRAPE:
		return "Scrape Brush"; break;
	default:
		return "Sculpting"; break;
	}
}

/**** Radial control ****/
static int sculpt_radial_control_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	Paint *p = paint_get_active(CTX_data_scene(C));
	Brush *brush = paint_brush(p);
	float col[4], tex_col[4];

	WM_paint_cursor_end(CTX_wm_manager(C), p->paint_cursor);
	p->paint_cursor = NULL;
	brush_radial_control_invoke(op, brush, 1);

	if((brush->flag & BRUSH_DIR_IN) && ELEM4(brush->sculpt_tool, SCULPT_TOOL_DRAW, SCULPT_TOOL_INFLATE, SCULPT_TOOL_CLAY, SCULPT_TOOL_PINCH))
		copy_v3_v3(col, brush->sub_col);
	else
		copy_v3_v3(col, brush->add_col);
	col[3]= 0.5f;
									    
	copy_v3_v3(tex_col, U.sculpt_paint_overlay_col);
	tex_col[3]= (brush->texture_overlay_alpha / 100.0f);

	RNA_float_set_array(op->ptr, "color", col);
	RNA_float_set_array(op->ptr, "texture_color", tex_col);

	return WM_radial_control_invoke(C, op, event);
}

static int sculpt_radial_control_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	int ret = WM_radial_control_modal(C, op, event);
	if(ret != OPERATOR_RUNNING_MODAL)
		paint_cursor_start(C, sculpt_poll);
	return ret;
}

static int sculpt_radial_control_exec(bContext *C, wmOperator *op)
{
	Brush *brush = paint_brush(&CTX_data_tool_settings(C)->sculpt->paint);
	int ret = brush_radial_control_exec(op, brush, 1);

	WM_event_add_notifier(C, NC_BRUSH|NA_EDITED, brush);

	return ret;
}

static void SCULPT_OT_radial_control(wmOperatorType *ot)
{
	WM_OT_radial_control_partial(ot);

	ot->name= "Sculpt Radial Control";
	ot->idname= "SCULPT_OT_radial_control";

	ot->invoke= sculpt_radial_control_invoke;
	ot->modal= sculpt_radial_control_modal;
	ot->exec= sculpt_radial_control_exec;
	ot->poll= sculpt_poll;

	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO|OPTYPE_BLOCKING;
}

/**** Operator for applying a stroke (various attributes including mouse path)
	  using the current brush. ****/

static void sculpt_cache_free(StrokeCache *cache)
{
	if(cache->face_norms)
		MEM_freeN(cache->face_norms);
	MEM_freeN(cache);
}

/* Initialize the stroke cache invariants from operator properties */
static void sculpt_update_cache_invariants(bContext* C, Sculpt *sd, SculptSession *ss, wmOperator *op, wmEvent *event)
{
	StrokeCache *cache = MEM_callocN(sizeof(StrokeCache), "stroke cache");
	Brush *brush = paint_brush(&sd->paint);
	ViewContext *vc = paint_stroke_view_context(op->customdata);
	Object *ob= CTX_data_active_object(C);
	ModifierData *md;
	int i;
	int mode;

	ss->cache = cache;
	ss->cache->stroke = op->customdata;

	/* Set scaling adjustment */
	ss->cache->scale[0] = 1.0f / ob->size[0];
	ss->cache->scale[1] = 1.0f / ob->size[1];
	ss->cache->scale[2] = 1.0f / ob->size[2];

	ss->cache->plane_trim_squared = brush->plane_trim * brush->plane_trim;

	/* Initialize mirror modifier clipping */

	ss->cache->flag = 0;

	for(md= ob->modifiers.first; md; md= md->next) {
		if(md->type==eModifierType_Mirror && (md->mode & eModifierMode_Realtime)) {
			const MirrorModifierData *mmd = (MirrorModifierData*) md;
			
			/* Mark each axis that needs clipping along with its tolerance */
			if(mmd->flag & MOD_MIR_CLIPPING) {
				ss->cache->flag |= CLIP_X << mmd->axis;
				if(mmd->tolerance > ss->cache->clip_tolerance[mmd->axis])
					ss->cache->clip_tolerance[mmd->axis] = mmd->tolerance;
			}
		}
	}

	mode = RNA_int_get(op->ptr, "mode");
	cache->invert = mode == WM_BRUSHSTROKE_INVERT;
	cache->alt_smooth = mode == WM_BRUSHSTROKE_SMOOTH;

	/* Alt-Smooth */
	if (ss->cache->alt_smooth) {
		Paint *p= &sd->paint;
		Brush *br;
		
		BLI_strncpy(cache->saved_active_brush_name, brush->id.name+2, sizeof(cache->saved_active_brush_name));

		br= (Brush *)find_id("BR", "Smooth");
		if(br) {
			paint_brush_set(p, br);
			brush = br;
		}
	}

	/* Truly temporary data that isn't stored in properties */

	cache->vc = vc;

	cache->brush = brush;

	viewvector(cache->vc->rv3d, cache->vc->rv3d->twmat[3], cache->true_view_normal);
	/* Initialize layer brush displacements and persistent coords */
	if(brush->sculpt_tool == SCULPT_TOOL_LAYER) {
		/* not supported yet for multires */
		if(!ss->multires && !ss->layer_co && (brush->flag & BRUSH_PERSISTENT)) {
			if(!ss->layer_co)
				ss->layer_co= MEM_mallocN(sizeof(float) * 3 * ss->totvert,
									   "sculpt mesh vertices copy");

			for(i = 0; i < ss->totvert; ++i)
				copy_v3_v3(ss->layer_co[i], ss->mvert[i].co);
		}
	}

	/* Make copies of the mesh vertex locations and normals for some tools */
	if(brush->flag & BRUSH_ANCHORED) {
		if(ss->face_normals) {
			float *fn = ss->face_normals;
			cache->face_norms= MEM_mallocN(sizeof(float) * 3 * ss->totface, "Sculpt face norms");
			for(i = 0; i < ss->totface; ++i, fn += 3)
				copy_v3_v3(cache->face_norms[i], fn);
		}

		cache->original = 1;
	}

	if(ELEM7(brush->sculpt_tool, SCULPT_TOOL_DRAW,  SCULPT_TOOL_CREASE, SCULPT_TOOL_BLOB, SCULPT_TOOL_LAYER, SCULPT_TOOL_INFLATE, SCULPT_TOOL_CLAY, SCULPT_TOOL_CLAY_TUBES))
		if(!(brush->flag & BRUSH_ACCUMULATE))
			cache->original = 1;

	//cache->last_rake[0] = sd->last_x;
	//cache->last_rake[1] = sd->last_y;

	cache->vertex_rotation= 0;
}

static void sculpt_update_brush_delta(Sculpt *sd, Object *ob, Brush *brush)
{
	SculptSession *ss = ob->paint->sculpt;
	StrokeCache *cache = ss->cache;
	int tool = brush->sculpt_tool;

	if(ELEM5(tool,
		 SCULPT_TOOL_GRAB, SCULPT_TOOL_NUDGE,
		 SCULPT_TOOL_CLAY_TUBES, SCULPT_TOOL_SNAKE_HOOK,
		 SCULPT_TOOL_THUMB)) {
		float grab_location[3], imat[4][4], delta[3];
		float mouse[2], location[3];

		paint_stroke_mouse_location(cache->stroke, mouse);
		paint_stroke_location(cache->stroke, location);

		if(paint_stroke_first_dab(cache->stroke)) {
			copy_v3_v3(cache->orig_grab_location,
				   location);
		}
		else if(tool == SCULPT_TOOL_SNAKE_HOOK)
			add_v3_v3(location, cache->grab_delta);

		/* compute 3d coordinate at same z from original location + mouse */
		initgrabz(cache->vc->rv3d,
			  cache->orig_grab_location[0],
			  cache->orig_grab_location[1],
			  cache->orig_grab_location[2]);

		window_to_3d_delta(cache->vc->ar, grab_location,
				   mouse[0], mouse[1]);

		/* compute delta to move verts by */
		if(!paint_stroke_first_dab(cache->stroke)) {
			switch(tool) {
			case SCULPT_TOOL_GRAB:
			case SCULPT_TOOL_THUMB:
				sub_v3_v3v3(delta, grab_location, cache->old_grab_location);
				invert_m4_m4(imat, ob->obmat);
				mul_mat3_m4_v3(imat, delta);
				add_v3_v3(cache->grab_delta, delta);
				break;
			case SCULPT_TOOL_CLAY_TUBES:
			case SCULPT_TOOL_NUDGE:
				sub_v3_v3v3(cache->grab_delta, grab_location, cache->old_grab_location);
				invert_m4_m4(imat, ob->obmat);
				mul_mat3_m4_v3(imat, cache->grab_delta);
				break;
			case SCULPT_TOOL_SNAKE_HOOK:
				sub_v3_v3v3(cache->grab_delta, grab_location, cache->old_grab_location);
				invert_m4_m4(imat, ob->obmat);
				mul_mat3_m4_v3(imat, cache->grab_delta);
				break;
			}
		}
		else {
			zero_v3(cache->grab_delta);
		}

		copy_v3_v3(cache->old_grab_location, grab_location);

		if(ELEM(tool, SCULPT_TOOL_GRAB, SCULPT_TOOL_THUMB)) {
			/* location stays the same for finding vertices in brush radius */
			copy_v3_v3(location, cache->orig_grab_location);
		}
	}
}

/* Initialize the stroke cache variants from operator properties */
static void sculpt_update_cache_variants(bContext *C, Sculpt *sd, SculptSession *ss, struct PaintStroke *stroke, PointerRNA *ptr)
{
	Object *ob = CTX_data_active_object(C);
	StrokeCache *cache = ss->cache;
	Brush *brush = paint_brush(&sd->paint);

	cache->pen_flip = RNA_boolean_get(ptr, "pen_flip");

	/* Truly temporary data that isn't stored in properties */

	sculpt_update_brush_delta(sd, ob, brush);

	if(brush->sculpt_tool == SCULPT_TOOL_ROTATE) {
		float mouse[2], initial_mouse[2];
		float dx, dy;

		paint_stroke_mouse_location(cache->stroke, mouse);
		paint_stroke_initial_mouse_location(cache->stroke, initial_mouse);

		dx = mouse[0] - initial_mouse[0];
		dy = mouse[1] - initial_mouse[1];

		cache->vertex_rotation = -atan2(dx, dy);
	}
}

static void sculpt_stroke_modifiers_check(bContext *C)
{
	Object *ob = CTX_data_active_object(C);

	if(ob->paint->sculpt->modifiers_active) {
		Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
		Brush *brush = paint_brush(&sd->paint);

		sculpt_update_mesh_elements(CTX_data_scene(C), ob, brush->sculpt_tool == SCULPT_TOOL_SMOOTH);
	}
}

static void sculpt_raycast_cb(PBVHNode *node, void *data_v, float* tmin)
{
	if (BLI_pbvh_node_get_tmin(node) < *tmin) {
		PaintStrokeRaycastData *data = data_v;
		SculptSession *ss = data->mode_data;
		float (*origco)[3]= NULL;

		if(data->original && ss->cache) {
			/* intersect with coordinates from before we started stroke */
			PBVHUndoNode *unode= pbvh_undo_get_node(node);
			origco= (unode)? pbvh_undo_node_co(unode): NULL;
		}

		if(BLI_pbvh_node_raycast(data->ob->paint->pbvh, node, origco, data->ray_start, data->ray_normal, &data->dist, NULL, NULL)) {
			data->hit = 1;
			*tmin = data->dist;
		}
	}
}

static int sculpt_stroke_get_location(bContext *C, struct PaintStroke *stroke, float out[3], float mouse[2])
{
 	ViewContext *vc = paint_stroke_view_context(stroke);
 	SculptSession *ss= vc->obact->paint->sculpt;
 	StrokeCache *cache= ss->cache;
	int original;

 	sculpt_stroke_modifiers_check(C);
	original = (cache)? cache->original: 0;
	
	return paint_stroke_get_location(C, stroke, sculpt_raycast_cb, ss,
					 out, mouse, original);	
}

static int sculpt_brush_stroke_init(bContext *C, ReportList *reports)
{
	Scene *scene= CTX_data_scene(C);
	Object *ob= CTX_data_active_object(C);
	Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
	SculptSession *ss = CTX_data_active_object(C)->paint->sculpt;
	Brush *brush = paint_brush(&sd->paint);

	if(ob_get_key(ob) && !(ob->shapeflag & OB_SHAPE_LOCK)) {
		BKE_report(reports, RPT_ERROR, "Shape key sculpting requires a locked shape.");
		return 0;
	}

	view3d_operator_needs_opengl(C);

	/* TODO: Shouldn't really have to do this at the start of every
	   stroke, but sculpt would need some sort of notification when
	   changes are made to the texture. */
	sculpt_update_tex(sd, ss);

	sculpt_update_mesh_elements(scene, ob, brush->sculpt_tool == SCULPT_TOOL_SMOOTH);

	return 1;
}

static void sculpt_restore_mesh(Sculpt *sd, Object *ob)
{
	Brush *brush = paint_brush(&sd->paint);

	/* Restore the mesh before continuing with anchored stroke */
	if((brush->flag & BRUSH_ANCHORED) ||
	   (brush->sculpt_tool == SCULPT_TOOL_GRAB && brush_use_size_pressure(brush)) ||
	   (brush->flag & BRUSH_RESTORE_MESH))
	{
		SculptSession *ss = ob->paint->sculpt;
		StrokeCache *cache = ss->cache;
		int i;

		PBVHNode **nodes;
		int n, totnode;

		BLI_pbvh_search_gather(ob->paint->pbvh, NULL, NULL, &nodes, &totnode);

		#pragma omp parallel for schedule(guided) if (sd->paint.flags & PAINT_USE_OPENMP)
		for(n=0; n<totnode; n++) {
			PBVHUndoNode *unode;
			
			unode= pbvh_undo_get_node(nodes[n]);
			if(unode) {
				PBVHVertexIter vd;

				BLI_pbvh_vertex_iter_begin(ob->paint->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
					copy_v3_v3(vd.co, pbvh_undo_node_co(unode)[vd.i]);
					if(vd.no) VECCOPY(vd.no, pbvh_undo_node_no(unode)[vd.i])
					else normal_short_to_float_v3(vd.fno, pbvh_undo_node_no(unode)[vd.i]);

					if(vd.mvert) vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
				}
				BLI_pbvh_vertex_iter_end;

				BLI_pbvh_node_mark_update(nodes[n]);
			}
		}

		if(ss->face_normals) {
			float *fn = ss->face_normals;
			for(i = 0; i < ss->totface; ++i, fn += 3)
				copy_v3_v3(fn, cache->face_norms[i]);
		}

		if(nodes)
			MEM_freeN(nodes);
	}
}

static void sculpt_flush_update(bContext *C)
{
	Object *ob = CTX_data_active_object(C);
	SculptSession *ss = ob->paint->sculpt;
	ARegion *ar = CTX_wm_region(C);
	MultiresModifierData *mmd = ss->multires;

	if(mmd)
		multires_mark_as_modified(ob);
	if(ob->derivedFinal) /* VBO no longer valid */
		GPU_drawobject_free(ob->derivedFinal);

	if(ss->modifiers_active) {
		DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
		ED_region_tag_redraw(ar);
	}
	else {
		BLI_pbvh_update(ob->paint->pbvh, PBVH_UpdateBB, NULL);

		paint_tag_partial_redraw(C, ob);
	}
}

static int sculpt_stroke_test_start(bContext *C, wmOperator *op, wmEvent *event)
{
	if(paint_stroke_over_mesh(C, op->customdata, event->x, event->y)) {
		Object *ob = CTX_data_active_object(C);
		SculptSession *ss = ob->paint->sculpt;
		Sculpt *sd = CTX_data_tool_settings(C)->sculpt;

		ED_view3d_init_mats_rv3d(ob, CTX_wm_region_view3d(C));

		sculpt_update_cache_invariants(C, sd, ss, op, event);

		pbvh_undo_push_begin(sculpt_tool_name(sd));

#ifdef _OPENMP
		/* If using OpenMP then create a number of threads two times the
		   number of processor cores.
		   Justification: Empirically I've found that two threads per
		   processor gives higher throughput. */
		if (sd->paint.flags & PAINT_USE_OPENMP) {
			int num_procs;

			num_procs = omp_get_num_procs();
			omp_set_num_threads(2*num_procs);
		}
#endif

		return 1;
	}
	else
		return 0;
}

static void sculpt_stroke_update_symmetry(bContext *C, struct PaintStroke *stroke,
					  char symm, char axis, float angle,
					  int mirror_symmetry_pass, int radial_symmetry_pass,
					  float (*symmetry_rot_mat)[4])
{	
	Object *ob = CTX_data_active_object(C);
	SculptSession *ss = ob->paint->sculpt;
	StrokeCache *cache = ss->cache;

	paint_flip_coord(cache->grab_delta_symmetry, cache->grab_delta, symm);
	paint_flip_coord(cache->view_normal, cache->true_view_normal, symm);

	copy_m4_m4(cache->symm_rot_mat, symmetry_rot_mat);

	mul_m4_v3(symmetry_rot_mat, cache->grab_delta_symmetry);
}

static void sculpt_stroke_update_step(bContext *C, struct PaintStroke *stroke, PointerRNA *itemptr)
{
	Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
	Object *ob = CTX_data_active_object(C);
	SculptSession *ss = ob->paint->sculpt;
	StrokeCache *cache = ss->cache;

	sculpt_stroke_modifiers_check(C);
	sculpt_update_cache_variants(C, sd, ss, stroke, itemptr);
	sculpt_restore_mesh(sd, ob);

	cache->bstrength= brush_strength(sd, cache, paint_stroke_feather(stroke));
	paint_stroke_apply_brush(C, stroke, &sd->paint);
	sculpt_combine_proxies(sd, ob);

	/* Cleanup */
	sculpt_flush_update(C);
}

static void sculpt_stroke_done(bContext *C, struct PaintStroke *unused)
{
	Object *ob= CTX_data_active_object(C);
	SculptSession *ss = ob->paint->sculpt;
	Sculpt *sd = CTX_data_tool_settings(C)->sculpt;

	(void)unused;

	/* Finished */
	if(ss->cache) {
		sculpt_stroke_modifiers_check(C);

		/* Alt-Smooth */
		if (ss->cache->alt_smooth) {
			Paint *p= &sd->paint;
			Brush *br= (Brush *)find_id("BR", ss->cache->saved_active_brush_name);
			if(br) {
				paint_brush_set(p, br);
			}
		}

		sculpt_cache_free(ss->cache);
		ss->cache = NULL;

		pbvh_undo_push_end();

		BLI_pbvh_update(ob->paint->pbvh, PBVH_UpdateOriginalBB, NULL);

		/* optimization: if there is locked key and active modifiers present in */
		/* the stack, keyblock is updating at each step. otherwise we could update */
		/* keyblock only when stroke is finished */
		if(ss->kb && !ss->modifiers_active) sculpt_update_keyblock(ob);

		ob->paint->partial_redraw = 0;

		/* try to avoid calling this, only for e.g. linked duplicates now */
		if(((Mesh*)ob->data)->id.us > 1)
			DAG_id_flush_update(&ob->id, OB_RECALC_DATA);

		WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, ob);
	}
}

static void sculpt_stroke_set_modifiers(PaintStroke *stroke, Brush *brush)
{
	if(ELEM(brush->sculpt_tool, SCULPT_TOOL_SNAKE_HOOK, SCULPT_TOOL_ROTATE))
		paint_stroke_set_modifier_use_original_location(stroke);

	if(ELEM(brush->sculpt_tool, SCULPT_TOOL_GRAB, SCULPT_TOOL_SNAKE_HOOK))
		paint_stroke_set_modifier_initial_radius_factor(stroke, 2.0f);

	if(ELEM4(brush->sculpt_tool, SCULPT_TOOL_GRAB, SCULPT_TOOL_SNAKE_HOOK,
		 SCULPT_TOOL_THUMB, SCULPT_TOOL_ROTATE))
		paint_stroke_set_modifier_use_original_texture_coords(stroke);
}

static int sculpt_brush_stroke_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
	struct PaintStroke *stroke;
	int ignore_background_click;

	if(!sculpt_brush_stroke_init(C, op->reports))
		return OPERATOR_CANCELLED;

	stroke = paint_stroke_new(C, sculpt_stroke_get_location,
				  sculpt_stroke_test_start,
				  sculpt_stroke_update_step,
				  sculpt_stroke_update_symmetry,
				  sculpt_stroke_brush_action,
				  sculpt_stroke_done);

	op->customdata = stroke;
	sculpt_stroke_set_modifiers(stroke, paint_brush(&sd->paint));

	/* For tablet rotation */
	ignore_background_click = RNA_boolean_get(op->ptr,
						  "ignore_background_click"); 

	if(ignore_background_click && !paint_stroke_over_mesh(C, stroke, event->x, event->y)) {
		paint_stroke_free(stroke);
		return OPERATOR_PASS_THROUGH;
	}
	
	/* add modal handler */
	WM_event_add_modal_handler(C, op);

	op->type->modal(C, op, event);
	
	return OPERATOR_RUNNING_MODAL;
}

static int sculpt_brush_stroke_exec(bContext *C, wmOperator *op)
{
	Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
	SculptSession *ss = CTX_data_active_object(C)->paint->sculpt;

	if(!sculpt_brush_stroke_init(C, op->reports))
		return OPERATOR_CANCELLED;

	op->customdata = paint_stroke_new(C, sculpt_stroke_get_location,
					  sculpt_stroke_test_start,
					  sculpt_stroke_update_step,
					  sculpt_stroke_update_symmetry,
					  sculpt_stroke_brush_action,
					  sculpt_stroke_done);

	sculpt_stroke_set_modifiers(op->customdata, paint_brush(&sd->paint));

	sculpt_update_cache_invariants(C, sd, ss, op, NULL);

	paint_stroke_exec(C, op);

	sculpt_flush_update(C);
	sculpt_cache_free(ss->cache);

	return OPERATOR_FINISHED;
}

static void SCULPT_OT_brush_stroke(wmOperatorType *ot)
{
	static EnumPropertyItem stroke_mode_items[] = {
		{ WM_BRUSHSTROKE_NORMAL, "NORMAL", 0, "Normal", "Apply brush normally" },
		{ WM_BRUSHSTROKE_INVERT, "INVERT", 0, "Invert", "Invert action of brush for duration of stroke" },
		{ WM_BRUSHSTROKE_SMOOTH, "SMOOTH", 0, "Smooth", "Switch brush to smooth mode for duration of stroke" },
		{ 0 }
	};

	/* identifiers */
	ot->name= "Sculpt Mode";
	ot->idname= "SCULPT_OT_brush_stroke";
	
	/* api callbacks */
	ot->invoke= sculpt_brush_stroke_invoke;
	ot->modal= paint_stroke_modal;
	ot->exec= sculpt_brush_stroke_exec;
	ot->poll= sculpt_poll;

	/* flags */
	ot->flag= OPTYPE_BLOCKING;

	/* properties */

	RNA_def_collection_runtime(ot->srna, "stroke", &RNA_OperatorStrokeElement,
			"Stroke", "");

	RNA_def_enum(ot->srna, "mode", stroke_mode_items, WM_BRUSHSTROKE_NORMAL, 
			"Sculpt Stroke Mode",
			"Action taken when a sculpt stroke is made");

	RNA_def_boolean(ot->srna, "ignore_background_click", 0,
			"Ignore Background Click",
			"Clicks on the background do not start the stroke");
}

static void sculpt_area_hide_update(bContext *C)
{
	Scene *scene = CTX_data_scene(C);
	Object *ob = CTX_data_active_object(C);
	DerivedMesh *dm;

	dm = mesh_get_derived_final(scene, ob, 0);
	/* Force the removal of the old pbvh */
	if(ob->paint->pbvh) {
		BLI_pbvh_free(ob->paint->pbvh);
		ob->paint->pbvh = NULL;
	}
	dm->getPBVH(NULL, dm);

	/* Update */
	sculpt_update_mesh_elements(scene, ob, 0);
}

static int sculpt_area_hide_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	int show_all = RNA_boolean_get(op->ptr, "show_all");

	if(show_all) {
		Object *ob = CTX_data_active_object(C);
		SculptSession *ss = ob->paint->sculpt;
		ARegion *ar= CTX_wm_region(C);

		if(ss->hidden_areas.first) {
			/* Free all hidden areas */
			BLI_freelistN(&ss->hidden_areas);
			sculpt_area_hide_update(C);

			/* Avoid cracks in multires */
			if(ss->multires) {
				BLI_pbvh_search_callback(ob->paint->pbvh, NULL, NULL,
							 BLI_pbvh_node_set_flags,
							 SET_INT_IN_POINTER(PBVH_UpdateAll));
				multires_stitch_grids(ob);
			}

			ED_region_tag_redraw(ar);
		}
		return OPERATOR_FINISHED;
	}
	else
		return WM_border_select_invoke(C, op, event);
}

static int sculpt_area_hide_exec(bContext *C, wmOperator *op)
{
	Object *ob = CTX_data_active_object(C);
	SculptSession *ss = ob->paint->sculpt;
	ViewContext vc;
	bglMats mats;
	BoundBox bb;
	rcti rect;
	HiddenArea *area;

	memset(&mats, 0, sizeof(bglMats));
	rect.xmin= RNA_int_get(op->ptr, "xmin");
	rect.ymin= RNA_int_get(op->ptr, "ymin");
	rect.xmax= RNA_int_get(op->ptr, "xmax");
	rect.ymax= RNA_int_get(op->ptr, "ymax");

	area = MEM_callocN(sizeof(HiddenArea), "hidden_area");
	area->hide_inside = RNA_boolean_get(op->ptr, "hide_inside");

	view3d_operator_needs_opengl(C);
	view3d_set_viewcontext(C, &vc);
	view3d_get_transformation(vc.ar, vc.rv3d, ob, &mats);
	view3d_calculate_clipping(&bb, area->clip_planes, &mats, &rect);
	mul_m4_fl(area->clip_planes, -1.0f);

	BLI_addtail(&ss->hidden_areas, area);

	sculpt_area_hide_update(C);

	return OPERATOR_FINISHED;
}

static void SCULPT_OT_area_hide(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Hide Area";
	ot->idname= "SCULPT_OT_area_hide";
	
	/* api callbacks */
	ot->invoke= sculpt_area_hide_invoke;
	ot->modal= WM_border_select_modal;
	ot->exec= sculpt_area_hide_exec;
	ot->poll= sculpt_mode_poll;
	
	ot->flag= OPTYPE_REGISTER;

	/* rna */
	RNA_def_boolean(ot->srna, "show_all", 0, "Show All", "");
	RNA_def_boolean(ot->srna, "hide_inside", 0, "Hide Inside", "");
	RNA_def_int(ot->srna, "xmin", 0, INT_MIN, INT_MAX, "X Min", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "xmax", 0, INT_MIN, INT_MAX, "X Max", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "ymin", 0, INT_MIN, INT_MAX, "Y Min", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "ymax", 0, INT_MIN, INT_MAX, "Y Max", "", INT_MIN, INT_MAX);
}

/**** Reset the copy of the mesh that is being sculpted on (currently just for the layer brush) ****/

static int sculpt_set_persistent_base(bContext *C, wmOperator *unused)
{
	SculptSession *ss = CTX_data_active_object(C)->paint->sculpt;

	(void)unused;

	if(ss) {
		if(ss->layer_co)
			MEM_freeN(ss->layer_co);
		ss->layer_co = NULL;
	}

	return OPERATOR_FINISHED;
}

static void SCULPT_OT_set_persistent_base(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Set Persistent Base";
	ot->idname= "SCULPT_OT_set_persistent_base";
	
	/* api callbacks */
	ot->exec= sculpt_set_persistent_base;
	ot->poll= sculpt_mode_poll;
	
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/**** Toggle operator for turning sculpt mode on or off ****/

static void sculpt_init_session(Scene *scene, Object *ob)
{
	create_paintsession(ob);

	ob->paint->sculpt = MEM_callocN(sizeof(SculptSession), "sculpt session");

	sculpt_update_mesh_elements(scene, ob, 0);
}

static int sculpt_toggle_mode(bContext *C, wmOperator *unused)
{
	Scene *scene = CTX_data_scene(C);
	ToolSettings *ts = CTX_data_tool_settings(C);
	Object *ob = CTX_data_active_object(C);
	MultiresModifierData *mmd= ED_paint_multires_active(scene, ob);
	int flush_recalc= 0;

	(void)unused;

	/* multires in sculpt mode could have different from object mode subdivision level */
	flush_recalc |= mmd && mmd->sculptlvl != mmd->lvl;
	/* if object has got active modifiers, it's dm could be different in sculpt mode  */
	//flush_recalc |= sculpt_modifiers_active(scene, ob);

	if(ob->mode & OB_MODE_SCULPT) {
		if(mmd)
			multires_force_update(ob);

		if(flush_recalc)
			DAG_id_flush_update(&ob->id, OB_RECALC_DATA);

		/* Leave sculptmode */
		ob->mode &= ~OB_MODE_SCULPT;

		free_paintsession(ob);
	}
	else {
		/* Enter sculptmode */
		ob->mode |= OB_MODE_SCULPT;

		if(flush_recalc)
			DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
		
		/* Create persistent sculpt mode data */
		if(!ts->sculpt) {
			ts->sculpt = MEM_callocN(sizeof(Sculpt), "sculpt mode data");

			/* Turn on X plane mirror symmetry by default */
			ts->sculpt->paint.flags |= PAINT_SYMM_X;
		}

		sculpt_init_session(scene, ob);

		paint_init(&ts->sculpt->paint, PAINT_CURSOR_SCULPT);
		
		paint_cursor_start(C, sculpt_poll);
	}

	WM_event_add_notifier(C, NC_SCENE|ND_MODE, CTX_data_scene(C));

	return OPERATOR_FINISHED;
}

static void SCULPT_OT_sculptmode_toggle(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Sculpt Mode";
	ot->idname= "SCULPT_OT_sculptmode_toggle";
	
	/* api callbacks */
	ot->exec= sculpt_toggle_mode;
	ot->poll= ED_operator_object_active;
	
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

void ED_operatortypes_sculpt()
{
	WM_operatortype_append(SCULPT_OT_radial_control);
	WM_operatortype_append(SCULPT_OT_brush_stroke);
	WM_operatortype_append(SCULPT_OT_sculptmode_toggle);
	WM_operatortype_append(SCULPT_OT_set_persistent_base);
	WM_operatortype_append(SCULPT_OT_area_hide);
}


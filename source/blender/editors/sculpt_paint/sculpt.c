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

#include "DNA_key_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_brush.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_DerivedMesh.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_image.h"
#include "BKE_key.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_multires.h"
#include "BKE_paint.h"
#include "BKE_report.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"
#include "BKE_colortools.h"

#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "WM_api.h"
#include "WM_types.h"
#include "ED_screen.h"
#include "ED_sculpt.h"
#include "ED_view3d.h"
#include "ED_mesh.h"
#include "paint_intern.h"
#include "sculpt_intern.h"

#include "RNA_access.h"
#include "RNA_define.h"


#include "RE_render_ext.h"
#include "RE_shader_ext.h"

#include "gpu_buffers.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifdef _OPENMP
#include <omp.h>
#endif

void ED_sculpt_force_update(bContext *C)
{
	Object *ob= CTX_data_active_object(C);

	if(ob && (ob->mode & OB_MODE_SCULPT))
		multires_force_update(ob);
}

/* Sculpt mode handles multires differently from regular meshes, but only if
   it's the last modifier on the stack and it is not on the first level */
struct MultiresModifierData *sculpt_multires_active(Scene *scene, Object *ob)
{
	ModifierData *md, *nmd;
	
	for(md= modifiers_getVirtualModifierList(ob); md; md= md->next) {
		if(md->type == eModifierType_Multires) {
			MultiresModifierData *mmd= (MultiresModifierData*)md;

			/* Check if any of the modifiers after multires are active
			 * if not it can use the multires struct */
			for(nmd= md->next; nmd; nmd= nmd->next)
				if(modifier_isEnabled(scene, nmd, eModifierMode_Realtime))
					break;

			if(!nmd && mmd->sculptlvl > 0)
				return mmd;
		}
	}

	return NULL;
}

/* Checks whether full update mode (slower) needs to be used to work with modifiers */
int sculpt_modifiers_active(Scene *scene, Object *ob)
{
	ModifierData *md;
	MultiresModifierData *mmd= sculpt_multires_active(scene, ob);

	/* check if there are any modifiers after what we are sculpting,
	   for a multires modifier with a deform modifier in front, we
	   do no need to recalculate the modifier stack. note that this
	   needs to be in sync with ccgDM_use_grid_pbvh! */
	if(mmd)
		md= mmd->modifier.next;
	else
		md= modifiers_getVirtualModifierList(ob);
	
	/* exception for shape keys because we can edit those */
	for(; md; md= md->next) {
		if(modifier_isEnabled(scene, md, eModifierMode_Realtime))
			if(md->type != eModifierType_ShapeKey)
				return 1;
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
	float initial_radius;
	float scale[3];
	int flag;
	float clip_tolerance[3];
	float initial_mouse[2];
	float detail;
	float smoothness;

	/* Variants */
	float radius;
	float true_location[3];
	float location[3];

	float pen_flip;
	float invert;
	float pressure;
	float mouse[2];
	float bstrength;
	float tex_mouse[2];
	int sculpt_plane;

	/* The rest is temporary storage that isn't saved as a property */

	int first_time; /* Beginning of stroke may do some things special */

	bglMats *mats;

	/* Clean this up! */
	ViewContext *vc;
	Brush *brush;

	float (*face_norms)[3]; /* Copy of the mesh faces' normals */
	float special_rotation; /* Texture rotation (radians) for anchored and rake modes */
	int pixel_radius, previous_pixel_radius;
	float grab_delta[3], grab_delta_symmetry[3];
	float old_grab_location[3], orig_grab_location[3];

	int symmetry; /* Symmetry index between 0 and 7 bit combo 0 is Brush only;
		1 is X mirror; 2 is Y mirror; 3 is XY; 4 is Z; 5 is XZ; 6 is YZ; 7 is XYZ */
	int symmetry_pass; /* the symmetry pass we are currently on between 0 and 7*/
	float view_normal[3], view_normal_symmetry[3];
	float last_area_normal[3];
	float last_view_normal_symmetry[3];
	float last_center[3];
	int last_rake[2]; /* Last location of updating rake rotation */
	int original;

	float vertex_rotation;

	char saved_active_brush_name[24];
	int alt_smooth;

} StrokeCache;

/* ===== OPENGL =====
 *
 * Simple functions to get data from the GL
 */

/* Convert a point in model coordinates to 2D screen coordinates. */
static void projectf(bglMats *mats, const float v[3], float p[2])
{
	double ux, uy, uz;

	gluProject(v[0],v[1],v[2], mats->modelview, mats->projection,
		   (GLint *)mats->viewport, &ux, &uy, &uz);
	p[0]= ux;
	p[1]= uy;
}

/*XXX: static void project(bglMats *mats, const float v[3], short p[2])
{
	float f[2];
	projectf(mats, v, f);

	p[0]= f[0];
	p[1]= f[1];
}
*/

/*** BVH Tree ***/

/* Get a screen-space rectangle of the modified area */
int sculpt_get_redraw_rect(ARegion *ar, RegionView3D *rv3d,
				Object *ob, rcti *rect)
{
	PBVH *pbvh= ob->sculpt->pbvh;
	float bb_min[3], bb_max[3], pmat[4][4];
	int i, j, k;

	view3d_get_object_project_mat(rv3d, ob, pmat);

	if(!pbvh)
		return 0;

	BLI_pbvh_redraw_BB(pbvh, bb_min, bb_max);

	rect->xmin = rect->ymin = INT_MAX;
	rect->xmax = rect->ymax = INT_MIN;

	if(bb_min[0] > bb_max[0] || bb_min[1] > bb_max[1] || bb_min[2] > bb_max[2])
		return 0;

	for(i = 0; i < 2; ++i) {
		for(j = 0; j < 2; ++j) {
			for(k = 0; k < 2; ++k) {
				float vec[3], proj[2];
				vec[0] = i ? bb_min[0] : bb_max[0];
				vec[1] = j ? bb_min[1] : bb_max[1];
				vec[2] = k ? bb_min[2] : bb_max[2];
				view3d_project_float(ar, vec, proj, pmat);
				rect->xmin = MIN2(rect->xmin, proj[0]);
				rect->xmax = MAX2(rect->xmax, proj[0]);
				rect->ymin = MIN2(rect->ymin, proj[1]);
				rect->ymax = MAX2(rect->ymax, proj[1]);
			}
		}
	}
	
	return rect->xmin < rect->xmax && rect->ymin < rect->ymax;
}

void sculpt_get_redraw_planes(float planes[4][4], ARegion *ar,
				  RegionView3D *rv3d, Object *ob)
{
	PBVH *pbvh= ob->sculpt->pbvh;
	BoundBox bb;
	bglMats mats;
	rcti rect;

	memset(&bb, 0, sizeof(BoundBox));

	view3d_get_transformation(ar, rv3d, ob, &mats);
	sculpt_get_redraw_rect(ar, rv3d,ob, &rect);

#if 1
	/* use some extra space just in case */
	rect.xmin -= 2;
	rect.xmax += 2;
	rect.ymin -= 2;
	rect.ymax += 2;
#else
	/* it was doing this before, allows to redraw a smaller
	   part of the screen but also gives artifaces .. */
	rect.xmin += 2;
	rect.xmax -= 2;
	rect.ymin += 2;
	rect.ymax -= 2;
#endif

	view3d_calculate_clipping(&bb, planes, &mats, &rect);
	mul_m4_fl(planes, -1.0f);

	/* clear redraw flag from nodes */
	if(pbvh)
		BLI_pbvh_update(pbvh, PBVH_UpdateRedraw, NULL);
}

/************************ Brush Testing *******************/

typedef struct SculptBrushTest {
	float radius_squared;
	float location[3];

	float dist;

	//float true_location[3];
	//int symmetry;
	//int symmetry_pass;
} SculptBrushTest;

static void sculpt_brush_test_init(SculptSession *ss, SculptBrushTest *test)
{
	test->radius_squared= ss->cache->radius*ss->cache->radius;
	copy_v3_v3(test->location, ss->cache->location);

	//copy_v3_v3(test->true_location, ss->cache->true_location);
	//test->symmetry = ss->cache->symmetry;
	//test->symmetry_pass = ss->cache->symmetry_pass;
}

//static int sculpt_brush_test_clip(SculptBrushTest* test, float co[3])
//{
//	if (test->symmetry) {
//		int i;
//
//		for (i = 0; i < 3; i++) {
//			if (test->symmetry_pass & (1<<i)) {
//				if (test->true_location[i] >= 0) {
//					if (co[i] >= 0) return 0;
//				}
//				else if (test->true_location[i] < 0) {
//					if (co[i] < 0) return 0;
//				}
//			}
//			else {
//				if (test->true_location[i] >= 0) {
//					if (co[i] < 0) return 0;
//				}
//				else if (test->true_location[i] < 0) {
//					if (co[i] >= 0) return 0;
//				}
//			}
//		}
//	}
//
//	return 1;
//}

static int sculpt_brush_test_cyl(SculptBrushTest *test, float x0[3], float x1[3], float x2[3])
{
	float t0[3], t1[3], t2[3], t3[3], dist;

	sub_v3_v3v3(t0, x2, x1);
	sub_v3_v3v3(t1, x1, x0);
	sub_v3_v3v3(t2, x2, x1);

	cross_v3_v3v3(t3, t0, t1);

	dist = len_v3(t3)/len_v3(t2);

	if (dist*dist < test->radius_squared) {
		test->dist = dist;
		return 1;
	}
	else {
		return 0;
	}
}

static int sculpt_brush_test(SculptBrushTest *test, float co[3])
{
	//if (sculpt_brush_test_clip(test, co)) {
		float distsq = len_squared_v3v3(co, test->location);

		if(distsq < test->radius_squared) {
			test->dist = sqrt(distsq);
			return 1;
		}
		else {
			return 0;
		}
	//}
	//else {
	//	return 0;
	//}
}

static int sculpt_brush_test_sq(SculptBrushTest *test, float co[3])
{
	//if (sculpt_brush_test_clip(test, co)) {
		float distsq = len_squared_v3v3(co, test->location);

		if(distsq < test->radius_squared) {
			test->dist = distsq;
			return 1;
		}
		else {
			return 0;
		}
	//}
	//else {
	//	return 0;
	//}
}

static int sculpt_brush_test_fast(SculptBrushTest *test, float co[3])
{
	//if (sculpt_brush_test_clip(test, co)) {
		return len_squared_v3v3(co, test->location) < test->radius_squared;
	//}
	//else {
	//	return 0;
	//}
}


/* ===== Sculpting =====
 *
 */
  
static void create_EditMesh_sculpt(SculptSession *ss) //Mio
{	
	EditEdge *eed;	
	Object *obedit= ss->ob;
	Mesh *me= obedit->data;
	EditMesh *em;

	int tempselectmode = ss->scene->toolsettings->selectmode; /* store temporal scene select mode*/
	ss->scene->toolsettings->selectmode = SCE_SELECT_VERTEX;	
	
	make_editMesh(ss->scene, obedit);						
	em = me->edit_mesh;	
	
	ss->scene->toolsettings->selectmode = tempselectmode; /* restore scene select mode*/	
		
	/*select all edges associated with every selected vertex*/
	for(eed= em->edges.first; eed; eed= eed->next){
		if(eed->v1->f&SELECT) eed->f1 = 1;
		else if(eed->v2->f&SELECT) eed->f1 = 1;
	}
			
	for(eed= em->edges.first; eed; eed= eed->next)
		if(eed->f1 == 1) EM_select_edge(eed,1);	
		
	ss->em = em;
}
 
static void unlimited_clay(SculptSession *ss, Object *ob)
{
	/*---- adaptive dynamic subdivission -- */
		float diff[3], edgeLength; 	
			
		if (ss->scene != NULL && ob != NULL) 
		{
			Object *obedit;
			Mesh *me;
			EditEdge *eed;
			float detail;
			float smoothness;

			create_EditMesh_sculpt(ss);
			obedit= ob;
			me= obedit->data;
			detail = ss->cache->detail * ss->cache->radius;
			smoothness = ss->cache->smoothness;
			 
			for(eed = me->edit_mesh->edges.first; eed; eed = eed->next){
				if (eed->f & SELECT)
				{
					float detail;

					sub_v3_v3v3(diff, eed->v1->co, eed->v2->co);
					edgeLength = len_v3(diff);
					
					detail = ss->cache->detail * ss->cache->radius;  
															
					if (edgeLength < detail)
						EM_select_edge(eed, 0);	
					else	
						EM_select_edge(eed, 1);			
				
				}									
					
			}									
			esubdivideflag(obedit, me->edit_mesh, SELECT,smoothness,0,B_SMOOTH,1, SUBDIV_CORNER_PATH, SUBDIV_SELECT_INNER);			
			
			/* Clear selection */
			for(eed = me->edit_mesh->edges.first; eed; eed = eed->next)
				EM_select_edge(eed, 0);	 
				
			load_editMesh(ss->scene, ob);
			
			free_editMesh(me->edit_mesh);
			DAG_id_flush_update(ob->data, OB_RECALC_DATA); //?					
		} 	

}

/* area of overlap of two circles of radius 1 seperated by d units from their centers */
static float circle_overlap(float d)
{
	return (2*acos(d/2)-(1/2)*d*sqrt(4-d*d));
}

/* percentage of overlap of two circles of radius 1 seperated by d units from their centers */
static float circle_overlap_percent(float d)
{
	return circle_overlap(d) / M_PI;
}

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
static float brush_strength(Sculpt *sd, StrokeCache *cache)
{
	Brush *brush = paint_brush(&sd->paint);

	/* Primary strength input; square it to make lower values more sensitive */
	float alpha        = brush->alpha * brush->alpha * brush->strength_multiplier;
	float dir          = brush->flag & BRUSH_DIR_IN ? -1 : 1;
	float pressure     = brush->flag & BRUSH_ALPHA_PRESSURE	? cache->pressure : 1;
	float pen_flip     = cache->pen_flip ? -1 : 1;
	float invert       = cache->invert ? -1 : 1;
	float accum        = integrate_overlap(brush);
	float overlap      = (brush->flag & BRUSH_SPACE_ATTEN && brush->flag & BRUSH_SPACE && !(brush->flag & BRUSH_ANCHORED)) && (brush->spacing < 100) ? 1.0f/accum : 1; // spacing is integer percentage of radius, divide by 50 to get normalized diameter
	//float overlap      = (brush->flag & BRUSH_SPACE && !(brush->flag & BRUSH_ANCHORED)) && (brush->spacing < 100.0f) ? 1 - circle_overlap_percent(brush->spacing/50.0f) : 1; // spacing is integer percentage of radius, divide by 50 to get normalized diameter
	//float overlap      = (brush->flag & BRUSH_SPACE_ATTEN && brush->flag & BRUSH_SPACE && !(brush->flag & BRUSH_ANCHORED)) && (brush->spacing < 100) ? (float)brush->spacing/100.0f : 1; // spacing is integer percentage of radius, divide by 50 to get normalized diameter
	//float overlap      = (brush->flag & BRUSH_SPACE_ATTEN && brush->flag & BRUSH_SPACE && !(brush->flag & BRUSH_ANCHORED)) && (brush->spacing < 50) ? (float)brush->spacing/50.0f : 1; // spacing is integer percentage of radius, divide by 50 to get normalized diameter
	float flip         = dir * invert * pen_flip;

	// XXX not functionally cohesive to update this here
	brush->autosmooth_overlap = overlap;

	switch(brush->sculpt_tool){
		case SCULPT_TOOL_CLAY:
		case SCULPT_TOOL_DRAW:
		case SCULPT_TOOL_WAX:
		case SCULPT_TOOL_LAYER:
			return alpha * flip * pressure * overlap;

		case SCULPT_TOOL_CREASE:
		case SCULPT_TOOL_BLOB:
			return alpha * flip * pressure * overlap;

		case SCULPT_TOOL_INFLATE:
			if (flip > 0) {
				return 0.250f * alpha * flip * pressure * overlap;
			}
			else {
				return 0.125f * alpha * flip * pressure * overlap;
			}

		case SCULPT_TOOL_FILL:
		case SCULPT_TOOL_SCRAPE:
		case SCULPT_TOOL_FLATTEN:
			if (flip > 0) {
				overlap = (1+overlap) / 2;
				return alpha * flip * pressure * overlap;
			}
			else {
				/* reduce strength for DEEPEN, PEAKS, and CONTRAST */
				return 0.5f * alpha * flip * pressure * overlap; 
			}

		case SCULPT_TOOL_SMOOTH:
			return alpha * pressure;

		case SCULPT_TOOL_PINCH:
			if (flip > 0) {
				return alpha * flip * pressure * overlap;
			}
			else {
				return 0.25f * alpha * flip * pressure * overlap;
			}

		case SCULPT_TOOL_NUDGE:
			overlap = (1+overlap) / 2;
			return alpha * pressure * overlap;

		case SCULPT_TOOL_THUMB:
			return alpha*pressure / 4;

		case SCULPT_TOOL_SNAKE_HOOK:
			return 1.0f;

		case SCULPT_TOOL_GRAB:
		case SCULPT_TOOL_ROTATE:
			/* rotate ignores strength */
			return 1.0f;

		default:
			return 0;
	}
}

/* Uses symm to selectively flip any axis of a coordinate. */
static void flip_coord(float out[3], float in[3], const char symm)
{
	if(symm & SCULPT_SYMM_X)
		out[0]= -in[0];
	else
		out[0]= in[0];
	if(symm & SCULPT_SYMM_Y)
		out[1]= -in[1];
	else
		out[1]= in[1];
	if(symm & SCULPT_SYMM_Z)
		out[2]= -in[2];
	else
		out[2]= in[2];
}

float get_tex_pixel(Brush* br, float u, float v)
{
	TexResult texres;
	float co[3];
	int hasrgb;

	co[0] = u;
	co[1] = v;
	co[2] = 0;

	memset(&texres, 0, sizeof(TexResult));
	hasrgb = multitex_ext(br->mtex.tex, co, NULL, NULL, 1, &texres);

	if (hasrgb & TEX_RGB)
		texres.tin = (0.35*texres.tr + 0.45*texres.tg + 0.2*texres.tb)*texres.ta;

	return texres.tin;
}

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

/* Return a multiplier for brush strength on a particular vertex. */
static float tex_strength(SculptSession *ss, Brush *br, float *point, const float len)
{
	MTex *mtex = &br->mtex;
	float avg= 1;

	if(!mtex->tex) {
		avg= 1;
	}
	else if(mtex->brush_map_mode == MTEX_MAP_MODE_3D) {
		float jnk;

		/* Get strength by feeding the vertex 
		   location directly into a texture */
		externtex(mtex, point, &avg,
			  &jnk, &jnk, &jnk, &jnk);
	}
	else if(ss->texcache) {
		float rotation = -mtex->rot;
		float x, y, point_2d[3];
		float diameter;

		/* if the active area is being applied for symmetry, flip it
		   across the symmetry axis in order to project it. This insures
		   that the brush texture will be oriented correctly. */
		flip_coord(point_2d, point, ss->cache->symmetry_pass);
		projectf(ss->cache->mats, point_2d, point_2d);

		/* if fixed mode, keep coordinates relative to mouse */
		if(mtex->brush_map_mode == MTEX_MAP_MODE_FIXED) {
			rotation += ss->cache->special_rotation;

			point_2d[0] -= ss->cache->tex_mouse[0];
			point_2d[1] -= ss->cache->tex_mouse[1];

			diameter = ss->cache->pixel_radius; // use pressure adjusted size for fixed mode

			x = point_2d[0];
			y = point_2d[1];
		}
		else /* else (mtex->brush_map_mode == MTEX_MAP_MODE_TILED),
		        leave the coordinates relative to the screen */
		{
			diameter = br->size; // use unadjusted size for tiled mode
		
			x = point_2d[0] - ss->cache->vc->ar->winrct.xmin;
			y = point_2d[1] - ss->cache->vc->ar->winrct.ymin;
		}

		x /= ss->cache->vc->ar->winx;
		y /= ss->cache->vc->ar->winy;

		if (mtex->brush_map_mode == MTEX_MAP_MODE_TILED) {
			x -= 0.5f;
			y -= 0.5f;
		}
		
		x *= ss->cache->vc->ar->winx / diameter;
		y *= ss->cache->vc->ar->winy / diameter;

		/* it is probably worth optimizing for those cases where 
		   the texture is not rotated by skipping the calls to
		   atan2, sqrtf, sin, and cos. */
		if (rotation > 0.001 || rotation < -0.001) {
			const float angle    = atan2(y, x) + rotation;
			const float flen     = sqrtf(x*x + y*y);

			x = flen * cos(angle);
			y = flen * sin(angle);
		}

		x *= br->mtex.size[0];
		y *= br->mtex.size[1];

		x += br->mtex.ofs[0];
		y += br->mtex.ofs[1];

		avg = get_tex_pixel(br, x, y);
	}

	avg += br->texture_sample_bias;

	avg *= brush_curve_strength(br, len, ss->cache->radius); /* Falloff curve */

	return avg;
}

typedef struct {
	Sculpt *sd;
	SculptSession *ss;
	float radius_squared;
	int original;
} SculptSearchSphereData;

/* Test AABB against sphere */
static int sculpt_search_sphere_cb(PBVHNode *node, void *data_v)
{
	SculptSearchSphereData *data = data_v;
	float *center = data->ss->cache->location, nearest[3];
	float t[3], bb_min[3], bb_max[3];
	int i;

	if(data->original)
		BLI_pbvh_node_get_original_BB(node, bb_min, bb_max);
	else
		BLI_pbvh_node_get_BB(node, bb_min, bb_max);
	
	for(i = 0; i < 3; ++i) {
		if(bb_min[i] > center[i])
			nearest[i] = bb_min[i];
		else if(bb_max[i] < center[i])
			nearest[i] = bb_max[i];
		else
			nearest[i] = center[i]; 
	}
	
	sub_v3_v3v3(t, center, nearest);

	return dot_v3v3(t, t) < data->radius_squared;
}

//static void symmetry_feather(Sculpt *sd, SculptSession* ss, float co[3], float val[3])
//{
//	if (ss->cache->symmetry && (sd->flags & SCULPT_SYMMETRY_FEATHER)) {
//		float distsq;
//		float mirror[3];
//		float overlap = 1.0f;
//		int symm = ss->cache->symmetry;
//		int i;
//
//		for (i = 1; i < symm; i++) {
//			if(symm & i && (symm != 5 || i != 3) && (symm != 6 || (i != 3 && i != 5))) {
//				flip_coord(mirror, ss->cache->location, i);
//				distsq = len_squared_v3v3(mirror, co);
//
//				if (distsq < ss->cache->radius*ss->cache->radius) overlap += 1 - sqrt(distsq)/ss->cache->radius;
//			}
//		}
//
//		mul_v3_fl(val, 1.0f / overlap);
//	}
//}

/* Handles clipping against a mirror modifier and SCULPT_LOCK axis flags */
static void sculpt_clip(Sculpt *sd, SculptSession *ss, float *co, const float val[3])
{
	int i;

	for(i=0; i<3; ++i) {
		if(sd->flags & (SCULPT_LOCK_X << i))
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

/* This initializes the faces to be moved for this sculpt for draw/layer/flatten; then it
 finds average normal for all active vertices - note that this is called once for each mirroring direction */
static void calc_area_normal(Sculpt *sd, SculptSession *ss, float an[3], PBVHNode **nodes, int totnode)
{
	Brush *brush = paint_brush(&sd->paint);

	if (ss->cache->symmetry_pass == 0 &&
	   (ss->cache->first_time || !(brush->flag & BRUSH_ORIGINAL_NORMAL)))
	{
		//int sculpt_plane = (brush->flag & BRUSH_ORIGINAL_NORMAL) ? brush->sculpt_plane : SCULPT_DISP_DIR_AREA;
		int sculpt_plane = brush->sculpt_plane;

		switch (sculpt_plane) {
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
				/* this calculates flatten center and area normal together, 
				amortizing the memory bandwidth and loop overhead to calculate both at the same time */
				{
					int n;

					float out_flip[3] = {0.0f, 0.0f, 0.0f};

					zero_v3(an);

					#pragma omp parallel for schedule(guided) if (sd->flags & SCULPT_USE_OPENMP)
					for(n=0; n<totnode; n++) {
						PBVHVertexIter vd;
						SculptBrushTest test;
						SculptUndoNode *unode;
						float private_an[3] = {0.0f, 0.0f, 0.0f};
						float private_out_flip[3] = {0.0f, 0.0f, 0.0f};
						float private_fc[3] = {0.0f, 0.0f, 0.0f};
						int private_count = 0;

						unode = sculpt_undo_push_node(ss, nodes[n]);
						sculpt_brush_test_init(ss, &test);

						if(ss->cache->original) {
							BLI_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
								if(sculpt_brush_test_fast(&test, unode->co[vd.i])) {
									float fno[3];

									normal_short_to_float_v3(fno, unode->no[vd.i]);
									add_norm_if(ss->cache->view_normal_symmetry, private_an, private_out_flip, fno);
								}
							}
							BLI_pbvh_vertex_iter_end;
						}
						else {
							BLI_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
								if(sculpt_brush_test_fast(&test, vd.co)) {
									if(vd.no) {
										float fno[3];

										normal_short_to_float_v3(fno, vd.no);
										add_norm_if(ss->cache->view_normal_symmetry, private_an, private_out_flip, fno);
									}
									else {
										add_norm_if(ss->cache->view_normal_symmetry, private_an, private_out_flip, vd.fno);
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

			default:
				break;
		}

		copy_v3_v3(ss->cache->last_area_normal, an);
		copy_v3_v3(ss->cache->last_view_normal_symmetry, ss->cache->view_normal_symmetry);
	}
	else {
		copy_v3_v3(an, ss->cache->last_area_normal);
		flip_coord(an, an, ss->cache->symmetry_pass);

		copy_v3_v3(ss->cache->view_normal_symmetry, ss->cache->last_view_normal_symmetry);
		flip_coord(ss->cache->view_normal_symmetry, ss->cache->view_normal_symmetry, ss->cache->symmetry_pass);
	}
}

/* For the smooth brush, uses the neighboring vertices around vert to calculate
   a smoothed location for vert. Skips corner vertices (used by only one
   polygon.) */
static void neighbor_average(SculptSession *ss, float avg[3], const unsigned vert)
{
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

static void do_mesh_smooth_brush(Sculpt *sd, SculptSession *ss, PBVHNode *node, float bstrength)
{
	Brush *brush = paint_brush(&sd->paint);
	PBVHVertexIter vd;
	SculptBrushTest test;
	float (*proxy)[3];

	proxy= BLI_pbvh_node_add_proxy(ss->pbvh, node)->co;
	
	CLAMP(bstrength, 0.0f, 1.0f);

	sculpt_brush_test_init(ss, &test);

	BLI_pbvh_vertex_iter_begin(ss->pbvh, node, vd, PBVH_ITER_UNIQUE) {
		if(sculpt_brush_test(&test, vd.co)) {
			const float fade = tex_strength(ss, brush, vd.co, test.dist)*bstrength;
			float avg[3], val[3];

			neighbor_average(ss, avg, vd.vert_indices[vd.i]);
			sub_v3_v3v3(val, avg, vd.co);
			mul_v3_fl(val, fade);

			add_v3_v3(val, vd.co);

			sculpt_clip(sd, ss, vd.co, val);

			if(vd.mvert) {
					vd.mvert->flag |= ME_VERT_PBVH_UPDATE;

					// unlimited clay
					//if(brush->flag & BRUSH_SUBDIV)
					//	vd.mvert->flag = 1;
			}
		}
	}
	BLI_pbvh_vertex_iter_end;
}

static void do_multires_smooth_brush(Sculpt *sd, SculptSession *ss, PBVHNode *node, float bstrength)
{
	Brush *brush = paint_brush(&sd->paint);
	SculptBrushTest test;
	DMGridData **griddata, *data;
	DMGridAdjacency *gridadj, *adj;
	float (*tmpgrid)[3], (*tmprow)[3];
	int v1, v2, v3, v4;
	int *grid_indices, totgrid, gridsize, i, x, y;

	sculpt_brush_test_init(ss, &test);

	CLAMP(bstrength, 0.0f, 1.0f);

	BLI_pbvh_node_get_grids(ss->pbvh, node, &grid_indices, &totgrid,
		NULL, &gridsize, &griddata, &gridadj);

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
			add_v3_v3v3(tmprow[0], data[v1].co, data[v1+gridsize].co);

			for (x= 0; x < gridsize-1; x++) {
				v1 = x + y*gridsize;
				v2 = v1 + 1;
				v3 = v1 + gridsize;
				v4 = v3 + 1;

				add_v3_v3v3(tmprow[x+1], data[v2].co, data[v4].co);
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

				if(x == 0 && adj->index[0] == -1)
					continue;

				if(x == gridsize - 1 && adj->index[2] == -1)
					continue;

				if(y == 0 && adj->index[3] == -1)
					continue;

				if(y == gridsize - 1 && adj->index[1] == -1)
					continue;

				co = data[x + y*gridsize].co;

				if(sculpt_brush_test(&test, co)) {
					const float fade = tex_strength(ss, brush, co, test.dist)*bstrength;
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

static void smooth(Sculpt *sd, SculptSession *ss, PBVHNode **nodes, int totnode, float bstrength)
{
	const int max_iterations = 4;
	const float fract = 1.0f/max_iterations;
	int iteration, n, count;
	float last;

	count = (int)(bstrength*max_iterations);
	last  = max_iterations*(bstrength - count*fract);

	for(iteration = 1; iteration <= count; ++iteration) {
		#pragma omp parallel for schedule(guided) if (sd->flags & SCULPT_USE_OPENMP)
		for(n=0; n<totnode; n++) {
			if(ss->multires) {
				do_multires_smooth_brush(sd, ss, nodes[n], iteration != count ? 1.0f : last);
			}
			else if(ss->fmap)
				do_mesh_smooth_brush(sd, ss, nodes[n], iteration != count ? 1.0f : last);
		}

		if(ss->multires)
			multires_stitch_grids(ss->ob);
	}
}

static void do_smooth_brush(Sculpt *sd, SculptSession *ss, PBVHNode **nodes, int totnode)
{
	smooth(sd, ss, nodes, totnode, ss->cache->bstrength);
}

static void do_draw_brush(Sculpt *sd, SculptSession *ss, PBVHNode **nodes, int totnode)
{
	Brush *brush = paint_brush(&sd->paint);
	float offset[3], area_normal[3];
	float bstrength= ss->cache->bstrength;
	int n;
	//float p[3];

	calc_area_normal(sd, ss, area_normal, nodes, totnode);
	
	/* offset with as much as possible factored in already */
	mul_v3_v3fl(offset, area_normal, ss->cache->radius);
	mul_v3_v3(offset, ss->cache->scale);
	mul_v3_fl(offset, bstrength);

	//add_v3_v3v3(p, ss->cache->location, area_normal);

	/* threaded loop over nodes */
	#pragma omp parallel for schedule(guided) if (sd->flags & SCULPT_USE_OPENMP)
	for(n=0; n<totnode; n++) {
		PBVHVertexIter vd;
		SculptBrushTest test;
		float (*proxy)[3];

		proxy= BLI_pbvh_node_add_proxy(ss->pbvh, nodes[n])->co;

		sculpt_brush_test_init(ss, &test);

		BLI_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
			if(sculpt_brush_test(&test, vd.co)) {
			//if(sculpt_brush_test_cyl(&test, vd.co, ss->cache->location, p)) {
				/* offset vertex */
				const float fade = tex_strength(ss, brush, vd.co, test.dist);

				mul_v3_v3fl(proxy[vd.i], offset, fade);

				if(vd.mvert) {
					vd.mvert->flag |= ME_VERT_PBVH_UPDATE;

					// XXX unlimited clay
					//if(brush->flag & BRUSH_SUBDIV)
					//	vd.mvert->flag = 1; 
				}
			}
		}
		BLI_pbvh_vertex_iter_end;
	}
}

static void do_crease_brush(Sculpt *sd, SculptSession *ss, PBVHNode **nodes, int totnode)
{
	Brush *brush = paint_brush(&sd->paint);
	float offset[3], area_normal[3];
	float bstrength= ss->cache->bstrength;
	float flippedbstrength, crease_correction;
	int n;
	
	calc_area_normal(sd, ss, area_normal, nodes, totnode);
	
	/* offset with as much as possible factored in already */
	mul_v3_v3fl(offset, area_normal, ss->cache->radius);
	mul_v3_v3(offset, ss->cache->scale);
	mul_v3_fl(offset, bstrength);
	
	/* we divide out the squared alpha and multiply by the squared crease to give us the pinch strength */
	
	if(brush->alpha > 0.0f)
		crease_correction = brush->crease_pinch_factor*brush->crease_pinch_factor/(brush->alpha*brush->alpha);
	else
		crease_correction = brush->crease_pinch_factor*brush->crease_pinch_factor;

	/* we always want crease to pinch or blob to relax even when draw is negative */
	flippedbstrength = (bstrength < 0) ? -crease_correction*bstrength : crease_correction*bstrength;

	if(brush->sculpt_tool == SCULPT_TOOL_BLOB) flippedbstrength *= -1.0f;

	/* threaded loop over nodes */
	#pragma omp parallel for schedule(guided) if (sd->flags & SCULPT_USE_OPENMP)
	for(n=0; n<totnode; n++) {
		PBVHVertexIter vd;
		SculptBrushTest test;
		float (*proxy)[3];

		proxy= BLI_pbvh_node_add_proxy(ss->pbvh, nodes[n])->co;

		sculpt_brush_test_init(ss, &test);

		BLI_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
			if(sculpt_brush_test(&test, vd.co)) {
				/* offset vertex */
				const float fade = tex_strength(ss, brush, vd.co, test.dist);
				float val1[3];
				float val2[3];

				/* first we pinch */
				sub_v3_v3v3(val1, test.location, vd.co);
				//mul_v3_v3(val1, ss->cache->scale);
				mul_v3_fl(val1, fade*flippedbstrength);

				/* then we draw */
				mul_v3_v3fl(val2, offset, fade);

				add_v3_v3v3(proxy[vd.i], val1, val2);

				if(vd.mvert) {
					vd.mvert->flag |= ME_VERT_PBVH_UPDATE;

					// unlimited clay
					//if(brush->flag & BRUSH_SUBDIV)
					//	vd.mvert->flag = 1; 
				}
			}
		}
		BLI_pbvh_vertex_iter_end;
	}
}

static void do_pinch_brush(Sculpt *sd, SculptSession *ss, PBVHNode **nodes, int totnode)
{
	Brush *brush = paint_brush(&sd->paint);
	float bstrength= ss->cache->bstrength;
	int n;

	#pragma omp parallel for schedule(guided) if (sd->flags & SCULPT_USE_OPENMP)
	for(n=0; n<totnode; n++) {
		PBVHVertexIter vd;
		SculptBrushTest test;
		float (*proxy)[3];

		proxy= BLI_pbvh_node_add_proxy(ss->pbvh, nodes[n])->co;

		sculpt_brush_test_init(ss, &test);

		BLI_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
			if(sculpt_brush_test(&test, vd.co)) {
				const float fade = tex_strength(ss, brush, vd.co, test.dist)*bstrength;
				float val[3];

				sub_v3_v3v3(val, test.location, vd.co);
				//mul_v3_v3(val, ss->cache->scale);
				mul_v3_v3fl(proxy[vd.i], val, fade);

				if(vd.mvert) {
					vd.mvert->flag |= ME_VERT_PBVH_UPDATE;

					// unlimited clay
					//if(brush->flag & BRUSH_SUBDIV)
					//	vd.mvert->flag = 1; 
				}
			}
		}
		BLI_pbvh_vertex_iter_end;
	}
}

static void do_grab_brush(Sculpt *sd, SculptSession *ss, PBVHNode **nodes, int totnode)
{
	Brush *brush= paint_brush(&sd->paint);
	float grab_delta[3], an[3];
	int n;
	float len;

	if (brush->normal_weight > 0)
		calc_area_normal(sd, ss, an, nodes, totnode);

	copy_v3_v3(grab_delta, ss->cache->grab_delta_symmetry);

	len = len_v3(grab_delta);

	if (brush->normal_weight > 0) {
		mul_v3_fl(an, len*brush->normal_weight);
		mul_v3_fl(grab_delta, 1.0f - brush->normal_weight);
		add_v3_v3(grab_delta, an);
	}

	#pragma omp parallel for schedule(guided) if (sd->flags & SCULPT_USE_OPENMP)
	for(n=0; n<totnode; n++) {
		PBVHVertexIter vd;
		SculptBrushTest test;
		float (*origco)[3];
		float (*proxy)[3];
	
		origco= sculpt_undo_push_node(ss, nodes[n])->co;

		proxy= BLI_pbvh_node_add_proxy(ss->pbvh, nodes[n])->co;

		sculpt_brush_test_init(ss, &test);

		BLI_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
			if(sculpt_brush_test(&test, origco[vd.i])) {
				const float fade = tex_strength(ss, brush, origco[vd.i], test.dist);

				mul_v3_v3fl(proxy[vd.i], grab_delta, fade);

				if(vd.mvert) {
					vd.mvert->flag |= ME_VERT_PBVH_UPDATE;

					// XXX unlimited clay
					//if(brush->flag & BRUSH_SUBDIV)
					//	vd.mvert->flag = 1; 
				}
			}
		}
		BLI_pbvh_vertex_iter_end;
	}
}

static void do_nudge_brush(Sculpt *sd, SculptSession *ss, PBVHNode **nodes, int totnode)
{
	Brush *brush = paint_brush(&sd->paint);
	float bstrength = ss->cache->bstrength;
	float grab_delta[3];
	int n;
	float an[3];
	float tmp[3], cono[3];

	copy_v3_v3(grab_delta, ss->cache->grab_delta_symmetry);

	calc_area_normal(sd, ss, an, nodes, totnode);

	cross_v3_v3v3(tmp, an, grab_delta);
	cross_v3_v3v3(cono, tmp, an);

	#pragma omp parallel for schedule(guided) if (sd->flags & SCULPT_USE_OPENMP)
	for(n = 0; n < totnode; n++) {
		PBVHVertexIter vd;
		SculptBrushTest test;
		float (*proxy)[3];

		proxy= BLI_pbvh_node_add_proxy(ss->pbvh, nodes[n])->co;

		sculpt_brush_test_init(ss, &test);

		BLI_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
			if(sculpt_brush_test(&test, vd.co)) {
				const float fade = bstrength*tex_strength(ss, brush, vd.co, test.dist);

				mul_v3_v3fl(proxy[vd.i], cono, fade);

				if(vd.mvert) {
					vd.mvert->flag |= ME_VERT_PBVH_UPDATE;

					// unlimited clay
					//if(brush->flag & BRUSH_SUBDIV)
					//	vd.mvert->flag = 1; 
				}
			}
		}
		BLI_pbvh_vertex_iter_end;
	}
}

static void do_snake_hook_brush(Sculpt *sd, SculptSession *ss, PBVHNode **nodes, int totnode)
{
	Brush *brush = paint_brush(&sd->paint);
	float bstrength = ss->cache->bstrength;
	float grab_delta[3], an[3];
	int n;
	float len;

	if (brush->normal_weight > 0)
		calc_area_normal(sd, ss, an, nodes, totnode);

	copy_v3_v3(grab_delta, ss->cache->grab_delta_symmetry);

	len = len_v3(grab_delta);

	if (bstrength < 0)
		negate_v3(grab_delta);

	if (brush->normal_weight > 0) {
		mul_v3_fl(an, len*brush->normal_weight);
		mul_v3_fl(grab_delta, 1.0f - brush->normal_weight);
		add_v3_v3(grab_delta, an);
	}

	#pragma omp parallel for schedule(guided) if (sd->flags & SCULPT_USE_OPENMP)
	for(n = 0; n < totnode; n++) {
		PBVHVertexIter vd;
		SculptBrushTest test;
		float (*proxy)[3];

		proxy= BLI_pbvh_node_add_proxy(ss->pbvh, nodes[n])->co;

		sculpt_brush_test_init(ss, &test);

		BLI_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
			if(sculpt_brush_test(&test, vd.co)) {
				const float fade = bstrength*tex_strength(ss, brush, vd.co, test.dist);

				mul_v3_v3fl(proxy[vd.i], grab_delta, fade);

				if(vd.mvert) {
					vd.mvert->flag |= ME_VERT_PBVH_UPDATE;

					// unlimited clay
					//if(brush->flag & BRUSH_SUBDIV)
					//	vd.mvert->flag = 1; 
				}
			}
		}
		BLI_pbvh_vertex_iter_end;
	}
}

static void do_thumb_brush(Sculpt *sd, SculptSession *ss, PBVHNode **nodes, int totnode)
{
	Brush *brush = paint_brush(&sd->paint);
	float bstrength = ss->cache->bstrength;
	float grab_delta[3];
	int n;
	float an[3];
	float tmp[3], cono[3];

	copy_v3_v3(grab_delta, ss->cache->grab_delta_symmetry);

	calc_area_normal(sd, ss, an, nodes, totnode);

	cross_v3_v3v3(tmp, an, grab_delta);
	cross_v3_v3v3(cono, tmp, an);

	#pragma omp parallel for schedule(guided) if (sd->flags & SCULPT_USE_OPENMP)
	for(n = 0; n < totnode; n++) {
		PBVHVertexIter vd;
		SculptBrushTest test;
		float (*origco)[3];
		float (*proxy)[3];

		origco= sculpt_undo_push_node(ss, nodes[n])->co;

		proxy= BLI_pbvh_node_add_proxy(ss->pbvh, nodes[n])->co;

		sculpt_brush_test_init(ss, &test);

		BLI_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
			if(sculpt_brush_test(&test, origco[vd.i])) {
				const float fade = bstrength*tex_strength(ss, brush, origco[vd.i], test.dist);

				mul_v3_v3fl(proxy[vd.i], cono, fade);

				if(vd.mvert) {
					vd.mvert->flag |= ME_VERT_PBVH_UPDATE;

					if(brush->flag & BRUSH_SUBDIV)
						vd.mvert->flag = 1; 
				}
			}
		}
		BLI_pbvh_vertex_iter_end;
	}
}

static void do_rotate_brush(Sculpt *sd, SculptSession *ss, PBVHNode **nodes, int totnode)
{
	Brush *brush= paint_brush(&sd->paint);
	float bstrength= ss->cache->bstrength;
	float an[3];
	int n;
	float m[3][3];
	static const int flip[8] = { 1, -1, -1, 1, -1, 1, 1, -1 };
	float angle = ss->cache->vertex_rotation * flip[ss->cache->symmetry_pass];

	calc_area_normal(sd, ss, an, nodes, totnode);

	axis_angle_to_mat3(m, an, angle);

	#pragma omp parallel for schedule(guided) if (sd->flags & SCULPT_USE_OPENMP)
	for(n=0; n<totnode; n++) {
		PBVHVertexIter vd;
		SculptBrushTest test;
		float (*origco)[3];
		float (*proxy)[3];
	
		origco= sculpt_undo_push_node(ss, nodes[n])->co;

		proxy= BLI_pbvh_node_add_proxy(ss->pbvh, nodes[n])->co;

		sculpt_brush_test_init(ss, &test);

		BLI_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
			if(sculpt_brush_test(&test, origco[vd.i])) {
				const float fade = bstrength*tex_strength(ss, brush, origco[vd.i], test.dist);

				mul_v3_m3v3(proxy[vd.i], m, origco[vd.i]);
				sub_v3_v3(proxy[vd.i], origco[vd.i]);
				mul_v3_fl(proxy[vd.i], fade);

				if(vd.mvert) {
					vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
					if(brush->flag & BRUSH_SUBDIV) vd.mvert->flag = 1; 
				}
			}
		}
		BLI_pbvh_vertex_iter_end;
	}
}

static void do_layer_brush(Sculpt *sd, SculptSession *ss, PBVHNode **nodes, int totnode)
{
	Brush *brush = paint_brush(&sd->paint);
	float bstrength= ss->cache->bstrength;
	float area_normal[3], offset[3];
	float lim= ss->cache->radius / 4;
	int n;

	if(bstrength < 0)
		lim = -lim;

	calc_area_normal(sd, ss, area_normal, nodes, totnode);

	mul_v3_v3v3(offset, ss->cache->scale, area_normal);

	#pragma omp parallel for schedule(guided) if (sd->flags & SCULPT_USE_OPENMP)
	for(n=0; n<totnode; n++) {
		PBVHVertexIter vd;
		SculptBrushTest test;
		SculptUndoNode *unode;
		float (*origco)[3], *layer_disp;
		//float (*proxy)[3]; // XXX layer brush needs conversion to proxy but its more complicated

		//proxy= BLI_pbvh_node_add_proxy(ss->pbvh, nodes[n])->co;
		
		unode= sculpt_undo_push_node(ss, nodes[n]);
		origco=unode->co;
		if(!unode->layer_disp)
			{
				#pragma omp critical 
				unode->layer_disp= MEM_callocN(sizeof(float)*unode->totvert, "layer disp");
			}

		layer_disp= unode->layer_disp;

		sculpt_brush_test_init(ss, &test);

		BLI_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
			if(sculpt_brush_test(&test, vd.co)) {
				const float fade = tex_strength(ss, brush, vd.co, test.dist)*bstrength*ss->cache->radius;
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
				if(vd.mvert) {
					vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
					if(brush->flag & BRUSH_SUBDIV) vd.mvert->flag = 1; 
				}
			}
		}
		BLI_pbvh_vertex_iter_end;
	}
}

static void do_inflate_brush(Sculpt *sd, SculptSession *ss, PBVHNode **nodes, int totnode)
{
	Brush *brush = paint_brush(&sd->paint);
	float bstrength= ss->cache->bstrength;
	int n;

	#pragma omp parallel for schedule(guided) if (sd->flags & SCULPT_USE_OPENMP)
	for(n=0; n<totnode; n++) {
		PBVHVertexIter vd;
		SculptBrushTest test;
		float (*proxy)[3];

		proxy= BLI_pbvh_node_add_proxy(ss->pbvh, nodes[n])->co;

		sculpt_brush_test_init(ss, &test);

		BLI_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
			if(sculpt_brush_test(&test, vd.co)) {
				const float fade = tex_strength(ss, brush, vd.co, test.dist)*bstrength;
				float val[3];

				if(vd.fno) copy_v3_v3(val, vd.fno);
				else normal_short_to_float_v3(val, vd.no);
				
				mul_v3_fl(val, fade * ss->cache->radius);
				mul_v3_v3v3(proxy[vd.i], val, ss->cache->scale);

				if(vd.mvert) {
					vd.mvert->flag |= ME_VERT_PBVH_UPDATE;

					// unlimited clay
					//if(brush->flag & BRUSH_SUBDIV)
					//	vd.mvert->flag = 1; 
				}
			}
		}
		BLI_pbvh_vertex_iter_end;
	}
}

static void calc_flatten_center(Sculpt *sd, SculptSession *ss, PBVHNode **nodes, int totnode, float fc[3])
{
	int n;

	float count = 0;

	zero_v3(fc);

	#pragma omp parallel for schedule(guided) if (sd->flags & SCULPT_USE_OPENMP)
	for(n=0; n<totnode; n++) {
		PBVHVertexIter vd;
		SculptBrushTest test;
		SculptUndoNode *unode;
		float private_an[3] = {0.0f, 0.0f, 0.0f};
		float private_out_flip[3] = {0.0f, 0.0f, 0.0f};
		float private_fc[3] = {0.0f, 0.0f, 0.0f};
		int private_count = 0;

		unode = sculpt_undo_push_node(ss, nodes[n]);
		sculpt_brush_test_init(ss, &test);

		if(ss->cache->original) {
			BLI_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
				if(sculpt_brush_test_fast(&test, unode->co[vd.i])) {
					add_v3_v3(private_fc, vd.co);
					private_count++;
				}
			}
			BLI_pbvh_vertex_iter_end;
		}
		else {
			BLI_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
				if(sculpt_brush_test_fast(&test, vd.co)) {
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

static void calc_area_normal_and_flatten_center(Sculpt *sd, SculptSession *ss, PBVHNode **nodes, int totnode, float an[3], float fc[3])
{
	Brush *brush = paint_brush(&sd->paint);

	if (ss->cache->symmetry_pass == 0 &&
	   (ss->cache->first_time || !(brush->flag & BRUSH_ORIGINAL_NORMAL)))
	{
		//int sculpt_plane = (brush->flag & BRUSH_ORIGINAL_NORMAL) ? brush->sculpt_plane : SCULPT_DISP_DIR_AREA;
		int sculpt_plane = brush->sculpt_plane;

		switch (sculpt_plane) {
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
				/* this calculates flatten center and area normal together, 
				amortizing the memory bandwidth and loop overhead to calculate both at the same time */
				{
					int n;

					// an
					float out_flip[3] = {0.0f, 0.0f, 0.0f};

					// fc
					float count = 0;

					// an
					zero_v3(an);

					// fc
					zero_v3(fc);

					#pragma omp parallel for schedule(guided) if (sd->flags & SCULPT_USE_OPENMP)
					for(n=0; n<totnode; n++) {
						PBVHVertexIter vd;
						SculptBrushTest test;
						SculptUndoNode *unode;
						float private_an[3] = {0.0f, 0.0f, 0.0f};
						float private_out_flip[3] = {0.0f, 0.0f, 0.0f};
						float private_fc[3] = {0.0f, 0.0f, 0.0f};
						int private_count = 0;

						unode = sculpt_undo_push_node(ss, nodes[n]);
						sculpt_brush_test_init(ss, &test);

						if(ss->cache->original) {
							BLI_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
								if(sculpt_brush_test_fast(&test, unode->co[vd.i])) {
									// an
									float fno[3];

									normal_short_to_float_v3(fno, unode->no[vd.i]);
									add_norm_if(ss->cache->view_normal_symmetry, private_an, private_out_flip, fno);

									// fc
									add_v3_v3(private_fc, vd.co);
									private_count++;
								}
							}
							BLI_pbvh_vertex_iter_end;
						}
						else {
							BLI_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
								if(sculpt_brush_test_fast(&test, vd.co)) {
									// an
									if(vd.no) {
										float fno[3];

										normal_short_to_float_v3(fno, vd.no);
										add_norm_if(ss->cache->view_normal_symmetry, private_an, private_out_flip, fno);
									}
									else {
										add_norm_if(ss->cache->view_normal_symmetry, private_an, private_out_flip, vd.fno);
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
					mul_v3_fl(fc, 1.0f / count);
				}

			default:
				break;
		}

		// fc
		/* flatten center has not been calculated yet if we are not using the area normal */
		if (sculpt_plane != SCULPT_DISP_DIR_AREA)
			calc_flatten_center(sd, ss, nodes, totnode, fc);

		// an
		copy_v3_v3(ss->cache->last_area_normal, an);
		copy_v3_v3(ss->cache->last_view_normal_symmetry, ss->cache->view_normal_symmetry);

		// fc
		copy_v3_v3(ss->cache->last_center, fc);
	}
	else {
		// an
		copy_v3_v3(an, ss->cache->last_area_normal);
		flip_coord(an, an, ss->cache->symmetry_pass);

		copy_v3_v3(ss->cache->view_normal_symmetry, ss->cache->last_view_normal_symmetry);
		flip_coord(ss->cache->view_normal_symmetry, ss->cache->view_normal_symmetry, ss->cache->symmetry_pass);

		// fc
		copy_v3_v3(fc, ss->cache->last_center);
		flip_coord(fc, fc, ss->cache->symmetry_pass);
	}
}

/* Projects a point onto a plane along the plane's normal */
static void point_plane_project(float intr[3], float co[3], float plane_normal[3], float plane_center[3])
{
    sub_v3_v3v3(intr, co, plane_center);
    mul_v3_v3fl(intr, plane_normal, dot_v3v3(plane_normal, intr));
    sub_v3_v3v3(intr, co, intr); 
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
		rv *= ss->cache->pressure;
	}

	return rv;
}

static void do_flatten_brush(Sculpt *sd, SculptSession *ss, PBVHNode **nodes, int totnode)
{
	Brush *brush = paint_brush(&sd->paint);

	float bstrength = ss->cache->bstrength;
	const float radius = ss->cache->radius;

	float an[3];
	float fc[3];

	float offset = get_offset(sd, ss);

	float displace;

	int n;

	float temp[3];

	calc_area_normal_and_flatten_center(sd, ss, nodes, totnode, an, fc);

	displace = radius*offset;

	mul_v3_v3v3(temp, an, ss->cache->scale);
	mul_v3_fl(temp, displace);
	add_v3_v3(fc, temp);

	#pragma omp parallel for schedule(guided) if (sd->flags & SCULPT_USE_OPENMP)
	for(n = 0; n < totnode; n++) {
		PBVHVertexIter  vd;
		SculptBrushTest test;
		float (*proxy)[3];

		proxy= BLI_pbvh_node_add_proxy(ss->pbvh, nodes[n])->co;

		sculpt_brush_test_init(ss, &test);

		BLI_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
			if(sculpt_brush_test(&test, vd.co)) {
				float intr[3];
				float val[3];

				const float fade = bstrength * tex_strength(ss, brush, vd.co, test.dist);

				point_plane_project(intr, vd.co, an, fc);
				sub_v3_v3v3(val, intr, vd.co);
				mul_v3_v3fl(proxy[vd.i], val, fade);

				if(vd.mvert) {
					vd.mvert->flag |= ME_VERT_PBVH_UPDATE;

					// unlimited clay
					//if(brush->flag & BRUSH_SUBDIV)
					//	vd.mvert->flag = 1; 
				}
			}
		}
		BLI_pbvh_vertex_iter_end;
	}
}

static void do_clay_brush(Sculpt *sd, SculptSession *ss, PBVHNode **nodes, int totnode)
{
	Brush *brush = paint_brush(&sd->paint);

	float bstrength = ss->cache->bstrength;
	float radius    = ss->cache->radius;
	float offset    = get_offset(sd, ss);
	
	float displace;

	float an[3]; // area normal
	float fc[3]; // flatten center

	int n;

	float temp[3];
	//float p[3];

	int flip;

	calc_area_normal_and_flatten_center(sd, ss, nodes, totnode, an, fc);

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

	#pragma omp parallel for schedule(guided) if (sd->flags & SCULPT_USE_OPENMP)
	for (n = 0; n < totnode; n++) {
		PBVHVertexIter vd;
		SculptBrushTest test;
		float (*proxy)[3];

		proxy= BLI_pbvh_node_add_proxy(ss->pbvh, nodes[n])->co;

		sculpt_brush_test_init(ss, &test);

		BLI_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
			if (sculpt_brush_test(&test, vd.co)) {
			//if (sculpt_brush_test_cyl(&test, vd.co, ss->cache->location, p)) {
				float intr[3];

				const float fade = bstrength*tex_strength(ss, brush, vd.co, test.dist);

				point_plane_project(intr, vd.co, an, fc);
				sub_v3_v3v3(proxy[vd.i], intr, vd.co);
				mul_v3_fl(proxy[vd.i], fade);

				if(vd.mvert) {
					vd.mvert->flag |= ME_VERT_PBVH_UPDATE;

					// unlimited clay
					//if(brush->flag & BRUSH_SUBDIV)
					//	vd.mvert->flag = 1; 
				}
			}
		}
		BLI_pbvh_vertex_iter_end;
	}
}

static void do_wax_brush(Sculpt *sd, SculptSession *ss, PBVHNode **nodes, int totnode)
{
	Brush *brush = paint_brush(&sd->paint);

	float bstrength = ss->cache->bstrength;
	float radius    = ss->cache->radius;
	float offset    = get_offset(sd, ss);
	
	float displace;

	float an[3]; // area normal
	float fc[3]; // flatten center

	int n;

	float temp[3];

	int flip;

	calc_area_normal_and_flatten_center(sd, ss, nodes, totnode, an, fc);

	flip = bstrength < 0;

	if (flip) {
		bstrength = -bstrength;
		radius    = -radius;
	}

	displace = radius * (0.25f+offset);

	mul_v3_v3v3(temp, an, ss->cache->scale);
	mul_v3_fl(temp, displace);
	add_v3_v3(fc, temp);

	#pragma omp parallel for schedule(guided) if (sd->flags & SCULPT_USE_OPENMP)
	for (n = 0; n < totnode; n++) {
		PBVHVertexIter vd;
		SculptBrushTest test;
		float (*proxy)[3];

		proxy= BLI_pbvh_node_add_proxy(ss->pbvh, nodes[n])->co;

		sculpt_brush_test_init(ss, &test);

		BLI_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
			if (sculpt_brush_test_sq(&test, vd.co)) {
				if (plane_point_side_flip(vd.co, an, fc, flip)) {
					float intr[3];
					float val[3];

					const float fade = bstrength*tex_strength(ss, brush, vd.co, sqrt(test.dist));

					point_plane_project(intr, vd.co, an, fc);
					sub_v3_v3v3(val, intr, vd.co);
					mul_v3_v3fl(proxy[vd.i], val, fade);

					if(vd.mvert) {
						vd.mvert->flag |= ME_VERT_PBVH_UPDATE;

						// unlimited clay
						//if (brush->flag & BRUSH_SUBDIV)
						//	vd.mvert->flag = 1; 
					}
				}
			}
		}
		BLI_pbvh_vertex_iter_end;
	}
}

static void do_fill_brush(Sculpt *sd, SculptSession *ss, PBVHNode **nodes, int totnode)
{
	Brush *brush = paint_brush(&sd->paint);

	float bstrength = ss->cache->bstrength;
	const float radius = ss->cache->radius;

	float an[3];
	float fc[3];
	float offset = get_offset(sd, ss);

	float displace;

	int n;

	float temp[3];

	calc_area_normal_and_flatten_center(sd, ss, nodes, totnode, an, fc);

	displace = radius*offset;

	mul_v3_v3v3(temp, an, ss->cache->scale);
	mul_v3_fl(temp, displace);
	add_v3_v3(fc, temp);

	#pragma omp parallel for schedule(guided) if (sd->flags & SCULPT_USE_OPENMP)
	for (n = 0; n < totnode; n++) {
		PBVHVertexIter vd;
		SculptBrushTest test;
		float (*proxy)[3];

		proxy= BLI_pbvh_node_add_proxy(ss->pbvh, nodes[n])->co;

		sculpt_brush_test_init(ss, &test);

		BLI_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
			if (sculpt_brush_test(&test, vd.co)) {
				if (plane_point_side(vd.co, an, fc)) {
					float intr[3];
					float val[3];

					const float fade = bstrength * tex_strength(ss, brush, vd.co, test.dist);

					point_plane_project(intr, vd.co, an, fc);
					sub_v3_v3v3(val, intr, vd.co);
					mul_v3_v3fl(proxy[vd.i], val, fade);

					if(vd.mvert) {
						vd.mvert->flag |= ME_VERT_PBVH_UPDATE;

						// unlimited clay
						//if(brush->flag & BRUSH_SUBDIV)
						//	vd.mvert->flag = 1; 
					}
				}
			}
		}
		BLI_pbvh_vertex_iter_end;
	}
}

static void do_scrape_brush(Sculpt *sd, SculptSession *ss, PBVHNode **nodes, int totnode)
{
	Brush *brush = paint_brush(&sd->paint);

	float bstrength = ss->cache->bstrength;
	const float radius = ss->cache->radius;

	float an[3];
	float fc[3];
	float offset = get_offset(sd, ss);

	float displace;

	int n;

	float temp[3];

	calc_area_normal_and_flatten_center(sd, ss, nodes, totnode, an, fc);

	displace = -radius*offset;

	mul_v3_v3v3(temp, an, ss->cache->scale);
	mul_v3_fl(temp, displace);
	add_v3_v3(fc, temp);

	#pragma omp parallel for schedule(guided) if (sd->flags & SCULPT_USE_OPENMP)
	for (n = 0; n < totnode; n++) {
		PBVHVertexIter vd;
		SculptBrushTest test;
		float (*proxy)[3];

		proxy= BLI_pbvh_node_add_proxy(ss->pbvh, nodes[n])->co;

		sculpt_brush_test_init(ss, &test);

		BLI_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
			if (sculpt_brush_test(&test, vd.co)) {
				if (!plane_point_side(vd.co, an, fc)) {
					float intr[3];
					float val[3];

					const float fade = bstrength * tex_strength(ss, brush, vd.co, test.dist);

					point_plane_project(intr, vd.co, an, fc);
					sub_v3_v3v3(val, intr, vd.co);
					mul_v3_v3fl(proxy[vd.i], val, fade);

					if(vd.mvert) {
						vd.mvert->flag |= ME_VERT_PBVH_UPDATE;

						// unlimited clay
						//if(brush->flag & BRUSH_SUBDIV)
						//	vd.mvert->flag = 1; 
					}
				}
			}
		}
		BLI_pbvh_vertex_iter_end;
	}
}

static void do_brush_action(Sculpt *sd, SculptSession *ss)
{
	SculptSearchSphereData data;
	Brush *brush = paint_brush(&sd->paint);
	PBVHNode **nodes = NULL;
	int n, totnode;

	/* Build a list of all nodes that are potentially within the brush's area of influence */
	data.ss = ss;
	data.sd = sd;
	data.radius_squared = ss->cache->radius * ss->cache->radius;
	data.original = ELEM4(brush->sculpt_tool, SCULPT_TOOL_GRAB, SCULPT_TOOL_ROTATE, SCULPT_TOOL_THUMB, SCULPT_TOOL_LAYER);
	BLI_pbvh_search_gather(ss->pbvh, sculpt_search_sphere_cb, &data, &nodes, &totnode);

	/* Only act if some verts are inside the brush area */
	if (totnode) {
		#pragma omp parallel for schedule(guided) if (sd->flags & SCULPT_USE_OPENMP)
		for (n= 0; n < totnode; n++) {
			sculpt_undo_push_node(ss, nodes[n]);
			BLI_pbvh_node_mark_update(nodes[n]);
		}

		/* Apply one type of brush action */
		switch(brush->sculpt_tool){
		case SCULPT_TOOL_DRAW:
			do_draw_brush(sd, ss, nodes, totnode);
			break;
		case SCULPT_TOOL_SMOOTH:
			do_smooth_brush(sd, ss, nodes, totnode);
			break;
		case SCULPT_TOOL_CREASE:
			do_crease_brush(sd, ss, nodes, totnode);
			break;
		case SCULPT_TOOL_BLOB:
			do_crease_brush(sd, ss, nodes, totnode);
			break;
		case SCULPT_TOOL_PINCH:
			do_pinch_brush(sd, ss, nodes, totnode);
			break;
		case SCULPT_TOOL_INFLATE:
			do_inflate_brush(sd, ss, nodes, totnode);
			break;
		case SCULPT_TOOL_GRAB:
			do_grab_brush(sd, ss, nodes, totnode);
			break;
		case SCULPT_TOOL_ROTATE:
			do_rotate_brush(sd, ss, nodes, totnode);
			break;
		case SCULPT_TOOL_SNAKE_HOOK:
			do_snake_hook_brush(sd, ss, nodes, totnode);
			break;
		case SCULPT_TOOL_NUDGE:
			do_nudge_brush(sd, ss, nodes, totnode);
			break;
		case SCULPT_TOOL_THUMB:
			do_thumb_brush(sd, ss, nodes, totnode);
			break;
		case SCULPT_TOOL_LAYER:
			do_layer_brush(sd, ss, nodes, totnode);
			break;
		case SCULPT_TOOL_FLATTEN:
			do_flatten_brush(sd, ss, nodes, totnode);
			break;
		case SCULPT_TOOL_CLAY:
			do_clay_brush(sd, ss, nodes, totnode);
			break;
		case SCULPT_TOOL_FILL:
			do_fill_brush(sd, ss, nodes, totnode);
			break;
		case SCULPT_TOOL_SCRAPE:
			do_scrape_brush(sd, ss, nodes, totnode);
			break;
		case SCULPT_TOOL_WAX:
			do_wax_brush(sd, ss, nodes, totnode);
			break;
		}

		if (brush->sculpt_tool != SCULPT_TOOL_SMOOTH && brush->autosmooth_factor > 0)
			if (brush->flag & BRUSH_INVERSE_SMOOTH_PRESSURE)
				smooth(sd, ss, nodes, totnode, brush->autosmooth_factor*(1-ss->cache->pressure)*brush->autosmooth_overlap);
			else
				smooth(sd, ss, nodes, totnode, brush->autosmooth_factor*brush->autosmooth_overlap);

		/* copy the modified vertices from mesh to the active key */
		if(ss->kb)
			mesh_to_key(ss->ob->data, ss->kb);

		MEM_freeN(nodes);
	}
}

static void sculpt_combine_proxies(Sculpt *sd, SculptSession *ss)
{
	Brush *brush= paint_brush(&sd->paint);
	PBVHNode** nodes;
	int totnode;
	int n;

	BLI_pbvh_gather_proxies(ss->pbvh, &nodes, &totnode);

	switch (brush->sculpt_tool) {
		case SCULPT_TOOL_GRAB:
		case SCULPT_TOOL_ROTATE:
		case SCULPT_TOOL_THUMB:
			#pragma omp parallel for schedule(guided) if (sd->flags & SCULPT_USE_OPENMP)
			for (n= 0; n < totnode; n++) {
				PBVHVertexIter vd;
				PBVHProxyNode* proxies;
				int proxy_count;
				float (*origco)[3];

				origco= sculpt_undo_push_node(ss, nodes[n])->co;

				BLI_pbvh_node_get_proxies(nodes[n], &proxies, &proxy_count);

				BLI_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
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
		case SCULPT_TOOL_CREASE:
		case SCULPT_TOOL_BLOB:
		case SCULPT_TOOL_FILL:
		case SCULPT_TOOL_FLATTEN:
		case SCULPT_TOOL_INFLATE:
		case SCULPT_TOOL_NUDGE:
		case SCULPT_TOOL_PINCH:
		case SCULPT_TOOL_SCRAPE:
		case SCULPT_TOOL_SNAKE_HOOK:
		case SCULPT_TOOL_WAX:
			#pragma omp parallel for schedule(guided) if (sd->flags & SCULPT_USE_OPENMP)
			for (n= 0; n < totnode; n++) {
				PBVHVertexIter vd;
				PBVHProxyNode* proxies;
				int proxy_count;

				BLI_pbvh_node_get_proxies(nodes[n], &proxies, &proxy_count);

				BLI_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
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
		default:
			break;
	}

	if (nodes)
		MEM_freeN(nodes);
}

/* Flip all the editdata across the axis/axes specified by symm. Used to
   calculate multiple modifications to the mesh when symmetry is enabled. */
static void calc_brushdata_symm(StrokeCache *cache, const char symm)
{
	flip_coord(cache->location, cache->true_location, symm);
	flip_coord(cache->view_normal_symmetry, cache->view_normal, symm);
	flip_coord(cache->grab_delta_symmetry, cache->grab_delta, symm);
}

static void do_symmetrical_brush_actions(Sculpt *sd, SculptSession *ss)
{
	Brush *brush = paint_brush(&sd->paint);
	StrokeCache *cache = ss->cache;
	const char symm = sd->flags & 7;
	int i;

	copy_v3_v3(cache->location, cache->true_location);
	copy_v3_v3(cache->grab_delta_symmetry, cache->grab_delta);
	cache->symmetry= symm;
	cache->symmetry_pass= 0;
	cache->bstrength= brush_strength(sd, cache);
	do_brush_action(sd, ss);

	/* symm is a bit combination of XYZ - 1 is mirror X; 2 is Y; 3 is XY; 4 is Z; 5 is XZ; 6 is YZ; 7 is XYZ */ 
	for(i = 1; i <= symm; ++i) {
		if(symm & i && (symm != 5 || i != 3) && (symm != 6 || (i != 3 && i != 5))) {
			cache->symmetry_pass = i;
			calc_brushdata_symm(cache, i);
			do_brush_action(sd, ss);
		}
	}

	sculpt_combine_proxies(sd, ss);

	cache->first_time= 0;
}

static void sculpt_update_tex(Sculpt *sd, SculptSession *ss)
{
	Brush *brush = paint_brush(&sd->paint);

	if(ss->texcache) {
		MEM_freeN(ss->texcache);
		ss->texcache= NULL;
	}

	/* Need to allocate a bigger buffer for bigger brush size */
	ss->texcache_side = brush->size * 2;
	if(!ss->texcache || ss->texcache_side > ss->texcache_actual) {
		ss->texcache = brush_gen_texture_cache(brush, brush->size);
		ss->texcache_actual = ss->texcache_side;
	}
}

void sculpt_key_to_mesh(KeyBlock *kb, Object *ob)
{
	Mesh *me= ob->data;

	key_to_mesh(kb, me);
	mesh_calc_normals(me->mvert, me->totvert, me->mface, me->totface, NULL);
}

void sculpt_mesh_to_key(Object *ob, KeyBlock *kb)
{
	Mesh *me= ob->data;

	mesh_to_key(me, kb);
}

void sculpt_update_mesh_elements(Scene *scene, Object *ob, int need_fmap)
{
	DerivedMesh *dm = mesh_get_derived_final(scene, ob, 0);
	SculptSession *ss = ob->sculpt;
	MultiresModifierData *mmd= sculpt_multires_active(scene, ob);

	ss->ob= ob;

	if((ob->shapeflag & OB_SHAPE_LOCK) && !mmd) {
		ss->kb= ob_get_keyblock(ob);
		ss->refkb= ob_get_reference_keyblock(ob);
	}
	else {
		ss->kb= NULL;
		ss->refkb= NULL;
	}

	/* need to make PBVH with shape key coordinates */
	if(ss->kb) sculpt_key_to_mesh(ss->kb, ss->ob);

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

	ss->pbvh = dm->getPBVH(ob, dm);
	ss->fmap = (need_fmap && dm->getFaceMap)? dm->getFaceMap(ob, dm): NULL;
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
	case SCULPT_TOOL_WAX:
		return "Wax Brush"; break;
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

	WM_paint_cursor_end(CTX_wm_manager(C), p->paint_cursor);
	p->paint_cursor = NULL;
	brush_radial_control_invoke(op, brush, 1);
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

static float unproject_brush_radius(Object *ob, ViewContext *vc, float center[3], float offset)
{
	float delta[3], scale, loc[3];

	mul_v3_m4v3(loc, ob->obmat, center);

	initgrabz(vc->rv3d, loc[0], loc[1], loc[2]);
	window_to_3d_delta(vc->ar, delta, offset, 0);

	scale= fabsf(mat4_to_scale(ob->obmat));
	scale= (scale == 0.0f)? 1.0f: scale;

	return len_v3(delta)/scale;
}

static void sculpt_cache_free(StrokeCache *cache)
{
	if(cache->face_norms)
		MEM_freeN(cache->face_norms);
	if(cache->mats)
		MEM_freeN(cache->mats);
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

	//Hopefully it is as easy as this... but I doubt it
	//if(U.sculpt_paint_settings & BRUSH_USE_UNIFIED_RADIUS_AND_STRENGTH) {
	//	brush->size = U.sculpt_paint_pixel_radius;
	//	brush->alpha = U.sculpt_paint_strength;
	//}
	
	ss->cache = cache;

	/* Set scaling adjustment */
	ss->cache->scale[0] = 1.0f / ob->size[0];
	ss->cache->scale[1] = 1.0f / ob->size[1];
	ss->cache->scale[2] = 1.0f / ob->size[2];

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

	/* Initial mouse location */
	if (event) {
		ss->cache->initial_mouse[0] = event->x;
		ss->cache->initial_mouse[1] = event->y;
	}
	else {
		ss->cache->initial_mouse[0] = 0;
		ss->cache->initial_mouse[1] = 0;
	}

	mode = RNA_int_get(op->ptr, "mode");
	cache->invert = mode == WM_BRUSHSTROKE_INVERT;
	cache->alt_smooth = mode == WM_BRUSHSTROKE_SMOOTH;

	/* Alt-Smooth */
	if (ss->cache->alt_smooth) {
		Paint *p= &sd->paint;
		Brush *br;
		int i;
		
		BLI_strncpy(cache->saved_active_brush_name, brush->id.name+2, sizeof(cache->saved_active_brush_name));

		for(i = 0; i < p->brush_count; ++i) {
			br = p->brushes[i];
		
			if (strcmp(br->id.name+2, "Smooth")==0) {
				paint_brush_set(p, br);
				brush = br;
				break;
			}
		}
	}

	copy_v2_v2(cache->mouse, cache->initial_mouse);
	copy_v2_v2(cache->tex_mouse, cache->initial_mouse);

	/* Truly temporary data that isn't stored in properties */

	cache->vc = vc;

	cache->brush = brush;

	if (brush->flag & BRUSH_SUBDIV){
		cache->detail = brush->detail;
		cache->smoothness = brush->smoothness;
	} 

	cache->mats = MEM_callocN(sizeof(bglMats), "sculpt bglMats");
	view3d_get_transformation(vc->ar, vc->rv3d, vc->obact, cache->mats);

	viewvector(cache->vc->rv3d, cache->vc->rv3d->twmat[3], cache->view_normal);
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

	if(ELEM7(brush->sculpt_tool, SCULPT_TOOL_DRAW,  SCULPT_TOOL_CREASE, SCULPT_TOOL_BLOB, SCULPT_TOOL_LAYER, SCULPT_TOOL_INFLATE, SCULPT_TOOL_CLAY, SCULPT_TOOL_WAX))
		if(!(brush->flag & BRUSH_ACCUMULATE))
			cache->original = 1;

	cache->special_rotation = (brush->flag & BRUSH_RAKE) ? brush->last_angle : 0;

	cache->first_time= 1;

	cache->vertex_rotation= 0;
}

/* Initialize the stroke cache variants from operator properties */
static void sculpt_update_cache_variants(bContext *C, Sculpt *sd, SculptSession *ss, struct PaintStroke *stroke, PointerRNA *ptr)
{
	StrokeCache *cache = ss->cache;
	Brush *brush = paint_brush(&sd->paint);

	int dx, dy;

	if (cache->first_time ||
	    !((brush->flag & BRUSH_ANCHORED)||
	      (brush->sculpt_tool == SCULPT_TOOL_SNAKE_HOOK)||
	      (brush->sculpt_tool == SCULPT_TOOL_ROTATE))
		 )
	{
		RNA_float_get_array(ptr, "location", cache->true_location);
	}

	cache->pen_flip = RNA_boolean_get(ptr, "pen_flip");
	RNA_float_get_array(ptr, "mouse", cache->mouse);

	cache->pressure = RNA_float_get(ptr, "pressure");
	cache->sculpt_plane = brush->sculpt_plane;

	/* Truly temporary data that isn't stored in properties */

	brush->draw_pressure=  1;
	brush->pressure_value= cache->pressure;

	cache->previous_pixel_radius = cache->pixel_radius;
	cache->pixel_radius = brush->size;

	if(cache->first_time) {
		if (!(brush->flag & BRUSH_LOCK_SIZE)) {
			cache->initial_radius= brush->unprojected_radius = unproject_brush_radius(ss->ob, cache->vc, cache->true_location, brush->size);
		}
		else {
			cache->initial_radius= brush->unprojected_radius;
		}

		if (ELEM(brush->sculpt_tool, SCULPT_TOOL_GRAB, SCULPT_TOOL_SNAKE_HOOK))
			cache->initial_radius *= 2.0f;
	}

	if(brush->flag & BRUSH_SIZE_PRESSURE) {
		cache->pixel_radius *= cache->pressure;
		cache->radius= cache->initial_radius * cache->pressure;
	}
	else
		cache->radius= cache->initial_radius;

	if(!(brush->flag & BRUSH_ANCHORED || ELEM3(brush->sculpt_tool, SCULPT_TOOL_GRAB, SCULPT_TOOL_THUMB, SCULPT_TOOL_ROTATE))) {
		copy_v2_v2(cache->tex_mouse, cache->mouse);
	}

	if(brush->flag & BRUSH_ANCHORED) {
		int hit = 0;

		dx = cache->mouse[0] - cache->initial_mouse[0];
		dy = cache->mouse[1] - cache->initial_mouse[1];

		brush->anchored_size = cache->pixel_radius = sqrt(dx*dx + dy*dy);

		cache->special_rotation = atan2(dx, dy) + M_PI;

		if (brush->flag & BRUSH_EDGE_TO_EDGE) {
			float d[3];
			float halfway[3];
			float out[3];

			d[0] = dx;
			d[1] = dy;
			d[2] = 0;

			mul_v3_v3fl(halfway, d, 0.5f);
			add_v3_v3(halfway, cache->initial_mouse);

			if (sculpt_stroke_get_location(C, stroke, out, halfway)) {
				copy_v3_v3(brush->anchored_location, out);
				copy_v3_v3(brush->anchored_initial_mouse, halfway);
				copy_v2_v2(cache->tex_mouse, halfway);
				copy_v3_v3(cache->true_location, brush->anchored_location);
				brush->anchored_size /= 2.0f;
				cache->pixel_radius  /= 2.0f;
				hit = 1;
			}
		}

		if (!hit)
			copy_v2_v2(brush->anchored_initial_mouse, cache->initial_mouse);

		cache->radius= unproject_brush_radius(ss->ob, paint_stroke_view_context(stroke), cache->true_location, cache->pixel_radius);

		copy_v3_v3(brush->anchored_location, cache->true_location);

		brush->draw_anchored = 1;
	}
	else if(brush->flag & BRUSH_RAKE) {
		int update;

		dx = cache->last_rake[0] - cache->mouse[0];
		dy = cache->last_rake[1] - cache->mouse[1];

		/* To prevent jitter, only update the angle if the mouse has moved over 10 pixels */
		update = dx*dx + dy*dy > 100;

		if(update && !cache->first_time)
			cache->special_rotation = atan2(dx, dy);

		if(update || cache->first_time) {
			cache->last_rake[0] = cache->mouse[0];
			cache->last_rake[1] = cache->mouse[1];
		}
	}

	/* Find the grab delta */
	if(brush->sculpt_tool == SCULPT_TOOL_GRAB) {
		float grab_location[3], imat[4][4];

		if(cache->first_time)
			copy_v3_v3(cache->orig_grab_location, cache->true_location);

		/* compute 3d coordinate at same z from original location + mouse */
		initgrabz(cache->vc->rv3d, cache->orig_grab_location[0],
			cache->orig_grab_location[1], cache->orig_grab_location[2]);
		window_to_3d_delta(cache->vc->ar, grab_location, cache->mouse[0], cache->mouse[1]);

		/* compute delta to move verts by */
		if(!cache->first_time) {
			float delta[3];
			sub_v3_v3v3(delta, grab_location, cache->old_grab_location);
			invert_m4_m4(imat, ss->ob->obmat);
			mul_mat3_m4_v3(imat, delta);
			add_v3_v3(cache->grab_delta, delta);
		}
		else {
			zero_v3(cache->grab_delta);
		}

		copy_v3_v3(cache->old_grab_location, grab_location);

		/* location stays the same for finding vertices in brush radius */
		copy_v3_v3(cache->true_location, cache->orig_grab_location);

		brush->draw_anchored = 1;
		copy_v3_v3(brush->anchored_location, cache->true_location);
		copy_v3_v3(brush->anchored_initial_mouse, cache->initial_mouse);
		brush->anchored_size = cache->pixel_radius;
	}
	/* Find the nudge delta */
	else if(brush->sculpt_tool == SCULPT_TOOL_NUDGE) {
		float grab_location[3], imat[4][4];

		if(cache->first_time)
			copy_v3_v3(cache->orig_grab_location, cache->true_location);

		/* compute 3d coordinate at same z from original location + mouse */
		initgrabz(cache->vc->rv3d, cache->orig_grab_location[0],
			cache->orig_grab_location[1], cache->orig_grab_location[2]);
		window_to_3d_delta(cache->vc->ar, grab_location, cache->mouse[0], cache->mouse[1]);

		/* compute delta to move verts by */
		if (!cache->first_time) {
			sub_v3_v3v3(cache->grab_delta, grab_location, cache->old_grab_location);
			invert_m4_m4(imat, ss->ob->obmat);
			mul_mat3_m4_v3(imat, cache->grab_delta);
		}
		else {
			zero_v3(cache->grab_delta);
		}

		copy_v3_v3(cache->old_grab_location, grab_location);
	}
	/* Find the snake hook delta */
	else if(brush->sculpt_tool == SCULPT_TOOL_SNAKE_HOOK) {
		float grab_location[3], imat[4][4];

		if(cache->first_time)
			copy_v3_v3(cache->orig_grab_location, cache->true_location);
		else
			add_v3_v3(cache->true_location, cache->grab_delta);

		/* compute 3d coordinate at same z from original location + mouse */
		initgrabz(cache->vc->rv3d, cache->orig_grab_location[0],
			cache->orig_grab_location[1], cache->orig_grab_location[2]);
		window_to_3d_delta(cache->vc->ar, grab_location, cache->mouse[0], cache->mouse[1]);

		/* compute delta to move verts by */
		if (!cache->first_time) {
			sub_v3_v3v3(cache->grab_delta, grab_location, cache->old_grab_location);
			invert_m4_m4(imat, ss->ob->obmat);
			mul_mat3_m4_v3(imat, cache->grab_delta);
		}
		else {
			zero_v3(cache->grab_delta);
		}

		copy_v3_v3(cache->old_grab_location, grab_location);
	}
	/* Find the thumb delta */
	else if(brush->sculpt_tool == SCULPT_TOOL_THUMB) {
		float grab_location[3], imat[4][4];

		if(cache->first_time)
			copy_v3_v3(cache->orig_grab_location, cache->true_location);

		/* compute 3d coordinate at same z from original location + mouse */
		initgrabz(cache->vc->rv3d, cache->orig_grab_location[0],
			cache->orig_grab_location[1], cache->orig_grab_location[2]);
		window_to_3d_delta(cache->vc->ar, grab_location, cache->mouse[0], cache->mouse[1]);

		/* compute delta to move verts by */
		if (!cache->first_time) {
			float delta[3];
			sub_v3_v3v3(delta, grab_location, cache->old_grab_location);
			invert_m4_m4(imat, ss->ob->obmat);
			mul_mat3_m4_v3(imat, delta);
			add_v3_v3(cache->grab_delta, delta);
		}
		else {
			zero_v3(cache->grab_delta);
		}

		copy_v3_v3(cache->old_grab_location, grab_location);

		/* location stays the same for finding vertices in brush radius */
		copy_v3_v3(cache->true_location, cache->orig_grab_location);

		brush->draw_anchored = 1;
		copy_v3_v3(brush->anchored_location, cache->orig_grab_location);
		copy_v3_v3(brush->anchored_initial_mouse, cache->initial_mouse);
		brush->anchored_size = cache->pixel_radius;
	}
	else if(brush->sculpt_tool == SCULPT_TOOL_ROTATE) {
		dx = cache->mouse[0] - cache->initial_mouse[0];
		dy = cache->mouse[1] - cache->initial_mouse[1];

		cache->vertex_rotation = -atan2(dx, dy) / 4.0f;

		brush->draw_anchored = 1;
		copy_v2_v2(brush->anchored_initial_mouse, cache->initial_mouse);
		copy_v3_v3(brush->anchored_location, cache->true_location);
		brush->anchored_size = cache->pixel_radius;
	}
}

static void sculpt_stroke_modifiers_check(bContext *C, SculptSession *ss)
{
	Scene *scene= CTX_data_scene(C);

	if(sculpt_modifiers_active(scene, ss->ob)) {
		Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
		Brush *brush = paint_brush(&sd->paint);

		sculpt_update_mesh_elements(CTX_data_scene(C), ss->ob, brush->sculpt_tool == SCULPT_TOOL_SMOOTH);
	}
}

typedef struct {
	SculptSession *ss;
	float *ray_start, *ray_normal;
	int hit;
	float dist;
	int original;
} SculptRaycastData;

void sculpt_raycast_cb(PBVHNode *node, void *data_v, float* tmin)
{
	if (BLI_pbvh_node_get_tmin(node) < *tmin) {
		SculptRaycastData *srd = data_v;
		float (*origco)[3]= NULL;

		if(srd->original && srd->ss->cache) {
			/* intersect with coordinates from before we started stroke */
			SculptUndoNode *unode= sculpt_undo_get_node(node);
			origco= (unode)? unode->co: NULL;
		}

		if (BLI_pbvh_node_raycast(srd->ss->pbvh, node, origco, srd->ray_start, srd->ray_normal, &srd->dist)) {
			srd->hit = 1;
			*tmin = srd->dist;
		}
	}
}

/* Do a raycast in the tree to find the 3d brush location
   (This allows us to ignore the GL depth buffer)
   Returns 0 if the ray doesn't hit the mesh, non-zero otherwise
 */
int sculpt_stroke_get_location(bContext *C, struct PaintStroke *stroke, float out[3], float mouse[2])
{
	ViewContext *vc = paint_stroke_view_context(stroke);
	SculptSession *ss= vc->obact->sculpt;
	StrokeCache *cache= ss->cache;
	float ray_start[3], ray_end[3], ray_normal[3], dist;
	float obimat[4][4];
	float mval[2];
	SculptRaycastData srd;

	mval[0] = mouse[0] - vc->ar->winrct.xmin;
	mval[1] = mouse[1] - vc->ar->winrct.ymin;

	sculpt_stroke_modifiers_check(C, ss);

	viewline(vc->ar, vc->v3d, mval, ray_start, ray_end);

	invert_m4_m4(obimat, ss->ob->obmat);
	mul_m4_v3(obimat, ray_start);
	mul_m4_v3(obimat, ray_end);

	sub_v3_v3v3(ray_normal, ray_end, ray_start);
	dist= normalize_v3(ray_normal);

	srd.ss = vc->obact->sculpt;
	srd.ray_start = ray_start;
	srd.ray_normal = ray_normal;
	srd.dist = dist;
	srd.hit = 0;
	srd.original = (cache)? cache->original: 0;
	BLI_pbvh_raycast(ss->pbvh, sculpt_raycast_cb, &srd,
			 ray_start, ray_normal, srd.original);
	
	copy_v3_v3(out, ray_normal);
	mul_v3_fl(out, srd.dist);
	add_v3_v3(out, ray_start);

	return srd.hit;
}

static int sculpt_brush_stroke_init(bContext *C, ReportList *reports)
{
	Scene *scene= CTX_data_scene(C);
	Object *ob= CTX_data_active_object(C);
	Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
	SculptSession *ss = CTX_data_active_object(C)->sculpt;
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

static void sculpt_restore_mesh(Sculpt *sd, SculptSession *ss)
{
	Brush *brush = paint_brush(&sd->paint);

	/* Restore the mesh before continuing with anchored stroke */
	if((brush->flag & BRUSH_ANCHORED) ||
	   (brush->sculpt_tool == SCULPT_TOOL_GRAB && (brush->flag & BRUSH_SIZE_PRESSURE)) ||
	   (brush->flag & BRUSH_RESTORE_MESH))
	{
		StrokeCache *cache = ss->cache;
		int i;

		PBVHNode **nodes;
		int n, totnode;

		BLI_pbvh_search_gather(ss->pbvh, NULL, NULL, &nodes, &totnode);

		#pragma omp parallel for schedule(guided) if (sd->flags & SCULPT_USE_OPENMP)
		for(n=0; n<totnode; n++) {
			SculptUndoNode *unode;
			
			unode= sculpt_undo_get_node(nodes[n]);
			if(unode) {
				PBVHVertexIter vd;

				BLI_pbvh_vertex_iter_begin(ss->pbvh, nodes[n], vd, PBVH_ITER_UNIQUE) {
					copy_v3_v3(vd.co, unode->co[vd.i]);
					if(vd.no) VECCOPY(vd.no, unode->no[vd.i])
					else normal_short_to_float_v3(vd.fno, unode->no[vd.i]);

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
	Scene *scene = CTX_data_scene(C);
	Object *ob = CTX_data_active_object(C);
	SculptSession *ss = ob->sculpt;
	ARegion *ar = CTX_wm_region(C);
	MultiresModifierData *mmd = ss->multires;
	int redraw = 0;

	if(mmd)
		multires_mark_as_modified(ob);

	if(sculpt_modifiers_active(scene, ob)) {
		DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
		ED_region_tag_redraw(ar);
	}
	else {
		rcti r;

		BLI_pbvh_update(ss->pbvh, PBVH_UpdateBB, NULL);
		redraw = sculpt_get_redraw_rect(ar, CTX_wm_region_view3d(C), ob, &r);

		if(redraw) {
			r.xmin += ar->winrct.xmin + 1;
			r.xmax += ar->winrct.xmin - 1;
			r.ymin += ar->winrct.ymin + 1;
			r.ymax += ar->winrct.ymin - 1;
			
			ss->partial_redraw = 1;
			ED_region_tag_redraw_partial(ar, &r);
		}
	}
}

/* Returns whether the mouse/stylus is over the mesh (1)
   or over the background (0) */
static int over_mesh(bContext *C, struct wmOperator *op, float x, float y)
{
	float mouse[2], co[3];

	mouse[0] = x;
	mouse[1] = y;

	return sculpt_stroke_get_location(C, op->customdata, co, mouse);
}

static int sculpt_stroke_test_start(bContext *C, struct wmOperator *op,
					wmEvent *event)
{
	/* Don't start the stroke until mouse goes over the mesh */
	if(over_mesh(C, op, event->x, event->y)) {
		Object *ob = CTX_data_active_object(C);
		SculptSession *ss = ob->sculpt;
		Sculpt *sd = CTX_data_tool_settings(C)->sculpt;

		ED_view3d_init_mats_rv3d(ob, CTX_wm_region_view3d(C));

		sculpt_update_cache_invariants(C, sd, ss, op, event);

		sculpt_undo_push_begin(sculpt_tool_name(sd));

#ifdef _OPENMP
		/* If using OpenMP then create a number of threads two times the
		   number of processor cores.
		   Justification: Empirically I've found that two threads per
		   processor gives higher throughput. */
		if (sd->flags & SCULPT_USE_OPENMP) {
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

static void sculpt_stroke_update_step(bContext *C, struct PaintStroke *stroke, PointerRNA *itemptr)
{
	Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
	SculptSession *ss = CTX_data_active_object(C)->sculpt;

	sculpt_stroke_modifiers_check(C, ss);
	sculpt_update_cache_variants(C, sd, ss, stroke, itemptr);
	sculpt_restore_mesh(sd, ss);
	do_symmetrical_brush_actions(sd, ss);

	/* Cleanup */
	sculpt_flush_update(C);
}

static void sculpt_stroke_done(bContext *C, struct PaintStroke *unused)
{
	Object *ob= CTX_data_active_object(C);
	SculptSession *ss = ob->sculpt;
	Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
	Brush *brush = paint_brush(&sd->paint);

	(void)unused;

	// reset values used to draw brush after completing the stroke
	brush->draw_anchored= 0;
	brush->draw_pressure= 0;

	/* Finished */
	if(ss->cache) {
		sculpt_stroke_modifiers_check(C, ss);
		
		if(brush->flag & BRUSH_SUBDIV)
			unlimited_clay(ss, ob);

		/* Alt-Smooth */
		if (ss->cache->alt_smooth) {
			Paint *p= &sd->paint;
			Brush *br;
			int i;

			for(i = 0; i < p->brush_count; ++i) {
				br = p->brushes[i];

				if (strcmp(br->id.name+2, ss->cache->saved_active_brush_name)==0) {
					paint_brush_set(p, br);
					break;
				}
			}
		}

		sculpt_cache_free(ss->cache);
		ss->cache = NULL;

		sculpt_undo_push_end();

		BLI_pbvh_update(ss->pbvh, PBVH_UpdateOriginalBB, NULL);

		if(ss->refkb) sculpt_key_to_mesh(ss->refkb, ob);

		ss->partial_redraw = 0;
		
		/* try to avoid calling this, only for e.g. linked duplicates now */
		if(((Mesh*)ob->data)->id.us > 1)
			DAG_id_flush_update(&ob->id, OB_RECALC_DATA);

		WM_event_add_notifier(C, NC_OBJECT|ND_DRAW, ob);
	}
}

static int sculpt_brush_stroke_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	struct PaintStroke *stroke;
	int ignore_background_click;

	if(!sculpt_brush_stroke_init(C, op->reports))
		return OPERATOR_CANCELLED;

	stroke = paint_stroke_new(C, sculpt_stroke_get_location,
				  sculpt_stroke_test_start,
				  sculpt_stroke_update_step,
				  sculpt_stroke_done);

	op->customdata = stroke;

	/* For tablet rotation */
	ignore_background_click = RNA_boolean_get(op->ptr,
						  "ignore_background_click"); 

	if(ignore_background_click && !over_mesh(C, op, event->x, event->y)) {
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
	SculptSession *ss = CTX_data_active_object(C)->sculpt;

	if(!sculpt_brush_stroke_init(C, op->reports))
		return OPERATOR_CANCELLED;

	op->customdata = paint_stroke_new(C, sculpt_stroke_get_location, sculpt_stroke_test_start,
					  sculpt_stroke_update_step, sculpt_stroke_done);

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

	ot->flag |= OPTYPE_REGISTER;

	/* identifiers */
	ot->name= "Sculpt Mode";
	ot->idname= "SCULPT_OT_brush_stroke";
	
	/* api callbacks */
	ot->invoke= sculpt_brush_stroke_invoke;
	ot->modal= paint_stroke_modal;
	ot->exec= sculpt_brush_stroke_exec;
	ot->poll= sculpt_poll;

	/* flags (sculpt does own undo? (ton) */
	ot->flag= OPTYPE_REGISTER|OPTYPE_BLOCKING;

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

/**** Reset the copy of the mesh that is being sculpted on (currently just for the layer brush) ****/

static int sculpt_set_persistent_base(bContext *C, wmOperator *unused)
{
	SculptSession *ss = CTX_data_active_object(C)->sculpt;

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
	
	ot->flag= OPTYPE_REGISTER;
}

/**** Toggle operator for turning sculpt mode on or off ****/

static void sculpt_init_session(Scene *scene, Object *ob)
{
	ob->sculpt = MEM_callocN(sizeof(SculptSession), "sculpt session");
	ob->sculpt->ob = ob; 	
	ob->sculpt->scene = scene;
	
	sculpt_update_mesh_elements(scene, ob, 0);

	if(ob->sculpt->refkb)
		sculpt_key_to_mesh(ob->sculpt->refkb, ob);
}

static int sculpt_toggle_mode(bContext *C, wmOperator *unused)
{
	Scene *scene = CTX_data_scene(C);
	ToolSettings *ts = CTX_data_tool_settings(C);
	Object *ob = CTX_data_active_object(C);
	MultiresModifierData *mmd= sculpt_multires_active(scene, ob);
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

		free_sculptsession(ob);
	}
	else {
		/* Enter sculptmode */
		ob->mode |= OB_MODE_SCULPT;

		if(flush_recalc)
			DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
		
		/* Create persistent sculpt mode data */
		if(!ts->sculpt)
			ts->sculpt = MEM_callocN(sizeof(Sculpt), "sculpt mode data");

		/* Create sculpt mode session data */
		if(ob->sculpt)
			free_sculptsession(ob);

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
}


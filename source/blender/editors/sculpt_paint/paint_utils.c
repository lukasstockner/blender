
#include <math.h>
#include <stdlib.h>

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"

#include "DNA_scene_types.h"
#include "DNA_brush_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "BLI_math.h"

#include "BKE_brush.h"
#include "BKE_context.h"
#include "BKE_DerivedMesh.h"
#include "BKE_modifier.h"
#include "BKE_multires.h"
#include "BKE_paint.h"


#include "BIF_gl.h"
#include "BIF_glutil.h"

#include "ED_view3d.h"
#include "ED_screen.h"

#include "BLO_sys_types.h"
#include "ED_mesh.h" /* for face mask functions */

#include "WM_api.h"
#include "WM_types.h"

#include "RE_render_ext.h"
#include "RE_shader_ext.h"

#include "paint_intern.h"

/* 3D Paint */

void ED_paint_force_update(bContext *C)
{
	Object *ob= CTX_data_active_object(C);

	if(ob && (ob->mode & (OB_MODE_SCULPT|OB_MODE_VERTEX_PAINT)))
		multires_force_update(ob);
}

static void imapaint_project(Object *ob, float *model, float *proj, float *co, float *pco)
{
	VECCOPY(pco, co);
	pco[3]= 1.0f;

	mul_m4_v3(ob->obmat, pco);
	mul_m4_v3((float(*)[4])model, pco);
	mul_m4_v4((float(*)[4])proj, pco);
}

static void imapaint_tri_weights(Object *ob, float *v1, float *v2, float *v3, float *co, float *w)
{
	float pv1[4], pv2[4], pv3[4], h[3], divw;
	float model[16], proj[16], wmat[3][3], invwmat[3][3];
	GLint view[4];

	/* compute barycentric coordinates */

	/* get the needed opengl matrices */
	glGetIntegerv(GL_VIEWPORT, view);
	glGetFloatv(GL_MODELVIEW_MATRIX, model);
	glGetFloatv(GL_PROJECTION_MATRIX, proj);
	view[0] = view[1] = 0;

	/* project the verts */
	imapaint_project(ob, model, proj, v1, pv1);
	imapaint_project(ob, model, proj, v2, pv2);
	imapaint_project(ob, model, proj, v3, pv3);

	/* do inverse view mapping, see gluProject man page */
	h[0]= (co[0] - view[0])*2.0f/view[2] - 1;
	h[1]= (co[1] - view[1])*2.0f/view[3] - 1;
	h[2]= 1.0f;

	/* solve for(w1,w2,w3)/perspdiv in:
	   h*perspdiv = Project*Model*(w1*v1 + w2*v2 + w3*v3) */

	wmat[0][0]= pv1[0];  wmat[1][0]= pv2[0];  wmat[2][0]= pv3[0];
	wmat[0][1]= pv1[1];  wmat[1][1]= pv2[1];  wmat[2][1]= pv3[1];
	wmat[0][2]= pv1[3];  wmat[1][2]= pv2[3];  wmat[2][2]= pv3[3];

	invert_m3_m3(invwmat, wmat);
	mul_m3_v3(invwmat, h);

	VECCOPY(w, h);

	/* w is still divided by perspdiv, make it sum to one */
	divw= w[0] + w[1] + w[2];
	if(divw != 0.0f)
		mul_v3_fl(w, 1.0f/divw);
}

/* compute uv coordinates of mouse in face */
void imapaint_pick_uv(Scene *scene, Object *ob, unsigned int faceindex, int *xy, float *uv)
{
	DerivedMesh *dm = mesh_get_derived_final(scene, ob, CD_MASK_BAREMESH);
	int *index = dm->getFaceDataArray(dm, CD_ORIGINDEX);
	MTFace *tface = dm->getFaceDataArray(dm, CD_MTFACE), *tf;
	int numfaces = dm->getNumFaces(dm), a, findex;
	float p[2], w[3], absw, minabsw;
	MFace mf;
	MVert mv[4];

	minabsw = 1e10;
	uv[0] = uv[1] = 0.0;

	/* test all faces in the derivedmesh with the original index of the picked face */
	for(a = 0; a < numfaces; a++) {
		findex= (index)? index[a]: a;

		if(findex == faceindex) {
			dm->getFace(dm, a, &mf);

			dm->getVert(dm, mf.v1, &mv[0]);
			dm->getVert(dm, mf.v2, &mv[1]);
			dm->getVert(dm, mf.v3, &mv[2]);
			if(mf.v4)
				dm->getVert(dm, mf.v4, &mv[3]);

			tf= &tface[a];

			p[0]= xy[0];
			p[1]= xy[1];

			if(mf.v4) {
				/* the triangle with the largest absolute values is the one
				   with the most negative weights */
				imapaint_tri_weights(ob, mv[0].co, mv[1].co, mv[3].co, p, w);
				absw= fabs(w[0]) + fabs(w[1]) + fabs(w[2]);
				if(absw < minabsw) {
					uv[0]= tf->uv[0][0]*w[0] + tf->uv[1][0]*w[1] + tf->uv[3][0]*w[2];
					uv[1]= tf->uv[0][1]*w[0] + tf->uv[1][1]*w[1] + tf->uv[3][1]*w[2];
					minabsw = absw;
				}

				imapaint_tri_weights(ob, mv[1].co, mv[2].co, mv[3].co, p, w);
				absw= fabs(w[0]) + fabs(w[1]) + fabs(w[2]);
				if(absw < minabsw) {
					uv[0]= tf->uv[1][0]*w[0] + tf->uv[2][0]*w[1] + tf->uv[3][0]*w[2];
					uv[1]= tf->uv[1][1]*w[0] + tf->uv[2][1]*w[1] + tf->uv[3][1]*w[2];
					minabsw = absw;
				}
			}
			else {
				imapaint_tri_weights(ob, mv[0].co, mv[1].co, mv[2].co, p, w);
				absw= fabs(w[0]) + fabs(w[1]) + fabs(w[2]);
				if(absw < minabsw) {
					uv[0]= tf->uv[0][0]*w[0] + tf->uv[1][0]*w[1] + tf->uv[2][0]*w[2];
					uv[1]= tf->uv[0][1]*w[0] + tf->uv[1][1]*w[1] + tf->uv[2][1]*w[2];
					minabsw = absw;
				}
			}
		}
	}

	dm->release(dm);
}

///* returns 0 if not found, otherwise 1 */
int imapaint_pick_face(ViewContext *vc, Mesh *me, int *mval, unsigned int *index)
{
	if(!me || me->totface==0)
		return 0;

	/* sample only on the exact position */
	*index = view3d_sample_backbuf(vc, mval[0], mval[1]);

	if((*index)<=0 || (*index)>(unsigned int)me->totface)
		return 0;

	(*index)--;
	
	return 1;
}

/* used for both 3d view and image window */
void paint_sample_color(Scene *scene, ARegion *ar, int x, int y)	/* frontbuf */
{
	Brush *br = paint_brush(paint_get_active(scene));
	unsigned int col;
	char *cp;

	CLAMP(x, 0, ar->winx);
	CLAMP(y, 0, ar->winy);
	
	glReadBuffer(GL_FRONT);
	glReadPixels(x+ar->winrct.xmin, y+ar->winrct.ymin, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &col);
	glReadBuffer(GL_BACK);

	cp = (char *)&col;
	
	if(br) {
		br->rgb[0]= cp[0]/255.0f;
		br->rgb[1]= cp[1]/255.0f;
		br->rgb[2]= cp[2]/255.0f;
	}
}

static int brush_curve_preset_exec(bContext *C, wmOperator *op)
{
	Brush *br = paint_brush(paint_get_active(CTX_data_scene(C)));
	brush_curve_preset(br, RNA_enum_get(op->ptr, "shape"));

	return OPERATOR_FINISHED;
}

static int brush_curve_preset_poll(bContext *C)
{
	Brush *br = paint_brush(paint_get_active(CTX_data_scene(C)));

	return br && br->curve;
}

void BRUSH_OT_curve_preset(wmOperatorType *ot)
{
	static EnumPropertyItem prop_shape_items[] = {
		{CURVE_PRESET_SHARP, "SHARP", 0, "Sharp", ""},
		{CURVE_PRESET_SMOOTH, "SMOOTH", 0, "Smooth", ""},
		{CURVE_PRESET_MAX, "MAX", 0, "Max", ""},
		{CURVE_PRESET_LINE, "LINE", 0, "Line", ""},
		{CURVE_PRESET_ROUND, "ROUND", 0, "Round", ""},
		{CURVE_PRESET_ROOT, "ROOT", 0, "Root", ""},
		{0, NULL, 0, NULL, NULL}};

	ot->name= "Preset";
	ot->description= "Set brush shape";
	ot->idname= "BRUSH_OT_curve_preset";

	ot->exec= brush_curve_preset_exec;
	ot->poll= brush_curve_preset_poll;

	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	RNA_def_enum(ot->srna, "shape", prop_shape_items, CURVE_PRESET_SMOOTH, "Mode", "");
}


/* face-select ops */
static int paint_select_linked_exec(bContext *C, wmOperator *UNUSED(op))
{
	select_linked_tfaces(C, CTX_data_active_object(C), NULL, 2);
	ED_region_tag_redraw(CTX_wm_region(C));
	return OPERATOR_FINISHED;
}

void PAINT_OT_face_select_linked(wmOperatorType *ot)
{
	ot->name= "Select Linked";
	ot->description= "Select linked faces";
	ot->idname= "PAINT_OT_face_select_linked";

	ot->exec= paint_select_linked_exec;
	ot->poll= facemask_paint_poll;

	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

static int paint_select_linked_pick_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	int mode= RNA_boolean_get(op->ptr, "extend") ? 1:0;
	select_linked_tfaces(C, CTX_data_active_object(C), event->mval, mode);
	ED_region_tag_redraw(CTX_wm_region(C));
	return OPERATOR_FINISHED;
}

void PAINT_OT_face_select_linked_pick(wmOperatorType *ot)
{
	ot->name= "Select Linked Pick";
	ot->description= "Select linked faces";
	ot->idname= "PAINT_OT_face_select_linked_pick";

	ot->invoke= paint_select_linked_pick_invoke;
	ot->poll= facemask_paint_poll;

	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	RNA_def_boolean(ot->srna, "extend", 0, "Extend", "Extend the existing selection");
}


static int face_select_all_exec(bContext *C, wmOperator *op)
{
	selectall_tface(CTX_data_active_object(C), RNA_enum_get(op->ptr, "action"));
	ED_region_tag_redraw(CTX_wm_region(C));
	return OPERATOR_FINISHED;
}


void PAINT_OT_face_select_all(wmOperatorType *ot)
{
	ot->name= "Face Selection";
	ot->description= "Change selection for all faces";
	ot->idname= "PAINT_OT_face_select_all";

	ot->exec= face_select_all_exec;
	ot->poll= facemask_paint_poll;

	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;

	WM_operator_properties_select_all(ot);
}

float paint_calc_object_space_radius(ViewContext *vc, float center[3],
				     float pixel_radius)
{
	Object *ob = vc->obact;
	float delta[3], scale, loc[3];

	mul_v3_m4v3(loc, ob->obmat, center);

	initgrabz(vc->rv3d, loc[0], loc[1], loc[2]);
	window_to_3d_delta(vc->ar, delta, pixel_radius, 0);

	scale= fabsf(mat4_to_scale(ob->obmat));
	scale= (scale == 0.0f)? 1.0f: scale;

	return len_v3(delta)/scale;
}

/* Paint modes can handle multires differently from regular meshes, but only
   if it's the last modifier on the stack and it is not on level zero */
struct MultiresModifierData *ED_paint_multires_active(Scene *scene, Object *ob)
{
	Mesh *me= (Mesh*)ob->data;
	ModifierData *md, *nmd;

	if(!CustomData_get_layer(&me->fdata, CD_MDISPS)) {
		/* multires can't work without displacement layer */
		return NULL;
	}
	
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

/*** BVH Tree ***/

/* Get a screen-space rectangle of the modified area */
static int paint_get_redraw_rect(ARegion *ar, RegionView3D *rv3d,
				 Object *ob, rcti *rect)
{
	PBVH *pbvh= ob->paint->pbvh;
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

void paint_tag_partial_redraw(bContext *C, Object *ob)
{
	RegionView3D *rv3d = CTX_wm_region_view3d(C);
	ARegion *ar = CTX_wm_region(C);
	rcti r;

	if(paint_get_redraw_rect(ar, rv3d, ob, &r)) {
		//rcti tmp;

		r.xmin += ar->winrct.xmin + 1;
		r.xmax += ar->winrct.xmin - 1;
		r.ymin += ar->winrct.ymin + 1;
		r.ymax += ar->winrct.ymin - 1;

		//tmp = r;

		//if (!BLI_rcti_is_empty(&ss->previous_r))
		//	BLI_union_rcti(&r, &ss->previous_r);

		//ss->previous_r= tmp;

		ob->paint->partial_redraw = 1;
		ED_region_tag_redraw_partial(ar, &r);
	}
}

void paint_get_redraw_planes(float planes[4][4], ARegion *ar,
			     RegionView3D *rv3d, Object *ob)
{
	PBVH *pbvh= ob->paint->pbvh;
	BoundBox bb;
	bglMats mats;
	rcti rect;

	memset(&bb, 0, sizeof(BoundBox));

	view3d_get_transformation(ar, rv3d, ob, &mats);
	paint_get_redraw_rect(ar, rv3d,ob, &rect);

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

/* selectively flip any axis of a coordinate */
void paint_flip_coord(float out[3], float in[3], const char symm)
{
	if(symm & PAINT_SYMM_X)
		out[0]= -in[0];
	else
		out[0]= in[0];
	if(symm & PAINT_SYMM_Y)
		out[1]= -in[1];
	else
		out[1]= in[1];
	if(symm & PAINT_SYMM_Z)
		out[2]= -in[2];
	else
		out[2]= in[2];
}

/* return a multiplier for brush strength at a coordinate,
   incorporating texture, curve control, and masking

   TODO: pulled almost directly from sculpt, still needs
   to be prettied up
*/
float brush_tex_strength(ViewContext *vc,
			 float pmat[4][4], Brush *br,
			 float co[3], float mask, const float len,
			 float pixel_radius, float radius3d,
			 float special_rotation, float tex_mouse[2])
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
		externtex(mtex, co, &avg,
			  &jnk, &jnk, &jnk, &jnk, 0);
	}
	else {
		float rotation = -mtex->rot;
		float x, y, point_2d[3];
		float radius;

		view3d_project_float(vc->ar, co, point_2d, pmat);

		/* if fixed mode, keep coordinates relative to mouse */
		if(mtex->brush_map_mode == MTEX_MAP_MODE_FIXED) {
			rotation += special_rotation;

			point_2d[0] -= tex_mouse[0];
			point_2d[1] -= tex_mouse[1];

			radius = pixel_radius; // use pressure adjusted size for fixed mode

			x = point_2d[0];
			y = point_2d[1];
		}
		else /* else (mtex->brush_map_mode == MTEX_MAP_MODE_TILED),
		        leave the coordinates relative to the screen */
		{
			radius = brush_size(br); // use unadjusted size for tiled mode
		
			x = point_2d[0] - vc->ar->winrct.xmin;
			y = point_2d[1] - vc->ar->winrct.ymin;
		}

		x /= vc->ar->winx;
		y /= vc->ar->winy;

		if (mtex->brush_map_mode == MTEX_MAP_MODE_TILED) {
			x -= 0.5f;
			y -= 0.5f;
		}
		
		x *= vc->ar->winx / radius;
		y *= vc->ar->winy / radius;

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

	avg *= brush_curve_strength(br, len, radius3d); /* Falloff curve */
	avg*= 1 - mask;
	
	return avg;
}

int paint_util_raycast(ViewContext *vc,
		       BLI_pbvh_HitOccludedCallback hit_cb, void *mode_data,
		       float out[3], float mouse[2], int original)
{
	float ray_start[3], ray_end[3], ray_normal[3], dist;
	float obimat[4][4];
	float mval[2] = {mouse[0] - vc->ar->winrct.xmin,
			 mouse[1] - vc->ar->winrct.ymin};
	PaintStrokeRaycastData hit_data;

	viewline(vc->ar, vc->v3d, mval, ray_start, ray_end);

	invert_m4_m4(obimat, vc->obact->obmat);
	mul_m4_v3(obimat, ray_start);
	mul_m4_v3(obimat, ray_end);

	sub_v3_v3v3(ray_normal, ray_end, ray_start);
	dist= normalize_v3(ray_normal);

	hit_data.mode_data = mode_data;
	hit_data.ob = vc->obact;
	hit_data.ray_start = ray_start;
	hit_data.ray_normal = ray_normal;
	hit_data.dist = dist;
	hit_data.hit = 0;
	hit_data.original = original;
	BLI_pbvh_raycast(vc->obact->paint->pbvh, hit_cb, &hit_data,
			 ray_start, ray_normal, original);
	
	copy_v3_v3(out, ray_normal);
	mul_v3_fl(out, hit_data.dist);
	add_v3_v3(out, ray_start);

	return hit_data.hit;
}

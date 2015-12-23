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
 * The Original Code is Copyright (C) 2005 Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/transform/transform_manipulator.c
 *  \ingroup edtransform
 */


#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

#include "DNA_armature_types.h"
#include "DNA_curve_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_lattice_types.h"
#include "DNA_meta_types.h"
#include "DNA_screen_types.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "RNA_access.h"

#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_global.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_editmesh.h"
#include "BKE_lattice.h"

#include "BIF_gl.h"

#include "WM_api.h"
#include "WM_types.h"
#include "wm.h"

#include "ED_armature.h"
#include "ED_curve.h"
#include "ED_object.h"
#include "ED_particle.h"
#include "ED_view3d.h"
#include "ED_screen.h"

#include "UI_resources.h"
#include "UI_interface.h"

/* local module include */
#include "transform.h"

#include "MEM_guardedalloc.h"

#include "GPU_select.h"


/* drawing flags */

#define MAN_TRANS_X  (1 << 0)
#define MAN_TRANS_Y  (1 << 1)
#define MAN_TRANS_Z  (1 << 2)
#define MAN_TRANS_C  (MAN_TRANS_X | MAN_TRANS_Y | MAN_TRANS_Z)

#define MAN_ROT_X    (1 << 3)
#define MAN_ROT_Y    (1 << 4)
#define MAN_ROT_Z    (1 << 5)
#define MAN_ROT_C    (MAN_ROT_X | MAN_ROT_Y | MAN_ROT_Z)

#define MAN_SCALE_X  (1 << 8)
#define MAN_SCALE_Y  (1 << 9)
#define MAN_SCALE_Z  (1 << 10)
#define MAN_SCALE_C  (MAN_SCALE_X | MAN_SCALE_Y | MAN_SCALE_Z)

/* threshold for testing view aligned manipulator axis */
#define TW_AXIS_DOT_MIN 0.02f
#define TW_AXIS_DOT_MAX 0.1f

#define MAN_AXIS_LINE_WIDTH 2.0

/* axes as index */
enum {
	MAN_AXIS_TRANS_X = 0,
	MAN_AXIS_TRANS_Y,
	MAN_AXIS_TRANS_Z,
	MAN_AXIS_TRANS_C,

	MAN_AXIS_ROT_X,
	MAN_AXIS_ROT_Y,
	MAN_AXIS_ROT_Z,
	MAN_AXIS_ROT_C,

	MAN_AXIS_SCALE_X,
	MAN_AXIS_SCALE_Y,
	MAN_AXIS_SCALE_Z,
	MAN_AXIS_SCALE_C,

	/* special */
	MAN_AXIS_TRANS_XY,
	MAN_AXIS_TRANS_YZ,
	MAN_AXIS_TRANS_ZX,

	MAN_AXIS_SCALE_XY,
	MAN_AXIS_SCALE_YZ,
	MAN_AXIS_SCALE_ZX,

	MAN_AXIS_LAST,
};

/* axis types */
enum {
	MAN_AXES_ALL = 0,
	MAN_AXES_TRANSLATE,
	MAN_AXES_ROTATE,
	MAN_AXES_SCALE,
};

typedef struct ManipulatorGroup {
	wmWidget *translate_x,
	         *translate_y,
	         *translate_z,
	         *translate_xy,
	         *translate_yz,
	         *translate_zx,
	         *translate_c,

	         *rotate_x,
	         *rotate_y,
	         *rotate_z,
	         *rotate_c,

	         *scale_x,
	         *scale_y,
	         *scale_z,
	         *scale_xy,
	         *scale_yz,
	         *scale_zx,
	         *scale_c;
} ManipulatorGroup;


/* **************** Utilities **************** */

/* loop over axes */
#define MAN_ITER_AXES_BEGIN \
	{ \
		wmWidget *axis; \
		int axis_idx; \
		for (axis_idx = 0; axis_idx < MAN_AXIS_LAST; axis_idx++) { \
			axis = manipulator_get_axis_from_index(man, axis_idx); \
			if (!axis) continue;

#define MAN_ITER_AXES_END \
		} \
	} ((void)0)

static wmWidget *manipulator_get_axis_from_index(const ManipulatorGroup *man, const short axis_idx)
{
	BLI_assert(IN_RANGE_INCL(axis_idx, (float)MAN_AXIS_TRANS_X, (float)MAN_AXIS_LAST));

	switch (axis_idx) {
		case MAN_AXIS_TRANS_X:
			return man->translate_x;
		case MAN_AXIS_TRANS_Y:
			return man->translate_y;
		case MAN_AXIS_TRANS_Z:
			return man->translate_z;
		case MAN_AXIS_TRANS_XY:
			return man->translate_xy;
		case MAN_AXIS_TRANS_YZ:
			return man->translate_yz;
		case MAN_AXIS_TRANS_ZX:
			return man->translate_zx;
		case MAN_AXIS_TRANS_C:
			return man->translate_c;
		case MAN_AXIS_ROT_X:
			return man->rotate_x;
		case MAN_AXIS_ROT_Y:
			return man->rotate_y;
		case MAN_AXIS_ROT_Z:
			return man->rotate_z;
		case MAN_AXIS_ROT_C:
			return man->rotate_c;
		case MAN_AXIS_SCALE_X:
			return man->scale_x;
		case MAN_AXIS_SCALE_Y:
			return man->scale_y;
		case MAN_AXIS_SCALE_Z:
			return man->scale_z;
		case MAN_AXIS_SCALE_XY:
			return man->scale_xy;
		case MAN_AXIS_SCALE_YZ:
			return man->scale_yz;
		case MAN_AXIS_SCALE_ZX:
			return man->scale_zx;
		case MAN_AXIS_SCALE_C:
			return man->scale_c;
	}

	return NULL;
}

static short manipulator_get_axis_type(const ManipulatorGroup *man, const wmWidget *axis)
{
	if (ELEM(axis, man->translate_x, man->translate_y, man->translate_z, man->translate_c,
	         man->translate_xy, man->translate_yz, man->translate_zx))
	{
		return MAN_AXES_TRANSLATE;
	}
	else if (ELEM(axis, man->rotate_x, man->rotate_y, man->rotate_z, man->rotate_c)) {
		return MAN_AXES_ROTATE;
	}
	else {
		return MAN_AXES_SCALE;
	}
}

/* get index within axis type, so that x == 0, y == 1 and z == 2, no matter which axis type */
static int manipulator_index_normalize(const int axis_idx)
{
	if (axis_idx > MAN_AXIS_TRANS_ZX) {
		return axis_idx - 15;
	}
	else if (axis_idx > MAN_AXIS_SCALE_C) {
		return axis_idx - 12;
	}
	else if (axis_idx > MAN_AXIS_ROT_C) {
		return axis_idx - 8;
	}
	else if (axis_idx > MAN_AXIS_TRANS_C) {
		return axis_idx - 4;
	}

	return axis_idx;
}

static bool manipulator_is_axis_visible(const View3D *v3d, const RegionView3D *rv3d, const int axis_idx)
{
	switch (axis_idx) {
		case MAN_AXIS_TRANS_X:
			return (rv3d->twdrawflag & MAN_TRANS_X);
		case MAN_AXIS_TRANS_Y:
			return (rv3d->twdrawflag & MAN_TRANS_Y);
		case MAN_AXIS_TRANS_Z:
			return (rv3d->twdrawflag & MAN_TRANS_Z);
		case MAN_AXIS_TRANS_C:
			return (rv3d->twdrawflag & MAN_TRANS_C);
		case MAN_AXIS_ROT_X:
			return (rv3d->twdrawflag & MAN_ROT_X);
		case MAN_AXIS_ROT_Y:
			return (rv3d->twdrawflag & MAN_ROT_Y);
		case MAN_AXIS_ROT_Z:
			return (rv3d->twdrawflag & MAN_ROT_Z);
		case MAN_AXIS_ROT_C:
			return (rv3d->twdrawflag & MAN_ROT_C);
		case MAN_AXIS_SCALE_X:
			return (rv3d->twdrawflag & MAN_SCALE_X);
		case MAN_AXIS_SCALE_Y:
			return (rv3d->twdrawflag & MAN_SCALE_Y);
		case MAN_AXIS_SCALE_Z:
			return (rv3d->twdrawflag & MAN_SCALE_Z);
		case MAN_AXIS_SCALE_C:
			return (rv3d->twdrawflag & MAN_SCALE_C && (v3d->twtype & V3D_MANIP_TRANSLATE) == 0);
		case MAN_AXIS_TRANS_XY:
			return (rv3d->twdrawflag & MAN_TRANS_X &&
			        rv3d->twdrawflag & MAN_TRANS_Y &&
			        (v3d->twtype & V3D_MANIP_ROTATE) == 0);
		case MAN_AXIS_TRANS_YZ:
			return (rv3d->twdrawflag & MAN_TRANS_Y &&
			        rv3d->twdrawflag & MAN_TRANS_Z &&
			        (v3d->twtype & V3D_MANIP_ROTATE) == 0);
		case MAN_AXIS_TRANS_ZX:
			return (rv3d->twdrawflag & MAN_TRANS_Z &&
			        rv3d->twdrawflag & MAN_TRANS_X &&
			        (v3d->twtype & V3D_MANIP_ROTATE) == 0);
		case MAN_AXIS_SCALE_XY:
			return (rv3d->twdrawflag & MAN_SCALE_X &&
			        rv3d->twdrawflag & MAN_SCALE_Y &&
			        (v3d->twtype & V3D_MANIP_TRANSLATE) == 0 &&
			        (v3d->twtype & V3D_MANIP_ROTATE) == 0);
		case MAN_AXIS_SCALE_YZ:
			return (rv3d->twdrawflag & MAN_SCALE_Y &&
			        rv3d->twdrawflag & MAN_SCALE_Z &&
			        (v3d->twtype & V3D_MANIP_TRANSLATE) == 0 &&
			        (v3d->twtype & V3D_MANIP_ROTATE) == 0);
		case MAN_AXIS_SCALE_ZX:
			return (rv3d->twdrawflag & MAN_SCALE_Z &&
			        rv3d->twdrawflag & MAN_SCALE_X &&
			        (v3d->twtype & V3D_MANIP_TRANSLATE) == 0 &&
			        (v3d->twtype & V3D_MANIP_ROTATE) == 0);
	}
	return false;
}

static void manipulator_get_axis_color(const RegionView3D *rv3d, const int axis_idx, float r_col[4], float r_col_hi[4])
{
	const float idot = rv3d->tw_idot[manipulator_index_normalize(axis_idx)];
	/* alpha values for normal/highlighted states */
	const float alpha = 0.6f;
	const float alpha_hi = 1.0f;
	/* get alpha fac based on axis angle, to fade axis out when hiding it because it points towards view */
	float alpha_fac_view = (idot > TW_AXIS_DOT_MAX) ?
	                        1.0f : (idot < TW_AXIS_DOT_MIN) ?
	                        0.0f : ((idot - TW_AXIS_DOT_MIN) / (TW_AXIS_DOT_MAX - TW_AXIS_DOT_MIN));

	switch (axis_idx) {
		case MAN_AXIS_TRANS_X:
		case MAN_AXIS_ROT_X:
		case MAN_AXIS_SCALE_X:
		case MAN_AXIS_TRANS_XY:
		case MAN_AXIS_SCALE_XY:
			UI_GetThemeColor4fv(TH_AXIS_X, r_col);
			break;
		case MAN_AXIS_TRANS_Y:
		case MAN_AXIS_ROT_Y:
		case MAN_AXIS_SCALE_Y:
		case MAN_AXIS_TRANS_YZ:
		case MAN_AXIS_SCALE_YZ:
			UI_GetThemeColor4fv(TH_AXIS_Y, r_col);
			break;
		case MAN_AXIS_TRANS_Z:
		case MAN_AXIS_ROT_Z:
		case MAN_AXIS_SCALE_Z:
		case MAN_AXIS_TRANS_ZX:
		case MAN_AXIS_SCALE_ZX:
			UI_GetThemeColor4fv(TH_AXIS_Z, r_col);
			break;
		case MAN_AXIS_TRANS_C:
		case MAN_AXIS_ROT_C:
		case MAN_AXIS_SCALE_C:
			copy_v4_fl(r_col, 1.0f);
			alpha_fac_view = 1.0f;
			break;
	}

	copy_v4_v4(r_col_hi, r_col);

	r_col[3] = alpha * alpha_fac_view;
	r_col_hi[3] = alpha_hi * alpha_fac_view;
}

static void manipulator_get_axis_constraint(const int axis_idx, int r_axis[3])
{
	zero_v3_int(r_axis);

	switch (axis_idx) {
		case MAN_AXIS_TRANS_X:
		case MAN_AXIS_ROT_X:
		case MAN_AXIS_SCALE_X:
			r_axis[0] = 1;
			break;
		case MAN_AXIS_TRANS_Y:
		case MAN_AXIS_ROT_Y:
		case MAN_AXIS_SCALE_Y:
			r_axis[1] = 1;
			break;
		case MAN_AXIS_TRANS_Z:
		case MAN_AXIS_ROT_Z:
		case MAN_AXIS_SCALE_Z:
			r_axis[2] = 1;
			break;
		case MAN_AXIS_TRANS_C:
		case MAN_AXIS_ROT_C:
		case MAN_AXIS_SCALE_C:
			break;
		case MAN_AXIS_TRANS_XY:
		case MAN_AXIS_SCALE_XY:
			r_axis[0] = r_axis[1] = 1;
			break;
		case MAN_AXIS_TRANS_YZ:
		case MAN_AXIS_SCALE_YZ:
			r_axis[1] = r_axis[2] = 1;
			break;
		case MAN_AXIS_TRANS_ZX:
		case MAN_AXIS_SCALE_ZX:
			r_axis[2] = r_axis[0] = 1;
			break;
	}
}


/* **************** Preparation Stuff **************** */

/* transform widget center calc helper for below */
static void calc_tw_center(Scene *scene, const float co[3])
{
	float *twcent = scene->twcent;
	float *min = scene->twmin;
	float *max = scene->twmax;

	minmax_v3v3_v3(min, max, co);
	add_v3_v3(twcent, co);
}

static void protectflag_to_drawflags(short protectflag, short *drawflags)
{
	if (protectflag & OB_LOCK_LOCX)
		*drawflags &= ~MAN_TRANS_X;
	if (protectflag & OB_LOCK_LOCY)
		*drawflags &= ~MAN_TRANS_Y;
	if (protectflag & OB_LOCK_LOCZ)
		*drawflags &= ~MAN_TRANS_Z;

	if (protectflag & OB_LOCK_ROTX)
		*drawflags &= ~MAN_ROT_X;
	if (protectflag & OB_LOCK_ROTY)
		*drawflags &= ~MAN_ROT_Y;
	if (protectflag & OB_LOCK_ROTZ)
		*drawflags &= ~MAN_ROT_Z;

	if (protectflag & OB_LOCK_SCALEX)
		*drawflags &= ~MAN_SCALE_X;
	if (protectflag & OB_LOCK_SCALEY)
		*drawflags &= ~MAN_SCALE_Y;
	if (protectflag & OB_LOCK_SCALEZ)
		*drawflags &= ~MAN_SCALE_Z;
}

/* for pose mode */
static void stats_pose(Scene *scene, Object *ob, RegionView3D *rv3d, bPoseChannel *pchan)
{
	Bone *bone = pchan->bone;

	if (bone) {
		/* update pose matrix after transform */
		BKE_pose_where_is(scene, ob);

		calc_tw_center(scene, pchan->pose_head);
		protectflag_to_drawflags(pchan->protectflag, &rv3d->twdrawflag);
	}
}

/* for editmode*/
static void stats_editbone(RegionView3D *rv3d, const EditBone *ebo)
{
	if (ebo->flag & BONE_EDITMODE_LOCKED)
		protectflag_to_drawflags(OB_LOCK_LOC | OB_LOCK_ROT | OB_LOCK_SCALE, &rv3d->twdrawflag);
}

/* could move into BLI_math however this is only useful for display/editing purposes */
static void axis_angle_to_gimbal_axis(float gmat[3][3], const float axis[3], const float angle)
{
	/* X/Y are arbitrary axies, most importantly Z is the axis of rotation */

	float cross_vec[3];
	float quat[4];

	/* this is an un-scientific method to get a vector to cross with
	 * XYZ intentionally YZX */
	cross_vec[0] = axis[1];
	cross_vec[1] = axis[2];
	cross_vec[2] = axis[0];

	/* X-axis */
	cross_v3_v3v3(gmat[0], cross_vec, axis);
	normalize_v3(gmat[0]);
	axis_angle_to_quat(quat, axis, angle);
	mul_qt_v3(quat, gmat[0]);

	/* Y-axis */
	axis_angle_to_quat(quat, axis, M_PI_2);
	copy_v3_v3(gmat[1], gmat[0]);
	mul_qt_v3(quat, gmat[1]);

	/* Z-axis */
	copy_v3_v3(gmat[2], axis);

	normalize_m3(gmat);
}


static int test_rotmode_euler(short rotmode)
{
	return (ELEM(rotmode, ROT_MODE_AXISANGLE, ROT_MODE_QUAT)) ? 0 : 1;
}

bool gimbal_axis(Object *ob, float gmat[3][3])
{
	if (ob->mode & OB_MODE_POSE) {
		bPoseChannel *pchan = BKE_pose_channel_active(ob);

		if (pchan) {
			float mat[3][3], tmat[3][3], obmat[3][3];
			if (test_rotmode_euler(pchan->rotmode)) {
				eulO_to_gimbal_axis(mat, pchan->eul, pchan->rotmode);
			}
			else if (pchan->rotmode == ROT_MODE_AXISANGLE) {
				axis_angle_to_gimbal_axis(mat, pchan->rotAxis, pchan->rotAngle);
			}
			else { /* quat */
				return 0;
			}


			/* apply bone transformation */
			mul_m3_m3m3(tmat, pchan->bone->bone_mat, mat);

			if (pchan->parent) {
				float parent_mat[3][3];

				copy_m3_m4(parent_mat, pchan->parent->pose_mat);
				mul_m3_m3m3(mat, parent_mat, tmat);

				/* needed if object transformation isn't identity */
				copy_m3_m4(obmat, ob->obmat);
				mul_m3_m3m3(gmat, obmat, mat);
			}
			else {
				/* needed if object transformation isn't identity */
				copy_m3_m4(obmat, ob->obmat);
				mul_m3_m3m3(gmat, obmat, tmat);
			}

			normalize_m3(gmat);
			return 1;
		}
	}
	else {
		if (test_rotmode_euler(ob->rotmode)) {
			eulO_to_gimbal_axis(gmat, ob->rot, ob->rotmode);
		}
		else if (ob->rotmode == ROT_MODE_AXISANGLE) {
			axis_angle_to_gimbal_axis(gmat, ob->rotAxis, ob->rotAngle);
		}
		else { /* quat */
			return 0;
		}

		if (ob->parent) {
			float parent_mat[3][3];
			copy_m3_m4(parent_mat, ob->parent->obmat);
			normalize_m3(parent_mat);
			mul_m3_m3m3(gmat, parent_mat, gmat);
		}
		return 1;
	}

	return 0;
}


/* centroid, boundbox, of selection */
/* returns total items selected */
static int calc_manipulator_stats(const bContext *C)
{
	const ScrArea *sa = CTX_wm_area(C);
	const ARegion *ar = CTX_wm_region(C);
	Scene *scene = CTX_data_scene(C);
	Object *obedit = CTX_data_edit_object(C);
	View3D *v3d = sa->spacedata.first;
	RegionView3D *rv3d = ar->regiondata;
	Base *base;
	Object *ob = OBACT;
	bGPdata *gpd = CTX_data_gpencil_data(C);
	const bool is_gp_edit = ((gpd) && (gpd->flag & GP_DATA_STROKE_EDITMODE));
	int a, totsel = 0;

	/* transform widget matrix */
	unit_m4(rv3d->twmat);

	rv3d->twdrawflag = 0xFFFF;

	/* transform widget centroid/center */
	INIT_MINMAX(scene->twmin, scene->twmax);
	zero_v3(scene->twcent);
	
	if (is_gp_edit) {
		CTX_DATA_BEGIN(C, bGPDstroke *, gps, editable_gpencil_strokes)
		{
			/* we're only interested in selected points here... */
			if (gps->flag & GP_STROKE_SELECT) {
				bGPDspoint *pt;
				int i;
				
				/* Change selection status of all points, then make the stroke match */
				for (i = 0, pt = gps->points; i < gps->totpoints; i++, pt++) {
					if (pt->flag & GP_SPOINT_SELECT) {
						calc_tw_center(scene, &pt->x);
						totsel++;
					}
				}
			}
		}
		CTX_DATA_END;
		
		/* selection center */
		if (totsel) {
			mul_v3_fl(scene->twcent, 1.0f / (float)totsel);   /* centroid! */
		}
	}
	else if (obedit) {
		ob = obedit;
		if ((ob->lay & v3d->lay) == 0) return 0;

		if (obedit->type == OB_MESH) {
			BMEditMesh *em = BKE_editmesh_from_object(obedit);
			BMEditSelection ese;
			float vec[3] = {0, 0, 0};

			/* USE LAST SELECTE WITH ACTIVE */
			if ((v3d->around == V3D_AROUND_ACTIVE) && BM_select_history_active_get(em->bm, &ese)) {
				BM_editselection_center(&ese, vec);
				calc_tw_center(scene, vec);
				totsel = 1;
			}
			else {
				BMesh *bm = em->bm;
				BMVert *eve;

				BMIter iter;

				BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
					if (!BM_elem_flag_test(eve, BM_ELEM_HIDDEN)) {
						if (BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
							totsel++;
							calc_tw_center(scene, eve->co);
						}
					}
				}
			}
		} /* end editmesh */
		else if (obedit->type == OB_ARMATURE) {
			const bArmature *arm = obedit->data;
			EditBone *ebo;

			if ((v3d->around == V3D_AROUND_ACTIVE) && (ebo = arm->act_edbone)) {
				/* doesn't check selection or visibility intentionally */
				if (ebo->flag & BONE_TIPSEL) {
					calc_tw_center(scene, ebo->tail);
					totsel++;
				}
				if ((ebo->flag & BONE_ROOTSEL) ||
				    ((ebo->flag & BONE_TIPSEL) == false))  /* ensure we get at least one point */
				{
					calc_tw_center(scene, ebo->head);
					totsel++;
				}
				stats_editbone(rv3d, ebo);
			}
			else {
				for (ebo = arm->edbo->first; ebo; ebo = ebo->next) {
					if (EBONE_VISIBLE(arm, ebo)) {
						if (ebo->flag & BONE_TIPSEL) {
							calc_tw_center(scene, ebo->tail);
							totsel++;
						}
						if (ebo->flag & BONE_ROOTSEL) {
							calc_tw_center(scene, ebo->head);
							totsel++;
						}
						if (ebo->flag & BONE_SELECTED) {
							stats_editbone(rv3d, ebo);
						}
					}
				}
			}
		}
		else if (ELEM(obedit->type, OB_CURVE, OB_SURF)) {
			Curve *cu = obedit->data;
			float center[3];

			if (v3d->around == V3D_AROUND_ACTIVE && ED_curve_active_center(cu, center)) {
				calc_tw_center(scene, center);
				totsel++;
			}
			else {
				Nurb *nu;
				BezTriple *bezt;
				BPoint *bp;
				const ListBase *nurbs = BKE_curve_editNurbs_get(cu);

				nu = nurbs->first;
				while (nu) {
					if (nu->type == CU_BEZIER) {
						bezt = nu->bezt;
						a = nu->pntsu;
						while (a--) {
							/* exceptions
							 * if handles are hidden then only check the center points.
							 * If the center knot is selected then only use this as the center point.
							 */
							if (cu->drawflag & CU_HIDE_HANDLES) {
								if (bezt->f2 & SELECT) {
									calc_tw_center(scene, bezt->vec[1]);
									totsel++;
								}
							}
							else if (bezt->f2 & SELECT) {
								calc_tw_center(scene, bezt->vec[1]);
								totsel++;
							}
							else {
								if (bezt->f1 & SELECT) {
									calc_tw_center(scene, bezt->vec[(v3d->around == V3D_AROUND_LOCAL_ORIGINS) ? 1 : 0]);
									totsel++;
								}
								if (bezt->f3 & SELECT) {
									calc_tw_center(scene, bezt->vec[(v3d->around == V3D_AROUND_LOCAL_ORIGINS) ? 1 : 2]);
									totsel++;
								}
							}
							bezt++;
						}
					}
					else {
						bp = nu->bp;
						a = nu->pntsu * nu->pntsv;
						while (a--) {
							if (bp->f1 & SELECT) {
								calc_tw_center(scene, bp->vec);
								totsel++;
							}
							bp++;
						}
					}
					nu = nu->next;
				}
			}
		}
		else if (obedit->type == OB_MBALL) {
			MetaBall *mb = (MetaBall *)obedit->data;
			MetaElem *ml;

			if ((v3d->around == V3D_AROUND_ACTIVE) && (ml = mb->lastelem)) {
				calc_tw_center(scene, &ml->x);
				totsel++;
			}
			else {
				for (ml = mb->editelems->first; ml; ml = ml->next) {
					if (ml->flag & SELECT) {
						calc_tw_center(scene, &ml->x);
						totsel++;
					}
				}
			}
		}
		else if (obedit->type == OB_LATTICE) {
			Lattice *lt = ((Lattice *)obedit->data)->editlatt->latt;
			BPoint *bp;

			if ((v3d->around == V3D_AROUND_ACTIVE) && (bp = BKE_lattice_active_point_get(lt))) {
				calc_tw_center(scene, bp->vec);
				totsel++;
			}
			else {
				bp = lt->def;
				a = lt->pntsu * lt->pntsv * lt->pntsw;
				while (a--) {
					if (bp->f1 & SELECT) {
						calc_tw_center(scene, bp->vec);
						totsel++;
					}
					bp++;
				}
			}
		}

		/* selection center */
		if (totsel) {
			mul_v3_fl(scene->twcent, 1.0f / (float)totsel);   // centroid!
			mul_m4_v3(obedit->obmat, scene->twcent);
			mul_m4_v3(obedit->obmat, scene->twmin);
			mul_m4_v3(obedit->obmat, scene->twmax);
		}
	}
	else if (ob && (ob->mode & OB_MODE_POSE)) {
		bPoseChannel *pchan;
		int mode = TFM_ROTATION; /* mislead counting bones... bah. We don't know the manipulator mode, could be mixed */
		bool ok = false;

		if ((ob->lay & v3d->lay) == 0)
			return 0;

		if ((v3d->around == V3D_AROUND_ACTIVE) && (pchan = BKE_pose_channel_active(ob))) {
			/* doesn't check selection or visibility intentionally */
			Bone *bone = pchan->bone;
			if (bone) {
				stats_pose(scene, ob, rv3d, pchan);
				totsel = 1;
				ok = true;
			}
		}
		else {
			totsel = count_set_pose_transflags(&mode, 0, ob);

			if (totsel) {
				/* use channels to get stats */
				for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
					Bone *bone = pchan->bone;
					if (bone && (bone->flag & BONE_TRANSFORM)) {
						stats_pose(scene, ob, rv3d, pchan);
					}
				}
				ok = true;
			}
		}

		if (ok) {
			mul_v3_fl(scene->twcent, 1.0f / (float)totsel);   // centroid!
			mul_m4_v3(ob->obmat, scene->twcent);
			mul_m4_v3(ob->obmat, scene->twmin);
			mul_m4_v3(ob->obmat, scene->twmax);
		}
	}
	else if (ob && (ob->mode & OB_MODE_ALL_PAINT)) {
		/* pass */
	}
	else if (ob && ob->mode & OB_MODE_PARTICLE_EDIT) {
		const PTCacheEdit *edit = PE_get_current(scene, ob);
		PTCacheEditPoint *point;
		PTCacheEditKey *ek;
		int k;

		if (edit) {
			point = edit->points;
			for (a = 0; a < edit->totpoint; a++, point++) {
				if (point->flag & PEP_HIDE) continue;

				for (k = 0, ek = point->keys; k < point->totkey; k++, ek++) {
					if (ek->flag & PEK_SELECT) {
						calc_tw_center(scene, ek->flag & PEK_USE_WCO ? ek->world_co : ek->co);
						totsel++;
					}
				}
			}

			/* selection center */
			if (totsel)
				mul_v3_fl(scene->twcent, 1.0f / (float)totsel);  // centroid!
		}
	}
	else {
		/* we need the one selected object, if its not active */
		ob = OBACT;
		if (ob && !(ob->flag & SELECT))
			ob = NULL;

		for (base = scene->base.first; base; base = base->next) {
			if (TESTBASELIB(v3d, base)) {
				if (ob == NULL)
					ob = base->object;

				calc_tw_center(scene, base->object->loc);
				protectflag_to_drawflags(base->object->protectflag, &rv3d->twdrawflag);
				totsel++;
			}
		}

		/* selection center */
		if (totsel) {
			mul_v3_fl(scene->twcent, 1.0f / (float)totsel);   // centroid!
		}
	}

	/* global, local or normal orientation? */
	if (ob && totsel && !is_gp_edit) {
		float mat[3][3];

		switch (v3d->twmode) {
			case V3D_MANIP_GLOBAL:
				break; /* nothing to do */
			case V3D_MANIP_GIMBAL:
				if (gimbal_axis(ob, mat)) {
					copy_m4_m3(rv3d->twmat, mat);
					break;
				}
				/* if not gimbal, fall through to normal */
				/* fall-through */
			case V3D_MANIP_NORMAL:
				if (obedit || ob->mode & OB_MODE_POSE) {
					ED_getTransformOrientationMatrix(C, mat, v3d->around);
					copy_m4_m3(rv3d->twmat, mat);
					break;
				}
				/* no break we define 'normal' as 'local' in Object mode */
				/* fall-through */
			case V3D_MANIP_LOCAL:
				if (ob->mode & OB_MODE_POSE) {
					/* each bone moves on its own local axis, but  to avoid confusion,
					 * use the active pones axis for display [#33575], this works as expected on a single bone
					 * and users who select many bones will understand whats going on and what local means
					 * when they start transforming */
					ED_getTransformOrientationMatrix(C, mat, v3d->around);
					copy_m4_m3(rv3d->twmat, mat);
					break;
				}
				copy_m4_m4(rv3d->twmat, ob->obmat);
				normalize_m4(rv3d->twmat);
				break;
			case V3D_MANIP_VIEW:
				copy_m3_m4(mat, rv3d->viewinv);
				normalize_m3(mat);
				copy_m4_m3(rv3d->twmat, mat);
				break;
			default: /* V3D_MANIP_CUSTOM */
				if (applyTransformOrientation(C, mat, NULL, v3d->twmode - V3D_MANIP_CUSTOM)) {
					copy_m4_m3(rv3d->twmat, mat);
				}
				break;
		}
	}

	return totsel;
}

/* don't draw axis perpendicular to the view */
static void manipulator_drawflags_refresh(RegionView3D *rv3d)
{
	float view_vec[3], axis_vec[3];
	float idot;
	int i;

	const int twdrawflag_axis[3] = {
	    (MAN_TRANS_X | MAN_SCALE_X),
	    (MAN_TRANS_Y | MAN_SCALE_Y),
	    (MAN_TRANS_Z | MAN_SCALE_Z)};


	ED_view3d_global_to_vector(rv3d, rv3d->twmat[3], view_vec);

	for (i = 0; i < 3; i++) {
		normalize_v3_v3(axis_vec, rv3d->twmat[i]);
		rv3d->tw_idot[i] = idot = 1.0f - fabsf(dot_v3v3(view_vec, axis_vec));
		if (idot < TW_AXIS_DOT_MIN) {
			rv3d->twdrawflag &= ~twdrawflag_axis[i];
		}
	}
}

static void manipulator_prepare_mat(const bContext *C, View3D *v3d, RegionView3D *rv3d)
{
	Scene *scene = CTX_data_scene(C);

	switch (v3d->around) {
		case V3D_AROUND_CENTER_BOUNDS:
		case V3D_AROUND_ACTIVE:
		{
				bGPdata *gpd = CTX_data_gpencil_data(C);
				Object *ob = OBACT;

				if (((v3d->around == V3D_AROUND_ACTIVE) && (scene->obedit == NULL)) &&
				    ((gpd == NULL) || !(gpd->flag & GP_DATA_STROKE_EDITMODE)) &&
				    (!(ob->mode & OB_MODE_POSE)))
				{
					copy_v3_v3(rv3d->twmat[3], ob->obmat[3]);
				}
				else {
					mid_v3_v3v3(rv3d->twmat[3], scene->twmin, scene->twmax);
				}
				break;
		}
		case V3D_AROUND_LOCAL_ORIGINS:
		case V3D_AROUND_CENTER_MEAN:
			copy_v3_v3(rv3d->twmat[3], scene->twcent);
			break;
		case V3D_AROUND_CURSOR:
			copy_v3_v3(rv3d->twmat[3], ED_view3d_cursor3d_get(scene, v3d));
			break;
	}

	mul_mat3_m4_fl(rv3d->twmat, ED_view3d_pixel_size(rv3d, rv3d->twmat[3]) * U.tw_size);
}

/**
 * Sets up \a r_start and \a r_len to define arrow line range.
 * Needed to adjust line drawing for combined manipulator axis types.
 */
static void manipulator_line_range(const View3D *v3d, const short axis_type, float *r_start, float *r_len)
{
	const float ofs = 0.2f;

	*r_start = 0.2f;
	*r_len = 1.0f;

	switch (axis_type) {
		case MAN_AXES_TRANSLATE:
			if (v3d->twtype & V3D_MANIP_SCALE) {
				*r_start = *r_len - ofs + 0.075f;
			}
			if (v3d->twtype & V3D_MANIP_ROTATE) {
				*r_len += ofs;
			}
			break;
		case MAN_AXES_SCALE:
			if (v3d->twtype & (V3D_MANIP_TRANSLATE | V3D_MANIP_ROTATE)) {
				*r_len -= ofs + 0.025f;
			}
			break;
	}

	*r_len -= *r_start;
}


/* **************** Actual Widget Stuff **************** */

static ManipulatorGroup *manipulatorgroup_init(
        wmWidgetGroup *wgroup, const bool init_trans, const bool init_rot, const bool init_scale)
{
	ManipulatorGroup *man;

	if (!(init_trans || init_rot || init_scale))
		return NULL;

	man = MEM_callocN(sizeof(ManipulatorGroup), "manipulator_data");

	/* add/init widgets - order matters! */
	if (init_scale) {
		man->scale_c = WIDGET_dial_new(wgroup, "scale_c", WIDGET_DIAL_STYLE_RING);
		man->scale_x = WIDGET_arrow_new(wgroup, "scale_x", WIDGET_ARROW_STYLE_BOX);
		man->scale_y = WIDGET_arrow_new(wgroup, "scale_y", WIDGET_ARROW_STYLE_BOX);
		man->scale_z = WIDGET_arrow_new(wgroup, "scale_z", WIDGET_ARROW_STYLE_BOX);
		man->scale_xy = WIDGET_plane_new(wgroup, "scale_xy", 0);
		man->scale_yz = WIDGET_plane_new(wgroup, "scale_yz", 0);
		man->scale_zx = WIDGET_plane_new(wgroup, "scale_zx", 0);
	}
	if (init_rot) {
		man->rotate_x = WIDGET_dial_new(wgroup, "rotate_x", WIDGET_DIAL_STYLE_RING_CLIPPED);
		man->rotate_y = WIDGET_dial_new(wgroup, "rotate_y", WIDGET_DIAL_STYLE_RING_CLIPPED);
		man->rotate_z = WIDGET_dial_new(wgroup, "rotate_z", WIDGET_DIAL_STYLE_RING_CLIPPED);
		/* init screen aligned widget last here, looks better, behaves better */
		man->rotate_c = WIDGET_dial_new(wgroup, "rotate_c", WIDGET_DIAL_STYLE_RING);
	}
	if (init_trans) {
		man->translate_c = WIDGET_dial_new(wgroup, "translate_c", WIDGET_DIAL_STYLE_RING);
		man->translate_x = WIDGET_arrow_new(wgroup, "translate_x", WIDGET_ARROW_STYLE_NORMAL);
		man->translate_y = WIDGET_arrow_new(wgroup, "translate_y", WIDGET_ARROW_STYLE_NORMAL);
		man->translate_z = WIDGET_arrow_new(wgroup, "translate_z", WIDGET_ARROW_STYLE_NORMAL);
		man->translate_xy = WIDGET_plane_new(wgroup, "translate_xy", 0);
		man->translate_yz = WIDGET_plane_new(wgroup, "translate_yz", 0);
		man->translate_zx = WIDGET_plane_new(wgroup, "translate_zx", 0);
	}

	return man;
}

/**
 * Custom handler for manipulator widgets
 */
static int manipulator_handler(bContext *C, const wmEvent *UNUSED(event), wmWidget *widget, const int UNUSED(flag))
{
	const ScrArea *sa = CTX_wm_area(C);
	ARegion *ar = CTX_wm_region(C);
	View3D *v3d = sa->spacedata.first;
	RegionView3D *rv3d = ar->regiondata;

	if (calc_manipulator_stats(C)) {
		manipulator_prepare_mat(C, v3d, rv3d);
		WM_widget_set_origin(widget, rv3d->twmat[3]);
	}

	ED_region_tag_redraw(ar);

	return OPERATOR_PASS_THROUGH;
}

void WIDGETGROUP_manipulator_create(const bContext *C, wmWidgetGroup *wgroup)
{
	const ScrArea *sa = CTX_wm_area(C);
	const ARegion *ar = CTX_wm_region(C);
	View3D *v3d = sa->spacedata.first;
	RegionView3D *rv3d = ar->regiondata;

	const bool any_visible   = (calc_manipulator_stats(C) != 0);
	const bool trans_visble  = (any_visible && (v3d->twtype & V3D_MANIP_TRANSLATE));
	const bool rot_visble    = (any_visible && (v3d->twtype & V3D_MANIP_ROTATE));
	const bool scale_visible = (any_visible && (v3d->twtype & V3D_MANIP_SCALE));
	ManipulatorGroup *man = manipulatorgroup_init(wgroup, trans_visble, rot_visble, scale_visible);

	if (!man)
		return;


	manipulator_prepare_mat(C, v3d, rv3d);
	manipulator_drawflags_refresh(rv3d);

	/* when looking through a selected camera, the manipulator can be at the
	 * exact same position as the view, skip so we don't break selection */
	if (fabsf(mat4_to_scale(rv3d->twmat)) < 1e-7f) {
		MAN_ITER_AXES_BEGIN
		{
			WM_widget_set_flag(axis, WM_WIDGET_HIDDEN, true);
		}
		MAN_ITER_AXES_END;

		MEM_freeN(man);
		return;
	}


	/* *** set properties for axes *** */

	MAN_ITER_AXES_BEGIN
	{
		const short axis_type = manipulator_get_axis_type(man, axis);
		const int aidx_norm = manipulator_index_normalize(axis_idx);
		int constraint_axis[3] = {1, 0, 0};

		PointerRNA *ptr;
		float col[4], col_hi[4];

		if (manipulator_is_axis_visible(v3d, rv3d, axis_idx) == false) {
			WM_widget_set_flag(axis, WM_WIDGET_HIDDEN, true);
			continue;
		}

		manipulator_get_axis_color(rv3d, axis_idx, col, col_hi);
		manipulator_get_axis_constraint(axis_idx, constraint_axis);

		WM_widget_set_origin(axis, rv3d->twmat[3]);
		WM_widget_set_colors(axis, col, col_hi);
		/* custom handler! */
		axis->handler = manipulator_handler;

		switch(axis_idx) {
			case MAN_AXIS_TRANS_X:
			case MAN_AXIS_TRANS_Y:
			case MAN_AXIS_TRANS_Z:
			case MAN_AXIS_SCALE_X:
			case MAN_AXIS_SCALE_Y:
			case MAN_AXIS_SCALE_Z:
			{
				float start_co[3] = {0.0f, 0.0f, 0.0f};
				float len;

				manipulator_line_range(v3d, axis_type, &start_co[2], &len);

				WIDGET_arrow_set_direction(axis, rv3d->twmat[aidx_norm]);
				WIDGET_arrow_set_line_len(axis, len);
				WM_widget_set_offset(axis, start_co);
				WM_widget_set_line_width(axis, MAN_AXIS_LINE_WIDTH);
				break;
			}
			case MAN_AXIS_ROT_X:
			case MAN_AXIS_ROT_Y:
			case MAN_AXIS_ROT_Z:
				WIDGET_dial_set_up_vector(axis, rv3d->twmat[aidx_norm]);
				WM_widget_set_line_width(axis, MAN_AXIS_LINE_WIDTH);
				break;
			case MAN_AXIS_TRANS_XY:
			case MAN_AXIS_TRANS_YZ:
			case MAN_AXIS_TRANS_ZX:
			case MAN_AXIS_SCALE_XY:
			case MAN_AXIS_SCALE_YZ:
			case MAN_AXIS_SCALE_ZX:
			{
				float ofs_ax = 11.0f;
				float ofs[3];

				ofs[0] = ofs_ax;
				ofs[1] = ofs_ax;
				ofs[2] = 0.0f;

				WIDGET_plane_set_direction(axis, rv3d->twmat[aidx_norm - 1 < 0 ? 2 : aidx_norm - 1]);
				WIDGET_plane_set_up_vector(axis, rv3d->twmat[aidx_norm + 1 > 2 ? 0 : aidx_norm + 1]);
				WM_widget_set_scale(axis, 0.07f);
				WM_widget_set_origin(axis, rv3d->twmat[3]);
				WM_widget_set_offset(axis, ofs);
				break;
			}
			case MAN_AXIS_TRANS_C:
			case MAN_AXIS_ROT_C:
			case MAN_AXIS_SCALE_C:
				WIDGET_dial_set_up_vector(axis, rv3d->viewinv[2]);
				if (axis_idx != MAN_AXIS_ROT_C) {
					WM_widget_set_scale(axis, 0.2f);
				}
				break;
		}

		switch (axis_type) {
			case MAN_AXES_TRANSLATE:
				ptr = WM_widget_set_operator(axis, "TRANSFORM_OT_translate");
				break;
			case MAN_AXES_ROTATE:
				ptr = WM_widget_set_operator(axis, "TRANSFORM_OT_rotate");
				break;
			case MAN_AXES_SCALE:
				ptr = WM_widget_set_operator(axis, "TRANSFORM_OT_resize");
				break;
		}
		RNA_boolean_set_array(ptr, "constraint_axis", constraint_axis);
		RNA_boolean_set(ptr, "release_confirm", 1);
	}
	MAN_ITER_AXES_END;

	MEM_freeN(man);
}

int WIDGETGROUP_manipulator_poll(const bContext *C, struct wmWidgetGroupType *UNUSED(wgrouptype))
{
	/* it's a given we only use this in 3D view */
	const ScrArea *sa = CTX_wm_area(C);
	const View3D *v3d = sa->spacedata.first;

	return ((v3d->twflag & V3D_USE_MANIPULATOR) != 0);
}

void WIDGETGROUP_object_manipulator_create(const bContext *C, wmWidgetGroup *wgroup)
{
	Object *ob = ED_object_active_context((bContext *)C);

	if (ob->wgroup == NULL) {
		ob->wgroup = wgroup;
	}

	WIDGETGROUP_manipulator_create(C, wgroup);
}

int WIDGETGROUP_object_manipulator_poll(const bContext *C, struct wmWidgetGroupType *wgrouptype)
{
	Object *ob = ED_object_active_context((bContext *)C);

	if (ED_operator_object_active((bContext *)C)) {
		char idname[MAX_NAME];

		WM_widgetgrouptype_idname_get(wgrouptype, idname);
		if (STREQ(idname, ob->id.name)) {
			return true;
		}
	}
	return false;
}


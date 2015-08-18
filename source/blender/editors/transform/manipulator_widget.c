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
#include "DNA_lattice_types.h"
#include "DNA_meta_types.h"
#include "DNA_screen_types.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "BLI_listbase.h"

#include "RNA_access.h"

#include "BKE_action.h"
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
#define MAN_ROT_V    (1 << 6)
#define MAN_ROT_T    (1 << 7)
#define MAN_ROT_C    (MAN_ROT_X | MAN_ROT_Y | MAN_ROT_Z | MAN_ROT_V | MAN_ROT_T)

#define MAN_SCALE_X  (1 << 8)
#define MAN_SCALE_Y  (1 << 9)
#define MAN_SCALE_Z  (1 << 10)
#define MAN_SCALE_C  (MAN_SCALE_X | MAN_SCALE_Y | MAN_SCALE_Z)

/* return codes for select */
enum {
	MAN_SEL_TRANS_X = 0,
	MAN_SEL_TRANS_Y,
	MAN_SEL_TRANS_Z,

	MAN_SEL_ROT_X,
	MAN_SEL_ROT_Y,
	MAN_SEL_ROT_Z,
	MAN_SEL_ROT_V,
	MAN_SEL_ROT_T,

	MAN_SEL_SCALE_X,
	MAN_SEL_SCALE_Y,
	MAN_SEL_SCALE_Z,

	/* those two stay at the end so the rest can be inferred with bitshifting */
	MAN_SEL_SCALE_C,
	MAN_SEL_TRANS_C,

	MAN_SEL_MAX
};

/* axes as index - XXX combine with return codes for select */
enum {
	MAN_AXIS_TRANS_X = 0,
	MAN_AXIS_TRANS_Y,
	MAN_AXIS_TRANS_Z,

	MAN_AXIS_ROT_X,
	MAN_AXIS_ROT_Y,
	MAN_AXIS_ROT_Z,
};

/* axis types */
enum {
	MAN_AXES_ALL = 0,
	MAN_AXES_TRANSLATE,
	MAN_AXES_ROTATE,
};

/* color codes */

#define MAN_RGB     0
#define MAN_GHOST   1
#define MAN_MOVECOL 2

/* threshold for testing view aligned manipulator axis */
#define TW_AXIS_DOT_MIN 0.02f
#define TW_AXIS_DOT_MAX 0.1f

/* loop over axes of given type */
#define MAN_ITER_AXES_BEGIN(axis_type) \
	{ \
		wmWidget *axis; \
		int i; \
		for (i = (axis_type == MAN_AXES_ROTATE ? MAN_AXIS_ROT_X : MAN_AXIS_TRANS_X); \
		     i < (axis_type == MAN_AXES_TRANSLATE ? MAN_AXIS_TRANS_Z + 1 : MAN_AXIS_ROT_Z + 1); \
		     i++) \
		{ \
			axis = manipulator_get_axis_from_index(man, i); \
			if (!axis) continue;

#define MAN_ITER_AXES_END \
		} \
	} ((void)0)

static wmWidget *manipulator_get_axis_from_index(const ManipulatorGroup *man, const short index)
{
	wmWidget *axis = NULL;

	BLI_assert(IN_RANGE_INCL(index, 0.0f, 5.0f));

	switch (index) {
		case MAN_AXIS_TRANS_X:
			axis = man->translate_x;
			break;
		case MAN_AXIS_TRANS_Y:
			axis = man->translate_y;
			break;
		case MAN_AXIS_TRANS_Z:
			axis = man->translate_z;
			break;
		case MAN_AXIS_ROT_X:
			axis = man->rotate_x;
			break;
		case MAN_AXIS_ROT_Y:
			axis = man->rotate_y;
			break;
		case MAN_AXIS_ROT_Z:
			axis = man->rotate_z;
			break;
	}

	return axis;
}

static short manipulator_get_axis_type(const ManipulatorGroup *man, const wmWidget *axis)
{
	if (ELEM(axis, man->translate_x, man->translate_y, man->translate_z)) {
		return MAN_AXES_TRANSLATE;
	}
	else {
		return MAN_AXES_ROTATE;
	}
}

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
static void stats_pose(Scene *scene, RegionView3D *rv3d, bPoseChannel *pchan)
{
	Bone *bone = pchan->bone;

	if (bone) {
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
	axis_angle_to_quat(quat, axis, M_PI / 2.0);
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
	if (ob) {
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
	}

	return 0;
}


/* centroid, boundbox, of selection */
/* returns total items selected */
static int calc_manipulator_stats(const bContext *C)
{
	const ScrArea *sa = CTX_wm_area(C);
	const ARegion *ar = CTX_wm_region(C);
	const ToolSettings *ts = CTX_data_tool_settings(C);
	const View3D *v3d = sa->spacedata.first;
	RegionView3D *rv3d = ar->regiondata;
	Scene *scene = CTX_data_scene(C);
	Object *obedit = CTX_data_edit_object(C);
	Object *ob = OBACT;
	Base *base;
	int a, totsel = 0;

	/* transform widget matrix */
	unit_m4(rv3d->twmat);

	rv3d->twdrawflag = 0xFFFF;

	/* transform widget centroid/center */
	INIT_MINMAX(scene->twmin, scene->twmax);
	zero_v3(scene->twcent);

	if (obedit) {
		ob = obedit;
		if ((ob->lay & v3d->lay) == 0) return 0;

		if (obedit->type == OB_MESH) {
			BMEditMesh *em = BKE_editmesh_from_object(obedit);
			BMEditSelection ese;
			float vec[3] = {0, 0, 0};

			/* USE LAST SELECTE WITH ACTIVE */
			if ((v3d->around == V3D_ACTIVE) && BM_select_history_active_get(em->bm, &ese)) {
				BM_editselection_center(&ese, vec);
				calc_tw_center(scene, vec);
				totsel = 1;
			}
			else {
				BMesh *bm = em->bm;
				BMVert *eve;

				BMIter iter;

				/* do vertices/edges/faces for center depending on selection
				 * mode. note we can't use just vertex selection flag because
				 * it is not flush down on changes */
				if (ts->selectmode & SCE_SELECT_VERTEX) {
					BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
						if (!BM_elem_flag_test(eve, BM_ELEM_HIDDEN)) {
							if (BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
								totsel++;
								calc_tw_center(scene, eve->co);
							}
						}
					}
				}
				else if (ts->selectmode & SCE_SELECT_EDGE) {
					BMIter itersub;
					BMEdge *eed;
					BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
						if (!BM_elem_flag_test(eve, BM_ELEM_HIDDEN)) {
							/* check the vertex has a selected edge, only add it once */
							BM_ITER_ELEM (eed, &itersub, eve, BM_EDGES_OF_VERT) {
								if (BM_elem_flag_test(eed, BM_ELEM_SELECT)) {
									totsel++;
									calc_tw_center(scene, eve->co);
									break;
								}
							}
						}
					}
				}
				else {
					BMIter itersub;
					BMFace *efa;
					BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
						if (!BM_elem_flag_test(eve, BM_ELEM_HIDDEN)) {
							/* check the vertex has a selected face, only add it once */
							BM_ITER_ELEM (efa, &itersub, eve, BM_FACES_OF_VERT) {
								if (BM_elem_flag_test(efa, BM_ELEM_SELECT)) {
									totsel++;
									calc_tw_center(scene, eve->co);
									break;
								}
							}
						}
					}
				}
			}
		} /* end editmesh */
		else if (obedit->type == OB_ARMATURE) {
			const bArmature *arm = obedit->data;
			EditBone *ebo;

			if ((v3d->around == V3D_ACTIVE) && (ebo = arm->act_edbone)) {
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

			if (v3d->around == V3D_ACTIVE && ED_curve_active_center(cu, center)) {
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
									calc_tw_center(scene, bezt->vec[(v3d->around == V3D_LOCAL) ? 1 : 0]);
									totsel++;
								}
								if (bezt->f3 & SELECT) {
									calc_tw_center(scene, bezt->vec[(v3d->around == V3D_LOCAL) ? 1 : 2]);
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

			if ((v3d->around == V3D_ACTIVE) && (ml = mb->lastelem)) {
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

			if ((v3d->around == V3D_ACTIVE) && (bp = BKE_lattice_active_point_get(lt))) {
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

		if ((ob->lay & v3d->lay) == 0) return 0;

		if ((v3d->around == V3D_ACTIVE) && (pchan = BKE_pose_channel_active(ob))) {
			/* doesn't check selection or visibility intentionally */
			Bone *bone = pchan->bone;
			if (bone) {
				stats_pose(scene, rv3d, pchan);
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
						stats_pose(scene, rv3d, pchan);
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
		if (ob && !(ob->flag & SELECT)) ob = NULL;

		for (base = scene->base.first; base; base = base->next) {
			if (TESTBASELIB(v3d, base)) {
				if (ob == NULL)
					ob = base->object;
				calc_tw_center(scene, base->object->obmat[3]);
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
	if (ob && totsel) {
		switch (v3d->twmode) {
			case V3D_MANIP_GLOBAL:
			{
				break; /* nothing to do */
			}
			case V3D_MANIP_GIMBAL:
			{
				float mat[3][3];
				if (gimbal_axis(ob, mat)) {
					copy_m4_m3(rv3d->twmat, mat);
					break;
				}
				/* if not gimbal, fall through to normal */
				/* fall-through */
			}
			case V3D_MANIP_NORMAL:
			{
				if (obedit || ob->mode & OB_MODE_POSE) {
					float mat[3][3];
					ED_getTransformOrientationMatrix(C, mat, (v3d->around == V3D_ACTIVE));
					copy_m4_m3(rv3d->twmat, mat);
					break;
				}
				/* no break we define 'normal' as 'local' in Object mode */
				/* fall-through */
			}
			case V3D_MANIP_LOCAL:
			{
				if (ob->mode & OB_MODE_POSE) {
					/* each bone moves on its own local axis, but  to avoid confusion,
					 * use the active pones axis for display [#33575], this works as expected on a single bone
					 * and users who select many bones will understand whats going on and what local means
					 * when they start transforming */
					float mat[3][3];
					ED_getTransformOrientationMatrix(C, mat, (v3d->around == V3D_ACTIVE));
					copy_m4_m3(rv3d->twmat, mat);
					break;
				}
				copy_m4_m4(rv3d->twmat, ob->obmat);
				normalize_m4(rv3d->twmat);
				break;
			}
			case V3D_MANIP_VIEW:
			{
				float mat[3][3];
				copy_m3_m4(mat, rv3d->viewinv);
				normalize_m3(mat);
				copy_m4_m3(rv3d->twmat, mat);
				break;
			}
			default: /* V3D_MANIP_CUSTOM */
			{
				float mat[3][3];
				if (applyTransformOrientation(C, mat, NULL)) {
					copy_m4_m3(rv3d->twmat, mat);
				}
				break;
			}
		}
	}

	return totsel;
}

/* don't draw axis perpendicular to the view */
static void test_manipulator_axis(const bContext *C)
{
	RegionView3D *rv3d = CTX_wm_region_view3d(C);
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


/* ********************************************* */

/* main call, does calc centers & orientation too */
static int drawflags = 0xFFFF;       /* only for the calls below, belongs in scene...? */


int WIDGETGROUP_manipulator_poll(const struct bContext *C, struct wmWidgetGroupType *UNUSED(wgrouptype))
{
	/* it's a given we only use this in 3D view */
	ScrArea *sa = CTX_wm_area(C);
	View3D *v3d = sa->spacedata.first;

	return ((v3d->twflag & V3D_USE_MANIPULATOR) != 0);
}

static void manipulator_prepare_mat(Scene *scene, View3D *v3d, RegionView3D *rv3d)
{
	switch (v3d->around) {
		case V3D_CENTER:
		case V3D_ACTIVE:
		{
			Object *ob;
			if (((v3d->around == V3D_ACTIVE) && (scene->obedit == NULL)) &&
					((ob = OBACT) && !(ob->mode & OB_MODE_POSE)))
			{
				copy_v3_v3(rv3d->twmat[3], ob->obmat[3]);
			}
			else {
				mid_v3_v3v3(rv3d->twmat[3], scene->twmin, scene->twmax);
			}
			break;
		}
		case V3D_LOCAL:
		case V3D_CENTROID:
			copy_v3_v3(rv3d->twmat[3], scene->twcent);
			break;
		case V3D_CURSOR:
			copy_v3_v3(rv3d->twmat[3], ED_view3d_cursor3d_get(scene, v3d));
			break;
	}

	mul_mat3_m4_fl(rv3d->twmat, ED_view3d_pixel_size(rv3d, rv3d->twmat[3]) * U.tw_size);
}

static ManipulatorGroup *manipulatorgroup_init(
        struct wmWidgetGroup *wgroup, const bool init_trans, const bool init_rot)
{
	ManipulatorGroup *man;

	const float color_green[4] = {0.27f, 1.0f, 0.27f, 1.0f};
	const float color_red[4] = {1.0f, 0.27f, 0.27f, 1.0f};
	const float color_blue[4] = {0.27f, 0.27f, 1.0f, 1.0f};

	if (init_trans == false && init_rot == false)
		return NULL;

	man = MEM_callocN(sizeof(ManipulatorGroup), "manipulator_data");

	if (init_trans) {
		man->translate_x = WIDGET_arrow_new(wgroup, "translate_x", WIDGET_ARROW_STYLE_NORMAL);
		man->translate_y = WIDGET_arrow_new(wgroup, "translate_y", WIDGET_ARROW_STYLE_NORMAL);
		man->translate_z = WIDGET_arrow_new(wgroup, "translate_z", WIDGET_ARROW_STYLE_NORMAL);
		WIDGET_arrow_set_color(man->translate_x, color_red);
		WIDGET_arrow_set_color(man->translate_y, color_green);
		WIDGET_arrow_set_color(man->translate_z, color_blue);
	}
	if (init_rot) {
		man->rotate_x = WIDGET_dial_new(wgroup, "rotate_x", WIDGET_DIAL_STYLE_RING_CLIPPED);
		man->rotate_y = WIDGET_dial_new(wgroup, "rotate_y", WIDGET_DIAL_STYLE_RING_CLIPPED);
		man->rotate_z = WIDGET_dial_new(wgroup, "rotate_z", WIDGET_DIAL_STYLE_RING_CLIPPED);
		WIDGET_dial_set_color(man->rotate_x, color_red);
		WIDGET_dial_set_color(man->rotate_y, color_green);
		WIDGET_dial_set_color(man->rotate_z, color_blue);
	}

	return man;
}

void WIDGETGROUP_manipulator_draw(const struct bContext *C, struct wmWidgetGroup *wgroup)
{
	ScrArea *sa = CTX_wm_area(C);
	ARegion *ar = CTX_wm_region(C);
	View3D *v3d = sa->spacedata.first;
	RegionView3D *rv3d = ar->regiondata;

	const bool any_visible = (calc_manipulator_stats(C) != 0);
	const bool trans_visble = (any_visible && (v3d->twtype & V3D_MANIP_TRANSLATE));
	const bool rot_visble = (any_visible && (v3d->twtype & V3D_MANIP_ROTATE));
	const ManipulatorGroup *man = manipulatorgroup_init(wgroup, trans_visble, rot_visble);

	if (!man)
		return;


	/* now we can define center */
	manipulator_prepare_mat(CTX_data_scene(C), v3d, rv3d);

	/* when looking through a selected camera, the manipulator can be at the
	 * exact same position as the view, skip so we don't break selection */
	if (fabsf(mat4_to_scale(rv3d->twmat)) < 1e-7f) {
		MAN_ITER_AXES_BEGIN(MAN_AXES_ALL)
		{
			WM_widget_flag_set(axis, WM_WIDGET_HIDDEN, true);
		}
		MAN_ITER_AXES_END;

		return;
	}

	test_manipulator_axis(C);
	drawflags = rv3d->twdrawflag;    /* set in calc_manipulator_stats */

	MAN_ITER_AXES_BEGIN(MAN_AXES_ALL)
	{
		const short atype = manipulator_get_axis_type(man, axis);
		const bool is_trans = (atype == MAN_AXES_TRANSLATE);

		/* should be added according to the order of axis */
		WM_widget_set_origin(axis, rv3d->twmat[3]);
		if (is_trans) {
			WIDGET_arrow_set_direction(axis, rv3d->twmat[i]);
		}
		else {
			WIDGET_dial_set_direction(axis, rv3d->twmat[i - 3]);
		}

		WM_widget_operator(axis, is_trans ? "TRANSFORM_OT_translate" : "TRANSFORM_OT_rotate");
	}
	MAN_ITER_AXES_END;
}

void WIDGETGROUP_object_manipulator_draw(const struct bContext *C, struct wmWidgetGroup *wgroup)
{
	Object *ob = ED_object_active_context((bContext *)C);

	if (ob->wgroup == NULL) {
		ob->wgroup = wgroup;
	}

	WIDGETGROUP_manipulator_draw(C, wgroup);
}

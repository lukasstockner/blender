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
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_view3d/view3d_widgets.c
 *  \ingroup spview3d
 */


#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_camera.h"
#include "BKE_context.h"
#include "BKE_object.h"

#include "DNA_armature_types.h"
#include "DNA_camera_types.h"
#include "DNA_object_types.h"
#include "DNA_object_force.h"
#include "DNA_lamp_types.h"

#include "ED_armature.h"
#include "ED_screen.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "view3d_intern.h"  /* own include */


int WIDGETGROUP_lamp_poll(const struct bContext *C, struct wmWidgetGroupType *UNUSED(wgrouptype))
{
	Object *ob = CTX_data_active_object(C);

	if (ob && ob->type == OB_LAMP) {
		Lamp *la = ob->data;
		return (la->type == LA_SPOT);
	}
	return false;
}

void WIDGETGROUP_lamp_create(const struct bContext *C, struct wmWidgetGroup *wgroup)
{
	Object *ob = CTX_data_active_object(C);
	Lamp *la = ob->data;
	wmWidget *widget;
	PointerRNA ptr;
	float dir[3];
	const char *propname = "spot_size";

	const float color[4] = {0.5f, 0.5f, 1.0f, 1.0f};
	const float color_hi[4] = {0.8f, 0.8f, 0.45f, 1.0f};


	negate_v3_v3(dir, ob->obmat[2]);

	widget = WIDGET_arrow_new(wgroup, propname, WIDGET_ARROW_STYLE_INVERTED);

	RNA_pointer_create(&la->id, &RNA_Lamp, la, &ptr);
	WIDGET_arrow_set_range_fac(widget, 4.0f);
	WIDGET_arrow_set_direction(widget, dir);
	WM_widget_set_origin(widget, ob->obmat[3]);
	WM_widget_set_colors(widget, color, color_hi);
	WM_widget_set_property(widget, ARROW_SLOT_OFFSET_WORLD_SPACE, &ptr, propname);
}

int WIDGETGROUP_camera_poll(const bContext *C, wmWidgetGroupType *UNUSED(wgrouptype))
{
	Object *ob = CTX_data_active_object(C);

	return (ob && ob->type == OB_CAMERA);
}

void WIDGETGROUP_camera_create(const bContext *C, wmWidgetGroup *wgroup)
{
	Object *ob = CTX_data_active_object(C);
	Camera *ca = ob->data;
	wmWidget *widget;
	PointerRNA cameraptr;
	float dir[3];
	const bool focallen_widget = true; /* TODO make optional */

	RNA_pointer_create(&ca->id, &RNA_Camera, ca, &cameraptr);
	negate_v3_v3(dir, ob->obmat[2]);

	/* dof distance */
	if (ca->flag & CAM_SHOWLIMITS) {
		const float color[4] = {1.0f, 0.3f, 0.0f, 1.0f};
		const float color_hi[4] = {1.0f, 0.3f, 0.0f, 1.0f};
		const char *propname = "dof_distance";

		widget = WIDGET_arrow_new(wgroup, propname, WIDGET_ARROW_STYLE_CROSS);
		WIDGET_arrow_set_direction(widget, dir);
		WIDGET_arrow_set_up_vector(widget, ob->obmat[1]);
		WM_widget_set_flag(widget, WM_WIDGET_DRAW_HOVER, true);
		WM_widget_set_flag(widget, WM_WIDGET_SCALE_3D, false);
		WM_widget_set_colors(widget, color, color_hi);
		WM_widget_set_origin(widget, ob->obmat[3]);
		WM_widget_set_property(widget, ARROW_SLOT_OFFSET_WORLD_SPACE, &cameraptr, propname);
		WM_widget_set_scale(widget, ca->drawsize);
	}

	/* focal length
	 * - logic/calculations are similar to BKE_camera_view_frame_ex, better keep in sync */
	if (focallen_widget) {
		const bool is_ortho = (ca->type == CAM_ORTHO);
		const char *propname = is_ortho ? "ortho_scale" : "lens";

		float offset[3], asp[2];
		float min, max, range;
		float step, precision;


		/* get aspect */
		const Scene *scene = CTX_data_scene(C);
		const float aspx = (float)scene->r.xsch * scene->r.xasp;
		const float aspy = (float)scene->r.ysch * scene->r.yasp;
		const int sensor_fit = BKE_camera_sensor_fit(ca->sensor_fit, aspx, aspy);
		asp[0] = (sensor_fit == CAMERA_SENSOR_FIT_HOR) ? 1.0 : aspx / aspy;
		asp[1] = (sensor_fit == CAMERA_SENSOR_FIT_HOR) ? aspy / aspx : 1.0f;

		/* account for lens shifting */
		offset[0] = ((ob->size[0] > 0.0f) ? -2.0f : 2.0f) * ca->shiftx;
		offset[1] = 2.0f * ca->shifty;
		offset[2] = 0.0f;

		/* get property range */
		PropertyRNA *prop = RNA_struct_find_property(&cameraptr, propname);
		RNA_property_float_ui_range(&cameraptr, prop, &min, &max, &step, &precision);
		range = max - min;


		/* *** actual widget stuff *** */

		const float scale[3] = {1.0f / len_v3(ob->obmat[0]), 1.0f / len_v3(ob->obmat[1]), 1.0f / len_v3(ob->obmat[2])};
		const float scale_fac = ca->drawsize;
		const float drawsize = is_ortho ? (0.5f * ca->ortho_scale) :
		                                  (scale_fac / ((scale[0] + scale[1] + scale[2]) / 3.0f));
		const float half_sensor = 0.5f * ((ca->sensor_fit == CAMERA_SENSOR_FIT_VERT) ? ca->sensor_y : ca->sensor_x);
		const float color[4] = {1.0f, 1.0, 0.27f, 0.5f};
		const float color_hi[4] = {1.0f, 1.0, 0.27f, 1.0f};

		widget = WIDGET_arrow_new(wgroup, propname, (WIDGET_ARROW_STYLE_CONE | WIDGET_ARROW_STYLE_CONSTRAINED));

		WIDGET_arrow_set_range_fac(widget, is_ortho ? (scale_fac * range) : (drawsize * range / half_sensor));
		WIDGET_arrow_set_direction(widget, dir);
		WIDGET_arrow_set_up_vector(widget, ob->obmat[1]);
		WIDGET_arrow_cone_set_aspect(widget, asp);
		WM_widget_set_property(widget, ARROW_SLOT_OFFSET_WORLD_SPACE, &cameraptr, propname);
		WM_widget_set_origin(widget, ob->obmat[3]);
		WM_widget_set_offset(widget, offset);
		WM_widget_set_scale(widget, drawsize);
		WM_widget_set_flag(widget, WM_WIDGET_SCALE_3D, false);
		WM_widget_set_colors(widget, color, color_hi);

	}
}

int WIDGETGROUP_forcefield_poll(const bContext *C, wmWidgetGroupType *UNUSED(wgrouptype))
{
	Object *ob = CTX_data_active_object(C);

	return (ob && ob->pd && ob->pd->forcefield);
}

void WIDGETGROUP_forcefield_create(const bContext *C, wmWidgetGroup *wgroup)
{
	Object *ob = CTX_data_active_object(C);
	PartDeflect *pd = ob->pd;
	PointerRNA ptr;
	wmWidget *widget;

	const float size = (ob->type == OB_EMPTY) ? ob->empty_drawsize : 1.0f;
	const float ofs[3] = {0.0f, -size, 0.0f};

	const float col[4] = {0.8f, 0.8f, 0.45f, 0.5f};
	const float col_hi[4] = {0.8f, 0.8f, 0.45f, 1.0f};


	/* only wind effector for now */
	if (pd->forcefield == PFIELD_WIND) {
		widget = WIDGET_arrow_new(wgroup, "field_strength", WIDGET_ARROW_STYLE_CONSTRAINED);

		RNA_pointer_create(&ob->id, &RNA_FieldSettings, pd, &ptr);
		WIDGET_arrow_set_direction(widget, ob->obmat[2]);
		WIDGET_arrow_set_ui_range(widget, -200.0f, 200.0f);
		WIDGET_arrow_set_range_fac(widget, 6.0f);
		WM_widget_set_colors(widget, col, col_hi);
		WM_widget_set_origin(widget, ob->obmat[3]);
		WM_widget_set_offset(widget, ofs);
		WM_widget_set_flag(widget, WM_WIDGET_SCALE_3D, false);
		WM_widget_set_property(widget, ARROW_SLOT_OFFSET_WORLD_SPACE, &ptr, "strength");
	}
}

/* draw facemaps depending on the selected bone in pose mode */
#define USE_FACEMAP_FROM_BONE

int WIDGETGROUP_armature_facemaps_poll(const bContext *C, wmWidgetGroupType *UNUSED(wgrouptype))
{
	Object *ob = CTX_data_active_object(C);

#ifdef USE_FACEMAP_FROM_BONE
	if (ob && BKE_object_pose_context_check(ob)) {
		for (bPoseChannel *pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
			if (pchan->fmap_object && pchan->fmap) {
				return true;
			}
		}
	}
#else
	if (ob && ob->type == OB_MESH && ob->fmaps.first) {
		ModifierData *md;
		VirtualModifierData virtualModifierData;

		md = modifiers_getVirtualModifierList(ob, &virtualModifierData);

		/* exception for shape keys because we can edit those */
		for (; md; md = md->next) {
			if (modifier_isEnabled(CTX_data_scene(C), md, eModifierMode_Realtime) &&
			    md->type == eModifierType_Armature)
			{
				ArmatureModifierData *amd = (ArmatureModifierData *) md;
				if (amd->object && (amd->deformflag & ARM_DEF_FACEMAPS))
					return true;
			}
		}
	}
#endif

	return false;
}

static void WIDGET_armature_facemaps_select(bContext *C, wmWidget *widget, const int action)
{
	Object *ob = CTX_data_active_object(C);

	switch (action) {
		case SEL_SELECT:
			for (bPoseChannel *pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
				if (pchan->fmap == WIDGET_facemap_get_fmap(widget)) {
					/* deselect all first */
					ED_pose_de_selectall(ob, SEL_DESELECT, false);
					ED_pose_bone_select(ob, pchan, true);
				}
			}
			break;
		default:
			BLI_assert(0);
	}
}

void WIDGETGROUP_armature_facemaps_create(const bContext *C, wmWidgetGroup *wgroup)
{
	Object *ob = CTX_data_active_object(C);
	bArmature *arm = (bArmature *)ob->data;

#ifdef USE_FACEMAP_FROM_BONE
	bPoseChannel *pchan;

	for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
		if (pchan->fmap && (pchan->bone->layer & arm->layer)) {
			ThemeWireColor *bcol = ED_pchan_get_colorset(arm, ob->pose, pchan);
			Object *fmap_ob = pchan->fmap_object;
			bFaceMap *fmap = pchan->fmap;
			float col[4] = {0.8f, 0.8f, 0.45f, 0.2f};
			float col_hi[4] = {0.8f, 0.8f, 0.45f, 0.4f};

			/* get custom bone group color */
			if (bcol) {
				rgb_uchar_to_float(col, (unsigned char *)bcol->solid);
				rgb_uchar_to_float(col_hi, (unsigned char *)bcol->active);
			}

			wmWidget *widget = WIDGET_facemap_new(wgroup, fmap->name, 0, fmap_ob, BLI_findindex(&fmap_ob->fmaps, fmap));

			WM_widget_set_operator(widget, "TRANSFORM_OT_translate");
			WM_widget_set_colors(widget, col, col_hi);
			WM_widget_set_flag(widget, WM_WIDGET_DRAW_HOVER, true);
			WM_widget_set_func_select(widget, WIDGET_armature_facemaps_select);
			PointerRNA *opptr = WM_widget_set_operator(widget, "TRANSFORM_OT_translate");
			RNA_boolean_set(opptr, "release_confirm", true);
		}
	}
#else
	Object *armature;
	ModifierData *md;
	VirtualModifierData virtualModifierData;
	int index = 0;
	bFaceMap *fmap = ob->fmaps.first;


	md = modifiers_getVirtualModifierList(ob, &virtualModifierData);

	/* exception for shape keys because we can edit those */
	for (; md; md = md->next) {
		if (modifier_isEnabled(CTX_data_scene(C), md, eModifierMode_Realtime) && md->type == eModifierType_Armature) {
			ArmatureModifierData *amd = (ArmatureModifierData *) md;
			if (amd->object && (amd->deformflag & ARM_DEF_FACEMAPS)) {
				armature = amd->object;
				break;
			}
		}
	}


	for (; fmap; fmap = fmap->next, index++) {
		if (BKE_pose_channel_find_name(armature->pose, fmap->name)) {
			PointerRNA *opptr;

			widget = WIDGET_facemap_new(wgroup, fmap->name, 0, ob, index);

			RNA_pointer_create(&ob->id, &RNA_FaceMap, fmap, &famapptr);
			WM_widget_set_colors(widget, color_shape, color_shape);
			WM_widget_set_flag(widget, WM_WIDGET_DRAW_HOVER, true);
			opptr = WM_widget_set_operator(widget, "TRANSFORM_OT_translate");
			if ((prop = RNA_struct_find_property(opptr, "release_confirm"))) {
				RNA_property_boolean_set(opptr, prop, true);
			}
		}
	}
#endif
}

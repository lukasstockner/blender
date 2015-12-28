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
 * The Original Code is Copyright (C) 2014 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Julian Eisel, Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/windowmanager/widgets/wm_widget.cc
 *  \ingroup wm
 */

#include <string.h>

#include "BKE_context.h"

#include "BLI_math.h"
#include "BLI_path_util.h"
#include "BLI_string.h"

#include "DNA_defs.h"
#include "DNA_listBase.h"
#include "DNA_view3d_types.h"
#include "DNA_windowmanager_types.h"

#include "ED_screen.h"
#include "ED_view3d.h"

#include "GL/glew.h"

#include "MEM_guardedalloc.h"

#include "RNA_access.h"

#include "WM_api.h"

#include "wm_widgetmap.h"
#include "wm_widgetgroup.h"
#include "wm_widget.h" // own include

#include "wm.h" // tmp


/**
 * Register \a widget.
 *
 * \param name  name used to create a unique idname for \a widget in \a wgroup
 */
wmWidget::wmWidget(wmWidgetGroup *wgroup, const char *name, const int max_prop_)
    : wgroup(wgroup),
      line_width(1.0f),
      user_scale(1.0f),
      max_prop(max_prop_)
{
	const float col_default[4] = {1.0f, 1.0f, 1.0f, 1.0f};

	unique_idname_set(name);

	copy_v4_v4(col, col_default);
	copy_v4_v4(col_hi, col_default);

	props = (PropertyRNA **)MEM_callocN(sizeof(PropertyRNA *) * max_prop, "widget->props");
	ptr = (PointerRNA *)MEM_callocN(sizeof(PointerRNA) * max_prop, "widget->ptr");

	BLI_addtail(&wgroup->widgets, this);

	/* XXX */
	fix_linking_widget_arrow();
	fix_linking_widget_cage();
	fix_linking_widget_dial();
	fix_linking_widget_plane();
	fix_linking_widget_facemap();
}

wmWidget::~wmWidget()
{
	if (opptr.data) {
		WM_operator_properties_free(&opptr);
	}

	MEM_freeN(props);
	MEM_freeN(ptr);
}

void wmWidget::unregister(ListBase *widgetlist)
{
	BLI_remlink(widgetlist, this);
}

/**
 * Assign an idname that is unique in \a wgroup to \a widget.
 *
 * \param rawname  Name used as basis to define final unique idname.
 */
void wmWidget::unique_idname_set(const char *rawname)
{
	char groupname[MAX_NAME];

	WM_widgetgrouptype_idname_get(wgroup->type, groupname);

	if (groupname[0]) {
		BLI_snprintf(idname, sizeof(idname), "%s_%s", groupname, rawname);
	}
	else {
		BLI_strncpy(idname, rawname, sizeof(idname));
	}

	/* ensure name is unique, append '.001', '.002', etc if not */
	BLI_uniquename(&wgroup->widgets, this, "Widget", '.', offsetof(wmWidget, idname), sizeof(idname));
}

void widget_find_active_3D_loop(const bContext *C, ListBase *visible_widgets)
{
	int selectionbase = 0;
	wmWidget *widget;

	for (LinkData *link = (LinkData *)visible_widgets->first; link; link = link->next) {
		widget = (wmWidget *)link->data;
		/* pass the selection id shifted by 8 bits. Last 8 bits are used for selected widget part id */
		widget->render_3d_intersection(C, widget, selectionbase << 8);

		selectionbase++;
	}
}

/**
 * Add \a widget to selection.
 * Reallocates memory for selected widgets so better not call for selecting multiple ones.
 */
void wmWidget::add_to_selection(wmWidgetMap *wmap, bContext *C)
{
	if (flag & WM_WIDGET_SELECTED)
		return;

	wmWidget ***sel = &wmap->wmap_context.selected_widgets;
	int *tot_selected = &wmap->wmap_context.tot_selected;

	(*tot_selected)++;

	*sel = (wmWidget **)MEM_reallocN(*sel, sizeof(**sel) * (*tot_selected));
	(*sel)[(*tot_selected) - 1] = this;

	flag |= WM_WIDGET_SELECTED;
	if (select) {
		select(C, this, SEL_SELECT);
	}
	wmap->set_highlighted_widget(C, this, highlighted_part);

	ED_region_tag_redraw(CTX_wm_region(C));
}

/**
 * Remove \a widget from selection.
 * Reallocates memory for selected widgets so better not call for selecting multiple ones.
 */
void wmWidget::remove_from_selection(wmWidgetMap *wmap, const bContext *C)
{
	wmWidget ***sel = &wmap->wmap_context.selected_widgets;
	int *tot_selected = &wmap->wmap_context.tot_selected;

	/* caller should check! */
	BLI_assert(flag & WM_WIDGET_SELECTED);

	/* remove widget from selected_widgets array */
	for (int i = 0; i < (*tot_selected); i++) {
		if (widget_compare((*sel)[i], this)) {
			for (int j = i; j < ((*tot_selected) - 1); j++) {
				(*sel)[j] = (*sel)[j + 1];
			}
			break;
		}
	}

	/* update array data */
	if ((*tot_selected) <= 1) {
		MEM_SAFE_FREE(*sel);
		*tot_selected = 0;
	}
	else {
		*sel = (wmWidget **)MEM_reallocN(*sel, sizeof(**sel) * (*tot_selected));
		(*tot_selected)--;
	}

	flag &= ~WM_WIDGET_SELECTED;

	ED_region_tag_redraw(CTX_wm_region(C));
}

void wmWidget::calculate_scale(const bContext *C)
{
	const RegionView3D *rv3d = CTX_wm_region_view3d(C);
	float scale_ = 1.0f;

	if (rv3d && (U.tw_flag & V3D_3D_WIDGETS) == 0 && (flag & WM_WIDGET_SCALE_3D)) {
		if (get_final_position) {
			float position[3];

			get_final_position(this, position);
			scale_ = ED_view3d_pixel_size(rv3d, position) * (float)U.tw_size;
		}
		else {
			scale_ = ED_view3d_pixel_size(rv3d, origin) * (float)U.tw_size;
		}
	}

	scale = scale_ * user_scale;
}

bool widget_compare(const wmWidget *a, const wmWidget *b)
{
	return STREQ(a->idname, b->idname);
}

void wmWidget::handle(bContext *C, const wmEvent *event, const int handle_flag)
{
	if (handler)
		handler(C, event, this, handle_flag);
}

void wmWidget::tweak_cancel(bContext *C)
{
	if (cancel)
		cancel(C, this);
}


/* -------------------------------------------------------------------- */
/* Widget drawing */

/**
 * \brief Main draw call for WidgetDrawInfo data
 */
void widget_draw(WidgetDrawInfo *info, const bool select)
{
	GLuint buf[3];

	const bool use_lighting = !select && ((U.tw_flag & V3D_SHADED_WIDGETS) != 0);

	if (use_lighting)
		glGenBuffers(3, buf);
	else
		glGenBuffers(2, buf);

	glEnableClientState(GL_VERTEX_ARRAY);
	glBindBuffer(GL_ARRAY_BUFFER, buf[0]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 3 * info->nverts, info->verts, GL_STATIC_DRAW);
	glVertexPointer(3, GL_FLOAT, 0, NULL);

	if (use_lighting) {
		glEnableClientState(GL_NORMAL_ARRAY);
		glBindBuffer(GL_ARRAY_BUFFER, buf[2]);
		glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 3 * info->nverts, info->normals, GL_STATIC_DRAW);
		glNormalPointer(GL_FLOAT, 0, NULL);
		glShadeModel(GL_SMOOTH);
	}

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buf[1]);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned short) * (3 * info->ntris), info->indices, GL_STATIC_DRAW);

	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);

	glDrawElements(GL_TRIANGLES, info->ntris * 3, GL_UNSIGNED_SHORT, NULL);

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	glDisableClientState(GL_VERTEX_ARRAY);

	if (use_lighting) {
		glDisableClientState(GL_NORMAL_ARRAY);
		glShadeModel(GL_FLAT);
		glDeleteBuffers(3, buf);
	}
	else {
		glDeleteBuffers(2, buf);
	}
}


/* -------------------------------------------------------------------- */

/** \name Widget Creation API
 *
 * API for defining data on widget creation.
 *
 * \{ */

const char *wmWidget::idname_get()
{
	return idname;
}

void wmWidget::property_set(const int slot, PointerRNA *ptr_, const char *propname)
{
	if (slot < 0 || slot >= max_prop) {
		fprintf(stderr, "invalid index %d when binding property for widget type %s\n", slot, idname);
		return;
	}

	/* if widget evokes an operator we cannot use it for property manipulation */
	opname = NULL;
	ptr[slot] = *ptr_;
	props[slot] = RNA_struct_find_property(ptr_, propname);

	if (bind_to_prop)
		bind_to_prop(this, slot);
}

PointerRNA *wmWidget::operator_set(const char *opname_)
{
	wmOperatorType *ot = WM_operatortype_find(opname_, 0);

	if (ot) {
		opname = opname_;

		WM_operator_properties_create_ptr(&opptr, ot);

		return &opptr;
	}
	else {
		fprintf(stderr, "Error binding operator to widget: operator %s not found!\n", opname_);
	}

	return NULL;
}

const char *wmWidget::operatorname_get()
{
	return opname;
}

void wmWidget::func_handler_set(int (*handler_)(bContext *, const wmEvent *, wmWidget *, const int))
{
	handler = handler_;
}

/**
 * \brief Set widget select callback.
 *
 * Callback is called when widget gets selected/deselected.
 */
void wmWidget::func_select_set(void (*select_)(bContext *, wmWidget *, const int ))
{
	flag |= WM_WIDGET_SELECTABLE;
	select = select_;
}

void wmWidget::origin_set(const float origin_[3])
{
	copy_v3_v3(origin, origin_);
}

void wmWidget::offset_set(const float offset_[3])
{
	copy_v3_v3(offset, offset_);
}

void wmWidget::flag_set(const int flag_, const bool enable)
{
	if (enable) {
		flag |= flag_;
	}
	else {
		flag &= ~flag_;
	}
}

bool wmWidget::flag_is_set(const int flag_)
{
	return (flag & flag_);
}

void wmWidget::scale_set(const float scale_)
{
	user_scale = scale_;
}

void wmWidget::line_width_set(const float line_width_)
{
	line_width = line_width_;
}

/**
 * Set widget rgba colors.
 *
 * \param col  Normal state color.
 * \param col_hi  Highlighted state color.
 */
void wmWidget::colors_set(const float col_[4], const float col_hi_[4])
{
	copy_v4_v4(col, col_);
	copy_v4_v4(col_hi, col_hi_);
}

/** \} */ // Widget Creation API


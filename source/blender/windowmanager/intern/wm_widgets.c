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
 * Contributor(s): Blender Foundation, 2008
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/windowmanager/intern/wm_widgets.c
 *  \ingroup wm
 *
 * Window management, widget API.
 */

#include <stdlib.h>
#include <string.h>

#include "DNA_screen_types.h"
#include "DNA_scene_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_view3d_types.h"
#include "DNA_userdef_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_ghash.h"
#include "BLI_string.h"
#include "BLI_math.h"
#include "BLI_path_util.h"

#include "BKE_context.h"
#include "BKE_idprop.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_screen.h"

#include "ED_view3d.h"
#include "ED_screen.h"
#include "ED_util.h"

#include "WM_api.h"
#include "WM_types.h"
#include "wm.h"
#include "wm_window.h"
#include "wm_event_system.h"
#include "wm_event_types.h"
#include "wm_draw.h"

#include "GL/glew.h"
#include "GPU_select.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "BPY_extern.h"


wmWidget *WM_widget_new(void (*draw)(const bContext *C, wmWidget *customdata),
                        void (*render_3d_intersection)(const bContext *C, wmWidget *customdata, int selectionbase),
                        int  (*intersect)(bContext *C, const wmEvent *event, wmWidget *widget),
                        int  (*handler)(bContext *C, const wmEvent *event, wmWidget *widget, const int flag))
{
	wmWidget *widget = MEM_callocN(sizeof(wmWidget), "widget");

	widget->draw = draw;
	widget->handler = handler;
	widget->intersect = intersect;
	widget->render_3d_intersection = render_3d_intersection;

	fix_linking_widget_lib();

	return widget;
}

BLI_INLINE bool widget_compare(const wmWidget *a, const wmWidget *b)
{
	return STREQ(a->idname, b->idname);
}

/**
 * Assign an idname that is unique in \a wgroup to \a widget.
 *
 * \param rawname  Name used as basis to define final unique idname.
 */
static void widget_unique_idname_set(wmWidgetGroup *wgroup, wmWidget *widget, const char *rawname)
{
	char groupname[MAX_NAME];

	WM_widgetgrouptype_idname_get(wgroup->type, groupname);

	if (groupname[0]) {
		BLI_snprintf(widget->idname, sizeof(widget->idname), "%s_%s", groupname, rawname);
	}
	else {
		BLI_strncpy(widget->idname, rawname, sizeof(widget->idname));
	}

	/* ensure name is unique, append '.001', '.002', etc if not */
	BLI_uniquename(&wgroup->widgets, widget, "Widget", '.', offsetof(wmWidget, idname), sizeof(widget->idname));
}

/**
 * Register \a widget.
 *
 * \param name  name used to create a unique idname for \a widget in \a wgroup
 */
bool wm_widget_register(wmWidgetGroup *wgroup, wmWidget *widget, const char *name)
{
	const float col_default[4] = {1.0f, 1.0f, 1.0f, 1.0f};

	widget_unique_idname_set(wgroup, widget, name);

	widget->user_scale = 1.0f;
	widget->line_width = 1.0f;

	/* defaults */
	copy_v4_v4(widget->col, col_default);
	copy_v4_v4(widget->col_hi, col_default);

	/* create at least one property for interaction */
	if (widget->max_prop == 0) {
		widget->max_prop = 1;
	}

	widget->props = MEM_callocN(sizeof(PropertyRNA *) * widget->max_prop, "widget->props");
	widget->ptr = MEM_callocN(sizeof(PointerRNA) * widget->max_prop, "widget->ptr");
	widget->opptr = PointerRNA_NULL;

	widget->wgroup = wgroup;

	BLI_addtail(&wgroup->widgets, widget);
	return true;
}


/** \name Widget Creation API
 *
 * API for defining data on widget creation.
 *
 * \{ */

void WM_widget_set_property(wmWidget *widget, const int slot, PointerRNA *ptr, const char *propname)
{
	if (slot < 0 || slot >= widget->max_prop) {
		fprintf(stderr, "invalid index %d when binding property for widget type %s\n", slot, widget->idname);
		return;
	}

	/* if widget evokes an operator we cannot use it for property manipulation */
	widget->opname = NULL;
	widget->ptr[slot] = *ptr;
	widget->props[slot] = RNA_struct_find_property(ptr, propname);

	if (widget->bind_to_prop)
		widget->bind_to_prop(widget, slot);
}

PointerRNA *WM_widget_set_operator(wmWidget *widget, const char *opname)
{
	wmOperatorType *ot = WM_operatortype_find(opname, 0);

	if (ot) {
		widget->opname = opname;

		WM_operator_properties_create_ptr(&widget->opptr, ot);

		return &widget->opptr;
	}
	else {
		fprintf(stderr, "Error binding operator to widget: operator %s not found!\n", opname);
	}

	return NULL;
}

/**
 * \brief Set widget select callback.
 *
 * Callback is called when widget gets selected/deselected.
 */
void WM_widget_set_func_select(wmWidget *widget, void (*select)(bContext *, wmWidget *, const int action))
{
	widget->flag |= WM_WIDGET_SELECTABLE;
	widget->select = select;
}

void WM_widget_set_origin(wmWidget *widget, const float origin[3])
{
	copy_v3_v3(widget->origin, origin);
}

void WM_widget_set_offset(wmWidget *widget, const float offset[3])
{
	copy_v3_v3(widget->offset, offset);
}

void WM_widget_set_flag(wmWidget *widget, const int flag, const bool enable)
{
	if (enable) {
		widget->flag |= flag;
	}
	else {
		widget->flag &= ~flag;
	}
}

void WM_widget_set_scale(wmWidget *widget, const float scale)
{
	widget->user_scale = scale;
}

void WM_widget_set_line_width(wmWidget *widget, const float line_width)
{
	widget->line_width = line_width;
}

/**
 * Set widget rgba colors.
 *
 * \param col  Normal state color.
 * \param col_hi  Highlighted state color.
 */
void WM_widget_set_colors(wmWidget *widget, const float col[4], const float col_hi[4])
{
	copy_v4_v4(widget->col, col);
	copy_v4_v4(widget->col_hi, col_hi);
}

/** \} */ // Widget Creation API


/** \name Widget operators
 *
 * Basic operators for widget interaction with user configurable keymaps.
 *
 * \{ */

static int widget_select_invoke(bContext *C, wmOperator *op)
{
	ARegion *ar = CTX_wm_region(C);

	bool extend = RNA_boolean_get(op->ptr, "extend");
	bool deselect = RNA_boolean_get(op->ptr, "deselect");
	bool toggle = RNA_boolean_get(op->ptr, "toggle");


	for (Link *link = ar->widgetmaps.first; link; link = link->next) {
		struct wmWidgetMap *wmap = (struct wmWidgetMap *)link;
		wmWidget *highlighted = wm_widgetmap_highlighted_widget_get(wmap);

		/* deselect all first */
		if (extend == false && deselect == false && toggle == false) {
			WM_widgetmap_select_all(wmap, C, SEL_DESELECT);
		}

		if (highlighted) {
			const bool is_selected = (highlighted->flag & WM_WIDGET_SELECTED);

			if (toggle) {
				/* toggle: deselect if already selected, else select */
				deselect = is_selected;
			}

			if (deselect) {
				if (is_selected)
					wm_widget_deselect(wmap, C, highlighted);
			}
			else {
				wm_widget_select(wmap, C, highlighted);
			}

			return OPERATOR_FINISHED;
		}
		else {
			BLI_assert(0);
			return (OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH);
		}
	}

	return OPERATOR_PASS_THROUGH;
}

void WIDGETGROUP_OT_widget_select(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Widget Select";
	ot->description = "Select the currently highlighted widget";
	ot->idname = "WIDGETGROUP_OT_widget_select";

	/* api callbacks */
	ot->exec = widget_select_invoke;

	ot->flag = OPTYPE_UNDO;

	WM_operator_properties_mouse_select(ot);
}

typedef struct WidgetTweakData {
	wmWidgetMapC *wmap;
	wmWidget *active;

	int init_event; /* initial event type */
	int flag;       /* tweak flags */
} WidgetTweakData;

enum {
	TWEAK_MODAL_CANCEL = 1,
	TWEAK_MODAL_CONFIRM,
	TWEAK_MODAL_PRECISION_ON,
	TWEAK_MODAL_PRECISION_OFF,
};

static void widget_tweak_finish(bContext *C, wmOperator *op)
{
	WidgetTweakData *wtweak = op->customdata;
	wm_widgetmap_active_widget_set(wtweak->wmap, C, NULL, NULL);
	MEM_freeN(wtweak);
}

static void widget_tweak_cancel(bContext *C, wmOperator *op)
{
	WidgetTweakData *wtweak = op->customdata;
	if (wtweak->active->cancel) {
		wtweak->active->cancel(C, wtweak->active);
	}
	widget_tweak_finish(C, op);
}

static int widget_tweak_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
	WidgetTweakData *wtweak = op->customdata;
	wmWidget *widget = wtweak->active;

	if (!widget) {
		BLI_assert(0);
		return (OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH);
	}

	if (event->type == wtweak->init_event && event->val == KM_RELEASE) {
		widget_tweak_finish(C, op);
		return OPERATOR_FINISHED;
	}


	if (event->type == EVT_MODAL_MAP) {
		switch (event->val) {
			case TWEAK_MODAL_CANCEL:
				widget_tweak_cancel(C, op);
				return OPERATOR_CANCELLED;
			case TWEAK_MODAL_CONFIRM:
				widget_tweak_finish(C, op);
				return OPERATOR_FINISHED;
			case TWEAK_MODAL_PRECISION_ON:
				wtweak->flag |= WM_WIDGET_TWEAK_PRECISE;
				break;
			case TWEAK_MODAL_PRECISION_OFF:
				wtweak->flag &= ~WM_WIDGET_TWEAK_PRECISE;
				break;
		}
	}

	/* handle widget */
	if (widget->handler) {
		widget->handler(C, event, widget, wtweak->flag);
	}

	/* Ugly hack to send widget events */
	((wmEvent *)event)->type = EVT_WIDGET_UPDATE;

	/* always return PASS_THROUGH so modal handlers
	 * with widgets attached can update */
	return OPERATOR_PASS_THROUGH;
}

static int widget_tweak_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	ARegion *ar = CTX_wm_region(C);
	wmWidgetMapC *wmap;
	wmWidget *widget;

	for (wmap = ar->widgetmaps.first; wmap; wmap = wmap->next)
		if ((widget = wmap->wmap_context.highlighted_widget))
			break;

	if (!widget) {
		/* wm_handlers_do_intern shouldn't let this happen */
		BLI_assert(0);
		return (OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH);
	}


	/* activate highlighted widget */
	wm_widgetmap_active_widget_set(wmap, C, event, widget);

	/* XXX temporary workaround for modal widget operator
	 * conflicting with modal operator attached to widget */
	if (widget->opname) {
		wmOperatorType *ot = WM_operatortype_find(widget->opname, true);
		if (ot->modal) {
			return OPERATOR_FINISHED;
		}
	}


	WidgetTweakData *wtweak = MEM_mallocN(sizeof(WidgetTweakData), __func__);

	wtweak->init_event = event->type;
	wtweak->active = wmap->wmap_context.highlighted_widget;
	wtweak->wmap = wmap;
	wtweak->flag = 0;

	op->customdata = wtweak;

	WM_event_add_modal_handler(C, op);

	return OPERATOR_RUNNING_MODAL;
}

void WIDGETGROUP_OT_widget_tweak(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Widget Tweak";
	ot->description = "Tweak the active widget";
	ot->idname = "WIDGETGROUP_OT_widget_tweak";

	/* api callbacks */
	ot->invoke = widget_tweak_invoke;
	ot->modal = widget_tweak_modal;
	ot->cancel = widget_tweak_cancel;
}

/** \} */ // Widget operators


static wmKeyMap *widgetgroup_tweak_modal_keymap(wmKeyConfig *keyconf, const char *wgroupname)
{
	wmKeyMap *keymap;
	char name[MAX_NAME];

	static EnumPropertyItem modal_items[] = {
		{TWEAK_MODAL_CANCEL, "CANCEL", 0, "Cancel", ""},
		{TWEAK_MODAL_CONFIRM, "CONFIRM", 0, "Confirm", ""},
		{TWEAK_MODAL_PRECISION_ON, "PRECISION_ON", 0, "Enable Precision", ""},
		{TWEAK_MODAL_PRECISION_OFF, "PRECISION_OFF", 0, "Disable Precision", ""},
		{0, NULL, 0, NULL, NULL}
	};


	BLI_snprintf(name, sizeof(name), "%s Tweak Modal Map", wgroupname);
	keymap = WM_modalkeymap_get(keyconf, name);

	/* this function is called for each spacetype, only needs to add map once */
	if (keymap && keymap->modal_items)
		return NULL;

	keymap = WM_modalkeymap_add(keyconf, name, modal_items);


	/* items for modal map */
	WM_modalkeymap_add_item(keymap, ESCKEY, KM_PRESS, KM_ANY, 0, TWEAK_MODAL_CANCEL);
	WM_modalkeymap_add_item(keymap, RIGHTMOUSE, KM_PRESS, KM_ANY, 0, TWEAK_MODAL_CANCEL);

	WM_modalkeymap_add_item(keymap, RETKEY, KM_PRESS, KM_ANY, 0, TWEAK_MODAL_CONFIRM);
	WM_modalkeymap_add_item(keymap, PADENTER, KM_PRESS, KM_ANY, 0, TWEAK_MODAL_CONFIRM);

	WM_modalkeymap_add_item(keymap, RIGHTSHIFTKEY, KM_PRESS, KM_ANY, 0, TWEAK_MODAL_PRECISION_ON);
	WM_modalkeymap_add_item(keymap, RIGHTSHIFTKEY, KM_RELEASE, KM_ANY, 0, TWEAK_MODAL_PRECISION_OFF);
	WM_modalkeymap_add_item(keymap, LEFTSHIFTKEY, KM_PRESS, KM_ANY, 0, TWEAK_MODAL_PRECISION_ON);
	WM_modalkeymap_add_item(keymap, LEFTSHIFTKEY, KM_RELEASE, KM_ANY, 0, TWEAK_MODAL_PRECISION_OFF);


	WM_modalkeymap_assign(keymap, "WIDGETGROUP_OT_widget_tweak");

	return keymap;
}

/**
 * Common default keymap for widget groups
 */
wmKeyMap *WM_widgetgroup_keymap_common(wmKeyConfig *config, const char *wgroupname)
{
	wmKeyMap *km = WM_keymap_find(config, wgroupname, 0, 0);
	wmKeyMapItem *kmi;

	WM_keymap_add_item(km, "WIDGETGROUP_OT_widget_tweak", ACTIONMOUSE, KM_PRESS, KM_ANY, 0);

	widgetgroup_tweak_modal_keymap(config, wgroupname);

	kmi = WM_keymap_add_item(km, "WIDGETGROUP_OT_widget_select", SELECTMOUSE, KM_PRESS, 0, 0);
	RNA_boolean_set(kmi->ptr, "extend", false);
	RNA_boolean_set(kmi->ptr, "deselect", false);
	RNA_boolean_set(kmi->ptr, "toggle", false);
	kmi = WM_keymap_add_item(km, "WIDGETGROUP_OT_widget_select", SELECTMOUSE, KM_PRESS, KM_SHIFT, 0);
	RNA_boolean_set(kmi->ptr, "extend", false);
	RNA_boolean_set(kmi->ptr, "deselect", false);
	RNA_boolean_set(kmi->ptr, "toggle", true);

	return km;
}


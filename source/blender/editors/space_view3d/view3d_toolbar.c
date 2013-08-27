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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_view3d/view3d_toolbar.c
 *  \ingroup spview3d
 */


#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>

#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_utildefines.h"
#include "BLI_ghash.h"

#include "BLF_translation.h"

#include "BKE_context.h"
#include "BKE_idprop.h"
#include "BKE_global.h"
#include "BKE_screen.h"


#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_enum_types.h"

#include "ED_screen.h"
#include "ED_util.h"
#include "ED_gpencil.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "view3d_intern.h"  /* own include */


/* ******************* view3d space & buttons ************** */

static void view3d_panel_operator_redo_buts(const bContext *C, Panel *pa, wmOperator *op)
{
	uiLayoutOperatorButs(C, pa->layout, op, NULL, 'H', 0);
}

static void view3d_panel_operator_redo_operator(const bContext *C, Panel *pa, wmOperator *op)
{
	if (op->type->flag & OPTYPE_MACRO) {
		for (op = op->macro.first; op; op = op->next) {
			uiItemL(pa->layout, RNA_struct_ui_name(op->type->srna), ICON_NONE);
			view3d_panel_operator_redo_operator(C, pa, op);
		}
	}
	else {
		uiItemL(pa->layout, RNA_struct_ui_name(op->type->srna), ICON_NONE);
		view3d_panel_operator_redo_buts(C, pa, op);
	}
}

/* TODO de-duplicate redo panel functions - campbell */
static void view3d_panel_operator_redo(const bContext *C, Panel *pa)
{
	wmOperator *op = WM_operator_last_redo(C);
	ARegion *ar;
	ARegion *ar1;

	if (op == NULL) {
		return;
	}

	/* keep in sync with logic in ED_undo_operator_repeat() */
	ar = CTX_wm_region(C);
	ar1 = BKE_area_find_region_active_win(CTX_wm_area(C));
	if (ar1)
		CTX_wm_region_set((bContext *)C, ar1);

	if (WM_operator_poll((bContext *)C, op->type)) {
		uiBlock *block = uiLayoutGetBlock(pa->layout);

		if (!WM_operator_check_ui_enabled(C, op->type->name))
			uiLayoutSetEnabled(pa->layout, FALSE);

		/* note, blockfunc is a default but->func, use Handle func to allow button callbacks too */
		uiBlockSetHandleFunc(block, ED_undo_operator_repeat_cb_evt, op);

		view3d_panel_operator_redo_operator(C, pa, op);
	}

	/* set region back */
	CTX_wm_region_set((bContext *)C, ar);
}

static void collapse_all_panels(bContext *C, void *UNUSED(arg1), void *UNUSED(arg2))
{
	ScrArea *sa = CTX_wm_area(C);
	ARegion *ar = CTX_wm_region(C);
	uiCollapseAllPanels(sa, ar, CTX_data_mode_string(C));
}

static void view3d_toolbar_header_draw(const bContext *C, Panel *pa)
{
	uiLayout *layout = pa->layout;
	uiLayout *row = uiLayoutRow(layout, TRUE);
	const char *name = "";
	int modeicon = UI_data_mode_icon(C);
	Object *ob = CTX_data_active_object(C);
	int modeselect = ob ? ob->mode : OB_MODE_OBJECT;
	EnumPropertyItem *item = object_mode_items;
	uiBut *but;
		
	while (item->identifier) {
		if (item->value == modeselect && item->identifier[0]) {
			name = IFACE_(item->name);
			break;
		}
		item++;
	}

	uiItemL(row, name, modeicon);

	// TODO: somehow this centers the button's text
	but = uiDefIconBut(uiLayoutGetBlock(row), BUT, 0, ICON_TRIA_RIGHT, 0, 0, UI_UNIT_X, UI_UNIT_Y, NULL, 0, 0, 0, 0, "Collapse all panels in this toolbar");
	//but = uiDefIconTextBut(uiLayoutGetBlock(column), BUT, 0, ICON_NONE, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Collapse All"), 0, 0, 0, UI_UNIT_Y, NULL, 0, 0, 0, 0, "");
	uiButSetFunc(but, collapse_all_panels, pa, NULL);
	
//	uiItemO(row, "", ICON_NONE, "WM_OT_create_custom_panel");
	
	uiDefIconButO(uiLayoutGetBlock(row), BUT, "WM_OT_create_custom_panel", WM_OP_INVOKE_DEFAULT, ICON_PLUS, 0, 0, UI_UNIT_X, UI_UNIT_Y, "Add a custom panel to this toolbar");
	
}

/* ******************* */

typedef struct CustomTool {
	struct CustomTool *next, *prev;
	char opname[OP_MAX_TYPENAME];
	char context[OP_MAX_TYPENAME];
} CustomTool;

static void operator_call_cb(struct bContext *C, void *arg_listbase, void *arg2)
{
	wmOperatorType *ot = arg2;
	
	if (ot) {
		CustomTool *ct = MEM_callocN(sizeof(CustomTool), "CustomTool");
		
		BLI_addtail(arg_listbase, ct);
		BLI_strncpy(ct->opname, ot->idname, OP_MAX_TYPENAME);
		BLI_strncpy(ct->context, CTX_data_mode_string(C), OP_MAX_TYPENAME);
	}
		
}

static void operator_search_cb(const struct bContext *C, void *UNUSED(arg), const char *str, uiSearchItems *items)
{
	GHashIterator *iter = WM_operatortype_iter();

	for (; !BLI_ghashIterator_done(iter); BLI_ghashIterator_step(iter)) {
		wmOperatorType *ot = BLI_ghashIterator_getValue(iter);

		if (BLI_strcasestr(ot->name, str)) {
			if (WM_operator_poll((bContext *)C, ot)) {
				
				if (false == uiSearchItemAdd(items, ot->name, ot, 0))
					break;
			}
		}
	}
	BLI_ghashIterator_free(iter);
}


/* ID Search browse menu, open */
static uiBlock *tool_search_menu(bContext *C, ARegion *ar, void *arg_listbase)
{
	static char search[OP_MAX_TYPENAME];
	wmEvent event;
	wmWindow *win = CTX_wm_window(C);
	uiBlock *block;
	uiBut *but;
	
	/* clear initial search string, then all items show */
	search[0] = 0;
	
	block = uiBeginBlock(C, ar, "_popup", UI_EMBOSS);
	uiBlockSetFlag(block, UI_BLOCK_LOOP | UI_BLOCK_REDRAW | UI_BLOCK_SEARCH_MENU);
	
	/* fake button, it holds space for search items */
	uiDefBut(block, LABEL, 0, "", 10, 15, uiSearchBoxWidth(), uiSearchBoxHeight(), NULL, 0, 0, 0, 0, NULL);
	
	but = uiDefSearchBut(block, search, 0, ICON_VIEWZOOM, sizeof(search), 10, 0, 150, 19, 0, 0, "");
	uiButSetSearchFunc(but, operator_search_cb, arg_listbase, operator_call_cb, NULL);
	
	uiBoundsBlock(block, 6);
	uiBlockSetDirection(block, UI_DOWN);
	uiEndBlock(C, block);
	
	event = *(win->eventstate);  /* XXX huh huh? make api call */
	event.type = EVT_BUT_OPEN;
	event.val = KM_PRESS;
	event.customdata = but;
	event.customdatafree = FALSE;
	wm_event_add(win, &event);
	
	return block;
}


static void view3d_panel_tool_shelf(const bContext *C, Panel *pa)
{
	SpaceLink *sl = CTX_wm_space_data(C);
	SpaceType *st = NULL;
	uiLayout *col;
	const char *context = CTX_data_mode_string(C);
	
	if (sl)
		st = BKE_spacetype_from_id(sl->spacetype);
	
	if (st && st->toolshelf.first) {
		CustomTool *ct;
		
		for (ct = st->toolshelf.first; ct; ct = ct->next) {
			if (0 == strncmp(context, ct->context, OP_MAX_TYPENAME)) {
				col = uiLayoutColumn(pa->layout, TRUE);
				uiItemFullO(col, ct->opname, NULL, ICON_NONE, NULL, WM_OP_INVOKE_REGION_WIN, 0);
			}
		}
	}
	col = uiLayoutColumn(pa->layout, TRUE);
	uiDefBlockBut(uiLayoutGetBlock(pa->layout), tool_search_menu, &st->toolshelf, "Add Tool", 0, 0, UI_UNIT_X, UI_UNIT_Y, "Add Tool in shelf, gets saved in files");
}


void view3d_toolshelf_register(ARegionType *art)
{
	PanelType *pt;

	pt = MEM_callocN(sizeof(PanelType), "spacetype view3d panel tools");
	strcpy(pt->idname, "VIEW3D_PT_tool_shelf");
	strcpy(pt->label, N_("Tool Shelf"));
	strcpy(pt->translation_context, BLF_I18NCONTEXT_DEFAULT_BPYRNA);
	pt->draw = view3d_panel_tool_shelf;
	BLI_addtail(&art->paneltypes, pt);
}

void view3d_tool_props_register(ARegionType *art)
{
	PanelType *pt;
	
	pt = MEM_callocN(sizeof(PanelType), "spacetype view3d panel last operator");
	strcpy(pt->idname, "VIEW3D_PT_last_operator");
	strcpy(pt->label, N_("Last Operator"));
	strcpy(pt->translation_context, BLF_I18NCONTEXT_DEFAULT_BPYRNA);
	pt->draw = view3d_panel_operator_redo;
	BLI_addtail(&art->paneltypes, pt);
}

void view3d_grease_register(ARegionType *art)
{
	PanelType *pt;
	
	pt = MEM_callocN(sizeof(PanelType), "spacetype view3d panel gpencil");
	strcpy(pt->idname, "VIEW3D_PT_gpencil");
	strcpy(pt->label, N_("Grease Pencil"));  /* XXX C panels are not available through RNA (bpy.types)! */
	strcpy(pt->translation_context, BLF_I18NCONTEXT_DEFAULT_BPYRNA);
	pt->draw_header = gpencil_panel_standard_header;
	pt->draw = gpencil_panel_standard;
	BLI_addtail(&art->paneltypes, pt);
}

void view3d_toolbar_header_register(ARegionType *art)
{
	PanelType *pt;
	
	pt = MEM_callocN(sizeof(PanelType), "spacetype view3d toolbar header");
	strcpy(pt->idname, "VIEW3D_PT_toolbar_header");
	strcpy(pt->label, N_("No Label!"));
	strcpy(pt->translation_context, BLF_I18NCONTEXT_DEFAULT_BPYRNA);
	pt->flag = PNL_NO_HEADER;
	pt->draw = view3d_toolbar_header_draw;
	BLI_addtail(&art->paneltypes, pt);
}

/* ********** operator to open/close toolshelf region */

static int view3d_toolshelf(bContext *C, wmOperator *UNUSED(op))
{
	ScrArea *sa = CTX_wm_area(C);
	ARegion *ar = BKE_area_find_region_type(sa, RGN_TYPE_TOOLS); //view3d_has_tools_region(sa);
	
	if (ar)
		ED_region_toggle_hidden(C, ar);

	return OPERATOR_FINISHED;
}

void VIEW3D_OT_toolshelf(wmOperatorType *ot)
{
	ot->name = "Tool Shelf";
	ot->description = "Toggles tool shelf display";
	ot->idname = "VIEW3D_OT_toolshelf";
	
	ot->exec = view3d_toolshelf;
	ot->poll = ED_operator_view3d_active;
	
	/* flags */
	ot->flag = 0;
}


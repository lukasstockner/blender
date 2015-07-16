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

/** \file blender/editors/interface/widgets/widgets.c
 *  \ingroup edinterface
 */

#include "BLI_utildefines.h"

#include "DNA_screen_types.h"

#include "UI_interface.h"
#include "UI_resources.h"


#include "widgets.h" /* own include */



uiWidgetDrawStyle *widget_drawstyle_get(const int widget_style_type) /* TODO widget draw styles are for later */
{
#if 0
	switch (widget_style_type) {
		case WIDGETSTYLE_TYPE_CLASSIC:
			return &WidgetStyle_Classic;
			break;
		default:
			BLI_assert(0);
	}
	return NULL;
#else
	return &WidgetStyle_Classic;

	(void)widget_style_type;
#endif
}

uiWidgetType *widget_type(const uiWidgetTypeEnum type)
{
	bTheme *btheme = UI_GetTheme();
	const uiWidgetDrawStyle *draw_style = widget_drawstyle_get(0); /* TODO drawstyles aren't in use yet */
	static uiWidgetType wt;


	/* defaults */
	wt.wcol_theme = &btheme->tui.wcol_regular;
	wt.wcol_state = &btheme->tui.wcol_state;
	wt.draw_type = NULL;

	switch (type) {
		case UI_WTYPE_BOX:
			wt.wcol_theme = &btheme->tui.wcol_box;
			wt.draw_type = draw_style->box;
			break;

		case UI_WTYPE_CHECKBOX:
			wt.wcol_theme = &btheme->tui.wcol_option;
			wt.draw_type = draw_style->checkbox;
			break;

		case UI_WTYPE_EXEC:
			wt.wcol_theme = &btheme->tui.wcol_tool;
			wt.draw_type = draw_style->exec;
			break;

		case UI_WTYPE_FILENAME:
			break;

		case UI_WTYPE_ICON:
			wt.draw_type = draw_style->icon;
			break;

		case UI_WTYPE_LABEL:
			wt.draw_type = draw_style->label;
			break;

		case UI_WTYPE_LINK:
			wt.draw_type = draw_style->link;
			break;

		case UI_WTYPE_LISTITEM:
			wt.wcol_theme = &btheme->tui.wcol_list_item;
			wt.draw_type = draw_style->listitem;
			break;

		case UI_WTYPE_MENU_BACK:
			wt.wcol_theme = &btheme->tui.wcol_menu_back;
			wt.draw_type = draw_style->menu_back;
			break;

		case UI_WTYPE_MENU_ICON_RADIO:
			wt.wcol_theme = &btheme->tui.wcol_menu;
			wt.draw_type = draw_style->menu_icon_radio;
			break;

		case UI_WTYPE_MENU_ITEM:
			wt.wcol_theme = &btheme->tui.wcol_menu_item;
			wt.draw_type = draw_style->menu_item;
			break;

		case UI_WTYPE_MENU_ITEM_PREVIEW:
			wt.wcol_theme = &btheme->tui.wcol_menu_item;
			wt.draw_type = draw_style->menu_item_preview;
			break;

		case UI_WTYPE_MENU_ITEM_RADIAL:
			wt.wcol_theme = &btheme->tui.wcol_pie_menu;
			wt.draw_type = draw_style->menu_item_radial;
			break;

		case UI_WTYPE_MENU_LABEL:
			wt.wcol_theme = &btheme->tui.wcol_menu_back;
			wt.draw_type = draw_style->menu_label;
			break;

		case UI_WTYPE_MENU_NODE_LINK:
			wt.wcol_theme = &btheme->tui.wcol_menu;
			wt.draw_type = draw_style->menu_node_link;
			break;

		case UI_WTYPE_MENU_POINTER_LINK:
			wt.wcol_theme = &btheme->tui.wcol_menu;
			wt.draw_type = draw_style->pointer_link;
			break;

		case UI_WTYPE_MENU_RADIO:
			wt.wcol_theme = &btheme->tui.wcol_menu;
			wt.draw_type = draw_style->menu_radio;
			break;

		case UI_WTYPE_NAME:
			wt.wcol_theme = &btheme->tui.wcol_text;
			wt.draw_type = draw_style->name;
			break;

		case UI_WTYPE_NAME_LINK:
			break;

		case UI_WTYPE_NUMBER:
			wt.wcol_theme = &btheme->tui.wcol_num;
			wt.draw_type = draw_style->number;
			break;

		case UI_WTYPE_POINTER_LINK:
			break;

		case UI_WTYPE_PROGRESSBAR:
			wt.wcol_theme = &btheme->tui.wcol_progress;
			wt.draw_type = draw_style->progressbar;
			break;

		case UI_WTYPE_PULLDOWN:
			wt.wcol_theme = &btheme->tui.wcol_pulldown;
			wt.draw_type = draw_style->pulldown;
			break;

		case UI_WTYPE_RADIO:
			wt.wcol_theme = &btheme->tui.wcol_radio;
			wt.draw_type = draw_style->radio;
			break;

		case UI_WTYPE_REGULAR:
			wt.draw_type = draw_style->regular;
			break;

		case UI_WTYPE_RGB_PICKER:
			break;

		case UI_WTYPE_SCROLL:
			wt.wcol_theme = &btheme->tui.wcol_scroll;
			wt.draw_type = draw_style->scroll;
			break;

		case UI_WTYPE_SLIDER:
			wt.wcol_theme = &btheme->tui.wcol_numslider;
			wt.draw_type = draw_style->slider;
			break;

		case UI_WTYPE_SWATCH:
			wt.draw_type = draw_style->swatch;
			break;

		case UI_WTYPE_TOGGLE:
			wt.wcol_theme = &btheme->tui.wcol_toggle;
			wt.draw_type = draw_style->toggle;
			break;

		case UI_WTYPE_TOOLTIP:
			wt.wcol_theme = &btheme->tui.wcol_tooltip;
			wt.draw_type = draw_style->tooltip;
			break;

		case UI_WTYPE_UNITVEC:
			wt.draw_type = draw_style->unitvec;
			break;
	}

	return &wt;
}

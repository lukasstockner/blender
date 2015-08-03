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

#ifndef __WIDGETS_H__
#define __WIDGETS_H__

/** \file blender/editors/interface/widgets/widgets.h
 *  \ingroup edinterface
 * 
 * \brief Blender widget API and internal functions
 * 
 * Seems a bit overkill do have such an isolated module just for widget drawing but plan is to
 * use this for widget handling as well and basically make it a standalone blenwidget module
 */

#include "DNA_userdef_types.h"

struct uiBut;
struct uiWidgetType;

typedef enum {
	/* default */
	UI_WTYPE_REGULAR,

	/* standard set */
	UI_WTYPE_LABEL,
	UI_WTYPE_TOGGLE,
	UI_WTYPE_CHECKBOX,
	UI_WTYPE_RADIO,
	UI_WTYPE_NUMBER,
	UI_WTYPE_SLIDER,
	UI_WTYPE_EXEC,
	UI_WTYPE_TOOLTIP,

	/* strings */
	UI_WTYPE_NAME,
	UI_WTYPE_NAME_LINK,
	UI_WTYPE_POINTER_LINK,
	UI_WTYPE_FILENAME,

	/* menus */
	UI_WTYPE_MENU_RADIO,
	UI_WTYPE_MENU_ICON_RADIO,
	UI_WTYPE_MENU_POINTER_LINK,
	UI_WTYPE_MENU_NODE_LINK,

	UI_WTYPE_PULLDOWN,
	UI_WTYPE_MENU_LABEL,
	UI_WTYPE_MENU_ITEM,
	UI_WTYPE_MENU_ITEM_PREVIEW,
	UI_WTYPE_MENU_ITEM_RADIAL,
	UI_WTYPE_MENU_BACK,
	UI_WTYPE_SEARCH_BACK,

	/* specials */
	UI_WTYPE_ICON,
	UI_WTYPE_EXTRA,
	UI_WTYPE_SWATCH,
	UI_WTYPE_RGB_PICKER,
	UI_WTYPE_UNITVEC,
	UI_WTYPE_COLORBAND,
	UI_WTYPE_HSV_CIRCLE,
	UI_WTYPE_HSV_CUBE,
	UI_WTYPE_HSV_VERT,
	UI_WTYPE_BOX,
	UI_WTYPE_SCROLL_BACK,
	UI_WTYPE_SCROLL_INNER,
	UI_WTYPE_LISTSCROLL,        /* scroll widget within lists */
	UI_WTYPE_LISTITEM,
	UI_WTYPE_PROGRESSBAR,
	UI_WTYPE_LINK,
	UI_WTYPE_SEPARATOR,
} uiWidgetTypeEnum;

/** uiWidgetType: for time being only for visual appearance,
 * later, a handling callback can be added too
 */
typedef struct uiWidgetType {
	/* pointer to theme color definition */
	uiWidgetColors *wcol_theme;
	uiWidgetStateColors *wcol_state;

	/* converted colors for state */
	uiWidgetColors wcol;

	struct uiWidgetDrawType *draw_type;
} uiWidgetType;

typedef struct uiWidgetDrawType {
	void (*state)(struct uiWidgetType *, int state);
	void (*draw)(struct uiWidgetColors *, rcti *, int state, int roundboxalign);
	/* XXX uiBut and uiFontStyle shouldn't be needed/used at this level,
	 * the needed data should be transferred using uiWidget API instead */
	void (*custom)(struct uiBut *, struct uiWidgetColors *, rcti *, int state, int roundboxalign);
	void (*text)(struct uiFontStyle *, struct uiWidgetColors *, struct uiBut *, rcti *, const char *str, const int iconid);
} uiWidgetDrawType;

typedef struct uiWidgetDrawStyle {
	/* let's keep this in a nice alphabetical order! */
	uiWidgetDrawType *box,
	                 *checkbox,
	                 *colorband,
	                 *exec,
	                 *extra_mask,
	                 *filename,
	                 *hsv_circle,
	                 *hsv_cube,
	                 *hsv_vert,
	                 *icon,
	                 *label,
	                 *link,
	                 *listitem,
	                 *listscroll,
	                 *menu_back,
	                 *menu_icon_radio,
	                 *menu_item,
	                 *menu_item_preview,
	                 *menu_item_radial,
	                 *menu_label,
	                 *menu_node_link,
	                 *menu_pointer_link,
	                 *menu_radio,
	                 *name,
	                 *name_link,
	                 *number,
	                 *pointer_link,
	                 *progressbar,
	                 *pulldown,
	                 *radio,
	                 *regular,
	                 *rgb_picker,
	                 *scroll_back,
	                 *scroll_inner,
	                 *search_back,
	                 *separator,
	                 *slider,
	                 *swatch,
	                 *toggle,
	                 *tooltip,
	                 *unitvec;
} uiWidgetDrawStyle;


/* *** external API *** */

uiWidgetType *WidgetTypeInit(const uiWidgetTypeEnum type);
void WidgetDraw(
        uiWidgetType *wt,
        uiFontStyle *fstyle, uiBut *but, rcti *rect,
        int state, int roundboxalign, const int iconid, const char *str,
        const bool use_text_blend);


/* *** internal *** */

uiWidgetDrawStyle *widget_drawstyle_get(const int widget_style_type);

extern struct uiWidgetDrawStyle WidgetStyle_Classic;

#endif  /* __WIDGETS_H__ */


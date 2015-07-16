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
 * \brief Blender Widget API
 * 
 * Seems a bit overkill do have such an isolated module just for widget drawing but plan is to
 * use this for widget handling as well and basically make it a standalone blenwidget module
 */

#include "DNA_userdef_types.h"

#include "widgets_draw_intern.h" /* XXX */

struct uiBut;
struct uiWidgetType;

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
	void (*text)(struct uiFontStyle *, struct uiWidgetColors *, struct uiBut *, rcti *);
} uiWidgetDrawType;

#if 0 /* uiWidgetDrawType init template */

uiWidgetDrawType drawtype_xxx_xxx = {
	/* state */  NULL,
	/* draw */   widget_roundbut,
	/* custom */ NULL,
	/* text */   NULL,
};
#endif

typedef struct uiWidgetDrawStyle {
	/* let's keep this in a nice alphabetical order! */
	uiWidgetDrawType *box,
	                 *checkbox,
	                 *exec,
	                 *filename,
	                 *icon,
	                 *label,
	                 *link,
	                 *listitem,
	                 *menu_back,
	                 *menu_icon_radio,
	                 *menu_item,
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
	                 *scroll,
	                 *slider,
	                 *swatch,
	                 *toggle,
	                 *tooltip,
	                 *unitvec;
} uiWidgetDrawStyle;

#if 0 /* uiWidgetDrawStyle init template */

uiWidgetDrawStyle WidgetStyle_xxx = {
	/* box */               NULL,
	/* checkbox */          NULL,
	/* exec */              NULL,
	/* filename */          NULL,
	/* icon */              NULL,
	/* label */             NULL,
	/* listitem */          NULL,
	/* menu_back */         NULL,
	/* menu_icon_radio */   NULL,
	/* menu_item */         NULL,
	/* menu_item_radial */  NULL,
	/* menu_node_link */    NULL,
	/* menu_pointer_link */ NULL,
	/* menu_radio */        NULL,
	/* name */              NULL,
	/* name_link */         NULL,
	/* number */            NULL,
	/* pointer_link */      NULL,
	/* progressbar */       NULL,
	/* pulldown */          NULL,
	/* radio */             NULL,
	/* regular */           NULL,
	/* rgb_picker */        NULL,
	/* scroll */            NULL,
	/* slider */            NULL,
	/* swatch */            NULL,
	/* toggle */            NULL,
	/* tooltip */           NULL,
	/* unitvec */           NULL,
};
#endif

extern struct uiWidgetDrawStyle WidgetStyle_Classic;

uiWidgetDrawStyle *widget_drawstyle_get(const int widget_style_type);

#endif  /* __WIDGETS_H__ */


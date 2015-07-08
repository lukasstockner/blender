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

#include "widgets_draw_intern.h" /* XXX */

struct uiBut;
struct uiWidgetType;

typedef struct uiWidgetDrawType {
	void (*state)(struct uiWidgetType *, int state);
	void (*draw)(uiWidgetColors *, rcti *, int state, int roundboxalign);
	/* XXX uiBut and uiFontStyle shouldn't be needed/used at this level,
	 * the needed data should be transferred using uiWidget API instead */
	void (*custom)(struct uiBut *, uiWidgetColors *, rcti *, int state, int roundboxalign);
	void (*text)(struct uiFontStyle *, uiWidgetColors *, struct uiBut *, rcti *);
} uiWidgetDrawType;

typedef struct uiWidgetDrawStyle {
	uiWidgetDrawType *widget_exec;
} uiWidgetDrawStyle;

extern struct uiWidgetDrawStyle WidgetStyle_Classic;

uiWidgetDrawStyle *widget_drawstyle_get(const int widget_style_type);

#endif  /* __WIDGETS_H__ */


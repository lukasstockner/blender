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

/** \file blender/windowmanager/widgets/wm_widget.h
 *  \ingroup wm
 */

#ifndef __WM_WIDGET_H__
#define __WM_WIDGET_H__

#include "BLI_compiler_attrs.h"

#define WIDGET_MAX_NAME (MAX_NAME + 4) /* + 4 for unique '.001', '.002', etc suffix */

/* widgets are set per region by registering them on widgetmaps */
class wmWidget
{
public:
	wmWidget *next, *prev;

	wmWidget(struct wmWidgetGroup *wgroup, const char *name, const int max_prop = 1);
	~wmWidget();
	void unregister(ListBase *widgetlist);

	void handle(struct bContext *C, const wmEvent *event, const int handle_flag);
	void tweak_cancel(struct bContext *C);

	void add_to_selection(wmWidgetMap *wmap, bContext *C);
	void remove_from_selection(wmWidgetMap *wmap, const bContext *C);
	void calculate_scale(const bContext *C);

	const char *idname_get();
	void        property_set(const int slot, PointerRNA *ptr_, const char *propname);
	PointerRNA *operator_set(const char *opname_);
	const char *operatorname_get();
	void        func_handler_set(int (*handler)(bContext *, const wmEvent *, wmWidget *, const int ));
	void        func_select_set(void (*select_)(bContext *, wmWidget *, const int ));
	void        origin_set(const float origin_[3]);
	void        offset_set(const float offset_[3]);
	void        flag_set(const int flag_, const bool enable);
	bool        flag_is_set(const int flag_);
	void        scale_set(const float scale_);
	void        line_width_set(const float line_width_);
	void        colors_set(const float col_[4], const float col_hi_[4]);

	friend bool widget_compare(const wmWidget *a, const wmWidget *b) ATTR_WARN_UNUSED_RESULT;

	/* pointer back to parent widget group */
	wmWidgetGroup *wgroup;

	/* determines 3d intersection by rendering the widget in a selection routine. */
	void (*render_3d_intersection)(const struct bContext *C, struct wmWidget *widget, int selectionbase);

	/* draw widget */
	void (*draw)(const struct bContext *C, struct wmWidget *widget);
	/* determine if the mouse intersects with the widget. The calculation should be done in the callback itself */
	int  (*intersect)(struct bContext *C, const struct wmEvent *event, struct wmWidget *widget);

	/* handler used by the widget. Usually handles interaction tied to a widget type */
	int (*handler)(bContext *, const wmEvent *, wmWidget *, const int );

	/* widget-specific handler to update widget attributes when a property is bound */
	void (*bind_to_prop)(struct wmWidget *widget, int slot);

	/* returns the final position which may be different from the origin, depending on the widget.
	 * used in calculations of scale */
	void (*get_final_position)(struct wmWidget *widget, float vec[3]);

	/* activate a widget state when the user clicks on it */
	int (*invoke)(struct bContext *C, const struct wmEvent *event, struct wmWidget *widget);

	/* called after canceling widget handling - used to reset property */
	void (*cancel)(struct bContext *C, struct wmWidget *widget);

	int (*get_cursor)(struct wmWidget *widget);

	/* called when widget selection state changes */
	void (*select)(struct bContext *C, struct wmWidget *widget, const int action);

	int flag; /* flags set by drawing and interaction, such as highlighting */

	unsigned char highlighted_part;

	/* center of widget in space, 2d or 3d */
	float origin[3];
	/* custom offset from origin */
	float offset[3];

	/* runtime property, set the scale while drawing on the viewport */
	float scale;

	/* user defined width for line drawing */
	float line_width;

	/* widget colors (uses default fallbacks if not defined) */
	float col[4], col_hi[4];

	/* data used during interaction */
	void *interaction_data;

	/* operator properties if widget spawns and controls an operator, or owner pointer if widget spawns and controls a property */
	PointerRNA opptr;

	/* arrays of properties attached to various widget parameters. As the widget is interacted with, those properties get updated */
	PointerRNA *ptr;
	PropertyRNA **props;

private:
	void unique_idname_set(const char *rawname);

	char idname[WIDGET_MAX_NAME];

	/* user defined scale, in addition to the original one */
	float user_scale;

	/* name of operator to spawn when activating the widget */
	const char *opname;

	/* maximum number of properties attached to the widget */
	int max_prop;
};

typedef struct WidgetDrawInfo {
	int nverts;
	int ntris;
	float (*verts)[3];
	float (*normals)[3];
	unsigned short *indices;
	bool init;
} WidgetDrawInfo;

void widget_find_active_3D_loop(const bContext *C, ListBase *visible_widgets);

void widget_draw(WidgetDrawInfo *info, const bool select);


void fix_linking_widget_arrow(void);
void fix_linking_widget_cage(void);
void fix_linking_widget_dial(void);
void fix_linking_widget_facemap(void);
void fix_linking_widget_plane(void);

#endif // __WM_WIDGET_H__

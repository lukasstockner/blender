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

/** \file blender/windowmanager/intern/widgets/wm_widget.cpp
 *  \ingroup wm
 */

#include "BKE_context.h"

#include "DNA_defs.h"
#include "DNA_listBase.h"
#include "DNA_windowmanager_types.h"

#include "MEM_guardedalloc.h"

#include "WM_api.h"

#include "wm_widgetgroup.h"
#include "wm_widget.h" // own include

#include "wm.h" // tmp

#if 0
wmWidget::wmWidget()
{
	
}
#endif

/**
 * Free widget data, not widget itself.
 */
static void widget_data_free(wmWidget *widget)
{
	if (widget->opptr.data) {
		WM_operator_properties_free(&widget->opptr);
	}

	MEM_freeN(widget->props);
	MEM_freeN(widget->ptr);
}

/**
 * Free and NULL \a widget.
 * \a widgetlist is allowed to be NULL.
 */
void widget_delete(ListBase *widgetlist, wmWidget *widget)
{
	widget_data_free(widget);
	if (widgetlist)
		BLI_remlink(widgetlist, widget);
	MEM_SAFE_FREE(widget);
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


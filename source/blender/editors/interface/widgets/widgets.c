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

#if 0 /* TODO */
static uiWidgetType *widget_type(uiWidgetTypeEnum type)
{
	
}
#endif

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

/** \file blender/editors/interface/widgets/widgets_draw/drawstyle_classic.c
 *  \ingroup edinterface
 */

#include "BLI_utildefines.h"

#include "DNA_userdef_types.h"


#include "widgets.h"
#include "widgets_draw_intern.h" /* own include */



static void widget_roundbut(uiWidgetColors *wcol, rcti *rect, int UNUSED(state), int roundboxalign)
{
	uiWidgetBase wtb;
	const float rad = 0.25f * U.widget_unit;

	widgetbase_init(&wtb);

	/* half rounded */
	round_box_edges(&wtb, roundboxalign, rect, rad);

	widgetbase_draw(&wtb, wcol);
}

uiWidgetDrawType drawtype_classic_exec = {
	/* state */  NULL,
	/* draw */   widget_roundbut,
	/* custom */ NULL,
	/* text */   NULL,
};

uiWidgetDrawStyle WidgetStyle_Classic = {
	/* widget_exec */ &drawtype_classic_exec,
};


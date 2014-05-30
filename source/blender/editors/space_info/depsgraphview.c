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
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_info/depsgraphview.c
 *  \ingroup spinfo
 */


#include <string.h>
#include <assert.h>

#include "MEM_guardedalloc.h"

#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_main.h"

#include "DEG_depsgraph_debug.h"

#include "UI_interface.h"

#include "depsgraphview.h"

static void draw_fields_header(uiLayout *layout)
{
	uiLayout *row = uiLayoutRow(layout, true);
	
	uiItemL(row, "Name", 0);
	uiItemL(row, "Last Duration [ms]", 0);
}

static void draw_fields_id(uiLayout *layout, ID *id, DepsgraphStatsID *id_stats)
{
	uiLayout *row = uiLayoutRow(layout, true);
	char num[256];
	
	uiItemL(row, id->name+2, 0);
	
	if (id_stats) {
		BLI_snprintf(num, sizeof(num), "%.3f", id_stats->times.duration_last);
		uiItemL(row, num, 0);
	}
	else {
		uiItemL(row, "-", 0);
	}
}

void depsgraphview_draw(const struct bContext *C, uiLayout *layout)
{
	Main *bmain = CTX_data_main(C);
	DepsgraphStats *stats = DEG_stats();
	uiLayout *col = uiLayoutColumn(layout, true);
	ID *id;
	
	if (!stats)
		return;
	
	draw_fields_header(col);
	
	for (id = bmain->object.first; id; id = id->next) {
		draw_fields_id(col, id, DEG_stats_id(id));
	}
}

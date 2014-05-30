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
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_node/info_toolbar.c
 *  \ingroup spinfo
 */


#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "DNA_listBase.h"

#include "BKE_context.h"
#include "BKE_screen.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "info_intern.h"  /* own include */


/* ******************* toolbar registration ************** */

void info_toolbar_register(ARegionType *UNUSED(art))
{
}

/* ********** operator to open/close toolshelf region */

static int info_toolbar_toggle_exec(bContext *C, wmOperator *UNUSED(op))
{
	ScrArea *sa = CTX_wm_area(C);
	ARegion *ar = info_has_tools_region(sa);
	
	if (ar)
		ED_region_toggle_hidden(C, ar);

	return OPERATOR_FINISHED;
}

/* non-standard poll operator which doesn't care if there are any nodes */
static int info_toolbar_poll(bContext *C)
{
	ScrArea *sa = CTX_wm_area(C);
	return (sa && (sa->spacetype == SPACE_INFO));
}

void INFO_OT_toolbar(wmOperatorType *ot)
{
	ot->name = "Tool Shelf";
	ot->description = "Toggles tool shelf display";
	ot->idname = "INFO_OT_toolbar";
	
	ot->exec = info_toolbar_toggle_exec;
	ot->poll = info_toolbar_poll;
	
	/* flags */
	ot->flag = 0;
}

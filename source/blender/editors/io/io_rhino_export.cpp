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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#include "opennurbs.h"
#include <cstdio>

extern "C" {
	#include "DNA_scene_types.h"
	#include "BLF_translation.h"
	#include "BKE_context.h"
	#include "BKE_global.h"
	#include "BKE_main.h"
	#include "BKE_report.h"
	#include "ED_screen.h"
	#include "ED_object.h"
	#include "RNA_access.h"
	#include "RNA_define.h"
	#include "UI_interface.h"
	#include "UI_resources.h"
	#include "WM_api.h"
	#include "WM_types.h"
	// BLI's lzma definitions don't play ball with opennurbs's zlib definitions
	// #include "BLI_blenlib.h"
	// #include "BLI_utildefines.h"
	bool BLI_replace_extension(char *path, size_t maxlen, const char *ext);

	#include "io_rhino_export.h"
}

static int rhino_export(bContext *C, wmOperator *op) {
	char filename[FILE_MAX];
	FILE *f;
	
	if (!RNA_struct_property_is_set(op->ptr, "filepath")) {
		BKE_report(op->reports, RPT_ERROR, "No filename given");
		return OPERATOR_CANCELLED;
	}
	RNA_string_get(op->ptr, "filepath", filename);

	f = ON::OpenFile(filename, "wb");
	ON::CloseFile(f);
	
	return OPERATOR_FINISHED; //OPERATOR_CANCELLED
}


/*--- Operator Registration ---*/

int wm_rhino_export_invoke(bContext *C, wmOperator *op, const struct wmEvent *evt)
{
	if (!RNA_struct_property_is_set(op->ptr, "filepath")) {
		char filepath[FILE_MAX];
		
		if (G.main->name[0] == 0)
			strncpy(filepath, "untitled", sizeof(filepath));
		else
			strncpy(filepath, G.main->name, sizeof(filepath));
		
		BLI_replace_extension(filepath, sizeof(filepath), ".3dm");
		RNA_string_set(op->ptr, "filepath", filepath);
	}
	
	WM_event_add_fileselect(C, op);
	
	return OPERATOR_RUNNING_MODAL;
}

void WM_OT_rhino_export(struct wmOperatorType *ot) {
	ot->name = "Export Rhino 3DM";
	ot->description = "Save a Rhino-compatible .3dm file";
	ot->idname = "WM_OT_rhino_export";

	ot->invoke = wm_rhino_export_invoke;
	ot->exec = rhino_export;
	ot->poll = WM_operator_winactive;
	
	ot->flag |= OPTYPE_PRESET;
	
	RNA_def_boolean(ot->srna, "selected", 0, "Selection Only",
	                "Export only selected elements");

	RNA_def_string(ot->srna, "filter_glob", "*.3dm", 16,
	                          "Glob Filter", "Rhino Extension Glob Filter");
	RNA_def_string(ot->srna, "filename_ext", ".3dm", 16,
	                          "Rhino File Extension", "Rhino File Extension");

	WM_operator_properties_filesel(ot, FOLDERFILE, FILE_BLENDER, FILE_SAVE,
	                               WM_FILESEL_FILEPATH, FILE_DEFAULTDISPLAY);
}

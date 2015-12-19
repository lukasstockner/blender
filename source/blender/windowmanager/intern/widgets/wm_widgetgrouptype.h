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

/** \file blender/windowmanager/intern/widgets/wm_widgetgrouptype.h
 *  \ingroup wm
 */

#ifndef __WM_WIDGETGROUPTYPE_H__
#define __WM_WIDGETGROUPTYPE_H__

#include "BLI_compiler_attrs.h"

#include "RNA_types.h"

struct wmKeyMap;
struct wmKeyConfig;
struct wmWidgetGroup;
struct wmWidgetMapType;


class wmWidgetGroupType
{
public:
	wmWidgetGroupType();
	wmWidgetGroupType *next, *prev;

	void init(
	        wmWidgetMapType *wmaptype, wmWidgetGroupType *wgrouptype,
	        int (*poll)(const bContext *, wmWidgetGroupType *),
	        void (*create)(const bContext *, wmWidgetGroup *),
	        wmKeyMap *(*keymap_init)(wmKeyConfig *, const char *),
	        const Main *bmain, const char *mapidname, const char *name,
	        const short spaceid, const short regionid, const bool is_3d);
	void unregister(bContext *C, Main *bmain);

	void keymap_init_do(wmKeyConfig *keyconf);
	void attach_to_handler(bContext *C, struct wmEventHandler *handler, struct wmOperator *op);
	size_t get_idname(char *r_idname);

private:
	char idname[64]; /* MAX_NAME */
	char name[64]; /* widget group name - displayed in UI (keymap editor) */

	/* poll if widgetmap should be active */
	int (*poll)(const bContext *, wmWidgetGroupType *) ATTR_WARN_UNUSED_RESULT;

	/* update widgets, called right before drawing */
	void (*create)(const bContext *, wmWidgetGroup *);

	/* keymap init callback for this widgetgroup */
	wmKeyMap *(*keymap_init)(wmKeyConfig *, const char *);

	/* keymap created with callback from above */
	wmKeyMap *keymap;

	/* rna for properties */
	StructRNA *srna;

	/* RNA integration */
	ExtensionRNA ext;

	/* general flag */
	int flag;

	/* if type is spawned from operator this is set here */
	void *op;

	/* same as widgetmaps, so registering/unregistering goes to the correct region */
	short spaceid, regionid;
	char mapidname[64];
	bool is_3d;
};

#endif // __WM_WIDGETGROUPTYPE_H__

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

/** \file blender/windowmanager/intern/widgets/wm_widgets_c_api.h
 *  \ingroup wm
 *
 * C-API for wmWidget types.
 */

#ifndef __WM_WIDGETS_C_API__
#define __WM_WIDGETS_C_API__

#ifdef __cplusplus
extern "C" {
#endif

struct Main;
struct bContext;
struct wmEventHandler;
struct wmWidgetGroup;
struct wmWidgetGroupType;
struct wmWidget;


void wm_widgets_keymap(struct wmKeyConfig *keyconfig);

struct wmWidgetMapType *WM_widgetmaptype_find(
        const char *idname, const int spaceid, const int regionid,
        const bool is_3d, const bool create);

struct wmWidgetMap *WM_widgetmap_from_type(
        const char *idname, const int spaceid, const int regionid,
        const bool is_3d);
/* highlighted widget */
void wm_widgetmap_set_highlighted_widget(
        struct bContext *C, struct wmWidgetMap *wmap, struct wmWidget *widget,
        unsigned char part);
struct wmWidget *wm_widgetmap_get_highlighted_widget(struct wmWidgetMap *wmap);
struct wmWidget *wm_widgetmap_find_highlighted_widget(
        struct wmWidgetMap *wmap, struct bContext *C, const struct wmEvent *event,
        unsigned char *part);
/* active widget */
void wm_widgetmap_set_active_widget(
        struct wmWidgetMap *wmap, struct bContext *C, const struct wmEvent *event, struct wmWidget *widget);
struct wmWidget *wm_widgetmap_get_active_widget(struct wmWidgetMap *wmap);

struct wmWidgetGroupType *WM_widgetgrouptype_new(
        int (*poll)(const struct bContext *, struct wmWidgetGroupType *),
        void (*create)(const struct bContext *, struct wmWidgetGroup *),
        struct wmKeyMap *(*keymap_init)(struct wmKeyConfig *, const char *),
        const struct Main *bmain, const char *mapidname, const char *name,
        const short spaceid, const short regionid, const bool is_3d);
void WM_widgetgrouptype_unregister(struct bContext *C, struct Main *bmain, struct wmWidgetGroupType *wgrouptype);
void WM_widgetgrouptype_attach_to_handler(
        struct bContext *C, struct wmWidgetGroupType *wgrouptype,
        struct wmEventHandler *handler, struct wmOperator *op);
size_t WM_widgetgrouptype_idname_get(struct wmWidgetGroupType *wgrouptype, char *r_idname);

void fix_linking_widgets(void);

#ifdef __cplusplus
}
#endif

#endif // __WM_WIDGETS_C_API__


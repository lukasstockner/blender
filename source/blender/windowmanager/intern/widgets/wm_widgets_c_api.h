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

#include "BLI_compiler_attrs.h"

struct ARegion;
struct Main;
struct bContext;
struct wmEventHandler;
struct wmWidgetGroup;
struct wmWidgetGroupType;
struct wmWidget;


struct wmWidgetMapType *WM_widgetmaptype_find(
        const char *idname, const int spaceid, const int regionid,
        const bool is_3d, const bool create);

struct wmWidgetMap *WM_widgetmap_new(const char *idname, const int spaceid, const int regionid, const bool is_3d);
void WM_widgetmap_remove(struct wmWidgetMap *wmap, struct ListBase *widgetmaps);
void WM_widgetmaps_remove(struct ListBase *widgetmaps);
void WM_widgetmap_widgets_update(struct wmWidgetMap *wmap, const struct bContext *C);
void WM_widgetmap_widgets_draw(const struct bContext *C, struct wmWidgetMap *wmap,
        const bool in_scene, const bool free_draw_widgets);
void wm_widgetmap_handler_context(struct bContext *C, struct wmEventHandler *handler);
void wm_widget_handler_modal_update(struct bContext *C, struct wmEvent *event, struct wmEventHandler *handler);
void WM_widgetmaps_create_region_handlers(struct ARegion *ar);
bool WM_widgetmap_cursor_set(struct wmWidgetMap *wmap, struct wmWindow *win);
struct GHash *wm_widgetmap_widget_hash_new(
        const struct bContext *C, struct wmWidgetMap *wmap,
        bool (*poll)(const struct wmWidget *, void *),
        void *data, const bool include_hidden) ATTR_WARN_UNUSED_RESULT;
/* highlighted widget */
void wm_widgetmap_highlighted_widget_set(
        struct bContext *C, struct wmWidgetMap *wmap, struct wmWidget *widget,
        unsigned char part);
struct wmWidget *wm_widgetmap_highlighted_widget_get(struct wmWidgetMap *wmap);
struct wmWidget *wm_widgetmap_highlighted_widget_find(
        struct wmWidgetMap *wmap, struct bContext *C, const struct wmEvent *event,
        unsigned char *part);
/* active widget */
void wm_widgetmap_active_widget_set(
        struct wmWidgetMap *wmap, struct bContext *C,
        const struct wmEvent *event, struct wmWidget *widget);
struct wmWidget *wm_widgetmap_active_widget_get(struct wmWidgetMap *wmap);
/* active group */
struct wmWidgetGroup *wm_widgetmap_active_group_get(struct wmWidgetMap *wmap);

void wm_widgets_keymap(struct wmKeyConfig *keyconf);
void WM_widgetmaptypes_free(void);

struct wmWidgetGroupType *WM_widgetgrouptype_new(
        int (*poll)(const struct bContext *, struct wmWidgetGroupType *),
        void (*create)(const struct bContext *, struct wmWidgetGroup *),
        struct wmKeyMap *(*keymap_init)(struct wmKeyConfig *, const char *),
        const struct Main *bmain, const char *mapidname, const char *name,
        const short spaceid, const short regionid, const bool is_3d);
void WM_widgetgrouptype_remove(struct bContext *C, struct Main *bmain, struct wmWidgetGroupType *wgrouptype);
void WM_widgetgrouptype_attach_to_handler(
        struct bContext *C, struct wmWidgetGroupType *wgrouptype,
        struct wmEventHandler *handler, struct wmOperator *op);
void        WM_widgetgrouptype_idname_set(struct wmWidgetGroupType *wgrouptype, const char *idname);
size_t      WM_widgetgrouptype_idname_get(struct wmWidgetGroupType *wgrouptype, char *r_idname);
wmOperator *WM_widgetgrouptype_operator_get(struct wmWidgetGroupType *wgrouptype);
wmKeyMap   *WM_widgetgrouptype_user_keymap_get(struct wmWidgetGroupType *wgrouptype);

void fix_linking_widgets(void);

#ifdef __cplusplus
}
#endif

#endif // __WM_WIDGETS_C_API__


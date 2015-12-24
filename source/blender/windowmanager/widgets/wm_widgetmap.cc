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

/** \file blender/windowmanager/widgets/wm_widgetmap.cpp
 *  \ingroup wm
 */

#include "BKE_context.h"

#include "BLI_utildefines.h"
#include "BLI_ghash.h"
#include "BLI_math.h"

#include "ED_screen.h"
#include "ED_view3d.h"

#include "GPU_glew.h"
#include "GPU_select.h"

#include "MEM_guardedalloc.h"

#include "WM_api.h"
#include "wm_cursors.h"
#include "wm_event_system.h"
#include "WM_types.h"
#include "wm.h" // tmp

#include "wm_widgetgroup.h"
#include "wm_widgetgrouptype.h"
#include "wm_widgetmaptype.h"
#include "wm_widget.h"
#include "wm_widgets_c_api.h"
#include "wm_widgetmap.h" // own include


/**
 * Hash table of all visible widgets to avoid unnecessary loops and wmWidgetGroupType->poll checks.
 * Collected in WM_widgets_update, freed in WM_widgets_draw.
 */
static GHash *draw_widgets = NULL;


wmWidgetMap::wmWidgetMap(const char *idname, const int spaceid, const int regionid, const bool is_3d)
    : widgetgroups(ListBase_NULL)
{
	type = WM_widgetmaptype_find(idname, spaceid, regionid, is_3d, true);

	/* create all widgetgroups for this widgetmap. We may create an empty one
	 * too in anticipation of widgets from operators etc */
	for (wmWidgetGroupType *wgrouptype = (wmWidgetGroupType *)type->widgetgrouptypes.first;
	     wgrouptype;
	     wgrouptype = wgrouptype->next)
	{
		wmWidgetGroup *wgroup = new wmWidgetGroup;
		wgroup->type = wgrouptype;
		wgroup->widgets = ListBase_NULL;
		BLI_addtail(&widgetgroups, wgroup);
	}
}

/**
 * \brief wmWidgetMap Destructor
 *
 * \warning As it currently is, this may only be called on exit/startup. If called on runtime,
 *          bContext * should be passed to #widgetgroup_remove so cursor can be reset.
 */
wmWidgetMap::~wmWidgetMap()
{
	wmWidgetGroup *wgroup = (wmWidgetGroup *)widgetgroups.first;

	while (wgroup) {
		wmWidgetGroup *wgroup_next = wgroup->next;

		/* bContext * can be NULL since this destructor is only called
		 * on exit and we don't need to change cursor state */
		widgetgroup_remove(NULL, this, wgroup);
		wgroup = wgroup_next;
	}

	/* XXX shouldn't widgets in wmap_context.selected_widgets be freed here? */
	MEM_SAFE_FREE(wmap_context.selected_widgets);
}

void wmWidgetMap::unregister(ListBase *widgetmaps)
{
	BLI_remlink(widgetmaps, this);
}

/**
 * \brief Remove all widgetmaps of ListBase \a widgetmaps.
 */
void WM_widgetmaps_remove(ListBase *widgetmaps)
{
	wmWidgetMap *wmap = (wmWidgetMap *)widgetmaps->first;

	while (wmap) {
		wmWidgetMap *wmap_next = wmap->next;
		WM_widgetmap_remove(wmap, widgetmaps);
		wmap = wmap_next;
	}

	BLI_assert(BLI_listbase_is_empty(widgetmaps));
}

static void widget_highlight_update(wmWidgetMap *wmap, const wmWidget *old_, wmWidget *new_)
{
	new_->flag |= WM_WIDGET_HIGHLIGHT;
	wmap->wmap_context.highlighted_widget = new_;
	new_->highlighted_part = old_->highlighted_part;
}

void wmWidgetMap::update(const bContext *C)
{
	wmWidget *widget = wmap_context.active_widget;

	if (!draw_widgets) {
		draw_widgets = BLI_ghash_str_new(__func__);
	}

	if (widget) {
		if ((widget->flag & WM_WIDGET_HIDDEN) == 0) {
			widget_calculate_scale(widget, C);
			BLI_ghash_reinsert(draw_widgets, widget->idname, widget, NULL, NULL);
		}
	}
	else if (!BLI_listbase_is_empty(&widgetgroups)) {
		wmWidget *highlighted = NULL;

		for (wmWidgetGroup *wgroup = (wmWidgetGroup *)widgetgroups.first; wgroup; wgroup = wgroup->next) {
			if (!wgroup->type->poll || wgroup->type->poll(C, wgroup->type)) {
				/* first delete and recreate the widgets */
				for (widget = (wmWidget *)wgroup->widgets.first; widget;) {
					wmWidget *widget_next = widget->next;

					/* do not delete selected and highlighted widgets,
					 * keep them to compare with new ones */
					if (widget->flag & WM_WIDGET_SELECTED) {
						BLI_remlink(&wgroup->widgets, widget);
						widget->next = widget->prev = NULL;
					}
					else if (widget->flag & WM_WIDGET_HIGHLIGHT) {
						highlighted = widget;
						BLI_remlink(&wgroup->widgets, widget);
						widget->next = widget->prev = NULL;
					}
					else {
						widget_remove(&wgroup->widgets, widget);
					}
					widget = widget_next;
				}

				if (wgroup->type->create) {
					wgroup->type->create(C, wgroup);
				}

				for (widget = (wmWidget *)wgroup->widgets.first; widget; widget = widget->next) {
					if (widget->flag & WM_WIDGET_HIDDEN)
						continue;

					widget_calculate_scale(widget, C);
					/* insert newly created widget into hash table */
					BLI_ghash_reinsert(draw_widgets, widget->idname, widget, NULL, NULL);
				}

				/* *** From now on, draw_widgets hash table can be used! *** */

			}
		}

		if (highlighted) {
			wmWidget *highlighted_new = (wmWidget *)BLI_ghash_lookup(draw_widgets, highlighted->idname);
			if (highlighted_new) {
				BLI_assert(widget_compare(highlighted, highlighted_new));
				widget_highlight_update(this, highlighted, highlighted_new);
				widget_remove(NULL, highlighted);
			}
			/* if we didn't find a highlighted widget, delete the old one here */
			else {
				MEM_SAFE_FREE(highlighted);
				wmap_context.highlighted_widget = NULL;
			}
		}

		if (wmap_context.selected_widgets) {
			for (int i = 0; i < wmap_context.tot_selected; i++) {
				wmWidget *sel_old = wmap_context.selected_widgets[i];
				wmWidget *sel_new = (wmWidget *)BLI_ghash_lookup(draw_widgets, sel_old->idname);

				/* fails if wgtype->poll state changed */
				if (!sel_new)
					continue;

				BLI_assert(widget_compare(sel_old, sel_new));

				/* widget was selected and highlighted */
				if (sel_old->flag & WM_WIDGET_HIGHLIGHT) {
					widget_highlight_update(this, sel_old, sel_new);
				}
				widget_data_free(sel_old);
				/* XXX freeing sel_old leads to crashes, hrmpf */

				sel_new->flag |= WM_WIDGET_SELECTED;
				wmap_context.selected_widgets[i] = sel_new;
			}
		}
	}
}

/**
 * Draw all visible widgets in \a wmap.
 * Uses global draw_widgets hash table.
 *
 * \param in_scene  draw depth-culled widgets (wmWidget->flag WM_WIDGET_SCENE_DEPTH) - TODO
 * \param free_drawwidgets  free global draw_widgets hash table (always enable for last draw call in region!).
 */
void wmWidgetMap::draw(const bContext *C, const bool in_scene, const bool free_draw_widgets)
{
	const bool draw_multisample = (U.ogl_multisamples != USER_MULTISAMPLE_NONE);
	const bool use_lighting = (U.tw_flag & V3D_SHADED_WIDGETS) != 0;

	/* enable multisampling */
	if (draw_multisample) {
		glEnable(GL_MULTISAMPLE);
	}

	if (use_lighting) {
		const float lightpos[4] = {0.0, 0.0, 1.0, 0.0};
		const float diffuse[4] = {1.0, 1.0, 1.0, 0.0};

		glPushAttrib(GL_LIGHTING_BIT | GL_ENABLE_BIT);

		glEnable(GL_LIGHTING);
		glEnable(GL_LIGHT0);
		glEnable(GL_COLOR_MATERIAL);
		glColorMaterial(GL_FRONT_AND_BACK, GL_DIFFUSE);
		glPushMatrix();
		glLoadIdentity();
		glLightfv(GL_LIGHT0, GL_POSITION, lightpos);
		glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
		glPopMatrix();
	}


	wmWidget *widget = wmap_context.active_widget;

	if (widget && in_scene == (widget->flag & WM_WIDGET_SCENE_DEPTH)) {
		if (widget->flag & WM_WIDGET_DRAW_ACTIVE) {
			/* notice that we don't update the widgetgroup, widget is now on
			 * its own, it should have all relevant data to update itself */
			widget->draw(C, widget);
		}
	}
	else if (!BLI_listbase_is_empty(&widgetgroups)) {
		GHashIterator gh_iter;

		GHASH_ITER (gh_iter, draw_widgets) { /* draw_widgets excludes hidden widgets */
			widget = (wmWidget*)BLI_ghashIterator_getValue(&gh_iter);
			if ((in_scene == (widget->flag & WM_WIDGET_SCENE_DEPTH)) &&
			    ((widget->flag & WM_WIDGET_SELECTED) == 0) && /* selected are drawn later */
			    ((widget->flag & WM_WIDGET_DRAW_HOVER) == 0 || (widget->flag & WM_WIDGET_HIGHLIGHT)))
			{
				widget->draw(C, widget);
			}
		}
	}

	/* draw selected widgets last */
	if (wmap_context.selected_widgets) {
		for (int i = 0; i < wmap_context.tot_selected; i++) {
			widget = (wmWidget*)BLI_ghash_lookup(draw_widgets, wmap_context.selected_widgets[i]->idname);
			if (widget && (in_scene == (widget->flag & WM_WIDGET_SCENE_DEPTH))) {
				/* notice that we don't update the widgetgroup, widget is now on
				 * its own, it should have all relevant data to update itself */
				widget->draw(C, widget);
			}
		}
	}

	if (draw_multisample)
		glDisable(GL_MULTISAMPLE);
	if (use_lighting)
		glPopAttrib();

	if (free_draw_widgets && draw_widgets) {
		BLI_ghash_free(draw_widgets, NULL, NULL);
		draw_widgets = NULL;
	}
}

void wm_widgetmap_handler_context(bContext *C, wmEventHandler *handler)
{
	bScreen *screen = CTX_wm_screen(C);

	if (screen) {
		if (handler->op_area == NULL) {
			/* do nothing in this context */
		}
		else {
			ScrArea *sa;

			for (sa = (ScrArea *)screen->areabase.first; sa; sa = sa->next)
				if (sa == handler->op_area)
					break;
			if (sa == NULL) {
				/* when changing screen layouts with running modal handlers (like render display), this
				 * is not an error to print */
				if (handler->widgetmap == NULL)
					printf("internal error: modal widgetmap handler has invalid area\n");
			}
			else {
				ARegion *ar;
				CTX_wm_area_set(C, sa);
				for (ar = (ARegion *)sa->regionbase.first; ar; ar = ar->next)
					if (ar == handler->op_region)
						break;
				/* XXX no warning print here, after full-area and back regions are remade */
				if (ar)
					CTX_wm_region_set(C, ar);
			}
		}
	}
}

/* Doesn't really fit in here, but in this case it's okay since this function will likely be replaced anyway */
void wm_widget_handler_modal_update(bContext *C, wmEvent *event, wmEventHandler *handler)
{
	/* happens on render */
	if (!handler->op_region)
		return;

	for (wmWidgetMap *wmap = (wmWidgetMap *)handler->op_region->widgetmaps.first; wmap; wmap = wmap->next) {
		wmWidget *widget = wm_widgetmap_active_widget_get(wmap);
		ScrArea *area = CTX_wm_area(C);
		ARegion *region = CTX_wm_region(C);

		if (!widget)
			continue;

		wm_widgetmap_handler_context(C, handler);

		/* regular update for running operator */
		if (handler->op) {
			if (widget && widget->handler && widget->opname && STREQ(widget->opname, handler->op->idname)) {
				widget->handler(C, event, widget, 0);
			}
		}
		/* operator not running anymore */
		else {
			wm_widgetmap_active_widget_set(wmap, C, event, NULL);
		}

		/* restore the area */
		CTX_wm_area_set(C, area);
		CTX_wm_region_set(C, region);
	}
}

void WM_widgetmaps_create_region_handlers(ARegion *ar)
{
	for (wmWidgetMap *wmap = (wmWidgetMap *)ar->widgetmaps.first; wmap; wmap = wmap->next) {
		wmEventHandler *handler = (wmEventHandler *)MEM_callocN(sizeof(wmEventHandler), "widget handler");

		handler->widgetmap = wmap;
		BLI_addtail(&ar->handlers, handler);
	}
}

bool wmWidgetMap::cursor_update(wmWindow *win)
{
	for (wmWidgetMap *wmap = this; wmap; wmap = next) {
		wmWidget *widget = wmap->wmap_context.highlighted_widget;
		if (widget && widget->get_cursor) {
			WM_cursor_set(win, widget->get_cursor(widget));
			return true;
		}
	}

	return false;
}

/**
 * Creates and returns idname hash table for (visible) widgets in \a wmap
 *
 * \param poll  Polling function for excluding widgets.
 * \param data  Custom data passed to \a poll
 */
GHash *wmWidgetMap::widget_hash_new(
        const bContext *C,
        bool (*poll)(const wmWidget *, void *),
        void *data,
        const bool include_hidden)
{
	GHash *hash = BLI_ghash_str_new(__func__);

	/* collect widgets */
	for (wmWidgetGroup *wgroup = (wmWidgetGroup *)widgetgroups.first; wgroup; wgroup = wgroup->next) {
		if (!wgroup->type->poll || wgroup->type->poll(C, wgroup->type)) {
			for (wmWidget *widget = (wmWidget *)wgroup->widgets.first; widget; widget = widget->next) {
				if ((include_hidden || (widget->flag & WM_WIDGET_HIDDEN) == 0) &&
				    (!poll || poll(widget, data)))
				{
					BLI_ghash_insert(hash, widget->idname, widget);
				}
			}
		}
	}

	return hash;
}

static int widgetmap_find_highlighted_widget_3d_intern(
        ListBase *visible_widgets, const bContext *C, const wmEvent *event,
        const float hotspot)
{
	ScrArea *sa = CTX_wm_area(C);
	ARegion *ar = CTX_wm_region(C);
	View3D *v3d = (View3D *)sa->spacedata.first;
	RegionView3D *rv3d = (RegionView3D *)ar->regiondata;
	rctf rect, selrect;
	GLuint buffer[64];      // max 4 items per select, so large enuf
	short hits;
	const bool do_passes = GPU_select_query_check_active();


	rect.xmin = event->mval[0] - hotspot;
	rect.xmax = event->mval[0] + hotspot;
	rect.ymin = event->mval[1] - hotspot;
	rect.ymax = event->mval[1] + hotspot;

	selrect = rect;

	ED_view3d_winmatrix_set(ar, v3d, &rect);
	mul_m4_m4m4(rv3d->persmat, rv3d->winmat, rv3d->viewmat);

	if (do_passes)
		GPU_select_begin(buffer, 64, &selrect, GPU_SELECT_NEAREST_FIRST_PASS, 0);
	else
		GPU_select_begin(buffer, 64, &selrect, GPU_SELECT_ALL, 0);
	/* do the drawing */
	widget_find_active_3D_loop(C, visible_widgets);

	hits = GPU_select_end();

	if (do_passes) {
		GPU_select_begin(buffer, 64, &selrect, GPU_SELECT_NEAREST_SECOND_PASS, hits);
		widget_find_active_3D_loop(C, visible_widgets);
		GPU_select_end();
	}

	ED_view3d_winmatrix_set(ar, v3d, NULL);
	mul_m4_m4m4(rv3d->persmat, rv3d->winmat, rv3d->viewmat);

	if (hits == 1) {
		return buffer[3];
	}
	/* find the widget the value belongs to */
	else if (hits > 1) {
		GLuint val, dep, mindep = 0, minval = -1;
		int a;

		/* we compare the hits in buffer, but value centers highest */
		/* we also store the rotation hits separate (because of arcs) and return hits on other widgets if there are */

		for (a = 0; a < hits; a++) {
			dep = buffer[4 * a + 1];
			val = buffer[4 * a + 3];

			if (minval == -1 || dep < mindep) {
				mindep = dep;
				minval = val;
			}
		}

		return minval;
	}

	return -1;
}

static void widgetmap_prepare_visible_widgets_3d(wmWidgetMap *wmap, ListBase *visible_widgets, bContext *C)
{
	wmWidget *widget;

	for (wmWidgetGroup *wgroup = (wmWidgetGroup *)wmap->widgetgroups.first; wgroup; wgroup = wgroup->next) {
		if (!wgroup->type->poll || wgroup->type->poll(C, wgroup->type)) {
			for (widget = (wmWidget *)wgroup->widgets.first; widget; widget = widget->next) {
				if (widget->render_3d_intersection && (widget->flag & WM_WIDGET_HIDDEN) == 0) {
					BLI_addhead(visible_widgets, BLI_genericNodeN(widget));
				}
			}
		}
	}
}

static wmWidget *widgetmap_find_highlighted_widget_3d(
        bContext *C, wmWidgetMap *wmap, const wmEvent *event,
        unsigned char *part)
{
	wmWidget *result = NULL;
	ListBase visible_widgets = {0};
	const float hotspot = 14.0f;
	int ret;

	widgetmap_prepare_visible_widgets_3d(wmap, &visible_widgets, C);

	*part = 0;
	/* set up view matrices */
	view3d_operator_needs_opengl(C);

	ret = widgetmap_find_highlighted_widget_3d_intern(&visible_widgets, C, event, 0.5f * hotspot);

	if (ret != -1) {
		LinkData *link;
		int retsec;
		retsec = widgetmap_find_highlighted_widget_3d_intern(&visible_widgets, C, event, 0.2f * hotspot);

		if (retsec != -1)
			ret = retsec;

		link = (LinkData *)BLI_findlink(&visible_widgets, ret >> 8);
		*part = ret & 255;
		result = (wmWidget *)link->data;
	}

	BLI_freelistN(&visible_widgets);

	return result;
}

static wmWidget *widgetmap_find_highlighted_widget(
        bContext *C, wmWidgetMap *wmap, const wmEvent *event,
        unsigned char *part)
{
	wmWidget *widget;

	for (wmWidgetGroup *wgroup = (wmWidgetGroup *)wmap->widgetgroups.first; wgroup; wgroup = wgroup->next) {
		if (!wgroup->type->poll || wgroup->type->poll(C, wgroup->type)) {
			for (widget = (wmWidget *)wgroup->widgets.first; widget; widget = widget->next) {
				if (widget->intersect) {
					if ((*part = widget->intersect(C, event, widget)))
						return widget;
				}
			}
		}
	}

	return NULL;
}

wmWidget *wmWidgetMap::find_highlighted_widget(bContext *C, const wmEvent *event, unsigned char *part)
{
	if (type->is_3d) {
		return widgetmap_find_highlighted_widget_3d(C, this, event, part);
	}
	else {
		return widgetmap_find_highlighted_widget(C, this, event, part);
	}
}

void wmWidgetMap::set_highlighted_widget(bContext *C, wmWidget *widget, unsigned char part)
{
	if ((widget != wmap_context.highlighted_widget) || (widget && part != widget->highlighted_part)) {
		if (wmap_context.highlighted_widget) {
			wmap_context.highlighted_widget->flag &= ~WM_WIDGET_HIGHLIGHT;
			wmap_context.highlighted_widget->highlighted_part = 0;
		}

		wmap_context.highlighted_widget = widget;

		if (widget) {
			widget->flag |= WM_WIDGET_HIGHLIGHT;
			widget->highlighted_part = part;
			wmap_context.activegroup = widget->wgroup;

			if (C && widget->get_cursor) {
				wmWindow *win = CTX_wm_window(C);
				WM_cursor_set(win, widget->get_cursor(widget));
			}
		}
		else {
			wmap_context.activegroup = NULL;
			if (C) {
				wmWindow *win = CTX_wm_window(C);
				WM_cursor_set(win, CURSOR_STD);
			}
		}

		/* tag the region for redraw */
		if (C) {
			ARegion *ar = CTX_wm_region(C);
			ED_region_tag_redraw(ar);
		}
	}
}

void wmWidgetMap::set_active_widget(bContext *C, const wmEvent *event, wmWidget *widget)
{
	if (widget) {
		if (widget->opname) {
			wmOperatorType *ot = WM_operatortype_find(widget->opname, 0);

			if (ot) {
				/* first activate the widget itself */
				if (widget->invoke && widget->handler) {
					widget->flag |= WM_WIDGET_ACTIVE;
					widget->invoke(C, event, widget);
				}
				wmap_context.active_widget = widget;

				WM_operator_name_call_ptr(C, ot, WM_OP_INVOKE_DEFAULT, &widget->opptr);

				/* we failed to hook the widget to the operator handler or operator was cancelled, return */
				if (!wmap_context.active_widget) {
					widget->flag &= ~WM_WIDGET_ACTIVE;
					/* first activate the widget itself */
					if (widget->interaction_data) {
						MEM_freeN(widget->interaction_data);
						widget->interaction_data = NULL;
					}
				}
				return;
			}
			else {
				printf("Widget error: operator not found\n");
				wmap_context.active_widget = NULL;
				return;
			}
		}
		else {
			if (widget->invoke && widget->handler) {
				widget->flag |= WM_WIDGET_ACTIVE;
				widget->invoke(C, event, widget);
				wmap_context.active_widget = widget;
			}
		}
	}
	else {
		widget = wmap_context.active_widget;

		/* deactivate, widget but first take care of some stuff */
		if (widget) {
			widget->flag &= ~WM_WIDGET_ACTIVE;
			/* first activate the widget itself */
			if (widget->interaction_data) {
				MEM_freeN(widget->interaction_data);
				widget->interaction_data = NULL;
			}
		}
		wmap_context.active_widget = NULL;

		ED_region_tag_redraw(CTX_wm_region(C));
		WM_event_add_mousemove(C);
	}
}

BLI_INLINE bool widget_selectable_poll(const wmWidget *widget, void *UNUSED(data))
{
	return (widget->flag & WM_WIDGET_SELECTABLE);
}

/**
 * Select all selectable widgets in \a wmap.
 * \return if selection has changed.
 */
bool wmWidgetMap::select_all_intern(bContext *C, wmWidget ***sel, const int action)
{
	/* GHash is used here to avoid having to loop over all widgets twice (once to
	 * get tot_sel for allocating, once for actually selecting). Instead we collect
	 * selectable widgets in hash table and use this to get tot_sel and do selection */

	GHash *hash = widget_hash_new(C, widget_selectable_poll, NULL, true);
	GHashIterator gh_iter;
	int i, *tot_sel = &wmap_context.tot_selected;
	bool changed = false;

	*tot_sel = BLI_ghash_size(hash);
	*sel = (wmWidget **)MEM_reallocN(*sel, sizeof(**sel) * (*tot_sel));

	GHASH_ITER_INDEX (gh_iter, hash, i) {
		wmWidget *widget_iter = (wmWidget *)BLI_ghashIterator_getValue(&gh_iter);

		if ((widget_iter->flag & WM_WIDGET_SELECTED) == 0) {
			changed = true;
		}
		widget_iter->flag |= WM_WIDGET_SELECTED;
		if (widget_iter->select) {
			widget_iter->select(C, widget_iter, action);
		}
		(*sel)[i] = widget_iter;
		BLI_assert(i < (*tot_sel));
	}
	/* highlight first widget */
	set_highlighted_widget(C, (*sel)[0], (*sel)[0]->highlighted_part);

	BLI_ghash_free(hash, NULL, NULL);
	return changed;
}

/**
 * Deselect all selected widgets in \a wmap.
 * \return if selection has changed.
 */
bool wmWidgetMap::deselect_all(wmWidget ***sel)
{
	if (*sel == NULL || wmap_context.tot_selected == 0)
		return false;

	for (int i = 0; i < wmap_context.tot_selected; i++) {
		(*sel)[i]->flag &= ~WM_WIDGET_SELECTED;
		(*sel)[i] = NULL;
	}
	MEM_SAFE_FREE(*sel);
	wmap_context.tot_selected = 0;

	/* always return true, we already checked
	 * if there's anything to deselect */
	return true;
}

/**
 * Select/Deselect all selectable widgets in \a wmap.
 * \return if selection has changed.
 *
 * TODO select all by type
 */
bool wmWidgetMap::select_all(bContext *C, const int action)
{
	wmWidget ***sel = &wmap_context.selected_widgets;
	bool changed = false;

	switch (action) {
		case SEL_SELECT:
			changed = select_all_intern(C, sel, action);
			break;
		case SEL_DESELECT:
			changed = deselect_all(sel);
			break;
		default:
			BLI_assert(0);
	}

	if (changed)
		WM_event_add_mousemove(C);

	return changed;
}

wmWidgetGroup *wmWidgetMap::get_active_group()
{
	return wmap_context.activegroup;
}


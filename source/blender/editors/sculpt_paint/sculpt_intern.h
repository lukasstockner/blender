/*
 * $Id$
 *
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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2006 by Nicholas Bishop
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */ 

#ifndef BDR_SCULPTMODE_H
#define BDR_SCULPTMODE_H

#include "DNA_listBase.h"
#include "DNA_vec_types.h"

struct bContext;
struct KeyBlock;
struct Object;
struct Scene;
struct PBVHNode;
struct SculptUndoNode;

int sculpt_poll(struct bContext *C);
void sculpt_update_mesh_elements(struct Scene *scene, struct Object *ob, int need_fmap);

/* Undo */
void sculpt_undo_push_begin(struct SculptSession *ss, char *name);
void sculpt_undo_push_end(struct SculptSession *ss);
struct SculptUndoNode *sculpt_undo_push_node(struct SculptSession *ss, struct PBVHNode *node);

#endif

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
 * Original Author: Lukas Toenne
 * Contributor(s): None Yet
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 * Public API for Querying and Filtering Depsgraph
 */

#ifndef __DEG_DEPSGRAPH_DEBUG_H__
#define __DEG_DEPSGRAPH_DEBUG_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "DNA_listBase.h"

struct DepsgraphSettings;
struct GHash;
struct ID;

struct Depsgraph;
struct DepsNode;
struct DepsRelation;

/* ************************************************ */
/* Statistics */

typedef struct DepsgraphStatsTimes {
	float duration_last;
} DepsgraphStatsTimes;

typedef struct DepsgraphStatsComponent {
	struct DepsgraphStatsComponent *next, *prev;
	
	char name[64];
	DepsgraphStatsTimes times;
} DepsgraphStatsComponent;

typedef struct DepsgraphStatsID {
	struct ID *id;
	
	DepsgraphStatsTimes times;
	ListBase components;
} DepsgraphStatsID;

typedef struct DepsgraphStats {
	struct GHash *id_stats;
} DepsgraphStats;

struct DepsgraphStats *DEG_stats(void);

void DEG_stats_verify(struct DepsgraphSettings *settings);

/* ************************************************ */
/* Graphviz Debugging */

void DEG_debug_graphviz(const struct Depsgraph *graph, FILE *stream, const char *label, bool show_eval);

typedef void (*DEG_DebugEvalCb)(void *userdata, const char *message);

void DEG_debug_eval_init(void *userdata, DEG_DebugEvalCb cb);
void DEG_debug_eval_end(void);

/* ************************************************ */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // __DEG_DEPSGRAPH_DEBUG_H__

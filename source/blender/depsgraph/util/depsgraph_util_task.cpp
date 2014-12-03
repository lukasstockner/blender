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
 * The Original Code is Copyright (C) 2013 Blender Foundation.
 * All rights reserved.
 *
 * Original Author: Brecht van Lommel
 * Contributor(s): Lukas Toenne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdlib.h>
#include "BLI_utildefines.h"
#include "BLI_compiler_attrs.h"
#include "PIL_time.h"

extern "C" {
#include "BLI_rand.h" /* XXX for eval simulation only, remove eventually */
#include "BLI_task.h"
}

#include "depsgraph_debug.h"
#include "depsgraph_eval.h"

#include "depsgraph_util_task.h"

/* Task */

/* **** eval simulation **** */
static RNG *deg_eval_sim_rng = NULL;

static void deg_eval_sim_init()
{
	deg_eval_sim_rng = BLI_rng_new((unsigned int)(PIL_check_seconds_timer() * 0x7FFFFFFF));
}

static void deg_eval_sim_free()
{
	BLI_rng_free(deg_eval_sim_rng);
	deg_eval_sim_rng = NULL;
}
/* ******** */

void DEG_task_run_func(TaskPool *pool, void *taskdata, int UNUSED(threadid))
{
	DepsgraphEvalState *state = (DepsgraphEvalState *)BLI_task_pool_userdata(pool);
	OperationDepsNode *node = (OperationDepsNode *)taskdata;
	if (node->is_noop()) {
		deg_schedule_children(pool, state->eval_ctx, state->graph, node);
		return;
	}

	/* get context */
	// TODO: who initialises this? "Init" operations aren't able to initialise it!!!
	ComponentDepsNode *comp = node->owner;
	BLI_assert(comp != NULL);

	/* take note of current time */
	double start_time = PIL_check_seconds_timer();
	DepsgraphDebug::task_started(node);

	/* should only be the case for NOOPs, which never get to this point */
	BLI_assert(node->evaluate);

	/* perform operation */
	node->evaluate(state->eval_ctx);

	/* note how long this took */
	double end_time = PIL_check_seconds_timer();
	DepsgraphDebug::task_completed(node, end_time - start_time);

	deg_schedule_children(pool, state->eval_ctx, state->graph, node);
}

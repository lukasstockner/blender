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
 * Original Author: Joshua Leung
 * Contributor(s): None Yet
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 * Evaluation engine entrypoints for Depsgraph Engine
 */

#include "MEM_guardedalloc.h"

#include "PIL_time.h"

extern "C" {
#include "BLI_task.h"
#include "BLI_utildefines.h"

#include "DNA_ID.h"

#include "BKE_depsgraph.h"
#include "BKE_global.h" /* XXX only for debug value, remove eventually */
#include "BKE_main.h"

} /* extern "C" */

#include "atomic_ops.h"

#include "depsgraph.h"
#include "depsnode.h"
#include "depsnode_component.h"
#include "depsnode_operation.h"
#include "depsgraph_types.h"
#include "depsgraph_eval.h"
#include "depsgraph_intern.h"
#include "depsgraph_debug.h"

eDEG_EvalMode DEG_get_eval_mode(void)
{
#ifdef WITH_LEGACY_DEPSGRAPH
	switch (G.debug_value) {
		case DEG_EVAL_MODE_NEW: return DEG_EVAL_MODE_NEW;
		default: return DEG_EVAL_MODE_OLD;
	}
#else
	BLI_assert(!"Should not be used with new depsgraph");
	return DEG_EVAL_MODE_NEW;
#endif
}

void DEG_set_eval_mode(eDEG_EvalMode mode)
{
#ifdef WITH_LEGACY_DEPSGRAPH
	switch (mode) {
		case DEG_EVAL_MODE_NEW: G.debug_value = DEG_EVAL_MODE_NEW;
		default: G.debug_value = DEG_EVAL_MODE_OLD;
	}
#else
	BLI_assert(!"Should not be used with new depsgraph");
#endif
}

/* ****************** */
/* Evaluation Context */

/* Create new evaluation context. */
EvaluationContext *DEG_evaluation_context_new(int mode)
{
	EvaluationContext *eval_ctx =
		(EvaluationContext *)MEM_callocN(sizeof(EvaluationContext),
		                                 "EvaluationContext");
	eval_ctx->mode = mode;
	return eval_ctx;
}

/* Initialize evaluation context.
 * Used by the areas which currently overrides the context or doesn't have
 * access to a proper one.
 */
void DEG_evaluation_context_init(EvaluationContext *eval_ctx, int mode)
{
	eval_ctx->mode = mode;
}

/* Free evaluation context. */
void DEG_evaluation_context_free(EvaluationContext *eval_ctx)
{
	MEM_freeN(eval_ctx);
}

/* ********************** */
/* Evaluation Entrypoints */

/* Forward declarations. */
static void schedule_children(TaskPool *pool,
                              EvaluationContext *eval_ctx,
                              Depsgraph *graph,
                              OperationDepsNode *node,
                              const int layers);

struct DepsgraphEvalState {
	EvaluationContext *eval_ctx;
	Depsgraph *graph;
	int layers;
};

static void deg_task_run_func(TaskPool *pool,
                              void *taskdata,
                              int UNUSED(threadid))
{
	DepsgraphEvalState *state = (DepsgraphEvalState *)BLI_task_pool_userdata(pool);
	OperationDepsNode *node = (OperationDepsNode *)taskdata;

	if (!node->is_noop()) {
		/* Get context. */
		// TODO: who initialises this? "Init" operations aren't able to initialise it!!!
		/* TODO(sergey): Wedon't use component contexts at this moment. */
		/* ComponentDepsNode *comp = node->owner; */
		BLI_assert(node->owner != NULL);

		/* Take note of current time. */
		double start_time = PIL_check_seconds_timer();
		DepsgraphDebug::task_started(state->graph, node);

		/* Should only be the case for NOOPs, which never get to this point. */
		BLI_assert(node->evaluate != NULL);

		/* Perform operation. */
		node->evaluate(state->eval_ctx);

		/* Note how long this took. */
		double end_time = PIL_check_seconds_timer();
		DepsgraphDebug::task_completed(state->graph,
		                               node,
		                               end_time - start_time);
	}

	schedule_children(pool, state->eval_ctx, state->graph, node, state->layers);
}

static void calculate_pending_parents(Depsgraph *graph)
{
	for (Depsgraph::OperationNodes::const_iterator it_op = graph->operations.begin();
	     it_op != graph->operations.end();
	     ++it_op)
	{
		OperationDepsNode *node = *it_op;

		node->num_links_pending = 0;
		node->scheduled = false;

		/* count number of inputs that need updates */
		if (node->flag & DEPSOP_FLAG_NEEDS_UPDATE) {
			for (OperationDepsNode::Relations::const_iterator it_rel = node->inlinks.begin();
			     it_rel != node->inlinks.end();
			     ++it_rel)
			{
				DepsRelation *rel = *it_rel;
				if (rel->from->type == DEPSNODE_TYPE_OPERATION) {
					OperationDepsNode *from = (OperationDepsNode *)rel->from;
					if (from->flag & DEPSOP_FLAG_NEEDS_UPDATE) {
						++node->num_links_pending;
					}
				}
			}
		}
	}
}

static void calculate_eval_priority(OperationDepsNode *node)
{
	if (node->done) {
		return;
	}
	node->done = 1;

	if (node->flag & DEPSOP_FLAG_NEEDS_UPDATE) {
		/* XXX standard cost of a node, could be estimated somewhat later on */
		const float cost = 1.0f;
		/* NOOP nodes have no cost */
		node->eval_priority = node->is_noop() ? cost : 0.0f;

		for (OperationDepsNode::Relations::const_iterator it = node->outlinks.begin();
		     it != node->outlinks.end();
		     ++it)
		{
			DepsRelation *rel = *it;
			OperationDepsNode *to = (OperationDepsNode *)rel->to;
			BLI_assert(to->type == DEPSNODE_TYPE_OPERATION);
			calculate_eval_priority(to);
			node->eval_priority += to->eval_priority;
		}
	}
	else {
		node->eval_priority = 0.0f;
	}
}

static void schedule_graph(TaskPool *pool,
                           EvaluationContext *eval_ctx,
                           Depsgraph *graph,
                           const int layers)
{
	BLI_spin_lock(&graph->lock);
	for (Depsgraph::OperationNodes::const_iterator it = graph->operations.begin();
	     it != graph->operations.end();
	     ++it)
	{
		OperationDepsNode *node = *it;
		IDDepsNode *id_node = node->owner->owner;
		if ((node->flag & DEPSOP_FLAG_NEEDS_UPDATE) &&
		    node->num_links_pending == 0 &&
		    (id_node->layers & layers) != 0)
		{
			BLI_task_pool_push(pool, deg_task_run_func, node, false, TASK_PRIORITY_LOW);
			node->scheduled = true;
		}
	}
	BLI_spin_unlock(&graph->lock);
}

static void schedule_children(TaskPool *pool,
                              EvaluationContext *eval_ctx,
                              Depsgraph *graph,
                              OperationDepsNode *node,
                              const int layers)
{
	for (OperationDepsNode::Relations::const_iterator it = node->outlinks.begin();
	     it != node->outlinks.end();
	     ++it)
	{
		DepsRelation *rel = *it;
		OperationDepsNode *child = (OperationDepsNode *)rel->to;
		BLI_assert(child->type == DEPSNODE_TYPE_OPERATION);

		if (child->flag & DEPSOP_FLAG_NEEDS_UPDATE) {
			IDDepsNode *id_child = child->owner->owner;

			BLI_assert(child->num_links_pending > 0);
			atomic_sub_uint32(&child->num_links_pending, 1);

			if (child->num_links_pending == 0 &&
			    (id_child->layers & layers) != 0)
			{
				BLI_spin_lock(&graph->lock);
				bool need_schedule = !child->scheduled;
				child->scheduled = true;
				BLI_spin_unlock(&graph->lock);

				if (need_schedule) {
					BLI_task_pool_push(pool, deg_task_run_func, child, false, TASK_PRIORITY_LOW);
				}
			}
		}
	}
}

/* Evaluate all nodes tagged for updating,
 * ! This is usually done as part of main loop, but may also be
 *   called from frame-change update.
 */
void DEG_evaluate_on_refresh_ex(EvaluationContext *eval_ctx,
                                Main *bmain,
                                Depsgraph *graph,
                                const int layers)
{
	/* Generate base evaluation context, upon which all the others are derived. */
	// TODO: this needs both main and scene access...

	/* XXX could use a separate pool for each eval context */
	DepsgraphEvalState state;
	state.eval_ctx = eval_ctx;
	state.graph = graph;
	state.layers = layers;

	TaskScheduler *task_scheduler = BLI_task_scheduler_get();
	TaskPool *task_pool = BLI_task_pool_create(task_scheduler, &state);

	/* Recursively push updates out to all nodes dependent on this,
	 * until all affected are tagged and/or scheduled up for eval
	 */
	DEG_graph_flush_updates(bmain, eval_ctx, graph, layers);

	calculate_pending_parents(graph);

	/* Clear tags. */
	for (Depsgraph::OperationNodes::const_iterator it = graph->operations.begin();
	     it != graph->operations.end();
	     ++it)
	{
		OperationDepsNode *node = *it;
		node->done = 0;
	}

	/* Calculate priority for operation nodes. */
	for (Depsgraph::OperationNodes::const_iterator it = graph->operations.begin();
	     it != graph->operations.end();
	     ++it)
	{
		OperationDepsNode *node = *it;
		calculate_eval_priority(node);
	}

	DepsgraphDebug::eval_begin(eval_ctx);

	schedule_graph(task_pool, eval_ctx, graph, layers);

	BLI_task_pool_work_and_wait(task_pool);
	BLI_task_pool_free(task_pool);

	DepsgraphDebug::eval_end(eval_ctx);

	/* Clear any uncleared tags - just in case. */
	DEG_graph_clear_tags(graph);
}

/* Evaluate all nodes tagged for updating. */
void DEG_evaluate_on_refresh(EvaluationContext *eval_ctx,
                             Main *bmain,
                             Depsgraph *graph)
{
	DEG_evaluate_on_refresh_ex(eval_ctx, bmain, graph, graph->layers);
}

/* Frame-change happened for root scene that graph belongs to. */
void DEG_evaluate_on_framechange(EvaluationContext *eval_ctx,
                                 Main *bmain,
                                 Depsgraph *graph,
                                 double ctime,
                                 const int layers)
{
	/* Update time on primary timesource. */
	TimeSourceDepsNode *tsrc = graph->find_time_source();
	tsrc->cfra = ctime;

	tsrc->tag_update(graph);

	/* Perform recalculation updates. */
	DEG_evaluate_on_refresh_ex(eval_ctx, bmain, graph, layers);
}

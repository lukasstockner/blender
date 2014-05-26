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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

extern "C" {
#include "BLI_blenlib.h"
#include "BLI_string.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "DNA_anim_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_action.h"
#include "BKE_animsys.h"
#include "BKE_constraint.h"
#include "BKE_DerivedMesh.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_scene.h"

#include "DEG_depsgraph.h"

#include "RNA_access.h"
#include "RNA_types.h"
} /* extern "C" */

#include "depsgraph.h"
#include "depsnode.h"
#include "depsnode_component.h"
#include "depsnode_operation.h"
#include "depsgraph_types.h"
#include "depsgraph_eval.h"
#include "depsgraph_queue.h"
#include "depsgraph_intern.h"

/* *************************************************** */
/* Multi-Threaded Evaluation Internals */

/* Initialise threading lock - called during application startup */
void DEG_threaded_init(void)
{
	DepsgraphTaskScheduler::init();
}

/* Free threading lock - called during application shutdown */
void DEG_threaded_exit(void)
{
	DepsgraphTaskScheduler::exit();
}


/* *************************************************** */
/* Evaluation Entrypoints */

static void calculate_pending_parents(Depsgraph *graph)
{
	for (Depsgraph::OperationNodes::const_iterator it_op = graph->operations.begin(); it_op != graph->operations.end(); ++it_op) {
		OperationDepsNode *node = *it_op;
		
		node->num_links_pending = 0;
		
		/* count number of inputs that need updates */
		if (node->flag & DEPSOP_FLAG_NEEDS_UPDATE) {
			for (OperationDepsNode::Relations::const_iterator it_rel = node->inlinks.begin(); it_rel != node->inlinks.end(); ++it_rel) {
				DepsRelation *rel = *it_rel;
				if (rel->from->flag & DEPSOP_FLAG_NEEDS_UPDATE)
					++node->num_links_pending;
			}
		}
	}
}

static void calculate_eval_priority(OperationDepsNode *node)
{
	if (node->done)
		return;
	node->done = 1;
	
	if (node->flag & DEPSOP_FLAG_NEEDS_UPDATE) {
		/* XXX standard cost of a node, could be estimated somewhat later on */
		const float cost = 1.0f;
		/* NOOP nodes have no cost */
		node->eval_priority = node->is_noop() ? cost : 0.0f;
		
		for (OperationDepsNode::Relations::const_iterator it = node->outlinks.begin(); it != node->outlinks.end(); ++it) {
			DepsRelation *rel = *it;
			calculate_eval_priority(rel->to);
			
			node->eval_priority += rel->to->eval_priority;
		}
	}
	else
		node->eval_priority = 0.0f;
}

static bool is_node_ready(OperationDepsNode *node)
{
	return (node->flag & DEPSOP_FLAG_NEEDS_UPDATE) && node->num_links_pending == 0;
}

static void schedule_graph(DepsgraphTaskPool &pool, Depsgraph *graph, eEvaluationContextType context_type)
{
	for (Depsgraph::OperationNodes::const_iterator it = graph->operations.begin(); it != graph->operations.end(); ++it) {
		OperationDepsNode *node = *it;
		
		if (is_node_ready(node)) {
			pool.push(graph, node, context_type);
		}
	}
}


/* Evaluate all nodes tagged for updating 
 * ! This is usually done as part of main loop, but may also be 
 *   called from frame-change update
 */
void DEG_evaluate_on_refresh(Depsgraph *graph, eEvaluationContextType context_type)
{
	/* generate base evaluation context, upon which all the others are derived... */
	// TODO: this needs both main and scene access...
	
	/* XXX could use a separate pool for each eval context */
	static DepsgraphTaskPool task_pool = DepsgraphTaskPool();
	
	/* clear tags */
	for (Depsgraph::OperationNodes::const_iterator it = graph->operations.begin(); it != graph->operations.end(); ++it) {
		OperationDepsNode *node = *it;
		node->done = 0;
	}
	
	calculate_pending_parents(graph);
	
	/* calculate priority for operation nodes */
	for (Depsgraph::OperationNodes::const_iterator it = graph->operations.begin(); it != graph->operations.end(); ++it) {
		OperationDepsNode *node = *it;
		calculate_eval_priority(node);
	}
	
	DEG_debug_eval_step("Eval Priority Calculation");
	
	schedule_graph(task_pool, graph, context_type);
	
	task_pool.wait();
	
	/* clear any uncleared tags - just in case */
	DEG_graph_clear_tags(graph);
}

/* Frame-change happened for root scene that graph belongs to */
void DEG_evaluate_on_framechange(Depsgraph *graph, eEvaluationContextType context_type, double ctime)
{
	TimeSourceDepsNode *tsrc;
	
	/* update time on primary timesource */
	tsrc = (TimeSourceDepsNode *)graph->find_node(NULL, "", DEPSNODE_TYPE_TIMESOURCE, "");
	tsrc->cfra = ctime;
	
#if 0 /* XXX TODO */
	graph->tag_update(tsrc);
#endif
	
	/* recursively push updates out to all nodes dependent on this, 
	 * until all affected are tagged and/or scheduled up for eval
	 */
	DEG_graph_flush_updates(graph);
	
	/* perform recalculation updates */
	DEG_evaluate_on_refresh(graph, context_type);
}

/* *************************************************** */
/* Evaluation Context Management */

/* Initialise evaluation context for given node */
static void deg_node_evaluation_context_init(ComponentDepsNode *comp, eEvaluationContextType context_type)
{
	/* check if the requested evaluation context exists already */
	if (comp->contexts[context_type] == NULL) {
		/* doesn't exist, so create new evaluation context here */
		bool valid = comp->eval_context_init(context_type);
		if (!valid) {
			/* initialise using standard techniques */
			comp->contexts[context_type] = MEM_callocN(sizeof(DEG_OperationsContext), "Evaluation Context");
			// TODO: init from master context somehow...
		}
	}
	else {
		// TODO: validate existing data, as some parts may no longer exist 
	}
}

/* Initialise evaluation contexts for all nodes */
void DEG_evaluation_context_init(Depsgraph *graph, eEvaluationContextType context_type)
{
	/* initialise master context first... */
	// ...
	
	/* loop over components, initialising their contexts */
	for (Depsgraph::IDNodeMap::const_iterator it = graph->id_hash.begin(); it != graph->id_hash.end(); ++it) {
		IDDepsNode *id_ref = it->second;
		
		/* loop over components */
		for (IDDepsNode::ComponentMap::iterator it = id_ref->components.begin(); it != id_ref->components.end(); ++it) {
			ComponentDepsNode *comp = it->second;
			/* initialise evaluation context */
			// TODO: we probably need to pass the master context down so that it can be handled properly
			deg_node_evaluation_context_init(comp, context_type);
		}
	}
}

/* --------------------------------------------------- */

/* Free evaluation contexts for node */
static void deg_node_evaluation_contexts_free(ComponentDepsNode *comp)
{
	size_t i;
	
	/* free each context in turn */
	for (i = 0; i < DEG_MAX_EVALUATION_CONTEXTS; i++) {
		if (comp->contexts[i]) {
			comp->eval_context_free((eEvaluationContextType)i);
			
			MEM_freeN(comp->contexts[i]);
			comp->contexts[i] = NULL;
		}
	}
}

/* Free evaluation contexts for all nodes */
void DEG_evaluation_contexts_free(Depsgraph *graph)
{
	/* free contexts for components first */
	for (Depsgraph::IDNodeMap::const_iterator it = graph->id_hash.begin(); it != graph->id_hash.end(); ++it) {
		IDDepsNode *id_ref = it->second;
		
		for (IDDepsNode::ComponentMap::iterator it = id_ref->components.begin(); it != id_ref->components.end(); ++it) {
			ComponentDepsNode *comp = it->second;
			/* free evaluation context */
			deg_node_evaluation_contexts_free(comp);
		}
	}
}

/* *************************************************** */

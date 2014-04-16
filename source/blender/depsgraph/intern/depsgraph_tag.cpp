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
 * Core routines for how the Depsgraph works
 */
 
#include <stdio.h>
#include <stdlib.h>

#include <queue>

extern "C" {
#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "DNA_defs.h"
#include "DNA_ID.h"
#include "DNA_listBase.h"

#include "DEG_depsgraph.h"

#include "RNA_access.h"
#include "RNA_types.h"
} /* extern "C" */

#include "depsgraph.h"
#include "depsnode.h"
#include "depsnode_component.h"
#include "depsgraph_types.h"
#include "depsgraph_intern.h"

/* ************************************************** */
/* Update Tagging/Flushing */

/* Data-Based Tagging ------------------------------- */

/* Tag all nodes in ID-block for update 
 * ! This is a crude measure, but is most convenient for old code
 */
void DEG_id_tag_update(Depsgraph *graph, const ID *id)
{
	DepsNode *node = graph->find_node(id, "", DEPSNODE_TYPE_ID_REF, "");
	graph->tag_update(node);
}

/* Tag nodes related to a specific piece of data */
void DEG_data_tag_update(Depsgraph *graph, const PointerRNA *ptr)
{
	DepsNode *node = graph->find_node_from_pointer(ptr, NULL);
	graph->tag_update(node);
}

/* Tag nodes related to a specific property */
void DEG_property_tag_update(Depsgraph *graph, const PointerRNA *ptr, const PropertyRNA *prop)
{
	DepsNode *node = graph->find_node_from_pointer(ptr, prop);
	graph->tag_update(node);
}

/* Update Flushing ---------------------------------- */

/* FIFO queue for tagged nodes that need flushing */
/* XXX This may get a dedicated implementation later if needed - lukas */
typedef std::queue<DepsNode*> FlushQueue;

/* Flush updates from tagged nodes outwards until all affected nodes are tagged */
void DEG_graph_flush_updates(Depsgraph *graph)
{
	/* sanity check */
	if (graph == NULL)
		return;
	
	DEG_debug_eval_step("Flush Begin");
	
	FlushQueue queue;
	/* starting from the tagged "entry" nodes, flush outwards... */
	// NOTE: also need to ensure that for each of these, there is a path back to root, or else they won't be done
	// NOTE: count how many nodes we need to handle - entry nodes may be component nodes which don't count for this purpose!
	for (Depsgraph::EntryTags::const_iterator it = graph->entry_tags.begin(); it != graph->entry_tags.end(); ++it) {
		DepsNode *node = *it;
		queue.push(node);
	}
	
	while (!queue.empty()) {
		DepsNode *node = queue.front();
		queue.pop();
		
		/* flush to sub-nodes... */
		// NOTE: if flushing to subnodes, we should then proceed to remove tag(s) from self, as only the subnode tags matter
		bool flushed_subnodes = false;
		switch (node->type) {
			case DEPSNODE_TYPE_ID_REF: {
				IDDepsNode *id_node = (IDDepsNode *)node;
				for (IDDepsNode::ComponentMap::const_iterator it = id_node->components.begin(); it != id_node->components.end(); ++it) {
					ComponentDepsNode *comp_node = it->second;
					
					if (!(comp_node->flag & DEPSNODE_FLAG_NEEDS_UPDATE)) {
						comp_node->flag |= DEPSNODE_FLAG_NEEDS_UPDATE;
						queue.push(comp_node);
						
						flushed_subnodes = true;
					}
				}
				break;
			}
			
			default: break;
		}
		
		if (flushed_subnodes)
			DEG_debug_eval_step(string_format("Flush Components: %s", node->name.c_str()).c_str());
		
		/* flush to nodes along links... */
		bool flushed_relations = false;
		for (DepsNode::Relations::const_iterator it = node->outlinks.begin(); it != node->outlinks.end(); ++it) {
			DepsRelation *rel = *it;
			DepsNode *to_node = rel->to;
			
			if (!(to_node->flag & DEPSNODE_FLAG_NEEDS_UPDATE)) {
				to_node->flag |= DEPSNODE_FLAG_NEEDS_UPDATE;
				queue.push(to_node);
				
				flushed_relations = true;
			}
		}
		
		if (flushed_relations)
			DEG_debug_eval_step(string_format("Flush Dependencies: %s", node->name.c_str()).c_str());
	}
	
	/* clear entry tags, since all tagged nodes should now be reachable from root */
	graph->entry_tags.clear();
	
	DEG_debug_eval_step("Flush End");
}

/* Clear tags from all operation nodes */
void DEG_graph_clear_tags(Depsgraph *graph)
{
	/* go over all operation nodes, clearing tags */
	for (Depsgraph::OperationNodes::const_iterator it = graph->all_opnodes.begin(); it != graph->all_opnodes.end(); ++it) {
		DepsNode *node = *it;
		
		/* clear node's "pending update" settings */
		node->flag &= ~(DEPSNODE_FLAG_DIRECTLY_MODIFIED | DEPSNODE_FLAG_NEEDS_UPDATE);
		node->num_links_pending = 0; /* reset so that it can be bumped up again */
	}
	
	/* clear any entry tags which haven't been flushed */
	graph->entry_tags.clear();
}

/* ************************************************** */

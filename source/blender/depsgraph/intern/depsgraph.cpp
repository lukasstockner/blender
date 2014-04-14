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

#include "MEM_guardedalloc.h"

extern "C" {
#include "DNA_action_types.h"
#include "DNA_sequence_types.h"

#include "RNA_access.h"
}

#include "depsgraph.h" /* own include */
#include "depsnode.h"
#include "depsnode_component.h"
#include "depsnode_operation.h"
#include "depsgraph_intern.h"


Depsgraph::Depsgraph()
{
	this->root_node = NULL;
}

Depsgraph::~Depsgraph()
{
	/* free root node - it won't have been freed yet... */
	if (this->root_node) {
		delete this->root_node;
	}
	
	clear_id_nodes();
	clear_subgraph_nodes();
}

/* Query Conditions from RNA ----------------------- */

/* Determine node-querying criteria for finding a suitable node,
 * given a RNA Pointer (and optionally, a property too)
 */
static void find_node_criteria_from_pointer(const PointerRNA *ptr, const PropertyRNA *prop,
                                            ID **id, string *subdata,
                                            eDepsNode_Type *type, string *name)
{
	/* set default values for returns */
	*id       = (ID *)ptr->id.data;        /* for obvious reasons... */
	*subdata  = "";                        /* default to no subdata (e.g. bone) name lookup in most cases */
	*type     = DEPSNODE_TYPE_PARAMETERS;  /* all unknown data effectively falls under "parameter evaluation" */
	*name     = "";                        /* default to no name to lookup in most cases */
	
	/* handling of commonly known scenarios... */
	if (ptr->type == &RNA_PoseBone) {
		bPoseChannel *pchan = (bPoseChannel *)ptr->data;
		
		/* bone - generally, we just want the bone component... */
		*type = DEPSNODE_TYPE_BONE;
		*subdata = pchan->name;
	}
	else if (ptr->type == &RNA_Object) {
		Object *ob = (Object *)ptr->data;
		
		/* transforms props? */
		// ...
	}
	else if (RNA_struct_is_a(ptr->type, &RNA_Sequence)) {
		Sequence *seq = (Sequence *)ptr->data;
		
		/* sequencer strip */
		*type = DEPSNODE_TYPE_SEQUENCER;
		*subdata = seq->name; // xxx?
	}
}

/* Convenience wrapper to find node given just pointer + property */
DepsNode *Depsgraph::find_node_from_pointer(const PointerRNA *ptr, const PropertyRNA *prop)
{
	ID *id;
	eDepsNode_Type type;
	string subdata;
	string name;
	
	/* get querying conditions */
	find_node_criteria_from_pointer(ptr, prop, &id, &subdata, &type, &name);
	
	/* use standard node finding code... */
	return find_node(id, subdata, type, name);
}

/* Node Management ---------------------------- */

RootDepsNode *Depsgraph::add_root_node()
{
	if (!root_node) {
		DepsNodeFactory *factory = DEG_get_node_factory(DEPSNODE_TYPE_ROOT);
		root_node = (RootDepsNode *)factory->create_node(NULL, "", "Root (Scene)");
	}
	return root_node;
}

SubgraphDepsNode *Depsgraph::add_subgraph_node(const ID *id)
{
	DepsNodeFactory *factory = DEG_get_node_factory(DEPSNODE_TYPE_SUBGRAPH);
	SubgraphDepsNode *subgraph_node = (SubgraphDepsNode *)factory->create_node(id, "", id->name+2);
	
	/* add to subnodes list */
	this->subgraphs.insert(subgraph_node);
	
	/* if there's an ID associated, add to ID-nodes lookup too */
	if (id) {
#if 0 /* XXX subgraph node is NOT a true IDDepsNode - what is this supposed to do? */
		// TODO: what to do if subgraph's ID has already been added?
		BLI_assert(!graph->find_id_node(id));
		graph->id_hash[id] = this;
#endif
	}
	
	return subgraph_node;
}

void Depsgraph::remove_subgraph_node(SubgraphDepsNode *subgraph_node)
{
	subgraphs.erase(subgraph_node);
	delete subgraph_node;
}

void Depsgraph::clear_subgraph_nodes()
{
	for (Subgraphs::iterator it = subgraphs.begin(); it != subgraphs.end(); ++it) {
		SubgraphDepsNode *subgraph_node = *it;
		delete subgraph_node;
	}
	subgraphs.clear();
}

IDDepsNode *Depsgraph::find_id_node(const ID *id) const
{
	IDNodeMap::const_iterator it = this->id_hash.find(id);
	return it != this->id_hash.end() ? it->second : NULL;
}

IDDepsNode *Depsgraph::add_id_node(const ID *id, const string &name)
{
	IDDepsNode *id_node = find_id_node(id);
	if (!id_node) {
		DepsNodeFactory *factory = DEG_get_node_factory(DEPSNODE_TYPE_ID_REF);
		id_node = (IDDepsNode *)factory->create_node(id, "", name);
		
		/* register */
		this->id_hash[id] = id_node;
	}
	return id_node;
}

void Depsgraph::remove_id_node(const ID *id)
{
	IDDepsNode *id_node = find_id_node(id);
	if (id_node) {
		/* unregister */
		this->id_hash.erase(id);
		
		delete id_node;
	}
}

void Depsgraph::clear_id_nodes()
{
	for (IDNodeMap::const_iterator it = id_hash.begin(); it != id_hash.end(); ++it) {
		IDDepsNode *id_node = it->second;
		delete id_node;
	}
	id_hash.clear();
}

/* Add new relationship between two nodes */
DepsRelation *Depsgraph::add_new_relation(DepsNode *from, DepsNode *to, 
                                          eDepsRelation_Type type, 
                                          const string &description)
{
	/* create new relation, and add it to the graph */
	DepsRelation *rel = new DepsRelation(from, to, type, description);
	
	DEG_debug_build_relation_added(rel);
	
	return rel;
}

/* Ensure that all implicit constraints between nodes are satisfied 
 * (e.g. components are only allowed to be executed in a certain order)
 */
void Depsgraph::validate_links()
{
	/* go over each ID node to recursively call validate_links()
	 * on it, which should be enough to ensure that all of those
	 * subtrees are valid
	 */
	for (Depsgraph::IDNodeMap::const_iterator it = this->id_hash.begin(); it != this->id_hash.end(); ++it) {
		DepsNode *node = it->second;
		node->validate_links(this);
	}
}

/* Sort nodes to determine evaluation order for operation nodes
 * where dependency relationships won't get violated.
 */
void Depsgraph::sort()
{
#if 0
	void *ctx = NULL; // XXX: temp struct for keeping track of visited nodes, etc.?
	
	/* 1) traverse graph from root
	 *   - note when each graph was visited (within its peers)
	 *   - tag/knock out relationships leading to cyclic dependencies
	 */
	DEG_graph_traverse(graph, DEG_Filter_ExecutableNodes, NULL, 
	                          tag_nodes_for_sorting,      ctx); 
	

	/* 2) tweak order of nodes within each set of links */
#endif	
}

/* ************************************************** */
/* Relationships Management */

DepsRelation::DepsRelation(DepsNode *from, DepsNode *to, eDepsRelation_Type type, const string &description)
{
	this->from = from;
	this->to = to;
	this->type = type;
	this->name = description;
	
	/* hook it up to the nodes which use it */
	from->outlinks.insert(this);
	to->inlinks.insert(this);
}

DepsRelation::~DepsRelation()
{
	/* sanity check */
	BLI_assert(this->from && this->to);
	/* remove it from the nodes that use it */
	this->from->outlinks.erase(this);
	this->to->inlinks.erase(this);
}

/* Low level tagging -------------------------------------- */

/* Tag a specific node as needing updates */
void Depsgraph::tag_update(DepsNode *node)
{
	/* sanity check */
	if (!node)
		return;
	
	/* tag for update, but also not that this was the source of an update */
	node->flag |= (DEPSNODE_FLAG_NEEDS_UPDATE | DEPSNODE_FLAG_DIRECTLY_MODIFIED);
	
	/* add to graph-level set of directly modified nodes to start searching from
	 * NOTE: this is necessary since we have several thousand nodes to play with...
	 */
	this->entry_tags.insert(node);
}

/* ************************************************** */
/* Public Graph API */

/* Initialise a new Depsgraph */
Depsgraph *DEG_graph_new()
{
	return new Depsgraph;
}

/* Free graph's contents and graph itself */
void DEG_graph_free(Depsgraph *graph)
{
	delete graph;
}

/* ************************************************** */

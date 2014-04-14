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
 * Datatypes for internal use in the Depsgraph
 * 
 * All of these datatypes are only really used within the "core" depsgraph.
 * In particular, node types declared here form the structure of operations
 * in the graph.
 */

#ifndef __DEPSGRAPH_H__
#define __DEPSGRAPH_H__

#include <vector>

#include "MEM_guardedalloc.h"

#include "depsgraph_types.h"

#include "depsgraph_util_map.h"
#include "depsgraph_util_set.h"
#include "depsgraph_util_string.h"

using std::vector;

struct PointerRNA;
struct PropertyRNA;

struct DepsNode;
struct RootDepsNode;
struct TimeSourceDepsNode;
struct IDDepsNode;
struct SubgraphDepsNode;
struct ComponentDepsNode;
struct OperationDepsNode;


/* ************************************* */
/* Relationships Between Nodes */

/* Settings/Tags on Relationship */
typedef enum eDepsRelation_Flag {
	/* "touched" tag is used when filtering, to know which to collect */
	DEPSREL_FLAG_TEMP_TAG   = (1 << 0),
	
	/* "cyclic" link - when detecting cycles, this relationship was the one
	 * which triggers a cyclic relationship to exist in the graph
	 */
	DEPSREL_FLAG_CYCLIC     = (1 << 1),
} eDepsRelation_Flag;

/* B depends on A (A -> B) */
struct DepsRelation {
	/* the nodes in the relationship (since this is shared between the nodes) */
	DepsNode *from;               /* A */
	DepsNode *to;                 /* B */
	
	/* relationship attributes */
	string name;                  /* label for debugging */
	
	eDepsRelation_Type type;      /* type */
	int flag;                     /* (eDepsRelation_Flag) */
	
	DepsRelation(DepsNode *from, DepsNode *to, eDepsRelation_Type type, const string &description);
	~DepsRelation();
	
#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("DEG:DepsNode")
#endif
};

/* ************************************* */
/* Depsgraph */

/* Dependency Graph object */
struct Depsgraph {
	typedef unordered_map<const ID *, IDDepsNode *> IDNodeMap;
	typedef unordered_set<SubgraphDepsNode *> Subgraphs;
	typedef unordered_set<DepsNode *> EntryTags;
	typedef vector<DepsNode *> OperationNodes;
	
	Depsgraph();
	~Depsgraph();
	
	/* Find node which matches the specified description
	 *
	 * < id: ID block that is associated with this
	 * < (subdata): identifier used for sub-ID data (e.g. bone)
	 * < type: type of node we're dealing with
	 * < (name): custom identifier assigned to node 
	 *
	 * > returns: A node matching the required characteristics if it exists
	 *            OR NULL if no such node exists in the graph
	 */
	DepsNode *find_node(const ID *id, const string &subdata, eDepsNode_Type type, const string &name);
	/* Convenience wrapper to find node given just pointer + property
	 * < ptr: pointer to the data that node will represent
	 * < (prop): optional property affected - providing this effectively results in inner nodes being returned
	 *
	 * > returns: A node matching the required characteristics if it exists
	 *            OR NULL if no such node exists in the graph
	 */
	DepsNode *find_node_from_pointer(const PointerRNA *ptr, const PropertyRNA *prop);
	
	RootDepsNode *add_root_node();
	
	SubgraphDepsNode *add_subgraph_node(const ID *id);
	void remove_subgraph_node(SubgraphDepsNode *subgraph_node);
	void clear_subgraph_nodes();
	
	IDDepsNode *find_id_node(const ID *id) const;
	IDDepsNode *add_id_node(const ID *id, const string &name = "");
	void remove_id_node(const ID *id);
	void clear_id_nodes();
	
	/* Add new relationship between two nodes */
	DepsRelation *add_new_relation(DepsNode *from, DepsNode *to,
	                               eDepsRelation_Type type, 
	                               const string &description);
	
	/* Ensure that all implicit constraints between nodes are satisfied 
	 * (e.g. components are only allowed to be executed in a certain order)
	 */
	void validate_links();
	
	/* Sort nodes to determine evaluation order for operation nodes
	 * where dependency relationships won't get violated.
	 */
	void sort();
	
	/* Tag a specific node as needing updates */
	void tag_update(DepsNode *node);
	
	
	/* Core Graph Functionality ........... */
	IDNodeMap id_hash;          /* <ID : IDDepsNode> mapping from ID blocks to nodes representing these blocks (for quick lookups) */
	RootDepsNode *root_node;    /* "root" node - the one where all evaluation enters from */
	
	Subgraphs subgraphs;        /* subgraphs referenced in tree... */
	
	/* Quick-Access Temp Data ............. */
	EntryTags entry_tags;       /* nodes which have been tagged as "directly modified" */
	
	/* Convenience Data ................... */
	/* XXX: should be collected after building (if actually needed?) */
	OperationNodes all_opnodes; /* all operation nodes, sorted in order of single-thread traversal order */
	
	// XXX: additional stuff like eval contexts, mempools for allocating nodes from, etc.
	
#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("DEG:DepsNode")
#endif
};

/* Helper macros for interating over set of relationship
 * links incident on each node.
 *
 * NOTE: it is safe to perform removal operations here...
 *
 * < relations_set: (DepsNode::Relations) set of relationships (in/out links)
 * > relation:  (DepsRelation *) identifier where DepsRelation that we're currently accessing comes up
 */
#define DEPSNODE_RELATIONS_ITER_BEGIN(relations_set_, relation_)                          \
	{                                                                                \
		DepsNode::Relations::const_iterator __rel_iter = relations_set_.begin();     \
		while (__rel_iter != relations_set_.end()) {                                 \
			DepsRelation *relation_ = *__rel_iter;                                   \
			++__rel_iter;

			/* ... code for iterator body can be written here ... */

#define DEPSNODE_RELATIONS_ITER_END                                                  \
		}                                                                            \
	}

/* ************************************* */

#endif // __DEPSGRAPH_H__

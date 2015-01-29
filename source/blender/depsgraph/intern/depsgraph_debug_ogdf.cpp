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
 * The Original Code is Copyright (C) 2015 Blender Foundation.
 * All rights reserved.
 *
 * Original Author: Joshua Leung
 * Contributor(s): None Yet
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 * Implementation of tools for debugging the depsgraph using OGDF
 */

#include <stdlib.h>
#include <string.h>

/* NOTE: OGDF needs to come before Blender headers, or else there will be compile errors on mingw64 */
#include <ogdf/basic/Graph.h>
#include <ogdf/layered/OptimalHierarchyLayout.h>

extern "C" {
#include "BLI_listbase.h"
#include "BLI_ghash.h"
#include "BLI_utildefines.h"

#include "DNA_scene_types.h"
#include "DNA_userdef_types.h"

#include "DEG_depsgraph_debug.h"
#include "DEG_depsgraph_build.h"

#include "WM_api.h"
#include "WM_types.h"
}  /* extern "C" */

#include "depsgraph_debug.h"
#include "depsnode.h"
#include "depsnode_component.h"
#include "depsnode_operation.h"
#include "depsgraph_types.h"
#include "depsgraph_intern.h"

/* ****************** */
/* OGDF Debugging */

/* Typedef for mapping from Depsgraph Nodes to OGDF Nodes */
typedef unordered_map<const DepsNode *, ogdf::node> GraphNodesMap;

/* Helper data passed to all calls here */
struct DebugContext {
	/* the output graph, and formatting info for the graph */
	ogdf::Graph *G;
	ogdf::GraphAttributes *GA;

	/* mapping from Depsgraph Nodes to OGDF nodes */
	GraphNodesMap node_map;

	/* flags for what to include */
	bool show_tags;
	bool show_eval_priority;

	bool show_owner_links;
	bool show_rel_labels;
};

static void deg_debug_ogdf_graph_nodes(const DebugContext &ctx, const Depsgraph *graph);
static void deg_debug_ogdf_graph_relations(const DebugContext &ctx, const Depsgraph *graph);

/* -------------------------------- */

/* Only one should be enabled, defines whether graphviz nodes
* get colored by individual types or classes.
*/
#define COLOR_SCHEME_NODE_CLASS 1
//#define COLOR_SCHEME_NODE_TYPE  2

#ifdef COLOR_SCHEME_NODE_TYPE
static const int deg_debug_node_type_color_map[][2] = {
	{ DEPSNODE_TYPE_ROOT, 0 },
	{ DEPSNODE_TYPE_TIMESOURCE, 1 },
	{ DEPSNODE_TYPE_ID_REF, 2 },
	{ DEPSNODE_TYPE_SUBGRAPH, 3 },

	/* Outer Types */
	{ DEPSNODE_TYPE_PARAMETERS, 4 },
	{ DEPSNODE_TYPE_PROXY, 5 },
	{ DEPSNODE_TYPE_ANIMATION, 6 },
	{ DEPSNODE_TYPE_TRANSFORM, 7 },
	{ DEPSNODE_TYPE_GEOMETRY, 8 },
	{ DEPSNODE_TYPE_SEQUENCER, 9 },
	{ DEPSNODE_TYPE_SHADING, 10 },
	{ -1, 0 }
};
#endif

static const int deg_debug_max_colors = 12;
static const char *deg_debug_colors_dark[] = {"#6e8997", "#144f77", "#76945b",
                                              "#216a1d", "#a76665", "#971112",
                                              "#a87f49", "#0a9540", "#86768e",
                                              "#462866", "#a9a965", "#753b1a"};
static const char *deg_debug_colors[] = {"#a6cee3", "#1f78b4", "#b2df8a",
                                         "#33a02c", "#fb9a99", "#e31a1c",
                                         "#fdbf6f", "#ff7f00", "#cab2d6",
                                         "#6a3d9a", "#ffff99", "#b15928"};
static const char *deg_debug_colors_light[] = {"#8dd3c7", "#ffffb3", "#bebada",
                                               "#fb8072", "#80b1d3", "#fdb462",
                                               "#b3de69", "#fccde5", "#d9d9d9",
                                               "#bc80bd", "#ccebc5","#ffed6f"};

static int deg_debug_node_color_index(const DepsNode *node)
{
#ifdef COLOR_SCHEME_NODE_CLASS
	/* Some special types. */
	switch (node->type) {
		case DEPSNODE_TYPE_ID_REF:
			return 5;
		case DEPSNODE_TYPE_OPERATION:
		{
			OperationDepsNode *op_node = (OperationDepsNode *)node;
			if (op_node->is_noop())
				return 8;
		}

		default:
			break;
	}
	/* Do others based on class. */
	switch (node->tclass) {
		case DEPSNODE_CLASS_OPERATION:
			return 4;
		case DEPSNODE_CLASS_COMPONENT:
			return 1;
		default:
			return 9;
	}
#endif

#ifdef COLOR_SCHEME_NODE_TYPE
	const int(*pair)[2];
	for (pair = deg_debug_node_type_color_map; (*pair)[0] >= 0; ++pair) {
		if ((*pair)[0] == node->type) {
			return (*pair)[1];
		}
	}
	return -1;
#endif
}

static const char *deg_debug_ogdf_node_color(const DebugContext &ctx, const DepsNode *node)
{
	const int color_index = deg_debug_node_color_index(node);
	const char *defaultcolor = "#DCDCDC";
	const char *fillcolor = (color_index < 0) ? defaultcolor : deg_debug_colors_light[color_index % deg_debug_max_colors];
	
	return fillcolor;
}

static void deg_debug_ogdf_node_single(const DebugContext &ctx, const DepsNode *node)
{
	string name = node->identifier();
	//float priority = -1.0f;

#if 0 // XXX: crashes for now
	if (node->type == DEPSNODE_TYPE_ID_REF) {
		IDDepsNode *id_node = (IDDepsNode *)node;

		char buf[256];
		BLI_snprintf(buf, sizeof(buf), " (Layers: %d)", id_node->layers);

		name += buf;
	}
#endif
	//if (ctx.show_eval_priority && node->tclass == DEPSNODE_CLASS_OPERATION) {
	//	priority = ((OperationDepsNode *)node)->eval_priority;
	//}

	/* create node */
	ogdf::node v = ctx.G->newNode();

	ctx.GA->labelNode(v) = ogdf::String(name.c_str());
	ctx.GA->colorNode(v) = ogdf::String(deg_debug_ogdf_node_color(ctx, node)); /* ogdf::Color == ogdf::String */
	// TODO: style/shape - rounded rect, vs straight-edge, vs ellipse?

	/* add to reference mapping for later reference when building relations */
	ctx.node_map[node] = v;
}

static void deg_debug_ogdf_node(const DebugContext &ctx, const DepsNode *node)
{
	switch (node->type) {
		case DEPSNODE_TYPE_ID_REF:
		{
			const IDDepsNode *id_node = (const IDDepsNode *)node;
			if (id_node->components.empty()) {
				deg_debug_ogdf_node_single(ctx, node);
			}
			else {
				for (IDDepsNode::ComponentMap::const_iterator it = id_node->components.begin();
					it != id_node->components.end();
					++it)
				{
					const ComponentDepsNode *comp = it->second;
					deg_debug_ogdf_node(ctx, comp);
				}
			}
			break;
		}
		case DEPSNODE_TYPE_SUBGRAPH:
		{
			SubgraphDepsNode *sub_node = (SubgraphDepsNode *)node;
			if (sub_node->graph) {
				//deg_debug_graphviz_node_cluster_begin(ctx, node);
				deg_debug_ogdf_graph_nodes(ctx, sub_node->graph);
				//deg_debug_graphviz_node_cluster_end(ctx);
			}
			else {
				deg_debug_ogdf_node_single(ctx, node);
			}
			break;
		}
		case DEPSNODE_TYPE_PARAMETERS:
		case DEPSNODE_TYPE_ANIMATION:
		case DEPSNODE_TYPE_TRANSFORM:
		case DEPSNODE_TYPE_PROXY:
		case DEPSNODE_TYPE_GEOMETRY:
		case DEPSNODE_TYPE_SEQUENCER:
		case DEPSNODE_TYPE_EVAL_POSE:
		case DEPSNODE_TYPE_BONE:
		case DEPSNODE_TYPE_SHADING:
		{
			ComponentDepsNode *comp_node = (ComponentDepsNode *)node;
			if (!comp_node->operations.empty()) {
				for (ComponentDepsNode::OperationMap::const_iterator it = comp_node->operations.begin();
					it != comp_node->operations.end();
					++it)
				{
					const DepsNode *op_node = it->second;
					deg_debug_ogdf_node(ctx, op_node);
				}
			}
			else {
				deg_debug_ogdf_node_single(ctx, node);
			}
			break;
		}
		default:
			deg_debug_ogdf_node_single(ctx, node);
			break;
	}
}

static void deg_debug_ogdf_graph_nodes(const DebugContext &ctx, const Depsgraph *graph)
{
	if (graph->root_node) {
		deg_debug_ogdf_node(ctx, graph->root_node);
	}
	for (Depsgraph::IDNodeMap::const_iterator it = graph->id_hash.begin();
		it != graph->id_hash.end();
		++it)
	{
		DepsNode *node = it->second;
		deg_debug_ogdf_node(ctx, node);
	}
	TimeSourceDepsNode *time_source = graph->find_time_source(NULL);
	if (time_source != NULL) {
		deg_debug_ogdf_node(ctx, time_source);
	}
}

/* -------------------------------- */

static void deg_debug_ogdf_node_relations(const DebugContext &ctx, const DepsNode *node)
{
	DEPSNODE_RELATIONS_ITER_BEGIN(node->inlinks, rel)
	{
		const DepsNode *head = rel->from;
		const DepsNode *tail = rel->to; /* same as node */

		ogdf::node head_node = ctx.node_map[head];
		ogdf::node tail_node = ctx.node_map[tail];

		/* create new edge for this relationship */
		ogdf::edge e = ctx.G->newEdge(head_node, tail_node);

		ctx.GA->arrowEdge(e) = ogdf::GraphAttributes::EdgeArrow::last;
		ctx.GA->labelEdge(e) = ogdf::String(rel->name.c_str());
	}
	DEPSNODE_RELATIONS_ITER_END;

#if 0
	if (node->tclass == DEPSNODE_CLASS_COMPONENT) {
		const ComponentDepsNode *comp_node = (const ComponentDepsNode *)node;
		for (ComponentDepsNode::OperationMap::const_iterator it = comp_node->operations.begin();
			 it != comp_node->operations.end();
			 ++it)
		{
			OperationDepsNode *op_node = it->second;
			deg_debug_ogdf_node_relations(ctx, op_node);
		}
	}
	else if (node->type == DEPSNODE_TYPE_ID_REF) {
		const IDDepsNode *id_node = (const IDDepsNode *)node;
		for (IDDepsNode::ComponentMap::const_iterator it = id_node->components.begin();
			 it != id_node->components.end();
			 ++it)
		{
			const ComponentDepsNode *comp = it->second;
			deg_debug_ogdf_node_relations(ctx, comp);
		}
	}
	else if (node->type == DEPSNODE_TYPE_SUBGRAPH) {
		SubgraphDepsNode *sub_node = (SubgraphDepsNode *)node;
		if (sub_node->graph) {
			deg_debug_ogdf_graph_relations(ctx, sub_node->graph);
		}
	}
#endif
}

static void deg_debug_ogdf_graph_relations(const DebugContext &ctx, const Depsgraph *graph)
{
#if 0
	if (graph->root_node) {
		deg_debug_ogdf_node_relations(ctx, graph->root_node);
	}
	for (Depsgraph::IDNodeMap::const_iterator it = graph->id_hash.begin();
		it != graph->id_hash.end();
		++it)
	{
		DepsNode *id_node = it->second;
		deg_debug_ogdf_node_relations(ctx, id_node);
	}
#else
	for (Depsgraph::IDNodeMap::const_iterator it = graph->id_hash.begin();
		it != graph->id_hash.end();
		++it)
	{
		IDDepsNode *id_node = it->second;
		for (IDDepsNode::ComponentMap::const_iterator it = id_node->components.begin();
			it != id_node->components.end();
			++it)
		{
			ComponentDepsNode *comp_node = it->second;
			for (ComponentDepsNode::OperationMap::const_iterator it = comp_node->operations.begin();
				it != comp_node->operations.end();
				++it)
			{
				OperationDepsNode *op_node = it->second;
				deg_debug_ogdf_node_relations(ctx, op_node);
			}
		}
	}

	TimeSourceDepsNode *time_source = graph->find_time_source(NULL);
	if (time_source != NULL) {
		deg_debug_ogdf_node_relations(ctx, time_source);
	}
#endif
}

/* -------------------------------- */

void DEG_debug_ogdf(const Depsgraph *graph, const char *filename)
{
	if (!graph) {
		return;
	}

	/* create OGDF graph */
	ogdf::Graph outgraph;

	ogdf::GraphAttributes GA(outgraph,
							 ogdf::GraphAttributes::nodeLabel |
							 ogdf::GraphAttributes::nodeColor |
							 ogdf::GraphAttributes::edgeLabel |
							 ogdf::GraphAttributes::edgeType |
		                     ogdf::GraphAttributes::edgeArrow);

	/* build OGDF graph from depsgraph */
	DebugContext ctx;

	ctx.G = &outgraph;
	ctx.GA = &GA;
	ctx.show_eval_priority = false;

	printf("Converting to OGDF...\n");
	
	deg_debug_ogdf_graph_nodes(ctx, graph);
	deg_debug_ogdf_graph_relations(ctx, graph);


	/* compute graph layout */
	printf("Computing Layout...\n");

	/* export it */
	printf("Exporting GML to '%s'...\n", filename);
	GA.writeGML(filename);
}

/* ****************** */


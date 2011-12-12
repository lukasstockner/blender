/*
 * Copyright 2011, Blender Foundation.
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
 * Contributor: 
 *		Jeroen Bakker 
 *		Monique Dewanchand
 */

#include "COM_ExecutionSystemHelper.h"

#include "PIL_time.h"
#include "BKE_node.h"
#include "COM_Converter.h"
#include <sstream>
#include "COM_NodeOperation.h"
#include "COM_ExecutionGroup.h"
#include "COM_NodeBase.h"
#include "COM_WorkScheduler.h"
#include "COM_ReadBufferOperation.h"
#include "COM_MemoryManager.h"
#include "stdio.h"
#include "COM_GroupNode.h"
#include "COM_WriteBufferOperation.h"
#include "COM_ReadBufferOperation.h"

Node* ExecutionSystemHelper::addbNodeTree(ExecutionSystem &system, int nodes_start, bNodeTree *tree) {
	vector<Node*>& nodes = system.getNodes();
	vector<SocketConnection*>& links = system.getConnections();
	Node* mainnode = NULL;
	/* add all nodes of the tree to the node list */
	bNode* node = (bNode*)tree->nodes.first;
	while (node != NULL) {
		Node* execnode = addNode(nodes, node);
		if (node->type == CMP_NODE_COMPOSITE) {
			mainnode = execnode;
		}
		node = (bNode*)node->next;
	}

	NodeRange node_range(nodes.begin()+nodes_start, nodes.end());

	/* add all nodelinks of the tree to the link list */
	bNodeLink* nodelink = (bNodeLink*)tree->links.first;
	while (nodelink != NULL) {
		addNodeLink(node_range, links, nodelink);
		nodelink = (bNodeLink*)nodelink->next;
	}

	/* Expand group nodes */
	for (int i=nodes_start; i < nodes.size(); ++i) {
		Node *execnode = nodes[i];
		if (execnode->isGroupNode()) {
			GroupNode * groupNode = (GroupNode*)execnode;
			groupNode->ungroup(system);
		}
	}

	return mainnode;
}

void ExecutionSystemHelper::addNode(vector<Node*>& nodes, Node *node) {
	nodes.push_back(node);
}

Node* ExecutionSystemHelper::addNode(vector<Node*>& nodes, bNode *bNode) {
	Converter converter;
	Node * node;
	node = converter.convert(bNode);
	if (node != NULL) {
		addNode(nodes, node);
		return node;
	}
	return NULL;
}
void ExecutionSystemHelper::addOperation(vector<NodeOperation*>& operations, NodeOperation *operation) {
	operations.push_back(operation);
}

void ExecutionSystemHelper::addExecutionGroup(vector<ExecutionGroup*>& executionGroups, ExecutionGroup *executionGroup) {
	executionGroups.push_back(executionGroup);
}

void ExecutionSystemHelper::findOutputNodeOperations(vector<NodeOperation*>* result, vector<NodeOperation*>& operations, bool rendering) {
	unsigned int index;

	for (index = 0 ; index < operations.size() ; index ++) {
		NodeOperation* operation = operations[index];
		if (operation->isOutputOperation(rendering)) {
			result->push_back(operation);
		 }
	}
}

static InputSocket* find_input(NodeRange &node_range, bNode *bnode, bNodeSocket* bsocket) {
	if (bnode != NULL) {
		for(NodeIterator it=node_range.first; it!=node_range.second; ++it) {
			Node* node = *it;
			if (node->getbNode() == bnode)
				return node->findInputSocketBybNodeSocket(bsocket);
		}
	} else {
		for(NodeIterator it=node_range.first; it!=node_range.second; ++it) {
			Node* node = *it;
			if (node->isProxyNode()) {
				InputSocket *proxySocket = node->getInputSocket(0);
				if (proxySocket->getbNodeSocket()==bsocket)
					return proxySocket;
			}
		}
	}
	return NULL;
}
static OutputSocket* find_output(NodeRange &node_range, bNode *bnode, bNodeSocket* bsocket) {
	if (bnode != NULL) {
		for(NodeIterator it=node_range.first; it!=node_range.second; ++it) {
			Node* node = *it;
			if (node->getbNode() == bnode)
				return node->findOutputSocketBybNodeSocket(bsocket);
		}
	} else {
		for(NodeIterator it=node_range.first; it!=node_range.second; ++it) {
			Node* node = *it;
			if (node->isProxyNode()) {
				OutputSocket *proxySocket = node->getOutputSocket(0);
				if (proxySocket->getbNodeSocket()==bsocket)
					return proxySocket;
			}
		}
	}
	return NULL;
}
SocketConnection* ExecutionSystemHelper::addNodeLink(NodeRange &node_range, vector<SocketConnection*>& links, bNodeLink *bNodeLink) {
	/// @note: cyclic lines will be ignored. This has been copied from node.c
	if (bNodeLink->tonode != 0 && bNodeLink->fromnode != 0) {
		if(!(bNodeLink->fromnode->level >= bNodeLink->tonode->level && bNodeLink->tonode->level!=0xFFF)) { // only add non cyclic lines! so execution will procede
			return NULL;
		}
	}

	InputSocket *inputSocket = find_input(node_range, bNodeLink->tonode, bNodeLink->tosock);
	OutputSocket *outputSocket = find_output(node_range, bNodeLink->fromnode, bNodeLink->fromsock);
	if (inputSocket == NULL || outputSocket == NULL) {
		return NULL;
	}
	if (inputSocket->isConnected()) {
		return NULL;
	}
	SocketConnection* connection = addLink(links, outputSocket, inputSocket);
	return connection;
}

SocketConnection* ExecutionSystemHelper::addLink(vector<SocketConnection*>& links, OutputSocket* fromSocket, InputSocket* toSocket) {
	SocketConnection * newconnection = new SocketConnection();
	newconnection->setFromSocket(fromSocket);
	newconnection->setToSocket(toSocket);
	fromSocket->addConnection(newconnection);
	toSocket->setConnection(newconnection);
	links.push_back(newconnection);
	return newconnection;
}

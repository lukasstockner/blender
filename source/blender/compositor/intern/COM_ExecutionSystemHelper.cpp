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

Node* ExecutionSystemHelper::addbNodeTree(vector<Node*>& nodes, vector<SocketConnection*>& links, bNodeTree *tree) {
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

	/* add all nodelinks of the tree to the link list */
	bNodeLink* nodelink = (bNodeLink*)tree->links.first;
    while (nodelink != NULL) {
			addNodeLink(nodes, links, nodelink);
			nodelink = (bNodeLink*)nodelink->next;
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


SocketConnection* ExecutionSystemHelper::addNodeLink(vector<Node*>& nodes, vector<SocketConnection*>& links, bNodeLink *bNodeLink) {
	/// @note: cyclic lines will be ignored. This has been copied from node.c
	if (bNodeLink->tonode != 0 && bNodeLink->fromnode != 0) {
		if(!(bNodeLink->fromnode->level >= bNodeLink->tonode->level && bNodeLink->tonode->level!=0xFFF)) { // only add non cyclic lines! so execution will procede
			return NULL;
		}
	}

	Node* fromNode = findNodeBybNode(nodes, bNodeLink->fromnode, bNodeLink->fromsock);
	Node* toNode = findNodeBybNode(nodes, bNodeLink->tonode, bNodeLink->tosock);

	OutputSocket *outputSocket = fromNode->findOutputSocketBybNodeSocket(bNodeLink->fromsock);
	InputSocket *inputSocket = toNode->findInputSocketBybNodeSocket(bNodeLink->tosock);
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


bool ExecutionSystemHelper::containsbNodeSocket(bNode *bnode, bNodeSocket* bsocket) {
	bNodeSocket *socket = (bNodeSocket*)bnode->inputs.first;
	while (socket != NULL) {
		if (socket->groupsock == bsocket) {
			return true;
		}
		socket = (bNodeSocket*)socket->next;
	}
	socket = (bNodeSocket*)bnode->outputs.first;
	while (socket != NULL) {
		if (socket->groupsock == bsocket) {
			return true;
		}
		socket = (bNodeSocket*)socket->next;
	}

	return false;
}


Node* ExecutionSystemHelper::findNodeBybNode(vector<Node*>& nodes, bNode *bnode, bNodeSocket* bsocket) {
	unsigned int index;
	if (bnode != NULL) {
		for(index = 0; index < nodes.size(); index++) {
			Node* node = nodes[index];
			if (node->getbNode() == bnode) {
				return node;
			}
		}
	} else {
		// only look in the GroupNodes.
		for(index = 0; index < nodes.size(); index++) {
			Node* node = nodes[index];
			if (node->isGroupNode()) {
				bNode *bnode = node->getbNode();
				if (containsbNodeSocket(bnode, bsocket)) {
					return node;
				}
			}
		}
	}
	return NULL;
}

void ExecutionSystemHelper::ungroup(ExecutionSystem &system) {
	unsigned int index;
	vector<Node*> &nodes = system.getNodes();
	for(index = 0; index < nodes.size(); index++) {
		Node* node = nodes[index];
		if (node->isGroupNode()) {
			GroupNode * groupNode = (GroupNode*)node;
			groupNode->ungroup(system);
		}
	}
}

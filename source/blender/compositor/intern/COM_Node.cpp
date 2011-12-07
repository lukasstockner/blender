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

#include "COM_Node.h"
#include "string.h"

#include "COM_NodeOperation.h"
#include "BKE_node.h"
#include "COM_SetValueOperation.h"
#include "COM_SetVectorOperation.h"
#include "COM_SetColorOperation.h"
#include "COM_SocketConnection.h"
#include "COM_ExecutionSystem.h"
#include "COM_PreviewOperation.h"
#include "COM_TranslateOperation.h"

//#include "stdio.h"
#include "COM_defines.h"

Node::Node(bNode* editorNode) {
    this->editorNode = editorNode;

    bNodeSocket * input = (bNodeSocket*)editorNode->inputs.first;
    while (input != NULL) {
		DataType dt = COM_DT_VALUE;
        if (input->type == SOCK_RGBA) dt = COM_DT_COLOR;
        if (input->type == SOCK_VECTOR) dt = COM_DT_VECTOR;

		this->addInputSocket(dt, (InputSocketResizeMode)input->resizemode, input);
        input = (bNodeSocket*)input->next;
    }
    bNodeSocket *output = (bNodeSocket*)editorNode->outputs.first;
    while(output != NULL) {
		DataType dt = COM_DT_VALUE;
        if (output->type == SOCK_RGBA) dt = COM_DT_COLOR;
        if (output->type == SOCK_VECTOR) dt = COM_DT_VECTOR;

        this->addOutputSocket(dt, output);
        output = (bNodeSocket*)output->next;
    }
}
Node::Node() {
    this->editorNode = NULL;
}

bNode* Node::getbNode() {return this->editorNode;}

void Node::addSetValueOperation(ExecutionSystem *graph, InputSocket* inputsocket, int editorNodeInputSocketIndex) {
    bNodeSocket *bSock = (bNodeSocket*)this->getEditorInputSocket(editorNodeInputSocketIndex);
    SetValueOperation *operation = new SetValueOperation();
    operation->setValue(bSock->ns.vec[0]);
	this->addLink(graph, operation->getOutputSocket(), inputsocket);
    graph->addOperation(operation);
}

void Node::addPreviewOperation(ExecutionSystem *system, OutputSocket *outputSocket, int priority) {
	PreviewOperation *operation = new PreviewOperation();
	system->addOperation(operation);
	operation->setbNode(this->getbNode());
	operation->setbNodeTree(system->getContext().getbNodeTree());
	operation->setPriority(priority);
	this->addLink(system, outputSocket, operation->getInputSocket(0));
}

void Node::addPreviewOperation(ExecutionSystem *system, InputSocket *inputSocket, int priority) {
    if (inputSocket->isConnected()) {
        OutputSocket *outputsocket = inputSocket->getConnection()->getFromSocket();
		this->addPreviewOperation(system, outputsocket, priority);
    }
}

SocketConnection* Node::addLink(ExecutionSystem *graph, OutputSocket* outputSocket, InputSocket* inputsocket) {
	if (inputsocket->isConnected()) {
		return NULL;
	}
	SocketConnection *connection = new SocketConnection();
	connection->setFromSocket(outputSocket);
	outputSocket->addConnection(connection);
	connection->setToSocket(inputsocket);
	inputsocket->setConnection(connection);
	graph->addSocketConnection(connection);
	return connection;
}

void Node::addSetColorOperation(ExecutionSystem *graph, InputSocket* inputsocket, int editorNodeInputSocketIndex) {
	bNodeSocket *bSock = (bNodeSocket*)this->getEditorInputSocket(editorNodeInputSocketIndex);
	SetColorOperation *operation = new SetColorOperation();
	operation->setChannel1(bSock->ns.vec[0]);
	operation->setChannel2(bSock->ns.vec[1]);
	operation->setChannel3(bSock->ns.vec[2]);
	operation->setChannel4(bSock->ns.vec[3]);
	this->addLink(graph, operation->getOutputSocket(), inputsocket);
	graph->addOperation(operation);
}

void Node::addSetVectorOperation(ExecutionSystem *graph, InputSocket* inputsocket, int editorNodeInputSocketIndex) {
	bNodeSocket *bSock = (bNodeSocket*)this->getEditorInputSocket(editorNodeInputSocketIndex);
	SetVectorOperation *operation = new SetVectorOperation();
	operation->setX(bSock->ns.vec[0]);
	operation->setY(bSock->ns.vec[1]);
	operation->setZ(bSock->ns.vec[2]);
	operation->setW(bSock->ns.vec[3]);
	this->addLink(graph, operation->getOutputSocket(), inputsocket);
	graph->addOperation(operation);
}

bNodeSocket* Node::getEditorInputSocket(int editorNodeInputSocketIndex) {
	bNodeSocket *bSock = (bNodeSocket*)this->getbNode()->inputs.first;
	int index = 0;
	while (bSock != NULL) {
		if (index == editorNodeInputSocketIndex) {
			return bSock;
		}
		index++;
		bSock = bSock->next;
	}
	return NULL;
}
bNodeSocket* Node::getEditorOutputSocket(int editorNodeInputSocketIndex) {
	bNodeSocket *bSock = (bNodeSocket*)this->getbNode()->outputs.first;
	int index = 0;
	while (bSock != NULL) {
		if (index == editorNodeInputSocketIndex) {
			return bSock;
		}
		index++;
		bSock = bSock->next;
	}
	return NULL;
}

InputSocket* Node::findInputSocketBybNodeSocket(bNodeSocket* socket) {
	vector<InputSocket*> &inputsockets = this->getInputSockets();
	unsigned int index;
	for (index = 0 ; index < inputsockets.size(); index ++) {
		InputSocket* input = inputsockets[index];
		if (input->getbNodeSocket() == socket) {
			return input;
		}
	}
	if (this->isGroupNode()) {
		vector<OutputSocket*> &outputsockets = this->getOutputSockets();
		for (index = 0 ; index < outputsockets.size(); index ++) {
			OutputSocket* output = outputsockets[index];
			if (output->getGroupInputSocket() != NULL) {
				if (output->getGroupInputSocket()->getbNodeSocket() == socket) {
					return output->getGroupInputSocket();
				}
			}
		}
	}
	return NULL;
}

OutputSocket* Node::findOutputSocketBybNodeSocket(bNodeSocket* socket) {
	vector<OutputSocket*> &outputsockets = this->getOutputSockets();
	unsigned int index;
	for (index = 0 ; index < outputsockets.size(); index ++) {
		OutputSocket* output = outputsockets[index];
		if (output->getbNodeSocket() == socket) {
			return output;
		}
	}
	if (this->isGroupNode()) {
		vector<InputSocket*> &inputsockets = this->getInputSockets();
		for (index = 0 ; index < inputsockets.size(); index ++) {
			InputSocket* input = inputsockets[index];
			if (input->getGroupOutputSocket() != NULL) {
				if (input->getGroupOutputSocket()->getbNodeSocket() == socket) {
					return input->getGroupOutputSocket();
				}
			}
		}
	}
	return NULL;
}

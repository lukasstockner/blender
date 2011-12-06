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

#include "COM_SocketProxyNode.h"
#include "COM_SocketConnection.h"
#include "stdio.h"
#include "COM_SocketProxyOperation.h"
#include "COM_ExecutionSystem.h"

SocketProxyNode::SocketProxyNode(bNode *editorNode): Node(editorNode) {
	this->clearInputAndOutputSockets();
	this->addInputSocket(COM_DT_COLOR);
	this->addOutputSocket(COM_DT_COLOR);
}

void SocketProxyNode::convertToOperations(ExecutionSystem *graph, CompositorContext * context) {
	OutputSocket * outputsocket = this->getOutputSocket(0);
	if (outputsocket->isConnected()) {
		SocketProxyOperation *operation = new SocketProxyOperation();
		this->getInputSocket(0)->relinkConnections(operation->getInputSocket(0));
		this->getOutputSocket(0)->relinkConnections(operation->getOutputSocket(0));
		graph->addOperation(operation);
	}
}
void SocketProxyNode::clearInputAndOutputSockets() {
	this->getInputSockets().clear();
	this->getOutputSockets().clear();
}

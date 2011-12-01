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

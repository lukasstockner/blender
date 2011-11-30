#include "COM_SwitchNode.h"
#include "COM_ExecutionSystem.h"
#include "COM_SocketProxyOperation.h"

SwitchNode::SwitchNode(bNode *editorNode): Node(editorNode) {
}


void SwitchNode::convertToOperations(ExecutionSystem *graph, CompositorContext * context) {
	SocketProxyOperation * operation = new SocketProxyOperation();
	int switchFrame = this->getbNode()->custom1;

	if (!switchFrame) {
		this->getInputSocket(0)->relinkConnections(operation->getInputSocket(0), true, 0, graph);
	} else {
		this->getInputSocket(1)->relinkConnections(operation->getInputSocket(0), true, 1, graph);
	}
	this->getOutputSocket(0)->relinkConnections(operation->getOutputSocket());

	graph->addOperation(operation);
}

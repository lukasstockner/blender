#include "COM_ValueNode.h"
#include "DNA_scene_types.h"
#include "COM_SetValueOperation.h"
#include "COM_ExecutionSystem.h"

ValueNode::ValueNode(bNode *editorNode): Node(editorNode) {
}

void ValueNode::convertToOperations(ExecutionSystem *graph, CompositorContext * context) {
	SetValueOperation *operation = new SetValueOperation();
	bNodeSocket* socket = this->getEditorOutputSocket(0);
	this->getOutputSocket(0)->relinkConnections(operation->getOutputSocket());
	operation->setValue(socket->ns.vec[0]);
	graph->addOperation(operation);
}

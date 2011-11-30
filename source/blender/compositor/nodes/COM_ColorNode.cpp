#include "COM_ColorNode.h"
#include "DNA_scene_types.h"
#include "COM_SetColorOperation.h"
#include "COM_ExecutionSystem.h"

ColorNode::ColorNode(bNode *editorNode): Node(editorNode) {
}

void ColorNode::convertToOperations(ExecutionSystem *graph, CompositorContext * context) {
	SetColorOperation *operation = new SetColorOperation();
	bNodeSocket* socket = this->getEditorOutputSocket(0);
	this->getOutputSocket(0)->relinkConnections(operation->getOutputSocket());
	operation->setChannels(socket->ns.vec);
	graph->addOperation(operation);
}

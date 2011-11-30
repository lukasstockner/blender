#include "COM_DilateErodeNode.h"
#include "DNA_scene_types.h"
#include "COM_ExecutionSystem.h"
#include "COM_DilateErodeOperation.h"

DilateErodeNode::DilateErodeNode(bNode *editorNode): Node(editorNode) {
}

void DilateErodeNode::convertToOperations(ExecutionSystem *graph, CompositorContext * context) {
	bNode* editorNode = this->getbNode();
	DilateErodeOperation *operation = new DilateErodeOperation();
	operation->setDistance(editorNode->custom2);

	this->getInputSocket(0)->relinkConnections(operation->getInputSocket(0));
	this->getOutputSocket(0)->relinkConnections(operation->getOutputSocket(0));

	graph->addOperation(operation);
}

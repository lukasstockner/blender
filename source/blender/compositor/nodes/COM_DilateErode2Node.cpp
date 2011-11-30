#include "COM_DilateErode2Node.h"
#include "DNA_scene_types.h"
#include "COM_ExecutionSystem.h"
#include "COM_DilateErodeOperation.h"

DilateErode2Node::DilateErode2Node(bNode *editorNode): Node(editorNode) {
}

void DilateErode2Node::convertToOperations(ExecutionSystem *graph, CompositorContext * context) {
	bNode* editorNode = this->getbNode();
	NodeDilateErode * data = (NodeDilateErode*)editorNode->storage;
	DilateErodeOperation *operation = new DilateErodeOperation();

	this->getInputSocket(0)->relinkConnections(operation->getInputSocket(0));
	this->getOutputSocket(0)->relinkConnections(operation->getOutputSocket(0));

	operation->setData(data);

	graph->addOperation(operation);
}

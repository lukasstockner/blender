#include "COM_BokehImageNode.h"
#include "DNA_scene_types.h"
#include "COM_BokehImageOperation.h"
#include "COM_ExecutionSystem.h"

BokehImageNode::BokehImageNode(bNode *editorNode): Node(editorNode) {
}

void BokehImageNode::convertToOperations(ExecutionSystem *graph, CompositorContext * context) {
	BokehImageOperation *operation = new BokehImageOperation();
    this->getOutputSocket(0)->relinkConnections(operation->getOutputSocket(0));
    graph->addOperation(operation);
	operation->setData((NodeBokehImage*)this->getbNode()->storage);
	addPreviewOperation(graph, operation->getOutputSocket(0), 9);
}

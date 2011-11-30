#include "COM_DirectionalBlurNode.h"
#include "DNA_scene_types.h"
#include "DNA_node_types.h"
#include "COM_ExecutionSystem.h"
#include "COM_DirectionalBlurOperation.h"

DirectionalBlurNode::DirectionalBlurNode(bNode *editorNode): Node(editorNode) {
}

void DirectionalBlurNode::convertToOperations(ExecutionSystem *graph, CompositorContext * context) {
	NodeDBlurData *data = (NodeDBlurData*)this->getbNode()->storage;
	DirectionalBlurOperation *operation = new DirectionalBlurOperation();
	operation->setQuality(context->getQuality());
	operation->setData(data);
	this->getInputSocket(0)->relinkConnections(operation->getInputSocket(0), true, 0, graph);
	this->getOutputSocket(0)->relinkConnections(operation->getOutputSocket());
	graph->addOperation(operation);
}

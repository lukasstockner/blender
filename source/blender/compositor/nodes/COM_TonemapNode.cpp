#include "COM_TonemapNode.h"
#include "DNA_scene_types.h"
#include "COM_TonemapOperation.h"
#include "COM_ExecutionSystem.h"

TonemapNode::TonemapNode(bNode *editorNode): Node(editorNode) {
}

void TonemapNode::convertToOperations(ExecutionSystem *system, CompositorContext * context) {
	NodeTonemap* data = (NodeTonemap*)this->getbNode()->storage;
	TonemapOperation *operation = data->type==1?new PhotoreceptorTonemapOperation():new TonemapOperation();

	operation->setData(data);
	this->getInputSocket(0)->relinkConnections(operation->getInputSocket(0), true, 0, system);
    this->getOutputSocket(0)->relinkConnections(operation->getOutputSocket(0));
	system->addOperation(operation);
}

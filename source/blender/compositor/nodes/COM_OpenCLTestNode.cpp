#include "COM_OpenCLTestNode.h"
#include "DNA_scene_types.h"
#include "COM_OpenCLTestOperation.h"
#include "COM_ExecutionSystem.h"

OpenCLTestNode::OpenCLTestNode(bNode *editorNode): Node(editorNode) {
}

void OpenCLTestNode::convertToOperations(ExecutionSystem *system, CompositorContext * context) {
	OpenCLTestOperation *operation = new OpenCLTestOperation();
	this->getOutputSocket(0)->relinkConnections(operation->getOutputSocket());
	system->addOperation(operation);
}

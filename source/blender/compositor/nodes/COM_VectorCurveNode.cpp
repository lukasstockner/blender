#include "COM_VectorCurveNode.h"
#include "DNA_scene_types.h"
#include "COM_VectorCurveOperation.h"
#include "COM_ExecutionSystem.h"

VectorCurveNode::VectorCurveNode(bNode *editorNode): Node(editorNode) {
}

void VectorCurveNode::convertToOperations(ExecutionSystem *graph, CompositorContext * context) {
	VectorCurveOperation *operation = new VectorCurveOperation();

	this->getInputSocket(0)->relinkConnections(operation->getInputSocket(0), true, 0, graph);
	this->getOutputSocket(0)->relinkConnections(operation->getOutputSocket());

	operation->setCurveMapping((CurveMapping*)this->getbNode()->storage);

    graph->addOperation(operation);
}

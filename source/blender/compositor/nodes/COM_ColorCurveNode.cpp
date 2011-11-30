#include "COM_ColorCurveNode.h"
#include "DNA_scene_types.h"
#include "COM_ColorCurveOperation.h"
#include "COM_ExecutionSystem.h"

ColorCurveNode::ColorCurveNode(bNode *editorNode): Node(editorNode) {
}

void ColorCurveNode::convertToOperations(ExecutionSystem *graph, CompositorContext * context) {
	ColorCurveOperation *operation = new ColorCurveOperation();

	this->getInputSocket(0)->relinkConnections(operation->getInputSocket(0), true, 0, graph);
	this->getInputSocket(1)->relinkConnections(operation->getInputSocket(1), true, 1, graph);
	this->getInputSocket(2)->relinkConnections(operation->getInputSocket(2), true, 2, graph);
	this->getInputSocket(3)->relinkConnections(operation->getInputSocket(3), true, 3, graph);

	this->getOutputSocket(0)->relinkConnections(operation->getOutputSocket());

	operation->setCurveMapping((CurveMapping*)this->getbNode()->storage);

    graph->addOperation(operation);
}

#include "COM_RotateNode.h"

#include "COM_RotateOperation.h"
#include "COM_ExecutionSystem.h"

RotateNode::RotateNode(bNode *editorNode) : Node(editorNode) {
}

void RotateNode::convertToOperations(ExecutionSystem *graph, CompositorContext * context) {
    InputSocket *inputSocket = this->getInputSocket(0);
    InputSocket *inputDegreeSocket = this->getInputSocket(1);
    OutputSocket *outputSocket = this->getOutputSocket(0);
    RotateOperation *operation = new RotateOperation();

    inputSocket->relinkConnections(operation->getInputSocket(0), true, 0, graph);
	operation->setDegree(inputDegreeSocket->getStaticValues()[0]);
    inputDegreeSocket->relinkConnections(operation->getInputSocket(1), true, 1, graph);
    outputSocket->relinkConnections(operation->getOutputSocket(0));
    graph->addOperation(operation);
}

#include "COM_ScaleNode.h"

#include "COM_ScaleOperation.h"
#include "COM_ExecutionSystem.h"

ScaleNode::ScaleNode(bNode *editorNode) : Node(editorNode) {
}

void ScaleNode::convertToOperations(ExecutionSystem *graph, CompositorContext * context) {
    InputSocket *inputSocket = this->getInputSocket(0);
    InputSocket *inputXSocket = this->getInputSocket(1);
    InputSocket *inputYSocket = this->getInputSocket(2);
    OutputSocket *outputSocket = this->getOutputSocket(0);
    ScaleOperation *operation = new ScaleOperation();

    inputSocket->relinkConnections(operation->getInputSocket(0), true, 0, graph);
    inputXSocket->relinkConnections(operation->getInputSocket(1), true, 1, graph);
    inputYSocket->relinkConnections(operation->getInputSocket(2), true, 2, graph);
    outputSocket->relinkConnections(operation->getOutputSocket(0));
    graph->addOperation(operation);
}

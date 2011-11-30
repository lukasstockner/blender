#include "COM_TranslateNode.h"

#include "COM_TranslateOperation.h"
#include "COM_ExecutionSystem.h"

TranslateNode::TranslateNode(bNode *editorNode) : Node(editorNode) {
}

void TranslateNode::convertToOperations(ExecutionSystem *graph, CompositorContext * context) {
    InputSocket *inputSocket = this->getInputSocket(0);
    InputSocket *inputXSocket = this->getInputSocket(1);
    InputSocket *inputYSocket = this->getInputSocket(2);
    OutputSocket *outputSocket = this->getOutputSocket(0);
    TranslateOperation *operation = new TranslateOperation();

    inputSocket->relinkConnections(operation->getInputSocket(0), true, 0, graph);
    inputXSocket->relinkConnections(operation->getInputSocket(1), true, 1, graph);
    inputYSocket->relinkConnections(operation->getInputSocket(2), true, 2, graph);
    outputSocket->relinkConnections(operation->getOutputSocket(0));
    graph->addOperation(operation);
}

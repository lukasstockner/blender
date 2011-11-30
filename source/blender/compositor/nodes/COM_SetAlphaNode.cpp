#include "COM_SetAlphaNode.h"
#include "COM_SetAlphaOperation.h"
#include "COM_ExecutionSystem.h"

void SetAlphaNode::convertToOperations(ExecutionSystem *graph, CompositorContext * context) {
    SetAlphaOperation* operation = new SetAlphaOperation();

    this->getInputSocket(0)->relinkConnections(operation->getInputSocket(0), true, 0, graph);
    this->getInputSocket(1)->relinkConnections(operation->getInputSocket(1), true, 0, graph);
    this->getOutputSocket(0)->relinkConnections(operation->getOutputSocket());

    graph->addOperation(operation);
}

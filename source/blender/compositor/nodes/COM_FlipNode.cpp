#include "COM_FlipNode.h"

#include "COM_FlipOperation.h"
#include "COM_ExecutionSystem.h"

FlipNode::FlipNode(bNode *editorNode) : Node(editorNode) {
//    this->addInputSocket(*(new InputSocket(COM_DT_COLOR)));
//    this->addOutputSocket(*(new OutputSocket(COM_DT_COLOR)));
}

void FlipNode::convertToOperations(ExecutionSystem *graph, CompositorContext * context) {
    InputSocket *inputSocket = this->getInputSocket(0);
    OutputSocket *outputSocket = this->getOutputSocket(0);
    FlipOperation *operation = new FlipOperation();
    switch (this->getbNode()->custom1) {
    case 0: /// @TODO: I didn't find any constants in the old implementation, should I introduce them.
        operation->setFlipX(true);
        operation->setFlipY(false);
        break;
    case 1:
        operation->setFlipX(false);
        operation->setFlipY(true);
        break;
    case 2:
        operation->setFlipX(true);
        operation->setFlipY(true);
        break;
    }

    inputSocket->relinkConnections(operation->getInputSocket(0), true, 0, graph);
    outputSocket->relinkConnections(operation->getOutputSocket(0));
    graph->addOperation(operation);
}

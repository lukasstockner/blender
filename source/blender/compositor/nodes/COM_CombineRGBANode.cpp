#include "COM_CombineRGBANode.h"

#include "COM_CombineChannelsOperation.h"

#include "COM_ExecutionSystem.h"
#include "COM_SetValueOperation.h"
#include "DNA_material_types.h" // the ramp types


CombineRGBANode::CombineRGBANode(bNode *editorNode): Node(editorNode) {
}


void CombineRGBANode::convertToOperations(ExecutionSystem *graph, CompositorContext * context) {
    InputSocket *inputRSocket = this->getInputSocket(0);
    InputSocket *inputGSocket = this->getInputSocket(1);
    InputSocket *inputBSocket = this->getInputSocket(2);
    InputSocket *inputASocket = this->getInputSocket(3);
    OutputSocket *outputSocket = this->getOutputSocket(0);

    CombineChannelsOperation *operation = new CombineChannelsOperation();
    if (inputRSocket->isConnected()) {
        operation->setResolutionInputSocketIndex(0);
    } else if (inputGSocket->isConnected()) {
        operation->setResolutionInputSocketIndex(1);
    } else if (inputBSocket->isConnected()) {
        operation->setResolutionInputSocketIndex(2);
    } else {
        operation->setResolutionInputSocketIndex(3);
    }
    inputRSocket->relinkConnections(operation->getInputSocket(0), true, 0, graph);
    inputGSocket->relinkConnections(operation->getInputSocket(1), true, 1, graph);
    inputBSocket->relinkConnections(operation->getInputSocket(2), true, 2, graph);
    inputASocket->relinkConnections(operation->getInputSocket(3), true, 3, graph);
    outputSocket->relinkConnections(operation->getOutputSocket(0));
    graph->addOperation(operation);
}

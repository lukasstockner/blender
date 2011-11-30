#include "COM_SeparateHSVANode.h"

#include "COM_SeparateChannelOperation.h"
#include "COM_ExecutionSystem.h"
#include "COM_SetValueOperation.h"
#include "COM_ConvertRGBToHSVOperation.h"

SeparateHSVANode::SeparateHSVANode(bNode *editorNode): SeparateRGBANode(editorNode) {
}

void SeparateHSVANode::convertToOperations(ExecutionSystem *graph, CompositorContext * context) {
    ConvertRGBToHSVOperation *operation = new ConvertRGBToHSVOperation();
    InputSocket *inputSocket = this->getInputSocket(0);
    if (inputSocket->isConnected()) {
        inputSocket->relinkConnections(operation->getInputSocket(0));
        addLink(graph, operation->getOutputSocket(), inputSocket);
    }
    graph->addOperation(operation);
	SeparateRGBANode::convertToOperations(graph, context);
}

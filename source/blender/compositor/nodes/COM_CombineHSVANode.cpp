#include "COM_CombineHSVANode.h"

#include "COM_CombineChannelsOperation.h"

#include "COM_ExecutionSystem.h"
#include "COM_SetValueOperation.h"
#include "COM_ConvertHSVToRGBOperation.h"

CombineHSVANode::CombineHSVANode(bNode *editorNode): CombineRGBANode(editorNode) {
}

void CombineHSVANode::convertToOperations(ExecutionSystem *graph, CompositorContext * context) {
    ConvertHSVToRGBOperation *operation = new ConvertHSVToRGBOperation();
    OutputSocket *outputSocket = this->getOutputSocket(0);
    if (outputSocket->isConnected()) {
        outputSocket->relinkConnections(operation->getOutputSocket());
        addLink(graph, outputSocket, operation->getInputSocket(0));
    }
    graph->addOperation(operation);
	CombineRGBANode::convertToOperations(graph, context);
}

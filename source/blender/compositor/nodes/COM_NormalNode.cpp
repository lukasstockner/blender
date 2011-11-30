#include "COM_NormalNode.h"
#include "COM_ExecutionSystem.h"
#include "BKE_node.h"
#include "COM_DotproductOperation.h"
#include "COM_SetVectorOperation.h"

NormalNode::NormalNode(bNode* editorNode): Node(editorNode)
{}

void NormalNode::convertToOperations(ExecutionSystem *graph, CompositorContext * context) {
    InputSocket *inputSocket = this->getInputSocket(0);
    OutputSocket *outputSocket = this->getOutputSocket(0);
    OutputSocket *outputSocketDotproduct = this->getOutputSocket(1);
    bNode* editorNode = this->getbNode();

    SetVectorOperation * operationSet = new SetVectorOperation();
    bNodeSocket * insock = (bNodeSocket*)editorNode->outputs.first;
    operationSet->setX(insock->ns.vec[0]);
    operationSet->setY(insock->ns.vec[1]);
    operationSet->setZ(insock->ns.vec[2]);
    operationSet->setW(0.0f);

    outputSocket->relinkConnections(operationSet->getOutputSocket(0));
    graph->addOperation(operationSet);

    if (outputSocketDotproduct->isConnected()) {
        DotproductOperation *operation = new DotproductOperation();
        outputSocketDotproduct->relinkConnections(operation->getOutputSocket(0));
        inputSocket->relinkConnections(operation->getInputSocket(0), true, 0, graph);
        addLink(graph, operationSet->getOutputSocket(0), operation->getInputSocket(1));
        graph->addOperation(operation);
    }
}

#include "COM_DifferenceMatteNode.h"
#include "COM_ExecutionSystem.h"
#include "BKE_node.h"
#include "COM_DifferenceMatteOperation.h"
#include "COM_SetAlphaOperation.h"

DifferenceMatteNode::DifferenceMatteNode(bNode* editorNode): Node(editorNode)
{}

void DifferenceMatteNode::convertToOperations(ExecutionSystem *graph, CompositorContext * context) {
	InputSocket *inputSocket = this->getInputSocket(0);
	InputSocket *inputSocket2 = this->getInputSocket(1);
	OutputSocket *outputSocketImage = this->getOutputSocket(0);
	OutputSocket *outputSocketMatte = this->getOutputSocket(1);
    bNode* editorNode = this->getbNode();

	DifferenceMatteOperation * operationSet = new DifferenceMatteOperation();
	operationSet->setSettings((NodeChroma*)editorNode->storage);
	inputSocket->relinkConnections(operationSet->getInputSocket(0), true, 0, graph);
	inputSocket2->relinkConnections(operationSet->getInputSocket(1), true, 1, graph);

	outputSocketMatte->relinkConnections(operationSet->getOutputSocket(0));
	graph->addOperation(operationSet);

	if (outputSocketImage->isConnected()) {
		SetAlphaOperation * operation = new SetAlphaOperation();
		addLink(graph, operationSet->getInputSocket(0)->getConnection()->getFromSocket(), operation->getInputSocket(0));
		addLink(graph, operationSet->getOutputSocket(), operation->getInputSocket(1));
		outputSocketImage->relinkConnections(operation->getOutputSocket());
		graph->addOperation(operation);
	}
}

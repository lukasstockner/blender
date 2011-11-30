#include "COM_BlurNode.h"
#include "DNA_scene_types.h"
#include "DNA_node_types.h"
#include "COM_GaussianXBlurOperation.h"
#include "COM_GaussianYBlurOperation.h"
#include "COM_ExecutionSystem.h"
#include "COM_GaussianBokehBlurOperation.h"

BlurNode::BlurNode(bNode *editorNode): Node(editorNode) {
}

void BlurNode::convertToOperations(ExecutionSystem *graph, CompositorContext * context) {
    bNode* editorNode = this->getbNode();
	NodeBlurData * data = (NodeBlurData*)editorNode->storage;
	const bNodeSocket *sock = this->getInputSocket(1)->getbNodeSocket();
	const float size = sock->ns.vec[0];
	if (!data->bokeh) {
		GaussianXBlurOperation *operationx = new GaussianXBlurOperation();
		operationx->setData(data);
		operationx->setQuality(context->getQuality());
		this->getInputSocket(0)->relinkConnections(operationx->getInputSocket(0), true, 0, graph);
		operationx->setSize(size);
		graph->addOperation(operationx);
		GaussianYBlurOperation *operationy = new GaussianYBlurOperation();
		operationy->setData(data);
		operationy->setQuality(context->getQuality());
		this->getOutputSocket(0)->relinkConnections(operationy->getOutputSocket());
		operationy->setSize(size);
		graph->addOperation(operationy);
		addLink(graph, operationx->getOutputSocket(), operationy->getInputSocket(0));
		addPreviewOperation(graph, operationy->getOutputSocket(), 5);
	} else {
		GaussianBokehBlurOperation *operation = new GaussianBokehBlurOperation();
		operation->setData(data);
		this->getInputSocket(0)->relinkConnections(operation->getInputSocket(0), true, 0, graph);
		operation->setSize(size);
		operation->setQuality(context->getQuality());
		graph->addOperation(operation);
		this->getOutputSocket(0)->relinkConnections(operation->getOutputSocket());
		addPreviewOperation(graph, operation->getOutputSocket(), 5);
	}
}

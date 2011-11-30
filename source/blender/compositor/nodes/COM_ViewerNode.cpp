#include "COM_ViewerNode.h"
#include "BKE_global.h"

#include "COM_ViewerOperation.h"
#include "COM_ExecutionSystem.h"

ViewerNode::ViewerNode(bNode *editorNode): Node(editorNode) {
}

void ViewerNode::convertToOperations(ExecutionSystem *graph, CompositorContext * context) {
	InputSocket *imageSocket = this->getInputSocket(0);
	InputSocket *alphaSocket = this->getInputSocket(1);
	Image* image = (Image*)this->getbNode()->id;
	ImageUser * imageUser = (ImageUser*) this->getbNode()->storage;
	if (imageSocket->isConnected()) {
		bNode* editorNode = this->getbNode();
		ViewerOperation *viewerOperation = new ViewerOperation();
		viewerOperation->setColorManagement( context->getScene()->r.color_mgt_flag);
		viewerOperation->setbNodeTree(context->getbNodeTree());
		viewerOperation->setImage(image);
		viewerOperation->setImageUser(imageUser);
		viewerOperation->setActive(editorNode->flag & NODE_DO_OUTPUT);
		viewerOperation->setChunkOrder((OrderOfChunks)editorNode->custom1);
		viewerOperation->setCenterX(editorNode->custom3);
		viewerOperation->setCenterY(editorNode->custom4);
		imageSocket->relinkConnections(viewerOperation->getInputSocket(0), true, 0, graph);
		alphaSocket->relinkConnections(viewerOperation->getInputSocket(1));
		graph->addOperation(viewerOperation);
		addPreviewOperation(graph, viewerOperation->getInputSocket(0), 0);
	}
}

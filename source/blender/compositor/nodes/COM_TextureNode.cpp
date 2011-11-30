#include "COM_TextureNode.h"
#include "COM_ExecutionSystem.h"
#include "COM_TextureOperation.h"

TextureNode::TextureNode(bNode *editorNode): Node(editorNode) {
}

void TextureNode::convertToOperations(ExecutionSystem *graph, CompositorContext * context) {
	bNode* editorNode = this->getbNode();
	Tex* texture = (Tex*)editorNode->id;
	TextureOperation* operation = new TextureOperation();
	this->getOutputSocket(1)->relinkConnections(operation->getOutputSocket());
	operation->setTextureOffset(this->getInputSocket(0)->getStaticValues());
	operation->setTextureSize(this->getInputSocket(1)->getStaticValues());
	operation->setTexture(texture);
	graph->addOperation(operation);
	addPreviewOperation(graph, operation->getOutputSocket(), 9);

	if (this->getOutputSocket(0)->isConnected()) {
		TextureAlphaOperation* operation = new TextureAlphaOperation();
		this->getOutputSocket(0)->relinkConnections(operation->getOutputSocket());
		operation->setTextureOffset(this->getInputSocket(0)->getStaticValues());
		operation->setTextureSize(this->getInputSocket(1)->getStaticValues());
		operation->setTexture(texture);
		graph->addOperation(operation);
	}
}

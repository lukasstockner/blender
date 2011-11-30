#include "COM_ImageNode.h"
#include "COM_ExecutionSystem.h"
#include "COM_ImageOperation.h"

ImageNode::ImageNode(bNode *editorNode): Node(editorNode) {
}

void ImageNode::convertToOperations(ExecutionSystem *graph, CompositorContext * context) {
	/// Image output
    OutputSocket *outputImage = this->getOutputSocket(0);
    bNode *editorNode = this->getbNode();
    Image *image = (Image*)editorNode->id;
    ImageUser *imageuser = (ImageUser*)editorNode->storage;


	ImageOperation *operation = new ImageOperation();
	if (outputImage->isConnected()) {
		outputImage->relinkConnections(operation->getOutputSocket());
	}
	operation->setImage(image);
	operation->setImageUser(imageuser);
	operation->setFramenumber(context->getFramenumber());
	operation->setInterpolationMode(context->getQuality() == COM_QUALITY_LOW?COM_IM_NEAREST:COM_IM_LINEAR);
	graph->addOperation(operation);
	addPreviewOperation(graph, operation->getOutputSocket(), 9);

	OutputSocket *alphaImage = this->getOutputSocket(1);
	if (alphaImage->isConnected()) {
		ImageAlphaOperation *alphaOperation = new ImageAlphaOperation();
		alphaOperation->setImage(image);
		alphaOperation->setImageUser(imageuser);
		alphaOperation->setFramenumber(context->getFramenumber());
		alphaOperation->setInterpolationMode(context->getQuality() == COM_QUALITY_LOW?COM_IM_NEAREST:COM_IM_LINEAR);
		alphaImage->relinkConnections(alphaOperation->getOutputSocket());
		graph->addOperation(alphaOperation);
	}
	/// @todo: ImageZOperation
}

#include "COM_LensDistortionNode.h"
#include "DNA_scene_types.h"
#include "COM_ExecutionSystem.h"
#include "COM_ProjectorLensDistortionOperation.h"
#include "COM_ScreenLensDistortionOperation.h"

LensDistortionNode::LensDistortionNode(bNode *editorNode): Node(editorNode) {
}

void LensDistortionNode::convertToOperations(ExecutionSystem *graph, CompositorContext * context) {
	bNode* editorNode = this->getbNode();
	NodeLensDist * data = (NodeLensDist*)editorNode->storage;
	if (data->proj) {
		ProjectorLensDistortionOperation *operation = new ProjectorLensDistortionOperation();

		this->getInputSocket(0)->relinkConnections(operation->getInputSocket(0), true, 0, graph);
		operation->setDispertion(this->getInputSocket(2)->getStaticValues()[0]);
		this->getOutputSocket(0)->relinkConnections(operation->getOutputSocket(0));

		operation->setData(data);
		graph->addOperation(operation);

	} else {
		ScreenLensDistortionOperation *operation = new ScreenLensDistortionOperation();

		this->getInputSocket(0)->relinkConnections(operation->getInputSocket(0), true, 0, graph);
		operation->setDistortion(this->getInputSocket(1)->getStaticValues()[0]);
		operation->setDispertion(this->getInputSocket(2)->getStaticValues()[0]);
		this->getOutputSocket(0)->relinkConnections(operation->getOutputSocket(0));

		operation->setData(data);
		graph->addOperation(operation);
	}

}

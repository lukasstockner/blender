#include "COM_LensFlareNode.h"
#include "DNA_scene_types.h"
#include "COM_LensGlowOperation.h"
#include "COM_LensGlowImageOperation.h"
#include "COM_BokehBlurOperation.h"
#include "COM_ExecutionSystem.h"
#include "DNA_camera_types.h"
#include "DNA_object_types.h"
#include "DNA_world_types.h"
#include "COM_LensGhostOperation.h"
#include "BLI_math.h"
#include "COM_SetColorOperation.h"

LensFlareNode::LensFlareNode(bNode *editorNode): Node(editorNode) {
}

void LensFlareNode::convertToOperations(ExecutionSystem *graph, CompositorContext * context) {


	Lamp* lamp = (Lamp*)this->getbNode()->id;
	Object *lampObject = NULL;

	for (Base* pos = (Base*)context->getScene()->base.first ; pos ; pos = (Base*)pos->next) {
		if (pos->object->type == OB_LAMP) {
			if (pos->object->data == lamp) {
				lampObject = pos->object;
				break;
			}
		}
	}
	Object *cameraObject = context->getScene()->camera;

	if (lampObject != NULL && cameraObject != NULL) {
		LensGhostProjectionOperation *operation=/*context->getQuality()==COM_QUALITY_LOW?new LensGhostProjectionOperation():*/new LensGhostOperation();

		operation->setLampObject(lampObject);
		operation->setCameraObject(cameraObject);

		operation->setQuality(context->getQuality());
		this->getOutputSocket(0)->relinkConnections(operation->getOutputSocket(0));
		this->getInputSocket(0)->relinkConnections(operation->getInputSocket(0), true, 0, graph);
		this->getInputSocket(2)->relinkConnections(operation->getInputSocket(1), true, 2, graph);

		graph->addOperation(operation);
	} else {
		SetColorOperation *operation = new SetColorOperation();
		operation->setChannel1(0.0f);
		operation->setChannel2(0.0f);
		operation->setChannel3(0.0f);
		operation->setChannel4(1.0f);
		this->getOutputSocket(0)->relinkConnections(operation->getOutputSocket());
		graph->addOperation(operation);

	}
}

#include "COM_TimeNode.h"
#include "DNA_scene_types.h"
#include "COM_SetValueOperation.h"
#include "COM_ExecutionSystem.h"
extern "C" {
	#include "BKE_colortools.h"
}
#include "BLI_utildefines.h"

TimeNode::TimeNode(bNode *editorNode): Node(editorNode) {
}

void TimeNode::convertToOperations(ExecutionSystem *graph, CompositorContext * context) {
	SetValueOperation *operation = new SetValueOperation();
	this->getOutputSocket(0)->relinkConnections(operation->getOutputSocket());
	bNode* node = this->getbNode();

	/* stack order output: fac */
	float fac= 0.0f;
	const int framenumber = context->getFramenumber();

	if (framenumber < node->custom1) {
		fac = 0.0f;
	} else if (framenumber > node->custom2) {
		fac = 1.0f;
	} else 	if(node->custom1 < node->custom2) {
		fac= (context->getFramenumber() - node->custom1)/(float)(node->custom2-node->custom1);
	}

	fac= curvemapping_evaluateF((CurveMapping*)node->storage, 0, fac);
	operation->setValue(CLAMPIS(fac, 0.0f, 1.0f));
	graph->addOperation(operation);
}

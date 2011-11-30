#include "COM_ZCombineNode.h"

#include "COM_ZCombineOperation.h"

#include "COM_ExecutionSystem.h"
#include "COM_SetValueOperation.h"
#include "COM_MathBaseOperation.h"

#include "DNA_material_types.h" // the ramp types

void ZCombineNode::convertToOperations(ExecutionSystem* system, CompositorContext * context) {
	if (this->getOutputSocket(0)->isConnected()) {
		ZCombineOperation * operation = NULL;
		if (this->getbNode()->custom1) {
			operation = new ZCombineAlphaOperation();
		} else {
			operation = new ZCombineOperation();
		}

		this->getInputSocket(0)->relinkConnections(operation->getInputSocket(0), true, 0, system);
		this->getInputSocket(1)->relinkConnections(operation->getInputSocket(1), true, 1, system);
		this->getInputSocket(2)->relinkConnections(operation->getInputSocket(2), true, 2, system);
		this->getInputSocket(3)->relinkConnections(operation->getInputSocket(3), true, 3, system);
		this->getOutputSocket(0)->relinkConnections(operation->getOutputSocket());
		system->addOperation(operation);
		if (this->getOutputSocket(1)->isConnected()) {
			MathMinimumOperation * zoperation = new MathMinimumOperation();
			addLink(system, operation->getInputSocket(1)->getConnection()->getFromSocket(), zoperation->getInputSocket(0));
			addLink(system, operation->getInputSocket(3)->getConnection()->getFromSocket(), zoperation->getInputSocket(1));
			this->getOutputSocket(1)->relinkConnections(zoperation->getOutputSocket());
			system->addOperation(zoperation);
		}
	} else {
		if (this->getOutputSocket(1)->isConnected()) {
			MathMinimumOperation * zoperation = new MathMinimumOperation();
			this->getInputSocket(1)->relinkConnections(zoperation->getInputSocket(0), true, 1, system);
			this->getInputSocket(3)->relinkConnections(zoperation->getInputSocket(1), true, 3, system);
			this->getOutputSocket(1)->relinkConnections(zoperation->getOutputSocket());
			system->addOperation(zoperation);
		}
	}
}

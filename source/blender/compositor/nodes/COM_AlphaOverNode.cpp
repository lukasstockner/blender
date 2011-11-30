#include "COM_AlphaOverNode.h"

#include "COM_MixBaseOperation.h"
#include "COM_AlphaOverKeyOperation.h"
#include "COM_AlphaOverMixedOperation.h"
#include "COM_AlphaOverPremultiplyOperation.h"

#include "COM_ExecutionSystem.h"
#include "COM_SetValueOperation.h"
#include "DNA_material_types.h" // the ramp types

void AlphaOverNode::convertToOperations(ExecutionSystem *graph, CompositorContext * context) {
    InputSocket *valueSocket = this->getInputSocket(0);
    InputSocket *color1Socket = this->getInputSocket(1);
    InputSocket *color2Socket = this->getInputSocket(2);
    OutputSocket *outputSocket = this->getOutputSocket(0);
    bNode* editorNode = this->getbNode();

    MixBaseOperation *convertProg;
    NodeTwoFloats *ntf= (NodeTwoFloats*)editorNode->storage;
    if (ntf->x!= 0.0f) {
        AlphaOverMixedOperation *mixOperation  = new AlphaOverMixedOperation();
        mixOperation->setX(ntf->x);
        convertProg = mixOperation;

    } else if (editorNode->custom1) {
        convertProg = new AlphaOverKeyOperation();
    } else {
        convertProg = new AlphaOverPremultiplyOperation();
    }

    convertProg->setUseValueAlphaMultiply(false);
    if (color1Socket->isConnected()) {
        convertProg->setResolutionInputSocketIndex(1);
    } else if (color2Socket->isConnected()) {
        convertProg->setResolutionInputSocketIndex(2);
    } else {
        convertProg->setResolutionInputSocketIndex(0);
    }
    valueSocket->relinkConnections(convertProg->getInputSocket(0), true, 0, graph);
    color1Socket->relinkConnections(convertProg->getInputSocket(1), true, 1, graph);
    color2Socket->relinkConnections(convertProg->getInputSocket(2), true, 2, graph);
    outputSocket->relinkConnections(convertProg->getOutputSocket(0));
    graph->addOperation(convertProg);
}

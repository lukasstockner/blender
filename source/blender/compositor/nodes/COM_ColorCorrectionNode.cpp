#include "COM_ColorCorrectionNode.h"
#include "DNA_scene_types.h"
#include "COM_ColorCorrectionOperation.h"
#include "COM_ExecutionSystem.h"

ColorCorrectionNode::ColorCorrectionNode(bNode *editorNode): Node(editorNode) {
}

void ColorCorrectionNode::convertToOperations(ExecutionSystem *graph, CompositorContext * context) {
    ColorCorrectionOperation *operation = new ColorCorrectionOperation();
    bNode* editorNode = getbNode();
    this->getInputSocket(0)->relinkConnections(operation->getInputSocket(0),true, 0, graph);
    this->getInputSocket(1)->relinkConnections(operation->getInputSocket(1),true, 1, graph);
    this->getOutputSocket(0)->relinkConnections(operation->getOutputSocket(0));
    operation->setData((NodeColorCorrection*)editorNode->storage);
    operation->setRedChannelEnabled((editorNode->custom1&1)>0);
    operation->setGreenChannelEnabled((editorNode->custom1&2)>0);
    operation->setBlueChannelEnabled((editorNode->custom1&4)>0);
    graph->addOperation(operation);
}

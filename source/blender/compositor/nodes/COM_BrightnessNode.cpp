#include "COM_BrightnessNode.h"
#include "DNA_scene_types.h"
#include "COM_BrightnessOperation.h"
#include "COM_ExecutionSystem.h"

BrightnessNode::BrightnessNode(bNode *editorNode): Node(editorNode) {
//    this->addInputSocket(*(new InputSocket(COM_DT_COLOR)));
//    this->addInputSocket(*(new InputSocket("Bright", COM_DT_VALUE)));
//    this->addInputSocket(*(new InputSocket("Contrast", COM_DT_VALUE)));

//    this->addOutputSocket(*(new OutputSocket(COM_DT_COLOR)));

}
/// @todo: add anti alias when not FSA
void BrightnessNode::convertToOperations(ExecutionSystem *graph, CompositorContext * context) {
    BrightnessOperation *operation = new BrightnessOperation();

    this->getInputSocket(0)->relinkConnections(operation->getInputSocket(0));
    this->getInputSocket(1)->relinkConnections(operation->getInputSocket(1),true, 1, graph);
    this->getInputSocket(2)->relinkConnections(operation->getInputSocket(2),true, 2, graph);
    this->getOutputSocket(0)->relinkConnections(operation->getOutputSocket(0));
    graph->addOperation(operation);
}

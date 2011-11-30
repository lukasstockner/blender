#include "COM_IDMaskNode.h"
#include "DNA_scene_types.h"
#include "COM_IDMaskOperation.h"
#include "COM_ExecutionSystem.h"

IDMaskNode::IDMaskNode(bNode *editorNode): Node(editorNode) {
//    this->addInputSocket(*(new InputSocket("ID value", COM_DT_VALUE)));

//    this->addOutputSocket(*(new OutputSocket(COM_DT_VALUE)));

}
/// @todo: add anti alias when not FSA
void IDMaskNode::convertToOperations(ExecutionSystem *graph, CompositorContext * context) {
//    RenderData *data = (RenderData*)this->getEditorNode()->storage;
    IDMaskOperation *operation;
//    if (data->scemode & R_FULL_SAMPLE) {
//        operation = new IDMaskFSAOperation();
//    } else {
    operation = new IDMaskOperation();
//    }
    operation->setObjectIndex(this->getbNode()->custom1);

    this->getInputSocket(0)->relinkConnections(operation->getInputSocket(0));
    this->getOutputSocket(0)->relinkConnections(operation->getOutputSocket(0));
    graph->addOperation(operation);
}

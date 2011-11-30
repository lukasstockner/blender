#include "COM_BoxMaskNode.h"
#include "DNA_scene_types.h"
#include "COM_BoxMaskOperation.h"
#include "COM_ExecutionSystem.h"

BoxMaskNode::BoxMaskNode(bNode *editorNode): Node(editorNode) {
}

void BoxMaskNode::convertToOperations(ExecutionSystem *graph, CompositorContext * context) {
    BoxMaskOperation *operation;
    operation = new BoxMaskOperation();
    operation->setData((NodeBoxMask*)this->getbNode()->storage);
    this->getInputSocket(0)->relinkConnections(operation->getInputSocket(0), true, 0, graph);
    this->getInputSocket(1)->relinkConnections(operation->getInputSocket(1), true, 1, graph);
    this->getOutputSocket(0)->relinkConnections(operation->getOutputSocket(0));
    operation->setMaskType(this->getbNode()->custom1);

    graph->addOperation(operation);
}

#include "COM_EllipseMaskNode.h"
#include "DNA_scene_types.h"
#include "COM_EllipseMaskOperation.h"
#include "COM_ExecutionSystem.h"

EllipseMaskNode::EllipseMaskNode(bNode *editorNode): Node(editorNode) {
}

void EllipseMaskNode::convertToOperations(ExecutionSystem *graph, CompositorContext * context) {
    EllipseMaskOperation *operation;
    operation = new EllipseMaskOperation();
    operation->setData((NodeEllipseMask*)this->getbNode()->storage);
    this->getInputSocket(0)->relinkConnections(operation->getInputSocket(0), true, 0, graph);
    this->getInputSocket(1)->relinkConnections(operation->getInputSocket(1), true, 1, graph);
    this->getOutputSocket(0)->relinkConnections(operation->getOutputSocket(0));
    operation->setMaskType(this->getbNode()->custom1);

    graph->addOperation(operation);
}

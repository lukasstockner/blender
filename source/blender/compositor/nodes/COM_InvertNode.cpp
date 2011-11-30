#include "COM_InvertNode.h"
#include "DNA_scene_types.h"
#include "COM_InvertOperation.h"
#include "COM_ExecutionSystem.h"
#include "BKE_node.h"

InvertNode::InvertNode(bNode *editorNode): Node(editorNode) {
}

void InvertNode::convertToOperations(ExecutionSystem *graph, CompositorContext * context) {
    InvertOperation *operation = new InvertOperation();
    bNode* node = this->getbNode();
    operation->setColor(node->custom1 & CMP_CHAN_RGB);
    operation->setAlpha(node->custom1 & CMP_CHAN_A);

    this->getInputSocket(0)->relinkConnections(operation->getInputSocket(0),true, 0, graph);
    this->getInputSocket(1)->relinkConnections(operation->getInputSocket(1),true, 1, graph);
    this->getOutputSocket(0)->relinkConnections(operation->getOutputSocket(0));
    graph->addOperation(operation);
}

#include "COM_GammaNode.h"
#include "DNA_scene_types.h"
#include "COM_GammaOperation.h"
#include "COM_ExecutionSystem.h"

GammaNode::GammaNode(bNode *editorNode): Node(editorNode) {
}

void GammaNode::convertToOperations(ExecutionSystem *graph, CompositorContext * context) {
    GammaOperation *operation = new GammaOperation();

    this->getInputSocket(0)->relinkConnections(operation->getInputSocket(0), true, 0, graph);
    this->getInputSocket(1)->relinkConnections(operation->getInputSocket(1), true, 1, graph);
    this->getOutputSocket(0)->relinkConnections(operation->getOutputSocket(0));
    graph->addOperation(operation);
}

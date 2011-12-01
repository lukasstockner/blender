#include "COM_ColorToBWNode.h"

#include "COM_ConvertColorToBWOperation.h"
#include "COM_ExecutionSystem.h"

ColourToBWNode::ColourToBWNode(bNode *editorNode): Node(editorNode) {
//    this->addInputSocket(COM_DT_COLOR);
//    this->addOutputSocket(COM_DT_VALUE);
}

void ColourToBWNode::convertToOperations(ExecutionSystem *graph, CompositorContext * context) {
    InputSocket *colourSocket = this->getInputSocket(0);
    OutputSocket *valueSocket = this->getOutputSocket(0);

    ConvertColorToBWOperation *convertProg = new ConvertColorToBWOperation();
    colourSocket->relinkConnections(convertProg->getInputSocket(0), true, 0, graph);
    valueSocket->relinkConnections(convertProg->getOutputSocket(0));
    graph->addOperation(convertProg);
}

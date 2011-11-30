#include "COM_CompositorNode.h"
#include "COM_CompositorOperation.h"
#include "COM_ExecutionSystem.h"

CompositorNode::CompositorNode(bNode *editorNode): Node(editorNode) {
}

void CompositorNode::convertToOperations(ExecutionSystem *graph, CompositorContext * context) {
    InputSocket *imageSocket = this->getInputSocket(0);
    InputSocket *alphaSocket = this->getInputSocket(1);
    if (imageSocket->isConnected()) {
        CompositorOperation *colourAlphaProg = new CompositorOperation();
		colourAlphaProg->setScene(context->getScene());
		colourAlphaProg->setbNodeTree(context->getbNodeTree());
        imageSocket->relinkConnections(colourAlphaProg->getInputSocket(0));
        alphaSocket->relinkConnections(colourAlphaProg->getInputSocket(1));
        graph->addOperation(colourAlphaProg);
		addPreviewOperation(graph, colourAlphaProg->getInputSocket(0), 5);
    }
}

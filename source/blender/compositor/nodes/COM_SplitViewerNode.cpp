#include "COM_SplitViewerNode.h"
#include "BKE_global.h"

#include "COM_SplitViewerOperation.h"
#include "COM_ExecutionSystem.h"

SplitViewerNode::SplitViewerNode(bNode *editorNode): Node(editorNode) {
}

void SplitViewerNode::convertToOperations(ExecutionSystem *graph, CompositorContext * context) {
    InputSocket *image1Socket = this->getInputSocket(0);
    InputSocket *image2Socket = this->getInputSocket(1);
    Image* image = (Image*)this->getbNode()->id;
    ImageUser * imageUser = (ImageUser*) this->getbNode()->storage;
    if (image1Socket->isConnected() && image2Socket->isConnected()) {
        SplitViewerOperation *splitViewerOperation = new SplitViewerOperation();
        splitViewerOperation->setImage(image);
        splitViewerOperation->setImageUser(imageUser);
        splitViewerOperation->setActive(this->getbNode()->flag & NODE_DO_OUTPUT);
        splitViewerOperation->setSplitPercentage(this->getbNode()->custom1);
        splitViewerOperation->setXSplit(!this->getbNode()->custom2);
        image1Socket->relinkConnections(splitViewerOperation->getInputSocket(0), true, 1, graph);
        image2Socket->relinkConnections(splitViewerOperation->getInputSocket(1), true, 1, graph);
		addPreviewOperation(graph, splitViewerOperation->getInputSocket(0), 0);
        graph->addOperation(splitViewerOperation);
    }
}

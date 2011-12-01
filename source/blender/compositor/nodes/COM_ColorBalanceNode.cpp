#include "COM_ColorBalanceNode.h"
#include "COM_ColorBalanceLGGOperation.h"
#include "COM_ColorBalanceASCCDLOperation.h"
#include "COM_ExecutionSystem.h"
#include "BKE_node.h"
#include "COM_MixBlendOperation.h"

ColorBalanceNode::ColorBalanceNode(bNode* editorNode): Node(editorNode)
{
//    this->addInputSocket(COM_DT_VALUE);
//    this->addInputSocket(COM_DT_COLOR);
//    this->addOutputSocket(COM_DT_COLOR);

}
void ColorBalanceNode::convertToOperations(ExecutionSystem *graph, CompositorContext * context) {
    InputSocket *inputSocket = this->getInputSocket(0);
    InputSocket *inputImageSocket = this->getInputSocket(1);
    OutputSocket *outputSocket = this->getOutputSocket(0);

    bNode* node = this->getbNode();
    NodeColorBalance *n= (NodeColorBalance *)node->storage;
    NodeOperation*operation;
    if (node->custom1 == 0) {
        ColorBalanceLGGOperation* operationLGG = new ColorBalanceLGGOperation();
        {
                int c;

                for (c = 0; c < 3; c++) {
                        n->lift_lgg[c] = 2.0f - n->lift[c];
                        n->gamma_inv[c] = (n->gamma[c] != 0.0f) ? 1.0f/n->gamma[c] : 1000000.0f;
                }
        }

        operationLGG->setGain(n->gain);
        operationLGG->setLift(n->lift_lgg);
        operationLGG->setGammaInv(n->gamma_inv);
        operation = operationLGG;
    } else {
        ColorBalanceASCCDLOperation *operationCDL = new ColorBalanceASCCDLOperation();
        operationCDL->setGain(n->gain);
        operationCDL->setLift(n->lift);
        operationCDL->setGamma(n->gamma);
        operation = operationCDL;
    }

    inputSocket->relinkConnections(operation->getInputSocket(0), true, 0, graph);
    inputImageSocket->relinkConnections(operation->getInputSocket(1), true, 0, graph);
    outputSocket->relinkConnections(operation->getOutputSocket(0));
    graph->addOperation(operation);
}

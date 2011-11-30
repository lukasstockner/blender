#include "COM_MathNode.h"
#include "COM_MathBaseOperation.h"
#include "COM_ExecutionSystem.h"

void MathNode::convertToOperations(ExecutionSystem *graph, CompositorContext * context) {
    MathBaseOperation* operation=NULL;

    switch(this->getbNode()->custom1)
    {
    case 0: /* Add */
            operation = new MathAddOperation();
            break;
    case 1: /* Subtract */
            operation = new MathSubtractOperation();
            break;
    case 2: /* Multiply */
            operation = new MathMultiplyOperation();
            break;
    case 3: /* Divide */
            operation = new MathDivideOperation();
            break;
    case 4: /* Sine */
            operation = new MathSineOperation();
            break;
    case 5: /* Cosine */
            operation = new MathCosineOperation();
            break;
    case 6: /* Tangent */
            operation = new MathTangentOperation();
            break;
    case 7: /* Arc-Sine */
            operation = new MathArcSineOperation();
            break;
    case 8: /* Arc-Cosine */
            operation = new MathArcCosineOperation();
            break;
    case 9: /* Arc-Tangent */
            operation = new MathArcTangentOperation();
            break;
    case 10: /* Power */
            operation = new MathPowerOperation();
            break;
    case 11: /* Logarithm */
            operation = new MathLogarithmOperation();
            break;
    case 12: /* Minimum */
            operation = new MathMinimumOperation();
            break;
    case 13: /* Maximum */
            operation = new MathMaximumOperation();
            break;
    case 14: /* Round */
            operation = new MathRoundOperation();
            break;
    case 15: /* Less Than */
            operation = new MathLessThanOperation();
            break;
    case 16: /* Greater Than */
            operation = new MathGreaterThanOperation();
            break;
    }

    if (operation != NULL) {
        this->getInputSocket(0)->relinkConnections(operation->getInputSocket(0), true, 0, graph);
        this->getInputSocket(1)->relinkConnections(operation->getInputSocket(1), true, 1, graph);
        this->getOutputSocket(0)->relinkConnections(operation->getOutputSocket());

        graph->addOperation(operation);
    }
}

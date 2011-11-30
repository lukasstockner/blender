#ifndef COM_ColorBalanceNODE_H
#define COM_ColorBalanceNODE_H

#include "COM_Node.h"

/**
  * @brief ColorBalanceNode
  * @ingroup Node
  */
class ColorBalanceNode : public Node
{
public:
    ColorBalanceNode(bNode* editorNode);
    void convertToOperations(ExecutionSystem* graph, CompositorContext * context);
};

#endif // COM_ColorBalanceNODE_H

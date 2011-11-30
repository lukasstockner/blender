#ifndef COM_NORMALNODE_H
#define COM_NORMALNODE_H

#include "COM_Node.h"

/**
  * @brief NormalNode
  * @ingroup Node
  */
class NormalNode : public Node
{
public:
    NormalNode(bNode* editorNode);
    void convertToOperations(ExecutionSystem* graph, CompositorContext * context);
};

#endif // COM_NormalNODE_H

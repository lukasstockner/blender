#ifndef COM_FILTERNODE_H
#define COM_FILTERNODE_H

#include "COM_Node.h"

/**
  * @brief FilterNode
  * @ingroup Node
  */
class FilterNode : public Node
{
public:
    FilterNode(bNode* editorNode);
    void convertToOperations(ExecutionSystem* graph, CompositorContext * context);
};

#endif // COM_FILTERNODE_H

#ifndef COM_DifferenceMatteNODE_H
#define COM_DifferenceMatteNODE_H

#include "COM_Node.h"

/**
  * @brief DifferenceMatteNode
  * @ingroup Node
  */
class DifferenceMatteNode : public Node
{
public:
	DifferenceMatteNode(bNode* editorNode);
    void convertToOperations(ExecutionSystem* graph, CompositorContext * context);
};

#endif // COM_DifferenceMatteNODE_H

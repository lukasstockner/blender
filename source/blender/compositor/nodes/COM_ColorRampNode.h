#ifndef COM_ColorRampNODE_H
#define COM_ColorRampNODE_H

#include "COM_Node.h"

/**
  * @brief ColorRampNode
  * @ingroup Node
  */
class ColorRampNode : public Node
{
public:
	ColorRampNode(bNode* editorNode);
    void convertToOperations(ExecutionSystem* graph, CompositorContext * context);
};

#endif // COM_ColorRampNODE_H

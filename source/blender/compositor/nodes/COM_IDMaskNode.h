#ifndef _COM_IDMaskNode_h_
#define _COM_IDMaskNode_h_

#include "COM_Node.h"

/**
  * @brief IDMaskNode
  * @ingroup Node
  */
class IDMaskNode: public Node {
public:
    IDMaskNode(bNode *editorNode);
    void convertToOperations(ExecutionSystem* graph, CompositorContext * context);
};

#endif

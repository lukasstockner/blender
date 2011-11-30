#ifndef _COM_EllipseMaskNode_h_
#define _COM_EllipseMaskNode_h_

#include "COM_Node.h"

/**
  * @brief EllipseMaskNode
  * @ingroup Node
  */
class EllipseMaskNode: public Node {
public:
    EllipseMaskNode(bNode *editorNode);
    void convertToOperations(ExecutionSystem* graph, CompositorContext * context);
};

#endif

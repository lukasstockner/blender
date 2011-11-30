#ifndef _COM_BoxMaskNode_h_
#define _COM_BoxMaskNode_h_

#include "COM_Node.h"

/**
  * @brief BoxMaskNode
  * @ingroup Node
  */
class BoxMaskNode: public Node {
public:
    BoxMaskNode(bNode *editorNode);
    void convertToOperations(ExecutionSystem* graph, CompositorContext * context);
};

#endif

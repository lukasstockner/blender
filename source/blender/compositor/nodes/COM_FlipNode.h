#ifndef _COM_FlipNode_h_
#define _COM_FlipNode_h_

#include "COM_Node.h"

/**
  * @brief FlipNode
  * @ingroup Node
  */
class FlipNode: public Node {
public:
    FlipNode(bNode *editorNode);
    void convertToOperations(ExecutionSystem* graph, CompositorContext * context);
};

#endif

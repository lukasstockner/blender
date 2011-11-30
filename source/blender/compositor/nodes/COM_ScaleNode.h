#ifndef _COM_ScaleNode_h_
#define _COM_ScaleNode_h_

#include "COM_Node.h"

/**
  * @brief ScaleNode
  * @ingroup Node
  */
class ScaleNode: public Node {
public:
    ScaleNode(bNode *editorNode);
    void convertToOperations(ExecutionSystem* graph, CompositorContext * context);
};

#endif

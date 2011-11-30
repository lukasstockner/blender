#ifndef _COM_GammaNode_h_
#define _COM_GammaNode_h_

#include "COM_Node.h"

/**
  * @brief GammaNode
  * @ingroup Node
  */
class GammaNode: public Node {
public:
    GammaNode(bNode *editorNode);
    void convertToOperations(ExecutionSystem* graph, CompositorContext * context);
};

#endif

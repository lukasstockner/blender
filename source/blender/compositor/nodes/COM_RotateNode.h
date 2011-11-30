#ifndef _COM_RotateNode_h_
#define _COM_RotateNode_h_

#include "COM_Node.h"

/**
  * @brief RotateNode
  * @ingroup Node
  */
class RotateNode: public Node {
public:
    RotateNode(bNode *editorNode);
    void convertToOperations(ExecutionSystem* graph, CompositorContext * context);
};

#endif

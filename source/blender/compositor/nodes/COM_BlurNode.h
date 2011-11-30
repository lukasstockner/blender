#ifndef _COM_BlurNode_h_
#define _COM_BlurNode_h_

#include "COM_Node.h"

/**
  * @brief BlurNode
  * @ingroup Node
  */

class BlurNode: public Node {
public:
	BlurNode(bNode *editorNode);
    void convertToOperations(ExecutionSystem* graph, CompositorContext * context);
};

#endif

#ifndef _COM_BokehImageNode_h_
#define _COM_BokehImageNode_h_

#include "COM_Node.h"

/**
  * @brief BokehImageNode
  * @ingroup Node
  */
class BokehImageNode: public Node {
public:
	BokehImageNode(bNode *editorNode);
    void convertToOperations(ExecutionSystem* graph, CompositorContext * context);
};

#endif

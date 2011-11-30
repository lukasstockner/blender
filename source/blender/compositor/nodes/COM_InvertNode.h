#ifndef _COM_InvertNode_h_
#define _COM_InvertNode_h_

#include "COM_Node.h"

/**
  * @brief InvertNode
  * @ingroup Node
  */
class InvertNode: public Node {
public:
    InvertNode(bNode *editorNode);
	void convertToOperations(ExecutionSystem* graph, CompositorContext * context);
};

#endif

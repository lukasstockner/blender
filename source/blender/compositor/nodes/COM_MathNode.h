#ifndef _COM_MathNode_h
#define _COM_MathNode_h

#include "COM_Node.h"

/**
  * @brief MathNode
  * @ingroup Node
  */
class MathNode: public Node {
public:
    MathNode(bNode* editorNode) :Node(editorNode) {}
	void convertToOperations(ExecutionSystem *graph, CompositorContext * context);
};

#endif

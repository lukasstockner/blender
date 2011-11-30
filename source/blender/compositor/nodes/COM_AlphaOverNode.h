#ifndef _COM_AlphaOverNode_h
#define _COM_AlphaOverNode_h

#include "COM_Node.h"

/**
  * @brief AlphaOverNode
  * @ingroup Node
  */
class AlphaOverNode: public Node {
public:
    AlphaOverNode(bNode* editorNode) :Node(editorNode) {}
	void convertToOperations(ExecutionSystem *graph, CompositorContext * context);
};

#endif

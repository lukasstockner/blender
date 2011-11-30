#ifndef _COM_ZCombineNode_h
#define _COM_ZCombineNode_h

#include "COM_Node.h"

/**
  * @brief ZCombineNode
  * @ingroup Node
  */
class ZCombineNode: public Node {
public:
	ZCombineNode(bNode* editorNode) :Node(editorNode) {}
	void convertToOperations(ExecutionSystem *graph, CompositorContext * context);
};

#endif

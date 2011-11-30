#ifndef _COM_SetAlphaNode_h
#define _COM_SetAlphaNode_h

#include "COM_Node.h"

/**
  * @brief SetAlphaNode
  * @ingroup Node
  */
class SetAlphaNode: public Node {
public:
    SetAlphaNode(bNode* editorNode) :Node(editorNode) {}
	void convertToOperations(ExecutionSystem *graph, CompositorContext * context);
};

#endif

#ifndef _COM_ValueNode_h_
#define _COM_ValueNode_h_

#include "COM_Node.h"

/**
  * @brief ValueNode
  * @ingroup Node
  */
class ValueNode: public Node {
public:
	ValueNode(bNode *editorNode);
	void convertToOperations(ExecutionSystem* graph, CompositorContext * context);
};

#endif

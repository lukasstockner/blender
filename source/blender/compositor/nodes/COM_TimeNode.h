#ifndef _COM_TimeNode_h_
#define _COM_TimeNode_h_

#include "COM_Node.h"

/**
  * @brief TimeNode
  * @ingroup Node
  */
class TimeNode: public Node {
public:
	TimeNode(bNode *editorNode);
	void convertToOperations(ExecutionSystem* graph, CompositorContext * context);
};

#endif

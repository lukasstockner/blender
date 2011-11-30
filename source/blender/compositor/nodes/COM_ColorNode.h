#ifndef _COM_ColorNode_h_
#define _COM_ColorNode_h_

#include "COM_Node.h"

/**
  * @brief ColorNode
  * @ingroup Node
  */
class ColorNode: public Node {
public:
	ColorNode(bNode *editorNode);
	void convertToOperations(ExecutionSystem* graph, CompositorContext * context);
};

#endif

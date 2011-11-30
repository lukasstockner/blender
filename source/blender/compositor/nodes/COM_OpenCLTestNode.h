#ifndef _COM_OpenCLTestNode_h_
#define _COM_OpenCLTestNode_h_

#include "COM_Node.h"

/**
  * @brief OpenCLTestNode
  * @ingroup Node
  */
class OpenCLTestNode: public Node {
public:
	OpenCLTestNode(bNode *editorNode);
	void convertToOperations(ExecutionSystem* graph, CompositorContext * context);
};

#endif

#ifndef _COM_TonemapNode_h_
#define _COM_TonemapNode_h_

#include "COM_Node.h"

/**
  * @brief TonemapNode
  * @ingroup Node
  */
class TonemapNode: public Node {
public:
	TonemapNode(bNode *editorNode);
    void convertToOperations(ExecutionSystem* graph, CompositorContext * context);
};

#endif

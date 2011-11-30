#ifndef _COM_DirectionalBlurNode_h_
#define _COM_DirectionalBlurNode_h_

#include "COM_Node.h"

/**
  * @brief DirectionalBlurNode
  * @ingroup Node
  */
class DirectionalBlurNode: public Node {
public:
	DirectionalBlurNode(bNode *editorNode);
    void convertToOperations(ExecutionSystem* graph, CompositorContext * context);
};

#endif

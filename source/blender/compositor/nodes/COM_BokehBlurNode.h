#ifndef _COM_BokehBlurNode_h_
#define _COM_BokehBlurNode_h_

#include "COM_Node.h"

/**
  * @brief BokehBlurNode
  * @ingroup Node
  */

class BokehBlurNode: public Node {
public:
	BokehBlurNode(bNode *editorNode);
    void convertToOperations(ExecutionSystem* graph, CompositorContext * context);
};

#endif

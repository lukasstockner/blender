#ifndef _COM_LensFlareNode_h_
#define _COM_LensFlareNode_h_

#include "COM_Node.h"

/**
  * @brief LensFlareNode
  * @ingroup Node
  */
class LensFlareNode: public Node {
public:
	LensFlareNode(bNode *editorNode);
    void convertToOperations(ExecutionSystem* graph, CompositorContext * context);
};

#endif

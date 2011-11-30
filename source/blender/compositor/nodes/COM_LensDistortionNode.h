#ifndef _COM_LensDistortionNode_h_
#define _COM_LensDistortionNode_h_

#include "COM_Node.h"

/**
  * @brief LensDistortionNode
  * @ingroup Node
  */
class LensDistortionNode: public Node {
public:
	LensDistortionNode(bNode *editorNode);
    void convertToOperations(ExecutionSystem* graph, CompositorContext * context);
};

#endif

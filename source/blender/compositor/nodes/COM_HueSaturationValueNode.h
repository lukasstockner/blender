#ifndef _COM_HueSaturationValueNode_h
#define _COM_HueSaturationValueNode_h

#include "COM_Node.h"

/**
  * @brief HueSaturationValueNode
  * @ingroup Node
  */
class HueSaturationValueNode : public Node {
public:
    HueSaturationValueNode(bNode *editorNode);
	void convertToOperations(ExecutionSystem *graph, CompositorContext * context);
};
#endif

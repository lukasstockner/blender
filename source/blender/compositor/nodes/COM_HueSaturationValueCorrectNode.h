#ifndef _COM_HueSaturationValueCorrectNode_h
#define _COM_HueSaturationValueCorrectNode_h

#include "COM_Node.h"

/**
  * @brief HueSaturationValueCorrectNode
  * @ingroup Node
  */
class HueSaturationValueCorrectNode : public Node {
public:
	HueSaturationValueCorrectNode(bNode *editorNode);
	void convertToOperations(ExecutionSystem *graph, CompositorContext * context);
};
#endif

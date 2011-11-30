#ifndef _COM_SeparateHSVANode_h
#define _COM_SeparateHSVANode_h

#include "COM_Node.h"
#include "DNA_node_types.h"
#include "COM_SeparateRGBANode.h"

/**
  * @brief SeparateHSVANode
  * @ingroup Node
  */
class SeparateHSVANode : public SeparateRGBANode {
public:
    SeparateHSVANode(bNode *editorNode);
	void convertToOperations(ExecutionSystem *graph, CompositorContext * context);
};
#endif

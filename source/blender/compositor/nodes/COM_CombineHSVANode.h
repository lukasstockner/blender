#ifndef _COM_CombineHSVANode_h
#define _COM_CombineHSVANode_h

#include "COM_Node.h"
#include "DNA_node_types.h"
#include "COM_CombineRGBANode.h"
/**
  * @brief CombineHSVANode
  * @ingroup Node
  */
class CombineHSVANode : public CombineRGBANode {
public:
    CombineHSVANode(bNode *editorNode);
	void convertToOperations(ExecutionSystem *graph, CompositorContext * context);
};
#endif

#ifndef _COM_CombineRGBANode_h
#define _COM_CombineRGBANode_h

#include "COM_Node.h"
#include "DNA_node_types.h"
/**
  * @brief CombineRGBANode
  * @ingroup Node
  */
class CombineRGBANode : public Node {
public:
    CombineRGBANode(bNode *editorNode);
	void convertToOperations(ExecutionSystem *graph, CompositorContext * context);
};
#endif

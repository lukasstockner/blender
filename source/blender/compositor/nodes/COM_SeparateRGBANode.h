#ifndef _COM_SeparateRGBANode_h
#define _COM_SeparateRGBANode_h

#include "COM_Node.h"
#include "DNA_node_types.h"
/**
  * @brief SeparateRGBANode
  * @ingroup Node
  */
class SeparateRGBANode : public Node {
public:
    SeparateRGBANode(bNode *editorNode);
	void convertToOperations(ExecutionSystem *graph, CompositorContext * context);
};
#endif

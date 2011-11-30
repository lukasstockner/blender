#ifndef _COM_CompositorNode_h
#define _COM_CompositorNode_h

#include "COM_Node.h"
#include "DNA_node_types.h"
/**
  * @brief CompositorNode
  * @ingroup Node
  */
class CompositorNode : public Node {
public:
    CompositorNode(bNode *editorNode);
	void convertToOperations(ExecutionSystem *graph, CompositorContext * context);
};
#endif

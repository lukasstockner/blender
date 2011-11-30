#ifndef _COM_MixNode_h
#define _COM_MixNode_h

#include "COM_Node.h"
#include "DNA_node_types.h"
/**
  * @brief MixNode
  * @ingroup Node
  */
class MixNode : public Node {
public:
	MixNode(bNode *editorNode);
	void convertToOperations(ExecutionSystem *graph, CompositorContext * context);
};
#endif

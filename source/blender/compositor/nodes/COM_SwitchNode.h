#ifndef _COM_SwitchNode_h
#define _COM_SwitchNode_h

#include "COM_Node.h"
#include "COM_NodeOperation.h"
#include "DNA_node_types.h"
/**
  * @brief SwitchNode
  * @ingroup Node
  */
class SwitchNode : public Node {
public:
	SwitchNode(bNode *editorNode);
	void convertToOperations(ExecutionSystem *graph, CompositorContext * context);
};
#endif

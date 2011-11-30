#ifndef _COM_ViewerNode_h
#define _COM_ViewerNode_h

#include "COM_Node.h"
#include "DNA_node_types.h"
/**
  * @brief ViewerNode
  * @ingroup Node
  */
class ViewerNode : public Node {
public:
    ViewerNode(bNode *editorNode);
	void convertToOperations(ExecutionSystem *graph, CompositorContext * context);
};
#endif

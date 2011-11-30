#ifndef _COM_SplitViewerNode_h
#define _COM_SplitViewerNode_h

#include "COM_Node.h"
#include "DNA_node_types.h"
/**
  * @brief SplitViewerNode
  * @ingroup Node
  */
class SplitViewerNode : public Node {
public:
    SplitViewerNode(bNode *editorNode);
	void convertToOperations(ExecutionSystem *graph, CompositorContext * context);
};
#endif

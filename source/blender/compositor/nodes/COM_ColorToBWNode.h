#ifndef _COM_ColourToBWNode_h
#define _COM_ColourToBWNode_h

#include "COM_Node.h"
#include "DNA_node_types.h"
/**
  * @brief ColourToBWNode
  * @ingroup Node
  */
class ColourToBWNode : public Node {
public:
    ColourToBWNode(bNode *editorNode);
	void convertToOperations(ExecutionSystem *graph, CompositorContext * context);
};
#endif

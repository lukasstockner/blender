#ifndef _COM_DilateErodeNode_h_
#define _COM_DilateErodeNode_h_

#include "COM_Node.h"

/**
  * @brief DilateErodeNode
  * @ingroup Node
  */
class DilateErodeNode: public Node {
public:
	DilateErodeNode(bNode *editorNode);
    void convertToOperations(ExecutionSystem* graph, CompositorContext * context);
};

#endif

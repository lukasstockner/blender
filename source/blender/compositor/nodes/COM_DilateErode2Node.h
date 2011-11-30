#ifndef _COM_DilateErode2Node_h_
#define _COM_DilateErode2Node_h_

#include "COM_Node.h"

/**
  * @brief DilateErode2Node
  * @ingroup Node
  */
class DilateErode2Node: public Node {
public:
    DilateErode2Node(bNode *editorNode);
    void convertToOperations(ExecutionSystem* graph, CompositorContext * context);
};

#endif

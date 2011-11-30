#ifndef _COM_TranslateNode_h_
#define _COM_TranslateNode_h_

#include "COM_Node.h"

/**
  * @brief TranslateNode
  * @ingroup Node
  */
class TranslateNode: public Node {
public:
    TranslateNode(bNode *editorNode);
    void convertToOperations(ExecutionSystem* graph, CompositorContext * context);
};

#endif

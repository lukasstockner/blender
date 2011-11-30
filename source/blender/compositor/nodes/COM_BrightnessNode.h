#ifndef _COM_BrightnessNode_h_
#define _COM_BrightnessNode_h_

#include "COM_Node.h"

/**
  * @brief BrightnessNode
  * @ingroup Node
  */
class BrightnessNode: public Node {
public:
    BrightnessNode(bNode *editorNode);
    void convertToOperations(ExecutionSystem* graph, CompositorContext * context);
};

#endif

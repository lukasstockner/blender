#ifndef _COM_ColorCorrectionNode_h_
#define _COM_ColorCorrectionNode_h_

#include "COM_Node.h"

/**
  * @brief ColorCorrectionNode
  * @ingroup Node
  */
class ColorCorrectionNode: public Node {
public:
    ColorCorrectionNode(bNode *editorNode);
    void convertToOperations(ExecutionSystem* graph, CompositorContext * context);
};

#endif

#ifndef _COM_ColorCurveNode_h_
#define _COM_ColorCurveNode_h_

#include "COM_Node.h"

/**
  * @brief ColorCurveNode
  * @ingroup Node
  */
class ColorCurveNode: public Node {
public:
	ColorCurveNode(bNode *editorNode);
	void convertToOperations(ExecutionSystem* graph, CompositorContext * context);
};

#endif

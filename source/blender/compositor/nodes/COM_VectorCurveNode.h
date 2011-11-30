#ifndef _COM_VectorCurveNode_h_
#define _COM_VectorCurveNode_h_

#include "COM_Node.h"

/**
  * @brief VectorCurveNode
  * @ingroup Node
  */
class VectorCurveNode: public Node {
public:
	VectorCurveNode(bNode *editorNode);
	void convertToOperations(ExecutionSystem* graph, CompositorContext * context);
};

#endif

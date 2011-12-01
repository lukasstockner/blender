#ifndef _COM_MuteNode_h_
#define _COM_MuteNode_h_

#include "COM_Node.h"

/**
  * @brief MuteNode
  * @ingroup Node
  */
class MuteNode: public Node {
public:
    MuteNode(bNode *editorNode);
    void convertToOperations(ExecutionSystem* graph, CompositorContext * context);
private:
	void reconnect(OutputSocket * output);
};

#endif

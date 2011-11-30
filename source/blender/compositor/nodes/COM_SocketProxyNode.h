#ifndef _COM_SocketProxyNode_h_
#define _COM_SocketProxyNode_h_

#include "COM_Node.h"

/**
  * @brief SocketProxyNode
  * @ingroup Node
  */
class SocketProxyNode: public Node {
public:
    SocketProxyNode(bNode *editorNode);
    void convertToOperations(ExecutionSystem* graph, CompositorContext * context);
private:
	void clearInputAndOutputSockets();

};

#endif

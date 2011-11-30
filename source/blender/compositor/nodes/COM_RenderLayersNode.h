#include "COM_Node.h"
#include "DNA_node_types.h"
#include "COM_RenderLayersBaseProg.h"

/**
  * @brief RenderLayersNode
  * @ingroup Node
  */
class RenderLayersNode : public Node {
public:
    RenderLayersNode(bNode* editorNode);
	void convertToOperations(ExecutionSystem *graph, CompositorContext * context);
private:
    void testSocketConnection(ExecutionSystem* graph, int outputSocketNumber, RenderLayersBaseProg * operation);
};

#include "COM_Node.h"
#include "DNA_node_types.h"

/**
  * @brief TextureNode
  * @ingroup Node
  */
class TextureNode : public Node {
public:
	TextureNode(bNode* editorNode);
	void convertToOperations(ExecutionSystem *graph, CompositorContext * context);
};

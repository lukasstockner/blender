#include "COM_Node.h"
#include "DNA_node_types.h"
#include "DNA_image_types.h"

/**
  * @brief ImageNode
  * @ingroup Node
  */
class ImageNode : public Node {


public:
    ImageNode(bNode* editorNode);
	void convertToOperations(ExecutionSystem *graph, CompositorContext * context);

};

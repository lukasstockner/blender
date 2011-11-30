#ifndef _COM_GroupNode_h_
#define _COM_GroupNode_h_

#include "COM_Node.h"
#include "COM_ExecutionSystem.h"

/**
  * @brief Represents a group node
  * @ingroup Node
  */
class GroupNode: public Node {
public:
	GroupNode(bNode *editorNode);
	void convertToOperations(ExecutionSystem* graph, CompositorContext * context);

	/**
	  * @brief check if this node a group node.
	  * @returns true
	  */
	const bool isGroupNode() const {return true;}

	/**
	  * @brief ungroup this group node.
	  * during ungroup the subtree (internal nodes and links) of the group node
	  * are added to the ExecutionSystem.
	  *
	  * Between the main tree and the subtree proxy nodes will be added
	  * to translate between InputSocket and OutputSocket
	  *
	  * @param system the ExecutionSystem where to add the subtree
	  */
	void ungroup(ExecutionSystem &system);
};

#endif

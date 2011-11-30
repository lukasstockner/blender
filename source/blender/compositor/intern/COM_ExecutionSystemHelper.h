class ExecutionGroup;

#ifndef _COM_ExecutionSystemHelper_h
#define _COM_ExecutionSystemHelper_h

#include "DNA_node_types.h"
#include <vector>
#include "COM_Node.h"
#include "COM_SocketConnection.h"
#include "BKE_text.h"
#include "COM_ExecutionGroup.h"

using namespace std;

/**
 *
 */
class ExecutionSystemHelper {

public:

	/**
	  * @brief add an bNodeTree to the nodes list and connections
	  * @param nodes vector of nodes
	  * @param links vector of links
	  * @param tree bNodeTree to add
	  * @return Node representing the "Compositor node" of the maintree. or NULL when a subtree is added
	  */
	static Node* addbNodeTree(vector<Node*>& nodes, vector<SocketConnection*>& links, bNodeTree * tree);

	/**
	  * @brief add an editor node to the system.
      * this node is converted to a Node instance.
      * and the converted node is returned
	  *
	  * @param bNode node to add
	  * @return Node that represents the bNode or null when not able to convert.
      */
	static Node* addNode(vector<Node*>& nodes, bNode* bNode);

	/**
	  * @brief Add a Node to a list
	  *
	  * @param nodes the list where the node needs to be added to
	  * @param node the node to be added
	  */
	static void addNode(vector<Node*>& nodes, Node* node);

	/**
	  * @brief Add an operation to the operation list
	  *
	  * The id of the operation is updated.
	  *
	  * @param operations the list where the operation need to be added to
	  * @param operation the operation to add
	  */
	static void addOperation(vector<NodeOperation*> &operations, NodeOperation* operation);

	/**
	  * @brief Add an ExecutionGroup to a list
	  *
	  * The id of the ExecutionGroup is updated.
	  *
	  * @param executionGroups the list where the executionGroup need to be added to
	  * @param executionGroup the ExecutionGroup to add
	  */
	static void addExecutionGroup(vector<ExecutionGroup*>& executionGroups, ExecutionGroup *executionGroup);

	/**
	  * @brief find the first node that contains the bNode
	  *
	  * @param nodes list of nodes where to look in
	  * @param node bNode that needs to be found. Can be NULL for Group nodes
	  * @param bsocket For Group nodes the socket is used to determine the Node.
	  */
	static Node* findNodeBybNode(vector<Node*>& nodes, bNode* node, bNodeSocket* bsocket);

	/**
	  * Find all Node Operations that needs to be executed.
	  * @param rendering
	  * the rendering parameter will tell what type of execution we are doing
	  * FALSE is editing, TRUE is rendering
	  */
	static void findOutputNodeOperations(vector<NodeOperation*>* result, vector<NodeOperation*>& operations , bool rendering);

	/**
	  * @brief add a bNodeLink to the list of links
	  * the bNodeLink will be wrapped in a SocketConnection
	  *
	  * @note Cyclic links will be ignored
	  *
	  * @param nodes list of nodes
	  * @param links list of links to add the bNodeLink to
	  * @param bNodeLink the link to be added
	  * @return the created SocketConnection or NULL
	  */
	static SocketConnection* addNodeLink(vector<Node*>& nodes, vector<SocketConnection*>& links, bNodeLink *bNodeLink);

	/**
	  * @brief check whether bnode contains bsocket
	  *
	  * @note only works for group nodes
	  *
	  * @param bnode node to look in
	  * @param bsocket socket to look for
	  */
	static bool containsbNodeSocket(bNode *bnode, bNodeSocket* bsocket);

	/**
	  * @brief Ungroup NodeGroup.
	  *
	  * The user can create Group of nodes. These groups are complex during execution. To reduce this complexity
	  * these groups are flattened and removed.
	  *
	  * Ungroup will modify the connections and nodes list.
	  */
	static void ungroup(ExecutionSystem& system);

	/**
	  * @brief create a new SocketConnection and add to a vector of links
	  * @param links the vector of links
	  * @param fromSocket the startpoint of the connection
	  * @param toSocket the endpoint of the connection
	  * @return the new created SocketConnection
	  */
	static SocketConnection* addLink(vector<SocketConnection*>& links, OutputSocket* fromSocket, InputSocket* toSocket);
};
#endif

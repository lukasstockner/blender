#ifndef _COM_Converter_h
#define _COM_Converter_h

#include "DNA_node_types.h"
#include "COM_Node.h"

/**
 * @brief Conversion methods for the compositor
 */
class Converter {
public:
    /**
	  * @brief Convert/wraps a bNode in its Node instance.
	  *
	  * For all nodetypes a wrapper class is created.
	  * Muted nodes are wrapped with MuteNode.
	  *
	  * @note When adding a new node to blender, this method needs to be changed to return the correct Node instance.
      *
      * @see Node
	  * @see MuteNode
      */
	static Node* convert(bNode* bNode);

    /**
	  * @brief This method will add a datetype conversion rule when the to-socket does not support the from-socket actual data type.
	  *
	  * @note this method is called when conversion is needed.
	  *
	  * @param connection the SocketConnection what needs conversion
	  * @param system the ExecutionSystem to add the conversion to.
      * @see SocketConnection - a link between two sockets
      */
	static void convertDataType(SocketConnection* connection, ExecutionSystem *system);

	/**
	  * @brief This method will add a resolution rule based on the settings of the InputSocket.
	  *
	  * @note Conversion logic is implemented in this method
	  * @see InputSocketResizeMode for the possible conversions.

	  * @param connection the SocketConnection what needs conversion
	  * @param system the ExecutionSystem to add the conversion to.
	  * @see SocketConnection - a link between two sockets
	  */
	static void convertResolution(SocketConnection* connection, ExecutionSystem *system);
};
#endif

#ifndef _COM_Socket_h
#define _COM_Socket_h

#include <vector>
#include "BKE_text.h"
#include <string>
#include "DNA_node_types.h"
#include "COM_defines.h"

using namespace std;
class SocketConnection;
class NodeBase;

/**
  * @brief Base class for InputSocket and OutputSocket.
  *
  * A socket are the points on an node where the user can make a connection between.
  * Sockets are always part of a node or an operation.
  *
  * @see InputSocket
  * @see OutputSocket
  * @see SocketConnection - a connection between an InputSocket and an OutputSocket
  * @ingroup Model
  */
class Socket {
private:
    /**
      * Reference to the node where this Socket belongs to
      */
    NodeBase* node;

    /**
      * the datatype of this socket. Is used for automatically data transformation.
      * @section data-conversion
      */
	DataType datatype;

    /**
      * the actual data type during execution. This can be different than the field datatype, based on the conversion rules of the node
      * @section data-conversion
      */
	DataType actualType;

    bNodeSocket* editorSocket;
    bool insideOfGroupNode;
public:
	Socket(DataType datatype);

	DataType getDataType() const;
    void setNode(NodeBase* node);
    NodeBase* getNode() const;

	/**
	  * @brief get the actual data type
	  *
	  * @note The actual data type can differ from the data type this socket expects.
	  * @return actual DataType
	  */
	DataType getActualDataType() const;

	/**
	  * @brief set the actual data type
	  * @param actualType the new actual type
	  */
	void setActualDataType(DataType actualType);

    const virtual int isConnected() const;
    int isInputSocket() const;
    int isOutputSocket() const;
    virtual void determineResolution(int resolution[], unsigned int preferredResolution[]) {}
    virtual void determineActualDataType() {}

	void setEditorSocket(bNodeSocket* editorSocket) {this->editorSocket = editorSocket;}
	bNodeSocket* getbNodeSocket() const {return this->editorSocket;}

    bool isInsideOfGroupNode() {return this->insideOfGroupNode;}
    void setInsideOfGroupNode(bool insideOfGroupNode) {this->insideOfGroupNode = insideOfGroupNode;}

};


#endif

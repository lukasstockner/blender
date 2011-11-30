#ifndef _COM_OutputSocket_h
#define _COM_OutputSocket_h

#include <vector>
#include "COM_Socket.h"
#include "COM_ChannelInfo.h"

using namespace std;
class SocketConnection;
class Node;
class InputSocket;
class WriteBufferOperation;

//#define COM_ST_INPUT 0
//#define COM_ST_OUTPUT 1

/**
  * @brief OutputSocket are sockets that can send data/input
  * @ingroup Model
  */
class OutputSocket : public Socket {
private:
    vector<SocketConnection*> connections;
    InputSocket* groupInput;

	/**
	  * @brief index of the inputsocket that determines the datatype of this outputsocket
	  * -1 will not use any inputsocket to determine the datatype, but use the outputsocket
	  * default datatype.
	  */
    int inputSocketDataTypeDeterminatorIndex;

	ChannelInfo channelinfo[4];
    void removeFirstConnection();
public:
	OutputSocket(DataType datatype);
	OutputSocket(DataType datatype, int inputSocketDataTypeDeterminatorIndex);
	OutputSocket(OutputSocket * from);
    ~OutputSocket();
    void addConnection(SocketConnection *connection);
    SocketConnection* getConnection(unsigned int index) {return this->connections[index];}
    const int isConnected() const;
    int isOutputSocket() const;

	/**
	  * @brief determine the resolution of this socket
	  * @param resolution the result of this operation
	  * @param preferredResolution the preferrable resolution as no resolution could be determined
	  */
    void determineResolution(unsigned int resolution[], unsigned int preferredResolution[]);

	/**
	  * @brief determine the actual data type and channel info.
	  */
    void determineActualDataType();
    void relinkConnections(OutputSocket *relinkToSocket) {this->relinkConnections(relinkToSocket, false);};
    void relinkConnections(OutputSocket *relinkToSocket, bool single);
    void setGroupInputSocket(InputSocket* groupInput) {this->groupInput = groupInput;}
    InputSocket* getGroupInputSocket() {return this->groupInput;}
    bool isActualDataTypeDeterminedByInputSocket() {
        return this->inputSocketDataTypeDeterminatorIndex>-1;
    }
	const int getNumberOfConnections() {return connections.size();}

	/**
	  * @brief get the index of the inputsocket that determines the datatype of this outputsocket
	  */
    int getInputSocketDataTypeDeterminatorIndex() {return this->inputSocketDataTypeDeterminatorIndex;}
    void clearConnections();

	/**
	  * @brief find a connected write buffer operation to this OutputSocket
	  * @return WriteBufferOperation or NULL
	  */
	WriteBufferOperation* findAttachedWriteBufferOperation() const;
	ChannelInfo* getChannelInfo(const int channelnumber);

	/**
	  * @brief trigger determine actual data type to all connected sockets
	  * @note will only be triggered just after the actual data type is set.
	  */
	void fireActualDataType();

private:

};
#endif

#include "COM_ExecutionSystem.h"

#include "PIL_time.h"
#include "BKE_node.h"
#include "COM_Converter.h"
#include <sstream>
#include "COM_NodeOperation.h"
#include "COM_ExecutionGroup.h"
#include "COM_NodeBase.h"
#include "COM_WorkScheduler.h"
#include "COM_ReadBufferOperation.h"
#include "COM_MemoryManager.h"
#include "stdio.h"
#include "COM_GroupNode.h"
#include "COM_WriteBufferOperation.h"
#include "COM_ReadBufferOperation.h"
#include "COM_ExecutionSystemHelper.h"

ExecutionSystem::ExecutionSystem(bNodeTree* editingtree, bool rendering) {
	this->starttime = PIL_check_seconds_timer();
	this->context.setbNodeTree(editingtree);
	printf("starttime %f\n", starttime);

	/* initialize the CompositorContext */
	if (rendering) {
		context.setQuality((CompositorQuality)editingtree->render_quality);
	} else {
		context.setQuality((CompositorQuality)editingtree->edit_quality);
	}
	context.setRendering(rendering);
	context.setHasActiveOpenCLDevices(WorkScheduler::hasGPUDevices() && (editingtree->flag & NTREE_COM_OPENCL));

	Node* mainOutputNode=NULL;
	printf(" - %f convert to editor tree\n", PIL_check_seconds_timer()-starttime);

	mainOutputNode = ExecutionSystemHelper::addbNodeTree(this->getNodes(), this->getConnections(), editingtree);

	if (mainOutputNode) {
		context.setScene((Scene*)mainOutputNode->getbNode()->id);

		printf(" - %f ungroup\n", PIL_check_seconds_timer()-starttime);
		ExecutionSystemHelper::ungroup(*this); /* copy subtrees of GroupNode in main tree, so only the main tree needs to be evaluated (reduces complexity) */
		printf(" - %f convert to operations\n", PIL_check_seconds_timer()-starttime);
		this->convertToOperations();

		printf(" - %f group operations to execution groups\n", PIL_check_seconds_timer()-starttime);
		this->groupOperations(); /* group operations in ExecutionGroups */


		printf(" - %f determine resolutions\n", PIL_check_seconds_timer()-starttime);
		vector<ExecutionGroup*> executionGroups;
		this->findOutputExecutionGroup(&executionGroups);
		unsigned int index;
		unsigned int resolution[2];
		for (index = 0 ; index < executionGroups.size(); index ++) {
			resolution[0]=0;
			resolution[1]=0;
			ExecutionGroup* executionGroup = executionGroups[index];
			executionGroup->determineResolution(resolution);
		}
	}
}

ExecutionSystem::~ExecutionSystem() {
	unsigned int index;
	for(index = 0; index < this->connections.size(); index++) {
		SocketConnection* connection = this->connections[index];
		delete connection;
	}
	this->connections.clear();
	for(index = 0; index < this->nodes.size(); index++) {
		Node* node = this->nodes[index];
		delete node;
	}
	this->nodes.clear();
	for(index = 0; index < this->operations.size(); index++) {
		NodeOperation* operation = this->operations[index];
		delete operation;
	}
	this->operations.clear();
	for(index = 0; index < this->groups.size(); index++) {
		ExecutionGroup* group = this->groups[index];
		delete group;
	}
	this->groups.clear();
}

void ExecutionSystem::execute() {
	unsigned int order = 0;
	for( vector<NodeOperation*>::iterator iter = this->operations.begin(); iter != operations.end(); ++iter ) {
		NodeBase* node = *iter;
		NodeOperation *operation = (NodeOperation*) node;
		if (operation->isReadBufferOperation()) {
			ReadBufferOperation * readOperation = (ReadBufferOperation*)operation;
			readOperation->setOffset(order);
			order ++;
		}
	}

	MemoryManager::initialize();
	unsigned int index;

	for (index = 0 ; index < this->operations.size() ; index ++) {
		NodeOperation * operation = this->operations[index];
		operation->initExecution();
	}
	for (index = 0 ; index < this->groups.size() ; index ++) {
		ExecutionGroup * executionGroup = this->groups[index];
		executionGroup->setChunksize(context.getChunksize());
		executionGroup->initExecution();
	}

	WorkScheduler::start(this->context);


	vector<ExecutionGroup*> executionGroups;
	this->findOutputExecutionGroup(&executionGroups);

	/* start execution of the ExecutionGroups based on priority of their output node */
	for (int priority = 9 ; priority>=0 ; priority--) {
		printf(" - %f executing priority %d groups\n", PIL_check_seconds_timer()-starttime, priority);

		for (index = 0 ; index < executionGroups.size(); index ++) {
			ExecutionGroup* group = executionGroups[index];
			NodeOperation* output = group->getOutputNodeOperation();
			if (output->getRenderPriority() == priority) {
				group->execute(this);
			}
		}
	}
	printf(" - %f clean up execution \n", PIL_check_seconds_timer()-starttime);


	WorkScheduler::finish();
	WorkScheduler::stop();

	for (index = 0 ; index < this->operations.size() ; index ++) {
		NodeOperation * operation = this->operations[index];
		operation->deinitExecution();
	}
	for (index = 0 ; index < this->groups.size() ; index ++) {
		ExecutionGroup * executionGroup = this->groups[index];
		executionGroup->deinitExecution();
	}
	MemoryManager::clear();
}

void ExecutionSystem::addOperation(NodeOperation *operation) {
	ExecutionSystemHelper::addOperation(this->operations, operation);
}

void ExecutionSystem::addReadWriteBufferOperations(NodeOperation *operation) {
	// for every input add write and read operation if input is not a read operation
	// only add read operation to other links when they are attached to buffered operations.
	unsigned int index;
	for (index = 0 ; index < operation->getNumberOfInputSockets();index++) {
		InputSocket* inputsocket = operation->getInputSocket(index);
		if (inputsocket->isConnected()) {
			SocketConnection *connection = inputsocket->getConnection();
			NodeOperation* otherEnd = (NodeOperation*)connection->getFromNode();
			if (!otherEnd->isReadBufferOperation()) {
				// check of other end already has write operation
				OutputSocket *fromsocket = connection->getFromSocket();
				WriteBufferOperation * writeoperation = fromsocket->findAttachedWriteBufferOperation();
				if (writeoperation == NULL) {
					writeoperation = new WriteBufferOperation();
					writeoperation->setbNodeTree(this->getContext().getbNodeTree());
					this->addOperation(writeoperation);
					ExecutionSystemHelper::addLink(this->getConnections(), fromsocket, writeoperation->getInputSocket(0));
				}
				ReadBufferOperation *readoperation = new ReadBufferOperation();
				readoperation->setMemoryProxy(writeoperation->getMemoryProxy());
				connection->setFromSocket(readoperation->getOutputSocket());
				readoperation->getOutputSocket()->addConnection(connection);
				this->addOperation(readoperation);
			}
		}
	}
	/*
		link the outputsocket to a write operation
		link the writeoperation to a read operation
		link the read operation to the next node.
	*/
	OutputSocket * outputsocket = operation->getOutputSocket();
	if (outputsocket->isConnected()) {
		int index;
		WriteBufferOperation *writeOperation;
		writeOperation = new WriteBufferOperation();
		writeOperation->setbNodeTree(this->getContext().getbNodeTree());
		this->addOperation(writeOperation);
		for (index = 0 ; index < outputsocket->getNumberOfConnections();index ++) {
			SocketConnection * connection = outputsocket->getConnection(index);
			ReadBufferOperation* readoperation = new ReadBufferOperation();
			readoperation->setMemoryProxy(writeOperation->getMemoryProxy());
			connection->setFromSocket(readoperation->getOutputSocket());
			readoperation->getOutputSocket()->addConnection(connection);
			this->addOperation(readoperation);
		}
		ExecutionSystemHelper::addLink(this->getConnections(), outputsocket, writeOperation->getInputSocket(0));
	}
}

void ExecutionSystem::convertToOperations() {
	unsigned int index;
	// first determine data types of the nodes, this can be used by the node to convert to a different operation system
	this->determineActualSocketDataTypes((vector<NodeBase*>&)this->nodes);
	for(index = 0; index < this->nodes.size(); index++) {
		Node* node = (Node*)this->nodes[index];
		node->convertToOperations(this, &this->context);
	}

	// update the socket types of the operations. this will be used to add conversion operations in the system
	this->determineActualSocketDataTypes((vector<NodeBase*>&)this->operations);
	for (index = 0 ; index < this->connections.size(); index ++) {
		SocketConnection *connection = this->connections[index];
		if (connection->isValid()) {
			if (connection->getFromSocket()->getActualDataType() != connection->getToSocket()->getActualDataType()) {
				Converter::convertDataType(connection, this);
			}
		}
	}

	// determine all resolutions of the operations (Width/Height)
	for (index = 0 ; index < this->operations.size(); index ++) {
		NodeOperation* operation= this->operations[index];
		if (operation->isOutputOperation(context.isRendering())) {
			unsigned int resolution[2] = {0,0};
			unsigned int preferredResolution[2] = {0,0};
			operation->determineResolution(resolution, preferredResolution);
			operation->setResolution(resolution);
		}
	}

	// add convert resolution operations when needed.
	for (index = 0 ; index < this->connections.size(); index ++) {
		SocketConnection *connection = this->connections[index];
		if (connection->isValid()) {
			if (connection->needsResolutionConversion()) {
				Converter::convertResolution(connection, this);
			}
		}
	}

}

void ExecutionSystem::groupOperations() {
	vector<NodeOperation*> outputOperations;
	NodeOperation * operation;
	unsigned int index;
	// surround complex operations with ReadBufferOperation and WriteBufferOperation
	for(index = 0; index < this->operations.size(); index++) {
		operation = this->operations[index];
		if (operation->isComplex()) {
			this->addReadWriteBufferOperations(operation);
		}
	}
	ExecutionSystemHelper::findOutputNodeOperations(&outputOperations, this->getOperations(), this->context.isRendering());
	for( vector<NodeOperation*>::iterator iter = outputOperations.begin(); iter != outputOperations.end(); ++iter ) {
		operation = *iter;
		ExecutionGroup *group = new ExecutionGroup();
		group->addOperation(this, operation);
		group->setOutputExecutionGroup(true);
		ExecutionSystemHelper::addExecutionGroup(this->getExecutionGroups(), group);
	}
}

void ExecutionSystem::addSocketConnection(SocketConnection *connection)
{
	this->connections.push_back(connection);
}


void ExecutionSystem::determineActualSocketDataTypes(vector<NodeBase*> &nodes) {
	unsigned int index;
	/* first do all input nodes */
	for(index = 0; index < nodes.size(); index++) {
		NodeBase* node = nodes[index];
		if (node->isInputNode()) {
			node->determineActualSocketDataTypes();
		}
	}

	/* then all other nodes */
	for(index = 0; index < nodes.size(); index++) {
		NodeBase* node = nodes[index];
		if (!node->isInputNode()) {
			node->determineActualSocketDataTypes();
		}
	}
}

void ExecutionSystem::findOutputExecutionGroup(vector<ExecutionGroup*> *result) const {
	unsigned int index;
	for (index = 0 ; index < this->groups.size() ; index ++) {
		ExecutionGroup* group = this->groups[index];
		if (group->isOutputExecutionGroup()) {
			result->push_back(group);
		}
	}
}

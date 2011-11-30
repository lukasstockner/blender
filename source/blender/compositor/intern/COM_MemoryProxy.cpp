#include "COM_MemoryProxy.h"


MemoryProxy::MemoryProxy() {
	this->state = NULL;
	this->writeBufferOperation = NULL;
	this->executor = NULL;
}

MemoryProxy::~MemoryProxy() {
	if (this->state) {
		delete this->state;
		this->state = NULL;
	}
}

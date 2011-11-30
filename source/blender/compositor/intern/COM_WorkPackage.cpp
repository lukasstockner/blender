#include "COM_WorkPackage.h"

WorkPackage::WorkPackage(ExecutionGroup *group, unsigned int chunkNumber) {
    this->executionGroup = group;
	this->chunkNumber = chunkNumber;
}

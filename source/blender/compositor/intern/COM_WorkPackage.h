class WorkPackage;

#ifndef _COM_WorkPackage_h_
#define _COM_WorkPackage_h_

#include "COM_ExecutionGroup.h"

/**
  * @brief contains data about work that can be scheduled
  * @see WorkScheduler
  */
class WorkPackage {
private:
	/**
	  * @brief executionGroup with the operations-setup to be evaluated
	  */
	ExecutionGroup* executionGroup;

	/**
	  * @brief number of the chunk to be executed
	  */
	unsigned int chunkNumber;
public:
	/**
	  * @constructor
	  * @param group the ExecutionGroup
	  * @param chunkNumber the number of the chunk
	  */
	WorkPackage(ExecutionGroup* group, unsigned int chunkNumber);

	/**
	  * @brief get the ExecutionGroup
	  */
	ExecutionGroup* getExecutionGroup() const {return this->executionGroup;}

	/**
	  * @brief get the number of the chunk
	  */
	unsigned int getChunkNumber() const {return this->chunkNumber;}
};

#endif

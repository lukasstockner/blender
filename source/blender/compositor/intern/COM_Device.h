#ifndef _COM_Device_h
#define _COM_Device_h

#include "COM_ExecutionSystem.h"
#include "COM_WorkPackage.h"
#include "COM_NodeOperation.h"
#include "BLI_rect.h"
#include "COM_MemoryBuffer.h"

/**
  * @brief Abstract class for device implementations to be used by the Compositor.
  * devices are queried, initialized and used by the WorkScheduler.
  * work are packaged as a WorkPackage instance.
  */
class Device {
public:
	/**
	  * @brief initialize the device
	  */
	virtual bool initialize() {return true;}

	/**
	  * @brief deinitialize the device
	  */
	virtual void deinitialize() {}

	/**
	  * @brief execute a WorkPackage
	  * @param work the WorkPackage to execute
	  */
	virtual void execute(WorkPackage *work) = 0;

};

#endif

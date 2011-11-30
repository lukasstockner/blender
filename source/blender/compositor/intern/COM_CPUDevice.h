#ifndef _COM_CPUDevice_h
#define _COM_CPUDevice_h

#include "COM_Device.h"

/**
  * @brief class representing a CPU device. 
  * @note for every hardware thread in the system a CPUDevice instance will exist in the workscheduler
  */
class CPUDevice: public Device {
public:
	/**
	  * @brief execute a WorkPackage
	  * @param work the WorkPackage to execute
	  */
	void execute(WorkPackage *work);
};

#endif

class OpenCLDevice;

#ifndef _COM_OpenCLDevice_h
#define _COM_OpenCLDevice_h

#include "COM_Device.h"
#include "OCL_opencl.h"
#include "COM_WorkScheduler.h"


/**
  * @brief device representing an GPU OpenCL device.
  * an instance of this class represents a single cl_device
  */
class OpenCLDevice: public Device {
private:
    /**
      *@brief opencl context
      */
    cl_context context;
    
    /**
      *@brief opencl device
      */
    cl_device_id device;

	/**
      *@brief opencl program
      */
    cl_program program;
    
    /**
      *@brief opencl command queue
      */
    cl_command_queue queue;
public:
	/**
	  *@brief constructor with opencl device
	  *@param context
	  *@param device
	  */
	OpenCLDevice(cl_context context, cl_device_id device, cl_program program);
    
    
    /**
      * @brief initialize the device
	  * During initialization the OpenCL cl_command_queue is created
	  * the command queue is stored in the field queue.
	  * @see queue 
      */
    bool initialize();
    
	/**
	  * @brief deinitialize the device
	  * During deintiialization the command queue is cleared
	  */
	void deinitialize();
    
	/**
	  * @brief execute a WorkPackage
	  * @param work the WorkPackage to execute
	  */
	void execute(WorkPackage *work);
};

#endif

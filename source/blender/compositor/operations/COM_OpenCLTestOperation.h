#ifndef _COM_OpenCLTestOperation_h
#define _COM_OpenCLTestOperation_h
#include "COM_NodeOperation.h"


/**
  * this program converts an input colour to an output value.
  * it assumes we are in sRGB colour space.
  */
class OpenCLTestOperation : public NodeOperation {
private:
	cl_kernel openCLKernel;
	
public:
    /**
      * Default constructor
      */
    OpenCLTestOperation();

    /**
      * the inner loop of this program
      */
	void executePixel(float* color, float x, float y, MemoryBuffer *inputBuffers[]);
	void executeOpenCL(cl_context context,cl_program program, cl_command_queue queue, MemoryBuffer* outputMemoryBuffer, cl_mem clOutputBuffer, MemoryBuffer** inputMemoryBuffers, list<cl_mem> *clMemToCleanUp, list<cl_kernel> *clKernelsToCleanUp);
    void determineResolution(unsigned int resolution[], unsigned int preferredResolution[]);

};
#endif

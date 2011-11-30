#ifndef _COM_ConvertValueToColourProg_h
#define _COM_ConvertValueToColourProg_h
#include "COM_NodeOperation.h"


class ConvertValueToColourProg : public NodeOperation {
private:
    /**
      * Cached reference to the inputProgram
      */
	SocketReader* inputProgram;
public:
    ConvertValueToColourProg();

    /**
      * the inner loop of this program
      */
	void executePixel(float* color, float x, float y, MemoryBuffer *inputBuffers[]);

    /**
      * Initialize the execution
      */
    void initExecution();

    /**
      * Deinitialize the execution
      */
    void deinitExecution();

};
#endif

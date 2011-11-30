#ifndef _COM_IDMaskOperation_h
#define _COM_IDMaskOperation_h
#include "COM_NodeOperation.h"


class IDMaskOperation : public NodeOperation {
private:
    /**
      * Cached reference to the inputProgram
      */
	SocketReader* inputProgram;

    float objectIndex;
public:
    IDMaskOperation();

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

    void setObjectIndex(float objectIndex) {this->objectIndex = objectIndex;}

};
#endif

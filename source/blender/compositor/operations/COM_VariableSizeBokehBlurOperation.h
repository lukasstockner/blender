#ifndef _COM_BokehVariableSizeBokehBlurOperation_h
#define _COM_VariableSizeBokehBlurOperation_h
#include "COM_NodeOperation.h"

class VariableSizeBokehBlurOperation : public NodeOperation {
private:
	int radx, rady;
	SocketReader* inputProgram;
	SocketReader* inputBokehProgram;
	SocketReader* inputSizeProgram;

public:
	VariableSizeBokehBlurOperation();

	void* initializeTileData(rcti *rect, MemoryBuffer **memoryBuffers);
    /**
      * the inner loop of this program
      */
	void executePixel(float* color, int x, int y, MemoryBuffer *inputBuffers[], void* data);

    /**
      * Initialize the execution
      */
    void initExecution();

    /**
      * Deinitialize the execution
      */
    void deinitExecution();

    bool determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output);


};
#endif

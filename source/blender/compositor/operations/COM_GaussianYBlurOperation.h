#ifndef _COM_GaussianYBlurOperation_h
#define _COM_GaussianYBlurOperation_h
#include "COM_NodeOperation.h"
#include "COM_BlurBaseOperation.h"

class GaussianYBlurOperation : public BlurBaseOperation {
private:
	float* gausstab;
	int rad;
public:
	GaussianYBlurOperation();

    /**
      * the inner loop of this program
      */
	void executePixel(float* color, int x, int y, MemoryBuffer *inputBuffers[], void *data);

    /**
      * Initialize the execution
      */
    void initExecution();

    /**
      * Deinitialize the execution
      */
    void deinitExecution();

	void* initializeTileData(rcti *rect, MemoryBuffer **memoryBuffers);
	bool determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output);
};
#endif

#ifndef _COM_GaussianXBlurOperation_h
#define _COM_GaussianXBlurOperation_h
#include "COM_NodeOperation.h"
#include "COM_BlurBaseOperation.h"

class GaussianXBlurOperation : public BlurBaseOperation {
private:
	float* gausstab;
	int rad;
public:
	GaussianXBlurOperation();

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

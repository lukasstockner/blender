#ifndef _COM_BokehGaussianBokehBlurOperation_h
#define _COM_GaussianBokehBlurOperation_h
#include "COM_NodeOperation.h"
#include "COM_BlurBaseOperation.h"
#include "COM_QualityStepHelper.h"

class GaussianBokehBlurOperation : public BlurBaseOperation {
private:
	float* gausstab;
	int radx, rady;

public:
	GaussianBokehBlurOperation();

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

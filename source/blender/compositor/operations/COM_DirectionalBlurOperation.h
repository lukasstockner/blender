#ifndef _COM_BokehDirectionalBlurOperation_h
#define _COM_DirectionalBlurOperation_h
#include "COM_NodeOperation.h"
#include "COM_QualityStepHelper.h"

class DirectionalBlurOperation : public NodeOperation, public QualityStepHelper {
private:
	SocketReader* inputProgram;
	NodeDBlurData* data;

	float center_x_pix, center_y_pix;
	float tx, ty;
	float sc, rot;

public:
	DirectionalBlurOperation();

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

	void setData(NodeDBlurData *data) {this->data = data;}
};
#endif

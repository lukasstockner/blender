#ifndef _COM_BoxMaskOperation_h
#define _COM_BoxMaskOperation_h
#include "COM_NodeOperation.h"


class BoxMaskOperation : public NodeOperation {
private:
    /**
      * Cached reference to the inputProgram
      */
	SocketReader * inputMask;
	SocketReader * inputValue;

    float sine;
    float cosine;
    float aspectRatio;
    int maskType;

    NodeBoxMask *data;
public:
    BoxMaskOperation();

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

    void setData(NodeBoxMask *data) {this->data = data;}

    void setMaskType(int maskType) {this->maskType = maskType;}

};
#endif

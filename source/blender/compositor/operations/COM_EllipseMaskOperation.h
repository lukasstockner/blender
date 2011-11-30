#ifndef _COM_EllipseMaskOperation_h
#define _COM_EllipseMaskOperation_h
#include "COM_NodeOperation.h"


class EllipseMaskOperation : public NodeOperation {
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

    NodeEllipseMask *data;
public:
    EllipseMaskOperation();

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

    void setData(NodeEllipseMask *data) {this->data = data;}

    void setMaskType(int maskType) {this->maskType = maskType;}

};
#endif

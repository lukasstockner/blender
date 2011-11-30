#ifndef _COM_ColorCorrectionOperation_h
#define _COM_ColorCorrectionOperation_h
#include "COM_NodeOperation.h"


class ColorCorrectionOperation : public NodeOperation {
private:
    /**
      * Cached reference to the inputProgram
      */
	SocketReader * inputImage;
	SocketReader* inputMask;
    NodeColorCorrection *data;

    bool redChannelEnabled;
    bool greenChannelEnabled;
    bool blueChannelEnabled;

public:
    ColorCorrectionOperation();

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

    void setData(NodeColorCorrection * data) {this->data = data;}
    void setRedChannelEnabled(bool enabled) {this->redChannelEnabled = enabled;}
    void setGreenChannelEnabled(bool enabled) {this->greenChannelEnabled = enabled;}
    void setBlueChannelEnabled(bool enabled) {this->blueChannelEnabled = enabled;}
};
#endif

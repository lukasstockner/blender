#ifndef _COM_ChangeHSVOperation_h
#define _COM_ChangeHSVOperation_h
#include "COM_MixBaseOperation.h"


/**
  * this program converts an input colour to an output value.
  * it assumes we are in sRGB colour space.
  */
class ChangeHSVOperation : public NodeOperation {
private:
	SocketReader * inputOperation;

    float hue;
    float saturation;
    float value;

public:
    /**
      * Default constructor
      */
    ChangeHSVOperation();

    void initExecution();
    void deinitExecution();

    /**
      * the inner loop of this program
      */
	void executePixel(float* color, float x, float y, MemoryBuffer *inputBuffers[]);

    void setHue(float hue) {this->hue = hue;}
    void setSaturation(float saturation) {this->saturation = saturation;}
    void setValue(float value) {this->value = value;}

};
#endif

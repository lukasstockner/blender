#ifndef _COM_SetColorOperation_h
#define _COM_SetColorOperation_h
#include "COM_NodeOperation.h"


/**
  * this program converts an input colour to an output value.
  * it assumes we are in sRGB colour space.
  */
class SetColorOperation : public NodeOperation {
private:
    float channel1;
    float channel2;
    float channel3;
    float channel4;

public:
    /**
      * Default constructor
      */
    SetColorOperation();

    const float getChannel1() {return this->channel1;}
    void setChannel1(float value) {this->channel1 = value;}
    const float getChannel2() {return this->channel2;}
    void setChannel2(float value) {this->channel2 = value;}
    const float getChannel3() {return this->channel3;}
    void setChannel3(float value) {this->channel3 = value;}
    const float getChannel4() {return this->channel4;}
	void setChannel4(float value) {this->channel4 = value;}
	void setChannels(float value[4]) {this->channel1 = value[0];this->channel2 = value[1];this->channel3 = value[2];this->channel4 = value[3];}

    /**
      * the inner loop of this program
      */
	void executePixel(float* color, float x, float y, MemoryBuffer *inputBuffers[]);

    void determineResolution(unsigned int resolution[], unsigned int preferredResolution[]);
	const bool isSetOperation() const {return true;}

};
#endif

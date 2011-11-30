#ifndef _COM_SetVectorOperation_h
#define _COM_SetVectorOperation_h
#include "COM_NodeOperation.h"


/**
  * this program converts an input colour to an output value.
  * it assumes we are in sRGB colour space.
  */
class SetVectorOperation : public NodeOperation {
private:
    float x;
    float y;
    float z;
    float w;

public:
    /**
      * Default constructor
      */
    SetVectorOperation();

    const float getX() {return this->x;}
    void setX(float value) {this->x = value;}
    const float getY() {return this->y;}
    void setY(float value) {this->y = value;}
    const float getZ() {return this->z;}
    void setZ(float value) {this->z = value;}
    const float getW() {return this->w;}
    void setW(float value) {this->w = value;}

    /**
      * the inner loop of this program
      */
	void executePixel(float* color, float x, float y, MemoryBuffer *inputBuffers[]);

    void determineResolution(unsigned int resolution[], unsigned int preferredResolution[]);
	const bool isSetOperation() const {return true;}

};
#endif

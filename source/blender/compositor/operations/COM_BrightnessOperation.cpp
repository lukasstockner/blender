#include "COM_BrightnessOperation.h"
#include "COM_InputSocket.h"
#include "COM_OutputSocket.h"

BrightnessOperation::BrightnessOperation(): NodeOperation() {
    this->addInputSocket(*(new InputSocket(COM_DT_COLOR)));
    this->addInputSocket(*(new InputSocket(COM_DT_VALUE)));
    this->addInputSocket(*(new InputSocket(COM_DT_VALUE)));
    this->addOutputSocket(*(new OutputSocket(COM_DT_VALUE)));
    this->inputProgram = NULL;
}
void BrightnessOperation::initExecution() {
	this->inputProgram = this->getInputSocketReader(0);
	this->inputBrightnessProgram = this->getInputSocketReader(1);
	this->inputContrastProgram = this->getInputSocketReader(2);
}

void BrightnessOperation::executePixel(float* color, float x, float y, MemoryBuffer *inputBuffers[]) {
    float inputValue[4];
    float a, b;
    float inputBrightness[4];
    float inputContrast[4];
	this->inputProgram->read(inputValue, x, y, inputBuffers);
	this->inputBrightnessProgram->read(inputBrightness, x, y, inputBuffers);
	this->inputContrastProgram->read(inputContrast, x, y, inputBuffers);
    float brightness = inputBrightness[0];
    float contrast = inputContrast[0];
    brightness /= 100.0f;
    float delta = contrast / 200.0f;
    a = 1.0f - delta * 2.0f;
    /*
    * The algorithm is by Werner D. Streidt
    * (http://visca.com/ffactory/archives/5-99/msg00021.html)
    * Extracted of OpenCV demhist.c
    */
    if( contrast > 0 )
    {
            a = 1.0f / a;
            b = a * (brightness - delta);
    }
    else
    {
            delta *= -1;
            b = a * (brightness + delta);
    }

    color[0] = a*inputValue[0]+b;
    color[1] = a*inputValue[1]+b;
    color[2] = a*inputValue[2]+b;
    color[3] = inputValue[3];
}

void BrightnessOperation::deinitExecution() {
    this->inputProgram = NULL;
    this->inputBrightnessProgram = NULL;
    this->inputContrastProgram = NULL;
}


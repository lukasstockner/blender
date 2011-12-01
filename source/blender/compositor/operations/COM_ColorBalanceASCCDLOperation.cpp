#include "COM_ColorBalanceASCCDLOperation.h"
#include "COM_InputSocket.h"
#include "COM_OutputSocket.h"
#include "BLI_math.h"

inline float colorbalance_cdl(float in, float offset, float power, float slope)
{
        float x = in * slope + offset;

        /* prevent NaN */
        CLAMP(x, 0.0, 1.0);

        return powf(x, power);
}

ColorBalanceASCCDLOperation::ColorBalanceASCCDLOperation(): NodeOperation() {
    this->addInputSocket(COM_DT_VALUE);
    this->addInputSocket(COM_DT_COLOR);
    this->addOutputSocket(COM_DT_COLOR);
    this->inputValueOperation = NULL;
    this->inputColorOperation = NULL;
    this->setResolutionInputSocketIndex(1);
}

void ColorBalanceASCCDLOperation::initExecution() {
	this->inputValueOperation = this->getInputSocketReader(0);
	this->inputColorOperation = this->getInputSocketReader(1);
}

void ColorBalanceASCCDLOperation::executePixel(float* outputColor, float x, float y, MemoryBuffer *inputBuffers[]) {
    float inputColor[4];
    float value[4];

	inputValueOperation->read(value, x, y, inputBuffers);
	inputColorOperation->read(inputColor, x, y, inputBuffers);

    float fac = value[0];
    fac = min(1.0f, fac);
    const float mfac= 1.0f - fac;

    outputColor[0] = mfac*inputColor[0] + fac * colorbalance_cdl(inputColor[0], this->lift[0], this->gamma[0], this->gain[0]);
    outputColor[1] = mfac*inputColor[1] + fac * colorbalance_cdl(inputColor[1], this->lift[1], this->gamma[1], this->gain[1]);
    outputColor[2] = mfac*inputColor[2] + fac * colorbalance_cdl(inputColor[2], this->lift[2], this->gamma[2], this->gain[2]);
    outputColor[3] = inputColor[3];

}

void ColorBalanceASCCDLOperation::deinitExecution() {
    this->inputValueOperation = NULL;
    this->inputColorOperation = NULL;
}

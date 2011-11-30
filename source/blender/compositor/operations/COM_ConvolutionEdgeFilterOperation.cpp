#include "COM_ConvolutionEdgeFilterOperation.h"
#include "BLI_math.h"

ConvolutionEdgeFilterOperation::ConvolutionEdgeFilterOperation() : ConvolutionFilterOperation() {
}
inline void addFilter(float* result, float*input, float value) {
    result[0] += input[0] * value;
    result[1] += input[1] * value;
    result[2] += input[2] * value;
}

void ConvolutionEdgeFilterOperation::executePixel(float *color,float x, float y, MemoryBuffer *inputBuffers[]) {
    float in1[4],in2[4], res1[4], res2[4];

    float value[4];
    this->inputValueOperation->read(value, x, y, inputBuffers);
    float mval = 1.0f - value[0];

    res1[0] = 0.0f;
    res1[1] = 0.0f;
    res1[2] = 0.0f;
    res1[3] = 0.0f;
    res2[0] = 0.0f;
    res2[1] = 0.0f;
    res2[2] = 0.0f;
    res2[3] = 0.0f;

    this->inputOperation->read(in1, x-1, y-1, inputBuffers);
    addFilter(res1, in1, this->filter[0]);
    addFilter(res2, in1, this->filter[0]);

    this->inputOperation->read(in1, x, y-1, inputBuffers);
    addFilter(res1, in1, this->filter[1]);
    addFilter(res2, in1, this->filter[3]);

    this->inputOperation->read(in1, x+1, y-1, inputBuffers);
    addFilter(res1, in1, this->filter[2]);
    addFilter(res2, in1, this->filter[6]);

    this->inputOperation->read(in1, x-1, y, inputBuffers);
    addFilter(res1, in1, this->filter[3]);
    addFilter(res2, in1, this->filter[1]);

    this->inputOperation->read(in2, x, y, inputBuffers);
    addFilter(res1, in2, this->filter[4]);
    addFilter(res2, in2, this->filter[4]);

    this->inputOperation->read(in1, x+1, y, inputBuffers);
    addFilter(res1, in1, this->filter[5]);
    addFilter(res2, in1, this->filter[7]);

    this->inputOperation->read(in1, x-1, y+1, inputBuffers);
    addFilter(res1, in1, this->filter[6]);
    addFilter(res2, in1, this->filter[2]);

    this->inputOperation->read(in1, x, y+1, inputBuffers);
    addFilter(res1, in1, this->filter[7]);
    addFilter(res2, in1, this->filter[5]);

    this->inputOperation->read(in1, x+1, y+1, inputBuffers);
    addFilter(res1, in1, this->filter[8]);
    addFilter(res2, in1, this->filter[8]);

    color[0] = sqrt(res1[0]*res1[0]+res2[0]*res2[0]);
    color[1] = sqrt(res1[1]*res1[1]+res2[1]*res2[1]);
    color[2] = sqrt(res1[2]*res1[2]+res2[2]*res2[2]);

    color[0] = color[0]*value[0] + in2[0] * mval;
    color[1] = color[1]*value[0] + in2[1] * mval;
    color[2] = color[2]*value[0] + in2[2] * mval;

    color[3] = in2[3];
}

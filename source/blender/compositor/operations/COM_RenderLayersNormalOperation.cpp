#include "COM_RenderLayersNormalOperation.h"

RenderLayersNormalOperation::RenderLayersNormalOperation() :RenderLayersBaseProg(SCE_PASS_NORMAL, 3) {
    this->addOutputSocket(*(new OutputSocket(COM_DT_VECTOR)));
}

//void RenderLayersNormalOperation::executePixel(float* output, float x, float y, MemoryBuffer *inputBuffers[]) {
//    float * inputBuffer = this->getInputBuffer();
//    unsigned int offset = (y*this->getWidth()+x) *3;
//    output[0] = inputBuffer[offset];
//    output[1] = inputBuffer[offset+1];
//    output[2] = inputBuffer[offset+2];
//}

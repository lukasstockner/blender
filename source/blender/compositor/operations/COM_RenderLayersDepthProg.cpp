#include "COM_RenderLayersDepthProg.h"

RenderLayersDepthProg::RenderLayersDepthProg() :RenderLayersBaseProg(SCE_PASS_Z, 1) {
    this->addOutputSocket(*(new OutputSocket(COM_DT_VALUE)));
}

//void RenderLayersDepthProg::executePixel(float* output, float x, float y, MemoryBuffer *inputBuffers[]) {
//    float * inputBuffer = this->getInputBuffer();
//    unsigned int offset = (y*getWidth()+x);
//    output[0] = inputBuffer[offset];
//}

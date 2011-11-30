#include "COM_RenderLayersSpeedOperation.h"

RenderLayersSpeedOperation::RenderLayersSpeedOperation() :RenderLayersBaseProg(SCE_PASS_VECTOR, 4) {
    this->addOutputSocket(*(new OutputSocket(COM_DT_VECTOR)));
}

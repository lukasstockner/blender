#include "COM_RenderLayersSpeedOperation.h"

RenderLayersSpeedOperation::RenderLayersSpeedOperation() :RenderLayersBaseProg(SCE_PASS_VECTOR, 4) {
    this->addOutputSocket(COM_DT_VECTOR);
}

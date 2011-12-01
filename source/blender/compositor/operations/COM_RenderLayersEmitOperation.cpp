#include "COM_RenderLayersEmitOperation.h"

RenderLayersEmitOperation::RenderLayersEmitOperation() :RenderLayersBaseProg(SCE_PASS_EMIT, 3) {
    this->addOutputSocket(COM_DT_COLOR);
}

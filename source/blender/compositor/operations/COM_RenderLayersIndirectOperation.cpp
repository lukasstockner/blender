#include "COM_RenderLayersIndirectOperation.h"

RenderLayersIndirectOperation::RenderLayersIndirectOperation() :RenderLayersBaseProg(SCE_PASS_INDIRECT, 3) {
    this->addOutputSocket(*(new OutputSocket(COM_DT_COLOR)));
}

#include "COM_RenderLayersIndirectOperation.h"

RenderLayersIndirectOperation::RenderLayersIndirectOperation() :RenderLayersBaseProg(SCE_PASS_INDIRECT, 3) {
    this->addOutputSocket(COM_DT_COLOR);
}

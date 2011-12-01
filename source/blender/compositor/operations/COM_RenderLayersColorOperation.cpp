#include "COM_RenderLayersColorOperation.h"

RenderLayersColorOperation::RenderLayersColorOperation() :RenderLayersBaseProg(SCE_PASS_RGBA, 4) {
    this->addOutputSocket(COM_DT_COLOR);
}

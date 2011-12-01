#include "COM_RenderLayersSpecularOperation.h"

RenderLayersSpecularOperation::RenderLayersSpecularOperation() :RenderLayersBaseProg(SCE_PASS_SPEC, 3) {
    this->addOutputSocket(COM_DT_COLOR);
}

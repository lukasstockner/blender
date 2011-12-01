#include "COM_RenderLayersUVOperation.h"

RenderLayersUVOperation::RenderLayersUVOperation() :RenderLayersBaseProg(SCE_PASS_UV, 3) {
    this->addOutputSocket(COM_DT_VECTOR);
}

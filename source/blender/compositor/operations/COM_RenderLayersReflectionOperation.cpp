#include "COM_RenderLayersReflectionOperation.h"

RenderLayersReflectionOperation::RenderLayersReflectionOperation() :RenderLayersBaseProg(SCE_PASS_REFLECT, 3) {
    this->addOutputSocket(*(new OutputSocket(COM_DT_COLOR)));
}

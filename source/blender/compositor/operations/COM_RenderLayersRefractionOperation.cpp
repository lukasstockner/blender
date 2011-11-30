#include "COM_RenderLayersRefractionOperation.h"

RenderLayersRefractionOperation::RenderLayersRefractionOperation() :RenderLayersBaseProg(SCE_PASS_REFRACT, 3) {
    this->addOutputSocket(*(new OutputSocket(COM_DT_COLOR)));
}

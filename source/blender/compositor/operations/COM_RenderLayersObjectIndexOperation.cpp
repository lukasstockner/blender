#include "COM_RenderLayersObjectIndexOperation.h"

RenderLayersObjectIndexOperation::RenderLayersObjectIndexOperation() :RenderLayersBaseProg(SCE_PASS_INDEXOB, 1) {
    this->addOutputSocket(*(new OutputSocket(COM_DT_VALUE)));
}

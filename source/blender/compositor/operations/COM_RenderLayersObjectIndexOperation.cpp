#include "COM_RenderLayersObjectIndexOperation.h"

RenderLayersObjectIndexOperation::RenderLayersObjectIndexOperation() :RenderLayersBaseProg(SCE_PASS_INDEXOB, 1) {
    this->addOutputSocket(COM_DT_VALUE);
}

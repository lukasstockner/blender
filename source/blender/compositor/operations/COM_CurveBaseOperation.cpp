#include "COM_CurveBaseOperation.h"
#include "COM_InputSocket.h"
#include "COM_OutputSocket.h"

#ifdef __cplusplus
extern "C" {
#endif
	#include "BKE_colortools.h"
#ifdef __cplusplus
}
#endif

CurveBaseOperation::CurveBaseOperation(): NodeOperation() {
	this->curveMapping = NULL;
}
void CurveBaseOperation::initExecution() {
	curvemapping_initialize(this->curveMapping);
}

#include "COM_HueSaturationValueCorrectOperation.h"
#include "COM_InputSocket.h"
#include "COM_OutputSocket.h"

#include "BLI_math.h"

#ifdef __cplusplus
extern "C" {
#endif
	#include "BKE_colortools.h"
#ifdef __cplusplus
}
#endif

HueSaturationValueCorrectOperation::HueSaturationValueCorrectOperation(): CurveBaseOperation() {
	this->addInputSocket(*(new InputSocket(COM_DT_COLOR)));
	this->addOutputSocket(*(new OutputSocket(COM_DT_COLOR)));

	this->inputProgram = NULL;
}
void HueSaturationValueCorrectOperation::initExecution() {
	CurveBaseOperation::initExecution();
	this->inputProgram = this->getInputSocketReader(0);
}

void HueSaturationValueCorrectOperation::executePixel(float* output, float x, float y, MemoryBuffer *inputBuffers[]) {
	float hsv[4], f;

	this->inputProgram->read(hsv, x, y, inputBuffers);

	/* adjust hue, scaling returned default 0.5 up to 1 */
	f = curvemapping_evaluateF(this->curveMapping, 0, hsv[0]);
	hsv[0] += f-0.5f;

	/* adjust saturation, scaling returned default 0.5 up to 1 */
	f = curvemapping_evaluateF(this->curveMapping, 1, hsv[0]);
	hsv[1] *= (f * 2.f);

	/* adjust value, scaling returned default 0.5 up to 1 */
	f = curvemapping_evaluateF(this->curveMapping, 2, hsv[0]);
	hsv[2] *= (f * 2.f);

	hsv[0] = hsv[0] - floor(hsv[0]);  /* mod 1.0 */
	CLAMP(hsv[1], 0.f, 1.f);

	output[0]= hsv[0];
	output[1]= hsv[1];
	output[2]= hsv[2];
	output[3]= hsv[3];
}

void HueSaturationValueCorrectOperation::deinitExecution() {
	this->inputProgram = NULL;
}

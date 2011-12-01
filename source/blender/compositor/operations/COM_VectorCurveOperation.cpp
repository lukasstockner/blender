#include "COM_VectorCurveOperation.h"
#include "COM_InputSocket.h"
#include "COM_OutputSocket.h"

#ifdef __cplusplus
extern "C" {
#endif
	#include "BKE_colortools.h"
#ifdef __cplusplus
}
#endif

VectorCurveOperation::VectorCurveOperation(): CurveBaseOperation() {
	this->addInputSocket(COM_DT_VECTOR);
	this->addOutputSocket(COM_DT_VECTOR);

	this->inputProgram = NULL;
}
void VectorCurveOperation::initExecution() {
	CurveBaseOperation::initExecution();
	this->inputProgram = this->getInputSocketReader(0);
}

void VectorCurveOperation::executePixel(float* output, float x, float y, MemoryBuffer *inputBuffers[]) {
	float input[4];


	this->inputProgram->read(input, x, y, inputBuffers);

	curvemapping_evaluate_premulRGBF(this->curveMapping, output, input);
	output[3]= input[3];
}

void VectorCurveOperation::deinitExecution() {
	this->inputProgram = NULL;
}

#include "COM_ColorCurveOperation.h"
#include "COM_InputSocket.h"
#include "COM_OutputSocket.h"

#ifdef __cplusplus
extern "C" {
#endif
	#include "BKE_colortools.h"
#ifdef __cplusplus
}
#endif

ColorCurveOperation::ColorCurveOperation(): CurveBaseOperation() {
	this->addInputSocket(COM_DT_VALUE);
	this->addInputSocket(COM_DT_COLOR);
	this->addInputSocket(COM_DT_COLOR);
	this->addInputSocket(COM_DT_COLOR);
	this->addOutputSocket(COM_DT_COLOR);

	this->inputFacProgram = NULL;
	this->inputImageProgram = NULL;
	this->inputBlackProgram = NULL;
	this->inputWhiteProgram = NULL;

	this->setResolutionInputSocketIndex(1);
}
void ColorCurveOperation::initExecution() {
	CurveBaseOperation::initExecution();
	this->inputFacProgram = this->getInputSocketReader(0);
	this->inputImageProgram = this->getInputSocketReader(1);
	this->inputBlackProgram = this->getInputSocketReader(2);
	this->inputWhiteProgram = this->getInputSocketReader(3);

	curvemapping_premultiply(this->curveMapping, 0);

}

void ColorCurveOperation::executePixel(float* color, float x, float y, MemoryBuffer *inputBuffers[]) {
	float black[4];
	float white[4];
	float fac[4];
	float image[4];

	this->inputBlackProgram->read(black, x, y, inputBuffers);
	this->inputWhiteProgram->read(white, x, y, inputBuffers);

	curvemapping_set_black_white(this->curveMapping, black, white);

	this->inputFacProgram->read(fac, x, y, inputBuffers);
	this->inputImageProgram->read(image, x, y, inputBuffers);

	if(fac[0]>=1.0)
		curvemapping_evaluate_premulRGBF(this->curveMapping, color, image);
	else if(*fac<=0.0) {
		color[0]= image[0];
		color[1]= image[1];
		color[2]= image[2];
	}
	else {
		float col[4], mfac= 1.0f-*fac;
		curvemapping_evaluate_premulRGBF(this->curveMapping, col, image);
		color[0]= mfac*image[0] + *fac*col[0];
		color[1]= mfac*image[1] + *fac*col[1];
		color[2]= mfac*image[2] + *fac*col[2];
	}
	color[3]= image[3];
}

void ColorCurveOperation::deinitExecution() {
	this->inputFacProgram = NULL;
	this->inputImageProgram = NULL;
	this->inputBlackProgram = NULL;
	this->inputWhiteProgram = NULL;
	curvemapping_premultiply(this->curveMapping, 1);
}

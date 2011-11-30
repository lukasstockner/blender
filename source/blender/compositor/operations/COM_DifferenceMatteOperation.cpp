#include "COM_DifferenceMatteOperation.h"
#include "COM_InputSocket.h"
#include "COM_OutputSocket.h"
#include "BLI_math.h"

DifferenceMatteOperation::DifferenceMatteOperation(): NodeOperation() {
	addInputSocket(*(new InputSocket(COM_DT_COLOR)));
	addInputSocket(*(new InputSocket(COM_DT_COLOR)));
	addOutputSocket(*(new OutputSocket(COM_DT_VALUE)));

	inputImage1Program = NULL;
	inputImage2Program = NULL;
}

void DifferenceMatteOperation::initExecution() {
	this->inputImage1Program = this->getInputSocketReader(0);
	this->inputImage2Program = this->getInputSocketReader(1);
}
void DifferenceMatteOperation::deinitExecution() {
	this->inputImage1Program= NULL;
	this->inputImage2Program= NULL;
}

void DifferenceMatteOperation::executePixel(float* outputValue, float x, float y, MemoryBuffer *inputBuffers[]) {
	float inColor1[4];
	float inColor2[4];

	const float tolerence=this->settings->t1;
	const float falloff=this->settings->t2;
	float difference;
	float alpha;

	this->inputImage1Program->read(inColor1, x, y, inputBuffers);
	this->inputImage2Program->read(inColor2, x, y, inputBuffers);

	difference= fabs(inColor2[0]-inColor1[0])+
			   fabs(inColor2[1]-inColor1[1])+
			   fabs(inColor2[2]-inColor1[2]);

	/*average together the distances*/
	difference=difference/3.0;

	/*make 100% transparent*/
	if(difference < tolerence) {
		outputValue[0]=0.0;
	}
	/*in the falloff region, make partially transparent */
	else if(difference < falloff+tolerence) {
		difference=difference-tolerence;
		alpha=difference/falloff;
		/*only change if more transparent than before */
		if(alpha < inColor1[3]) {
			outputValue[0]=alpha;
		}
		else { /* leave as before */
			outputValue[0]=inColor1[3];
		}
	}
	else {
		/*foreground object*/
		outputValue[0]= inColor1[3];
	}
}


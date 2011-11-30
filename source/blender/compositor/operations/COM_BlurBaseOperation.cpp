#include "COM_BlurBaseOperation.h"
#include "COM_InputSocket.h"
#include "COM_OutputSocket.h"
#include "BLI_math.h"

extern "C" {
	#include "RE_pipeline.h"
}

BlurBaseOperation::BlurBaseOperation(): NodeOperation() {
	this->addInputSocket(*(new InputSocket(COM_DT_COLOR)));
	this->addOutputSocket(*(new OutputSocket(COM_DT_COLOR)));
	this->setComplex(true);
	this->inputProgram = NULL;
	this->data = NULL;
	this->size = 1.0f;

}
void BlurBaseOperation::initExecution() {
	this->inputProgram = this->getInputSocketReader(0);
	this->data->image_in_width= this->getWidth();
	this->data->image_in_height= this->getHeight();
	if(this->data->relative) {
		switch (this->data->aspect) {
		case CMP_NODE_BLUR_ASPECT_NONE:
			this->data->sizex= (int)(this->data->percentx*0.01f*this->data->image_in_width);
			this->data->sizey= (int)(this->data->percenty*0.01f*this->data->image_in_height);
			break;
		case CMP_NODE_BLUR_ASPECT_Y:
			this->data->sizex= (int)(this->data->percentx*0.01f*this->data->image_in_width);
			this->data->sizey= (int)(this->data->percenty*0.01f*this->data->image_in_width);
			break;
		case CMP_NODE_BLUR_ASPECT_X:
			this->data->sizex= (int)(this->data->percentx*0.01f*this->data->image_in_height);
			this->data->sizey= (int)(this->data->percenty*0.01f*this->data->image_in_height);
			break;
		}
	}

	QualityStepHelper::initExecution(COM_QH_MULTIPLY);

}

float* BlurBaseOperation::make_gausstab(int rad)
{
	float *gausstab, sum, val;
	int i, n;

	n = 2 * rad + 1;

	gausstab = new float[n];

	sum = 0.0f;
	for (i = -rad; i <= rad; i++) {
		val= RE_filter_value(this->data->filtertype, (float)i/(float)rad);
		sum += val;
		gausstab[i+rad] = val;
	}

	sum= 1.0f/sum;
	for(i=0; i<n; i++)
		gausstab[i]*= sum;

	return gausstab;
}

void BlurBaseOperation::deinitExecution() {
	this->inputProgram = NULL;
}

#include "COM_TextureOperation.h"

#include "BLI_listbase.h"
#include "DNA_scene_types.h"

TextureBaseOperation::TextureBaseOperation(): NodeOperation() {
	this->texture = NULL;
}
TextureOperation::TextureOperation() : TextureBaseOperation() {
	this->addOutputSocket(*new OutputSocket(COM_DT_COLOR));
}
TextureAlphaOperation::TextureAlphaOperation() : TextureBaseOperation() {
	this->addOutputSocket(*new OutputSocket(COM_DT_VALUE));
}

void TextureBaseOperation::determineResolution(unsigned int resolution[], unsigned int preferredResolution[]) {
	if (preferredResolution[0] == 0 || preferredResolution[1] == 0) {
		resolution[0] = COM_DEFAULT_RESOLUTION_WIDTH;
		resolution[1] = COM_DEFAULT_RESOLUTION_HEIGHT;
	} else {
		resolution[0] = preferredResolution[0];
		resolution[1] = preferredResolution[1];
	}
}

void TextureAlphaOperation::executePixel(float *color, float x, float y, MemoryBuffer *inputBuffers[]) {
	TextureBaseOperation::executePixel(color, x, y, inputBuffers);
	color[0] = color[3];
	color[1] = 0.0f;
	color[2] = 0.0f;
	color[3] = 0.0f;
}

void TextureBaseOperation::executePixel(float *color, float x, float y, MemoryBuffer *inputBuffers[]) {
	TexResult texres= {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0, NULL};
	float vec[3];
	int retval;
	const float cx = this->getWidth()/2;
	const float cy = this->getHeight()/2;
	const float u = (cx-x)/this->getWidth()*2;
	const float v = (cy-y)/this->getHeight()*2;


	vec[0]= textureSize[0]*(u + this->textureOffset[0]);
	vec[1]= textureSize[1]*(v + this->textureOffset[1]);
	vec[2]= textureSize[2]*this->textureOffset[2];

	retval= multitex_ext(this->texture, vec, NULL, NULL, 0, &texres);

	if(texres.talpha)
		color[3]= texres.ta;
	else
		color[3]= texres.tin;

	if((retval & TEX_RGB)) {
		color[0]= texres.tr;
		color[1]= texres.tg;
		color[2]= texres.tb;
	}
	else color[0]= color[1]= color[2]= color[3];
}

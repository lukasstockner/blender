/*
 * Copyright 2011, Blender Foundation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor: 
 *		Jeroen Bakker 
 *		Monique Dewanchand
 */

#include "COM_BilateralBlurOperation.h"
#include "BLI_math.h"

extern "C" {
	#include "RE_pipeline.h"
}

BilateralBlurOperation::BilateralBlurOperation() : NodeOperation() {
	this->addInputSocket(COM_DT_COLOR);
	this->addInputSocket(COM_DT_COLOR);
	this->addOutputSocket(COM_DT_COLOR);
	this->setComplex(true);

	this->inputColorProgram = NULL;
	this->inputDeterminatorProgram = NULL;
}

void BilateralBlurOperation::initExecution() {
	this->inputColorProgram = getInputSocketReader(0);
	this->inputDeterminatorProgram = getInputSocketReader(1);
	QualityStepHelper::initExecution(COM_QH_INCREASE);
}

void BilateralBlurOperation::executePixel(float* color, int x, int y, MemoryBuffer *inputBuffers[], void* data) {
	// read the determinator color at x, y, this will be used as the reference color for the determinator
	float determinatorReferenceColor[4];
	float determinator[4];
	float tempColor[4];
	float blurColor[4];
	float blurDivider;
	int minx = x - this->data->sigma_space;
	int maxx = x + this->data->sigma_space;
	int miny = y - this->data->sigma_space;
	int maxy = y + this->data->sigma_space;
	float deltaColor;
	this->inputDeterminatorProgram->read(determinatorReferenceColor, x, y, inputBuffers, data);
	
	blurColor[0] = 0.0f;
	blurColor[1] = 0.0f;
	blurColor[2] = 0.0f;
	blurDivider = 0.0f;
	for (int yi = miny ; yi < maxy ; yi++) {
		for (int xi = minx ; xi < maxx ; xi++) {
			// read determinator
			this->inputDeterminatorProgram->read(determinator, xi, yi, inputBuffers, data);
			deltaColor = fabsf(determinatorReferenceColor[0] - determinator[0])+
			        fabsf(determinatorReferenceColor[1] - determinator[1])+
			        fabsf(determinatorReferenceColor[2] - determinator[2]); // do not take the alpha channel into account
			if (deltaColor< this->data->sigma_color) {
				// add this to the blur
				this->inputColorProgram->read(tempColor, xi, yi, inputBuffers, data);
				blurColor[0]+=tempColor[0];
				blurColor[1]+=tempColor[1];
				blurColor[2]+=tempColor[2];
				blurDivider += 1.0f;
			}
		}
	}
	
	if (blurDivider > 0.0f) {
		color[0] = blurColor[0]/blurDivider;
		color[1] = blurColor[1]/blurDivider;
		color[2] = blurColor[2]/blurDivider;
		color[3] = 1.0f;
	} else {
		color[0] = 0.0f;
		color[1] = 0.0f;
		color[2] = 0.0f;
		color[3] = 1.0f;
	}
}

void BilateralBlurOperation::deinitExecution() {
	this->inputColorProgram = NULL;
	this->inputDeterminatorProgram = NULL;
}

bool BilateralBlurOperation::determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output) {
	rcti newInput;

	newInput.xmax = input->xmax + (this->data->sigma_space);
	newInput.xmin = input->xmin - (this->data->sigma_space);
	newInput.ymax = input->ymax + (this->data->sigma_space);
	newInput.ymin = input->ymin - (this->data->sigma_space);

	return NodeOperation::determineDependingAreaOfInterest(&newInput, readOperation, output);
}

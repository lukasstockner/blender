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

#include "COM_ConvertDepthToRadiusOperation.h"
#include "BLI_math.h"
#include "DNA_camera_types.h"

ConvertDepthToRadiusOperation::ConvertDepthToRadiusOperation(): NodeOperation() {
	this->addInputSocket(COM_DT_VALUE);
	this->addOutputSocket(COM_DT_VALUE);
    this->inputOperation = NULL;
	this->fStop = 128.0f;
	this->cameraObject = NULL;
	this->maxRadius = 32.0f;
}

float ConvertDepthToRadiusOperation::determineFocalDistance() const {

	if (cameraObject == NULL || cameraObject->type != OB_CAMERA) {
		return 10.0f;
	} else {
		Camera *camera= (Camera*)this->cameraObject->data;
		if (camera->dof_ob) {
			/* too simple, better to return the distance on the view axis only
			 * return len_v3v3(ob->obmat[3], cam->dof_ob->obmat[3]); */
			float mat[4][4], imat[4][4], obmat[4][4];

			copy_m4_m4(obmat, cameraObject->obmat);
			normalize_m4(obmat);
			invert_m4_m4(imat, obmat);
			mult_m4_m4m4(mat, imat, camera->dof_ob->obmat);
			return (float)fabs(mat[3][2]);
		}
		return camera->YF_dofdist;
	}
}

void ConvertDepthToRadiusOperation::initExecution() {
	this->inputOperation = this->getInputSocketReader(0);
	this->focalDistance = determineFocalDistance();
}

void ConvertDepthToRadiusOperation::executePixel(float* outputValue, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]) {
    float inputColor[4];
	inputOperation->read(&inputColor[0], x, y, sampler, inputBuffers);
	float inputValue = inputColor[0];
	if (inputValue<0.0f) {
		inputValue = 0.0f;
	}

	float radius = fabs(focalDistance-inputValue)/1000.0f*this->getWidth();

	if (radius<1.0f) {radius = 1.0f;}
	if (radius>maxRadius) {radius = maxRadius;}

	outputValue[0] = radius;
}

void ConvertDepthToRadiusOperation::deinitExecution() {
    this->inputOperation = NULL;
}

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

#include "COM_MovieUndistortionOperation.h"
extern "C" {
#include "BKE_tracking.h"
}
MovieUndistortionOperation::MovieUndistortionOperation() : MovieDistortionOperation() {
}
void MovieUndistortionOperation::executePixel(float *color,float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]) {
	if (this->movieClip != NULL) {
		float in[2];
		float out[2];
		
		in[0] = x;
		in[1] = y;
		
		BKE_tracking_apply_intrinsics(&this->movieClip->tracking, in, out);
		this->inputOperation->read(color, out[0], out[1], sampler, inputBuffers);
	} 
	else {
		this->inputOperation->read(color, x, y, sampler, inputBuffers);
		
	}
}


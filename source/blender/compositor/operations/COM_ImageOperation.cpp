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

#include "COM_ImageOperation.h"

#include "BLI_listbase.h"
#include "DNA_scene_types.h"
#include "DNA_image_types.h"
#include "BKE_image.h"
#include "BLI_math.h"

extern "C" {
    #include "RE_pipeline.h"
    #include "RE_shader_ext.h"
    #include "RE_render_ext.h"
#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
}

BaseImageOperation::BaseImageOperation(): NodeOperation() {
    this->image = NULL;
    this->imageBuffer = NULL;
    this->imageUser = NULL;
    this->imagewidth = 0;
    this->imageheight = 0;
	this->framenumber = 0;
	this->interpolation = COM_IM_NEAREST;
}
ImageOperation::ImageOperation(): BaseImageOperation() {
    this->addOutputSocket(COM_DT_COLOR);
}
ImageAlphaOperation::ImageAlphaOperation(): BaseImageOperation() {
    this->addOutputSocket(COM_DT_VALUE);
}

static ImBuf *node_composit_get_image(Image *ima, ImageUser *iuser)
{
        ImBuf *ibuf;

        ibuf= BKE_image_get_ibuf(ima, iuser);
        if(ibuf==NULL || (ibuf->rect==NULL && ibuf->rect_float==NULL)) {
                return NULL;
        }

        if (ibuf->rect_float == NULL) {
                IMB_float_from_rect(ibuf);
        }

        return ibuf;
}

void BaseImageOperation::initExecution() {
	BKE_image_user_calc_frame(this->imageUser, this->framenumber, 0);
	ImBuf *stackbuf= node_composit_get_image(this->image, this->imageUser);
    if (stackbuf) {
        this->imageBuffer = stackbuf->rect_float;
        this->imagewidth = stackbuf->x;
        this->imageheight = stackbuf->y;
    }
}

void BaseImageOperation::deinitExecution() {
    this->imageBuffer= NULL;
}

void BaseImageOperation::determineResolution(unsigned int resolution[], unsigned int preferredResolution[]) {
    BKE_image_user_calc_frame(this->imageUser, 0, 0);
	ImBuf *stackbuf= node_composit_get_image(this->image, this->imageUser);
    if (stackbuf) {
        resolution[0] = stackbuf->x;
        resolution[1] = stackbuf->y;
    }
}

void ImageOperation::executePixel(float *color, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]) {
	int ix = x;
	int iy = y;

	if (this->imageBuffer == NULL || x < 0 || y < 0 || x >= this->getWidth() || y >= this->getHeight() ) {
		color[0] = 0.0f;
		color[1] = 0.0f;
		color[2] = 0.0f;
		color[3] = 0.0f;
	} else {
		int index;
		switch (interpolation) {
		case COM_IM_NEAREST:
			index = (iy*this->imagewidth+ix)*4;
			color[0] = this->imageBuffer[index];
			color[1] = this->imageBuffer[index+1];
			color[2] = this->imageBuffer[index+2];
			color[3] = this->imageBuffer[index+3];
			break;
		case COM_IM_LINEAR:
			int x1 = floor(x);
			int x2 = min(x1 + 1, (int)this->getWidth()-1);
			int y1 = floor(y);
			int y2 = min(y1 + 1, (int)this->getHeight()-1);

			float valuex = x - x1;
			float valuey = y - y1;
			float mvaluex = 1.0 - valuex;
			float mvaluey = 1.0 - valuey;

			float color1[4];
			float color2[4];
			float color3[4];
			float color4[4];

			index = (y1*this->imagewidth+x1)*4;
			color1[0] = this->imageBuffer[index];
			color1[1] = this->imageBuffer[index+1];
			color1[2] = this->imageBuffer[index+2];
			color1[3] = this->imageBuffer[index+3];
			index = (y2*this->imagewidth+x1)*4;
			color2[0] = this->imageBuffer[index];
			color2[1] = this->imageBuffer[index+1];
			color2[2] = this->imageBuffer[index+2];
			color2[3] = this->imageBuffer[index+3];
                        index = (y1*this->imagewidth+x2)*4;
			color3[0] = this->imageBuffer[index];
			color3[1] = this->imageBuffer[index+1];
			color3[2] = this->imageBuffer[index+2];
			color3[3] = this->imageBuffer[index+3];
			index = (y2*this->imagewidth+x2)*4;
			color4[0] = this->imageBuffer[index];
			color4[1] = this->imageBuffer[index+1];
			color4[2] = this->imageBuffer[index+2];
			color4[3] = this->imageBuffer[index+3];

			color1[0] = color1[0]*mvaluey + color2[0]*valuey;
			color1[1] = color1[1]*mvaluey + color2[1]*valuey;
			color1[2] = color1[2]*mvaluey + color2[2]*valuey;
			color1[3] = color1[3]*mvaluey + color2[3]*valuey;

			color3[0] = color3[0]*mvaluey + color4[0]*valuey;
			color3[1] = color3[1]*mvaluey + color4[1]*valuey;
			color3[2] = color3[2]*mvaluey + color4[2]*valuey;
			color3[3] = color3[3]*mvaluey + color4[3]*valuey;

			color[0] = color1[0]*mvaluex + color3[0]*valuex;
			color[1] = color1[1]*mvaluex + color3[1]*valuex;
			color[2] = color1[2]*mvaluex + color3[2]*valuex;
			color[3] = color1[3]*mvaluex + color3[3]*valuex;

			break;
		}
	}
}

void ImageAlphaOperation::executePixel(float *color, float x, float y, PixelSampler sampler, MemoryBuffer *inputBuffers[]) {
	int ix = x;
	int iy = y;

	if (this->imageBuffer == NULL || x < 0 || y < 0 || x >= this->getWidth() || y >= this->getHeight() ) {
		color[0] = 0.0f;
	} else {
		int index;
		switch (interpolation) {
		case COM_IM_NEAREST:
			index = (iy*this->imagewidth+ix)*4;
			color[0] = this->imageBuffer[index+3];
			break;
		case COM_IM_LINEAR:
			int x1 = floor(x);
			int x2 = min(x1 + 1, (int)this->getWidth()-1);
			int y1 = floor(y);
			int y2 = min(y1 + 1, (int)this->getHeight()-1);

			float valuex = x - x1;
			float valuey = y - y1;
			float mvaluex = 1.0 - valuex;
			float mvaluey = 1.0 - valuey;

			float color1;
			float color2;
			float color3;
			float color4;

			index = (y1*this->imagewidth+x1)*4;
			color1 = this->imageBuffer[index+3];
			index = (y2*this->imagewidth+x1)*4;
			color2 = this->imageBuffer[index+3];
			index = (y1*this->imagewidth+x2)*4;
			color3 = this->imageBuffer[index+3];
			index = (y2*this->imagewidth+x2)*4;
			color4 = this->imageBuffer[index+3];

			color1 = color1*mvaluey + color2*valuey;
			color3 = color3*mvaluey + color4*valuey;
			color[0] = color1*mvaluex + color3*valuex;
			break;
		}
	}
}

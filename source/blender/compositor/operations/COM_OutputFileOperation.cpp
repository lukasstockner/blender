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

#include "COM_OutputFileOperation.h"
#include "COM_SocketConnection.h"
#include "BLI_listbase.h"
#include "DNA_scene_types.h"
#include "BKE_image.h"
#include "BKE_global.h"
#include "BKE_main.h"

extern "C" {
	#include "MEM_guardedalloc.h"
	#include "IMB_imbuf.h"
	#include "IMB_imbuf_types.h"
}


OutputFileOperation::OutputFileOperation() : NodeOperation() {
	this->addInputSocket(COM_DT_COLOR);
	this->addInputSocket(COM_DT_VALUE);

	this->setScene(NULL);
	this->outputBuffer = NULL;
	this->zBuffer = NULL;
	this->imageInput = NULL;
	this->zInput = NULL;
}

void OutputFileOperation::initExecution() {
	// When initializing the tree during initial load the width and height can be zero.
	this->imageInput = getInputSocketReader(0);
	this->zInput = getInputSocketReader(1);
	if (this->getWidth() * this->getHeight() != 0) {
		this->outputBuffer=(float*) MEM_callocN(this->getWidth()*this->getHeight()*4*sizeof(float), "OutputFileOperation");
		this->zBuffer=(float*) MEM_callocN(this->getWidth()*this->getHeight()*sizeof(float), "OutputFileOperation");
	}
}

void OutputFileOperation::deinitExecution() {
	// TODO: create ImBuf
	if (this->getWidth() * this->getHeight() != 0) {
		
		ImBuf *ibuf= IMB_allocImBuf(this->getWidth(), this->getHeight(), 32, 0);
		Main *bmain= G.main; /* TODO, have this passed along */
		char string[256];
		
		ibuf->rect_float= this->outputBuffer;
		ibuf->dither= scene->r.dither_intensity;
		
		if (scene->r.color_mgt_flag & R_COLOR_MANAGEMENT)
			ibuf->profile = IB_PROFILE_LINEAR_RGB;
		
		if (zInput != NULL) {
			this->imageFile->im_format.flag |= R_IMF_FLAG_ZBUF;
			ibuf->zbuf_float= zBuffer;
		}
		
		BKE_makepicstring(string, this->imageFile->name, bmain->name, this->scene->r.cfra, this->imageFile->im_format.imtype, (this->scene->r.scemode & R_EXTENSION), true);
		
		if(0 == BKE_write_ibuf(ibuf, string, &this->imageFile->im_format))
			printf("Cannot save Node File Output to %s\n", string);
		else
			printf("Saved: %s\n", string);
		
		IMB_freeImBuf(ibuf);	
	}
	this->outputBuffer = NULL;
	this->zBuffer = NULL;
	this->imageInput = NULL;
	this->zInput = NULL;
}


void OutputFileOperation::executeRegion(rcti *rect, unsigned int tileNumber, MemoryBuffer** memoryBuffers) {
	float color[4];
	float z[4];
	float* buffer = this->outputBuffer;
	float* zbuffer = this->zBuffer;

	if (!buffer) return;
	int x1 = rect->xmin;
	int y1 = rect->ymin;
	int x2 = rect->xmax;
	int y2 = rect->ymax;
	int offset = (y1*this->getWidth() + x1 ) * 4;
	int zoffset = (y1*this->getWidth() + x1 );
	int x;
	int y;
	bool breaked = false;

	for (y = y1 ; y < y2 && (!breaked); y++) {
		for (x = x1 ; x < x2 && (!breaked) ; x++) {
			imageInput->read(color, x, y, COM_PS_NEAREST, memoryBuffers);
			buffer[offset] = color[0];
			buffer[offset+1] = color[1];
			buffer[offset+2] = color[2];
			buffer[offset+3] = color[3];
			if (zInput != NULL) {
				zInput->read(z, x, y, COM_PS_NEAREST, memoryBuffers);
				zbuffer[zoffset] = z[0];
			}
			offset +=4;
			zoffset ++;
			if (tree->test_break && tree->test_break(tree->tbh)) {
				breaked = true;
			}
		}
		offset += (this->getWidth()-(x2-x1))*4;
		zoffset += (this->getWidth()-(x2-x1));
	}
}

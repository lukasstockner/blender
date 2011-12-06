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

#include "COM_MemoryBuffer.h"
#include "MEM_guardedalloc.h"
#include "BLI_math.h"
#include <fstream>
#include "BKE_global.h"

unsigned int MemoryBuffer::determineBufferSize() {
	return getWidth() * getHeight();
}

int MemoryBuffer::getWidth() const {
	return  this->rect.xmax-this->rect.xmin;
}
int MemoryBuffer::getHeight() const {
	return  this->rect.ymax-this->rect.ymin;
}

MemoryBuffer::MemoryBuffer(MemoryProxy * memoryProxy, unsigned int chunkNumber, rcti* rect) {
    BLI_init_rcti(&this->rect, rect->xmin, rect->xmax, rect->ymin, rect->ymax);
	this->memoryProxy = memoryProxy;
	this->chunkNumber = chunkNumber;
    this->buffer = (float*)MEM_mallocN(sizeof(float)*determineBufferSize()*4, "COM_MemoryBuffer");
    this->state = COM_MB_ALLOCATED;
    this->datatype = COM_DT_COLOR;
	this->chunkWidth = this->rect.xmax - this->rect.xmin;
    this->filename = "";
    this->numberOfUsers = 0;
    BLI_mutex_init(&this->mutex);
}

MemoryBuffer::MemoryBuffer(MemoryProxy * memoryProxy, rcti* rect) {
    BLI_init_rcti(&this->rect, rect->xmin, rect->xmax, rect->ymin, rect->ymax);
	this->memoryProxy = memoryProxy;
	this->chunkNumber = -1;
    this->buffer = (float*)MEM_mallocN(sizeof(float)*determineBufferSize()*4, "COM_MemoryBuffer");
    this->state = COM_MB_TEMPORARILY;
    this->datatype = COM_DT_COLOR;
	this->chunkWidth = this->rect.xmax - this->rect.xmin;
}

MemoryBuffer::~MemoryBuffer() {
    if (this->buffer) {
        MEM_freeN(this->buffer);
        this->buffer = NULL;
    }
    if (this->state != COM_MB_TEMPORARILY) {
        BLI_mutex_end(&this->mutex);
        if (this->filename.length() != 0) {
            if (remove(this->filename.c_str()) != 0) {
//                printf("ERROR: remove tempfile %s\n", this->filename);
            }
        }
    }
}

void MemoryBuffer::copyContentFrom(MemoryBuffer *otherBuffer) {
    if (!otherBuffer) {
        printf("no buffer to copy from\n");
        return;
    }
    unsigned int otherY;
    unsigned int minX = max(this->rect.xmin, otherBuffer->rect.xmin);
    unsigned int maxX = min(this->rect.xmax, otherBuffer->rect.xmax);
    unsigned int minY = max(this->rect.ymin, otherBuffer->rect.ymin);
    unsigned int maxY = min(this->rect.ymax, otherBuffer->rect.ymax);
    int offset;
    int otherOffset;


    for (otherY = minY ; otherY<maxY ; otherY ++) {
		otherOffset = ((otherY-otherBuffer->rect.ymin) * otherBuffer->chunkWidth + minX-otherBuffer->rect.xmin)*4;
		offset = ((otherY - this->rect.ymin) * this->chunkWidth + minX-this->rect.xmin)*4;
        memcpy(&this->buffer[offset], &otherBuffer->buffer[otherOffset], (maxX-minX) * 4*sizeof(float));
    }
}

void MemoryBuffer::read(float* result, int x, int y) {
    if (x>=this->rect.xmin && x < this->rect.xmax &&
	        y>=this->rect.ymin && y < this->rect.ymax) {
        int dx = x-this->rect.xmin;
        int dy = y-this->rect.ymin;
		int offset = (this->chunkWidth*dy+dx)*4;
        result[0] = this->buffer[offset];
        result[1] = this->buffer[offset+1];
        result[2] = this->buffer[offset+2];
        result[3] = this->buffer[offset+3];
    }
    else {
        result[0] = 0.0f;
        result[1] = 0.0f;
        result[2] = 0.0f;
        result[3] = 0.0f;
    }
}

void MemoryBuffer::readCubic(float* result, float x, float y) {
    int x1 = floor(x);
    int x2 = x1 + 1;
    int y1 = floor(y);
    int y2 = y1 + 1;

    float valuex = x - x1;
    float valuey = y - y1;
    float mvaluex = 1.0 - valuex;
    float mvaluey = 1.0 - valuey;

    float color1[4];
    float color2[4];
    float color3[4];
    float color4[4];

    read(color1, x1, y1);
    read(color2, x1, y2);
    read(color3, x2, y1);
    read(color4, x2, y2);

    color1[0] = color1[0]*mvaluey + color2[0]*valuey;
    color1[1] = color1[1]*mvaluey + color2[1]*valuey;
    color1[2] = color1[2]*mvaluey + color2[2]*valuey;
    color1[3] = color1[3]*mvaluey + color2[3]*valuey;

    color3[0] = color3[0]*mvaluey + color4[0]*valuey;
    color3[1] = color3[1]*mvaluey + color4[1]*valuey;
    color3[2] = color3[2]*mvaluey + color4[2]*valuey;
    color3[3] = color3[3]*mvaluey + color4[3]*valuey;

    result[0] = color1[0]*mvaluex + color3[0]*valuex;
    result[1] = color1[1]*mvaluex + color3[1]*valuex;
    result[2] = color1[2]*mvaluex + color3[2]*valuex;
    result[3] = color1[3]*mvaluex + color3[3]*valuex;

}

long MemoryBuffer::getAllocatedMemorySize() {
    if (this->state == COM_MB_STORED) {
        return 0l;
    } else {
        return sizeof(float)*determineBufferSize()*4;
    }
}

bool MemoryBuffer::makeAvailable(bool addUser) {
    bool result = false;
    BLI_mutex_lock(&this->mutex);
    if (this->state == COM_MB_STORED) {
        result = true;
        readFromDisc();
    }
    if (addUser) {
        this->numberOfUsers++;
    }
    BLI_mutex_unlock(&this->mutex);
    return result;
}

bool MemoryBuffer::isAvailable() {
    return this->state == COM_MB_AVAILABLE;
}

bool MemoryBuffer::saveToDisc() {
    bool result = false;
    BLI_mutex_lock(&this->mutex);
    if (this->numberOfUsers == 0) {
        if (this->state == COM_MB_AVAILABLE) {
            if (determineFilename()) {
                // save
                ofstream output(this->filename.c_str(), ios::out|ios::trunc);
                int bufferSize = determineBufferSize()*4;
                output.write((char*)this->buffer, bufferSize*sizeof(float));
                output.flush();
                output.close();
                MEM_freeN(this->buffer);
                this->buffer = NULL;

            }
            this->state = COM_MB_STORED;
            result = true;
        }
    }
    BLI_mutex_unlock(&this->mutex);
    return result;
}
void MemoryBuffer::readFromDisc() {
    if (this->state == COM_MB_STORED) {
        // load
        ifstream input(this->filename.c_str(), ios::in);

        int bufferSize = determineBufferSize()*4;
        this->buffer = (float*)MEM_mallocN(sizeof(float)*bufferSize, "COM_MemoryBuffer");

        input.read((char*)this->buffer, bufferSize*sizeof(float));

        input.close();
        this->state = COM_MB_AVAILABLE;
    }
}

bool MemoryBuffer::determineFilename() {
    if (this->filename.length() == 0) {
        std::ostringstream buffer;
//		G

		/// @todo: use configured tmp folder
		buffer << "/tmp/comp_chunk_";
        buffer << &this->memoryProxy;
        buffer << "_";
		buffer << this->chunkNumber;
        buffer << ".bin";
        this->filename = buffer.str();
        return true;
    } else {
        return false;
    }
}

void MemoryBuffer::addUser() {
    BLI_mutex_lock(&this->mutex);
    this->numberOfUsers++;
    BLI_mutex_unlock(&this->mutex);

}
void MemoryBuffer::removeUser() {
    BLI_mutex_lock(&this->mutex);
    this->numberOfUsers--;
    BLI_mutex_unlock(&this->mutex);
}


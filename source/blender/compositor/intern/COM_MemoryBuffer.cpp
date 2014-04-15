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
//#include "BKE_global.h"
#include "COM_MemoryBufferColor.h"
#include "COM_MemoryBufferVector.h"
#include "COM_MemoryBufferValue.h"

unsigned int MemoryBuffer::determineBufferSize() const
{
	return getWidth() * getHeight();
}

int MemoryBuffer::getWidth() const
{
	return this->m_rect.xmax - this->m_rect.xmin;
}
int MemoryBuffer::getHeight() const
{
	return this->m_rect.ymax - this->m_rect.ymin;
}

MemoryBuffer* MemoryBuffer::create(MemoryProxy *memoryProxy, unsigned int chunkNumber, rcti *rect) {
	DataType type;
	type = memoryProxy->getDataType();

	if (type == COM_DT_VALUE) {
        return new MemoryBufferValue(memoryProxy, chunkNumber, rect);
	}
	else if (type == COM_DT_VECTOR) {
		return new MemoryBufferVector(memoryProxy, chunkNumber, rect);
	}
	else {
		return new MemoryBufferColor(memoryProxy, chunkNumber, rect);
	}
}

MemoryBuffer* MemoryBuffer::create(MemoryProxy *memoryProxy, rcti *rect) {
    DataType type;
    type = memoryProxy->getDataType();

    if (type==COM_DT_VALUE){
        return new MemoryBufferValue(memoryProxy, rect);
    }
    else if (type == COM_DT_VECTOR) {
        return new MemoryBufferVector(memoryProxy, rect);
    }
    else {
        return new MemoryBufferColor(memoryProxy, rect);
    }
}

MemoryBuffer* MemoryBuffer::create(DataType datatype, rcti *rect) {
    if (datatype==COM_DT_VALUE){
        return new MemoryBufferValue(datatype, rect);
    }
    else if (datatype == COM_DT_VECTOR) {
        return new MemoryBufferVector(datatype, rect);
    }
    else {
        return new MemoryBufferColor(datatype, rect);
    }
}

MemoryBuffer::MemoryBuffer(MemoryProxy *memoryProxy, unsigned int chunkNumber, rcti *rect, unsigned int no_channels)
{
	BLI_rcti_init(&this->m_rect, rect->xmin, rect->xmax, rect->ymin, rect->ymax);
	this->m_memoryProxy = memoryProxy;
	this->m_chunkNumber = chunkNumber;
	this->m_buffer = (float *)MEM_mallocN(sizeof(float) * determineBufferSize() * no_channels, "COM_MemoryBuffer");
	this->m_state = COM_MB_ALLOCATED;
	this->m_chunkWidth = this->m_rect.xmax - this->m_rect.xmin;
	this->m_no_channels = no_channels;
}

MemoryBuffer::MemoryBuffer(MemoryProxy *memoryProxy, rcti *rect, unsigned int no_channels)
{
	BLI_rcti_init(&this->m_rect, rect->xmin, rect->xmax, rect->ymin, rect->ymax);
	this->m_memoryProxy = memoryProxy;
	this->m_chunkNumber = -1;
	this->m_buffer = (float *)MEM_mallocN(sizeof(float) * determineBufferSize() * no_channels, "COM_MemoryBuffer");
	this->m_state = COM_MB_TEMPORARILY;
	this->m_chunkWidth = this->m_rect.xmax - this->m_rect.xmin;
	this->m_no_channels = no_channels;
}

MemoryBuffer::MemoryBuffer(DataType datatype, rcti *rect, unsigned int no_channels) {
    BLI_rcti_init(&this->m_rect, rect->xmin, rect->xmax, rect->ymin, rect->ymax);
    this->m_memoryProxy = NULL;
    this->m_chunkNumber = -1;
    this->m_buffer = (float *)MEM_mallocN(sizeof(float) * determineBufferSize() * no_channels, "COM_MemoryBuffer");
    this->m_state = COM_MB_TEMPORARILY;
    this->m_chunkWidth = this->m_rect.xmax - this->m_rect.xmin;
    this->m_no_channels = no_channels;
}

void MemoryBuffer::clear()
{
	memset(this->m_buffer, 0, this->determineBufferSize() * this->m_no_channels * sizeof(float));
}

void MemoryBuffer::copyContentFrom(MemoryBuffer *otherBuffer)
{
	if (!otherBuffer) {
		BLI_assert(0);
		return;
	}
	unsigned int otherY;
	unsigned int minX = max(this->m_rect.xmin, otherBuffer->m_rect.xmin);
	unsigned int maxX = min(this->m_rect.xmax, otherBuffer->m_rect.xmax);
	unsigned int minY = max(this->m_rect.ymin, otherBuffer->m_rect.ymin);
	unsigned int maxY = min(this->m_rect.ymax, otherBuffer->m_rect.ymax);
	int offset;
	int otherOffset;


	for (otherY = minY; otherY < maxY; otherY++) {
		otherOffset = ((otherY - otherBuffer->m_rect.ymin) * otherBuffer->m_chunkWidth + minX - otherBuffer->m_rect.xmin) * this->m_no_channels;
		offset = ((otherY - this->m_rect.ymin) * this->m_chunkWidth + minX - this->m_rect.xmin) * this->m_no_channels;
		memcpy(&this->m_buffer[offset], &otherBuffer->m_buffer[otherOffset], (maxX - minX) * this->m_no_channels * sizeof(float));
	}
}


// TODO: this method needs to be checked! At Mind 2014
float *MemoryBuffer::convertToValueBuffer()
{
	const unsigned int size = this->determineBufferSize();
	unsigned int i;

	float *result = (float *)MEM_mallocN(sizeof(float) * size, __func__);

	const float *fp_src = this->m_buffer;
	float       *fp_dst = result;

    for (i = 0; i < size; i++, fp_dst++, fp_src += COM_NO_CHANNELS_COLOR) {
		*fp_dst = *fp_src;
	}

	return result;
}

float MemoryBuffer::getMaximumValue() const
{
    return 0.0f;
}

float MemoryBuffer::getMaximumValue(rcti *rect)
{
	rcti rect_clamp;

	/* first clamp the rect by the bounds or we get un-initialized values */
	BLI_rcti_isect(rect, &this->m_rect, &rect_clamp);

	if (!BLI_rcti_is_empty(&rect_clamp)) {
        MemoryBuffer *temp = MemoryBuffer::create(COM_DT_VALUE, &rect_clamp);
		temp->copyContentFrom(this);
		float result = temp->getMaximumValue();
		delete temp;
		return result;
	}
	else {
		BLI_assert(0);
		return 0.0f;
	}
}

MemoryBuffer::~MemoryBuffer()
{
	if (this->m_buffer) {
        MEM_freeN(this->m_buffer);
		this->m_buffer = NULL;
	}
}

const int MemoryBuffer::get_no_channels() const {
	return this->m_no_channels;
}


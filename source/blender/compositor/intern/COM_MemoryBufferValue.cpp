/*
 * Copyright 2014, Blender Foundation.
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

#include "COM_MemoryBufferValue.h"

#define NUMBER_OF_CHANNELS COM_NUM_CHANNELS_VALUE

MemoryBufferValue::MemoryBufferValue(MemoryProxy *memoryProxy, unsigned int chunkNumber, rcti *rect):
	MemoryBuffer(memoryProxy, chunkNumber, rect, NUMBER_OF_CHANNELS)
{
	this->init_samplers();
}
	
MemoryBufferValue::MemoryBufferValue(MemoryProxy *memoryProxy, rcti *rect) :
	MemoryBuffer(memoryProxy, rect, NUMBER_OF_CHANNELS)
{
	this->init_samplers();
}

MemoryBufferValue::MemoryBufferValue(DataType datatype, rcti *rect) :
	MemoryBuffer(datatype, rect, NUMBER_OF_CHANNELS) {
	this->init_samplers();
}

MemoryBuffer *MemoryBufferValue::duplicate()
{
	MemoryBufferValue *result = new MemoryBufferValue(this->m_memoryProxy, &this->m_rect);
	memcpy(result->getBuffer(), this->getBuffer(), this->determineBufferSize() * NUMBER_OF_CHANNELS * sizeof(float));
	return result;
}


// --- write pixels ---
void MemoryBufferValue::writePixel(int x, int y, const float *color)
{
	if (x >= this->m_rect.xmin && x < this->m_rect.xmax &&
		y >= this->m_rect.ymin && y < this->m_rect.ymax)
	{
		const int offset = (this->m_chunkWidth * (y - this->m_rect.ymin) + x - this->m_rect.xmin) * NUMBER_OF_CHANNELS;
		copy_v4_v4(&this->m_buffer[offset], color);
	}
}

void MemoryBufferValue::addPixel(int x, int y, const float *color)
{
	if (x >= this->m_rect.xmin && x < this->m_rect.xmax &&
		y >= this->m_rect.ymin && y < this->m_rect.ymax)
	{
		const int offset = (this->m_chunkWidth * (y - this->m_rect.ymin) + x - this->m_rect.xmin) * NUMBER_OF_CHANNELS;
		this->m_buffer[offset] = color[0];
	}
}

// --- SAMPLERS ---
inline void MemoryBufferValue::read(float *result, int x, int y,
									MemoryBufferExtend extend_x,
									MemoryBufferExtend extend_y)
{
	this->m_sampler_nearest->read(result, x, y, extend_x, extend_y);
}

void MemoryBufferValue::init_samplers() {
	this->m_sampler_nearest = new SamplerNearestValue(this);
	this->m_sampler_nocheck = new SamplerNearestNoCheckValue(this);
	this->m_sampler_bilinear = new SamplerBilinearValue(this);
}

void MemoryBufferValue::deinit_samplers() {
	delete this->m_sampler_nearest;
	delete this->m_sampler_nocheck;
	delete this->m_sampler_bilinear;
}


inline void MemoryBufferValue::readNoCheck(float *result, int x, int y,
										   MemoryBufferExtend extend_x,
										   MemoryBufferExtend extend_y)
{
	this->m_sampler_nocheck->read(result, x, y, extend_x, extend_y);
}

inline void MemoryBufferValue::readBilinear(float *result, float x, float y,
											MemoryBufferExtend extend_x,
											MemoryBufferExtend extend_y)
{
	this->m_sampler_bilinear->read(result, x, y, extend_x, extend_y);
}

float MemoryBufferValue::getMaximumValue() const
{
	float result = this->m_buffer[0];
	const unsigned int size = this->determineBufferSize();
	unsigned int i;

	const float *fp_src = this->m_buffer;

	for (i = 0; i < size; i++, fp_src += NUMBER_OF_CHANNELS) {
		float value = *fp_src;
		if (value > result) {
			result = value;
		}
	}

	return result;
}

float MemoryBufferValue::getMaximumValue(rcti *rect)
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

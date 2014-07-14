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

#include "COM_MemoryBufferVector.h"

#define NUMBER_OF_CHANNELS COM_NUM_CHANNELS_VECTOR

MemoryBufferVector::MemoryBufferVector(MemoryProxy *memoryProxy, unsigned int chunkNumber, rcti *rect): 
	MemoryBuffer(memoryProxy, chunkNumber, rect, NUMBER_OF_CHANNELS)
{
	this->init_samplers();
}
	
MemoryBufferVector::MemoryBufferVector(MemoryProxy *memoryProxy, rcti *rect) :
	MemoryBuffer(memoryProxy, rect, NUMBER_OF_CHANNELS)
{
	this->init_samplers();
}

MemoryBufferVector::MemoryBufferVector(DataType datatype, rcti *rect) :
	MemoryBuffer(datatype, rect, NUMBER_OF_CHANNELS) {
	this->init_samplers();
}


MemoryBuffer *MemoryBufferVector::duplicate()
{
	MemoryBufferVector *result = new MemoryBufferVector(this->m_memoryProxy, &this->m_rect);
	memcpy(result->getBuffer(), this->getBuffer(), this->determineBufferSize() * NUMBER_OF_CHANNELS * sizeof(float));
	return result;
}

void MemoryBufferVector::init_samplers() {
	this->m_sampler_nearest = new SamplerNearestVector(this);
	this->m_sampler_nocheck = new SamplerNearestNoCheckVector(this);
	this->m_sampler_bilinear = new SamplerBilinearVector(this);
}

void MemoryBufferVector::deinit_samplers() {
	delete this->m_sampler_nearest;
	delete this->m_sampler_nocheck;
	delete this->m_sampler_bilinear;
}


// --- write pixels ---
void MemoryBufferVector::writePixel(int x, int y, const float *color)
{
	if (x >= this->m_rect.xmin && x < this->m_rect.xmax &&
		y >= this->m_rect.ymin && y < this->m_rect.ymax)
	{
		const int offset = (this->m_chunkWidth * (y - this->m_rect.ymin) + x - this->m_rect.xmin) * NUMBER_OF_CHANNELS;
		copy_v4_v4(&this->m_buffer[offset], color);
	}
}

void MemoryBufferVector::addPixel(int x, int y, const float *color)
{
	if (x >= this->m_rect.xmin && x < this->m_rect.xmax &&
		y >= this->m_rect.ymin && y < this->m_rect.ymax)
	{
		const int offset = (this->m_chunkWidth * (y - this->m_rect.ymin) + x - this->m_rect.xmin) * NUMBER_OF_CHANNELS;
		add_v4_v4(&this->m_buffer[offset], color);
	}
}

// --- SAMPLERS ---
inline void MemoryBufferVector::read(float *result, int x, int y,
									 MemoryBufferExtend extend_x,
									 MemoryBufferExtend extend_y)
{
	this->m_sampler_nearest->read(result, x, y, extend_x, extend_y);
}

inline void MemoryBufferVector::readNoCheck(float *result, int x, int y,
											MemoryBufferExtend extend_x,
											MemoryBufferExtend extend_y)
{
	this->m_sampler_nocheck->read(result, x, y, extend_x, extend_y);
}

inline void MemoryBufferVector::readBilinear(float *result, float x, float y,
											 MemoryBufferExtend extend_x,
											 MemoryBufferExtend extend_y)
{
	this->m_sampler_bilinear->read(result, x, y, extend_x, extend_y);
}


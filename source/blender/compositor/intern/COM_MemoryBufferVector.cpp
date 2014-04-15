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

#define NUMBER_OF_CHANNELS COM_NO_CHANNELS_VECTOR

MemoryBufferVector::MemoryBufferVector(MemoryProxy *memoryProxy, unsigned int chunkNumber, rcti *rect): 
	MemoryBuffer(memoryProxy, chunkNumber, rect, NUMBER_OF_CHANNELS)
{
}
	
MemoryBufferVector::MemoryBufferVector(MemoryProxy *memoryProxy, rcti *rect) :
	MemoryBuffer(memoryProxy, rect, NUMBER_OF_CHANNELS)
{
}

MemoryBufferVector::MemoryBufferVector(DataType datatype, rcti *rect) :
    MemoryBuffer(datatype, rect, NUMBER_OF_CHANNELS) {
}


MemoryBuffer *MemoryBufferVector::duplicate()
{
	MemoryBufferVector *result = new MemoryBufferVector(this->m_memoryProxy, &this->m_rect);
	memcpy(result->getBuffer(), this->getBuffer(), this->determineBufferSize() * NUMBER_OF_CHANNELS * sizeof(float));
	return result;
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
	bool clip_x = (extend_x == COM_MB_CLIP && (x < m_rect.xmin || x >= m_rect.xmax));
	bool clip_y = (extend_y == COM_MB_CLIP && (y < m_rect.ymin || y >= m_rect.ymax));
	if (clip_x || clip_y) {
		/* clip result outside rect is zero */
		zero_v4(result);
	}
	else 
	{
		wrap_pixel(x, y, extend_x, extend_y);
		const int offset = (this->m_chunkWidth * y + x) * NUMBER_OF_CHANNELS;
		copy_v4_v4(result, &this->m_buffer[offset]);
	}
}

inline void MemoryBufferVector::readNoCheck(float *result, int x, int y,
	                        MemoryBufferExtend extend_x,
	                        MemoryBufferExtend extend_y)
{

	wrap_pixel(x, y, extend_x, extend_y);
	const int offset = (this->m_chunkWidth * y + x) * NUMBER_OF_CHANNELS;

	BLI_assert(offset >= 0);
	BLI_assert(offset < this->determineBufferSize() * NUMBER_OF_CHANNELS);
	BLI_assert(!(extend_x == COM_MB_CLIP && (x < m_rect.xmin || x >= m_rect.xmax)) &&
		   !(extend_y == COM_MB_CLIP && (y < m_rect.ymin || y >= m_rect.ymax)));

	copy_v4_v4(result, &this->m_buffer[offset]);
}

inline void MemoryBufferVector::readBilinear(float *result, float x, float y,
			 MemoryBufferExtend extend_x,
			 MemoryBufferExtend extend_y)
{
	int x1 = floor(x);
	int y1 = floor(y);
	int x2 = x1 + 1;
	int y2 = y1 + 1;
	wrap_pixel(x1, y1, extend_x, extend_y);
	wrap_pixel(x2, y2, extend_x, extend_y);

	float valuex = x - x1;
	float valuey = y - y1;
	float mvaluex = 1.0f - valuex;
	float mvaluey = 1.0f - valuey;

    float vector1[NUMBER_OF_CHANNELS];
    float vector2[NUMBER_OF_CHANNELS];
    float vector3[NUMBER_OF_CHANNELS];
    float vector4[NUMBER_OF_CHANNELS];

    read(vector1, x1, y1);
    read(vector2, x1, y2);
    read(vector3, x2, y1);
    read(vector4, x2, y2);

    vector1[0] = vector1[0] * mvaluey + vector2[0] * valuey;
    vector1[1] = vector1[1] * mvaluey + vector2[1] * valuey;
    vector1[2] = vector1[2] * mvaluey + vector2[2] * valuey;

    vector3[0] = vector3[0] * mvaluey + vector4[0] * valuey;
    vector3[1] = vector3[1] * mvaluey + vector4[1] * valuey;
    vector3[2] = vector3[2] * mvaluey + vector4[2] * valuey;

    result[0] = vector1[0] * mvaluex + vector3[0] * valuex;
    result[1] = vector1[1] * mvaluex + vector3[1] * valuex;
    result[2] = vector1[2] * mvaluex + vector3[2] * valuex;
}


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

#define NUMBER_OF_CHANNELS COM_NO_CHANNELS_VALUE

MemoryBufferValue::MemoryBufferValue(MemoryProxy *memoryProxy, unsigned int chunkNumber, rcti *rect):
	MemoryBuffer(memoryProxy, chunkNumber, rect, NUMBER_OF_CHANNELS)
{
}
	
MemoryBufferValue::MemoryBufferValue(MemoryProxy *memoryProxy, rcti *rect) :
	MemoryBuffer(memoryProxy, rect, NUMBER_OF_CHANNELS)
{
}

MemoryBufferValue::MemoryBufferValue(DataType datatype, rcti *rect) :
    MemoryBuffer(datatype, rect, NUMBER_OF_CHANNELS) {
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
        result[0] = this->m_buffer[offset];
	}
}

inline void MemoryBufferValue::readNoCheck(float *result, int x, int y,
	                        MemoryBufferExtend extend_x,
	                        MemoryBufferExtend extend_y)
{

	wrap_pixel(x, y, extend_x, extend_y);
	const int offset = (this->m_chunkWidth * y + x) * NUMBER_OF_CHANNELS;

	BLI_assert(offset >= 0);
	BLI_assert(offset < this->determineBufferSize() * NUMBER_OF_CHANNELS);
	BLI_assert(!(extend_x == COM_MB_CLIP && (x < m_rect.xmin || x >= m_rect.xmax)) &&
		   !(extend_y == COM_MB_CLIP && (y < m_rect.ymin || y >= m_rect.ymax)));

    result[0] = this->m_buffer[offset];
}

inline void MemoryBufferValue::readBilinear(float *result, float x, float y,
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

    float value1;
    float value2;
    float value3;
    float value4;

    read(&value1, x1, y1);
    read(&value2, x1, y2);
    read(&value3, x2, y1);
    read(&value4, x2, y2);

    value1 = value1 * mvaluey + value2 * valuey;
    value3 = value3 * mvaluey + value4 * valuey;
    result[0] = value1 * mvaluex + value3 * valuex;
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

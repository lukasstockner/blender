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

class SamplerNearestValue;
class SamplerNearestNoCheckValue;
class SamplerLinearValue;
class SamplerNearestVector;
class SamplerNearestNoCheckVector;
class SamplerLinearVector;
class SamplerNearestColor;
class SamplerNearestNoCheckColor;
class SamplerLinearColor;

#ifndef _COM_Sampler_h_
#define _COM_Sampler_h_

#include "COM_MemoryBuffer.h"

class BaseSampler {
protected:
	float* m_buffer;
	int m_width;
	int m_height;

	inline void wrap_pixel(int &x, int &y, MemoryBufferExtend extend_x, MemoryBufferExtend extend_y)
	{
		int w = this->m_width;
		int h = this->m_height;

		switch (extend_x) {
			case COM_MB_CLIP:
				break;
			case COM_MB_EXTEND:
				if (x < 0) x = 0;
				if (x >= w) x = w;
				break;
			case COM_MB_REPEAT:
				x = (x >= 0.0f ? (x % w) : (x % w) + w);
				break;
		}

		switch (extend_y) {
			case COM_MB_CLIP:
				break;
			case COM_MB_EXTEND:
				if (y < 0) y = 0;
				if (y >= h) y = h;
				break;
			case COM_MB_REPEAT:
				y = (y >= 0.0f ? (y % h) : (y % h) + h);
				break;
		}
	}

	inline void read_value(float *result, int x, int y, MemoryBufferExtend extend_x = COM_MB_CLIP, MemoryBufferExtend extend_y = COM_MB_CLIP) {
		bool clip_x = (extend_x == COM_MB_CLIP && (x < 0 || x >= this->m_width));
		bool clip_y = (extend_y == COM_MB_CLIP && (y < 0 || y >= this->m_height));
		if (clip_x || clip_y) {
			/* clip result outside rect is zero */
			result[0] = 0.0f;
		}
		else
		{
			this->wrap_pixel(x, y, extend_x, extend_y);
			const int offset = (this->m_width * y + x) * COM_NUM_CHANNELS_VALUE;
			result[0] = this->m_buffer[offset];
		}
	}
	inline void read_vector(float *result, int x, int y, MemoryBufferExtend extend_x = COM_MB_CLIP, MemoryBufferExtend extend_y = COM_MB_CLIP) {
		bool clip_x = (extend_x == COM_MB_CLIP && (x < 0 || x >= this->m_width));
		bool clip_y = (extend_y == COM_MB_CLIP && (y < 0 || y >= this->m_height));
		if (clip_x || clip_y) {
			/* clip result outside rect is zero */
			zero_v3(result);
		}
		else
		{
			this->wrap_pixel(x, y, extend_x, extend_y);
			const int offset = (this->m_width * y + x) * COM_NUM_CHANNELS_VECTOR;
			copy_v3_v3(result, &this->m_buffer[offset]);
		}
	}
	inline void read_color(float *result, int x, int y, MemoryBufferExtend extend_x = COM_MB_CLIP, MemoryBufferExtend extend_y = COM_MB_CLIP) {
		bool clip_x = (extend_x == COM_MB_CLIP && (x < 0 || x >= this->m_width));
		bool clip_y = (extend_y == COM_MB_CLIP && (y < 0 || y >= this->m_height));
		if (clip_x || clip_y) {
			/* clip result outside rect is zero */
			zero_v4(result);
		}
		else
		{
			this->wrap_pixel(x, y, extend_x, extend_y);
			const int offset = (this->m_width * y + x) * COM_NUM_CHANNELS_COLOR;
			copy_v4_v4(result, &this->m_buffer[offset]);
		}
	}


	BaseSampler(MemoryBuffer* buffer) {
		this->m_buffer = buffer->getBuffer();
		this->m_width = buffer->getWidth();
		this->m_height = buffer->getHeight();
	}
};

class SamplerNearestValue: public BaseSampler {
public:
	SamplerNearestValue(MemoryBuffer* buffer) : BaseSampler(buffer) {

	}

	void read(float *result, int x, int y,
			  MemoryBufferExtend extend_x = COM_MB_CLIP,
			  MemoryBufferExtend extend_y = COM_MB_CLIP);

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("COM:SamplerNearestValue")
#endif
};

class SamplerNearestVector: public BaseSampler {
public:
	SamplerNearestVector(MemoryBuffer* buffer) : BaseSampler(buffer) {

	}
	void read(float *result, int x, int y,
			  MemoryBufferExtend extend_x = COM_MB_CLIP,
			  MemoryBufferExtend extend_y = COM_MB_CLIP);

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("COM:SamplerNearestVector")
#endif
};

class SamplerNearestColor: public BaseSampler {
public:
	SamplerNearestColor(MemoryBuffer* buffer) : BaseSampler(buffer) {

	}
	void read(float *result, int x, int y,
			  MemoryBufferExtend extend_x = COM_MB_CLIP,
			  MemoryBufferExtend extend_y = COM_MB_CLIP);

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("COM:SamplerNearestColor")
#endif
};


// -- No CHECK --
class SamplerNearestNoCheckValue: public BaseSampler {
public:
	SamplerNearestNoCheckValue(MemoryBuffer* buffer) : BaseSampler(buffer) {

	}

	void read(float *result, int x, int y,
			  MemoryBufferExtend extend_x = COM_MB_CLIP,
			  MemoryBufferExtend extend_y = COM_MB_CLIP);

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("COM:SamplerNearestValue")
#endif
};

class SamplerNearestNoCheckVector: public BaseSampler {
public:
	SamplerNearestNoCheckVector(MemoryBuffer* buffer) : BaseSampler(buffer) {

	}
	void read(float *result, int x, int y,
			  MemoryBufferExtend extend_x = COM_MB_CLIP,
			  MemoryBufferExtend extend_y = COM_MB_CLIP);

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("COM:SamplerNearestVector")
#endif
};

class SamplerNearestNoCheckColor: public BaseSampler {
public:
	SamplerNearestNoCheckColor(MemoryBuffer* buffer) : BaseSampler(buffer) {

	}
	void read(float *result, int x, int y,
			  MemoryBufferExtend extend_x = COM_MB_CLIP,
			  MemoryBufferExtend extend_y = COM_MB_CLIP);

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("COM:SamplerNearestColor")
#endif
};

// -- Bilinear --
class SamplerBilinearValue: public BaseSampler {
public:
	SamplerBilinearValue(MemoryBuffer* buffer) : BaseSampler(buffer) {

	}

	void read(float *result, int x, int y,
			  MemoryBufferExtend extend_x = COM_MB_CLIP,
			  MemoryBufferExtend extend_y = COM_MB_CLIP);

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("COM:SamplerNearestValue")
#endif
};

class SamplerBilinearVector: public BaseSampler {
public:
	SamplerBilinearVector(MemoryBuffer* buffer) : BaseSampler(buffer) {

	}
	void read(float *result, int x, int y,
			  MemoryBufferExtend extend_x = COM_MB_CLIP,
			  MemoryBufferExtend extend_y = COM_MB_CLIP);

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("COM:SamplerNearestVector")
#endif
};

class SamplerBilinearColor: public BaseSampler {
public:
	SamplerBilinearColor(MemoryBuffer* buffer) : BaseSampler(buffer) {

	}
	void read(float *result, int x, int y,
			  MemoryBufferExtend extend_x = COM_MB_CLIP,
			  MemoryBufferExtend extend_y = COM_MB_CLIP);

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("COM:SamplerNearestColor")
#endif
};

#endif

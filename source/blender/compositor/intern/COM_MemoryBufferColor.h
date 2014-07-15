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

class MemoryBufferColor;

#ifndef _COM_MemoryBufferColor_h_
#define _COM_MemoryBufferColor_h_

#include "COM_MemoryBuffer.h"
#include "COM_Sampler.h"

class MemoryBufferColor: MemoryBuffer
{
private:
	SamplerNearestColor *m_sampler_nearest;
	SamplerNearestNoCheckColor *m_sampler_nocheck;
	SamplerBilinearColor *m_sampler_bilinear;

protected:
	/**
	 * @brief construct new MemoryBuffer for a chunk
	 */
	MemoryBufferColor(MemoryProxy *memoryProxy, unsigned int chunkNumber, rcti *rect);
	
	/**
	 * @brief construct new temporarily MemoryBuffer for an area
	 */
	MemoryBufferColor(MemoryProxy *memoryProxy, rcti *rect);

    MemoryBufferColor(DataType datatype, rcti *rect);


public:
	void init_samplers();
	void deinit_samplers();
	void writePixel(int x, int y, const float *color);
	void addPixel(int x, int y, const float *color);
	void read(float *result, int x, int y,
	                 MemoryBufferExtend extend_x = COM_MB_CLIP,
	                 MemoryBufferExtend extend_y = COM_MB_CLIP);

	void readNoCheck(float *result, int x, int y,
	                        MemoryBufferExtend extend_x = COM_MB_CLIP,
	                        MemoryBufferExtend extend_y = COM_MB_CLIP);

	void readBilinear(float *result, float x, float y,
	                         MemoryBufferExtend extend_x = COM_MB_CLIP,
	                         MemoryBufferExtend extend_y = COM_MB_CLIP);

	SamplerNearestColor* get_sampler_nearest() {return this->m_sampler_nearest;}
	SamplerNearestNoCheckColor* get_sampler_nocheck() {return this->m_sampler_nocheck;}
	SamplerBilinearColor* get_sampler_bilinear() {return this->m_sampler_bilinear;}

 	void readEWA(float result[4], const float uv[2], const float derivatives[2][2], PixelSampler sampler);

	MemoryBuffer *duplicate();
	

	friend class MemoryBuffer;
	
#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("COM:MemoryBufferColor")
#endif
};

#endif

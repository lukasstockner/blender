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

#ifndef _COM_ReadBufferOperation_h
#define _COM_ReadBufferOperation_h

#include "COM_NodeOperation.h"
#include "COM_MemoryProxy.h"
#include "COM_MemoryBufferColor.h"
#include "COM_MemoryBufferValue.h"
#include "COM_MemoryBufferVector.h"

class ReadBufferOperation : public NodeOperation {
private:
	MemoryProxy *m_memoryProxy;
	bool m_single_value; /* single value stored in buffer, copied from associated write operation */
	unsigned int m_offset;
	MemoryBuffer *m_buffer;
public:
    ReadBufferOperation(DataType type);
	void setMemoryProxy(MemoryProxy *memoryProxy) { this->m_memoryProxy = memoryProxy; }
	MemoryProxy *getMemoryProxy() { return this->m_memoryProxy; }
	void determineResolution(unsigned int resolution[2], unsigned int preferredResolution[2]);
	
	void *initializeTileData(rcti *rect);
	void executePixelSampled(float output[4], float x, float y, PixelSampler sampler);
	void executePixelExtend(float output[4], float x, float y, PixelSampler sampler,
	                        MemoryBufferExtend extend_x, MemoryBufferExtend extend_y);
	void executePixelFiltered(float output[4], float x, float y, float dx[2], float dy[2], PixelSampler sampler);
	const bool isReadBufferOperation() const { return true; }
	void setOffset(unsigned int offset) { this->m_offset = offset; }
	unsigned int getOffset() const { return this->m_offset; }
	bool determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output);
	MemoryBuffer *getInputMemoryBuffer(MemoryBuffer **memoryBuffers) { return memoryBuffers[this->m_offset]; }
	void readResolutionFromWriteBuffer();
	void updateMemoryBuffer();

	/**
	 * @brief get_sampler_nearest_color
	 * only supported for when DataType COM_DT_COLOR is used in the constructor
	 * @return
	 */
	SamplerNearestColor* get_sampler_nearest_color() {return ((MemoryBufferColor*)this->m_buffer)->get_sampler_nearest(); }
	SamplerNearestVector* get_sampler_nearest_vector() {return ((MemoryBufferVector*)this->m_buffer)->get_sampler_nearest(); }
	SamplerNearestValue* get_sampler_nearest_value() {return ((MemoryBufferValue*)this->m_buffer)->get_sampler_nearest(); }
	SamplerNearestNoCheckColor* get_sampler_nocheck_color() {return ((MemoryBufferColor*)this->m_buffer)->get_sampler_nocheck(); }
	SamplerNearestNoCheckVector* get_sampler_nocheck_vector() {return ((MemoryBufferVector*)this->m_buffer)->get_sampler_nocheck(); }
	SamplerNearestNoCheckValue* get_sampler_nocheck_value() {return ((MemoryBufferValue*)this->m_buffer)->get_sampler_nocheck(); }
	SamplerBilinearColor* get_sampler_bilinear_color() {return ((MemoryBufferColor*)this->m_buffer)->get_sampler_bilinear(); }
	SamplerBilinearVector* get_sampler_bilinear_vector() {return ((MemoryBufferVector*)this->m_buffer)->get_sampler_bilinear(); }
	SamplerBilinearValue* get_sampler_bilinear_value() {return ((MemoryBufferValue*)this->m_buffer)->get_sampler_bilinear(); }
};

#endif

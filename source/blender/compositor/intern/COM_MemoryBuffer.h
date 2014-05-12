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

class MemoryBuffer;

#ifndef _COM_MemoryBuffer_h_
#define _COM_MemoryBuffer_h_

#include "COM_ExecutionGroup.h"
#include "COM_MemoryProxy.h"
#include "COM_SocketReader.h"

extern "C" {
#  include "BLI_math.h"
#  include "BLI_rect.h"
}

/**
 * @brief state of a memory buffer
 * @ingroup Memory
 */
typedef enum MemoryBufferState {
	/** @brief memory has been allocated on creator device and CPU machine, but kernel has not been executed */
	COM_MB_ALLOCATED = 1,
	/** @brief memory is available for use, content has been created */
	COM_MB_AVAILABLE = 2,
	/** @brief chunk is consolidated from other chunks. special state.*/
	COM_MB_TEMPORARILY = 6
} MemoryBufferState;

typedef enum MemoryBufferExtend {
	COM_MB_CLIP,
	COM_MB_EXTEND,
	COM_MB_REPEAT
} MemoryBufferExtend;

class MemoryProxy;

/**
 * @brief a MemoryBuffer contains access to the data of a chunk
 */
class MemoryBuffer {
private:
	/**
	 * brief refers to the chunknumber within the executiongroup where related to the MemoryProxy
	 * @see memoryProxy
	 */
	unsigned int m_chunkNumber;
	
	
	/**
	 * @brief state of the buffer
	 */
	MemoryBufferState m_state;

	/**
	 * @brief the number of channels that form a single pixel in this buffer
	 */
	unsigned int m_no_channels;
	

protected:
	/**
	 * @brief width of the chunk
	 */
	unsigned int m_chunkWidth;

	/**
	 * @brief region of this buffer inside relative to the MemoryProxy
	 */
	rcti m_rect;
	
	/**
	 * @brief proxy of the memory (same for all chunks in the same buffer)
	 */
	MemoryProxy *m_memoryProxy;

	/**
	 * @brief the actual float buffer/data
	 */
	float *m_buffer;

	/**
	 * @brief construct new MemoryBuffer for a chunk
	 *
	 * @param no_channels Number of channels that must be allocated for every pixel
	 */
	MemoryBuffer(MemoryProxy *memoryProxy, unsigned int chunkNumber, rcti *rect, unsigned int no_channels);
	
	/**
	 * @brief construct new temporarily MemoryBuffer for an area
	 *
	 * @param no_channels Number of channels that must be allocated for every pixel
	 */
	MemoryBuffer(MemoryProxy *memoryProxy, rcti *rect, unsigned int no_channels);

    /**
     * @brief construct new temporarily MemoryBuffer for an area
     *
     * @param no_channels Number of channels that must be allocated for every pixel
     */
    MemoryBuffer(DataType datatype, rcti *rect, unsigned int no_channels);
public:
	/**
	 * @brief factory method for the constructor, selecting the right subclass
	 */
	static MemoryBuffer* create(MemoryProxy *memoryProxy, unsigned int chunkNumber, rcti *rect);

	/**
	 * @brief factory method for the constructor, selecting the right subclass, creating a temporarily buffer
	 */
	static MemoryBuffer* create(MemoryProxy *memoryProxy, rcti *rect);

    /**
     * @brief factory method for the constructor, selecting the right subclass, creating a temporarily buffer
     */
    static MemoryBuffer* create(DataType datatype, rcti *rect);

    /**
	 * @brief destructor
	 */
	virtual ~MemoryBuffer();
	
	/**
	 * @brief read the ChunkNumber of this MemoryBuffer
	 */
	unsigned int getChunkNumber() { return this->m_chunkNumber; }
	
	/**
	 * @brief get the data of this MemoryBuffer
	 * @note buffer should already be available in memory
	 */
	float *getBuffer() { return this->m_buffer; }
	
	/**
	 * @brief after execution the state will be set to available by calling this method
	 */
	void setCreatedState()
	{
		this->m_state = COM_MB_AVAILABLE;
	}
	
	inline void wrap_pixel(int &x, int &y, MemoryBufferExtend extend_x, MemoryBufferExtend extend_y)
	{
		int w = m_rect.xmax - m_rect.xmin;
		int h = m_rect.ymax - m_rect.ymin;
		x = x - m_rect.xmin;
		y = y - m_rect.ymin;
		
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
	
	virtual void read(float *result, int x, int y, 
	                 MemoryBufferExtend extend_x = COM_MB_CLIP,
	                 MemoryBufferExtend extend_y = COM_MB_CLIP) = 0;


	virtual void readNoCheck(float *result, int x, int y,
	                        MemoryBufferExtend extend_x = COM_MB_CLIP,
	                        MemoryBufferExtend extend_y = COM_MB_CLIP) = 0;

		
	virtual void writePixel(int x, int y, const float *color) = 0;
	virtual void addPixel(int x, int y, const float *color) = 0;
	virtual void readBilinear(float *result, float x, float y,
	                         MemoryBufferExtend extend_x = COM_MB_CLIP,
	                         MemoryBufferExtend extend_y = COM_MB_CLIP) = 0;


	virtual void readEWA(float result[4], const float uv[2], const float derivatives[2][2], PixelSampler sampler) {}
	
	/**
	 * @brief is this MemoryBuffer a temporarily buffer (based on an area, not on a chunk)
	 */
	inline const bool isTemporarily() const { return this->m_state == COM_MB_TEMPORARILY; }
	
	/**
	 * @brief add the content from otherBuffer to this MemoryBuffer
	 * @param otherBuffer source buffer
	 *
	 * @note take care when running this on a new buffer since it wont fill in
	 *       uninitialized values in areas where the buffers don't overlap.
	 */
	void copyContentFrom(MemoryBuffer *otherBuffer);
	
	/**
	 * @brief get the rect of this MemoryBuffer
	 */
	rcti *getRect() { return &this->m_rect; }
	
	/**
	 * @brief get the width of this MemoryBuffer
	 */
	int getWidth() const;
	
	/**
	 * @brief get the height of this MemoryBuffer
	 */
	int getHeight() const;
	
	/**
	 * @brief clear the buffer. Make all pixels black transparent.
	 */
	void clear();
	
	virtual MemoryBuffer *duplicate() = 0;
	
	float *convertToValueBuffer();
    virtual float getMaximumValue() const;
	float getMaximumValue(rcti *rect);

	/**
	 * @brief return the number of channels that form a single pixel.
	 *
	 * Value = 1
	 * Vector= 3
	 * Color = 4
	 */
	const int get_no_channels() const;

protected:
    unsigned int determineBufferSize() const;

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("COM:MemoryBuffer")
#endif
};

#endif

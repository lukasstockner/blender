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

#include "COM_MemoryBufferColor.h"

#define NUMBER_OF_CHANNELS COM_NUM_CHANNELS_COLOR

MemoryBufferColor::MemoryBufferColor(MemoryProxy *memoryProxy, unsigned int chunkNumber, rcti *rect): 
	MemoryBuffer(memoryProxy, chunkNumber, rect, NUMBER_OF_CHANNELS)
{
	this->init_samplers();

}
	
MemoryBufferColor::MemoryBufferColor(MemoryProxy *memoryProxy, rcti *rect) :
	MemoryBuffer(memoryProxy, rect, NUMBER_OF_CHANNELS)
{
	this->init_samplers();
}

MemoryBufferColor::MemoryBufferColor(DataType datatype, rcti *rect) :
    MemoryBuffer(datatype, rect, NUMBER_OF_CHANNELS) {
	this->init_samplers();
}

void MemoryBufferColor::init_samplers() {
	this->m_sampler_nearest = new SamplerNearestColor(this);
	this->m_sampler_nocheck = new SamplerNearestNoCheckColor(this);
	this->m_sampler_bilinear = new SamplerBilinearColor(this);
}

void MemoryBufferColor::deinit_samplers() {
	delete this->m_sampler_nearest;
	delete this->m_sampler_nocheck;
	delete this->m_sampler_bilinear;
}

MemoryBuffer *MemoryBufferColor::duplicate()
{
	MemoryBufferColor *result = new MemoryBufferColor(this->m_memoryProxy, &this->m_rect);
	memcpy(result->getBuffer(), this->getBuffer(), this->determineBufferSize() * NUMBER_OF_CHANNELS * sizeof(float));
	return result;
}

// --- write pixels ---
void MemoryBufferColor::writePixel(int x, int y, const float *color)
{
	if (x >= this->m_rect.xmin && x < this->m_rect.xmax &&
	    y >= this->m_rect.ymin && y < this->m_rect.ymax)
	{
		const int offset = (this->m_chunkWidth * (y - this->m_rect.ymin) + x - this->m_rect.xmin) * NUMBER_OF_CHANNELS;
		copy_v4_v4(&this->m_buffer[offset], color);
	}
}

void MemoryBufferColor::addPixel(int x, int y, const float *color)
{
	if (x >= this->m_rect.xmin && x < this->m_rect.xmax &&
	    y >= this->m_rect.ymin && y < this->m_rect.ymax)
	{
		const int offset = (this->m_chunkWidth * (y - this->m_rect.ymin) + x - this->m_rect.xmin) * NUMBER_OF_CHANNELS;
		add_v4_v4(&this->m_buffer[offset], color);
	}
}

// --- SAMPLERS ---
inline void MemoryBufferColor::read(float *result, int x, int y,
									MemoryBufferExtend extend_x,
									MemoryBufferExtend extend_y)
{
	this->m_sampler_nearest->read(result, x, y, extend_x, extend_y);
}

inline void MemoryBufferColor::readNoCheck(float *result, int x, int y,
	                        MemoryBufferExtend extend_x,
	                        MemoryBufferExtend extend_y)
{
	this->m_sampler_nocheck->read(result, x, y, extend_x, extend_y);
}

inline void MemoryBufferColor::readBilinear(float *result, float x, float y,
			 MemoryBufferExtend extend_x,
			 MemoryBufferExtend extend_y)
{
	this->m_sampler_bilinear->read(result, x, y, extend_x, extend_y);
}

/** EWA filtering **/
typedef struct ReadEWAData {
	MemoryBuffer *buffer;
	PixelSampler sampler;
	float ufac, vfac;
} ReadEWAData;

static void read_ewa_pixel_sampled(void *userdata, int x, int y, float result[4])
{
	ReadEWAData *data = (ReadEWAData *) userdata;
	switch (data->sampler) {
		case COM_PS_NEAREST:
			data->buffer->read(result, x, y);
			break;
		case COM_PS_BILINEAR:
			data->buffer->readBilinear(result,
			                           (float)x + data->ufac,
			                           (float)y + data->vfac);
			break;
		case COM_PS_BICUBIC:
			/* TOOD(sergey): no readBicubic method yet */
			data->buffer->readBilinear(result,
			                           (float)x + data->ufac,
			                           (float)y + data->vfac);
			break;
		default:
			zero_v4(result);
			break;
	}
}

void MemoryBufferColor::readEWA(float result[4], const float uv[2], const float derivatives[2][2], PixelSampler sampler)
{
	ReadEWAData data;
	data.buffer = this;
	data.sampler = sampler;
	data.ufac = uv[0] - floorf(uv[0]);
	data.vfac = uv[1] - floorf(uv[1]);

	int width = this->getWidth(), height = this->getHeight();
	/* TODO(sergey): Render pipeline uses normalized coordinates and derivatives,
	 * but compositor uses pixel space. For now let's just divide the values and
	 * switch compositor to normalized space for EWA later.
	 */
	float uv_normal[2] = {uv[0] / width, uv[1] / height};
	float du_normal[2] = {derivatives[0][0] / width, derivatives[0][1] / height};
	float dv_normal[2] = {derivatives[1][0] / width, derivatives[1][1] / height};

	BLI_ewa_filter(this->getWidth(), this->getHeight(),
	               false,
	               true,
	               uv_normal, du_normal, dv_normal,
	               read_ewa_pixel_sampled,
	               &data,
	               result);
}

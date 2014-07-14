#include "COM_Sampler.h"


void SamplerNearestValue::read(float *result, int x, int y, MemoryBufferExtend extend_x, MemoryBufferExtend extend_y) {
	this->read_value(result, x, y, extend_x, extend_y);
}

void SamplerNearestVector::read(float *result, int x, int y, MemoryBufferExtend extend_x, MemoryBufferExtend extend_y) {
	this->read_vector(result, x, y, extend_x, extend_y);
}

void SamplerNearestColor::read(float *result, int x, int y, MemoryBufferExtend extend_x, MemoryBufferExtend extend_y) {
	this->read_color(result, x, y, extend_x, extend_y);
}

// -- Nearest no check --
void SamplerNearestNoCheckValue::read(float *result, int x, int y, MemoryBufferExtend extend_x, MemoryBufferExtend extend_y) {
	this->wrap_pixel(x, y, extend_x, extend_y);
	const int offset = (this->m_width * y + x) * COM_NUM_CHANNELS_VALUE;
	result[0] = this->m_buffer[offset];
}

void SamplerNearestNoCheckVector::read(float *result, int x, int y, MemoryBufferExtend extend_x, MemoryBufferExtend extend_y) {
	this->wrap_pixel(x, y, extend_x, extend_y);
	const int offset = (this->m_width * y + x) * COM_NUM_CHANNELS_VECTOR;
	result[0] = this->m_buffer[offset];
}

void SamplerNearestNoCheckColor::read(float *result, int x, int y, MemoryBufferExtend extend_x, MemoryBufferExtend extend_y) {
	wrap_pixel(x, y, extend_x, extend_y);
	const int offset = (this->m_width * y + x) * COM_NUM_CHANNELS_COLOR;

	BLI_assert(offset >= 0);
	BLI_assert(offset < this->m_width * this->m_height * COM_NUM_CHANNELS_COLOR);
	BLI_assert(!(extend_x == COM_MB_CLIP && (x < 0 || x >= this->m_width)) &&
		   !(extend_y == COM_MB_CLIP && (y < 0 || y >= this->m_height)));

	copy_v4_v4(result, &this->m_buffer[offset]);
}

// -- Bilinear --
void SamplerBilinearValue::read(float *result, int x, int y, MemoryBufferExtend extend_x, MemoryBufferExtend extend_y) {
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

	read_value(&value1, x1, y1);
	read_value(&value2, x1, y2);
	read_value(&value3, x2, y1);
	read_value(&value4, x2, y2);

	value1 = value1 * mvaluey + value2 * valuey;
	value3 = value3 * mvaluey + value4 * valuey;
	result[0] = value1 * mvaluex + value3 * valuex;
}

void SamplerBilinearVector::read(float *result, int x, int y, MemoryBufferExtend extend_x, MemoryBufferExtend extend_y) {
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

	float vector1[COM_NUM_CHANNELS_VECTOR];
	float vector2[COM_NUM_CHANNELS_VECTOR];
	float vector3[COM_NUM_CHANNELS_VECTOR];
	float vector4[COM_NUM_CHANNELS_VECTOR];

	read_vector(vector1, x1, y1);
	read_vector(vector2, x1, y2);
	read_vector(vector3, x2, y1);
	read_vector(vector4, x2, y2);

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

void SamplerBilinearColor::read(float *result, int x, int y, MemoryBufferExtend extend_x, MemoryBufferExtend extend_y) {
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

	float color1[COM_NUM_CHANNELS_COLOR];
	float color2[COM_NUM_CHANNELS_COLOR];
	float color3[COM_NUM_CHANNELS_COLOR];
	float color4[COM_NUM_CHANNELS_COLOR];

	read_color(color1, x1, y1);
	read_color(color2, x1, y2);
	read_color(color3, x2, y1);
	read_color(color4, x2, y2);

	color1[0] = color1[0] * mvaluey + color2[0] * valuey;
	color1[1] = color1[1] * mvaluey + color2[1] * valuey;
	color1[2] = color1[2] * mvaluey + color2[2] * valuey;
	color1[3] = color1[3] * mvaluey + color2[3] * valuey;

	color3[0] = color3[0] * mvaluey + color4[0] * valuey;
	color3[1] = color3[1] * mvaluey + color4[1] * valuey;
	color3[2] = color3[2] * mvaluey + color4[2] * valuey;
	color3[3] = color3[3] * mvaluey + color4[3] * valuey;

	result[0] = color1[0] * mvaluex + color3[0] * valuex;
	result[1] = color1[1] * mvaluex + color3[1] * valuex;
	result[2] = color1[2] * mvaluex + color3[2] * valuex;
	result[3] = color1[3] * mvaluex + color3[3] * valuex;
}


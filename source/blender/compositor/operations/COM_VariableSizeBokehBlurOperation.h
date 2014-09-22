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

#ifndef __COM_VARIABLESIZEBOKEHBLUROPERATION_H__
#define __COM_VARIABLESIZEBOKEHBLUROPERATION_H__
#include "COM_NodeOperation.h"
#include "COM_QualityStepHelper.h"


class VariableSizeBokehBlurOperation : public NodeOperation, public QualityStepHelper {
private:
	int m_maxBlur;
	float m_threshold;
	bool m_do_size_scale;  /* scale size, matching 'BokehBlurNode' */
	SocketReader *m_inputProgram;
	SocketReader *m_inputBokehProgram;
	SocketReader *m_inputSizeProgram;

public:
	VariableSizeBokehBlurOperation();

	/**
	 * the inner loop of this program
	 */
	void executePixel(float output[4], int x, int y, void *data);
	
	/**
	 * Initialize the execution
	 */
	void initExecution();
	
	void *initializeTileData(rcti *rect);
	
	void deinitializeTileData(rcti *rect, void *data);
	
	/**
	 * Deinitialize the execution
	 */
	void deinitExecution();
	
	bool determineDependingAreaOfInterest(rcti *input, ReadBufferOperation *readOperation, rcti *output);
	
	void setMaxBlur(int maxRadius) { this->m_maxBlur = maxRadius; }

	void setThreshold(float threshold) { this->m_threshold = threshold; }

	void setDoScaleSize(bool scale_size) { this->m_do_size_scale = scale_size; }

	void executeOpenCL(OpenCLDevice *device, MemoryBuffer *outputMemoryBuffer, cl_mem clOutputBuffer, MemoryBuffer **inputMemoryBuffers, list<cl_mem> *clMemToCleanUp, list<cl_kernel> *clKernelsToCleanUp);
};

#endif

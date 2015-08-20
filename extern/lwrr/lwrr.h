/******************************************************************************\

  Copyright 2012 KAIST (Korea Advanced Institute of Science and Technology)
  All Rights Reserved.

  Permission to use, copy, modify and distribute this software and its
  documentation for educational, research and non-profit purposes, without
  fee, and without a written agreement is hereby granted, provided that the
  above copyright notice and the following three paragraphs appear in all
  copies. Any use in a commercial organization requires a separate license.

IN NO EVENT SHALL KAIST BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT, 
SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST PROFITS, 
ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF 
KAIST HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.

KAIST SPECIFICALLY DISCLAIM ANY WARRANTIES, INCLUDING, BUT NOT LIMITED TO, 
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS" BASIS, AND KAIST 
HAS NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS,
OR MODIFICATIONS.

   ---------------------------------
  |Please send all BUG REPORTS to:  |
  |                                 |
  |     moonbochang@gmail.com       |
  |                                 |
   ---------------------------------

  The authors may be contacted via:

Mail:         Bochang Moon or Sung-Eui Yoon
 			Dept. of Computer Science, E3-1 
KAIST 
291 Daehak-ro(373-1 Guseong-dong), Yuseong-gu 
DaeJeon, 305-701 
Republic of Korea
\*****************************************************************************/

#pragma once

#include "lwrr_mem.h"
#include "lwrr_setting.h"

class LWRR
{
public:
	LWR_cpu_mem* m_mem;

private:	
	LWR_cuda_mem m_cudaMem;

	int m_nPix;
	int m_width, m_height;
	float** m_width_guess;
	float* m_optImg;
	float* m_optVar;
	float* m_ranks;
	float* m_mse;

	// pointers for input buffers	
	float *m_accImg, *m_accImg2;
	float *m_accNormal, *m_accNormal2;
	float *m_accTexture, *m_accTexture2;
	float *m_accDepth, *m_accDepth2;
	int *m_mapSPP, *m_mapMovingSPP;
	float *m_accTextureMoving, *m_accTextureMoving2;	

protected:
	void estimate_error(float* map_MSE, const int halfWindowSize, const bool isFinalPass);
	void estimate_mse_opt_img(float* _width_img, double* _coef0_bias, double* _coef1_bias, double* _coef0_var, double* _coef1_var, int nIterate, float* _opt_img, float* map_MSE);
	void computeSampleMean();
	void biasCurveFit(float** _bias_map, float** _var_map, const int nIterate, double* _coef0_bias, double* _coef1_bias);
	void varCurveFit(float** _var_map, const int nIterate, double* _coef0_var, double* _coef1_var);
	void estimateOptimalWidth(float* _width_img, double* _coef_bias, double* _coef_var, int nIterate);
public:
	LWRR(const int width, const int height, const int nPix);
	~LWRR();

	void init_lwrr(float* _accImg, float* _accImg2, float* _accNormal, float* _accNormal2, float* _accTexture, float* _accTexture2,
		           float* _accDepth, float* _accDepth2, int* _mapSPP, int* _mapMovingSPP, float* _accTextureMoving, float* _accTextureMoving2);

	// main entry function
	void run_lwrr(int numSamplePerIterations, bool isFinalPass);

	//
	inline float* get_inputImg() { return m_mem->_img; }
	inline float* get_ranks() { return m_ranks;	}
	inline float* get_optImg() { return m_optImg; }
	inline float* get_mse_optImg() { return m_mse; }
};
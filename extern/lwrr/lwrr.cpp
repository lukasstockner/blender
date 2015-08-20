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

#include "lwrr.h"
#include "lwrr_fit.h"

LWRR::LWRR(const int width, const int height, const int nPix) : m_width(width), m_height(height), m_nPix(nPix)
{
	m_mem = new LWR_cpu_mem(nPix, NUM_TEST);
	
	m_width_guess = new float*[NUM_TEST];
	for (int i = 0; i < NUM_TEST; ++i) 
		m_width_guess[i] = new float[nPix];

	m_mse = (float*)malloc(nPix * 3 * sizeof(float));
	m_ranks = (float*)calloc(nPix, sizeof(float));
	m_optImg = (float*)malloc(nPix * 3 * sizeof(float));
	m_optVar = (float*)malloc(nPix * sizeof(float));
}

LWRR::~LWRR()
{
	if (m_mem)
		delete m_mem;

	for (int i = 0; i < NUM_TEST; ++i) 
		delete[] m_width_guess[i];		
	delete[] m_width_guess;

	free(m_mse);
	free(m_ranks);	
	free(m_optImg);
	free(m_optVar);
}

void LWRR::init_lwrr(float* _accImg, float* _accImg2, float* _accNormal, float* _accNormal2, float* _accTexture, float* _accTexture2,
		             float* _accDepth, float* _accDepth2, int* _mapSPP, int* _mapMovingSPP, float* _accTextureMoving, float* _accTextureMoving2)
{
	m_accImg = _accImg;
	m_accImg2 = _accImg2;
	m_accNormal = _accNormal;
	m_accNormal2 = _accNormal2;
	m_accTexture = _accTexture;
    m_accTexture2 = _accTexture2;
	m_accDepth = _accDepth;
	m_accDepth2 = _accDepth2;
	m_mapSPP = _mapSPP;
	m_mapMovingSPP = _mapMovingSPP;
	m_accTextureMoving = _accTextureMoving;
	m_accTextureMoving2 = _accTextureMoving2;
}

void LWRR::computeSampleMean()
{
#pragma omp parallel for schedule(guided, 4)
	for (int i = 0; i < m_nPix; ++i) {	
		const int spp = m_mapSPP[i];
		const float invSPP = 1.0f / spp;
		float invSPP_var = invSPP;

		if (spp > 2) // for unbiased
			invSPP_var = 1.f / (float)(spp - 1);

		for (int c = 0; c < 3; ++c) {
			float meanImg = m_accImg[i * 3 + c] * invSPP;
			m_mem->_img[i * 3 + c] = meanImg;			
			m_mem->_var_img[i * 3 + c] = MAX(0.0f, invSPP_var * (m_accImg2[i * 3 + c] - (float)spp * meanImg * meanImg));
		}

#ifndef FEATURE_MOTION
		const float meanDepth = m_accDepth[i] * invSPP;
		m_mem->_depth[i] = meanDepth;	
		
		m_mem->_var_depth[i] = invSPP_var * (m_accDepth2[i] - (float)spp * meanDepth * meanDepth);
		m_mem->_var_depth[i] = MAX(0.0f, MIN(1.f, m_mem->_var_depth[i]));
		
		for (int c = 0; c < 3; ++c) {
			const float meanNormal = m_accNormal[i * 3 + c] * invSPP;
			m_mem->_normal[i * 3 + c] = meanNormal;
			
			m_mem->_var_normal[i * 3 + c] = invSPP_var * (m_accNormal2[i * 3 + c] - (float)spp * meanNormal * meanNormal);			
			m_mem->_var_normal[i * 3 + c] = MAX(0.f, MIN(1.f, m_mem->_var_normal[i * 3 + c]));

			const float meanTex = m_accTexture[i * 3 + c] * invSPP;
			m_mem->_texture[i * 3 + c] = meanTex;
					
			m_mem->_var_texture[i * 3 + c] = invSPP_var * (m_accTexture2[i * 3 + c] - (float)spp * meanTex * meanTex);					
			m_mem->_var_texture[i * 3 + c] = MAX(0.0f, MIN(1.0f, m_mem->_var_texture[i * 3 + c]));
		}

#else
		const int spp_still = MAX(spp - m_mapMovingSPP[i], 1);
		const int spp_moving = MAX(m_mapMovingSPP[i], 1);		
		float invSPP_var_still = 1.f / (float)spp_still;
		if (spp_still > 1)
			invSPP_var_still = 1.f / (spp_still - 1);

		float invSPP_var_moving = 1.f / (float)spp_moving;
		if (spp_moving > 1)
			invSPP_var_moving = 1.f / (float)(spp_moving - 1);

		const float meanDepth = m_accDepth[i] / (float)spp_still;		
		m_mem->_depth[i] = meanDepth;				
		m_mem->_var_depth[i] = MAX(0.0f, invSPP_var_still * (m_accDepth2[i] - (float)spp_still * meanDepth * meanDepth));

		for (int c = 0; c < 3; ++c) {
			const float meanNormal = m_accNormal[i * 3 + c] / (float)spp_still;
			m_mem->_normal[i * 3 + c] = meanNormal;
			m_mem->_var_normal[i * 3 + c] = MAX(0.0f, invSPP_var_still * (m_accNormal2[i * 3 + c] - (float)spp_still * meanNormal * meanNormal));

			const float meanTex = m_accTexture[i * 3 + c] / (float)spp_still;
			m_mem->_texture[i * 3 + c] = meanTex;			
			m_mem->_var_texture[i * 3 + c] = MAX(0.0f, invSPP_var_still * (m_accTexture2[i * 3 + c] - (float)spp_still * meanTex * meanTex));							

			const float meanTexMov = m_accTextureMoving[i * 3 + c] / (float)spp_moving;
			m_mem->_texture_moving[i * 3 + c] = meanTexMov;	
			m_mem->_var_texture_moving[i * 3 + c] = MAX(0.0f, invSPP_var_moving * (m_accTextureMoving2[i * 3 + c] - (float)spp_moving * meanTexMov * meanTexMov));			
		}
#endif
	}
}

void LWRR::biasCurveFit(float** _bias_map, float** _var_map, const int nIterate, double* _coef0_bias, double* _coef1_bias)
{
	// for 1D quadratic
	const int numCol_quad = 2;		

#pragma omp parallel for schedule(guided, 4)
	for (int idx = 0; idx < m_nPix; ++idx) {						
		double XtX[numCol_quad][numCol_quad] = {0,};
		double XtB[numCol_quad] = {0,};
		double invXtX[numCol_quad][numCol_quad];

		for (int hi = 0; hi < nIterate; ++hi) {			
			double h = m_width_guess[hi][idx];	
			double h2 = h * h;

			XtX[0][0] += 1.0;
			XtX[0][1] += h2;
			XtX[1][0] += h2;
			XtX[1][1] += h2 * h2;

			double b = _bias_map[hi][idx];		
			XtB[0] += b;
			XtB[1] += h2 * b;
		}			

		double det = XtX[0][0] * XtX[1][1] - XtX[1][0] * XtX[0][1] + 0.0001;	
		double invDet = 1.0 / det;

		invXtX[0][0] = invDet * XtX[1][1];
		invXtX[0][1] = -1.0 * invDet * XtX[0][1];
		invXtX[1][0] = -1.0 * invDet * XtX[1][0];
		invXtX[1][1] = invDet * XtX[0][0];		
	

		_coef0_bias[idx] = invXtX[0][0] * XtB[0] + invXtX[0][1] * XtB[1];			
		_coef1_bias[idx] = invXtX[1][0] * XtB[0] + invXtX[1][1] * XtB[1];			
	}
}

void LWRR::varCurveFit(float** _var_map, const int nIterate, double* _coef0_var, double* _coef1_var)
{
	const int numCol_quad = 2;	

	// model = a0 + a2/h^d
	// d = rank
#pragma omp parallel for schedule(guided, 4)
	for (int idx = 0; idx < m_nPix; ++idx) {
		double XtX[numCol_quad][numCol_quad] = {0,};
		double XtB[numCol_quad] = {0,};
		double invXtX[numCol_quad][numCol_quad];

		double rank = m_ranks[idx];		

		double n = m_mapSPP[idx];

		for (int hi = 0; hi < nIterate; ++hi) {			
			double h = m_width_guess[hi][idx];	

			XtX[0][0] += 1.0;
			XtX[0][1] += 1.0 / pow(h, rank);			
			XtX[1][0] += 1.0 / pow(h, rank);
			XtX[1][1] += 1.0 / pow(h, rank * 2.0);

			double v = n * MAX(0.0f, _var_map[hi][idx]);
			XtB[0] += v;
			XtB[1] += v / pow(h, rank);
		}

		double det = XtX[0][0] * XtX[1][1] - XtX[1][0] * XtX[0][1] + 0.0001;		
		double invDet = 1.0 / det;
		invXtX[0][0] = invDet * XtX[1][1];
		invXtX[0][1] = -1.0 * invDet * XtX[0][1];
		invXtX[1][0] = -1.0 * invDet * XtX[1][0];
		invXtX[1][1] = invDet * XtX[0][0];

		double beta[2]; 
		beta[0] = invXtX[0][0] * XtB[0] + invXtX[0][1] * XtB[1];			
		beta[1] = invXtX[1][0] * XtB[0] + invXtX[1][1] * XtB[1];	

		if (beta[1] < 0.f) {
			_coef0_var[idx] = 0.f;			
			_coef1_var[idx] = fabs(beta[1]);
		}
		else {
			_coef0_var[idx] = beta[0];			
			_coef1_var[idx] = beta[1];			
		}	
	}
}

void LWRR::estimateOptimalWidth(float* _width_img, double* _coef_bias, double* _coef_var, int nIterate)
{
#pragma omp parallel for schedule(guided, 4)
	for (int idx = 0; idx < m_nPix; ++idx) {
		// NOTE! this can be small minus value because of precision error.
		//double v = fabs(_coef_var[idx]);
		double n = m_mapSPP[idx];
		double b = _coef_bias[idx];
		double rank = m_ranks[idx];
		double v = _coef_var[idx];
		double opt_h = 0.0;					

		if (v <= 0.0)
			opt_h = 0.0;
		else if (b != 0.0) 			
			opt_h = (float)pow((rank * v) / (4.0 * b * b * n), 1.0 / (double)(4 + rank));				

		// to avoid extrapolation		
		opt_h = MIN(opt_h, (double)m_width_guess[nIterate - 1][idx]);									
		opt_h = MAX(opt_h, 0.01);					
	
		_width_img[idx] = opt_h;
	}
}

void LWRR::estimate_mse_opt_img(float* _width_img, double* _coef0_bias, double* _coef1_bias, double* _coef0_var, double* _coef1_var, int nIterate, float* _opt_img, float* map_MSE)
{
#pragma omp parallel for schedule(guided, 4)
	for (int i = 0; i < m_nPix; ++i) {		
		double h = _width_img[i];
		double rank = m_ranks[i];
		double n = m_mapSPP[i];
		double invN = 1.0 / n;
		double b;
		double v;

		v = (_coef0_var[i] + _coef1_var[i] * pow(h, -1.0 * rank)) * invN;				
		b = _coef1_bias[i] * h * h;

		double mse = MAX(0.0, v + b * b);		
		double luminance = _opt_img[i * 3 + 0] * 0.33333f + 
			               _opt_img[i * 3 + 1] * 0.33333f + 
						   _opt_img[i * 3 + 2] * 0.33333f;		

		double MSE_per_sample = mse * pow(n, -4.0 / (rank + 4.0));
		double sppMSE =  MSE_per_sample / (luminance * luminance + 0.001);			
		map_MSE[i * 3 + 0] = map_MSE[i * 3 + 1] = map_MSE[i * 3 + 2] = sppMSE; 
	}
}

void LWRR::estimate_error(float* map_MSE, const int halfWindowSize, const bool isFinalPass) 
{
	const int nIterate = NUM_TEST;

	// First Pass
	if (!m_cudaMem.m_isInit) {
		m_cudaMem.allocMemory(m_nPix);
		allocTextureMemory(m_width, m_height);

		#pragma omp parallel for schedule(guided, 4)
		for (int i = 0; i < m_nPix; ++i) {
			float hmax = 1.f;						
			for (int iter = 0; iter < nIterate; ++iter)	
				m_width_guess[iter][i] = hmax * ((float)(iter + 1) / (float)nIterate);			
		}		
	}

	/////
	int64_t totalNum = 0LL;
	int maxSamples = 0;
	for (int idx = 0; idx < m_nPix; ++idx) {
		totalNum += m_mapSPP[idx];	
		if (maxSamples < m_mapSPP[idx])
			maxSamples = m_mapSPP[idx];
	}
	double avgSpp = (long double)(totalNum) / m_nPix;	
	printf("Avg SPP = %.1f\n", avgSpp);
	////////////////

#ifndef FEATURE_MOTION
	// prepare device memory
	initDeviceMemory(m_mem->_img, m_mem->_var_img, m_mem->_texture, m_mem->_var_texture, m_mem->_normal, m_mem->_var_normal, 
		             m_mem->_depth, m_mem->_var_depth, m_mem->_texture_moving, m_mem->_var_texture_moving,
					 m_mapSPP, m_width, m_height);
#else
	// Fill Undefined holes
	int* still_spp = (int*)malloc(m_nPix * sizeof(int));
#pragma omp parallel for schedule(guided, 4)
	for (int i = 0; i < m_nPix; ++i) {
		still_spp[i] = MAX(0, m_mapSPP[i] - m_mapMovingSPP[i]);
	}
	localGuassianFillHoles(m_mem->_normal, still_spp, m_width, m_height, halfWindowSize, true);
	localGuassianFillHoles(m_mem->_texture, still_spp, m_width, m_height, halfWindowSize, true);
	localGuassianFillHoles(m_mem->_depth, still_spp, m_width, m_height, halfWindowSize, false);
	
	initDeviceMemory(m_mem->_img, m_mem->_var_img, m_mem->_texture, m_mem->_var_texture, m_mem->_normal, m_mem->_var_normal, 
		             m_mem->_depth, m_mem->_var_depth, m_mem->_texture_moving, m_mem->_var_texture_moving,
					 m_mapSPP, m_width, m_height, m_mapMovingSPP);	
	free(still_spp);
#endif

	float* dbgImg = NULL;

	localFitShared(dbgImg, m_width, m_height, halfWindowSize,
		           m_mem->_hessians, m_mem->_fit_map, m_mem->_var_map, m_mem->_bias_map, m_ranks, m_width_guess,
				   m_cudaMem, m_mapSPP);

	/////////////////////////////////////////////////////////////////////	
	// parametric curve fit	
	varCurveFit(m_mem->_var_map, nIterate, m_mem->_coef0_var, m_mem->_coef1_var);
	biasCurveFit(m_mem->_bias_map, m_mem->_var_map, nIterate, m_mem->_coef0_bias, m_mem->_coef1_bias);	
	/////////////////////////////////////////////////////////////////////	

	estimateOptimalWidth(m_mem->_width_img, m_mem->_coef1_bias, m_mem->_coef1_var, nIterate);	
	localGuassian2(m_mem->_width_img, m_cudaMem._d_temp_mem1, m_cudaMem._d_temp_mem2, m_width, m_height, 1.f, false);	

	// Last Filtting
	localFitSharedFinal(m_optImg, m_width, m_height, halfWindowSize, m_mem->_width_img, m_cudaMem, m_optVar);	


	///////////////////////////////////////////////////////////////////////////////////////////////	
	if (!isFinalPass) {
		estimate_mse_opt_img(m_mem->_width_img, m_mem->_coef0_bias, m_mem->_coef1_bias, m_mem->_coef0_var, m_mem->_coef1_var, nIterate, m_optImg, map_MSE);
		localGuassian2(map_MSE, m_cudaMem._d_temp_mem1, m_cudaMem._d_temp_mem2, m_width, m_height, 1.f, true);	
	}

	if (isFinalPass)
		freeDeviceMemory();	
}

void LWRR::run_lwrr(int numSamplePerIterations, bool isFinalPass)
{
	int halfWindowSize = MAX_HALF_WINDOW;
	if (!isFinalPass) 
		halfWindowSize = MAX_HALF_WINDOW_INTER;	

	computeSampleMean();
	estimate_error(m_mse, halfWindowSize, isFinalPass);
}

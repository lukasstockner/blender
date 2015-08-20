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

#include "lwrr_setting.h"

class LWR_cpu_mem {
private:
	int m_nIterate;
public:
	float* _img;
	float* _var_img;
	float* _normal;
	float* _var_normal;
	float* _texture;
	float* _var_texture;
	float* _depth;
	float* _var_depth;

	// for video
	float* _opt_var;

	// Estimation 	
	float* _width_img;

	float** _var_map;
	float** _bias_map;
	float** _fit_map;

	double* _coef0_bias;
	double* _coef1_bias;

	double* _coef1_var;
	double* _coef0_var;

	float* _hessians;
	float* _transform;

	// Additional feature
	float* _texture_moving;
	float* _var_texture_moving;
	
	// Video
	int* _spp;
	
public:
	LWR_cpu_mem(int nPix, int nIterate) {
		m_nIterate = nIterate;

		_img = (float*)malloc(nPix * 3 * sizeof(float));
		_var_img = (float*)malloc(nPix * 3 * sizeof(float));
		_normal = (float*)malloc(nPix * 3 * sizeof(float));
		_var_normal = (float*)malloc(nPix * 3 * sizeof(float));
		_texture = (float*)malloc(nPix * 3 * sizeof(float));
		_var_texture = (float*)malloc(nPix * 3 * sizeof(float));
		_depth = (float*)malloc(nPix * sizeof(float));
		_var_depth = (float*)malloc(nPix * sizeof(float));

		//
		_opt_var = (float*)malloc(nPix * sizeof(float));

		//
		_width_img = (float*)malloc(nPix * sizeof(float));	

		_coef0_bias = (double*)malloc(nPix * sizeof(double));
		_coef1_bias = (double*)malloc(nPix * sizeof(double));

		_coef0_var = (double*)malloc(nPix * sizeof(double));
		_coef1_var = (double*)malloc(nPix * sizeof(double));

		_var_map = (float**)malloc(nIterate * sizeof(float*));
		_bias_map = (float**)malloc(nIterate * sizeof(float*));
		_fit_map = (float**)malloc(nIterate * sizeof(float*));


		for (int i = 0; i < nIterate; ++i) {
			_var_map[i] = (float*)malloc(nPix * sizeof(float));
			_bias_map[i] = (float*)malloc(nPix * sizeof(float));
			_fit_map[i] = (float*)malloc(nPix * 3 * sizeof(float));
		}

		_hessians = (float*)malloc(nPix * nDimens * sizeof(float));				
		_transform = (float*)malloc(nPix * nDimens * nDimens * sizeof(float));	

		//
		_texture_moving = _var_texture_moving = NULL;

		_spp = (int*)malloc(nPix * sizeof(int));

		#ifdef FEATURE_MOTION
		_texture_moving = (float*)malloc(nPix * 3 * sizeof(float));
		_var_texture_moving = (float*)malloc(nPix * 3 * sizeof(float));
		#endif
	}
	~LWR_cpu_mem() {
		free(_img);
		free(_var_img);
		free(_normal);
		free(_var_normal);
		free(_texture);
		free(_var_texture);
		free(_depth);
		free(_var_depth);
		free(_opt_var);

		free(_width_img);
		free(_coef0_bias);
		free(_coef1_bias);
		free(_coef0_var);
		free(_coef1_var);
		for (int i = 0; i < m_nIterate; ++i) {
			free(_var_map[i]);
			free(_bias_map[i]);
			free(_fit_map[i]);
		}

		free(_var_map);
		free(_bias_map);
		free(_fit_map);
		free(_hessians);
		free(_transform);

		free(_spp);

		// Additional feature
		if (!_texture_moving) 
			free(_texture_moving);
		if (!_var_texture_moving) 
			free(_var_texture_moving);
	}
};

class LWR_cuda_mem {
public:
	bool m_isInit;
	float* _d_out;
	float* _d_ranks;
	float* _d_var_map;
	float* _d_bias_map;
	float* _d_transform;
	float* _d_hessians;
	float* _d_bandwidth;

	float* _d_temp_mem1;
	float* _d_temp_mem2;

public:
	LWR_cuda_mem() 
	{		
		m_isInit = false;
		_d_out = _d_ranks = _d_var_map = _d_bias_map = _d_transform = _d_hessians = _d_bandwidth = NULL;
		_d_temp_mem1 = _d_temp_mem2 = NULL;
	}
	~LWR_cuda_mem() {
		deallocMemory();
	}

	void allocMemory(int nPix);
	void deallocMemory();
};
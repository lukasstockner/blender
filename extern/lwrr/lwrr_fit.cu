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

#include "lwrr_fit.h"

#include <cuda_runtime.h>
#include <cuda_runtime_api.h>
#include <cuda.h>
#include "helper_cuda.h"

#include "svd.cuh"

#define PI		3.141592f	// Pi
#define PI2		6.283185f	// Pi^2

#define IMAD(a, b, c) ( __mul24((a), (b)) + (c) )

////////////////////////////////////////////////////////////////////////////////
// Global data handlers and parameters
////////////////////////////////////////////////////////////////////////////////
texture<float4, cudaTextureType2D, cudaReadModeElementType> g_img;
texture<float4, cudaTextureType2D, cudaReadModeElementType> g_texture;
texture<float4, cudaTextureType2D, cudaReadModeElementType> g_normal_depth;
texture<float,  cudaTextureType2D, cudaReadModeElementType> g_texture_moving;

texture<float4, cudaTextureType2D, cudaReadModeElementType> g_var_img;
texture<float4, cudaTextureType2D, cudaReadModeElementType> g_var_texture;
texture<float4, cudaTextureType2D, cudaReadModeElementType> g_var_texture_moving;
texture<float4, cudaTextureType2D, cudaReadModeElementType> g_var_normal;
texture<float4, cudaTextureType2D, cudaReadModeElementType> g_var_feature;

texture<float, cudaTextureType2D, cudaReadModeElementType> g_grey_texture;
texture<float, cudaTextureType2D, cudaReadModeElementType> g_depth;
texture<float, cudaTextureType2D, cudaReadModeElementType> g_var_depth;

texture<float, cudaTextureType2D, cudaReadModeElementType> g_spp;
texture<float, cudaTextureType2D, cudaReadModeElementType> g_spp_still;

//CUDA array descriptor
cudaArray *g_src_img;
cudaArray *g_src_var_img;
cudaArray *g_src_texture;
cudaArray *g_src_texture_moving;
cudaArray *g_src_depth;
cudaArray *g_src_normal_depth;
cudaArray *g_src_var_feature;
cudaArray *g_src_grey_texture;

cudaArray *g_src_var_depth;
cudaArray *g_src_var_texture;
cudaArray *g_src_var_texture_moving;
cudaArray *g_src_var_normal;
cudaArray *g_src_spp;
cudaArray *g_src_spp_still;

inline int iDivUp(int a, int b) { return ((a % b) != 0) ? (a / b + 1) : (a / b); }

__device__ float4 operator+ (const float4& a, const float4& b)  
{ 
	return make_float4(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w); 
}

__device__ float4 operator- (const float4& a, const float4& b)  
{ 
	return make_float4(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w); 
}

__device__ float dist2(const float4& val1, const float4& val2)
{
	return ((val1.x - val2.x) * (val1.x - val2.x) +
		    (val1.y - val2.y) * (val1.y - val2.y) +
		    (val1.z - val2.z) * (val1.z - val2.z));
}

__device__ float Color2Grey(const float4& color)
{
	return (color.x * 0.33333f + color.y * 0.33333f + color.z * 0.33333f);
}

// Input - A (only upper triangle is set!)
__device__ void cholesky(float *A, int n, float *L) 
{
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < (i+1); ++j) {
            float s = 0.0f;
            for (int k = 0; k < j; ++k)
                s += L[i * n + k] * L[j * n + k];
            L[i * n + j] = (i == j) ? sqrtf(A[i * n + i] - s) : (1.0f / L[j * n + j] * (A[j * n + i] - s));
        }
	}
}

// FilterMemoryCUDA members
void LWR_cuda_mem::allocMemory(int nPix)
{
	checkCudaErrors(cudaMalloc((void **)&_d_out,      nPix * 3 * sizeof(float)));	

	checkCudaErrors(cudaMalloc((void **)&_d_var_map,  nPix * sizeof(float)));
	checkCudaErrors(cudaMalloc((void **)&_d_bias_map, nPix * sizeof(float)));

	checkCudaErrors(cudaMalloc((void **)&_d_ranks, nPix * sizeof(float)));
	checkCudaErrors(cudaMalloc((void **)&_d_hessians,  nDimens * nPix * sizeof(float)));		
	checkCudaErrors(cudaMalloc((void **)&_d_transform, nDimens * nDimens * nPix * sizeof(float)));
	checkCudaErrors(cudaMalloc((void **)&_d_bandwidth, nPix * sizeof(float)));

	checkCudaErrors(cudaMalloc((void **)&_d_temp_mem1, nPix * 3 * sizeof(float)));
	checkCudaErrors(cudaMalloc((void **)&_d_temp_mem2, nPix * 3 * sizeof(float)));

	checkCudaErrors(cudaGetLastError());

	m_isInit = true;
}

void LWR_cuda_mem::deallocMemory()
{
	if (m_isInit) {
		checkCudaErrors(cudaFree(_d_out));		
		checkCudaErrors(cudaFree(_d_var_map));
		checkCudaErrors(cudaFree(_d_bias_map));
		checkCudaErrors(cudaFree(_d_ranks));
		checkCudaErrors(cudaFree(_d_hessians));		
		checkCudaErrors(cudaFree(_d_transform));		
		checkCudaErrors(cudaFree(_d_bandwidth));

		checkCudaErrors(cudaFree(_d_temp_mem1));	
		checkCudaErrors(cudaFree(_d_temp_mem2));	

		checkCudaErrors(cudaGetLastError());
	}
	m_isInit = false;
}

__device__ void getTransfCoord(float* iNewVec, float transf[][nDimens], float4& cNorDepth, float4& cTex,  
	                           int x, int y, int cx, int cy, int localD)
{	
	float4 iTex = tex2D(g_texture, x, y);
	float4 iNorDepth = tex2D(g_normal_depth, x, y);				
	for (int col = 0; col < localD; ++col) {							
		iNewVec[col] =  transf[0][col] * (x - cx) + 
		        	transf[1][col] * (y - cy) +
				transf[2][col] * (iNorDepth.w - cNorDepth.w) +
				transf[3][col] * (iNorDepth.x - cNorDepth.x) +
				transf[4][col] * (iNorDepth.y - cNorDepth.y) +
				transf[5][col] * (iNorDepth.z - cNorDepth.z) +
				transf[6][col] * (iTex.x - cTex.x) + 
				transf[7][col] * (iTex.y - cTex.y) +
				transf[8][col] * (iTex.z - cTex.z);				
	}
}

__device__ void getTransfCoordExtended(float* iNewVec, float transf[][nDimens], float4& cNorDepth, float4& cTex,  float cMovTex,
	                           int x, int y, int cx, int cy, int localD)
{	
	float iMovTex = tex2D(g_texture_moving, x, y);
	float4 iTex = tex2D(g_texture, x, y);
	float4 iNorDepth = tex2D(g_normal_depth, x, y);			

	for (int col = 0; col < localD; ++col) {							
		iNewVec[col] = transf[0][col] * (x - cx) + 
			           transf[1][col] * (y - cy) +
				       transf[2][col] * (iNorDepth.w - cNorDepth.w) +
					   transf[3][col] * (iNorDepth.x - cNorDepth.x) +
					   transf[4][col] * (iNorDepth.y - cNorDepth.y) +
					   transf[5][col] * (iNorDepth.z - cNorDepth.z) +
					   transf[6][col] * (iTex.x - cTex.x) + 
					   transf[7][col] * (iTex.y - cTex.y) +
					   transf[8][col] * (iTex.z - cTex.z) +				
					   transf[9][col] * (iMovTex - cMovTex);
	}
}

__global__
void gaussian_fill_hole(const float* _in, const int* _spp, float* _out, int halfWidth, bool isColor, int xSize, int ySize)
{
	const int cx = IMAD(blockDim.x, blockIdx.x, threadIdx.x);
    const int cy = IMAD(blockDim.y, blockIdx.y, threadIdx.y);

	if (cx >= xSize || cy >= ySize)
		return;
	
	const int cIdx = cy * xSize + cx;	

	int ix, iy, idx;	

	int2 startWindow = make_int2(MAX(0, cx - halfWidth), MAX(0, cy - halfWidth));
	int2 endWindow   = make_int2(MIN(xSize - 1, cx + halfWidth), MIN(ySize - 1, cy + halfWidth));	

	int isHole = 0;

	isHole = isHole | (_spp[cIdx] == 0);

	if (isHole == 0) {
		if (isColor) {
			_out[cIdx * 3 + 0] = _in[cIdx * 3 + 0];
			_out[cIdx * 3 + 1] = _in[cIdx * 3 + 1];
			_out[cIdx * 3 + 2] = _in[cIdx * 3 + 2];
		}
		else
			_out[cIdx] = _in[cIdx];
		return;
	}


	float weight;
	float outColor[3] = {0.f,};
	float sumWeight = 0.f;
	for (iy = startWindow.y; iy <= endWindow.y; ++iy) {
		for (ix = startWindow.x; ix <= endWindow.x; ++ix) {	
			idx = iy * xSize + ix;
			if (_spp[idx] > 0) {
				weight = 1.f;
				if (isColor) {
					outColor[0] += weight * _in[idx * 3 + 0];
					outColor[1] += weight * _in[idx * 3 + 1];
					outColor[2] += weight * _in[idx * 3 + 2];
				}
				else 
					outColor[0] += weight * _in[idx];
				sumWeight += weight;
			}			
		}
	}

	float invSum = 1.f / max(sumWeight, 0.01f);
	if (isColor) {
		_out[cIdx * 3 + 0] = outColor[0] * invSum;
		_out[cIdx * 3 + 1] = outColor[1] * invSum;
		_out[cIdx * 3 + 2] = outColor[2] * invSum;
	}
	else
		_out[cIdx] = outColor[0] * invSum;
}

__global__
void gaussian_fit(float* _in, float* _out, float h, bool isColor, int xSize, int ySize, bool isIntegral)
{
	const int cx = IMAD(blockDim.x, blockIdx.x, threadIdx.x);
    const int cy = IMAD(blockDim.y, blockIdx.y, threadIdx.y);

	if (cx >= xSize || cy >= ySize)
		return;
	
	const int cIdx = cy * xSize + cx;	
	int halfWindowSize = int(h * 3.f + 0.5f);

	int ix, iy, idx;	

	int2 startWindow = make_int2(MAX(0, cx - halfWindowSize), MAX(0, cy - halfWindowSize));
	int2 endWindow   = make_int2(MIN(xSize - 1, cx + halfWindowSize), MIN(ySize - 1, cy + halfWindowSize));	

	float dist, weight;
	float outColor[3] = {0.f,};
	float sumWeight = 0.f;
	for (iy = startWindow.y; iy <= endWindow.y; ++iy) {
		for (ix = startWindow.x; ix <= endWindow.x; ++ix) {	
			idx = iy * xSize + ix;
			dist = (iy - cy) * (iy - cy) + (ix - cx) * (ix - cx);
			weight = expf(-1.f * dist / (2.f * h * h));
			if (isColor) {
				outColor[0] += weight * _in[idx * 3 + 0];
				outColor[1] += weight * _in[idx * 3 + 1];
				outColor[2] += weight * _in[idx * 3 + 2];
			}
			else 
				outColor[0] += weight * _in[idx];
			sumWeight += weight;
		}
	}

	float invSum = 1.f / sumWeight;
	if (isIntegral)
		invSum = 1.f;

	if (isColor) {
		_out[cIdx * 3 + 0] = outColor[0] * invSum;
		_out[cIdx * 3 + 1] = outColor[1] * invSum;
		_out[cIdx * 3 + 2] = outColor[2] * invSum;
	}
	else
		_out[cIdx] = outColor[0] * invSum;
}

__global__ void kernel_compute_derivatives_approx(float* _out,
												  float* _hessians, 
												  const float* _ranks, const float* _transform, 
												  const int xSize, const int ySize, const int MAX_HALF, const float h) 
{
	const int cx = IMAD(blockDim.x, blockIdx.x, threadIdx.x);
    const int cy = IMAD(blockDim.y, blockIdx.y, threadIdx.y);	

	// this branch should be here after shared memory loading!!
	if (cx >= xSize || cy >= ySize)
		return;

	const int cIdx = cy * xSize + cx;
	int2 startWindow = make_int2(MAX(0, cx - MAX_HALF), MAX(0, cy - MAX_HALF));
	int2 endWindow   = make_int2(MIN(xSize - 1, cx + MAX_HALF), MIN(ySize - 1, cy + MAX_HALF));	

	/////////////////////////////////////////////////
	// transform loading here
	const int localD = _ranks[cIdx];
	const int localQuadD = localD + localD;
	const int localP = localD + 1;	
	const int localQuadP = localQuadD + 1;

	float transform[nDimens][nDimens];

	#pragma unroll
	for (int row = 0; row < nDimens; ++row) {
		for (int col = 0; col < localD; ++col)
			transform[row][col] = _transform[xSize * ySize * (row * nDimens + col) + cIdx];
	}
	////////////////////////

	float4 cImg = tex2D(g_img, cx, cy);
	float4 cNorDepth = tex2D(g_normal_depth, cx, cy);
	float4 cTex = tex2D(g_texture, cx, cy);
	const int QuadP = (nDimens + 1) + nDimens + nDimens;
	float A[QuadP * QuadP] = {0.f,};	
	float XtB[QuadP][3] = {0.f,};	
	float iNewVecX[QuadP - 1];

#ifdef FEATURE_MOTION
	float cMovTex = tex2D(g_texture_moving, cx, cy);
#endif

	for (int y = startWindow.y; y <= endWindow.y; ++y) {		
		for (int x = startWindow.x; x <= endWindow.x; ++x) {
			float4 iImg = tex2D(g_img, x, y);	

#ifdef FEATURE_MOTION
			getTransfCoordExtended(iNewVecX, transform, cNorDepth, cTex, cMovTex, x, y, cx, cy, localD);						
#else
			getTransfCoord(iNewVecX, transform, cNorDepth, cTex, x, y, cx, cy, localD);			
#endif
			
			float weight = 1.f;
			for (int col = 0; col < localD; ++col) {
				float t = iNewVecX[col] / h;
				if (fabs(t) < 1.f)
					weight *= 0.75f * (1.f - t * t); 
				else {
					weight = 0.f;
					break;
				}
			}	

			if (weight > 0.f) {				
#ifdef OUTLIER_TRICK	
				weight /= MAX(iImg.w, 1.f);							
#endif
			
				for (int col = 0; col < localD; ++col)
					iNewVecX[localD + col] = iNewVecX[col] * iNewVecX[col];		

				A[0] += weight;				
				XtB[0][0] += weight * iImg.x;
				XtB[0][1] += weight * iImg.y;
				XtB[0][2] += weight * iImg.z;				

				
				for (int col = 1; col < localQuadP; ++col) {
					float temp = weight * iNewVecX[col - 1];

					A[col] += temp;					
					XtB[col][0] += temp * iImg.x;
					XtB[col][1] += temp * iImg.y;
					XtB[col][2] += temp * iImg.z;
				}

				// other rows
				#pragma unroll
				for (int row = 1; row < localQuadP; ++row) {
					for (int col = row; col < localQuadP; ++col) 
						A[row * localQuadP + col] += weight * iNewVecX[row - 1] * iNewVecX[col - 1];
				}		
			}
		}
	}

	for (int row = 0; row < localQuadP; ++row)
		A[row * localQuadP + row] += 0.001f;

	float* L = A;
	cholesky(A, localQuadP, L);

	for (int c = 0; c < 3; ++c) {
		////////////////////////////////////////////////////////////////////////////////////////////////
		// Forward substitution
		XtB[0][c] = XtB[0][c] / L[0 * localQuadP + 0];
		for (int i = 1; i < localQuadP; ++i) {
			float s = 0.f;
			for (int k = 0; k < i; ++k) {
				s += L[i * localQuadP + k] * XtB[k][c];
			}
			XtB[i][c] = (XtB[i][c] - s) / L[i * localQuadP + i];
		}
		////////////////////////////////////////////////////////////////////////////////////////////////
		// Backward substituation
		XtB[localQuadP - 1][c] = XtB[localQuadP - 1][c] / L[(localQuadP - 1) * localQuadP + (localQuadP - 1)];		
		for (int i = localQuadP - 2; i >= 0; --i) {
			float s = 0.f;
			for (int k = localQuadP - 1; k > i; --k) {
				s += L[k * localQuadP + i] * XtB[k][c];
			}
			XtB[i][c] = (XtB[i][c] - s) / L[i * localQuadP + i];
		}
	}
	
	for (int f = 0; f < nDimens; ++f) { 			
		_hessians[xSize * ySize * f + cIdx] = 2.f * (fabs(XtB[localP + f][0]) * 0.33333f +
													 fabs(XtB[localP + f][1]) * 0.33333f +
													 fabs(XtB[localP + f][2]) * 0.33333f);
	}
}

__global__ void kernel_compute_transform(float* _out, float* _transform, float* _ranks,
										 const int xSize, const int ySize, const int MAX_HALF, const float* _bandwidth)
{
	const int cx = IMAD(blockDim.x, blockIdx.x, threadIdx.x);
    const int cy = IMAD(blockDim.y, blockIdx.y, threadIdx.y);		
	if (cx >= xSize || cy >= ySize)
		return;

	int2 startWindow = make_int2(MAX(0, cx - MAX_HALF), MAX(0, cy - MAX_HALF));
	int2 endWindow   = make_int2(MIN(xSize - 1, cx + MAX_HALF), MIN(ySize - 1, cy + MAX_HALF));	

	float4 avgNorDepth = make_float4(0.f, 0.f, 0.f, 0.f);
	float4 avgTex = make_float4(0.f, 0.f, 0.f, 0.f);
	float avgMovTex = 0.f;

	for (int y = endWindow.y; y >= startWindow.y; y--) {		
		for (int x = endWindow.x; x >= startWindow.x; x--) {
			const float4& iTex = tex2D(g_texture, x, y);
			const float4& iNorDepth = tex2D(g_normal_depth, x, y);
			avgNorDepth = avgNorDepth + iNorDepth;
			avgTex = avgTex + iTex;

#ifdef FEATURE_MOTION
			avgMovTex += tex2D(g_texture_moving, x, y);
#endif
		}
	}

	float invN = 1.f / ((endWindow.y - startWindow.y + 1) * (endWindow.x - startWindow.x + 1));
	avgNorDepth = make_float4(avgNorDepth.x * invN, avgNorDepth.y * invN, avgNorDepth.z * invN, avgNorDepth.w * invN);
	avgTex = make_float4(avgTex.x * invN, avgTex.y * invN, avgTex.z * invN, 0.f);

#ifdef FEATURE_MOTION
	avgMovTex *= invN;
#endif

	/////////////////////////////////////////////////////////////////////////////////
	// Column normalization
	float factorDepth, factorNormal, factorTexture;
	factorDepth = factorNormal = factorTexture = 0.f;

#ifndef FEATURE_MOTION
	{
		for (int y = startWindow.y; y <= endWindow.y; ++y) {		
			for (int x = startWindow.x; x <= endWindow.x; ++x) {
				const float4& iNorDepth = tex2D(g_normal_depth, x, y);
				const float4& iTex = tex2D(g_texture, x, y);
				factorDepth = max(factorDepth, fabs(iNorDepth.w - avgNorDepth.w));
				factorNormal = max(factorNormal, dist2(iNorDepth, avgNorDepth));
				factorTexture = max(factorTexture, dist2(iTex, avgTex));
			}
		}
		factorTexture = 1.f / max(sqrtf(factorTexture), 0.01f);
		factorNormal = 1.f / max(sqrtf(factorNormal), 0.01f);	
		factorDepth = 1.f / max(factorDepth, 0.01f);
	}
#else
	float facMovTex = 0.f;	
	for (int y = startWindow.y; y <= endWindow.y; ++y) {		
		for (int x = startWindow.x; x <= endWindow.x; ++x) {			
			const float iMovTex = tex2D(g_texture_moving, x, y);
			facMovTex = max(facMovTex, fabs(iMovTex - avgMovTex));
		}
	}
	facMovTex = 1.f / max(sqrtf(facMovTex), 0.01f);		

	float spp_still = tex2D(g_spp_still, cx, cy);
	if (spp_still > 0.01f) 
	{
		for (int y = startWindow.y; y <= endWindow.y; ++y) {		
			for (int x = startWindow.x; x <= endWindow.x; ++x) {
				const float4& iNorDepth = tex2D(g_normal_depth, x, y);
				const float4& iTex = tex2D(g_texture, x, y);
				factorDepth = max(factorDepth, fabs(iNorDepth.w - avgNorDepth.w));
				factorNormal = max(factorNormal, dist2(iNorDepth, avgNorDepth));
				factorTexture = max(factorTexture, dist2(iTex, avgTex));
			}
		}
		factorTexture = 1.f / max(sqrtf(factorTexture), 0.01f);
		factorDepth = 1.f / max(factorDepth, 0.01f);
		factorNormal = 1.f / max(sqrtf(factorNormal), 0.01f);	
	}	
	// else
	// We don't have any samples for this basic G-buffer (Undefined) - Ignore those buffers by making factor zero
#endif

	float delta[nDimens];
	float A[nDimens * nDimens] = {0.f,};	
	float errNorm = 0.f;	
	const float invMaxHalfWindow = 1.f / MAX_HALF;

	for (int y = endWindow.y; y >= startWindow.y; y--) {		
		for (int x = endWindow.x; x >= startWindow.x; x--) {
			const float4& iTex = tex2D(g_texture, x, y);
			const float4& iNorDepth = tex2D(g_normal_depth, x, y);

			delta[0] = (x - cx) * invMaxHalfWindow;
			delta[1] = (y - cy) * invMaxHalfWindow;
			delta[2] = (iNorDepth.w - avgNorDepth.w) * factorDepth;
			delta[3] = (iNorDepth.x - avgNorDepth.x) * factorNormal;
			delta[4] = (iNorDepth.y - avgNorDepth.y) * factorNormal;
			delta[5] = (iNorDepth.z - avgNorDepth.z) * factorNormal;
			delta[6] = (iTex.x - avgTex.x) * factorTexture;
			delta[7] = (iTex.y - avgTex.y) * factorTexture;
			delta[8] = (iTex.z - avgTex.z) * factorTexture;

#ifdef FEATURE_MOTION
			delta[9] = (tex2D(g_texture_moving, x, y) - avgMovTex) * facMovTex;
#endif

			#pragma unroll
			for (int row = 0; row < nDimens; ++row) {				
				for (int col = row; col < nDimens; ++col) 
					A[row * nDimens + col] += delta[row] * delta[col];				
			}	

			const float4& varFeature = tex2D(g_var_feature, x, y);		
			errNorm += (factorDepth * factorDepth * varFeature.x + 
		                factorNormal * factorNormal * varFeature.y +  
					    factorTexture * factorTexture * varFeature.z);		
#ifdef FEATURE_MOTION
			errNorm += facMovTex * facMovTex * varFeature.w;
#endif
		}
	}	

	// Fill lower parts of A
	for (int row = 1; row < nDimens; ++row) {
		for (int col = 0; col < row; ++col) {
			A[row * nDimens + col] = A[col * nDimens + row];
		}
	}

	float V[nDimens * nDimens] = {0.f,};
	float S[nDimens];

	// Initialize V as an identity 
	for (int col = 0; col < nDimens; col++) 
		V[col * nDimens + col] = 1.f;
	
	int rank = svd(A, V, S, nDimens);	
	for (int col = 0; col < nDimens; ++col)
		S[col] = sqrtf(fabs(S[col]));

	// We approximate the Spectral norm (expensive) using the Frobenius norm (cheap) 
	errNorm = sqrtf(errNorm) / (sqrtf(rank) * 0.5f);	
	rank = 0;
	
	// 0.01 is for thin-SVD 
	float tol = 0.01f + 2.f * errNorm;																					

	// Update V = VS^(-2)Vt		
	// T-SVD
	const int nPix = xSize * ySize;
	const int cIdx = cy * xSize + cx;

	for (int col = 0; col < nDimens; ++col) {	
		float singular = sqrtf(fabs(S[col]));					

		if (singular > tol || col < 2) {		
			++rank;
			// Pre-Multiply normalization factor
			_transform[nPix * (0 * nDimens + col) + cIdx] = V[0 * nDimens + col] * invMaxHalfWindow;
			_transform[nPix * (1 * nDimens + col) + cIdx] = V[1 * nDimens + col] * invMaxHalfWindow;
			_transform[nPix * (2 * nDimens + col) + cIdx] = V[2 * nDimens + col] * factorDepth;
			_transform[nPix * (3 * nDimens + col) + cIdx] = V[3 * nDimens + col] * factorNormal;
			_transform[nPix * (4 * nDimens + col) + cIdx] = V[4 * nDimens + col] * factorNormal;
			_transform[nPix * (5 * nDimens + col) + cIdx] = V[5 * nDimens + col] * factorNormal;
			_transform[nPix * (6 * nDimens + col) + cIdx] = V[6 * nDimens + col] * factorTexture;		
			_transform[nPix * (7 * nDimens + col) + cIdx] = V[7 * nDimens + col] * factorTexture;		
			_transform[nPix * (8 * nDimens + col) + cIdx] = V[8 * nDimens + col] * factorTexture;	

#ifdef FEATURE_MOTION
			_transform[nPix * (9 * nDimens + col) + cIdx] = V[9 * nDimens + col] * facMovTex;	
#endif
		}
	}	
	_ranks[cIdx] = rank;		
}

__global__ void kernel_fit_anisotropic(float* _out, 
									   const float* _ranks, const float* _transform,
									   float* _hessians, 
									   int xSize, int ySize, const int MAX_HALF, const float* _bandwidth,
									   float* _bias_map, float* _var_map, bool isFinalFit) 
{
	const int cx = IMAD(blockDim.x, blockIdx.x, threadIdx.x);
    const int cy = IMAD(blockDim.y, blockIdx.y, threadIdx.y);	

	// this branch should be here after shared memory loading!!
	if (cx >= xSize || cy >= ySize)
		return;

	int2 startWindow = make_int2(MAX(0, cx - MAX_HALF), MAX(0, cy - MAX_HALF));
	int2 endWindow   = make_int2(MIN(xSize - 1, cx + MAX_HALF), MIN(ySize - 1, cy + MAX_HALF));	

	const int nPix = xSize * ySize;
	const int cIdx = cy * xSize + cx;
	const int localD = _ranks[cIdx];
	const int localP = localD + 1;	

	float transform[nDimens][nDimens];
	for (int row = 0; row < nDimens; ++row) {
		for (int col = 0; col < localD; ++col) {			
			transform[row][col] = _transform[xSize * ySize * (row * nDimens + col) + cIdx];
		}
	}

	//
	float band[nDimens];
	for (int i = 0; i < localD; ++i) {
		// feature bandwidth bi = (0, 2.5]
		float bi = 1.f / sqrtf(fabs(_hessians[nPix * i + cIdx]) + 0.16f);				
		band[i] = _bandwidth[cIdx] * bi;				
	}

	float iNewVecX[nDimens];	

	float4 cImg = tex2D(g_img, cx, cy);	
	float cGreyImg = Color2Grey(cImg);

	float4 cVarImg = tex2D(g_var_img, cx, cy);
	float4 cNorDepth = tex2D(g_normal_depth, cx, cy);
	float4 cTex = tex2D(g_texture, cx, cy);

#ifdef FEATURE_MOTION
	float cMovTex = tex2D(g_texture_moving, cx, cy);
#endif

	float __weights[MAX_HALF_WINDOW * 2 + 1][MAX_HALF_WINDOW * 2 + 1];
	float A[(nDimens + 1) * (nDimens + 1)] = {0.f,};		
	int nSample = 0;
	float sumWeight = 0.f;
	for (int y = startWindow.y; y <= endWindow.y; ++y) {		
		for (int x = startWindow.x; x <= endWindow.x; ++x) {
			const float4& iImg = tex2D(g_img, x, y);	

			// Confidence interval test: reject stastically distant samples
			float iGreyImg = Color2Grey(iImg);			
			if (fabs(cGreyImg - iGreyImg) > 3.f * (sqrtf(iImg.w) + sqrtf(cImg.w)) + 0.005f) {
				__weights[y - startWindow.y][x - startWindow.x] = 0.f;
				continue;		
			}			

#ifdef FEATURE_MOTION
			getTransfCoordExtended(iNewVecX, transform, cNorDepth, cTex, cMovTex, x, y, cx, cy, localD);				
#else
			getTransfCoord(iNewVecX, transform, cNorDepth, cTex, x, y, cx, cy, localD);	
#endif

			float weight = 1.f;
			for (int col = 0; col < localD; ++col) {
				float t = iNewVecX[col] / band[col];
				if (fabs(t) < 1.f)
					weight *= 0.75f * (1.f - t * t); 
				else {
					weight = 0.f;
					break;
				}
			}		

			if (weight > 0.0f) {				
#ifdef OUTLIER_TRICK
				// outlier	
				weight /= MAX(iImg.w, 1.0f);		
#endif
				A[0] += weight;
				for (int col = 1; col < localP; ++col)									
					A[col] += weight * iNewVecX[col - 1];					

				#pragma unroll
				for (int row = 1; row < localP; ++row) {
					for (int col = row; col < localP; ++col)												
						A[row*localP+col] += weight * iNewVecX[row - 1] * iNewVecX[col - 1];						
				}
				++nSample;
			}
			__weights[y - startWindow.y][x - startWindow.x] = weight;
			sumWeight += weight;
		}			
	}
	if (nSample <= 1) {	
		_out[cIdx * 3 + 0] = cImg.x;
		_out[cIdx * 3 + 1] = cImg.y;
		_out[cIdx * 3 + 2] = cImg.z;
		_bias_map[cIdx] = 0.f;				
		_var_map[cIdx] = cImg.w;		
		return;
	}

	for (int row = 0; row < localP; ++row)
		A[row * localP + row] += 0.0001f;

	float* L = A;
	cholesky(A, localP, L);

	// invL^T = save L^-1 to upper part in L
	float invL[(nDimens + 1) * (nDimens + 1)] = {0.f,};
	for (int j = localP - 1; j >= 0; --j) {
		invL[j * localP + j] = 1.f / L[j * localP + j];
		for (int k = j + 1; k < localP; ++k) {
			for (int i = j + 1; i < localP; ++i) {
				invL[k * localP + j] += invL[k * localP + i] * L[i * localP + j];
			}
		}
		for (int k = j + 1; k < localP; ++k) {
			invL[k * localP + j] = -1.f * invL[j * localP + j] * invL[k * localP + j];
		}
	}

	// First row in (XtWX)^-1
	float invA[nDimens + 1];
	for (int i = 0; i < localP; ++i) {
		float e = 0.f;
		for (int k = i; k < localP; ++k) {
			e += invL[k * localP] * invL[k * localP + i];
		}
		invA[i] = e;
	}

	// Final Fitting	
	float err_beta[3] = {0.f,};	
	float err_sum_l = 0.f;
	float beta[3] = {0.f,};

	float var = 0.f;
	float err_var = 0.f;

	for (int y = startWindow.y; y <= endWindow.y; ++y) {		
		for (int x = startWindow.x; x <= endWindow.x; ++x) {						
			float weight = __weights[y - startWindow.y][x - startWindow.x];

			if (weight > 0.f) {
#ifdef FEATURE_MOTION
				getTransfCoordExtended(iNewVecX, transform, cNorDepth, cTex, cMovTex, x, y, cx, cy, localD);					
#else
				getTransfCoord(iNewVecX, transform, cNorDepth, cTex, x, y, cx, cy, localD);					
#endif

				float l = invA[0];

				for (int f = 0; f < localD; ++f)											
					l += iNewVecX[f] * invA[f + 1];			

				l *= weight;

				const float4& iImg = tex2D(g_img, x, y);				

				beta[0] += l * iImg.x;
				beta[1] += l * iImg.y;
				beta[2] += l * iImg.z;

				var += l * l * iImg.w;

				//
				if (l > 0.f) {
					err_beta[0] += l * iImg.x;
					err_beta[1] += l * iImg.y;
					err_beta[2] += l * iImg.z;

					err_var += l * l * iImg.w;
					err_sum_l += l;
				}								
			}
		}
	}

	// exception handling
	if (beta[0] < 0.f || beta[1] < 0.f || beta[2] < 0.f) {	
		err_sum_l = max(err_sum_l, 0.001f);
		beta[0] = err_beta[0] / err_sum_l;
		beta[1] = err_beta[1] / err_sum_l;
		beta[2] = err_beta[2] / err_sum_l;

		var = err_var / (err_sum_l * err_sum_l);
	}

	_out[cIdx * 3 + 0] = max(0.f, beta[0]);
	_out[cIdx * 3 + 1] = max(0.f, beta[1]);
	_out[cIdx * 3 + 2] = max(0.f, beta[2]);	

	// Store bias and variance from this fitting!
	_bias_map[cIdx] = (beta[0] - cImg.x) * 0.33333f + 
		              (beta[1] - cImg.y) * 0.33333f + 
					  (beta[2] - cImg.z) * 0.33333f;	
	_var_map[cIdx] = var;		
}

extern "C"
void allocTextureMemory(int xSize, int ySize) 
{
	cudaChannelFormatDesc channelDesc = cudaCreateChannelDesc<float4>();	
	cudaChannelFormatDesc channelDescGrey = cudaCreateChannelDesc<float>();	

	checkCudaErrors(cudaMallocArray(&g_src_texture, &channelDesc, xSize, ySize));
	checkCudaErrors(cudaMallocArray(&g_src_img, &channelDesc, xSize, ySize));	
	checkCudaErrors(cudaMallocArray(&g_src_var_feature, &channelDesc, xSize, ySize));			
	checkCudaErrors(cudaMallocArray(&g_src_normal_depth, &channelDesc, xSize, ySize));	

#ifdef FEATURE_MOTION
	checkCudaErrors(cudaMallocArray(&g_src_spp_still, &channelDescGrey, xSize, ySize));	
	checkCudaErrors(cudaMallocArray(&g_src_texture_moving, &channelDescGrey, xSize, ySize));	
#endif

	checkCudaErrors(cudaGetLastError());
}

extern "C"
void initDeviceMemory(const float* _img, const float* _var_img, const float* _texture, const float* _var_texture, 
                      const float* _normal, const float* _var_normal, const float* _depth, const float* _var_depth, 
					  const float* _texture_moving, const float* _var_texture_moving,
					  const int* _mapSPP, int xSize, int ySize,
					  const int* _mapMovingSPP)
{
	int nPix = xSize * ySize;

	float4* _h_texture = (float4*)malloc(nPix * sizeof(float4));
	float4* _h_img = (float4*)malloc(nPix * sizeof(float4));
	float4* _h_normal_depth = (float4*)malloc(nPix * sizeof(float4));
	float4* _h_var_feature = (float4*)malloc(nPix * sizeof(float4));

#ifdef FEATURE_MOTION
	float* _h_texture_moving = (float*)malloc(nPix * sizeof(float));
	float* _h_spp_still = (float*)malloc(nPix * sizeof(float));
#endif

#pragma omp parallel for schedule(guided, 4)
	for (int i = 0; i < nPix; ++i) {
		float inv_spp_idx = 1.f / (float)_mapSPP[i];
		float mean_var_img = max(0.f, inv_spp_idx * (_var_img[i * 3 + 0] * 0.33333f +
											         _var_img[i * 3 + 1] * 0.33333f +
											         _var_img[i * 3 + 2] * 0.33333f));
		float mean_var_tex = max(0.f, inv_spp_idx * (_var_texture[i * 3 + 0] +
										       	     _var_texture[i * 3 + 1] +
											         _var_texture[i * 3 + 2]));
		float mean_var_nor = max(0.f, inv_spp_idx * (_var_normal[i * 3 + 0] + _var_normal[i * 3 + 1] + _var_normal[i * 3 + 2]));			               
		float mean_var_depth = max(0.f, inv_spp_idx * _var_depth[i]);

		_h_img[i] = make_float4(_img[i * 3 + 0], _img[i * 3 + 1], _img[i * 3 + 2], mean_var_img);		
		_h_texture[i] = make_float4(_texture[i * 3 + 0], _texture[i * 3 + 1], _texture[i * 3 + 2], 0.f);
		_h_normal_depth[i] = make_float4(_normal[i * 3 + 0], _normal[i * 3 + 1], _normal[i * 3 + 2], _depth[i]);


#ifndef FEATURE_MOTION
		_h_var_feature[i] = make_float4(mean_var_depth, mean_var_nor, mean_var_tex, 0.f);
#else
		float mean_var_moving = inv_spp_idx * (_var_texture_moving[i * 3 + 0] + 
			                                   _var_texture_moving[i * 3 + 1] +
											   _var_texture_moving[i * 3 + 2] );
		_h_var_feature[i] = make_float4(mean_var_depth, mean_var_nor, mean_var_tex, mean_var_moving);
		_h_texture_moving[i] = _texture_moving[i * 3 + 0] * 0.33333f + 
			                   _texture_moving[i * 3 + 1] * 0.33333f +
							   _texture_moving[i * 3 + 2] * 0.33333f;
		_h_spp_still[i] = _mapSPP[i] - _mapMovingSPP[i];
#endif
	}	    

	cudaChannelFormatDesc channelDesc = cudaCreateChannelDesc<float4>();	

	checkCudaErrors(cudaMemcpyToArray(g_src_texture, 0, 0,		_h_texture,      nPix * sizeof(float4), cudaMemcpyHostToDevice));	
	checkCudaErrors(cudaMemcpyToArray(g_src_img, 0, 0,          _h_img,          nPix * sizeof(float4), cudaMemcpyHostToDevice));	
	checkCudaErrors(cudaMemcpyToArray(g_src_var_feature, 0, 0,  _h_var_feature,  nPix * sizeof(float4), cudaMemcpyHostToDevice));	
	checkCudaErrors(cudaMemcpyToArray(g_src_normal_depth, 0, 0, _h_normal_depth, nPix * sizeof(float4), cudaMemcpyHostToDevice));	


	checkCudaErrors(cudaBindTextureToArray(&g_texture, g_src_texture, &channelDesc));
	checkCudaErrors(cudaBindTextureToArray(&g_img, g_src_img, &channelDesc));	
	checkCudaErrors(cudaBindTextureToArray(&g_var_feature, g_src_var_feature, &channelDesc));	
	checkCudaErrors(cudaBindTextureToArray(&g_normal_depth, g_src_normal_depth, &channelDesc));

#ifdef FEATURE_MOTION
	cudaChannelFormatDesc channelDescGrey = cudaCreateChannelDesc<float>();

	checkCudaErrors(cudaMemcpyToArray(g_src_spp_still, 0, 0, _h_spp_still, nPix * sizeof(float), cudaMemcpyHostToDevice));	
	checkCudaErrors(cudaMemcpyToArray(g_src_texture_moving, 0, 0, _h_texture_moving, nPix * sizeof(float), cudaMemcpyHostToDevice));	

	checkCudaErrors(cudaBindTextureToArray(&g_spp_still, g_src_spp_still, &channelDescGrey));
	checkCudaErrors(cudaBindTextureToArray(&g_texture_moving, g_src_texture_moving, &channelDescGrey));
#endif

	checkCudaErrors(cudaGetLastError());	

	free(_h_texture);
	free(_h_normal_depth);
	free(_h_img);		
	free(_h_var_feature);

#ifdef FEATURE_MOTION
	free(_h_texture_moving);
	free(_h_spp_still);
#endif
}

extern "C"
void freeDeviceMemory()
{
	checkCudaErrors(cudaFreeArray(g_src_texture));	
	checkCudaErrors(cudaFreeArray(g_src_img));	
	checkCudaErrors(cudaFreeArray(g_src_var_feature));	
	checkCudaErrors(cudaFreeArray(g_src_normal_depth));	

#ifdef FEATURE_MOTION
	checkCudaErrors(cudaFreeArray(g_src_spp_still));	
	checkCudaErrors(cudaFreeArray(g_src_texture_moving));	
#endif
	checkCudaErrors(cudaGetLastError());	
}

extern "C"
void localFitShared(float* _dbgImg,
					int xSize, int ySize, const int MAX_HALF,
					float* _dbg_hessians,
					float** _fit_map, float** _var_map, float** _bias_map, float* _ranks,
					float** _width_guess,
					LWR_cuda_mem& gloMemory,
					const int* _spp) 
{
	cudaDeviceSetCacheConfig(cudaFuncCachePreferL1);

	int nPix = xSize * ySize;
	dim3 threads(BLOCKDIM, BLOCKDIM);
    dim3 grid(iDivUp(xSize, BLOCKDIM), iDivUp(ySize, BLOCKDIM));

	// init cudaPrintf
	cudaEvent_t start, stop;	
	cudaEventCreate(&start);
	cudaEventCreate(&stop);
	float time;

	///////////////////////////////////////
	cudaEventRecord(start, 0);
	kernel_compute_transform<<<grid, threads>>>(gloMemory._d_out, gloMemory._d_transform, gloMemory._d_ranks,
												xSize, ySize, MAX_HALF, gloMemory._d_bandwidth);
	checkCudaErrors(cudaDeviceSynchronize());
	cudaEventRecord(stop, 0);
	cudaEventSynchronize(stop);		
	cudaEventElapsedTime(&time, start, stop);
	//
	printf("Time for Computing Transform Matrix: %f ms\n", time);	
	checkCudaErrors(cudaMemcpy(_ranks, gloMemory._d_ranks, nPix * sizeof(float), cudaMemcpyDeviceToHost));	

	/////////////////////	
	cudaEventRecord(start, 0);	
	const float band_derivatives = 1.f;
	kernel_compute_derivatives_approx<<<grid, threads>>>(gloMemory._d_out,
													     gloMemory._d_hessians, 
														 gloMemory._d_ranks, gloMemory._d_transform, 
														 xSize, ySize, MAX_HALF, band_derivatives);		
	checkCudaErrors(cudaDeviceSynchronize());
	cudaEventRecord(stop, 0);
	cudaEventSynchronize(stop);		
	cudaEventElapsedTime(&time, start, stop);
	printf("Time for derivatives: %f ms\n", time);
	
	for (int iter = 0; iter < NUM_TEST; ++iter) {		
		cudaEventRecord(start, 0);	

		checkCudaErrors(cudaMemcpy(gloMemory._d_bandwidth, _width_guess[iter], nPix * sizeof(float), cudaMemcpyHostToDevice));
		//
		kernel_fit_anisotropic<<<grid, threads>>>(gloMemory._d_out, 
												  gloMemory._d_ranks, gloMemory._d_transform,						
												  gloMemory._d_hessians, xSize, ySize, MAX_HALF, gloMemory._d_bandwidth,
												  gloMemory._d_bias_map, gloMemory._d_var_map, false);				
		checkCudaErrors(cudaDeviceSynchronize());
		cudaEventRecord(stop, 0);
		cudaEventSynchronize(stop);		
		cudaEventElapsedTime(&time, start, stop);
		printf("Time for the anisotropic kernel: %f ms\n", time);

		checkCudaErrors(cudaMemcpy(_var_map[iter], gloMemory._d_var_map, nPix * sizeof(float), cudaMemcpyDeviceToHost));
		checkCudaErrors(cudaMemcpy(_bias_map[iter], gloMemory._d_bias_map, nPix * sizeof(float), cudaMemcpyDeviceToHost));
	}
	/////////////////////////////////
	
	checkCudaErrors(cudaGetLastError());	
}

extern "C"
void localFitSharedFinal(float* _out, 
						 int xSize, int ySize, const int MAX_HALF,	
						 float* _bandwidth,
						 LWR_cuda_mem& gloMemory,
						 float* _opt_var) 
{
	int nPix = xSize * ySize;

	dim3 threads(BLOCKDIM, BLOCKDIM);
    dim3 grid(iDivUp(xSize, BLOCKDIM), iDivUp(ySize, BLOCKDIM));

	// init cudaPrintf
	cudaEvent_t start, stop;	
	cudaEventCreate(&start);
	cudaEventCreate(&stop);
	float time;

	checkCudaErrors(cudaMemcpy(gloMemory._d_bandwidth, _bandwidth, nPix * sizeof(float), cudaMemcpyHostToDevice));

	cudaEventRecord(start, 0);			

	kernel_fit_anisotropic<<<grid, threads>>>(gloMemory._d_out, 
											  gloMemory._d_ranks, gloMemory._d_transform, 
											  gloMemory._d_hessians, xSize, ySize, MAX_HALF, gloMemory._d_bandwidth,
											  gloMemory._d_bias_map, gloMemory._d_var_map, true);
	checkCudaErrors(cudaDeviceSynchronize());
	cudaEventRecord(stop, 0);
	cudaEventSynchronize(stop);		
	cudaEventElapsedTime(&time, start, stop);
	printf("Time for the adaptive kernel: %f ms\n", time);

	///////////////////////////////////////////////////////////////////////////////
	// data transfer from device to host
	checkCudaErrors(cudaMemcpy(_out, gloMemory._d_out, nPix * 3 * sizeof(float), cudaMemcpyDeviceToHost));

	// free texture memory
	checkCudaErrors(cudaUnbindTexture(&g_texture));	
	checkCudaErrors(cudaUnbindTexture(&g_img));	
	checkCudaErrors(cudaUnbindTexture(&g_var_feature));	
	checkCudaErrors(cudaUnbindTexture(&g_normal_depth));	

#ifdef FEATURE_MOTION
	checkCudaErrors(cudaUnbindTexture(&g_spp_still));	
	checkCudaErrors(cudaUnbindTexture(&g_texture_moving));	
#endif

	checkCudaErrors(cudaGetLastError());	
}

extern "C"
void localGuassian2(float* _img, float* _d_in_mem, float* _d_out_mem, int xSize, int ySize, float h, bool isColor, bool isIntegral) 
{
	int nPix = xSize * ySize;
	size_t memSize = nPix * sizeof(float);
	if (isColor)
		memSize *= 3;
	/////////////////////////////////////////////////////////////////////////////////

	/////////////////////////////////////////////////////////////////////////////////
	// data transfer from host to device	
	checkCudaErrors(cudaMemcpy(_d_in_mem, _img, memSize, cudaMemcpyHostToDevice));

	///////////////////////////////////////////////////////////////////////////////
	// launch CUDA kernel
	dim3 threads(BLOCKDIM, BLOCKDIM);
    dim3 grid(iDivUp(xSize, BLOCKDIM), iDivUp(ySize, BLOCKDIM));

	gaussian_fit<<<grid, threads>>>(_d_in_mem, _d_out_mem, h, isColor, xSize, ySize, isIntegral);
	checkCudaErrors(cudaDeviceSynchronize());			

	checkCudaErrors(cudaMemcpy(_img, _d_out_mem, memSize, cudaMemcpyDeviceToHost));
	checkCudaErrors(cudaGetLastError());	
	/////////////////////////////////////////////////////////////////////////////////
}

extern "C"
void localGuassianFillHoles(float* _img, const int* _still_spp, int xSize, int ySize, int halfWidth, bool isColor) 
{
	int nPix = xSize * ySize;
	size_t memSize = nPix * sizeof(float);
	if (isColor)
		memSize *= 3;

	/////////////////////////////////////////////////////////////////////////////////
	// allocate the device memory	
	float *_d_in = NULL;
	float *_d_out = NULL;
	int *_d_spp = NULL;

	/////////////////////////////////////////////////////////////////
	// device memory allocation	
	checkCudaErrors(cudaMalloc((void **)&_d_in, memSize));
	checkCudaErrors(cudaMalloc((void **)&_d_out, memSize));
	checkCudaErrors(cudaMalloc((void **)&_d_spp, nPix * sizeof(int)));
	/////////////////////////////////////////////////////////////////////////////////

	/////////////////////////////////////////////////////////////////////////////////
	// data transfer from host to device	
	checkCudaErrors(cudaMemcpy(_d_in, _img, memSize, cudaMemcpyHostToDevice));
	checkCudaErrors(cudaMemcpy(_d_spp, _still_spp, nPix * sizeof(int), cudaMemcpyHostToDevice));

	///////////////////////////////////////////////////////////////////////////////
	// launch CUDA kernel
	dim3 threads(BLOCKDIM, BLOCKDIM);
    dim3 grid(iDivUp(xSize, BLOCKDIM), iDivUp(ySize, BLOCKDIM));

	gaussian_fill_hole<<<grid, threads>>>(_d_in, _d_spp, _d_out, halfWidth, isColor, xSize, ySize);
	checkCudaErrors(cudaDeviceSynchronize());
	checkCudaErrors(cudaGetLastError());			

	///////////////////////////////////////////////////////////////////////////////
	// data transfer from device to host
	checkCudaErrors(cudaMemcpy(_img, _d_out, memSize, cudaMemcpyDeviceToHost));

	/////////////////////////////////////////////////////////////////////////////////
	// free device memory	
	checkCudaErrors(cudaFree(_d_in));
	checkCudaErrors(cudaFree(_d_out));	
	checkCudaErrors(cudaFree(_d_spp));		
	/////////////////////////////////////////////////////////////////////////////////
}

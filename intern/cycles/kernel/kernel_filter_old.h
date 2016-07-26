/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

CCL_NAMESPACE_BEGIN

/* Performs Singular Value Decomposition on A, fills V and S2 (containing the squares of the singular values) and returns the estimated rank */
ccl_device int orig_svd(float *A, float *V, float *S2, int n)
{
	int  i, j, k, EstColRank = n, RotCount = n, SweepCount = 0;
	int slimit = 8;
	float eps = 1e-8f;
	float e2 = 10.f * n * eps * eps;
	float tol = 0.1f * eps;
	float vt, p, x0, y0, q, r, c0, s0, d1, d2;

	for(int r = 0; r < n; r++)
		for(int c = 0; c < n; c++)
			V[r*n+c] = (c == r)? 1.0f: 0.0f;

	while (RotCount != 0 && SweepCount++ <= slimit) {
		RotCount = EstColRank * (EstColRank - 1) / 2;

		for (j = 0; j < EstColRank-1; ++j) {
			for (k = j+1; k < EstColRank; ++k) {
				p = q = r = 0.0;

				for (i = 0; i < n; ++i) {
					x0 = A[i * n + j];
					y0 = A[i * n + k];
					p += x0 * y0;
					q += x0 * x0;
					r += y0 * y0;
				}

				S2[j] = q;
				S2[k] = r;

				if (q >= r) {
					if (q <= e2 * S2[0] || fabsf(p) <= tol * q)
						RotCount--;
					else {
						p /= q;
						r = 1.f - r/q;
						vt = sqrtf(4.0f * p * p + r * r);
						c0 = sqrtf(0.5f * (1.f + r / vt));
						s0 = p / (vt*c0);

						// Rotation
						for (i = 0; i < n; ++i) {
							d1 = A[i * n + j];
							d2 = A[i * n + k];
							A[i * n + j] = d1*c0+d2*s0;
							A[i * n + k] = -d1*s0+d2*c0;
						}
						for (i = 0; i < n; ++i) {
							d1 = V[i * n + j];
							d2 = V[i * n + k];
							V[i * n + j] = d1 * c0 + d2 * s0;
							V[i * n + k] = -d1 * s0 + d2 * c0;
						}
					}
				} else {
					p /= r;
					q = q / r - 1.f;
					vt = sqrtf(4.f * p * p + q * q);
					s0 = sqrtf(0.5f * (1.f - q / vt));
					if (p < 0.f)
						s0 = -s0;
					c0 = p / (vt * s0);

					// Rotation
					for (i = 0; i < n; ++i) {
						d1 = A[i * n + j];
						d2 = A[i * n + k];
						A[i * n + j] = d1 * c0 + d2 * s0;
						A[i * n + k] = -d1 * s0 + d2 * c0;
					}
					for (i = 0; i < n; ++i) {
						d1 = V[i * n + j];
						d2 = V[i * n + k];
						V[i * n + j] = d1 * c0 + d2 * s0;
						V[i * n + k] = -d1 * s0 + d2 * c0;
					}
				}
			}
		}
		while (EstColRank >= 3 && S2[EstColRank-1] <= S2[0] * tol + tol * tol)
			EstColRank--;
	}
	return EstColRank;
}

ccl_device void orig_cholesky(float *A, int n, float *L)
{
	for (int i = 0; i < n; ++i) {
		for (int j = 0; j <= i; ++j) {
			float s = 0.0f;
			for (int k = 0; k < j; ++k)
				s += L[i * n + k] * L[j * n + k];
			L[i * n + j] = (i == j) ? sqrtf(A[i * n + i] - s) : (1.0f / L[j * n + j] * (A[j * n + i] - s));
		}
	}
}

#define Buf_F(x, y, o) (buffers[((y) * w + (x)) * kernel_data.film.pass_stride + (o)])
#define Buf_F3(x, y, o) *((float3*) (buffers + ((y) * w + (x)) * kernel_data.film.pass_stride + (o)))
#define Buf_F4(x, y, o) *((float4*) (buffers + ((y) * w + (x)) * kernel_data.film.pass_stride + (o)))

ccl_device_inline void filter_get_color_passes(KernelGlobals *kg, int &m_C, int &v_C)
{
	m_C = kernel_data.film.pass_denoising + 20;
	v_C = kernel_data.film.pass_denoising + 23;
}

ccl_device void kernel_filter1_pixel(KernelGlobals *kg, float *buffers, int x, int y, int w, int h, int samples, int halfWindow, float bandwidthFactor, FilterStorage* storage)
{
	float invS = 1.0f / samples;
	float invSv = 1.0f / (samples - 1);

	int2 lo = make_int2(max(x - halfWindow, 0), max(y - halfWindow, 0));
	int2 hi = make_int2(min(x + halfWindow, w-1), min(y + halfWindow, h-1));
	int num = (hi.x - lo.x + 1) * (hi.y - lo.y + 1);

	int m_D = kernel_data.film.pass_denoising + 12, v_D = kernel_data.film.pass_denoising + 13,
	    m_N = kernel_data.film.pass_denoising, m_T = kernel_data.film.pass_denoising + 6,
	    v_N = kernel_data.film.pass_denoising + 3, v_T = kernel_data.film.pass_denoising + 9,
	    m_C, v_C;
	filter_get_color_passes(kg, m_C, v_C);

	float3 meanT = make_float3(0.0f, 0.0f, 0.0f);
	float3 meanN = make_float3(0.0f, 0.0f, 0.0f);
	float meanD = 0.0f;

	for(int py = lo.y; py <= hi.y; py++) {
		for(int px = lo.x; px <= hi.x; px++) {
			meanT += Buf_F3(px, py, m_T) * invS;
			meanN += Buf_F3(px, py, m_N) * invS;
			meanD += Buf_F (px, py, m_D) * invS;
		}
	}
	meanT /= num;
	meanN /= num;
	meanD /= num;

#ifdef WITH_CYCLES_DEBUG_FILTER
	storage->means[0] = meanN.x;
	storage->means[1] = meanN.y;
	storage->means[2] = meanN.z;
	storage->means[3] = meanT.x;
	storage->means[4] = meanT.y;
	storage->means[5] = meanT.z;
	storage->means[6] = meanD;
#endif

	float delta[9], transform[81], norm;
	int rank;
	/* Generate transform */
	{
		float top_nD = 0.0f;
		float3 top_nN = make_float3(0.0f, 0.0f, 0.0f);
		float3 top_nT = make_float3(0.0f, 0.0f, 0.0f);
		for(int py = lo.y; py <= hi.y; py++) {
			for(int px = lo.x; px <= hi.x; px++) {
/* Instead of going for the highest value, which might be a firefly, the code uses the 5th highest value
 * (kind of like a median filter, but we want to keep near the maximum for non-firefly pixels).
 * To do that, the 5 highest values are kept around and every value is insertion-sorted into it if it belongs there.
 * In the end, we can then just pick the smallest of the 5 values and have our percentile filter in linear time. */
/*#define UPDATE_TOP(top, v, len) \
	if(top[0] < v) { \
		top[0] = v; \
		for(int i = 0; i < (len)-1; i++) { \
			if(top[i+1] >= top[i]) break; \
			float t = top[i+1]; \
			top[i+1] = top[i]; \
			top[i] = t; \
		} \
	}*/
#define UPDATE_TOP(top, v, len) if(top < v) top = v;
#define UPDATE_TOP3(top, v, len) if(top.x < fabsf(v.x)) top.x = fabsf(v.x); if(top.y < fabsf(v.y)) top.y = fabsf(v.y); if(top.z < fabsf(v.z)) top.z = fabsf(v.z);
				float nD = fabsf(Buf_F(px, py, m_D) * invS - meanD);
				UPDATE_TOP(top_nD, nD, 5)
				float3 nN = (Buf_F3(px, py, m_N) * invS - meanN);
				UPDATE_TOP3(top_nN, nN, 5)
				float3 nT = (Buf_F3(px, py, m_T) * invS - meanT);
				UPDATE_TOP3(top_nT, nT, 5)
#undef UPDATE_TOP
			}
		}

		float nD = 1.0f / max(top_nD, 0.01f);
		float3 nN = 1.0f / max(top_nN, make_float3(0.01f, 0.01f, 0.01f));
		float3 nT = 1.0f / max(top_nT, make_float3(0.01f, 0.01f, 0.01f));

		norm = 0.0f;
		for(int i = 0; i < 81; i++)
			transform[i] = 0.0f;
		for(int py = lo.y; py <= hi.y; py++) {
			for(int px = lo.x; px <= hi.x; px++) {
				delta[0] = ((float) px - x) / halfWindow;
				delta[1] = ((float) py - y) / halfWindow;
				delta[2] = (Buf_F(px, py, m_D) * invS - meanD) * nD;
				float3 dN = (Buf_F3(px, py, m_N) * invS - meanN) * nN;
				delta[3] = dN.x;
				delta[4] = dN.y;
				delta[5] = dN.z;
				float3 dT = (Buf_F3(px, py, m_T) * invS - meanT) * nT;
				delta[6] = dT.x;
				delta[7] = dT.y;
				delta[8] = dT.z;

				for(int r = 0; r < 9; r++)
					for(int c = r; c < 9; c++)
						transform[9*r+c] += delta[r]*delta[c];

				//norm += 10.0f * nD * nD * Buf_F(px, py, v_D) * invSv * invS;
				norm += nD*nD*Buf_F(px, py, v_D) * invSv * invS + 3.0f * average(nN * nN * Buf_F3(px, py, v_N) * invSv * invS) + 3.0f * average(nT * nT * Buf_F3(px, py, v_T) * invSv * invS);
			}
		}

		/* Here, transform is self-adjoint (TODO term symmetric?) by construction, so one half can be copied from the other one */
		for(int r = 1; r < 9; r++)
			for(int c = 0; c < r; c++)
				transform[9*r+c] = transform[9*c+r];

		float V[81], S[9];
		rank = orig_svd(transform, V, S, 9);

		for(int i = 0; i < 9; i++) {
			S[i] = sqrtf(fabsf(S[i]));
#ifdef WITH_CYCLES_DEBUG_FILTER
			storage->singular[i] = S[i];
#endif
		}

#ifdef WITH_CYCLES_DEBUG_FILTER
		storage->scales[0] = nN.x;
		storage->scales[1] = nN.y;
		storage->scales[2] = nN.z;
		storage->scales[3] = nT.x;
		storage->scales[4] = nT.y;
		storage->scales[5] = nT.z;
		storage->scales[6] = nD;
#endif

		float threshold = 0.01f + 2.0f * (sqrtf(norm) / (sqrtf(rank) * 0.5f));
		rank = 0;

#ifdef WITH_CYCLES_DEBUG_FILTER
		storage->singular_threshold = threshold;
		storage->feature_matrix_norm = norm;
#endif

		/* Truncate matrix to reduce the rank */
		for(int c = 0; c < 9; c++) {
			float singular = S[c]; //TODO 2x sqrtf?
			if((singular > threshold) || (c < 2)) { /* Image position is always used */
				transform[     c] = V[     c] / halfWindow;
				transform[ 9 + c] = V[ 9 + c] / halfWindow;
				transform[18 + c] = V[18 + c] * nD;
				transform[27 + c] = V[27 + c] * nN.x;
				transform[36 + c] = V[36 + c] * nN.y;
				transform[45 + c] = V[45 + c] * nN.z;
				transform[54 + c] = V[54 + c] * nT.x;
				transform[63 + c] = V[63 + c] * nT.y;
				transform[72 + c] = V[72 + c] * nT.z;
				rank++;
			}
		}
	}

	float bi[9];
	/* Approximate bandwidths */
	{
		const int size = 2*rank+1;
		float z[18]; /* Only 0 to rank-1 gets used (and rank to 2*rank-1 for the squared values) */
		float A[19*19];
		float3 XtB[19];

		meanD = Buf_F (x, y, m_D) * invS;
		meanN = Buf_F3(x, y, m_N) * invS;
		meanT = Buf_F3(x, y, m_T) * invS;

		for(int i = 0; i < size*size; i++)
			A[i] = 0.0f;
		for(int i = 0; i < size; i++)
			XtB[i] = make_float3(0.0f, 0.0f, 0.0f);
		for(int py = lo.y; py <= hi.y; py++) {
			for(int px = lo.x; px <= hi.x; px++) {
				float dD = Buf_F(px, py, m_D) * invS - meanD;
				float3 dN = Buf_F3(px, py, m_N) * invS - meanN;
				float3 dT = Buf_F3(px, py, m_T) * invS - meanT;

				float weight = 1.0f;
				for(int col = 0; col < rank; col++) {
					z[col] = transform[     col] * (px - x) + transform[ 9 + col] * (py - y)
					       + transform[18 + col] * dD       + transform[27 + col] * dN.x
					       + transform[36 + col] * dN.y     + transform[45 + col] * dN.z
					       + transform[54 + col] * dT.x     + transform[63 + col] * dT.y
					       + transform[72 + col] * dT.z;
					if(fabsf(z[col]) < 1.0f)
						weight *= 0.75f * (1.0f - z[col]*z[col]);
					else {
						weight = 0.0f;
						break;
					}
				}

				if(weight > 0.0f) {
					weight /= max(1.0f, average(Buf_F3(px, py, v_C)) * invS*invSv);
					for(int i = 0; i < rank; i++)
						z[rank+i] = z[i]*z[i];
					A[0] += weight;
					XtB[0] += Buf_F3(px, py, m_C) * invS * weight;
					for(int c = 1; c < size; c++) {
						float lweight = weight * z[c-1];
						A[c] += lweight;
						XtB[c] += Buf_F3(px, py, m_C) * invS * lweight;
					}
					for(int r = 1; r < size; r++)
						for(int c = r; c < size; c++)
							A[r*size + c] += weight * z[r-1] * z[c-1];
				}
			}
		}

		for(int i = 0; i < size; i++)
			A[i*size+i] += 0.001f;

		orig_cholesky(A, size, A);

		XtB[0] /= A[0];
		for(int i = 1; i < size; i++) {
			float3 s = make_float3(0.0f, 0.0f, 0.0f);
			for(int j = 0; j < i; j++)
				s += A[i*size + j] * XtB[j];
			XtB[i] = (XtB[i] - s) / A[i*size + i];
		}
		XtB[size-1] /= A[(size-1)*size + (size-1)];
		for(int i = size-2; i >= 0; i--) {
			float3 s = make_float3(0.0f, 0.0f, 0.0f);
			for(int j = size-1; j > i; j--)
				s += A[j*size + i] * XtB[j];
			XtB[i] = (XtB[i] - s) / A[i*size + i];
		}

		for(int i = 0; i < 9; i++) //TODO < rank enough? Why +0.16?
		{
			bi[i] = bandwidthFactor / sqrtf(fabsf(2.0f * average(fabs(XtB[i + rank + 1]))) + 0.16f);
		}
	}

	double bias_XtX[4], bias_XtB[2];
	double  var_XtX[4],  var_XtB[2];
	bias_XtX[0] = bias_XtX[1] = bias_XtX[2] = bias_XtX[3] = bias_XtB[0] = bias_XtB[1] = 0.0;
	 var_XtX[0] =  var_XtX[1] =  var_XtX[2] =  var_XtX[3] =  var_XtB[0] =  var_XtB[1] = 0.0;

	const float candidate_bw[6] = {0.05f, 0.1f, 0.25f, 0.5f, 1.0f, 2.0f};
	for(int g = 0; g < 6; g++) {
		float A[100], z[9], invL[100], invA[10];
		for(int i = 0; i < 100; i++)
			A[i] = 0.0f;

		float g_w = candidate_bw[g];
		float bandwidth[9];
		for(int i = 0; i < rank; i++)
			bandwidth[i] = g_w * bi[i];

		float cCm = average(Buf_F3(x, y, m_C))*invS;
		float cCs = sqrtf(average(Buf_F3(x, y, v_C))*invS*invSv);

		for(int py = lo.y; py <= hi.y; py++) {
			for(int px = lo.x; px <= hi.x; px++) {
				if(fabsf(average(Buf_F3(px, py, m_C))*invS - cCm) > 3.0f * (cCs + sqrtf(average(Buf_F3(px, py, v_C))*invS*invSv)) + 0.005f)
					continue;

				float dD = Buf_F(px, py, m_D) * invS - meanD;
				float3 dN = Buf_F3(px, py, m_N) * invS - meanN;
				float3 dT = Buf_F3(px, py, m_T) * invS - meanT;

				float weight = 1.0f;
				for(int col = 0; col < rank; col++) {
					z[col] = transform[     col] * (px - x) + transform[ 9 + col] * (py - y)
					       + transform[18 + col] * dD       + transform[27 + col] * dN.x
					       + transform[36 + col] * dN.y     + transform[45 + col] * dN.z
					       + transform[54 + col] * dT.x     + transform[63 + col] * dT.y
					       + transform[72 + col] * dT.z;
					float t = z[col] / bandwidth[col];
					if(fabsf(t) < 1.0f)
						weight *= 0.75f * (1.0f - t*t);
					else {
						weight = 0.0f;
						break;
					}
				}

				if(weight > 0.0f) {
					weight /= max(1.0f, average(Buf_F3(px, py, v_C)) * invS*invSv);
					A[0] += weight;
					for(int c = 1; c < rank+1; c++)
						A[c] += weight*z[c-1];
					for(int r = 1; r < rank+1; r++)
						for(int c = r; c < rank+1; c++)
							A[r*(rank+1)+c] += weight*z[c-1]*z[r-1];
				}
			}
		}
		for(int i = 0; i < rank+1; i++)
			A[i*(rank+1)+i] += 0.0001f;

		orig_cholesky(A, rank+1, A);

		for(int i = 0; i < 100; i++)
			invL[i] = 0.0f;

		for(int j = rank; j >= 0; j--) {
			invL[j*(rank+1)+j] = 1.0f / A[j*(rank+1)+j];
			for(int k = j+1; k < rank+1; k++)
				for(int i = j+1; i < rank+1; i++)
					invL[k*(rank+1)+j] += invL[k*(rank+1)+i] * A[i*(rank+1)+j];
			for(int k = j+1; k < rank+1; k++)
				invL[k*(rank+1)+j] *= -invL[j*(rank+1)+j];
		}

		for(int i = 0; i < rank+1; i++) {
			invA[i] = 0.0f;
			for(int k = i; k < rank+1; k++)
				invA[i] += invL[k*(rank+1)] * invL[k*(rank+1)+i];
		}

		float3 beta, err_beta;
		beta = err_beta = make_float3(0.0f, 0.0f, 0.0f);
		float err_sum_l, var, err_var;
		err_sum_l = var = err_var = 0.0f;

		for(int py = lo.y; py <= hi.y; py++) {
			for(int px = lo.x; px <= hi.x; px++) {
				if(fabsf(average(Buf_F3(px, py, m_C))*invS - cCm) > 3.0f * (cCs + sqrtf(average(Buf_F3(px, py, v_C))*invS*invSv)) + 0.005f)
					continue;

				float dD = Buf_F(px, py, m_D) * invS - meanD;
				float3 dN = Buf_F3(px, py, m_N) * invS - meanN;
				float3 dT = Buf_F3(px, py, m_T) * invS - meanT;

				float weight = 1.0f;
				for(int col = 0; col < rank; col++) {
					z[col] = transform[     col] * (px - x) + transform[ 9 + col] * (py - y)
					       + transform[18 + col] * dD       + transform[27 + col] * dN.x
					       + transform[36 + col] * dN.y     + transform[45 + col] * dN.z
					       + transform[54 + col] * dT.x     + transform[63 + col] * dT.y
					       + transform[72 + col] * dT.z;
					float t = z[col] / bandwidth[col];
					if(fabsf(t) < 1.0f)
						weight *= 0.75f * (1.0f - t*t);
					else {
						weight = 0.0f;
						break;
					}
				}

				if(weight > 0.0f) {
					weight /= max(1.0f, average(Buf_F3(px, py, v_C)) * invS*invSv);

					float l = invA[0];
					for(int f = 0; f < rank; f++)
						l += z[f]*invA[f+1];
					l *= weight;

					beta += l * Buf_F3(px, py, m_C)*invS;
					var += l*l * max(average(Buf_F3(px, py, v_C))*invSv*invS, 0.0f);

					if(l > 0.0f) {
						err_beta += l * Buf_F3(px, py, m_C)*invS;
						err_var += l*l * max(average(Buf_F3(px, py, v_C))*invSv*invS, 0.0f);
						err_sum_l += l;
					}
				}
			}
		}

		if(beta.x < 0.0f || beta.y < 0.0f || beta.z < 0.0f) {
			err_sum_l = max(err_sum_l, 0.001f);
			beta = err_beta / err_sum_l;
			var = err_var / (err_sum_l*err_sum_l);
		}

		double h2 = g_w*g_w;
		double bias = average(beta - (Buf_F3(x, y, m_C)*invS));
		double i_h_r = pow(g_w, -rank);
		double svar = max(samples*var, 0.0f);

		bias_XtX[0] += 1.0;
		bias_XtX[1] += h2;
		bias_XtX[2] += h2;
		bias_XtX[3] += h2*h2;
		bias_XtB[0] += bias;
		bias_XtB[1] += h2*bias;

		var_XtX[0] += 1.0;
		var_XtX[1] += i_h_r;
		var_XtX[2] += i_h_r;
		var_XtX[3] += i_h_r*i_h_r;
		var_XtB[0] += svar;
		var_XtB[1] += svar*i_h_r;
	}

	double coef_bias[2], coef_var[2];

	double i_det = 1.0 / (bias_XtX[0]*bias_XtX[3] - bias_XtX[1]*bias_XtX[2] + 0.0001);
	coef_bias[0] = i_det*bias_XtX[3]*bias_XtB[0] - i_det*bias_XtX[1]*bias_XtB[1];
	coef_bias[1] = i_det*bias_XtX[0]*bias_XtB[1] - i_det*bias_XtX[2]*bias_XtB[0];

	i_det = 1.0 / (var_XtX[0]*var_XtX[3] - var_XtX[1]*var_XtX[2] + 0.0001);
	coef_var[0] = i_det*var_XtX[3]*var_XtB[0] - i_det*var_XtX[1]*var_XtB[1];
	coef_var[1] = i_det*var_XtX[0]*var_XtB[1] - i_det*var_XtX[2]*var_XtB[0];
	if(coef_var[1] < 0.0) {
		coef_var[0] = 0.0f;
		coef_var[1] = (coef_var[1] < 0.0)? -coef_var[1]: coef_var[1];
	}

	double h_opt = pow((rank * coef_var[1]) / (4.0 * coef_bias[1] * coef_bias[1] * samples), 1.0 / (4 + rank));
	if(h_opt < (double) candidate_bw[0]) h_opt = (double) candidate_bw[0];
	if(h_opt > (double) candidate_bw[5]) h_opt = (double) candidate_bw[5];

	storage->rank = rank;
	storage->global_bandwidth = (float) h_opt;
	for(int i = 0; i < 9; i++)
		storage->bandwidth[i] = (i < rank)? bi[i]: 0.0f;
	for(int i = 0; i < 81; i++)
		storage->transform[i] = transform[i];
}

ccl_device void kernel_filter2_pixel(KernelGlobals *kg, float *buffers, int x, int y, int w, int h, int samples, int halfWindow, float bandwidthFactor, FilterStorage *storage, int4 tile)
{
	float invS = 1.0f / samples;
	float invSv = 1.0f / (samples - 1);

	int2 lo = make_int2(max(x - halfWindow, 0), max(y - halfWindow, 0));
	int2 hi = make_int2(min(x + halfWindow, w-1), min(y + halfWindow, h-1));
	int num = (hi.x - lo.x + 1) * (hi.y - lo.y + 1);

	int m_D = kernel_data.film.pass_denoising + 12, m_N = kernel_data.film.pass_denoising, m_T = kernel_data.film.pass_denoising + 6,
	    m_C, v_C;
	filter_get_color_passes(kg, m_C, v_C);

	//Load storage data
	float *transform = storage->transform;
	int rank = storage->rank;

	float meanD = Buf_F (x, y, m_D) * invS;
	float3 meanN = Buf_F3(x, y, m_N) * invS;
	float3 meanT = Buf_F3(x, y, m_T) * invS;

	float h_opt = 0.0f, sum_w = 0.0f;

	float sorting[9];
	int num_sort = 0;
	for(int dy = -1; dy < 2; dy++) {
		if(dy+y < tile.y || dy+y >= tile.y+tile.w) continue;
		for(int dx = -1; dx < 2; dx++) {
			if(dx+x < tile.x || dx+x >= tile.x+tile.z) continue;
			sorting[num_sort++] = storage[dx + tile.z*dy].global_bandwidth;
		}
	}
	h_opt /= sum_w;
	for(int i = 1; i < num_sort; i++) {
		float v = sorting[i];
		int j;
		for(j = i-1; j >= 0 && sorting[j] > v; j--)
			sorting[j+1] = sorting[j];
		sorting[j+1] = v;
	}
	h_opt = sorting[num_sort/2];
#ifdef WITH_CYCLES_DEBUG_FILTER
	storage->filtered_global_bandwidth = h_opt;
#endif

	float *bandwidth = storage->bandwidth;
	for(int b = 0; b < rank; b++) {
		/*num_sort = 0;
		for(int dy = -1; dy < 2; dy++) {
			if(dy+y < tile.y || dy+y >= tile.y+tile.w) continue;
			for(int dx = -1; dx < 2; dx++) {
				if(dx+x < tile.x || dx+x >= tile.x+tile.z) continue;
				if(__float_as_int(storage[90 + 99*(dx + tile.z*dy)]) <= b) continue;
				sorting[num_sort++] = storage[b + 99*(dx + tile.z*dy)];
			}
		}
		for(int i = 1; i < num_sort; i++) {
			float v = sorting[i];
			int j;
			for(j = i-1; j && sorting[j] > v; j--)
				sorting[j+1] = sorting[j];
			sorting[j+1] = v;
		}
		bandwidth[b] = (float) h_opt * sorting[num_sort/2];*/
		bandwidth[b] *= h_opt;
	}


	{
		float A[100], z[9], invL[100], invA[10];
		for(int i = 0; i < 100; i++)
			A[i] = 0.0f;

		float cCm = average(Buf_F3(x, y, m_C))*invS;
		float cCs = sqrtf(average(Buf_F3(x, y, v_C))*invS*invSv);

		for(int py = lo.y; py <= hi.y; py++) {
			for(int px = lo.x; px <= hi.x; px++) {
				if(fabsf(cCm - average(Buf_F3(px, py, m_C))*invS) > 3.0f * (cCs + sqrtf(average(Buf_F3(px, py, v_C))*invS*invSv)) + 0.005f)
					continue;

				float dD = Buf_F(px, py, m_D) * invS - meanD;
				float3 dN = Buf_F3(px, py, m_N) * invS - meanN;
				float3 dT = Buf_F3(px, py, m_T) * invS - meanT;

				float weight = 1.0f;
				for(int col = 0; col < rank; col++) {
					z[col] = transform[     col] * (px - x) + transform[ 9 + col] * (py - y)
					       + transform[18 + col] * dD       + transform[27 + col] * dN.x
					       + transform[36 + col] * dN.y     + transform[45 + col] * dN.z
					       + transform[54 + col] * dT.x     + transform[63 + col] * dT.y
					       + transform[72 + col] * dT.z;
					float t = z[col] / bandwidth[col];
					if(fabsf(t) < 1.0f)
						weight *= 0.75f * (1.0f - t*t);
					else {
						weight = 0.0f;
						break;
					}
				}

				if(weight > 0.0f) {
					weight /= max(1.0f, average(Buf_F3(px, py, v_C)) * invS*invSv);
					A[0] += weight;
					for(int c = 1; c < rank+1; c++)
						A[c] += weight*z[c-1];
					for(int r = 1; r < rank+1; r++)
						for(int c = r; c < rank+1; c++)
							A[r*(rank+1)+c] += weight*z[c-1]*z[r-1];
				}
			}
		}
		for(int i = 0; i < rank+1; i++)
			A[i*(rank+1)+i] += 0.0001f;

#ifdef WITH_CYCLES_DEBUG_FILTER
		storage->sum_weight = A[0];
#endif
		orig_cholesky(A, rank+1, A);

		for(int i = 0; i < 100; i++)
			invL[i] = 0.0f;

		for(int j = rank; j >= 0; j--) {
			invL[j*(rank+1)+j] = 1.0f / A[j*(rank+1)+j];
			for(int k = j+1; k < rank+1; k++)
				for(int i = j+1; i < rank+1; i++)
					invL[k*(rank+1)+j] += invL[k*(rank+1)+i] * A[i*(rank+1)+j];
			for(int k = j+1; k < rank+1; k++)
				invL[k*(rank+1)+j] *= -invL[j*(rank+1)+j];
		}

		for(int i = 0; i < rank+1; i++) {
			invA[i] = 0.0f;
			for(int k = i; k < rank+1; k++)
				invA[i] += invL[k*(rank+1)] * invL[k*(rank+1)+i];
		}

		float3 out = make_float3(0.0f, 0.0f, 0.0f);
		float3 outP = make_float3(0.0f, 0.0f, 0.0f);
		float Psum = 0.0f;

		float avg_t[9] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
		float weightsum = 0.0f;

		for(int py = lo.y; py <= hi.y; py++) {
			for(int px = lo.x; px <= hi.x; px++) {
				if(fabsf(average(Buf_F3(px, py, m_C))*invS - cCm) > 3.0f * (cCs + sqrtf(average(Buf_F3(px, py, v_C))*invS*invSv)) + 0.005f)
					continue;

				float dD = Buf_F(px, py, m_D) * invS - meanD;
				float3 dN = Buf_F3(px, py, m_N) * invS - meanN;
				float3 dT = Buf_F3(px, py, m_T) * invS - meanT;

				float weight = 1.0f;
				for(int col = 0; col < rank; col++) {
					z[col] = transform[     col] * (px - x) + transform[ 9 + col] * (py - y)
					       + transform[18 + col] * dD       + transform[27 + col] * dN.x
					       + transform[36 + col] * dN.y     + transform[45 + col] * dN.z
					       + transform[54 + col] * dT.x     + transform[63 + col] * dT.y
					       + transform[72 + col] * dT.z;
					float t = z[col] / bandwidth[col];
					avg_t[col] += fabsf(t);
					if(fabsf(t) < 1.0f)
						weight *= 0.75f * (1.0f - t*t);
					else {
						weight = 0.0f;
						break;
					}
				}

				weightsum += weight;

				if(weight > 0.0f) {
					weight /= max(1.0f, average(Buf_F3(px, py, v_C)) * invS*invSv);

					float l = invA[0];
					for(int f = 0; f < rank; f++)
						l += z[f]*invA[f+1];
					l *= weight;

					out += l * Buf_F3(px, py, m_C)*invS;

					if(l > 0.0f) {
						outP += l * Buf_F3(px, py, m_C)*invS;
						Psum += l;
					}
				}
			}
		}

		if(out.x < 0.0f || out.y < 0.0f || out.z < 0.0f) {
			Psum = max(Psum, 1e-3f);
			out = outP / Psum;
		}


		out *= samples;
		float o_alpha = Buf_F(x, y, 3);
		Buf_F4(x, y, 0) = make_float4(out.x, out.y, out.z, o_alpha);
	}
}

CCL_NAMESPACE_END

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

#define Buf_F(px, py, o) denoise_data[(py - rect.y)*denoise_stride + (px - rect.x) + pass_stride*(o)]//(buffers[((y) * w + (x)) * kernel_data.film.pass_stride + (o)])
#define Buf_F3(px, py, o) make_float3(denoise_data[(py - rect.y)*denoise_stride + (px - rect.x) + pass_stride*(o)], denoise_data[(py - rect.y)*denoise_stride + (px - rect.x) + pass_stride*((o)+2)], denoise_data[(py - rect.y)*denoise_stride + (px - rect.x) + pass_stride*((o)+4)])//(buffers + ((y) * w + (x)) * kernel_data.film.pass_stride + (o)))
//#define Buf_F4(x, y, o) *((float4*) (buffers + ((y) * w + (x)) * kernel_data.film.pass_stride + (o)))

ccl_device float3 saturate(float3 a)
{
	return make_float3(saturate(a.x), saturate(a.y), saturate(a.z));
}

ccl_device void cholesky(float *A, int n, float *L)
{
	for (int i = 0; i < n; ++i) {
		for (int j = 0; j <= i; ++j) {
			float s = 0.0f;
			for (int k = 0; k < j; ++k) {
				s += L[i * n + k] * L[j * n + k];
			}
			L[i * n + j] = (i == j) ? sqrtf(A[i * n + i] - s) : (1.0f / L[j * n + j] * (A[j * n + i] - s));
		}
	}
}

ccl_device int old_svd(float *A, float *V, float *S2, int n)
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
					if (q <= e2 * S2[0] || fabsf(p) <= tol * q) {
						RotCount--;
					}
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

ccl_device void kernel_filter1_pixel(KernelGlobals *kg, float ccl_readonly_ptr denoise_data, int x, int y, int samples, int halfWindow, float bandwidthFactor, float* storage2, int4 rect)
{
	FilterStorage *storage = (FilterStorage*) storage2;
	int2 lo = make_int2(max(x - halfWindow, rect.x), max(y - halfWindow, rect.y));
	int2 hi = make_int2(min(x + halfWindow, rect.z-1), min(y + halfWindow, rect.w-1));
	int num = (hi.x - lo.x + 1) * (hi.y - lo.y + 1);
	int denoise_stride = align_up(rect.z-rect.x, 4);
	int pass_stride = (rect.w-rect.y)*denoise_stride;

	float3 meanT = make_float3(0.0f, 0.0f, 0.0f);
	float3 meanN = make_float3(0.0f, 0.0f, 0.0f);
	float meanD = 0.0f, meanS = 0.0f;

	for(int py = lo.y; py <= hi.y; py++) {
		for(int px = lo.x; px <= hi.x; px++) {
			meanD += Buf_F (px, py, 6);
			meanN += Buf_F3(px, py, 0);
			meanT += Buf_F3(px, py, 10);
			meanS += Buf_F (px, py, 8);
		}
	}
	meanT /= num;
	meanN /= num;
	meanD /= num;
	meanS /= num;
	float delta[11], transform[121], norm;
	int rank;
	/* Generate transform */
	{
		float nD = 0.0f, nT = 0.0f, nN = 0.0f, nS = 0.0f;
		for(int py = lo.y; py <= hi.y; py++) {
			for(int px = lo.x; px <= hi.x; px++) {
				nD = max(fabsf(Buf_F(px, py, 6) - meanD), nD);
				nN = max(len_squared(Buf_F3(px, py, 0) - meanN), nN);
				nT = max(len_squared(Buf_F3(px, py, 10) - meanT), nT);
				nS = max(fabsf(Buf_F(px, py, 8) - meanS), nS);
			}
		}

		nD = 1.0f / max(nD, 0.01f);
		nN = 1.0f / max(sqrtf(nN), 0.01f);
		nT = 1.0f / max(sqrtf(nT), 0.01f);
		nS = 1.0f / max(nS, 0.01f);

		norm = 0.0f;
		for(int i = 0; i < 121; i++)
			transform[i] = 0.0f;
		for(int py = lo.y; py <= hi.y; py++) {
			for(int px = lo.x; px <= hi.x; px++) {
				delta[0] = ((float) px - x) / halfWindow;
				delta[1] = ((float) py - y) / halfWindow;
				delta[2] = 0.0f;
				delta[3] = (Buf_F(px, py, 6) - meanD) * nD;
				float3 dN = (Buf_F3(px, py, 0) - meanN) * nN;
				delta[4] = dN.x;
				delta[5] = dN.y;
				delta[6] = dN.z;
				delta[7] = (Buf_F(px, py, 8) - meanS) * nS;
				float3 dT = (Buf_F3(px, py, 10) - meanT) * nT;
				delta[8] = dT.x;
				delta[9] = dT.y;
				delta[10] = dT.z;

				for(int r = 0; r < 11; r++)
					for(int c = r; c < 11; c++)
						transform[11*r+c] += delta[r]*delta[c];

				norm += nD * nD * saturate(Buf_F(px, py, 7));
				//norm += nS * nS * saturate(Buf_F(px, py, 9));
				norm += nN * nN * 3.0f * average(saturate(Buf_F3(px, py, 1)));
				norm += nT * nT * 3.0f * average(saturate(Buf_F3(px, py, 11)));
			}
		}

		/* Here, transform is self-adjoint (TODO term symmetric?) by construction, so one half can be copied from the other one */
		for(int r = 1; r < 11; r++)
			for(int c = 0; c < r; c++)
				transform[11*r+c] = transform[11*c+r];

		float V[121], S[11];
		rank = old_svd(transform, V, S, 11);

		for(int i = 0; i < 11; i++)
			S[i] = sqrtf(fabsf(S[i]));

		float threshold = 0.01f + 2.0f * (sqrtf(norm) / (sqrtf(rank) * 0.5f));
		rank = 0;

		/* Truncate matrix to reduce the rank */
		for(int c = 0; c < 11; c++) {
			float singular = sqrtf(S[c]); //TODO 2x sqrtf?
			if((singular > threshold) || (c < 2)) { /* Image position is always used */
				transform[     c] = V[     c] / halfWindow;
				transform[11 + c] = V[11 + c] / halfWindow;
				transform[22 + c] = V[22 + c] * 0.0f;
				transform[33 + c] = V[33 + c] * nD;
				transform[44 + c] = V[44 + c] * nN;
				transform[55 + c] = V[55 + c] * nN;
				transform[66 + c] = V[66 + c] * nN;
				transform[77 + c] = V[77 + c] * nS;
				transform[88 + c] = V[88 + c] * nT;
				transform[99 + c] = V[99 + c] * nT;
				transform[110 + c] = V[110 + c] * nT;
				rank++;
			}
		}

#ifdef WITH_CYCLES_DEBUG_FILTER
		storage->means[0] = x; storage->means[1] = y; storage->means[2] = 0.0f;
		storage->means[3] = meanD; storage->means[4] = meanN.x; storage->means[5] = meanN.y; storage->means[6] = meanN.z;
		storage->means[7] = meanS; storage->means[8] = meanT.x; storage->means[9] = meanT.y; storage->means[10] = meanT.z;
		storage->scales[0] = 1.0f/halfWindow; storage->scales[1] = 1.0f/halfWindow; storage->scales[2] = 0.0f;
		storage->scales[3] = nD; storage->scales[4] = nN; storage->scales[5] = nN; storage->scales[6] = nN;
		storage->scales[7] = nS; storage->scales[8] = nT; storage->scales[9] = nT; storage->scales[10] = nT;
		storage->singular_threshold = threshold;
		storage->feature_matrix_norm = norm;
		for(int i = 0; i < 11; i++)
			storage->singular[i] = S[i];
#endif
	}

	float bi[11];
	/* Approximate bandwidths */
	{
		const int size = 2*rank+1;
		float z[22]; /* Only 0 to rank-1 gets used (and rank to 2*rank-1 for the squared values) */
		float A[23*23];
		float3 XtB[23];

		meanD = Buf_F (x, y, 6);
		meanN = Buf_F3(x, y, 0);
		meanT = Buf_F3(x, y, 10);
		meanS = Buf_F (x, y, 8);

		for(int i = 0; i < size*size; i++)
			A[i] = 0.0f;
		for(int i = 0; i < size; i++)
			XtB[i] = make_float3(0.0f, 0.0f, 0.0f);
		for(int py = lo.y; py <= hi.y; py++) {
			for(int px = lo.x; px <= hi.x; px++) {
				float dD = Buf_F(px, py, 6) - meanD;
				float3 dN = Buf_F3(px, py, 0) - meanN;
				float3 dT = Buf_F3(px, py, 10) - meanT;
				float dS = Buf_F(px, py, 8) - meanS;

				float weight = 1.0f;
				for(int col = 0; col < rank; col++) {
					z[col] = transform[     col] * (px - x) + transform[11 + col] * (py - y)
					       + transform[33 + col] * dD       + transform[44 + col] * dN.x
					       + transform[55 + col] * dN.y     + transform[66 + col] * dN.z
					       + transform[77 + col] * dS       + transform[88 + col] * dT.x
					       + transform[99 + col] * dT.y     + transform[110 + col] * dT.z;
					if(fabsf(z[col]) < 1.0f)
						weight *= 0.75f * (1.0f - z[col]*z[col]);
					else {
						weight = 0.0f;
						break;
					}
				}

				if(weight > 0.0f) {
					weight /= max(1.0f, average(Buf_F3(px, py, 17)));
					for(int i = 0; i < rank; i++)
						z[rank+i] = z[i]*z[i];
					A[0] += weight;
					XtB[0] += Buf_F3(px, py, 16) * weight;
					for(int c = 1; c < size; c++) {
						float lweight = weight * z[c-1];
						A[c] += lweight;
						XtB[c] += Buf_F3(px, py, 16) * lweight;
					}
					for(int r = 1; r < size; r++)
						for(int c = r; c < size; c++)
							A[r*size + c] += weight * z[r-1] * z[c-1];
				}
			}
		}

		for(int i = 0; i < size; i++)
			A[i*size+i] += 0.001f;

		cholesky(A, size, A);

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

		for(int i = 0; i < 11; i++) //TODO < rank enough? Why +0.16?
		{
			bi[i] = bandwidthFactor / sqrtf(fabsf(2.0f * average(fabs(XtB[i + rank + 1]))) + 0.16f);
		}

	}

	double bias_XtX = 0.0, bias_XtY = 0.0, var_XtX = 0.0, var_XtY = 0.0;
	for(int g = 0; g < 6; g++) {
		float A[144], z[11], invL[144], invA[12];
		for(int i = 0; i < 144; i++)
			A[i] = 0.0f;

		const float g_lookup[] = {0.05f, 0.1f, 0.25f, 0.5f, 1.0f, 2.0f};
		float g_w = g_lookup[g];
		float bandwidth[11];
		for(int i = 0; i < rank; i++)
			bandwidth[i] = g_w * bi[i];

		float cCm = average(Buf_F3(x, y, 16));
		float cCs = sqrtf(average(Buf_F3(x, y, 17)));

		for(int py = lo.y; py <= hi.y; py++) {
			for(int px = lo.x; px <= hi.x; px++) {
				if(fabsf(average(Buf_F3(px, py, 16)) - cCm) > 3.0f * (cCs + sqrtf(average(Buf_F3(px, py, 17)))) + 0.005f)
					continue;

				float dD = Buf_F(px, py, 6) - meanD;
				float3 dN = Buf_F3(px, py, 0) - meanN;
				float3 dT = Buf_F3(px, py, 10) - meanT;
				float dS = Buf_F(px, py, 8) - meanS;

				float weight = 1.0f;
				for(int col = 0; col < rank; col++) {
					z[col] = transform[     col] * (px - x) + transform[11 + col] * (py - y)
					       + transform[33 + col] * dD       + transform[44 + col] * dN.x
					       + transform[55 + col] * dN.y     + transform[66 + col] * dN.z
					       + transform[77 + col] * dS       + transform[88 + col] * dT.x
					       + transform[99 + col] * dT.y     + transform[110 + col] * dT.z;
					float t = z[col] / bandwidth[col];
					if(fabsf(t) < 1.0f)
						weight *= 0.75f * (1.0f - t*t);
					else {
						weight = 0.0f;
						break;
					}
				}

				if(weight > 0.0f) {
					weight /= max(1.0f, average(Buf_F3(px, py, 17)));
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

		cholesky(A, rank+1, A);

		for(int i = 0; i < 144; i++)
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
				if(fabsf(average(Buf_F3(px, py, 16)) - cCm) > 3.0f * (cCs + sqrtf(average(Buf_F3(px, py, 17)))) + 0.005f)
					continue;

				float dD = Buf_F(px, py, 6) - meanD;
				float3 dN = Buf_F3(px, py, 0) - meanN;
				float3 dT = Buf_F3(px, py, 10) - meanT;
				float dS = Buf_F(px, py, 8) - meanS;

				float weight = 1.0f;
				for(int col = 0; col < rank; col++) {
					z[col] = transform[     col] * (px - x) + transform[11 + col] * (py - y)
					       + transform[33 + col] * dD       + transform[44 + col] * dN.x
					       + transform[55 + col] * dN.y     + transform[66 + col] * dN.z
					       + transform[77 + col] * dS       + transform[88 + col] * dT.x
					       + transform[99 + col] * dT.y     + transform[110 + col] * dT.z;
					float t = z[col] / bandwidth[col];
					if(fabsf(t) < 1.0f)
						weight *= 0.75f * (1.0f - t*t);
					else {
						weight = 0.0f;
						break;
					}
				}

				if(weight > 0.0f) {
					weight /= max(1.0f, average(Buf_F3(px, py, 17)));

					float l = invA[0];
					for(int f = 0; f < rank; f++)
						l += z[f]*invA[f+1];
					l *= weight;

					beta += l * Buf_F3(px, py, 16);
					var += l*l * max(average(Buf_F3(px, py, 17)), 0.0f);

					if(l > 0.0f) {
						err_beta += l * Buf_F3(px, py, 16);
						err_var += l*l * max(average(Buf_F3(px, py, 17)), 0.0f);
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
		double bias = average(beta - (Buf_F3(x, y, 16)));

		bias_XtX += h2*h2;
		bias_XtY += h2 * bias;
		double var_x = 1.0 / (pow(g_w, rank) * samples);
		var_XtX += var_x*var_x;
		var_XtY += var_x * (double) var;
	}

	double bias_coef = bias_XtY / bias_XtX;
	double variance_coef = var_XtY / var_XtX;
	/* Since MSE is defined as bias^2 + variance, from the models above it follows that MSE(h) = a^2*h^4 + b/(n*h^k).
	 * Minimizing that term w.r.t h gives h_min = (k*b / (4*a^2*n))^(1/k+4).
	 * Note that this is the term that is given in the paper as well, but they incorrectly use different models for bias and variance.
	 */
	float h_opt = (float) pow((rank * variance_coef) / (4.0 * bias_coef*bias_coef * samples), 1.0 / (rank + 4));
	//h_opt = clamp(h_opt, 0.2f, 1.0f);

	for(int i = 0; i < 11; i++)
		storage->bandwidth[i] = (i < rank)? bi[i]: 0.0f;
	for(int i = 0; i < 121; i++)
		storage->transform[i] = transform[i];
	storage->rank = rank;
	storage->global_bandwidth = h_opt;
}

ccl_device void kernel_filter2_pixel(KernelGlobals *kg, float *buffers, float ccl_readonly_ptr denoise_data, int x, int y, int offset, int stride, int samples, int halfWindow, float bandwidthFactor, float *storage2, int4 rect, int4 tile)
{
	FilterStorage *storage = (FilterStorage*) storage2;

	int2 lo = make_int2(max(x - halfWindow, rect.x), max(y - halfWindow, rect.y));
	int2 hi = make_int2(min(x + halfWindow, rect.z-1), min(y + halfWindow, rect.w-1));
	int num = (hi.x - lo.x + 1) * (hi.y - lo.y + 1);
	int denoise_stride = align_up(rect.z-rect.x, 4);
	int pass_stride = (rect.w-rect.y)*denoise_stride;

	//Load storage data
	float ccl_readonly_ptr bi = storage->bandwidth;
	float ccl_readonly_ptr transform = storage->transform;
	int rank = storage->rank;

	float meanD = Buf_F (x, y, 6);
	float3 meanN = Buf_F3(x, y, 0);
	float3 meanT = Buf_F3(x, y, 10);
	float meanS = Buf_F (x, y, 8);

	float h_opt = 0.0f, sum_w = 0.0f;
	for(int dy = -3; dy < 4; dy++) {
		if(dy+y < tile.y || dy+y >= tile.y+tile.w) continue;
		for(int dx = -3; dx < 4; dx++) {
			if(dx+x < tile.x || dx+x >= tile.x+tile.z) continue;
			float we = expf(-0.5f*(dx*dx+dy*dy));
			h_opt += we*storage[dx + tile.z*dy].global_bandwidth;
			sum_w += we;
		}
	}
	h_opt /= sum_w;

	{
		float A[144], z[11], invL[144], invA[12];
		for(int i = 0; i < 144; i++)
			A[i] = 0.0f;

		float bandwidth[11];
		for(int i = 0; i < rank; i++)
			bandwidth[i] = (float) h_opt * bi[i];

		float cCm = average(Buf_F3(x, y, 16));
		float cCs = sqrtf(average(Buf_F3(x, y, 17)));

		for(int py = lo.y; py <= hi.y; py++) {
			for(int px = lo.x; px <= hi.x; px++) {
				if(fabsf(cCm - average(Buf_F3(px, py, 16))) > 3.0f * (cCs + sqrtf(average(Buf_F3(px, py, 17)))) + 0.005f)
					continue;

				float dD = Buf_F(px, py, 6) - meanD;
				float3 dN = Buf_F3(px, py, 0) - meanN;
				float3 dT = Buf_F3(px, py, 10) - meanT;
				float dS = Buf_F(px, py, 8) - meanS;

				float weight = 1.0f;
				for(int col = 0; col < rank; col++) {
					z[col] = transform[     col] * (px - x) + transform[11 + col] * (py - y)
					       + transform[33 + col] * dD       + transform[44 + col] * dN.x
					       + transform[55 + col] * dN.y     + transform[66 + col] * dN.z
					       + transform[77 + col] * dS       + transform[88 + col] * dT.x
					       + transform[99 + col] * dT.y     + transform[110 + col] * dT.z;
					float t = z[col] / bandwidth[col];
					if(fabsf(t) < 1.0f)
						weight *= 0.75f * (1.0f - t*t);
					else {
						weight = 0.0f;
						break;
					}
				}

				if(weight > 0.0f) {
					weight /= max(1.0f, average(Buf_F3(px, py, 17)));
					A[0] += weight;
					for(int c = 1; c < rank+1; c++)
						A[c] += weight*z[c-1];
					for(int r = 1; r < rank+1; r++)
						for(int c = r; c < rank+1; c++)
							A[r*(rank+1)+c] += weight*z[c-1]*z[r-1];
				}
			}
		}

#ifdef WITH_CYCLES_DEBUG_FILTER
		storage->filtered_global_bandwidth = h_opt;
		storage->sum_weight = A[0];
#endif

		for(int i = 0; i < rank+1; i++)
			A[i*(rank+1)+i] += 0.0001f;

		cholesky(A, rank+1, A);

		for(int i = 0; i < 144; i++)
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

		for(int py = lo.y; py <= hi.y; py++) {
			for(int px = lo.x; px <= hi.x; px++) {
				if(fabsf(average(Buf_F3(px, py, 16)) - cCm) > 3.0f * (cCs + sqrtf(average(Buf_F3(px, py, 17)))) + 0.005f)
					continue;

				float dD = Buf_F(px, py, 6) - meanD;
				float3 dN = Buf_F3(px, py, 0) - meanN;
				float3 dT = Buf_F3(px, py, 10) - meanT;
				float dS = Buf_F(px, py, 8) - meanS;

				float weight = 1.0f;
				for(int col = 0; col < rank; col++) {
					z[col] = transform[     col] * (px - x) + transform[11 + col] * (py - y)
					       + transform[33 + col] * dD       + transform[44 + col] * dN.x
					       + transform[55 + col] * dN.y     + transform[66 + col] * dN.z
					       + transform[77 + col] * dS       + transform[88 + col] * dT.x
					       + transform[99 + col] * dT.y     + transform[110 + col] * dT.z;
					float t = z[col] / bandwidth[col];
					if(fabsf(t) < 1.0f)
						weight *= 0.75f * (1.0f - t*t);
					else {
						weight = 0.0f;
						break;
					}
				}

				if(weight > 0.0f) {
					weight /= max(1.0f, average(Buf_F3(px, py, 17)));

					float l = invA[0];
					for(int f = 0; f < rank; f++)
						l += z[f]*invA[f+1];
					l *= weight;

					out += l * Buf_F3(px, py, 16);
					if(l > 0.0f) {
						outP += l * Buf_F3(px, py, 16);
						Psum += l;
					}
				}
			}
		}

		if(out.x < 0.0f || out.y < 0.0f || out.z < 0.0f)
			out = outP / max(Psum, 0.001f);
		out *= samples;

		float *combined_buffer = buffers + (offset + y*stride + x)*kernel_data.film.pass_stride;
		float o_alpha = combined_buffer[3];
		*((float4*) combined_buffer) = make_float4(out.x, out.y, out.z, o_alpha);
	}
}

CCL_NAMESPACE_END
/*
 * Copyright 2011-2016 Blender Foundation
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

#ifndef __UTIL_MATH_MATRIX_H__
#define __UTIL_MATH_MATRIX_H__

CCL_NAMESPACE_BEGIN

#define MAT(A, size, row, col) A[(row)*(size)+(col)]

/* Variants that use a constant stride on GPUS. */
#ifdef __KERNEL_GPU__
#define MATS(A, n, r, c, s) A[((r)*(n)+(c))*(s)]
#else
#define MATS(A, n, r, c, s) MAT(A, n, r, c)
#endif

ccl_device_inline void math_matrix_zero(float *A, int n)
{
	for(int i = 0; i < n*n; i++)
		A[i] = 0.0f;
}

ccl_device_inline void math_vector_zero(float *v, int n)
{
	for(int i = 0; i < n; i++)
		v[i] = 0.0f;
}

ccl_device_inline void math_vec3_zero(float3 *v, int n)
{
	for(int i = 0; i < n; i++)
		v[i] = make_float3(0.0f, 0.0f, 0.0f);
}

ccl_device_inline void math_matrix_zero_lower(float *A, int n)
{
	for(int row = 0; row < n; row++)
		for(int col = 0; col <= row; col++)
			MAT(A, n, row, col) = 0.0f;
}

ccl_device_inline void math_matrix_identity(float *A, int n)
{
	for(int row = 0; row < n; row++)
		for(int col = 0; col < n; col++)
			MAT(A, n, row, col) = (row == col)? 1.0f: 0.0f;
}

/* In-place Cholesky-Banachiewicz decomposition of the square, positive-definite matrix A
 * into a lower triangular matrix L so that A = L*L^T. A is being overwritten by L.
 * Also, only the lower triangular part of A is ever accessed. */
ccl_device void math_cholesky(float *A, int n)
{
	for(int row = 0; row < n; row++) {
		for(int col = 0; col <= row; col++) {
			float sum_col = MAT(A, n, row, col);
			for(int k = 0; k < col; k++) {
				sum_col -= MAT(A, n, row, k) * MAT(A, n, col, k);
			}
			if(row == col) {
				sum_col = sqrtf(max(sum_col, 0.0f));
			}
			else {
				sum_col /= MAT(A, n, col, col);
			}
			MAT(A, n, row, col) = sum_col;
		}
	}
}

ccl_device_inline void math_matrix_add_diagonal(float *A, int n, float val)
{
	for(int row = 0; row < n; row++)
		MAT(A, n, row, row) += val;
}

/* Add Gramian matrix of v to A.
 * The Gramian matrix of v is v^T*v, so element (i,j) is v[i]*v[j].
 * Obviously, the resulting matrix is symmetric, so only the lower triangluar part is stored. */
ccl_device_inline void math_add_gramian(float *A, int n, float *v, float weight)
{
	for(int row = 0; row < n; row++)
		for(int col = 0; col <= row; col++)
			MAT(A, n, row, col) += v[row]*v[col]*weight;
}

ccl_device_inline void math_add_vec3(float3 *v, int n, float *x, float3 w)
{
	for(int i = 0; i < n; i++)
		v[i] += w*x[i];
}

ccl_device_inline float math_dot(float ccl_readonly_ptr a, float ccl_readonly_ptr b, int n)
{
	float d = 0.0f;
	for(int i = 0; i < n; i++)
		d += a[i]*b[i];
	return d;
}

ccl_device_inline float3 math_dot_vec3(float ccl_readonly_ptr a, float3 ccl_readonly_ptr b, int n)
{
	float3 d = make_float3(0.0f, 0.0f, 0.0f);
	for(int i = 0; i < n; i++)
		d += a[i]*b[i];
	return d;
}

#ifdef __KERNEL_CUDA__
ccl_device_inline float math_dot_cuda(float ccl_readonly_ptr a, float ccl_readonly_ptr b, int bstride, int n)
{
	float d = 0.0f;
	for(int i = 0; i < n; i++)
		d += a[i]*b[i*bstride];
	return d;
}
#endif

/* Solve the linear equation system L*x = b through forward substitution, where L is a lower triangular matrix.
 * x is initially set to the right-hand-side vector and is overwritten with the solution vector x. */
ccl_device_inline void math_substitute_forward_vec3(float *L, int n, float3 *x)
{
	for(int row = 0; row < n; row++) {
		float3 sum = make_float3(0.0f, 0.0f, 0.0f);
		for(int col = 0; col < row; col++)
			sum += MAT(L, n, row, col) * x[col];
		x[row] = (x[row] - sum) / MAT(L, n, row, row);
	}
}

/* Solve the linear equation system L*x = b through backsubstitution, where L is a upper triangular matrix.
 * In this implementation, instead of L, L^T is passed instead.
 * x is initially set to the right-hand-side vector and is overwritten with the solution vector x. */
ccl_device_inline void math_substitute_back_vec3(float *LT, int n, float3 *x)
{
	for(int row = n-1; row >= 0; row--) {
		float3 sum = make_float3(0.0f, 0.0f, 0.0f);
		for(int col = row+1; col < n; col++)
			sum += MAT(LT, n, col, row) * x[col];
		x[row] = (x[row] - sum) / MAT(LT, n, row, row);
	}
}

ccl_device_inline void math_inverse_lower_tri(float *L, float *invL, int n)
{
	for(int comp = 0; comp < n; comp++) {
		for(int row = 0; row < comp; row++)
			MAT(invL, n, row, comp) = 0.0f;
		MAT(invL, n, comp, comp) = 1.0f / MAT(L, n, comp, comp);
		for(int row = comp+1; row < n; row++) {
			float sum = 0.0f;
			for(int col = comp; col < row; col++)
				sum += MAT(L, n, row, col) * MAT(invL, n, col, comp);
			MAT(invL, n, row, comp) = -sum/MAT(L, n, row, row);
		}
	}
}

/* Inverts the lower triangular matrix L and overwrites it with the transpose
 * of the result. */
ccl_device_inline void math_inverse_lower_tri_inplace(float *L, int n)
{
	for(int row = 0; row < n; row++)
		MAT(L, n, row, row) = 1.0f / MAT(L, n, row, row);

	for(int comp = 0; comp < n; comp++) {
		for(int row = comp+1; row < n; row++) {
			float sum = 0.0f;
			for(int col = comp; col < row; col++)
				sum += MAT(L, n, row, col) * MAT(L, n, comp, col);
			MAT(L, n, comp, row) = -sum*MAT(L, n, row, row);
		}
	}
}

/* Solve S = inv(Xt*W*X)*Xt*W*y.
 * Instead of explicitly inverting Xt*W*X, we rearrange to:
 * (Xt*W*X)*S = Xt*W*y
 * Xt*W*X is per definition symmetric positive-semidefinite, so we can apply Cholesky decomposition to find a lower triangular L so that L*Lt = Xt*W*X.
 * With that we get (L*Lt)*S = L*(Lt*S) = L*b = Xt*W*y.
 * Since L is lower triangular, finding b (=Lt*S) is relatively easy since Xt*W*y is known.
 * Then, the remaining problem is Lt*S = b, which again can be solved easily.
 *
 * Both inputs are destroyed in the process.
 * The result is returned in XtWy. */
ccl_device_inline void math_solve_normal_equation(float *XtWX, float3 *XtWy, int n)
{
	math_matrix_add_diagonal(XtWX, n, 1e-4f); /* Improve the numerical stability. */
	math_cholesky(XtWX, n); /* Find L so that L*Lt = Xt*W*X. */
	math_substitute_forward_vec3(XtWX, n, XtWy); /* Solve L*b = X^T*W*y, replacing X^T*W*y by b. */
	math_substitute_back_vec3(XtWX, n, XtWy); /* Solve L^T*S = b, replacing b by S. */
}

ccl_device float math_largest_eigenvalue(float *A, int n, float *vec, float *tmp)
{
	/* Matrix-Vector-Multiplication that only accesses the lower triangular part of A. */
	float fac = 0.0f;
	float eigval = 1.0f;

	for(int r = 0; r < n; r++)
		fac += vec[r]*vec[r];
	fac = 1.0f / sqrtf(fac);
	for(int r = 0; r < n; r++)
		vec[r] *= fac;

	for(int i = 0; i < 100; i++) {
		fac = 0.0f;
		for(int r = 0; r < n; r++) {
			tmp[r] = 0.0f;
			int c;
			for(c = 0; c <= r; c++)
				tmp[r] += MAT(A, n, r, c)*vec[c];
			for(; c < n; c++)
				tmp[r] += MAT(A, n, c, r)*vec[c];
			fac += tmp[r]*tmp[r];
		}

		if(fac < 1e-10f) return 0.0f;
		float new_eigval = sqrtf(fac);

		fac = 1.0f / sqrtf(fac);
		for(int r = 0; r < n; r++) {
			vec[r] = tmp[r]*fac;
		}

		if(fabsf(new_eigval - eigval)/max(new_eigval, 1e-7f) < 1e-6f)
			return new_eigval;
		eigval = new_eigval;
	}

	return 0.0f;
}

/* Perform the Jacobi Eigenvalue Methon on matrix A.
 * A is assumed to be a symmetrical matrix, therefore only the lower-triangular part is ever accessed.
 * The algorithm overwrites the contents of A.
 *
 * After returning, A will be overwritten with D, which is (almost) diagonal,
 * and V will contain the eigenvectors of the original A in its rows (!),
 * so that A = V^T*D*V. Therefore, the diagonal elements of D are the (sorted) eigenvalues of A.
 *
 * Additionally, the function returns an estimate of the rank of A.
 */
ccl_device int math_jacobi_eigendecomposition(float *A, float *V, int n, int v_stride)
{
	const float epsilon = 1e-7f;
	const float singular_epsilon = 1e-9f;

	for (int row = 0; row < n; row++)
		for (int col = 0; col < n; col++)
			MATS(V, n, row, col, v_stride) = (col == row) ? 1.0f : 0.0f;

	for (int sweep = 0; sweep < 8; sweep++) {
		float off_diagonal = 0.0f;
		for (int row = 1; row < n; row++)
			for (int col = 0; col < row; col++)
				off_diagonal += fabsf(MAT(A, n, row, col));
		if (off_diagonal < 1e-7f) {
			/* The matrix has nearly reached diagonal form.
			 * Since the eigenvalues are only used to determine truncation, their exact values aren't required - a relative error of a few ULPs won't matter at all. */
			break;
		}

		/* Set the threshold for the small element rotation skip in the first sweep:
		 * Skip all elements that are less than a tenth of the average off-diagonal element. */
		float threshold = 0.2f*off_diagonal / (n*n);

		for(int row = 1; row < n; row++) {
			for(int col = 0; col < row; col++) {
				/* Perform a Jacobi rotation on this element that reduces it to zero. */
				float element = MAT(A, n, row, col);
				float abs_element = fabsf(element);

				/* If we're in a later sweep and the element already is very small, just set it to zero and skip the rotation. */
				if (sweep > 3 && abs_element <= singular_epsilon*fabsf(MAT(A, n, row, row)) && abs_element <= singular_epsilon*fabsf(MAT(A, n, col, col))) {
					MAT(A, n, row, col) = 0.0f;
					continue;
				}

				if(element == 0.0f) {
					continue;
				}

				/* If we're in one of the first sweeps and the element is smaller than the threshold, skip it. */
				if(sweep < 3 && (abs_element < threshold)) {
					continue;
				}

				/* Determine rotation: The rotation is characterized by its angle phi - or, in the actual implementation, sin(phi) and cos(phi).
				 * To find those, we first compute their ratio - that might be unstable if the angle approaches 90°, so there's a fallback for that case.
				 * Then, we compute sin(phi) and cos(phi) themselves. */
				float singular_diff = MAT(A, n, row, row) - MAT(A, n, col, col);
				float ratio;
				if (abs_element > singular_epsilon*fabsf(singular_diff)) {
					float cot_2phi = 0.5f*singular_diff / element;
					ratio = 1.0f / (fabsf(cot_2phi) + sqrtf(1.0f + cot_2phi*cot_2phi));
					if (cot_2phi < 0.0f) ratio = -ratio; /* Copy sign. */
				}
				else {
					ratio = element / singular_diff;
				}

				float c = 1.0f / sqrtf(1.0f + ratio*ratio);
				float s = ratio*c;
				/* To improve numerical stability by avoiding cancellation, the update equations are reformulized to use sin(phi) and tan(phi/2) instead. */
				float tan_phi_2 = s / (1.0f + c);

				/* Update the singular values in the diagonal. */
				float singular_delta = ratio*element;
				MAT(A, n, row, row) += singular_delta;
				MAT(A, n, col, col) -= singular_delta;

				/* Set the element itself to zero. */
				MAT(A, n, row, col) = 0.0f;

				/* Perform the actual rotations on the matrices. */
#define ROT(M, r1, c1, r2, c2, stride)                                   \
				{                                                        \
					float M1 = MATS(M, n, r1, c1, stride);               \
					float M2 = MATS(M, n, r2, c2, stride);               \
					MATS(M, n, r1, c1, stride) -= s*(M2 + tan_phi_2*M1); \
					MATS(M, n, r2, c2, stride) += s*(M1 - tan_phi_2*M2); \
				}

				/* Split into three parts to ensure correct accesses since we only store the lower-triangular part of A. */
				for(int i = 0    ; i < col; i++) ROT(A, col, i, row, i, 1);
				for(int i = col+1; i < row; i++) ROT(A, i, col, row, i, 1);
				for(int i = row+1; i < n  ; i++) ROT(A, i, col, i, row, 1);

				for(int i = 0    ; i < n  ; i++) ROT(V, col, i, row, i, v_stride);
#undef ROT
			}
		}
	}

	/* Sort eigenvalues and the associated eigenvectors. */
	for (int i = 0; i < n - 1; i++) {
		float v = MAT(A, n, i, i);
		int k = i;
		for (int j = i; j < n; j++) {
			if (MAT(A, n, j, j) >= v) {
				v = MAT(A, n, j, j);
				k = j;
			}
		}
		if (k != i) {
			/* Swap eigenvalues. */
			MAT(A, n, k, k) = MAT(A, n, i, i);
			MAT(A, n, i, i) = v;
			/* Swap eigenvectors. */
			for (int j = 0; j < n; j++) {
				float v = MATS(V, n, i, j, v_stride);
				MATS(V, n, i, j, v_stride) = MATS(V, n, k, j, v_stride);
				MATS(V, n, k, j, v_stride) = v;
			}
		}
	}

	/* Estimate the rank of the original A. */
	int rank = 0;
	for (int i = 0; i < n; i++) {
		if (MAT(A, n, i, i) > epsilon) rank++;
	}
	return rank;
}

#ifdef __KERNEL_SSE3__
ccl_device_inline void math_matrix_zero_lower_sse(__m128 *A, int n)
{
	for(int row = 0; row < n; row++)
		for(int col = 0; col <= row; col++)
			MAT(A, n, row, col) = _mm_set1_ps(0.0f);
}

/* Add Gramian matrix of v to A.
 * The Gramian matrix of v is v^T*v, so element (i,j) is v[i]*v[j].
 * Obviously, the resulting matrix is symmetric, so only the lower triangluar part is stored. */
ccl_device_inline void math_add_gramian_sse(__m128 *A, int n, __m128 ccl_readonly_ptr v, __m128 weight)
{
	for(int row = 0; row < n; row++)
		for(int col = 0; col <= row; col++)
			MAT(A, n, row, col) = _mm_add_ps(MAT(A, n, row, col), _mm_mul_ps(_mm_mul_ps(v[row], v[col]), weight));
}

ccl_device_inline void math_add_vector_sse(__m128 *V, int n, __m128 ccl_readonly_ptr a)
{
	for(int i = 0; i < n; i++)
		V[i] = _mm_add_ps(V[i], a[i]);
}

ccl_device_inline void math_mul_vector_sse(__m128 *V, int n, __m128 ccl_readonly_ptr a)
{
	for(int i = 0; i < n; i++)
		V[i] = _mm_mul_ps(V[i], a[i]);
}

ccl_device_inline void math_mul_vector_scalar_sse(__m128 *V, int n, __m128 a)
{
	for(int i = 0; i < n; i++)
		V[i] = _mm_mul_ps(V[i], a);
}

ccl_device_inline void math_mask_vector_sse(__m128 *V, int n, __m128 mask)
{
	for(int i = 0; i < n; i++)
		V[i] = _mm_mask_ps(V[i], mask);
}

ccl_device_inline __m128 math_dot_sse(__m128 ccl_readonly_ptr a, __m128 ccl_readonly_ptr b, int n)
{
	__m128 d = _mm_setzero_ps();
	for(int i = 0; i < n; i++)
		d = _mm_add_ps(d, _mm_mul_ps(a[i], b[i]));
	return d;
}

ccl_device_inline float3 math_sum_float3(__m128 ccl_readonly_ptr a)
{
	return make_float3(_mm_hsum_ss(a[0]), _mm_hsum_ss(a[1]), _mm_hsum_ss(a[2]));
}

ccl_device_inline void math_hsum_matrix_lower(float *A, int n, __m128 ccl_readonly_ptr B)
{
	for(int row = 0; row < n; row++)
		for(int col = 0; col <= row; col++)
			MAT(A, n, row, col) = _mm_hsum_ss(MAT(B, n, row, col));
}
#endif

#undef MAT

CCL_NAMESPACE_END

#endif  /* __UTIL_MATH_MATRIX_H__ */

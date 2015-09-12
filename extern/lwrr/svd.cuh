// Adapted from Compact numerical methods for computers written by J. C. Nash
// Input: matrix A - (nCol = nRow = n)
// Output: matrix V and S2 that contains the squares of singular values
__device__ int svd(float *A, float *V, float *S2, int n)
{
    int  i, j, k, EstColRank = n, RotCount = n, SweepCount = 0;
    int slimit = 6;
    float eps = 1e-7;
    float e2 = 10.f * n * eps * eps;
    float tol = 0.1f * eps;
    float vt, p, x0, y0, q, r, c0, s0, d1, d2;

    while (RotCount != 0 && SweepCount++ <= slimit) {
        RotCount = EstColRank * (EstColRank - 1) / 2;

        for (j = 0; j < EstColRank-1; ++j)
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
                    if (q <= e2 * S2[0] || fabs(p) <= tol * q)
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
        while (EstColRank >= 3 && S2[EstColRank-1] <= S2[0] * tol + tol * tol)
            EstColRank--;
    }
    return EstColRank;
}

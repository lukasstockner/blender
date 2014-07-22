/*
 * ***** BEGIN GPL LICENSE BLOCK *****
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/curve_eval.c
 *  \ingroup bke
 */
#include <stdlib.h>
#include <stdio.h>

#if defined(NURBS_eval_test)
#include "curve_eval.h"
#else
extern "C" {
#include "BKE_curve.h"
#include "DNA_curve_types.h"
}
#endif

int BKE_nurb_binom_coeffs[NURBS_MAX_ORDER][NURBS_MAX_ORDER+1];
int BKE_nurb_binom_coeff(int k, int i) {
	static int inited=0;
	if (!inited) {
		inited=1;
		for (int kk=0; kk<NURBS_MAX_ORDER; kk++) {
			int last = 1;
			BKE_nurb_binom_coeffs[kk][0] = last;
			for (int ii=1; ii<=kk; ii++) {
				last *= kk-ii+1;
				last /= i;
				BKE_nurb_binom_coeffs[kk][ii] = last;
			}
		}
	}
#ifndef NDEBUG
	if (k>=NURBS_MAX_ORDER || i>k) {
		fprintf(stderr, "Invalid binomial coeff %i-choose-%i.\n",k,i);
		return 0;
	}
#endif
	return BKE_nurb_binom_coeffs[k][i];
}



/* ~~~~~~~~~~~~~~~~~~~~Non Uniform Rational B Spline calculations ~~~~~~~~~~~ */
float nurbs_eps = 1e-5;

void BKE_bspline_knot_calc(int flags, int pnts, int order, float knots[])
{
	int num_knots = pnts + order;
	if (flags & CU_NURB_CYCLIC) num_knots += order-1;
	if (flags & CU_NURB_BEZIER) {
		/* Previous versions of blender supported "Bezier" knot vectors. These
		 * are useless to the user. *All* NURBS surfaces can be transformed into
		 * Bezier quilts (grids of connected Bezier surfaces). The "Bezier" knot
		 * vector is only an intermediate mathematical step in this process.
		 * Bezier quilts may be exposed to the user but there is no point in having
		 * the option to use Bezier-style knot vectors without Bezier-style controls.
		 *           |------------------ pnts+order -------------|
		 *           |--ord--||-ord-1-||-ord-1-||-ord-1-||--ord--|
		 * Bezier:  { 0 0 0 0   1 1 1    2 2 2    3 3 3   4 4 4 4 }
		 */
		int v=0;
		for (int i=0; i<order; i++)
			knots[i] = v;
		for (int i=order,reps=0; i<pnts; i++) {
			if (reps==0) {
				v += 1;
				reps = order-1;
			}
			knots[i] = v;
		}
		for (int i=pnts; i<pnts+order; i++)
			knots[i] = v;
	} else if (flags & CU_NURB_ENDPOINT) {
		/* "Endpoint" knot vectors ensure that the first and last knots are
		 * repeated $order times so that the valid NURBS domain actually touches
		 * the edges of the control polygon. These are the default.
		 *  |------- pnts+order ------------|
		 *  |-order-||--pnts-order-||-order-|
		 * { 0 0 0 0 1 2 3 4 5 6 7 8 9 9 9 9 }
		 */
		for (int i=0; i<order; i++)
			knots[i] = 0;
		int v=1;
		for (int i=order; i<pnts; i++)
			knots[i] = v++;
		for (int i=pnts; i<pnts+order; i++)
			knots[i] = v;
	} else if ((flags&CU_NURB_CYCLIC) || true) {
		/* Uniform knot vectors are the simplest mathematically but they create
		 * the annoying problem where the edges of the surface do not reach the
		 * edges of the control polygon which makes positioning them difficult.
		 *  |------ pnts+order --------|
		 *  |-----pnts----||---order---|
		 * { 0 1 2 3 4 5 6  7 8 9 10 11 }
		 */
		/* Cyclic knot vectors are equivalent to uniform knot vectors over
		 * a control polygon of pnts+(order-1) points for a total of
		 * pnts+(order-1)+order knots. The additional (order-1) control points
		 * are repeats of the first (order-1) control points. This guarantees
		 * that all derivatives match at the join point.
		 *  |----- (pnts+order-1)+order --------|
		 *  |-----pnts----||--reps-||---order---|
		 * { 0 1 2 3 4 5 6  7  8  9  10 11 12 13 }
		 */
		for (int i=0; i<num_knots; i++)
			knots[i]=i;
	}
	/*
	printf("Knots%s: ",u?"u":"v");
	for (int i=0; i<num_knots; i++)
		printf((i!=num_knots-1)?"%f, ":"%f\n", knots[i]);
	 */
}

/* Points on a NURBS surface are defined as an affine combination
 * (sum of coeffs is 1) of points from the control polygon. HOWEVER, the
 * locality property of NURBS dictates that at most $order consecutive
 * control points have nonzero weight at a given curve coordinate u (or v, but
 * we assume u WLOG for the purposes of naming the variable). Therefore
 *     C(u) = \sum_{j=0}^n   Njp(u)*Pj
 *          = \sum_{j=i-p}^j Njp(u)*Pj
 *          = N(i-p)p*P(i-p) + N(i-p+1)p*P(i-p+1) + ... + Nip*Pi
 * for u in knot range [u_i, u_{i+1}) given the m+1 knots
 *     U[] = {u0, u1, ..., um}
 * Arguments:
 * uv = 1:u, 2:v
 *  u = the curve parameter, as above (u is actually v if uv==2)
 * returns: i such that basis functions i-p,i-p+1,...,i are possibly nonzero
 */
int BKE_bspline_nz_basis_range(float u, float *knots, int num_pts, int order)
{
	int p = order-1; /* p is the NURBS degree */
	int n = num_pts-1;
	// Binary Search
 	if (u>=knots[n+1])
		return n;
 	if (u<=knots[p])
		return p;
	int low=p, high=n+1, mid=(low+high)/2;
	while (u<knots[mid] || u>=knots[mid+1]) {
		if (u<knots[mid]) high=mid;
		else              low=mid;
		mid = (low+high)/2;
	}
#ifndef NDEBUG
	// Linear Search
	int ls_ret;
	for (int i=p; i<=n; i++) {
		if (knots[i]<=u && u<knots[i+1]) {
			ls_ret = i;
			break;
		}
	}
	if (mid!=ls_ret) {
		fprintf(stderr, "BS%i[%f,%f) LS%i[%f,%f) MISMATCH!\n",mid,knots[mid],knots[mid+1], ls_ret,knots[ls_ret],knots[ls_ret+1]);
	}
	if (!(p<=ls_ret && ls_ret<=n+1)) {
		fprintf(stderr,"NURBS tess: knot index 2 out of bounds\n");
	}
#endif
	return mid;
}

/* Computes the p+1=order nonvanishing NURBS basis functions at coordinate u.
 * Arguments:
 *  u = the curve parameter, as above (u is actually v if uv==2)
 *  i = such that u_i<=u<u_i+1 = the return value of nurbs_nz_basis_range(u,...)
 *  U = the knot vector
 *  num_pts = number of knots in U
 *  order = the order of the NURBS curve
 *  nd = the number of derivatives to calculate (0 = just regular basis funcs)
 * out = an array to put N(i-p),N(i-p+1),...,N(i) and their derivatives into:
 *  out[0][0]=N(i-p)        out[0][1]=N(i-p+1)        ...   out[0][p]=N(i)
 *  out[1][0]=N'(i-p)       out[1][1]=N'(i-p+1)       ...   out[1][p]=N'(i)
 *  ...
 *  out[nd-1][0]=N'''(i-p)  out[nd-1][1]=N'''(i-p+1)  ...   out[nd-1][p]=N'''(i)
 *                 ^ let ''' stand for differentiation nd-1 times
 * Let N(i,p,u) stand for the ith basis func of order p eval'd at u.
 * Let N(i) be a shorthand when p and u are implicit from context.
 * Efficient algorithm adapted from Piegl&Tiller 1995
 */
void BKE_bspline_basis_eval(float u, int i, float *U, int num_pts, int order, int nd, float out[][NURBS_MAX_ORDER]) {
	int num_knots = num_pts + order;
	int p = order-1; /* p is the NURBS degree */
	int n = num_knots-p; /* = number of control points + 1 */
	double left[NURBS_MAX_ORDER], right[NURBS_MAX_ORDER];
	double ndu[NURBS_MAX_ORDER][NURBS_MAX_ORDER];
	double a[2][NURBS_MAX_ORDER];
	double saved,temp;
	if (u<U[p]) u=U[p];
	if (u>U[num_knots-p-1]) u=U[num_knots-p-1];
	if (i==-1) {
		i = BKE_bspline_nz_basis_range(u, U, num_pts, order);
	}
#ifndef NDEBUG
	if (!(U[i]<=u && u<=U[i+1])) {
		fprintf(stderr, "NURBS tess error: curve argument out of bounds\n");
		return;
	}
	if (!(p<=i && i<=num_knots-p-1 && i<=10)) {
		fprintf(stderr, "NURBS tess error: knot index i out of bounds\n");
		return;
	}
#endif
	
	/* First, compute the 0th derivatives of the basis functions. */
	ndu[0][0] = 1.0;
	for (int j=1; j<=p; j++) {
		/* Compute degree j basis functions eval'd at u */
		/* N(i-j,j,u) N(i-j+1,j,u) ... N(i,j,u) */
		left[j] = u-U[i+1-j];
		right[j] = U[i+j]-u;
		saved = 0;
		for (int r=0; r<j; r++) {
			/* Compute N(i-j+r, j, u) */
			ndu[j][r] = right[r+1]+left[j-r];
			temp = ndu[r][j-1]/ndu[j][r];
			ndu[r][j] = saved+right[r+1]*temp;  /* N(i-p+r,j,u) */
			saved = left[j-r]*temp;
		}
		/* Edge case: prev loop didn't store N(i-j+j,j,u)==N(i,j,u) */
		ndu[j][j] = saved; /* N(i,j,u) */
	}
	for (int j=0; j<=p; j++)
		out[0][j] = ndu[j][p];

	/* Now compute the higher nd derivatives */
	for (int r=0; r<=p; r++) {
		int s1=0, s2=1, j1=0, j2=0;
		a[0][0] = 1.0;
		for (int k=1; k<=nd && k<=n; k++) {
			/* compute kth derivative */
			double d = 0.0;
			int rk=r-k, pk=p-k, j=0;
			if (r>=k) {
				a[s2][0] = a[s1][0]/ndu[pk+1][rk];
				d = a[s2][0]*ndu[rk][pk];
			}
			j1 = (rk>=-1)? 1 : -rk;
			j2 = (r-1<=pk)? k-1 : p-r;
			for (j=j1; j<=j2; j++) {
				a[s2][j] = (a[s1][j]-a[s1][j-1])/ndu[pk+1][rk+j];
				d += a[s2][j]*ndu[rk+j][pk];
			}
			if (r<=pk) {
				a[s2][k] = -a[s1][k-1]/ndu[pk+1][r];
				d += a[s2][k]*ndu[r][pk];
			}
			out[k][r] = d;
			j=s1; s1=s2; s2=j;
		}
	}
	/* fix the normalization of derivatives */
	int r=p;
	for (int k=1; k<=n && k<=nd; k++) {
		for (int j=0; j<=p; j++) {
			out[k][j] *= r;
		}
		r *= (p-k);
	}
}

/* Evaluates a B-Spline curve (and nd of its derivatives) at point u.
 * NOTE: *DOES NOT* perform perspective divide on output 4-vectors. Some algorithms
 * need the information stored in the weights.
 *
 * u: the point to eval the curve at
 * U: the knot vector
 * num_pts: number of control points
 * order: order of the NURBS curve
 * P,stride: P[0], P[stride], ..., P[(num_pts-1)*stride] are the ctrl points
 * nd: number of derivatives to calculate
 * out: out[0], out[1], ..., out[nd] are the evaluated points/derivatives
 */
void BKE_bspline_curve_eval(float u, float *U, int num_pts, int order, BPoint *P, int stride, int nd, BPoint *out)
{
	int p = order-1;
	if (!(nd<=p)) {
		fprintf(stderr, "NURBS eval error: curve requested too many derivatives\n");
		return;
	}
	/* Nonzero basis functions are N(i-p), N(i-p+1), ..., N(i). */
	int i = BKE_bspline_nz_basis_range(u, U, num_pts, order);
	float basisu[NURBS_MAX_ORDER][NURBS_MAX_ORDER];
	BKE_bspline_basis_eval(u, i, U, num_pts, order, nd, basisu);
	for (int d=0; d<=nd; d++) { /* calculate derivative d */
		float accum[4] = {0,0,0,0};
		for (int j=0; j<=p; j++)
			madd_v4_v4fl(accum, P[(i-p+j)*stride].vec, basisu[d][j]);
		madd_v4_v4fl(out[d].vec, accum, 1.0);
	}
}

/* Evaluates a NURBS curve (and nd of its derivatives) at point u.
 * Note: the 0th derivative is trivial (divide by w), the others aren't.
 *  let Cw(u)=(xw,yw,zw,w)  A(u)=(xw,yw,zw)  C(u)=(x,y,z)
 *  C^(k)(u) = (   A^(k)(u) - sum_i=1^k (k choose i)*w^(i)*C^(k-i)   )/w
 */
void BKE_nurbs_curve_eval(float u, float *U, int num_pts, int order, BPoint *P, int stride, int nd, BPoint *out)
{
	/* Get Cw, Cw^(1), ..., Cw^(nd). Cw^(i) gives both A^(i)(u) and w^(i)(u). */
	BKE_bspline_curve_eval(u, U, num_pts, order, P, stride, nd, out);
	for (int k=0; k<=nd; k++) {
		float v[4]; mul_v4_v4fl(v, out[k].vec, 1.0);
		for (int i=1; i<=k; i++) {
			int kCi = BKE_nurb_binom_coeff(k, i);
			float wi = out[i].vec[3];
			madd_v4_v4fl(v, out[k-i].vec, -kCi*wi);
		}
		mul_v3_v3fl(out[k].vec, v, 1.0/out[0].vec[3]);
	}
}

/* Evaluates a B-Spline surface and nd of its partial derivatives at (u,v).
 * (u,v): the point in parameter space being pushed through the map
 * pntsu,orderu,U: # control points, order, knots in the U direction
 * pntsv,orderv,V: # control points, order, knots in the V direction
 * P: points of the control polygon packed in u-major order:
 *     P[0]~u0v0      P[1]~u1v0        ... P[pntsu-1]~u(pntsu-1)v0
 *     P[pntsu]~u0v1  P[pntsu+1]~u1v1  ... P[2*pntsu-1]~u(pntsu-1)v1 ...
 * nd: (#calc'd derivatives in u direction + # calc'd derivatives in v direction <= nd)
 * out: S(u,v),   Su(u,v),Sv(u,v),   Suu(u,v),Suv(u,v),Svv(u,v),  ...
 *     out[nuv*(nuv+1)/2 + nv] is the pt with nuv u+v partials, nv v partials
 */
void BKE_bspline_surf_eval(float u, float v,
						   int pntsu, int orderu, float *U,
						   int pntsv, int orderv, float *V,
						   BPoint *P, int nd, BPoint *out) {
	int pu=orderu-1, pv=orderv-1;
	if (!(nd<=orderu-1 && nd<=orderv-1)) {
		fprintf(stderr, "NURBS eval error: surf requested too many derivatives\n");
		return;
	}
	float Nu[NURBS_MAX_ORDER][NURBS_MAX_ORDER];
	int iu = BKE_bspline_nz_basis_range(u, U, pntsu, orderu);
	BKE_bspline_basis_eval(u, iu, U, pntsu, orderu, nd, Nu);
	float Nv[NURBS_MAX_ORDER][NURBS_MAX_ORDER];
	int iv = BKE_bspline_nz_basis_range(v, V, pntsv, orderv);
	BKE_bspline_basis_eval(v, iv, V, pntsv, orderv, nd, Nv);
	float ivt=0; for (int i=0; i<orderv; i++) ivt+=Nv[0][i];
	float iut=0; for (int i=0; i<orderu; i++) iut+=Nu[0][i];
	
	int outidx=0;
	for (int nuv=0; nuv<=nd; nuv++) {
		for (int nv=0; nv<=nuv; nv++) {
			int nu = nuv-nv;
			/* nu = # derivs in u direction      nv = # derivs in v direction */
			float accum[4] = {0,0,0,0};
			double basis_sum=0;
			for (int i=iu-pu; i<=iu; i++) {
				for (int j=iv-pv; j<=iv; j++) {
					int ii=i-(iu-pu), jj=j-(iv-pv);
					float basis = Nu[nu][ii]*Nv[nv][jj];
					float *ctrlpt = P[pntsu*j+i].vec;
					madd_v4_v4fl(accum, ctrlpt, basis);
					basis_sum += basis;
				}
			}
			mul_v4_v4fl(out[outidx].vec, accum, 1.0);
			outidx++;
		}
	}
}
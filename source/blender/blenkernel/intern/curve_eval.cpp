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
#include <math.h>

#if defined(NURBS_eval_test)
#include "curve_eval.h"
#else
extern "C" {
#include "BKE_curve.h"
#include "DNA_curve_types.h"
#include "BLI_math.h"
}
#endif

int BKE_nurb_binom_coeffs[NURBS_MAX_ORDER][NURBS_MAX_ORDER+1];
static int BKE_nurb_binom_coeff(int k, int i) {
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
		/* On a NURBS curve, points between knots have infinitely many continuous
		 * derivatives. Points *at* knots have p-n continuous derivatives, where
		 * p is the degree of the curve and n is the multiplicity of the knot.
		 * p-n==-1 corresponds to a discontinuity in the curve itself. By ensuring
		 * that n==p (each knot has a multiplicity equal to the degree of a curve)
		 * we ensure that at each knot the curve is
		 *  1. continuous
		 *  2. has a (pontentially) discontinuous 1st derivative
		 * this gives the curve behavior equivalent to that of a Bezier curve.
		 *           |------------------ pnts+order -------------|
		 *           |--ord--||-ord-1-||-ord-1-||-ord-1-||--ord--|
		 * Bezier:  { 0 0 0 0   1 1 1    2 2 2    3 3 3   4 4 4 4 }
		 */
		int v=0;
		for (int i=0; i<order; i++)
			knots[i] = v;
		for (int i=order,reps=0; i<pnts; i++,reps--) {
			if (reps==0) {
				v += 1;
				reps = order-1;
			}
			knots[i] = v;
		}
		v++;
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
	if (!(p<=i && i<=num_knots-p-1)) {
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
			if (!isfinite(temp)) temp=0;
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
				if (!isfinite(a[s2][0])) a[s2][0]=0;
				d = a[s2][0]*ndu[rk][pk];
			}
			j1 = (rk>=-1)? 1 : -rk;
			j2 = (r-1<=pk)? k-1 : p-r;
			for (j=j1; j<=j2; j++) {
				a[s2][j] = (a[s1][j]-a[s1][j-1])/ndu[pk+1][rk+j];
				if (!isfinite(a[s2][j])) a[s2][j]=0;
				d += a[s2][j]*ndu[rk+j][pk];
			}
			if (r<=pk) {
				a[s2][k] = -a[s1][k-1]/ndu[pk+1][r];
				if (!isfinite(a[s2][k])) a[s2][k]=0;
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
 * premultiply_weights: multiply x,y,z coords by w. If in doubt, pass "false".
 */
void BKE_bspline_curve_eval(float u, float *U, int num_pts, int order, BPoint *P, int stride, int nd, BPoint *out, bool premultiply_weights)
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
		for (int j=0; j<=p; j++) {
			float N = basisu[d][j];
			float *pt = P[(i-p+j)*stride].vec;
			float w = (premultiply_weights)? pt[3] : 1.0;
			accum[0] += pt[0]*w*N;
			accum[1] += pt[1]*w*N;
			accum[2] += pt[2]*w*N;
			accum[3] += pt[3]*N;
		}
		mul_v4_v4fl(out[d].vec, accum, 1.0);
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
	BKE_bspline_curve_eval(u, U, num_pts, order, P, stride, nd, out, true);
	for (int k=0; k<=nd; k++) {
		float *Ak = out[k].vec;
		float v[4] = {Ak[0], Ak[1], Ak[2], Ak[3]};
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
 * premultiply_weights: multiply x,y,z coords by w. If in doubt, pass "false".
 * cache: share a BSplineCacheU object to make evaling multiple pts at same u coord efficient
 */
void BKE_bspline_surf_eval(float u, float v,
						   int pntsu, int orderu, float *U,
						   int pntsv, int orderv, float *V,
						   BPoint *P, int nd, BPoint *out, bool premultiply_weights,
						   BSplineCacheU *ucache) {
	int pu=orderu-1, pv=orderv-1;
	if (!(nd<=orderu-1 && nd<=orderv-1)) {
		fprintf(stderr, "NURBS eval error: surf requested too many derivatives\n");
		return;
	}
	float Nu_local[NURBS_MAX_ORDER][NURBS_MAX_ORDER];
	float (*Nu)[NURBS_MAX_ORDER] = Nu_local;
	int iu = -1;
	if (ucache) {
		Nu = ucache->Nu;
		if (ucache->u==u) { // Cache Hit
			iu = ucache->iu;
		} else { // Cache miss
			ucache->u = u;
			iu = ucache->iu = BKE_bspline_nz_basis_range(u, U, pntsu, orderu);
			BKE_bspline_basis_eval(u, iu, U, pntsu, orderu, nd, Nu);
		}
	} else { // No cache
		iu = BKE_bspline_nz_basis_range(u, U, pntsu, orderu);
		BKE_bspline_basis_eval(u, iu, U, pntsu, orderu, nd, Nu);
	}
	float Nv[NURBS_MAX_ORDER][NURBS_MAX_ORDER];
	int iv = BKE_bspline_nz_basis_range(v, V, pntsv, orderv);
	BKE_bspline_basis_eval(v, iv, V, pntsv, orderv, nd, Nv);
	float ivt=0; for (int i=0; i<orderv; i++) ivt+=Nv[0][i];
	float iut=0; for (int i=0; i<orderu; i++) iut+=Nu[0][i];
	
	int outidx=0;
	for (int nuv=0; nuv<=nd; nuv++) { // nuv = (# u derivs)+(#v derivs)
		for (int nv=0; nv<=nuv; nv++) { // nv = #v derivs
			int nu = nuv-nv;
			float accum[4] = {0,0,0,0};
			for (int i=iu-pu; i<=iu; i++) {
				for (int j=iv-pv; j<=iv; j++) {
					// i,j index into the global (0 to pnts{u,v}-1) basis list
					// ii,jj index into the nonzero stretch (N_i-p) to N_i for u in [u_i, u_i+1).)
					int ii=i-(iu-pu), jj=j-(iv-pv);
					float basis = Nu[nu][ii]*Nv[nv][jj];
					float *ctrlpt = P[pntsu*j+i].vec;
					float w = (premultiply_weights)? ctrlpt[3] : 1.0;
					accum[0] += ctrlpt[0]*basis*w;
					accum[1] += ctrlpt[1]*basis*w;
					accum[2] += ctrlpt[2]*basis*w;
					accum[3] += ctrlpt[3]*basis;
				}
			}
			mul_v4_v4fl(out[outidx].vec, accum, 1.0);
			outidx++;
		}
	}
}

#define Skl(k,l) out[((k)+(l))*((k)+(l)+1)/2+(l)].vec
/* Evaluates a NURBS surface and nd of its partial derivatives at (u,v).
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
void BKE_nurbs_surf_eval(float u, float v,
						 int pntsu, int orderu, float *U,
						 int pntsv, int orderv, float *V,
						 BPoint *P, int nd, BPoint *out, BSplineCacheU *ucache) {
	// Let S(nu,nv) denote (d/du)^nu (d/dv)^nv S(u,v)->(x,y,z)
	// Let A(nu,nv) denote (d/du)^nu (d/dv)^nv A(u,v)->(wx,wy,wz)
	// First, calculate A
	BKE_bspline_surf_eval(u,v, pntsu,orderu,U, pntsv,orderv,V, P, nd, out, true, ucache);
	float invw = 1/Skl(0,0)[3];
	int outidx=0;
	for (int nuv=0; nuv<=nd; nuv++) { // nuv = (# u derivs)+(#v derivs)
		for (int nv=0; nv<=nuv; nv++) { // nv = #v derivs
			int nu = nuv-nv;
			int k=nu, l=nv; //  k=(# u derivs)    l=(# v derivs)
			float *Akl = out[outidx].vec;
			for (int i=1; i<=k; i++) {
				float coeff = - BKE_nurb_binom_coeff(k, i) * Skl(i,0)[3];
				madd_v4_v4fl(Akl, Skl(k-i,l), coeff);
			}
			for (int j=1; j<=l; j++) {
				float coeff = - BKE_nurb_binom_coeff(l, j) * Skl(0,j)[3];
				madd_v4_v4fl(Akl, Skl(k,l-j), coeff);
			}
			for (int i=1; i<=k; i++) {
				for (int j=1; j<=l; j++) {
					float coeff = -BKE_nurb_binom_coeff(k,i)*BKE_nurb_binom_coeff(l,j)*Skl(i,j)[3];
					madd_v4_v4fl(Akl, Skl(k-i, l-j), coeff);
				}
			}
			mul_v4_v4fl(Akl, Akl, invw);
			outidx++;
		}
	}
}
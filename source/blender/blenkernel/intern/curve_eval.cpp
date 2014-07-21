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


/* ~~~~~~~~~~~~~~~~~~~~Non Uniform Rational B Spline calculations ~~~~~~~~~~~ */
float nurbs_eps = 1e-5;

void BKE_nurb_knot_calc(int flags, int pnts, int order, float knots[])
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

/* Points on the surface of a NURBS curve are defined as an affine combination
 * (sum of coeffs is 1) of points from the control polygon. HOWEVER, the
 * locality property of NURBS dictates that at most $order consecutive
 * control points are nonzero at a given curve coordinate u (or v, but we assume
 * u WLOG for the purposes of naming the variable). Therefore
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
int BKE_nurbs_nz_basis_range(float u, float *knots, int num_knots, int order)
{
	int p = order-1; /* p is the NURBS degree */
	int m = num_knots-1; /* 0-based index of the last knot */
	int n = m-p-1; /* = number of control points + 1 */
	
	/*
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
	return mid;
	*/
	
}

/* Computes the p+1=order nonvanishing NURBS basis functions at coordinate u.
 * Arguments:
 *  u = the curve parameter, as above (u is actually v if uv==2)
 *  i = the return value of nurbs_nz_basis_range(u,...)
 *  U = the knot vector
 *  num_knots = number of knots in U
 *  nd= the number of derivatives to calculate (0 = just regular basis funcs)
 * out = an array to put N(i-p),N(i-p+1),...,N(i) and their derivatives into
 *  out[0][0]=N(i-p)        out[0][1]=N(i-p+1)        ...   out[0][p]=N(i)
 *  out[1][0]=N'(i-p)       out[1][1]=N'(i-p+1)       ...   out[1][p]=N'(i)
 *  ...
 *  out[nd-1][0]=N'''(i-p)  out[nd-1][1]=N'''(i-p+1)  ...   out[nd-1][p]=N'''(i)
 *                 ^ let ''' stand for differentiation nd-1 times
 * Adapted from Piegl&Tiller 1995
 */
void BKE_nurbs_basis_eval(float u, int i, float *U, int num_knots, int order, int nd, float out[][NURBS_MAX_ORDER]) {
	int p = order-1; /* p is the NURBS degree */
	int n = num_knots-p; /* = number of control points + 1 */
	double left[NURBS_MAX_ORDER], right[NURBS_MAX_ORDER];
	double ndu[NURBS_MAX_ORDER][NURBS_MAX_ORDER];
	double a[2][NURBS_MAX_ORDER];
	double saved,temp;
	if (u<U[p]) u=U[p];
	if (u>U[num_knots-p-2]) u=U[num_knots-p-2];
	if (i==-1) {
		i = BKE_nurbs_nz_basis_range(u, U, num_knots, order);
	}
	
	/* First, compute the 0th derivatives of the basis functions. */
	ndu[0][0] = 1.0;
	for (int j=1; j<=p; j++) {
		left[j] = u-U[i+1-j];
		right[j] = U[i+j]-u;
		saved = 0;
		/* Upper and lower triangles of Piegl&Tiller eval grid */
		for (int r=0; r<j; r++) {
			ndu[j][r] = right[r+1]+left[j-r];
			printf("ndu[%i][%i]=%f  for i=%i\n",j,r,ndu[j][r],i);
			temp = ndu[r][j-1]/ndu[j][r];
			ndu[r][j] = saved+right[r+1]*temp;
			saved = left[j-r]*temp;
		}
		ndu[j][j] = saved;
	}
	for (int j=0; j<=p; j++)
		out[0][j] = ndu[j][p];
	return;
	/* Now compute the higher nd derivatives */
	for (int r=0; r<=p; r++) {
		int s1=0, s2=1, j1=0, j2=0;
		a[0][0] = 1.0;
		for (int k=1; k<=nd && k<=n; k++) {
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
	int r=p;
	for (int k=1; k<=n; k++) {
		for (int j=0; j<=p; j++) {
			out[k][j] *= r;
		}
		r *=(p-k);
	}
}


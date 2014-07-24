//
//  curve_eval.h
//  EvalTest
//
//  Created by Jonathan deWerd on 7/20/14.
//  Copyright (c) 2014 a.b.c. All rights reserved.
//

#ifndef EvalTest_curve_eval_h
#define EvalTest_curve_eval_h

#define CU_NURB_CYCLIC		1
#define CU_NURB_ENDPOINT	2
#define CU_NURB_BEZIER		4

#define NURBS_MAX_ORDER 10

#define STRINGIFY_ARG(x) "" #x
#define STRINGIFY_APPEND(a, b) "" a #b
#define STRINGIFY(x) STRINGIFY_APPEND("", x)

#define _BLI_DUMMY_ABORT() (void)0

#define BLI_assert(a)                                                     \
(void)((!(a)) ?  (                                                        \
(                                                                     \
fprintf(stderr,                                                       \
"BLI_assert failed: %s:%d, %s(), at \'%s\'\n",                    \
__FILE__, __LINE__, __func__, STRINGIFY(a)),                      \
_BLI_DUMMY_ABORT(),                                                   \
NULL)) : NULL)

typedef struct BPoint {
	float vec[4];
	float alfa, weight;		/* alfa: tilt in 3D View, weight: used for softbody goal weight */
	short f1, hide;			/* f1: selection status,  hide: is point hidden or not */
	float radius, pad;		/* user-set radius per point for beveling etc */
} BPoint;

/* Caches the value of basis functions evaluated at a given u coordinate
 * so that a surface patch only has to evaluate once per row. We could do
 * this with columns, too, but at a much worse space/time tradeoff.
 */
typedef struct BSplineCacheU {
	float u;
	int iu;
	float Nu[NURBS_MAX_ORDER][NURBS_MAX_ORDER];
} BSplineCacheU;

extern "C" {
	void BKE_bspline_knot_calc(int flags, int pnts, int order, float knots[]);
	int BKE_bspline_nz_basis_range(float u, float *knots, int num_pts, int order);
	void BKE_bspline_basis_eval(float u, int i, float *U, int num_knots, int order, int nd, float out[][NURBS_MAX_ORDER]);
	void BKE_bspline_curve_eval(float u, float *U, int num_pts, int order, BPoint *P, int stride, int nd, BPoint *out, bool premultiply_weight=false);
	void BKE_nurbs_curve_eval(float u, float *U, int num_pts, int order, BPoint *P, int stride, int nd, BPoint *out);
	void BKE_bspline_surf_eval(float u, float v,
							   int pntsu, int orderu, float *U,
							   int pntsv, int orderv, float *V,
							   BPoint *P, int nd, BPoint *out,
							   bool premultiply_weights=false, BSplineCacheU *ucache=NULL);
	void BKE_nurbs_surf_eval(float u, float v,
							 int pntsu, int orderu, float *U,
							 int pntsv, int orderv, float *V,
							 BPoint *P, int nd, BPoint *out, BSplineCacheU *ucache=NULL);
}

inline void madd_v4_v4fl(float r[4], const float a[4], float f)
{
	r[0] += a[0] * f;
	r[1] += a[1] * f;
	r[2] += a[2] * f;
	r[3] += a[3] * f;
}

inline void mul_v4_v4fl(float r[4], const float a[4], float f)
{
	r[0] = a[0] * f;
	r[1] = a[1] * f;
	r[2] = a[2] * f;
	r[3] = a[3] * f;
}

inline void mul_v3_v3fl(float r[3], const float a[3], float f)
{
	r[0] = a[0] * f;
	r[1] = a[1] * f;
	r[2] = a[2] * f;
}

#endif

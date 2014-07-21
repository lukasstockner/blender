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

void BKE_nurb_knot_calc(int flags, int pnts, int order, float knots[]);
int BKE_nurbs_nz_basis_range(float u, float *knots, int num_knots, int order);
void BKE_nurbs_basis_eval(float u, int i, float *U, int num_knots, int order, int nd, float out[][NURBS_MAX_ORDER]);

#endif

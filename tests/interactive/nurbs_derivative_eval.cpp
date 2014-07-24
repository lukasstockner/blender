//
//  main.cpp
//  EvalTest
//
//  Created by Jonathan deWerd on 7/19/14.
//  Copyright (c) 2014 a.b.c. All rights reserved.
//

#include <iostream>
#include <stdio.h>
#include <string.h>
#include "curve_eval.h"

/*******  Test BSpline Basis Functions + first two derivatives ********/
float knots[] = {0,0,0,0,.3,.6,1,1,1,1};

void test_basis() {
	int num_knots = 10;
	int order = 3;
	int p = order-1;
	float out[NURBS_MAX_ORDER][NURBS_MAX_ORDER];
	float umin=0, umax=1;
	int N = 15;
	float du = (umax-umin)/N;
	
	FILE *of = fopen("/tmp/bspline_basis.txt","w");
	fprintf(of,"knots={");
	for (int i=0; i<num_knots; i++) {
		fprintf(of,(i==num_knots-1)?"%f};\n":"%f,",knots[i]);
	}
	fprintf(of,"p=%i\n",p);
	fprintf(of,"numKnots=%i;\n",num_knots);
	for (int i=0; i<num_knots-order; i++) {
		for (int k=0; k<=2; k++) {
			fprintf(of,"nn[%i,%i,%i] = {",i,p,k);
			for (int uidx=0; uidx<=N; uidx++) {
				float u = (uidx==N)? umax : umin + du*uidx;
				int ii = BKE_bspline_nz_basis_range(u, knots, num_knots, order);
				float Nu = 0;
				if (ii-p<=i && i<=ii) {
					BKE_bspline_basis_eval(u, ii, knots, num_knots, order, 2, out);
					Nu = out[k][i];
				}
				fprintf(of,(uidx!=N)?"{%f,%f},":"{%f,%f}};\n",u,Nu);
			}
		}
	}
}

/*******  Test BSpline Curves (values + first 2 derivatives) ********/
float curveKnots[] = {0,0,0,.25,.5,.75,1,1,1};
float curveCp[6][4] = {{-2.0, 0.0, 0.0, 1.0}, {0.0, 0.0, 0.0, 1.0}, {1.0, 0.0, 0.0, 1.0}, {2.0, 0.0, 1.3202035427093506, 1.0}, {0.9792511463165283, 0.014374732971191406, 1.8168606758117676, 9.309999465942383}, {-0.0020470619201660156, -0.3087310791015625, -0.42721521854400635, 1.0}};
void test_bspline_curve() {
	/* Put control polygon in BPoint format */
	BPoint bp_cpts[25];
	for (int u=0; u<6; u++) {
		mul_v4_v4fl(bp_cpts[u].vec, &curveCp[u][0], 1.0);
	}
	/* Pass it to the evaluator */
	FILE *of = fopen("/tmp/bspline_curve.txt","w");
	double umin=0, umax=1; int nu=30;
	int nd = 2; // Maximum derivative to evaluate
	fprintf(of, "crv={");
	for (int i=0; i<nu; i++) {
		float u = umin + (umax-umin)/(nu-1)*i;
		BPoint out[6];
		BKE_bspline_curve_eval(u, curveKnots, 6, 3, bp_cpts, 1, nd, out);
		fprintf(of, "{");
		for (int d=0; d<nd; d++) {
			fprintf(of,(d==nd-1)?"%f,%f,%f":"%f,%f,%f,",out[d].vec[0],out[d].vec[1],out[d].vec[2]);
		}
		fprintf(of,(i==nu-1)?"}};":"},\n");
	}
	fclose(of);
}

/*******  Test BSpline Surfaces (values + first 2 derivatives) ********/
float surfCp[5][5][4] =
{{{1,1,-0.7417486553512296,1},{1,2,-0.8272445659579368,1},{1,3,0.056384039811391506,1},{1,4,-0.8565668867842464,1},{1,5,-0.027038808836199912,1}},{{2,1,-0.8954639917925951,1},{2,2,0.7766849857222553,10},{2,3,-0.2762882340944248,1},{2,4,0.18616037830975074,1},{2,5,-0.8933549711042139,1}},{{3,1,-0.4470460035460979,1},{3,2,-0.9258719097096733,1},{3,3,-0.5237670863586921,1},{3,4,0.11854173291263326,1},{3,5,0.9615924014809489,1}},{{4,1,0.9195532263713426,1},{4,2,-0.10327619833646784,1},{4,3,-0.16756710093868232,1},{4,4,0.7063963753875271,1},{4,5,-0.05516995763136112,1}},{{5,1,0.9561682326127552,1},{5,2,-0.500642765940114,1},{5,3,0.2473747081580262,1},{5,4,0.10467945795748923,1},{5,5,0.5112014481491505,1}}};
float surfU[] = {0,0,0,0,.5,1,1,1,1};
float surfV[] = {0,0,0,0,.5,1,1,1,1};

void test_bspline_surf() {
	/* Put control mesh in BPoint format */
	BPoint bp_cpts[25];
	for (int u=0; u<5; u++) {
		for (int v=0; v<5; v++) {
			mul_v4_v4fl(bp_cpts[5*v+u].vec, &surfCp[u][v][0], 1.0);
			bp_cpts[5*v+u].vec[3] = 1.0;
		}
	}
	/* Pass it to the evaluator */
	double umin=0, umax=1; int nu=20;
	double vmin=0, vmax=1; int nv=20;
	FILE *of = fopen("/tmp/bspline_surf.txt","w");
	fprintf(of,"surf={");
	for (int i=0; i<nu; i++) {
		for (int j=0; j<nv; j++) {
			fprintf(of,"{");
			float u = umin + (umax-umin)/(nu-1)*i;
			float v = vmin + (vmax-vmin)/(nv-1)*j;
			BPoint out[6];
			BKE_bspline_surf_eval(u,v, 5,4,surfU, 5,4,surfV, bp_cpts, 1, out);
			
			fprintf(of,"%f,%f,%f,  %f,%f,%f,  %f,%f,%f",
					out[0].vec[0], out[0].vec[1], out[0].vec[2],
					out[1].vec[0], out[1].vec[1], out[1].vec[2],
					out[2].vec[0], out[2].vec[1], out[2].vec[2]
					);
			fprintf(of,(i!=nu-1||j!=nv-1)?"},\n":"}\n");
		}
	}
	fprintf(of,"}");
	fclose(of);
}

/*******  Test NURBS Curves (values + first 2 derivatives) ********/
void test_nurbs_curve() {
	/* Put control polygon in BPoint format */
	BPoint bp_cpts[25];
	for (int u=0; u<6; u++) {
		mul_v4_v4fl(bp_cpts[u].vec, &curveCp[u][0], 1.0);
	}
	/* Pass it to the evaluator */
	FILE *of = fopen("/tmp/nurbs_curve.txt","w");
	double umin=0, umax=1; int nu=30;
	int nd = 2; // Maximum derivative to evaluate
	fprintf(of, "crv={");
	for (int i=0; i<nu; i++) {
		float u = umin + (umax-umin)/(nu-1)*i;
		BPoint out[6];
		BKE_nurbs_curve_eval(u, curveKnots, 6, 3, bp_cpts, 1, nd, out);
		fprintf(of, "{");
		for (int d=0; d<nd; d++) {
			fprintf(of,(d==nd-1)?"%f,%f,%f":"%f,%f,%f,",out[d].vec[0],out[d].vec[1],out[d].vec[2]);
		}
		fprintf(of,(i==nu-1)?"}};":"},\n");
	}
	fclose(of);
}

/*******  Test NURBS Surf (values + first 2 derivatives) ********/
void test_nurbs_surf() {
	/* Put control mesh in BPoint format */
	BPoint bp_cpts[25];
	for (int u=0; u<5; u++) {
		for (int v=0; v<5; v++) {
			mul_v4_v4fl(bp_cpts[5*v+u].vec, &surfCp[u][v][0], 1.0);
		}
	}
	/* Pass it to the evaluator */
	double umin=0, umax=1; int nu=20;
	double vmin=0, vmax=1; int nv=20;
	FILE *of = fopen("/tmp/nurbs_surf.txt","w");
	fprintf(of,"surf={");
	for (int i=0; i<nu; i++) {
		for (int j=0; j<nv; j++) {
			fprintf(of,"{");
			float u = umin + (umax-umin)/(nu-1)*i;
			float v = vmin + (vmax-vmin)/(nv-1)*j;
			BPoint out[6];
			BKE_nurbs_surf_eval(u,v, 5,4,surfU, 5,4,surfV, bp_cpts, 1, out);
			
			fprintf(of,"%f,%f,%f,  %f,%f,%f,  %f,%f,%f",
					out[0].vec[0], out[0].vec[1], out[0].vec[2],
					out[1].vec[0], out[1].vec[1], out[1].vec[2],
					out[2].vec[0], out[2].vec[1], out[2].vec[2]
					);
			fprintf(of,(i!=nu-1||j!=nv-1)?"},\n":"}\n");
		}
	}
	fprintf(of,"}");
	fclose(of);
}

int main(int argc, const char * argv[])
{
	test_basis();
	test_bspline_curve();
	test_nurbs_curve();
	test_bspline_surf();
	test_nurbs_surf();
    return 0;
}


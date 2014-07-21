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

int main(int argc, const char * argv[])
{
	float knots[] = {0,0,0,0,1,1,1,1};
	int num_knots = 8;
	int order = 4;
	int p = order-1;
	float out[2][NURBS_MAX_ORDER];
	float umin=0, umax=1;
	int N = 3;
	float du = (umax-umin)/N;
	
	FILE *of = fopen("/tmp/nurbs_eval.txt","w");
	fprintf(of,"knots={");
	for (int i=0; i<num_knots; i++) {
		fprintf(of,(i==num_knots-1)?"%f};\n":"%f,",knots[i]);
	}
	fprintf(of,"p=%i\n",p);
	fprintf(of,"numKnots=%i;\n",num_knots);
	for (int i=0; i<num_knots-order; i++) {
		for (int k=0; k<=1; k++) {
			fprintf(of,"nn[%i,%i,%i] = {",i,p,k);
			for (int uidx=0; uidx<=N; uidx++) {
				float u = (uidx==N)? umax : umin + du*uidx;
				int ii = BKE_nurbs_nz_basis_range(u, knots, num_knots, order);
				float Nu = 0;
				if (ii-p<=i && i<=ii) {
					BKE_nurbs_basis_eval(u, ii, knots, num_knots, order, 1, out);
					Nu = out[k][i];
				}
				fprintf(of,(uidx!=N)?"{%f,%f},":"{%f,%f}};\n",u,Nu);
			}
		}
	}
	printf("!\n");
    return 0;
}


/**
 * $Id: math_base_inline.c 26367 2010-01-28 16:02:26Z blendix $
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: some of this file.
 *
 * ***** END GPL LICENSE BLOCK *****
 * */

#include "BLI_math.h"

#ifndef BLI_MATH_GEOM_INLINE
#define BLI_MATH_GEOM_INLINE

/****************************** Spherical Harmonics **************************/

MINLINE void copy_sh_sh(float r[9], float a[9])
{
	memcpy(r, a, sizeof(float)*9);
}

MINLINE void mul_sh_fl(float r[9], float f)
{
	int i;

	for(i=0; i<9; i++)
		r[i] *= f;
}

MINLINE void add_sh_shsh(float r[9], float a[9], float b[9])
{
	int i;

	for(i=0; i<9; i++)
		r[i]= a[i] + b[i];
}


MINLINE float eval_shv3(float sh[9], float v[3])
{
	/* See formula (13) in:
	   "An Efficient Representation for Irradiance Environment Maps" */
	static const float c1 = 0.429043f, c2 = 0.511664f, c3 = 0.743125f;
	static const float c4 = 0.886227f, c5 = 0.247708f;
	float x, y, z, sum;

	x= v[0];
	y= v[1];
	z= v[2];

	sum= c1*sh[8]*(x*x - y*y);
	sum += c3*sh[6]*z*z;
	sum += c4*sh[0];
	sum += -c5*sh[6];
	sum += 2.0f*c1*(sh[4]*x*y + sh[7]*x*z + sh[5]*y*z);
	sum += 2.0f*c2*(sh[3]*x + sh[1]*y + sh[2]*z);

	return sum;
}

MINLINE void disc_to_sh(float r[9], float n[3], float area)
{
	/* See formula (3) in:
	   "An Efficient Representation for Irradiance Environment Maps" */
	float sh[9], x, y, z;

	x= n[0];
	y= n[1];
	z= n[2];

	sh[0]= 0.282095f;

	sh[1]= 0.488603f*y;
	sh[2]= 0.488603f*z;
	sh[3]= 0.488603f*x;
	
	sh[4]= 1.092548f*x*y;
	sh[5]= 1.092548f*y*z;
	sh[6]= 0.315392f*(3.0f*z*z - 1.0f);
	sh[7]= 1.092548f*x*z;
	sh[8]= 0.546274f*(x*x - y*y);

	mul_sh_fl(sh, area);
	copy_sh_sh(r, sh);
}

#endif /* BLI_MATH_GEOM_INLINE */


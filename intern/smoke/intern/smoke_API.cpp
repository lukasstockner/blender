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
 * The Original Code is Copyright (C) 2009 by Daniel Genrich
 * All rights reserved.
 *
 * Contributor(s): Daniel Genrich
 *                 Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file smoke/intern/smoke_API.cpp
 *  \ingroup smoke
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "smoke.h"

// #define FLUID_3D void
#define WTURBULENCE void

using namespace DDF;

// y in smoke is z in blender
extern "C" FLUID_3D *smoke_init(int *res, float *p0, float dtdef)
{
	res[0] = 200;
	res[1] = 80;
	res[2] = 150;
	FLUID_3D *ddf = new FLUID_3D(res);
	ddf->init();
	delete ddf;
	return NULL;
}

extern "C" WTURBULENCE *smoke_turbulence_init(int *res, int amplify, int noisetype)
{
	return NULL;
}

extern "C" void smoke_free(FLUID_3D *fluid)
{
	delete fluid;
	fluid = NULL;
}

extern "C" void smoke_turbulence_free(WTURBULENCE *wt)
{
	 delete wt;
	 wt = NULL;
}

extern "C" size_t smoke_get_index(int x, int max_x, int y, int max_y, int z /*, int max_z */)
{
	// // const int index = x + y * smd->res[0] + z * smd->res[0]*smd->res[1];
	return x + y * max_x + z * max_x*max_y;
}

extern "C" size_t smoke_get_index2d(int x, int max_x, int y /*, int max_y, int z, int max_z */)
{
	return x + y * max_x;
}

extern "C" void smoke_step(FLUID_3D *fluid, float dtSubdiv)
{
	
}

extern "C" void smoke_turbulence_step(WTURBULENCE *wt, FLUID_3D *fluid)
{
	
}

extern "C" void smoke_initBlenderRNA(FLUID_3D *fluid, float *alpha, float *beta, float *dt_factor, float *vorticity, int *border_colli)
{
	
}

extern "C" void smoke_dissolve(FLUID_3D *fluid, int speed, int log)
{
	
}

extern "C" void smoke_dissolve_wavelet(WTURBULENCE *wt, int speed, int log)
{
	
}

extern "C" void smoke_initWaveletBlenderRNA(WTURBULENCE *wt, float *strength)
{

}

template < class T > inline T ABS( T a )
{
	return (0 < a) ? a : -a ;
}

extern "C" void smoke_export(FLUID_3D *fluid, float *dt, float *dx, float **dens, float **densold, float **heat, float **heatold, float **vx, float **vy, float **vz, float **vxold, float **vyold, float **vzold, unsigned char **obstacles)
{
	
}

extern "C" void smoke_turbulence_export(WTURBULENCE *wt, float **dens, float **densold, float **tcu, float **tcv, float **tcw)
{
	
}

extern "C" float *smoke_get_density(FLUID_3D *fluid)
{
	return NULL;
}

extern "C" float *smoke_get_heat(FLUID_3D *fluid)
{
	return NULL;
}

extern "C" float *smoke_get_velocity_x(FLUID_3D *fluid)
{
	return NULL;
}

extern "C" float *smoke_get_velocity_y(FLUID_3D *fluid)
{
	return NULL;
}

extern "C" float *smoke_get_velocity_z(FLUID_3D *fluid)
{
	return NULL;
}

extern "C" float *smoke_get_force_x(FLUID_3D *fluid)
{
	return NULL;
}

extern "C" float *smoke_get_force_y(FLUID_3D *fluid)
{
	return NULL;
}

extern "C" float *smoke_get_force_z(FLUID_3D *fluid)
{
	return NULL;
}

extern "C" float *smoke_turbulence_get_density(WTURBULENCE *wt)
{
	return NULL;
}

extern "C" void smoke_turbulence_get_res(WTURBULENCE *wt, int *res)
{
	
}

extern "C" unsigned char *smoke_get_obstacle(FLUID_3D *fluid)
{
	return NULL;
}

extern "C" void smoke_get_ob_velocity(FLUID_3D *fluid, float **x, float **y, float **z)
{

}

extern "C" unsigned char *smoke_get_obstacle_anim(FLUID_3D *fluid)
{
	return NULL;
}

extern "C" void smoke_turbulence_set_noise(WTURBULENCE *wt, int type)
{

}

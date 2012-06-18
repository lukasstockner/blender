/** \file smoke/intern/FLUID_3D_SOLVERS.cpp
*  \ingroup smoke
*/
//////////////////////////////////////////////////////////////////////
// This file is part of Wavelet Turbulence.
// 
// Wavelet Turbulence is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// Wavelet Turbulence is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with Wavelet Turbulence.  If not, see <http://www.gnu.org/licenses/>.
// 
// Copyright 2008 Theodore Kim and Nils Thuerey
// 
// FLUID_3D.cpp: implementation of the FLUID_3D class.
//
//////////////////////////////////////////////////////////////////////
// Both solvers optimized by merging loops and precalculating
// stuff used in iteration loop.
//		- MiikaH
//////////////////////////////////////////////////////////////////////

#include "FLUID_3D.h"
#include <cstring>
#define SOLVER_ACCURACY 1e-06

#include <iostream>
#include <fstream>

//////////////////////////////////////////////////////////////////////
// solve the heat equation with CG
//////////////////////////////////////////////////////////////////////
void FLUID_3D::solveHeat(float* field, float* b, unsigned char* skip)
{
	int x, y, z;
	const int twoxr = 2 * _xRes;
	size_t index;
	const float heatConst = _dt * _heatDiffusion / (_dx * _dx);
	float *_q, *_residual, *_direction, *_Acenter;

	// i = 0
	int i = 0;

	_residual     = new float[_totalCells]; // set 0
	_direction    = new float[_totalCells]; // set 0
	_q            = new float[_totalCells]; // set 0
	_Acenter       = new float[_totalCells]; // set 0

	memset(_residual, 0, sizeof(float)*_totalCells);
	memset(_q, 0, sizeof(float)*_totalCells);
	memset(_direction, 0, sizeof(float)*_totalCells);
	memset(_Acenter, 0, sizeof(float)*_totalCells);

	float deltaNew = 0.0f;

	// r = b - Ax
	index = _slabSize + _xRes + 1;
	for (z = 1; z < _zRes - 1; z++, index += twoxr)
		for (y = 1; y < _yRes - 1; y++, index += 2)
			for (x = 1; x < _xRes - 1; x++, index++)
			{
				// if the cell is a variable
				_Acenter[index] = 1.0f;
				if (!skip[index])
				{
					// set the matrix to the Poisson stencil in order
					if (!skip[index + 1]) _Acenter[index] += heatConst;
					if (!skip[index - 1]) _Acenter[index] += heatConst;
					if (!skip[index + _xRes]) _Acenter[index] += heatConst;
					if (!skip[index - _xRes]) _Acenter[index] += heatConst;
					if (!skip[index + _slabSize]) _Acenter[index] += heatConst;
					if (!skip[index - _slabSize]) _Acenter[index] += heatConst;

					_residual[index] = b[index] - (_Acenter[index] * field[index] + 
						field[index - 1] * (skip[index - 1] ? 0.0 : -heatConst) + 
						field[index + 1] * (skip[index + 1] ? 0.0 : -heatConst) +
						field[index - _xRes] * (skip[index - _xRes] ? 0.0 : -heatConst) + 
						field[index + _xRes] * (skip[index + _xRes] ? 0.0 : -heatConst) +
						field[index - _slabSize] * (skip[index - _slabSize] ? 0.0 : -heatConst) + 
						field[index + _slabSize] * (skip[index + _slabSize] ? 0.0 : -heatConst));
				}
				else
				{
					_residual[index] = 0.0f;
				}

				_direction[index] = _residual[index];
				deltaNew += _residual[index] * _residual[index];
			}


			// While deltaNew > (eps^2) * delta0
			const float eps  = SOLVER_ACCURACY;
			float maxR = 2.0f * eps;
			while ((i < _iterations) && (maxR > eps))
			{
				// q = Ad
				float alpha = 0.0f;

				index = _slabSize + _xRes + 1;
				for (z = 1; z < _zRes - 1; z++, index += twoxr)
					for (y = 1; y < _yRes - 1; y++, index += 2)
						for (x = 1; x < _xRes - 1; x++, index++)
						{
							// if the cell is a variable
							if (!skip[index])
							{

								_q[index] = (_Acenter[index] * _direction[index] + 
									_direction[index - 1] * (skip[index - 1] ? 0.0 : -heatConst) + 
									_direction[index + 1] * (skip[index + 1] ? 0.0 : -heatConst) +
									_direction[index - _xRes] * (skip[index - _xRes] ? 0.0 : -heatConst) + 
									_direction[index + _xRes] * (skip[index + _xRes] ? 0.0 : -heatConst) +
									_direction[index - _slabSize] * (skip[index - _slabSize] ? 0.0 : -heatConst) + 
									_direction[index + _slabSize] * (skip[index + _slabSize] ? 0.0 : -heatConst));
							}
							else
							{
								_q[index] = 0.0f;
							}
							alpha += _direction[index] * _q[index];
						}

						if (fabs(alpha) > 0.0f)
							alpha = deltaNew / alpha;

						float deltaOld = deltaNew;
						deltaNew = 0.0f;

						maxR = 0.0f;

						index = _slabSize + _xRes + 1;
						for (z = 1; z < _zRes - 1; z++, index += twoxr)
							for (y = 1; y < _yRes - 1; y++, index += 2)
								for (x = 1; x < _xRes - 1; x++, index++)
								{
									field[index] += alpha * _direction[index];

									_residual[index] -= alpha * _q[index];
									maxR = (_residual[index] > maxR) ? _residual[index] : maxR;

									deltaNew += _residual[index] * _residual[index];
								}

								float beta = deltaNew / deltaOld;

								index = _slabSize + _xRes + 1;
								for (z = 1; z < _zRes - 1; z++, index += twoxr)
									for (y = 1; y < _yRes - 1; y++, index += 2)
										for (x = 1; x < _xRes - 1; x++, index++)
											_direction[index] = _residual[index] + beta * _direction[index];


								i++;
			}
			// cout << i << " iterations converged to " << maxR << endl;

			if (_residual) delete[] _residual;
			if (_direction) delete[] _direction;
			if (_q)       delete[] _q;
			if (_Acenter)  delete[] _Acenter;
}


#define SMOKE_LOOP \
		index = _slabSize + _xRes + 1; \
		for (z = 1; z < _zRes - 1; z++, index += 2 * _xRes) \
			for (y = 1; y < _yRes - 1; y++, index += 2) \
				for (x = 1; x < _xRes - 1; x++, index++)

void FLUID_3D::solvePressurePre(float* _x, float* b, unsigned char* skip)
{
	int x, y, z;
	size_t index;
	float *_q, *_Precond, *_z, *_r, *_p;

	float alpha = 0.0f, beta = 0.0f;

	float eps  = SOLVER_ACCURACY;
	float currentR = 0.0f;
	float targetR = 0.0f;

	int k = 0;

	_r		      = new float[_totalCells]; // set 0
	_p			  = new float[_totalCells]; // set 0
	_z			  = new float[_totalCells]; // set 0

	// q = A p
	_q            = new float[_totalCells]; // set 0

	_Precond	  = new float[_totalCells]; // set 0

	memset(_r, 0, sizeof(float)*_xRes*_yRes*_zRes);
	memset(_z, 0, sizeof(float)*_xRes*_yRes*_zRes);
	memset(_q, 0, sizeof(float)*_xRes*_yRes*_zRes);
	memset(_p, 0, sizeof(float)*_xRes*_yRes*_zRes);
	memset(_Precond, 0, sizeof(float)*_xRes*_yRes*_zRes);

	// delta = r^T z
	float delta = 0.0f;

	// deltaNew = r^T_k+1 z_k+1
	float deltaNew = 0.0f;

	// r_0 = b - A x_0
	SMOKE_LOOP
	{
		// if the cell is a variable
		float Acenter = 0.0f;
		if (!skip[index])
		{
			// set the matrix to the Poisson stencil in order
			if (!skip[index + 1]) Acenter += 1.;
			if (!skip[index - 1]) Acenter += 1.;
			if (!skip[index + _xRes]) Acenter += 1.;
			if (!skip[index - _xRes]) Acenter += 1.;
			if (!skip[index + _slabSize]) Acenter += 1.;
			if (!skip[index - _slabSize]) Acenter += 1.;

			_r[index] = b[index] - (Acenter * _x[index] +  
				_x[index - 1] * (skip[index - 1] ? 0.0 : -1.0f)+ 
				_x[index + 1] * (skip[index + 1] ? 0.0 : -1.0f)+
				_x[index - _xRes] * (skip[index - _xRes] ? 0.0 : -1.0f)+ 
				_x[index + _xRes] * (skip[index + _xRes] ? 0.0 : -1.0f)+
				_x[index - _slabSize] * (skip[index - _slabSize] ? 0.0 : -1.0f)+ 
				_x[index + _slabSize] * (skip[index + _slabSize] ? 0.0 : -1.0f) );
		}
		else
		{
			_r[index] = 0.0f;
		}

		// P^-1
		if(Acenter < 1.0)
			_Precond[index] = 0.0;
		else
			_Precond[index] = 1.0 / Acenter;

		// z_0 = P^-1 * r_0
		_z[index] = _Precond[index] * _r[index];

		// p = z
		_p[index] = _z[index];

		targetR += b[index] * b[index];
	}

	targetR = targetR * eps * eps;

	while (k < _iterations)
	{
		alpha = 0.0f;
		delta = 0.0f;

		SMOKE_LOOP
		{
			// if the cell is a variable
			float Acenter = 0.0f;
			if (!skip[index])
			{
				// set the matrix to the Poisson stencil in order
				if (!skip[index + 1]) Acenter += 1.;
				if (!skip[index - 1]) Acenter += 1.;
				if (!skip[index + _xRes]) Acenter += 1.;
				if (!skip[index - _xRes]) Acenter += 1.;
				if (!skip[index + _slabSize]) Acenter += 1.;
				if (!skip[index - _slabSize]) Acenter += 1.;

				_q[index] = Acenter * _p[index] +  
					_p[index - 1] * (skip[index - 1] ? 0.0 : -1.0f) + 
					_p[index + 1] * (skip[index + 1] ? 0.0 : -1.0f) +
					_p[index - _xRes] * (skip[index - _xRes] ? 0.0 : -1.0f) + 
					_p[index + _xRes] * (skip[index + _xRes] ? 0.0 : -1.0f)+
					_p[index - _slabSize] * (skip[index - _slabSize] ? 0.0 : -1.0f) + 
					_p[index + _slabSize] * (skip[index + _slabSize] ? 0.0 : -1.0f);
			}
			else
			{
				_q[index] = 0.0f;
			}

			// p^T A p
			alpha += _p[index] * _q[index];

			// r^T z
			delta += _r[index] * _z[index];
		}

		// alpha = r^T * z / p^T A p		
		alpha = delta / alpha;

		currentR = 0.0f;

		SMOKE_LOOP
		{
			// x_k+1 = x_k + alpha * p
			_x[index] += alpha * _p[index];

			// r_k+1 = r_k - alpha A p
			_r[index] -= alpha * _q[index];

			currentR += _r[index] * _r[index];
		}

		//if (r_k+1 > EPSILON) exit
		if(currentR < targetR)
			break;

		deltaNew = 0.0f;

		SMOKE_LOOP
		{
			// z_k+1 = P^-1 r_k+1
			_z[index] = _Precond[index] * _r[index];

			deltaNew += _z[index] * _r[index];
		}

		// beta = z^T_k+1 r_k+1 / z^T r
		beta = deltaNew / delta;	

		// p_k+1 = z + beta * p
		SMOKE_LOOP
		{
			_p[index] = _z[index] + beta * _p[index];
		}

		k++;
	}
	// tol_error = sqrt(residualNorm2 / rhsNorm2); FROM Eigen3
	cout << k << " iterations converged to " << sqrt(currentR / targetR) << endl;

	if (_Precond) delete[] _Precond;
	if (_r) delete[] _r;
	if (_p) delete[] _p;
	if (_z) delete[] _z;
	if (_q) delete[] _q;
}

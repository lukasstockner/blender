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
 * Contributor(s): Alexander Pinzon
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/bmesh/operators/bmo_deform_utils.cpp
 *  \ingroup bmesh
 *
 * Deform Laplacian Utilities.
 */

#include "bmo_deform_utils.h"
#include <Eigen/Dense>
#include <Eigen/Core>
#include <Eigen/SVD>
#include <Eigen/Sparse>
#include <Eigen/IterativeLinearSolvers>
#include <Eigen/Eigenvalues>
#include <vector>
#include <iostream>
#include <float.h>

using namespace Eigen;

typedef float ScalarType;
typedef Eigen::SparseMatrix<ScalarType> spMatrixXd_type;
typedef Eigen::MatrixXf MatrixXd_type;
typedef Eigen::VectorXf VectorXd_type;
typedef Eigen::Triplet<ScalarType> TripletD_type;
typedef std::vector<TripletD_type> TripletDList_type;


inline spMatrixXd_type * convert_void_to_spMatrixXd_type(vptrSpMatrixD m)
{
	return static_cast < spMatrixXd_type *> (m);
}

inline MatrixXd_type * convert_void_to_matrixdXd_type(vptrMatrixD m)
{
	return static_cast < MatrixXd_type *> (m);
}

inline VectorXd_type * convert_void_to_vectord(vptrVectorD b)
{
	return static_cast < VectorXd_type *> (b);
}

inline TripletDList_type * convert_void_to_triplet(vptrTripletD t)
{
	return static_cast < TripletDList_type *> (t);
}

vptrSpMatrixD new_spmatrix(int rows, int cols)
{
	spMatrixXd_type * m = new spMatrixXd_type(rows, cols);
	return m;
}

void delete_spmatrix(vptrSpMatrixD m)
{
	if ( m != NULL)	{
		spMatrixXd_type * spM = convert_void_to_spMatrixXd_type(m);
		if ( spM != NULL)	{
			delete spM;
			spM = NULL;
		}
	}
	m = NULL;
}

void set_spmatrix(vptrSpMatrixD m, int row, int col, float      val)
{
	spMatrixXd_type * spM = convert_void_to_spMatrixXd_type(m);
	spM->coeffRef(row, col) = val;	
}

void add_spmatrix(vptrSpMatrixD m, int row, int col, float      val)
{
	spMatrixXd_type * spM = convert_void_to_spMatrixXd_type(m);
	spM->coeffRef(row, col) += val;
}

void set_spmatrix_from_triplets(vptrSpMatrixD m, vptrTripletD t)
{
	spMatrixXd_type * spM = convert_void_to_spMatrixXd_type(m);
	TripletDList_type * tp = convert_void_to_triplet(t);
	spM->setFromTriplets(tp->begin(), tp->end());
}

vptrMatrixD new_matrixd(int rows, int cols)
{
	MatrixXd_type * m = new MatrixXd_type (rows, cols);
	m->setZero(rows, cols);
	return m;
}
void delete_matrixd(vptrMatrixD m)
{
	if ( m != NULL)	{
		MatrixXd_type * Ma = convert_void_to_matrixdXd_type(m);
		if ( Ma != NULL)	{
			delete Ma;
			Ma = NULL;
		}
	}
	m = NULL;
}
void set_matrixd(vptrMatrixD m, int row, int col, float val)
{
	MatrixXd_type * Ma = convert_void_to_matrixdXd_type(m);
	(*Ma)(row, col) = val;
}
void add_matrixd(vptrMatrixD m, int row, int col, float val)
{
	MatrixXd_type * Ma = convert_void_to_matrixdXd_type(m);
	(*Ma)(row, col) += val;

}

void compute_delta_rotations_matrixd(vptrMatrixD m, vptrMatrixD td, float dx, float dy, float dz)
{
	MatrixXd_type * m2 = convert_void_to_matrixdXd_type(m);
	MatrixXd_type m3(*m2);
	MatrixXd_type m4 = m3.transpose()*m3;

	JacobiSVD<MatrixXd_type> svd(m4, ComputeFullU | ComputeFullV);
	typedef JacobiSVD<MatrixXd_type>::SingularValuesType SingularValuesType;
	const SingularValuesType singVals = svd.singularValues();
	SingularValuesType invSingVals = singVals;
	for(int i=0; i<singVals.rows(); i++) {
		if(singVals(i) <= std::numeric_limits<ScalarType>::epsilon()) {
			invSingVals(i) = 0.0; 
		}
		else {
			invSingVals(i) = 1.0 / invSingVals(i);
		}
	}
	MatrixXd_type Minv =	MatrixXd_type(svd.matrixV() *
							invSingVals.asDiagonal() *
							svd.matrixU().transpose()) * m3.transpose();

	MatrixXd_type tdelta(3, m2->rows());
	tdelta.setZero(3, m2->rows());
	for(int j=0; j<m2->rows(); j++){
		tdelta(0, j) = tdelta(0, j) + dx*Minv(0, j) - dy*Minv(3, j) + dz*Minv(2, j);
		tdelta(1, j) = tdelta(1, j) + dx*Minv(3, j) + dy*Minv(0, j) - dz*Minv(1, j);
		tdelta(2, j) = tdelta(2, j) - dx*Minv(2, j) + dy*Minv(1, j) + dz*Minv(0, j);
	}
	MatrixXd_type * td2 = convert_void_to_matrixdXd_type(td);
	*td2 = tdelta;
}

vptrVectorD new_vectord(int size)
{
	VectorXd_type * m = new VectorXd_type(size);
	m->setZero(size);
	return m;
}

void delete_vectord(vptrVectorD b)
{
	if ( b != NULL)	{
		VectorXd_type * spB = convert_void_to_vectord(b);
		if ( spB != NULL)	{
			delete spB;
			spB = NULL;
		}
	}
	b = NULL;
}

void set_vectord(vptrVectorD b, int index, float      val)
{
	VectorXd_type * spB = convert_void_to_vectord(b);
	(*spB)(index) = val;	
}

void add_vectord(vptrVectorD b, int index, float      val)
{
	VectorXd_type * spB = convert_void_to_vectord(b);
	(*spB)(index) += val;	
}

vptrTripletD new_triplet(int size) 
{
	TripletDList_type * t = new TripletDList_type(size);
	return t;
}
void delete_triplet(vptrTripletD b)
{
	if ( b != NULL)	{
		TripletDList_type * spB = convert_void_to_triplet(b);
		if ( spB != NULL)	{
			delete spB;
			spB = NULL;
		}
	}
	b = NULL;
}
void push_back_triplet(vptrTripletD b, int row, int col, float      val)
{
	ScalarType value = (ScalarType)val;
	TripletDList_type * tp = convert_void_to_triplet(b);
	tp->push_back(TripletD_type(row,col,value));
}

void solve_system(vptrSpMatrixD m, vptrVectorD b, vptrVectorD x)
{

	spMatrixXd_type * spM = convert_void_to_spMatrixXd_type(m);
	VectorXd_type * spB = convert_void_to_vectord(b);
	VectorXd_type * spX = convert_void_to_vectord(x);

	spMatrixXd_type A(spM->cols(), spM->rows());
	spMatrixXd_type Mtrans((*spM).transpose());
	A = (Mtrans*(*spM));
	BiCGSTAB<spMatrixXd_type> solver(A);
	
	VectorXd_type B = (*spM).transpose()*(*spB);
	VectorXd_type  y = solver.solve(B);

	*spX = y;	
	
}

void print_vectord(vptrVectorD b)
{
	std::cout<< (*convert_void_to_vectord(b));
}

float get_vectord(vptrVectorD b, int index)
{
	VectorXd_type * spB = convert_void_to_vectord(b);
	return (*spB)(index);
}

void print_matrixd(vptrMatrixD m)
{
	MatrixXd_type * spm = convert_void_to_matrixdXd_type(m);
	std::cout<<(*spm);
}

float get_matrixd(vptrMatrixD m, int row, int col)
{	
	MatrixXd_type * mnm = convert_void_to_matrixdXd_type(m);
	return (*mnm)(row, col);
}

void print_spmatrixd(vptrSpMatrixD m)
{
	spMatrixXd_type * spM = convert_void_to_spMatrixXd_type(m);
	std::cout<<(* spM);
}
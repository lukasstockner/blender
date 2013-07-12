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

/** \file blender/bmesh/operators/bmo_deform_utils.h
 *  \ingroup bmesh
 *
 * Deform Laplacian Utilities.
 */
#ifndef __BMO_DEFORM_UTILS_H__
#define __BMO_DEFORM_UTILS_H__

#ifdef __cplusplus
extern "C" {
#endif

	typedef void* vptrSpMatrixD;
	typedef void* vptrMatrixD;
	typedef void* vptrVectorD;
	typedef void* vptrTripletD;

	vptrSpMatrixD new_spmatrix(int rows, int cols);
	void delete_spmatrix(vptrSpMatrixD m);
	void set_spmatrix(vptrSpMatrixD m, int row, int col, float      val);
	void add_spmatrix(vptrSpMatrixD m, int row, int col, float      val);
	void set_spmatrix_from_triplets(vptrSpMatrixD m, vptrTripletD t);
	void solve_system(vptrSpMatrixD m, vptrMatrixD b, vptrMatrixD x);
	void print_spmatrixd(vptrSpMatrixD m);

	vptrMatrixD new_matrixd(int rows, int cols);
	void delete_matrixd(vptrMatrixD m);
	void set_matrixd(vptrMatrixD m, int row, int col, float val);
	void add_matrixd(vptrMatrixD m, int row, int col, float val);
	float get_matrixd(vptrMatrixD m, int row, int col);
	void compute_delta_rotations_matrixd(vptrMatrixD m, vptrMatrixD mi, float dex, float dey, float dez);
	void print_matrixd(vptrMatrixD m);
	
	vptrMatrixD new_vectord(int size);
	void delete_vectord(vptrMatrixD b);
	void set_vectord(vptrMatrixD b, int index, float val);
	void add_vectord(vptrMatrixD b, int index, float val);
	float get_vectord(vptrMatrixD b, int index);
	void print_vectord(vptrMatrixD b);
	
	vptrTripletD new_triplet(int size);
	void delete_triplet(vptrTripletD b);
	void push_back_triplet(vptrTripletD b, int row, int col, float val);

#ifdef __cplusplus
};
#endif

#endif //__BMO_DEFORM_UTILS_H__

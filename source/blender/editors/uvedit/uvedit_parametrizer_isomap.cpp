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
 * Contributor(s): Antony Riakiotakis
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <Eigen/Dense>
#include "uvedit_parametrizer_isomap.h"
#include <vector>
#include <iostream>
#include "BLI_math_base.h"

using namespace std;
using namespace Eigen;

/**
  containt the Eigen solver and does the actual solving
*/
class IsomapSolver {
	public:
		IsomapSolver (int nverts);
		~IsomapSolver () {}
		int solve(float dist_matrix[]);
		void load_uv_solution(int index, float uv[2]);

	private:
		int size;
		SelfAdjointEigenSolver <MatrixXf> eigensolver;
};


static IsomapSolver *solver = NULL;



/************************ Solver Implementation *********************************/
IsomapSolver::IsomapSolver(int nverts): size(nverts), eigensolver()
{
}

int IsomapSolver::solve(float dist_matrix[])
{
	/* use Map class to reuse memory from C */
	Map <MatrixXf> map_matrix(dist_matrix, size, size);
	MatrixXf final;
	MatrixXf centering_transform;

	centering_transform.setConstant(size, size, 1.0/size);
	centering_transform = MatrixXf::Identity(size, size) - centering_transform;

	final = -0.5 * centering_transform * map_matrix * centering_transform;

	eigensolver.compute(final);

	#ifdef UNWRAP_DEBUG
	cout << map_matrix << endl << endl;
	cout << final << endl << endl;
	#endif

	if (eigensolver.info() != Success) {
		cout << "isomap solver failure" << endl;
		return false;
	}

	#ifdef UNWRAP_DEBUG
	cout << eigensolver.eigenvectors() << endl << endl;


	cout << endl << "eigenvalues" << endl << eigensolver.eigenvalues() << endl;
	cout << endl << "UVs:" << endl;
	#endif
	return true;
}

void IsomapSolver::load_uv_solution(int index, float uv[2])
{
	float eigenvalue1 = eigensolver.eigenvalues()(size - 1);
	float eigenvalue2 = eigensolver.eigenvalues()(size - 2);

	uv[0] = eigensolver.eigenvectors()(index, size - 1)*sqrtf(eigenvalue1);
	uv[1] = eigensolver.eigenvectors()(index, size - 2)*sqrtf(eigenvalue2);

	#ifdef UNWRAP_DEBUG
	cout << index << ' ' << uv[0] << ' ' << uv[1] << endl;
	cout << index << ' ' << eigensolver.eigenvectors()(index, size - 1)
	     << ' ' << eigensolver.eigenvectors()(index, size - 2) << endl;
	#endif
}

void param_isomap_new_solver(int nverts)
{
	solver = new IsomapSolver(nverts);
}

void param_isomap_delete_solver(void)
{
	delete solver;
}

int param_isomap_solve(float dist_matrix[]) {
	return solver->solve(dist_matrix);
}

void param_isomap_load_uv_solution(int index, float uv[2])
{
	solver->load_uv_solution(index, uv);
}

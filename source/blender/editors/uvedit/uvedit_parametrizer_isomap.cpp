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

using namespace std;

/**
  containt the Eigen solver and does the actual solving
*/
class IsomapSolver {
	public:
		IsomapSolver (int nverts){}
		~IsomapSolver () {}
};


/**
  maintain the pool of solvers for each chart
*/
class SolverPool {
	public:
		SolverPool() {}
		~SolverPool ();
		IsomapSolver *getSolverAtIndex(int index) {return chart_solvers[index];}
		int addNewSolver(int nverts);
	private:
		vector <IsomapSolver *> chart_solvers;
};

static SolverPool *solver_pool = NULL;



/************************ SolverPool Implementation *********************************/
SolverPool::~SolverPool ()
{
	for (int i = 0; i < chart_solvers.size(); i++) {
		delete chart_solvers[i];
	}
}

int SolverPool::addNewSolver(int nverts)
{
	chart_solvers.push_back(new IsomapSolver(nverts));
	return chart_solvers.size() - 1;
}


int param_new_isomap_solver (int nverts)
{
	return solver_pool->addNewSolver(nverts);
}

void param_new_solver_pool (void)
{
	solver_pool = new SolverPool;
}

void param_delete_solver_pool(void)
{
	delete solver_pool;
	solver_pool = 0;
}

#ifndef DDF_SMOKE_H
#define DDF_SMOKE_H

#include "globals.h"
#include "fluidsolver.h"

#include "solverparams.h"
#include "paramset.h"
#include "solverplugin.h"
#include "solverinit.h"

namespace DDF
{
	class FLUID_3D
	{
		private:
			vector<SolverObject*> _solvers;

			/* DG: use 'public' for variables shared with blender ! */

		private:
			// functions taken over from standard main
			bool advanceAllSolvers()
			{
				for (unsigned i=0;i<_solvers.size();i++) {
					if (_solvers[i]->performStep()) return true;
				}
				return false;
			}

			void finalizeAllSolvers()
			{
				for (unsigned i=0;i<_solvers.size();i++)
					_solvers[i]->finalize();
			}

			void initAllSolvers()
			{
				for (int i = 0; i < _solvers.size(); i++)
					_solvers[i]->forceInit();
			}

			void freeAllSolvers() 
			{
				finalizeAllSolvers();

				for (unsigned i=0;i<_solvers.size();i++)
					delete _solvers[i];

				_solvers.clear();
			}

		public:
			int _res[3];

			/* Init */
			Grid<int>	*_flags;
			Grid<Vec3>	*_normal;
			Grid<Real>	*_dist;

			/* Precompute */
			Grid<Vec3>	*_meanVel;
			Grid<Vec3>	*_abl;

			FLUID_3D(int res[3]) 
			{
				_flags = NULL;
				_normal = NULL;
				_dist = NULL;
				_meanVel = NULL;
				_abl = NULL;
				
				_res[0] = res[0];
				_res[1] = res[1];
				_res[2] = res[2];
			};

			~FLUID_3D() 
			{
				freeAllSolvers();

				delete _flags;
				delete _normal;
				delete _dist;
				delete _meanVel;
				delete _abl;
			};

			void init();
			void precompute();
			void step();
	};

};


#endif /* DDF_SMOKE_H */
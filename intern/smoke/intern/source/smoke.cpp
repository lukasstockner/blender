#include "smoke.h"

using namespace std;
using namespace DDF;

void FLUID_3D::init()
{
	printf("-------------------- SMOKE INIT A-------------------------\n");
	{ // static scene
		printf("init res %d, %d, %d\n", _res[0], _res[1], _res[2]);
		SolverObject* solver = new SolverObject( "makescene", nVec3i ( _res[0], _res[1], _res[2] ), DDF_GRID_NO_FREE ); // 200, 80, 150
		solver->createVec3Grid ( "normal", DDF_GRID_NO_FREE );
		solver->createRealGrid ( "dist", DDF_GRID_NO_FREE );
		solver->addInitPlugin ( "init-box-domain",  IntArg ( "flag-inside",FFLUID ) + IntArg ( "flag-border",FINFLOW ) + IntArg("flag-floor", FOBSTACLE) );
		// obstacles
		solver->addInitPlugin ( "init-sphere", VecArg("center",Vec3(0.25,0.6,0.35)) + RealArg("radius",0.1) + StringArg("norm","normal")+StringArg("dist","dist"));	
		solver->addInitPlugin ( "init-box", VecArg("pos1",Vec3(0.2,0.0,0.3)) + VecArg("pos2",Vec3(0.3,0.6,0.4)) + StringArg("norm","normal")+StringArg("dist","dist"));	
		solver->addInitPlugin ( "init-box", VecArg("pos1",Vec3(0.3,0.0,0.65)) + VecArg("pos2",Vec3(0.5,0.3,0.9)) + StringArg("norm","normal")+StringArg("dist","dist"));	
		solver->addInitPlugin ( "init-box", VecArg("pos1",Vec3(0.6,0.0,0.3)) + VecArg("pos2",Vec3(0.65,0.4,0.5)) + StringArg("norm","normal")+StringArg("dist","dist"));	
		// smoke seed
		solver->addInitPlugin ( "init-sphere", VecArg("center",Vec3(0.3,0.65,0.35)) + RealArg("radius",0.1) + IntArg("type", FDENSITYSOURCE));	
		solver->addInitPlugin ( "init-box", VecArg("pos1",Vec3(0.25,0.0,0.3)) + VecArg("pos2",Vec3(0.35,0.65,0.4)) + IntArg("type", FDENSITYSOURCE));	
		solver->addInitPlugin ( "init-box", VecArg("pos1",Vec3(0.35,0.0,0.65)) + VecArg("pos2",Vec3(0.55,0.35,0.9)) + IntArg("type", FDENSITYSOURCE));	
		solver->addInitPlugin ( "init-box", VecArg("pos1",Vec3(0.65,0.0,0.3)) + VecArg("pos2",Vec3(0.7,0.45,0.5)) + IntArg("type", FDENSITYSOURCE));	

		solver->addInitPlugin ( "dump-universal", StringArg ( "grid", "flags" ) + StringArg ( "override-name","scene/static-flags" )  + IntArg ( "single-dump", 1 ) );	
		solver->addInitPlugin ( "dump-universal", StringArg ( "grid", "dist" ) + StringArg ( "override-name","scene/static-dist" )  + IntArg ( "single-dump", 1 ) );	
		solver->addInitPlugin ( "dump-universal", StringArg ( "grid", "normal" ) + StringArg ( "override-name","scene/static-normal" )  + IntArg ( "single-dump", 1 ) );

		_solvers.push_back(solver);
	}

	initAllSolvers();

	advanceAllSolvers();

	finalizeAllSolvers();

	_flags = _solvers[0]->getParams().getGridInt("flags");
	_normal = _solvers[0]->getParams().getGridVec3("normal");
	_dist = _solvers[0]->getParams().getGridReal("dist");

	freeAllSolvers();
/*
	{
		Vec3 inflow (0.4, 0, 0);
		const int nFrames = 50;
		precompute("static", inflow, nFrames, false);
	}
*/
	printf("-------------------- SMOKE INIT B-------------------------\n");
	{
		Vec3 inflow (0.4, 0, 0);
		SolverObject* solver = new SolverObject( "run_static",/*_flags */  "scene/static-flags.gz" );

		{
			Grid<int> *tf = solver->getParams().getGridInt("flags");

			printf("data size: %ld, %ld\n", _flags->getDataSize(), tf->getDataSize());
			printf("display flags: %d, %d\n", _flags->getDisplayFlags(), tf->getDisplayFlags());
			printf("grid flags: %d, %d\n", _flags->getGridFlags(), tf->getGridFlags());
			printf("grid id: %d, %d\n", _flags->getGridId(), tf->getGridId());
			printf("grid size: %d, %d, %d; %d, %d, %d\n", _flags->getGridSize().x, _flags->getGridSize().y, _flags->getGridSize().z, tf->getGridSize().x, tf->getGridSize().y, tf->getGridSize().z);
			printf("max size: %d, %d\n", _flags->getMaxSize(), tf->getMaxSize());
			printf("name: %s, %s\n", _flags->getName().c_str(), tf->getName().c_str());
			printf("numelements: %d, %d\n", _flags->getNumElements(), tf->getNumElements());
			printf("sanity: %d, %d\n", _flags->getSanityCheckMode(), tf->getSanityCheckMode());

			int count = 0;
			int min = INT_MAX, max = -INT_MAX;
			for(unsigned int x = 0; x < tf->getGridSize().x; x++)
				for(unsigned int y= 0; y< tf->getGridSize().y; y++)
					for(unsigned int z = 0; z < tf->getGridSize().z; z++)
			{
				if(_flags->get(x, y, z) != tf->get(x, y, z))
					count++;

				int tm = tf->get(x, y, z);

				if(min > tm)
					min = tm;
				if(max < tm)
					max = tm;
			}
			printf("count: %d, min: %d, max: %d\n", count, min, max);
		}

		solver->getParams().mU0 = inflow;
		solver->getParams().mTimestepAnim = 0.005;
			
		// create grids
		solver->createVec3Grid ( "mean-flow" );
		solver->createRealGrid ( "dist", _dist );
		solver->createVec3Grid ( "vorticity", DDF_GRID_NO_FREE );
		solver->createVec3Grid ( "ABL" );
		solver->createVec3Grid ( "pre-ABL" );
		solver->createVec3Grid ( "vort" );
		solver->createRealGrid ( "pdf" );
		solver->createRealGrid ( "density", DDF_GRID_NO_FREE );
		solver->addStandardSolverGrids();
		solver->createNoiseField("noise", Vec3(0.), Vec3(50,50,50), -0.4, 20.0 /* 2.0 */, 0.002);

		// program solver initialization process
		// solver->addInitPlugin ( "load-universal", StringArg("grid","dist") + StringArg("file","scene/static-dist.gz"));
		solver->addInitPlugin ( "load-universal", StringArg("grid","mean-flow") + StringArg("file","scene/static-mean.gz"));
		solver->addInitPlugin ( "load-universal", StringArg("grid","pre-ABL") + StringArg("file","scene/static-abl.gz"));
		
		// program solver main loop
		solver->addPlugin ( "copy-grid", StringArg ( "src","mean-flow" ) + StringArg ( "dest","vel-curr") );
		solver->addPlugin ("init-density-inflow", StringArg("density","density") + RealArg("target-value",0.7) + IntArg("flag", FDENSITYSOURCE) + StringArg("noise","noise"));
		solver->addPlugin ( "gen-vpart", StringArg ("source","pre-ABL") + StringArg ("flow","ABL") + StringArg("dist","dist") + StringArg("pdf","pdf") +
							RealArg("thres-vort", 2e-2) + RealArg("thres-pdf",5e-5) + RealArg("mult-pdf",1) + RealArg("scale-flow", 0.94) + RealArg("max-bl",0.15) +
							RealArg("min-dist", 3) + RealArg("min-rad", 3) + RealArg("max-rad", 7) + RealArg("vortex-gain", 2.5) + RealArg("fade-in", 0));
	 
		solver->addPlugin ( "semi-lagr-advect-vec3", StringArg ( "vel-src","ABL" ) + IntArg ( "mac", 0) );
		solver->addPlugin ( "apply-vpart", StringArg ( "vorticity", "vorticity" ) );
		solver->addPlugin ( "advect-vpart");
		solver->addPlugin ( "merge-vpart", StringArg("ndist","dist") + RealArg("init-time", 30) + RealArg("decay-time",450) + RealArg("merge-dist",0.8) +
   							RealArg("dissipate-radius",1) + RealArg("radius-cascade",1.5) );
		solver->addPlugin ("compute-vorticity", StringArg("vorticity","vort"));	
		solver->addPlugin ("maccormack-advect-real", StringArg("real-src","density"));
		// solver->addPlugin( "dump-df3", IntArg("max-frames",200) + StringArg("gridname","density") + StringArg("prefix","sta") + RealArg("start-time",0) + IntArg("pbrt",1) + StringArg ( "override-name", "render/static" ));

		_solvers.push_back(solver);
	}

	initAllSolvers();

	_vorticity = _solvers[0]->getParams().getGridVec3("vorticity");
	_density = _solvers[0]->getParams().getGridReal("density");

	_init = 1;

}

// static precompute
void FLUID_3D::precompute(const std::string& name, const Vec3& inflow, int frames, bool dynamic, const Vec3& rotAxis, Real rotSpeed)
{
	bool rotate = (rotSpeed != 0.);
	SolverObject* solver = new SolverObject( "precompute", _flags );

	// create grids
	solver->createVec3Grid ( "normal" );
	solver->createRealGrid ( "dist" );
	solver->createVec3Grid ( "mean-vel" );
	solver->createVec3Grid ( "abl" );
	solver->addStandardSolverGrids();
	
	// additional grids for rot. precomputation
	if (rotate) {
		solver->createIntGrid ("obstacle-flags");
		solver->createIntGrid ("empty-flags");
		solver->addInitPlugin ( "load-universal", StringArg("grid","obstacle-flags") + StringArg("file","scene/" + name + "-flags.gz"));
		solver->addInitPlugin ( "init-box-domain",  StringArg("gridname","empty-flags") + IntArg ( "flag-inside",FFLUID ) + IntArg ( "flag-border",FINFLOW ) );		
	}

	// load grids, initialize fluid velocities
	solver->addInitPlugin ( "load-universal", StringArg("grid","dist") + StringArg("file","scene/" + name + "-dist.gz"));
	solver->addInitPlugin ( "load-universal", StringArg("grid","normal") + StringArg("file","scene/" + name + "-normal.gz"));
	solver->addInitPlugin ( "set-conditional", StringArg ( "gridname","vel-curr" ) + VecArg ( "target-vec",inflow ) + IntArg ( "flag", FFLUID ) );

	// program solver main loop
	solver->addPlugin ( "set-conditional", StringArg ( "gridname","vel-curr" ) + VecArg ( "target-vec",inflow ) + IntArg ( "flag", FINFLOW ) );
	solver->addPlugin ( "maccormack-advect-vec3", StringArg ( "vel-src", "vel-curr" ) );
	solver->addPlugin ( "set-noslip-bcs", StringArg ( "grid","vel-curr" ) );
	solver->addPlugin ( "diffuse-grid", StringArg ( "src-vec3", "vel-curr" ) + RealArg ( "diff", 0.3 ) );
	if (rotate)
		solver->addPlugin ("set-moving-obs-bcs", StringArg("obstacle","obstacle-flags") + StringArg("flags-src","empty-flags") +
							VecArg("obs-rot-axis", rotAxis) + RealArg("obs-rot-vel", rotSpeed) + VecArg("obs-center", Vec3(0.5,0.5,0.5)));
	solver->addPlugin ( "solve-pressure", IntArg ( "openbound",0 ) );
	if (rotate)
		solver->addPlugin ( "average", StringArg ( "gridname","vel-curr" ) + StringArg ( "sumgrid","mean-vel" ) + IntArg ( "from", frames ) + IntArg ( "frames", 3 ) + IntArg ( "post-quit",1 ) + IntArg("stride", frames) );
	else
		solver->addPlugin ( "average", StringArg ( "gridname","vel-curr" ) + StringArg ( "sumgrid","mean-vel" ) + IntArg ( "from", frames ) + IntArg ( "frames", frames ) + IntArg ( "post-quit",1 ) );
	
	// program final steps
	solver->addEndPlugin ( "calc-abl", StringArg ("mean-vel","mean-vel") + StringArg ("dist","dist") + StringArg("normal","normal") + StringArg("abl","abl") + RealArg("d", 1.7));
	
	if (dynamic)
		solver->addEndPlugin ( "add-database", StringArg("grid","abl") + StringArg("normal","normal") + VecArg("u0",rotate ? (rotSpeed*rotAxis):inflow));
	else {
		solver->addEndPlugin ( "dump-universal", StringArg ( "grid","abl" ) + StringArg ( "override-name","scene/" + name + "-abl" )  + IntArg ( "single-dump", 1 ) );	
		solver->addEndPlugin ( "dump-universal", StringArg ( "grid","mean-vel" ) + StringArg ( "override-name","scene/" + name + "-mean" ) + IntArg ( "single-dump", 1 ) );
	}
		
	_solvers.push_back(solver);

	initAllSolvers();

	/* Calculate all requested frames */
	/* DG TODO: Can I precalc one frame at a time instead of all at once? */
	while(!advanceAllSolvers()) {};

	finalizeAllSolvers();

	// _abl = _solvers[0]->getParams().getGridVec3("abl");
	// _meanVel = _solvers[0]->getParams().getGridVec3("mean-vel");

	freeAllSolvers();
}

void FLUID_3D::step()
{
	// run();

	/* Calculate all requested frames */
	/* DG TODO: Can I precalc one frame at a time instead of all at once? */
	advanceAllSolvers();

}

void FLUID_3D::del()
{
	finalizeAllSolvers();
	freeAllSolvers();

	_init = 0;
}